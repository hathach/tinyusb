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

#if MODE_DEVICE_SUPPORTED

#define _TINY_USB_SOURCE_FILE_

#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"

#define USBD_TASK_QUEUE_DEPTH   16

#ifndef CFG_TUD_TASK_STACKSIZE
#define CFG_TUD_TASK_STACKSIZE 150
#endif

#ifndef CFG_TUD_TASK_PRIO
#define CFG_TUD_TASK_PRIO 0
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t class_code;

  void         (* init           ) (void);
  tusb_error_t (* open           ) (uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t* p_length);
  tusb_error_t (* control_req_st ) (uint8_t rhport, tusb_control_request_t const *);
  tusb_error_t (* xfer_cb        ) (uint8_t rhport, uint8_t ep_addr, tusb_event_t, uint32_t);
  void         (* sof            ) (uint8_t rhport);
  void         (* close          ) (uint8_t);
} usbd_class_driver_t;


enum {
  USBD_INTERFACE_NUM_MAX = 16 // USB specs specify up to 16 endpoints per device
};

typedef struct {
  volatile uint8_t state;
  uint8_t  config_num;

  uint8_t interface2class[USBD_INTERFACE_NUM_MAX]; // determine interface number belongs to which class
}usbd_device_info_t;

//--------------------------------------------------------------------+
// Class & Device Driver
//--------------------------------------------------------------------+
CFG_TUSB_ATTR_USBRAM CFG_TUSB_MEM_ALIGN uint8_t usbd_enum_buffer[CFG_TUD_ENUM_BUFFER_SIZE];

tud_desc_init_t _usbd_descs[CONTROLLER_DEVICE_NUMBER];
usbd_device_info_t usbd_devices[CONTROLLER_DEVICE_NUMBER];

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
        .close          = cdcd_close
    },
  #endif

  #if DEVICE_CLASS_HID
    {
        .class_code     = TUSB_CLASS_HID,
        .init           = hidd_init,
        .open           = hidd_open,
        .control_req_st = hidd_control_request_st,
        .xfer_cb        = hidd_xfer_cb,
        .sof            = NULL,
        .close          = hidd_close
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
        .close          = mscd_close
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
        .close          = cusd_close
    },
  #endif
};

enum { USBD_CLASS_DRIVER_COUNT = sizeof(usbd_class_drivers) / sizeof(usbd_class_driver_t) };

//tusb_desc_device_qualifier_t _device_qual =
//{
//    .bLength = sizeof(tusb_desc_device_qualifier_t),
//    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
//    .bcdUSB = 0x0200,
//    .bDeviceClass =
//};


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

VERIFY_STATIC(sizeof(usbd_task_event_t) <= 12, "size is not correct");

OSAL_TASK_DEF(_usbd_task_def, "usbd", usbd_task, CFG_TUD_TASK_PRIO, CFG_TUD_TASK_STACKSIZE);

/*------------- event queue -------------*/
OSAL_QUEUE_DEF(_usbd_qdef, USBD_TASK_QUEUE_DEPTH, usbd_task_event_t);
static osal_queue_t _usbd_q;

/*------------- control transfer semaphore -------------*/
static osal_semaphore_def_t _usbd_sem_def;
/*static*/ osal_semaphore_t _usbd_ctrl_sem;

//--------------------------------------------------------------------+
// INTERNAL FUNCTION
//--------------------------------------------------------------------+
static tusb_error_t proc_set_config_req(uint8_t rhport, uint8_t config_number);
static uint16_t get_descriptor(uint8_t rhport, tusb_control_request_t const * const p_request, uint8_t const ** pp_buffer);

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_n_mounted(uint8_t rhport)
{
  return usbd_devices[rhport].state == TUSB_DEVICE_STATE_CONFIGURED;
}

bool tud_n_set_descriptors(uint8_t rhport, tud_desc_init_t const* desc_cfg)
{
  _usbd_descs[rhport] = *desc_cfg;
  return true;
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
  VERIFY(_usbd_q, TUSB_ERROR_OSAL_QUEUE_FAILED);

  _usbd_ctrl_sem = osal_semaphore_create(&_usbd_sem_def);
  VERIFY(_usbd_q, TUSB_ERROR_OSAL_SEMAPHORE_FAILED);

  osal_task_create(&_usbd_task_def);

  //------------- Core init -------------//
  arrclr_( _usbd_descs );

  //------------- class init -------------//
  for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++)
  {
    usbd_class_drivers[i].init();
  }

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
    // TODO only call respective interface callback
    // Call class handling function. Those does not own the endpoint should check and return
    for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++)
    {
      if ( usbd_class_drivers[i].xfer_cb )
      {
        usbd_class_drivers[i].xfer_cb( event.rhport, event.xfer_done.ep_addr, (tusb_event_t) event.xfer_done.result, event.xfer_done.xferred_byte);
      }
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

  OSAL_SUBTASK_END
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
        usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, (uint8_t*) buffer, len );
      }else
      {
        dcd_control_stall(rhport); // stall unsupported descriptor
      }
    }
    else if (TUSB_REQ_GET_CONFIGURATION == p_request->bRequest )
    {
      memcpy(usbd_enum_buffer, &usbd_devices[rhport].config_num, 1);
      usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, (uint8_t*) usbd_enum_buffer, 1);
    }
    else if ( TUSB_REQ_SET_ADDRESS == p_request->bRequest )
    {
      dcd_set_address(rhport, (uint8_t) p_request->wValue);
      usbd_devices[rhport].state = TUSB_DEVICE_STATE_ADDRESSED;

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
  else if ( TUSB_REQ_RCPT_INTERFACE == p_request->bmRequestType_bit.recipient)
  {
    static uint8_t drid;
    uint8_t const class_code = usbd_devices[rhport].interface2class[ u16_low_u8(p_request->wIndex) ];

    for (drid = 0; drid < USBD_CLASS_DRIVER_COUNT; drid++)
    {
      if ( usbd_class_drivers[drid].class_code == class_code ) break;
    }

    if ( (drid < USBD_CLASS_DRIVER_COUNT) && usbd_class_drivers[drid].control_req_st )
    {
      STASK_INVOKE( usbd_class_drivers[drid].control_req_st(rhport, p_request), error );
    }else
    {
      dcd_control_stall(rhport); // Stall unsupported request
    }
  }

  //------------- Endpoint Request -------------//
  else if ( TUSB_REQ_RCPT_ENDPOINT == p_request->bmRequestType_bit.recipient &&
            TUSB_REQ_TYPE_STANDARD == p_request->bmRequestType_bit.type)
  {
    if (TUSB_REQ_CLEAR_FEATURE == p_request->bRequest )
    {
      dcd_edpt_clear_stall(rhport, u16_low_u8(p_request->wIndex) );
      dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
    } else
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

// TODO Host (windows) can get HID report descriptor before set configured
// may need to open interface before set configured
static tusb_error_t proc_set_config_req(uint8_t rhport, uint8_t config_number)
{
  dcd_set_config(rhport, config_number);

  usbd_devices[rhport].state = TUSB_DEVICE_STATE_CONFIGURED;
  usbd_devices[rhport].config_num = config_number;

  //------------- parse configuration & open drivers -------------//
  uint8_t const * p_desc_config = _usbd_descs[rhport].configuration;
  TU_ASSERT(p_desc_config != NULL, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

  uint8_t const * p_desc = p_desc_config + sizeof(tusb_desc_configuration_t);

  uint16_t const config_len = ((tusb_desc_configuration_t*)p_desc_config)->wTotalLength;

  while( p_desc < p_desc_config + config_len )
  {
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == p_desc[DESCRIPTOR_OFFSET_TYPE])
    {
      p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // ignore Interface Association
    }else
    {
      TU_ASSERT( TUSB_DESC_INTERFACE == p_desc[DESCRIPTOR_OFFSET_TYPE], TUSB_ERROR_NOT_SUPPORTED_YET );

      tusb_desc_interface_t* p_desc_itf = (tusb_desc_interface_t*) p_desc;
      uint8_t const class_code = p_desc_itf->bInterfaceClass;

      // Check if class is supported
      uint8_t drid;
      for (drid = 0; drid < USBD_CLASS_DRIVER_COUNT; drid++)
      {
        if ( usbd_class_drivers[drid].class_code == class_code ) break;
      }
      TU_ASSERT( drid < USBD_CLASS_DRIVER_COUNT, TUSB_ERROR_NOT_SUPPORTED_YET );

      // Check duplicate interface number TODO support alternate setting
      TU_ASSERT( 0 == usbd_devices[rhport].interface2class[p_desc_itf->bInterfaceNumber], TUSB_ERROR_FAILED);
      usbd_devices[rhport].interface2class[p_desc_itf->bInterfaceNumber] = class_code;

      uint16_t length=0;
      TU_ASSERT_ERR( usbd_class_drivers[drid].open( rhport, p_desc_itf, &length ) );

      TU_ASSERT( length >= sizeof(tusb_desc_interface_t), TUSB_ERROR_FAILED );
      p_desc += length;
    }
  }

  // invoke callback
  tud_mount_cb(rhport);

  return TUSB_ERROR_NONE;
}

static uint16_t get_descriptor(uint8_t rhport, tusb_control_request_t const * const p_request, uint8_t const ** pp_buffer)
{
  tusb_desc_type_t const desc_type = (tusb_desc_type_t) u16_high_u8(p_request->wValue);
  uint8_t const desc_index = u16_low_u8( p_request->wValue );

  uint8_t const * desc_data = NULL ;
  uint16_t len = 0;

  //------------- Descriptor Check -------------//
  tud_desc_init_t const* descs = &_usbd_descs[rhport];

  switch(desc_type)
  {
    case TUSB_DESC_DEVICE:
      desc_data = descs->device;
      len       = sizeof(tusb_desc_device_t);
    break;

    case TUSB_DESC_CONFIGURATION:
      desc_data = descs->configuration;
      len       = ((tusb_desc_configuration_t*)descs->configuration)->wTotalLength;
    break;

    case TUSB_DESC_STRING:
      // windows sometimes ask for string at index 238 !!!
      if ( !(desc_index < 100) ) return 0;

      desc_data = descs->string_arr[desc_index];
      VERIFY( desc_data != NULL, 0 );

      len  = desc_data[0];  // first byte of descriptor is its size
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
  len = min16_of(p_request->wLength, len );
  TU_ASSERT( len <= CFG_TUD_ENUM_BUFFER_SIZE, 0);

  // FIXME copy data to enum buffer
  memcpy(usbd_enum_buffer, desc_data, len);
  (*pp_buffer) = usbd_enum_buffer;

  return len;
}
//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USBD-DCD Callback API
//--------------------------------------------------------------------+
void dcd_bus_event(uint8_t rhport, usbd_bus_event_type_t bus_event)
{
  switch(bus_event)
  {
    case USBD_BUS_EVENT_RESET     :
      memclr_(&usbd_devices[rhport], sizeof(usbd_device_info_t));
      osal_queue_flush(_usbd_q);
      osal_semaphore_reset_isr(_usbd_ctrl_sem);
      for (uint8_t i = 0; i < USBD_CLASS_DRIVER_COUNT; i++)
      {
        if ( usbd_class_drivers[i].close ) usbd_class_drivers[i].close( rhport );
      }
    break;

    case USBD_BUS_EVENT_SOF:
    {
      usbd_task_event_t task_event =
      {
          .rhport          = rhport,
          .event_id        = USBD_EVT_SOF,
      };
      osal_queue_send_isr(_usbd_q, &task_event);
    }
    break;

    case USBD_BUS_EVENT_UNPLUGGED:
      // invoke callback
      tud_umount_cb(rhport);
    break;

    case USBD_BUS_EVENT_SUSPENDED:
      usbd_devices[rhport].state = TUSB_DEVICE_STATE_SUSPENDED;
    break;

    default: break;
  }
}

void dcd_setup_received(uint8_t rhport, uint8_t const* p_request)
{
  usbd_task_event_t task_event =
  {
      .rhport          = rhport,
      .event_id        = USBD_EVT_SETUP_RECEIVED,
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
      .rhport       = 0,
      .event_id     = USBD_EVT_FUNC_CALL,
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
