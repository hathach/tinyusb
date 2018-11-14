/**************************************************************************/
/*!
    @file     usbd.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

// This top level class manages the bus state and delegates events to class-specific drivers.

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED

#define _TINY_USB_SOURCE_FILE_

#include "control.h"
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"

#ifndef CFG_TUD_TASK_QUEUE_SZ
#define CFG_TUD_TASK_QUEUE_SZ   16
#endif

#ifndef CFG_TUD_TASK_STACK_SZ
#define CFG_TUD_TASK_STACK_SZ 150
#endif

#ifndef CFG_TUD_TASK_PRIO
#define CFG_TUD_TASK_PRIO 0
#endif


//--------------------------------------------------------------------+
// Device Data
//--------------------------------------------------------------------+
typedef struct {
  uint8_t config_num;

  // map interface number to driver (0xff is invalid)
  uint8_t itf2drv[16];

  // map endpoint to driver ( 0xff is invalid )
  uint8_t ep2drv[2][8];
}usbd_device_t;

static usbd_device_t _usbd_dev;


// Auto descriptor is enabled, descriptor set point to auto generated one
#if CFG_TUD_DESC_AUTO
extern tud_desc_set_t const _usbd_auto_desc_set;
tud_desc_set_t const* usbd_desc_set = &_usbd_auto_desc_set;
#else
tud_desc_set_t const* usbd_desc_set = &tud_desc_set;
#endif

//--------------------------------------------------------------------+
// Class Driver
//--------------------------------------------------------------------+
typedef struct {
  uint8_t class_code;

  void         (* init           ) (void);
  tusb_error_t (* open           ) (uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t* p_length);
  // Control request is called one or more times for a request and can queue multiple data packets.
  tusb_error_t (* control_request ) (uint8_t rhport, tusb_control_request_t const *, uint16_t bytes_already_sent);
  void (* control_request_complete ) (uint8_t rhport, tusb_control_request_t const *);
  tusb_error_t (* xfer_cb        ) (uint8_t rhport, uint8_t ep_addr, tusb_event_t, uint32_t);
  void         (* sof            ) (uint8_t rhport);
  void         (* reset          ) (uint8_t);
} usbd_class_driver_t;

static usbd_class_driver_t const usbd_class_drivers[] =
{
    {
        .class_code      = TUSB_CLASS_UNSPECIFIED,
        .init            = controld_init,
        .open            = NULL,
        .control_request = NULL,
        .control_request_complete = NULL,
        .xfer_cb         = controld_xfer_cb,
        .sof             = NULL,
        .reset           = controld_reset
    },
  #if CFG_TUD_CDC
    {
        .class_code      = TUSB_CLASS_CDC,
        .init            = cdcd_init,
        .open            = cdcd_open,
        .control_request = cdcd_control_request,
        .control_request_complete = cdcd_control_request_complete,
        .xfer_cb         = cdcd_xfer_cb,
        .sof             = NULL,
        .reset           = cdcd_reset
    },
  #endif

  #if CFG_TUD_MSC
    {
        .class_code      = TUSB_CLASS_MSC,
        .init            = mscd_init,
        .open            = mscd_open,
        .control_request = mscd_control_request,
        .control_request_complete = mscd_control_request_complete,
        .xfer_cb         = mscd_xfer_cb,
        .sof             = NULL,
        .reset           = mscd_reset
    },
  #endif


  #if CFG_TUD_HID
    {
        .class_code      = TUSB_CLASS_HID,
        .init            = hidd_init,
        .open            = hidd_open,
        .control_request = hidd_control_request,
        .control_request_complete = hidd_control_request_complete,
        .xfer_cb         = hidd_xfer_cb,
        .sof             = NULL,
        .reset           = hidd_reset
    },
  #endif

  #if CFG_TUD_CUSTOM_CLASS
    {
        .class_code      = TUSB_CLASS_VENDOR_SPECIFIC,
        .init            = cusd_init,
        .open            = cusd_open,
        .control_request = cusd_control_request,
        .control_request_complete = cusd_control_request_complete,
        .xfer_cb         = cusd_xfer_cb,
        .sof             = NULL,
        .reset           = cusd_reset
    },
  #endif
};

enum { USBD_CLASS_DRIVER_COUNT = sizeof(usbd_class_drivers) / sizeof(usbd_class_driver_t) };


//--------------------------------------------------------------------+
// DCD Event
//--------------------------------------------------------------------+
OSAL_TASK_DEF(_usbd_task_def, "usbd", usbd_task, CFG_TUD_TASK_PRIO, CFG_TUD_TASK_STACK_SZ);

/*------------- event queue -------------*/
OSAL_QUEUE_DEF(_usbd_qdef, CFG_TUD_TASK_QUEUE_SZ, dcd_event_t);
static osal_queue_t _usbd_q;

//--------------------------------------------------------------------+
// INTERNAL FUNCTION
//--------------------------------------------------------------------+
static void mark_interface_endpoint(uint8_t const* p_desc, uint16_t desc_len, uint8_t driver_id);

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_mounted(void)
{
  return _usbd_dev.config_num > 0;
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
static tusb_error_t usbd_main_st(void);

tusb_error_t usbd_init (void)
{
  #if (CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE)
  dcd_init(0);
  #endif

  #if (CFG_TUSB_RHPORT1_MODE & OPT_MODE_DEVICE)
  dcd_init(1);
  #endif

  //------------- Task init -------------//
  _usbd_q = osal_queue_create(&_usbd_qdef);
  TU_VERIFY(_usbd_q, TUSB_ERROR_OSAL_QUEUE_FAILED);

  osal_task_create(&_usbd_task_def);

  //------------- class init -------------//
  for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++) usbd_class_drivers[i].init();

  return TUSB_ERROR_NONE;
}

static void usbd_reset(uint8_t rhport)
{
  tu_varclr(&_usbd_dev);
  memset(_usbd_dev.itf2drv, 0xff, sizeof(_usbd_dev.itf2drv)); // invalid mapping
  memset(_usbd_dev.ep2drv , 0xff, sizeof(_usbd_dev.ep2drv )); // invalid mapping
  // Always map the 0th endpoint to the control driver.
  _usbd_dev.ep2drv[TUSB_DIR_IN][0] = 0;
  _usbd_dev.ep2drv[TUSB_DIR_OUT][0] = 0;

  for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++)
  {
    if ( usbd_class_drivers[i].reset ) usbd_class_drivers[i].reset( rhport );
  }
}

// To enable the TASK_ASSERT style (quick return on false condition) in a real RTOS, a task must act as a wrapper
// and is used mainly to call subtasks. Within a subtask return statement can be called freely, the task with
// forever loop cannot have any return at all.

// Within tinyusb stack, all task's code must be placed in subtask to be able to support multiple RTOS
// including none.
void usbd_task( void* param)
{
  (void) param;

#if CFG_TUSB_OS != OPT_OS_NONE
  while (1) {
#endif

  usbd_main_st();

#if CFG_TUSB_OS != OPT_OS_NONE
  }
#endif
}

static tusb_error_t usbd_main_st(void)
{
  dcd_event_t event;

  // Loop until there is no more events in the queue
  while (1)
  {
    if ( !osal_queue_receive(_usbd_q, &event) ) return TUSB_ERROR_NONE;

    if ( DCD_EVENT_SETUP_RECEIVED == event.event_id )
    {
      // Setup tokens are unique to the Control endpointso we delegate to it directly.
      controld_process_setup_request(event.rhport, &event.setup_received);
    }
    else if (DCD_EVENT_XFER_COMPLETE == event.event_id)
    {
      // Invoke the class callback associated with the endpoint address
      uint8_t const ep_addr = event.xfer_complete.ep_addr;
      uint8_t const drv_id  = _usbd_dev.ep2drv[ edpt_dir(ep_addr) ][ edpt_number(ep_addr) ];

      if (drv_id < USBD_CLASS_DRIVER_COUNT)
      {
        usbd_class_drivers[drv_id].xfer_cb( event.rhport, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
      }
    }
    else if (DCD_EVENT_BUS_RESET == event.event_id)
    {
      usbd_reset(event.rhport);
      osal_queue_reset(_usbd_q);
    }
    else if (DCD_EVENT_UNPLUGGED == event.event_id)
    {
      usbd_reset(event.rhport);
      osal_queue_reset(_usbd_q);

      tud_umount_cb(); // invoke callback
    }
    else if (DCD_EVENT_SOF == event.event_id)
    {
      for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++)
      {
        if ( usbd_class_drivers[i].sof )
        {
          usbd_class_drivers[i].sof( event.rhport );
        }
      }
    }
    else if ( USBD_EVT_FUNC_CALL == event.event_id )
    {
      if ( event.func_call.func ) event.func_call.func(event.func_call.param);
    }
    else
    {
      TU_BREAKPOINT();
    }
  }

  return TUSB_ERROR_NONE;
}

void tud_control_interface_control_complete_cb(uint8_t rhport, uint8_t interface, tusb_control_request_t const * const p_request) {
    if (_usbd_dev.itf2drv[ interface ] < USBD_CLASS_DRIVER_COUNT)
    {
      const usbd_class_driver_t *driver = &usbd_class_drivers[_usbd_dev.itf2drv[interface]];
      if (driver->control_request_complete != NULL) {
        driver->control_request_complete(rhport, p_request);
      }
    }
}

tusb_error_t tud_control_interface_control_cb(uint8_t rhport, uint8_t interface, tusb_control_request_t const * const p_request, uint16_t bytes_already_sent) {
    if (_usbd_dev.itf2drv[ interface ] < USBD_CLASS_DRIVER_COUNT)
    {
      return usbd_class_drivers[_usbd_dev.itf2drv[interface]].control_request(rhport, p_request, bytes_already_sent);
    }
    return TUSB_ERROR_FAILED;
}

// Process Set Configure Request
// TODO Host (windows) can get HID report descriptor before set configured
// may need to open interface before set configured
tusb_error_t tud_control_set_config_cb(uint8_t rhport, uint8_t config_number)
{
  dcd_set_config(rhport, config_number);

  _usbd_dev.config_num = config_number;

  //------------- parse configuration & open drivers -------------//
  uint8_t const * desc_cfg = (uint8_t const *) usbd_desc_set->config;
  TU_ASSERT(desc_cfg != NULL, TUSB_ERROR_DESCRIPTOR_CORRUPTED);
  uint8_t const * p_desc = desc_cfg + sizeof(tusb_desc_configuration_t);
  uint16_t const cfg_len = ((tusb_desc_configuration_t*)desc_cfg)->wTotalLength;

  while( p_desc < desc_cfg + cfg_len )
  {
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == descriptor_type(p_desc) )
    {
      p_desc = descriptor_next(p_desc); // ignore Interface Association
    }else
    {
      TU_ASSERT( TUSB_DESC_INTERFACE == descriptor_type(p_desc), TUSB_ERROR_NOT_SUPPORTED_YET );

      tusb_desc_interface_t* p_desc_itf = (tusb_desc_interface_t*) p_desc;
      uint8_t const class_code = p_desc_itf->bInterfaceClass;

      // Check if class is supported
      uint8_t drv_id;
      for (drv_id = 0; drv_id < USBD_CLASS_DRIVER_COUNT; drv_id++)
      {
        if ( usbd_class_drivers[drv_id].class_code == class_code ) break;
      }
      TU_ASSERT( drv_id < USBD_CLASS_DRIVER_COUNT, TUSB_ERROR_NOT_SUPPORTED_YET );

      // Interface number must not be used
      TU_ASSERT( 0xff == _usbd_dev.itf2drv[p_desc_itf->bInterfaceNumber], TUSB_ERROR_FAILED);
      _usbd_dev.itf2drv[p_desc_itf->bInterfaceNumber] = drv_id;

      uint16_t len=0;
      TU_ASSERT_ERR( usbd_class_drivers[drv_id].open( rhport, p_desc_itf, &len ) );
      TU_ASSERT( len >= sizeof(tusb_desc_interface_t), TUSB_ERROR_FAILED );

      mark_interface_endpoint(p_desc, len, drv_id);

      p_desc += len; // next interface
    }
  }

  // invoke callback
  tud_mount_cb();

  return TUSB_ERROR_NONE;
}

// Helper marking endpoint of interface belongs to class driver
static void mark_interface_endpoint(uint8_t const* p_desc, uint16_t desc_len, uint8_t driver_id)
{
  uint16_t len = 0;

  while( len < desc_len )
  {
    if ( TUSB_DESC_ENDPOINT == descriptor_type(p_desc) )
    {
      uint8_t const ep_addr = ((tusb_desc_endpoint_t const*) p_desc)->bEndpointAddress;

      _usbd_dev.ep2drv[ edpt_dir(ep_addr) ][ edpt_number(ep_addr) ] = driver_id;
    }

    len   += descriptor_len(p_desc);
    p_desc = descriptor_next(p_desc);
  }
}

//--------------------------------------------------------------------+
// USBD-DCD Callback API
//--------------------------------------------------------------------+
void dcd_event_handler(dcd_event_t const * event, bool in_isr)
{
  switch (event->event_id)
  {
    case DCD_EVENT_BUS_RESET:
    case DCD_EVENT_UNPLUGGED:
    case DCD_EVENT_SOF:
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_SUSPENDED:
      // TODO support suspended
    break;

    case DCD_EVENT_RESUME:
      // TODO support resume
    break;

    case DCD_EVENT_SETUP_RECEIVED:
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_XFER_COMPLETE:
      osal_queue_send(_usbd_q, event, in_isr);
      TU_ASSERT(event->xfer_complete.result == DCD_XFER_SUCCESS,);
    break;

    default: break;
  }
}

void dcd_event_handler(dcd_event_t const * event, bool in_isr);

// helper to send bus signal event
void dcd_event_bus_signal (uint8_t rhport, dcd_eventid_t eid, bool in_isr)
{
  dcd_event_t event = { .rhport = 0, .event_id = eid, };
  dcd_event_handler(&event, in_isr);
}

// helper to send setup received
void dcd_event_setup_received(uint8_t rhport, uint8_t const * setup, bool in_isr)
{
  dcd_event_t event = { .rhport = 0, .event_id = DCD_EVENT_SETUP_RECEIVED };
  memcpy(&event.setup_received, setup, 8);

  dcd_event_handler(&event, true);
}

// helper to send transfer complete event
void dcd_event_xfer_complete (uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, uint8_t result, bool in_isr)
{
  dcd_event_t event = { .rhport = 0, .event_id = DCD_EVENT_XFER_COMPLETE };

  event.xfer_complete.ep_addr = ep_addr;
  event.xfer_complete.len     = xferred_bytes;
  event.xfer_complete.result  = result;

  dcd_event_handler(&event, in_isr);
}

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+

tusb_error_t usbd_open_edpt_pair(uint8_t rhport, tusb_desc_endpoint_t const* p_desc_ep, uint8_t xfer_type, uint8_t* ep_out, uint8_t* ep_in)
{
  for(int i=0; i<2; i++)
  {
    TU_ASSERT(TUSB_DESC_ENDPOINT == p_desc_ep->bDescriptorType &&
              xfer_type          == p_desc_ep->bmAttributes.xfer, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

    TU_ASSERT( dcd_edpt_open(rhport, p_desc_ep), TUSB_ERROR_DCD_OPEN_PIPE_FAILED );

    if ( edpt_dir(p_desc_ep->bEndpointAddress) ==  TUSB_DIR_IN )
    {
      (*ep_in) = p_desc_ep->bEndpointAddress;
    }else
    {
      (*ep_out) = p_desc_ep->bEndpointAddress;
    }

    p_desc_ep = (tusb_desc_endpoint_t const *) descriptor_next( (uint8_t const*)  p_desc_ep );
  }

  return TUSB_ERROR_NONE;
}

void usbd_defer_func(osal_task_func_t func, void* param, bool in_isr )
{
  dcd_event_t event =
  {
      .rhport   = 0,
      .event_id = USBD_EVT_FUNC_CALL,
  };

  event.func_call.func  = func;
  event.func_call.param = param;

  osal_queue_send(_usbd_q, &event, in_isr);
}

#endif
