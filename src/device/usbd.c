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

  uint8_t itf2drv[16];  // map interface number to driver (0xff is invalid)
  uint8_t ep2drv[2][8]; // map endpoint to driver ( 0xff is invalid )

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
  bool         (* control_request ) (uint8_t rhport, tusb_control_request_t const * request);
  bool (* control_request_complete ) (uint8_t rhport, tusb_control_request_t const * request);
  tusb_error_t (* xfer_cb        ) (uint8_t rhport, uint8_t ep_addr, xfer_result_t, uint32_t);
  void         (* sof            ) (uint8_t rhport);
  void         (* reset          ) (uint8_t);
} usbd_class_driver_t;

static usbd_class_driver_t const usbd_class_drivers[] =
{
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
// Prototypes
//--------------------------------------------------------------------+
static void mark_interface_endpoint(uint8_t const* p_desc, uint16_t desc_len, uint8_t driver_id);
static bool process_control_request(uint8_t rhport, tusb_control_request_t const * p_request);
static bool process_set_config(uint8_t rhport, uint8_t config_number);
static void const* get_descriptor(tusb_control_request_t const * p_request, uint16_t* desc_len);

void usbd_control_reset (uint8_t rhport);
bool usbd_control_xfer_cb (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void usbd_control_set_complete_callback( bool (*fp) (uint8_t, tusb_control_request_t const * ) );

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_mounted(void)
{
  return _usbd_dev.config_num > 0;
}

//--------------------------------------------------------------------+
// USBD Task
//--------------------------------------------------------------------+
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

  usbd_control_reset(rhport);

  for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++)
  {
    if ( usbd_class_drivers[i].reset ) usbd_class_drivers[i].reset( rhport );
  }
}

// Main device task implementation
static void usbd_task_body(void)
{
  // Loop until there is no more events in the queue
  while (1)
  {
    dcd_event_t event;

    if ( !osal_queue_receive(_usbd_q, &event) ) return;

    switch ( event.event_id )
    {
      case DCD_EVENT_SETUP_RECEIVED:
        // Process control request, if failed control endpoint is stalled
        if ( !process_control_request(event.rhport, &event.setup_received) )
        {
          usbd_control_stall(event.rhport);
        }
      break;

      case DCD_EVENT_XFER_COMPLETE:
      {
        // Invoke the class callback associated with the endpoint address
        uint8_t const ep_addr = event.xfer_complete.ep_addr;

        if ( 0 == edpt_number(ep_addr) )
        {
          // control transfer DATA stage callback
          usbd_control_xfer_cb(event.rhport, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
        }
        else
        {
          uint8_t const drv_id = _usbd_dev.ep2drv[edpt_dir(ep_addr)][edpt_number(ep_addr)];
          TU_ASSERT(drv_id < USBD_CLASS_DRIVER_COUNT,);

          usbd_class_drivers[drv_id].xfer_cb(event.rhport, ep_addr, event.xfer_complete.result, event.xfer_complete.len);
        }
      }
      break;

      case DCD_EVENT_BUS_RESET:
        // note: if task is too slow, we could clear the event of the new attached
        usbd_reset(event.rhport);
        osal_queue_reset(_usbd_q);
      break;

      case DCD_EVENT_UNPLUGGED:
        // note: if task is too slow, we could clear the event of the new attached
        usbd_reset(event.rhport);
        osal_queue_reset(_usbd_q);

        tud_umount_cb();    // invoke callback
      break;

      case DCD_EVENT_SOF:
        for ( uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++ )
        {
          if ( usbd_class_drivers[i].sof )
          {
            usbd_class_drivers[i].sof(event.rhport);
          }
        }
      break;

      case USBD_EVT_FUNC_CALL:
        if ( event.func_call.func ) event.func_call.func(event.func_call.param);
      break;

      default:
        TU_BREAKPOINT();
      break;
    }
  }
}

/* USB device task
 * Thread that handles all device events. With an real RTOS, the task must be a forever loop and never return.
 * For codign convenience with no RTOS, we use wrapped sub-function for processing to easily return at any time.
 */
void usbd_task( void* param)
{
  (void) param;

#if CFG_TUSB_OS != OPT_OS_NONE
  while (1) {
#endif

  usbd_task_body();

#if CFG_TUSB_OS != OPT_OS_NONE
  }
#endif
}

//--------------------------------------------------------------------+
// Control Request Parser & Handling
//--------------------------------------------------------------------+

// This handles the actual request and its response.
// return false will cause its caller to stall control endpoint
static bool process_control_request(uint8_t rhport, tusb_control_request_t const * p_request)
{
  usbd_control_set_complete_callback(NULL);

  if ( TUSB_REQ_RCPT_DEVICE == p_request->bmRequestType_bit.recipient &&
       TUSB_REQ_TYPE_STANDARD == p_request->bmRequestType_bit.type )
  {
    //------------- Standard Device Requests e.g in enumeration -------------//
    void* data_buf = NULL;
    uint16_t data_len = 0;

    switch ( p_request->bRequest )
    {
      case TUSB_REQ_SET_ADDRESS:
        // response with status first before changing device address
        usbd_control_status(rhport, p_request);
        dcd_set_address(rhport, (uint8_t) p_request->wValue);
        return true; // skip the rest
      break;

      case TUSB_REQ_GET_CONFIGURATION:
        data_buf = &_usbd_dev.config_num;
        data_len = 1;
      break;

      case TUSB_REQ_SET_CONFIGURATION:
      {
        uint8_t const config = (uint8_t) p_request->wValue;

        dcd_set_config(rhport, config);
        _usbd_dev.config_num = config;

        TU_ASSERT( TUSB_ERROR_NONE == process_set_config(rhport, config) );
      }
      break;

      case TUSB_REQ_GET_DESCRIPTOR:
        data_buf = (void*) get_descriptor(p_request, &data_len);
        if ( data_buf == NULL || data_len == 0 ) return false;
      break;

      default:
        TU_BREAKPOINT();
      return false;
    }

    usbd_control_xfer(rhport, p_request, data_buf, data_len);
  }
  else if ( TUSB_REQ_RCPT_INTERFACE == p_request->bmRequestType_bit.recipient )
  {
    //------------- Class/Interface Specific Request -------------//
    uint8_t const itf = tu_u16_low(p_request->wIndex);
    uint8_t const drvid = _usbd_dev.itf2drv[ itf ];

    TU_VERIFY (drvid < USBD_CLASS_DRIVER_COUNT );

    usbd_control_set_complete_callback(usbd_class_drivers[drvid].control_request_complete );

    // control endpoint will be stalled if driver return false
    return usbd_class_drivers[drvid].control_request(rhport, p_request);
  }
  else if ( p_request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT &&
            p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD )
  {
    //------------- Endpoint Request -------------//
    switch ( p_request->bRequest )
    {
      case TUSB_REQ_GET_STATUS:
      {
        uint16_t status = dcd_edpt_stalled(rhport, tu_u16_low(p_request->wIndex)) ? 0x0001 : 0x0000;
        usbd_control_xfer(rhport, p_request, &status, 2);
      }
      break;

      case TUSB_REQ_CLEAR_FEATURE:
        // only endpoint feature is halted/stalled
        dcd_edpt_clear_stall(rhport, tu_u16_low(p_request->wIndex));
        usbd_control_status(rhport, p_request);
      break;

      case TUSB_REQ_SET_FEATURE:
        // only endpoint feature is halted/stalled
        dcd_edpt_stall(rhport, tu_u16_low(p_request->wIndex));
        usbd_control_status(rhport, p_request);
      break;

      default:
        TU_BREAKPOINT();
      return false;
    }
  }
  else
  {
    //------------- Unsupported Request -------------//
    TU_BREAKPOINT();
    return false;
  }

  return true;
}

// Process Set Configure Request
// This function parse configuration descriptor & open drivers accordingly
static bool process_set_config(uint8_t rhport, uint8_t config_number)
{
  uint8_t const * desc_cfg = (uint8_t const *) usbd_desc_set->config;
  TU_ASSERT(desc_cfg != NULL);

  uint8_t const * p_desc = desc_cfg + sizeof(tusb_desc_configuration_t);
  uint16_t const cfg_len = ((tusb_desc_configuration_t*)desc_cfg)->wTotalLength;

  while( p_desc < desc_cfg + cfg_len )
  {
    // Each interface always starts with Interface or Association descriptor
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == descriptor_type(p_desc) )
    {
      p_desc = descriptor_next(p_desc); // ignore Interface Association
    }else
    {
      TU_ASSERT( TUSB_DESC_INTERFACE == descriptor_type(p_desc) );

      tusb_desc_interface_t* desc_itf = (tusb_desc_interface_t*) p_desc;

      // Check if class is supported
      uint8_t drv_id;
      for (drv_id = 0; drv_id < USBD_CLASS_DRIVER_COUNT; drv_id++)
      {
        if ( usbd_class_drivers[drv_id].class_code == desc_itf->bInterfaceClass ) break;
      }
      TU_ASSERT( drv_id < USBD_CLASS_DRIVER_COUNT ); // unsupported class

      // Interface number must not be used already TODO alternate interface
      TU_ASSERT( 0xff == _usbd_dev.itf2drv[desc_itf->bInterfaceNumber] );
      _usbd_dev.itf2drv[desc_itf->bInterfaceNumber] = drv_id;

      uint16_t len=0;
      TU_ASSERT_ERR( usbd_class_drivers[drv_id].open( rhport, desc_itf, &len ), false );
      TU_ASSERT( len >= sizeof(tusb_desc_interface_t) );

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

// return descriptor's buffer and update desc_len
static void const* get_descriptor(tusb_control_request_t const * p_request, uint16_t* desc_len)
{
  tusb_desc_type_t const desc_type = (tusb_desc_type_t) tu_u16_high(p_request->wValue);
  uint8_t const desc_index = tu_u16_low( p_request->wValue );

  uint8_t const * desc_data = NULL;
  uint16_t len = 0;

  *desc_len = 0;

  switch(desc_type)
  {
    case TUSB_DESC_DEVICE:
      desc_data = (uint8_t const *) usbd_desc_set->device;
      len       = sizeof(tusb_desc_device_t);
    break;

    case TUSB_DESC_CONFIGURATION:
      desc_data = (uint8_t const *) usbd_desc_set->config;
      len       = ((tusb_desc_configuration_t const*) desc_data)->wTotalLength;
    break;

    case TUSB_DESC_STRING:
      // String Descriptor always uses the desc set from user
      if ( desc_index < tud_desc_set.string_count )
      {
        desc_data = tud_desc_set.string_arr[desc_index];
        TU_VERIFY( desc_data != NULL, NULL );

        len  = desc_data[0];  // first byte of descriptor is its size
      }else
      {
        // out of range
        /* The 0xEE index string is a Microsoft USB extension.
         * It can be used to tell Windows what driver it should use for the device !!!
         */
        return NULL;
      }
    break;

    case TUSB_DESC_DEVICE_QUALIFIER:
      // TODO If not highspeed capable stall this request otherwise
      // return the descriptor that could work in highspeed
      return NULL;
    break;

    default: return NULL;
  }

  *desc_len = len;
  return desc_data;
}

//--------------------------------------------------------------------+
// DCD Event Handler
//--------------------------------------------------------------------+
void dcd_event_handler(dcd_event_t const * event, bool in_isr)
{
  switch (event->event_id)
  {
    case DCD_EVENT_BUS_RESET:
    case DCD_EVENT_UNPLUGGED:
      osal_queue_send(_usbd_q, event, in_isr);
    break;

    case DCD_EVENT_SOF:
      // nothing to do now
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
      // skip zero-length control status complete event, should dcd notifies us.
      if ( 0 == edpt_number(event->xfer_complete.ep_addr) && event->xfer_complete.len == 0) break;

      osal_queue_send(_usbd_q, event, in_isr);
      TU_ASSERT(event->xfer_complete.result == XFER_RESULT_SUCCESS,);
    break;

    default: break;
  }
}

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
