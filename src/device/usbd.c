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

  // map interface number to driver (0xff is invalid)
  uint8_t itf2drv[16];

  // map endpoint to driver ( 0xff is invalid )
  uint8_t ep2drv[2][8];
}usbd_device_t;

CFG_TUSB_ATTR_USBRAM CFG_TUSB_MEM_ALIGN uint8_t _usbd_ctrl_buf[CFG_TUD_CTRL_BUFSIZE];
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
  tusb_error_t (* control_req_st ) (uint8_t rhport, tusb_control_request_t const *);
  tusb_error_t (* xfer_cb        ) (uint8_t rhport, uint8_t ep_addr, tusb_event_t, uint32_t);
  void         (* sof            ) (uint8_t rhport);
  void         (* reset          ) (uint8_t);
} usbd_class_driver_t;

static usbd_class_driver_t const usbd_class_drivers[] =
{
  #if CFG_TUD_CDC
    {
        .class_code     = TUSB_CLASS_CDC,
        .init           = cdcd_init,
        .open           = cdcd_open,
        .control_req_st = cdcd_control_request_st,
        .xfer_cb        = cdcd_xfer_cb,
        .sof            = cdcd_sof,
        .reset          = cdcd_reset
    },
  #endif

  #if CFG_TUD_MSC
    {
        .class_code     = TUSB_CLASS_MSC,
        .init           = mscd_init,
        .open           = mscd_open,
        .control_req_st = mscd_control_request_st,
        .xfer_cb        = mscd_xfer_cb,
        .sof            = NULL,
        .reset          = mscd_reset
    },
  #endif


  #if CFG_TUD_HID
    {
        .class_code     = TUSB_CLASS_HID,
        .init           = hidd_init,
        .open           = hidd_open,
        .control_req_st = hidd_control_request_st,
        .xfer_cb        = hidd_xfer_cb,
        .sof            = NULL,
        .reset          = hidd_reset
    },
  #endif

  #if CFG_TUD_CUSTOM_CLASS
    {
        .class_code     = TUSB_CLASS_VENDOR_SPECIFIC,
        .init           = cusd_init,
        .open           = cusd_open,
        .control_req_st = cusd_control_request_st,
        .xfer_cb        = cusd_xfer_cb,
        .sof            = NULL,
        .reset          = cusd_reset
    },
  #endif
};

enum { USBD_CLASS_DRIVER_COUNT = sizeof(usbd_class_drivers) / sizeof(usbd_class_driver_t) };


//--------------------------------------------------------------------+
// DCD Event
//--------------------------------------------------------------------+
typedef enum
{
  USBD_EVT_SETUP_RECEIVED = 1,
  USBD_EVT_XFER_DONE,
  USBD_EVT_SOF,

  USBD_EVT_FUNC_CALL
}usbd_eventid_t;

typedef struct ATTR_ALIGNED(4)
{
  uint8_t rhport;
  uint8_t event_id;

  union {
    // USBD_EVT_SETUP_RECEIVED
    tusb_control_request_t setup_received;

    // USBD_EVT_XFER_DONE
    struct {
      uint8_t  ep_addr;
      uint8_t  result;
      uint32_t xferred_byte;
    }xfer_done;

    // USBD_EVT_FUNC_CALL
    struct {
      osal_task_func_t func;
      void* param;
    }func_call;
  };
} usbd_task_event_t;

TU_VERIFY_STATIC(sizeof(usbd_task_event_t) <= 12, "size is not correct");

OSAL_TASK_DEF(_usbd_task_def, "usbd", usbd_task, CFG_TUD_TASK_PRIO, CFG_TUD_TASK_STACK_SZ);

/*------------- event queue -------------*/
OSAL_QUEUE_DEF(_usbd_qdef, CFG_TUD_TASK_QUEUE_SZ, usbd_task_event_t);
static osal_queue_t _usbd_q;

/*------------- control transfer semaphore -------------*/
static osal_semaphore_def_t _usbd_sem_def;
/*static*/ osal_semaphore_t _usbd_ctrl_sem;

//--------------------------------------------------------------------+
// INTERNAL FUNCTION
//--------------------------------------------------------------------+
static void mark_interface_endpoint(uint8_t const* p_desc, uint16_t desc_len, uint8_t driver_id);
static tusb_error_t proc_set_config_req(uint8_t rhport, uint8_t config_number);
static uint16_t get_descriptor(uint8_t rhport, tusb_control_request_t const * const p_request, uint8_t const ** pp_buffer);

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
static tusb_error_t proc_control_request_st(uint8_t rhport, tusb_control_request_t const * const p_request);
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

  _usbd_ctrl_sem = osal_semaphore_create(&_usbd_sem_def);
  TU_VERIFY(_usbd_q, TUSB_ERROR_OSAL_SEMAPHORE_FAILED);

  osal_task_create(&_usbd_task_def);

  //------------- class init -------------//
  for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++) usbd_class_drivers[i].init();

  return TUSB_ERROR_NONE;
}

// To enable the TASK_ASSERT style (quick return on false condition) in a real RTOS, a task must act as a wrapper
// and is used mainly to call subtasks. Within a subtask return statement can be called freely, the task with
// forever loop cannot have any return at all.
void usbd_task( void* param)
{
  (void) param;

  OSAL_TASK_BEGIN
  usbd_main_st();
  OSAL_TASK_END
}

static tusb_error_t usbd_main_st(void)
{
  static usbd_task_event_t event;

  OSAL_SUBTASK_BEGIN

  // Loop until there is no more events in the queue
  while (1)
  {
    tusb_error_t err;
    err = TUSB_ERROR_NONE;

    memclr_(&event, sizeof(usbd_task_event_t));

    osal_queue_receive(_usbd_q, &event, OSAL_TIMEOUT_WAIT_FOREVER, &err);

    if ( USBD_EVT_SETUP_RECEIVED == event.event_id )
    {
      STASK_INVOKE( proc_control_request_st(event.rhport, &event.setup_received), err );
    }
    else if (USBD_EVT_XFER_DONE == event.event_id)
    {
      // Invoke the class callback associated with the endpoint address
      uint8_t const ep_addr = event.xfer_done.ep_addr;
      uint8_t const drv_id  = _usbd_dev.ep2drv[ edpt_dir(ep_addr) ][ edpt_number(ep_addr) ];

      if (drv_id < USBD_CLASS_DRIVER_COUNT)
      {
        usbd_class_drivers[drv_id].xfer_cb( event.rhport, ep_addr, (tusb_event_t) event.xfer_done.result, event.xfer_done.xferred_byte);
      }
    }
    else if (USBD_EVT_SOF == event.event_id)
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
      STASK_ASSERT(false);
    }
  }

  OSAL_SUBTASK_END
}

static void usbd_reset(uint8_t rhport)
{
  varclr_(&_usbd_dev);
  memset(_usbd_dev.itf2drv, 0xff, sizeof(_usbd_dev.itf2drv)); // invalid mapping
  memset(_usbd_dev.ep2drv , 0xff, sizeof(_usbd_dev.ep2drv )); // invalid mapping

  for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++)
  {
    if ( usbd_class_drivers[i].reset ) usbd_class_drivers[i].reset( rhport );
  }
}

//--------------------------------------------------------------------+
// CONTROL REQUEST
//--------------------------------------------------------------------+
static tusb_error_t proc_control_request_st(uint8_t rhport, tusb_control_request_t const * const p_request)
{
  OSAL_SUBTASK_BEGIN

  tusb_error_t error;
  error = TUSB_ERROR_NONE;

  //------------- Standard Request e.g in enumeration -------------//
  if( TUSB_REQ_RCPT_DEVICE    == p_request->bmRequestType_bit.recipient &&
      TUSB_REQ_TYPE_STANDARD  == p_request->bmRequestType_bit.type )
  {
    if ( TUSB_REQ_GET_DESCRIPTOR == p_request->bRequest )
    {
      uint8_t  const * buffer = NULL;
      uint16_t const   len    = get_descriptor(rhport, p_request, &buffer);

      if ( len )
      {
        STASK_ASSERT( len <= CFG_TUD_CTRL_BUFSIZE );
        memcpy(_usbd_ctrl_buf, buffer, len);
        usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _usbd_ctrl_buf, len);
      }else
      {
        dcd_control_stall(rhport); // stall unsupported descriptor
      }
    }
    else if (TUSB_REQ_GET_CONFIGURATION == p_request->bRequest )
    {
      memcpy(_usbd_ctrl_buf, &_usbd_dev.config_num, 1);
      usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _usbd_ctrl_buf, 1);
    }
    else if ( TUSB_REQ_SET_ADDRESS == p_request->bRequest )
    {
      dcd_set_address(rhport, (uint8_t) p_request->wValue);

      #if CFG_TUSB_MCU != OPT_MCU_NRF5X // nrf5x auto handle set address, we must not return status
      dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
      #endif
    }
    else if ( TUSB_REQ_SET_CONFIGURATION == p_request->bRequest )
    {
      proc_set_config_req(rhport, (uint8_t) p_request->wValue);
      dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
    }
    else
    {
      dcd_control_stall(rhport); // Stall unsupported request
    }
  }

  //------------- Class/Interface Specific Request -------------//
  else if ( TUSB_REQ_RCPT_INTERFACE == p_request->bmRequestType_bit.recipient )
  {
    if (_usbd_dev.itf2drv[ tu_u16_low(p_request->wIndex) ] < USBD_CLASS_DRIVER_COUNT)
    {
      STASK_INVOKE( usbd_class_drivers[ _usbd_dev.itf2drv[ tu_u16_low(p_request->wIndex) ] ].control_req_st(rhport, p_request), error );
    }else
    {
      dcd_control_stall(rhport); // Stall unsupported request
    }
  }

  //------------- Endpoint Request -------------//
  else if ( TUSB_REQ_RCPT_ENDPOINT == p_request->bmRequestType_bit.recipient &&
            TUSB_REQ_TYPE_STANDARD == p_request->bmRequestType_bit.type)
  {
    if (TUSB_REQ_GET_STATUS == p_request->bRequest )
    {
      uint16_t status = dcd_edpt_stalled(rhport, tu_u16_low(p_request->wIndex)) ? 0x0001 : 0x0000;
      memcpy(_usbd_ctrl_buf, &status, 2);

      usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _usbd_ctrl_buf, 2);
    }
    else if (TUSB_REQ_CLEAR_FEATURE == p_request->bRequest )
    {
      // only endpoint feature is halted/stalled
      dcd_edpt_clear_stall(rhport, tu_u16_low(p_request->wIndex));
      dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
    }
    else if (TUSB_REQ_SET_FEATURE == p_request->bRequest )
    {
      // only endpoint feature is halted/stalled
      dcd_edpt_stall(rhport, tu_u16_low(p_request->wIndex));
      dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
    }
    else
    {
      dcd_control_stall(rhport); // Stall unsupported request
    }
  }

  //------------- Unsupported Request -------------//
  else
  {
    dcd_control_stall(rhport); // Stall unsupported request
  }

  OSAL_SUBTASK_END
}

// Process Set Configure Request
// TODO Host (windows) can get HID report descriptor before set configured
// may need to open interface before set configured
static tusb_error_t proc_set_config_req(uint8_t rhport, uint8_t config_number)
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

// return len of descriptor and change pointer to descriptor's buffer
static uint16_t get_descriptor(uint8_t rhport, tusb_control_request_t const * const p_request, uint8_t const ** pp_buffer)
{
  (void) rhport;

  tusb_desc_type_t const desc_type = (tusb_desc_type_t) tu_u16_high(p_request->wValue);
  uint8_t const desc_index = tu_u16_low( p_request->wValue );

  uint8_t const * desc_data = NULL ;
  uint16_t len = 0;

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
        TU_VERIFY( desc_data != NULL, 0 );

        len  = desc_data[0];  // first byte of descriptor is its size
      }else
      {
        // out of range
        /* The 0xee string is indeed a Microsoft USB extension.
         * It can be used to tell Windows what driver it should use for the device !!!
         */
        return 0;
      }
    break;

    case TUSB_DESC_DEVICE_QUALIFIER:
      // TODO If not highspeed capable stall this request otherwise
      // return the descriptor that could work in highspeed
      return 0;
    break;

    default: return 0;
  }

  TU_ASSERT( desc_data != NULL, 0);

  // up to Host's length
  len = tu_min16(p_request->wLength, len );
  (*pp_buffer) = desc_data;

  return len;
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
void dcd_bus_event(uint8_t rhport, usbd_bus_event_type_t bus_event)
{
  switch(bus_event)
  {
    case USBD_BUS_EVENT_RESET:
      usbd_reset(rhport);

      osal_queue_flush(_usbd_q);
      osal_semaphore_reset_isr(_usbd_ctrl_sem);
    break;

    case USBD_BUS_EVENT_SOF:
    {
      #if CFG_TUD_CDC_FLUSH_ON_SOF
      usbd_task_event_t task_event =
      {
          .rhport          = rhport,
          .event_id        = USBD_EVT_SOF,
      };
      osal_queue_send_isr(_usbd_q, &task_event);
      #endif
    }
    break;

    case USBD_BUS_EVENT_UNPLUGGED:
      usbd_reset(rhport);
      tud_umount_cb(); // invoke callback
    break;

    case USBD_BUS_EVENT_SUSPENDED:
      // TODO support suspended
    break;

    default: break;
  }
}

void dcd_setup_received(uint8_t rhport, uint8_t const* p_request)
{
  usbd_task_event_t task_event =
  {
      .rhport   = rhport,
      .event_id = USBD_EVT_SETUP_RECEIVED,
  };

  memcpy(&task_event.setup_received, p_request, sizeof(tusb_control_request_t));
  osal_queue_send_isr(_usbd_q, &task_event);
}

void dcd_xfer_complete(uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, bool succeeded)
{
  if (ep_addr == 0 )
  {
    // Control Transfer
    (void) rhport;
    (void) succeeded;

    // only signal data stage, skip status (zero byte)
    if (xferred_bytes) osal_semaphore_post_isr( _usbd_ctrl_sem );
  }else
  {
    usbd_task_event_t event =
    {
        .rhport         = rhport,
        .event_id     = USBD_EVT_XFER_DONE,
    };

    event.xfer_done.ep_addr      = ep_addr;
    event.xfer_done.xferred_byte = xferred_bytes;
    event.xfer_done.result       = succeeded ? TUSB_EVENT_XFER_COMPLETE : TUSB_EVENT_XFER_ERROR;

    osal_queue_send_isr(_usbd_q, &event);
  }

  TU_ASSERT(succeeded, );
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

void usbd_defer_func(osal_task_func_t func, void* param, bool isr )
{
  usbd_task_event_t event =
  {
      .rhport   = 0,
      .event_id = USBD_EVT_FUNC_CALL,
  };

  event.func_call.func  = func;
  event.func_call.param = param;

  if ( isr )
  {
    osal_queue_send_isr(_usbd_q, &event);
  }else
  {
    osal_queue_send(_usbd_q, &event);
  }
}

#endif
