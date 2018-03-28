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

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"


typedef struct {
  void (* init) (void);
  tusb_error_t (* open)(uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t* p_length);
  tusb_error_t (* control_request_st) (uint8_t rhport, tusb_control_request_t const *);
  tusb_error_t (* xfer_cb) (uint8_t rhport, uint8_t ep_addr, tusb_event_t, uint32_t);
//  void (* routine)(void);
  void (* sof)(uint8_t rhport);
  void (* close) (uint8_t);
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
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
usbd_device_info_t usbd_devices[CONTROLLER_DEVICE_NUMBER];
TUSB_CFG_ATTR_USBRAM ATTR_USB_MIN_ALIGNMENT uint8_t usbd_enum_buffer[TUSB_CFG_DEVICE_ENUM_BUFFER_SIZE];

static usbd_class_driver_t const usbd_class_drivers[] =
{
  #if DEVICE_CLASS_HID
    [TUSB_CLASS_HID] =
    {
        .init                    = hidd_init,
        .open                    = hidd_open,
        .control_request_st = hidd_control_request_st,
        .xfer_cb                 = hidd_xfer_cb,
//        .routine                 = NULL,
        .sof                     = NULL,
        .close                   = hidd_close
    },
  #endif

  #if TUSB_CFG_DEVICE_MSC
    [TUSB_CLASS_MSC] =
    {
        .init                    = mscd_init,
        .open                    = mscd_open,
        .control_request_st = mscd_control_request_st,
        .xfer_cb                 = mscd_xfer_cb,
//        .routine                 = NULL,
        .sof                     = NULL,
        .close                   = mscd_close
    },
  #endif

  #if TUSB_CFG_DEVICE_CDC
    [TUSB_CLASS_CDC] =
    {
        .init                    = cdcd_init,
        .open                    = cdcd_open,
        .control_request_st = cdcd_control_request_st,
        .xfer_cb                 = cdcd_xfer_cb,
//        .routine                 = NULL,
        .sof                     = cdcd_sof,
        .close                   = cdcd_close
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
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static tusb_error_t proc_set_config_req(uint8_t rhport, uint8_t config_number);
static uint16_t get_descriptor(uint8_t rhport, tusb_control_request_t const * const p_request, uint8_t const ** pp_buffer);

//--------------------------------------------------------------------+
// APPLICATION INTERFACE
//--------------------------------------------------------------------+
bool tud_n_mounted(uint8_t rhport)
{
    return usbd_devices[rhport].state == TUSB_DEVICE_STATE_CONFIGURED;
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

//------------- OSAL Task -------------//
enum { USBD_TASK_QUEUE_DEPTH = 16 };

typedef enum
{
  USBD_EVENTID_SETUP_RECEIVED = 1,
  USBD_EVENTID_XFER_DONE,
  USBD_EVENTID_SOF
}usbd_eventid_t;

typedef struct ATTR_ALIGNED(4)
{
  uint8_t rhport;
  uint8_t event_id;
  uint8_t sub_event_id;
  uint8_t reserved;

  union {
    tusb_control_request_t setup_received;

    struct { // USBD_EVENTID_XFER_DONE
      uint8_t  ep_addr;
      uint32_t xferred_byte;
    }xfer_done;
  };
} usbd_task_event_t;

STATIC_ASSERT(sizeof(usbd_task_event_t) <= 12, "size is not correct");

#ifndef TUC_DEVICE_STACKSIZE
#define TUC_DEVICE_STACKSIZE 150
#endif

#ifndef TUSB_CFG_OS_TASK_PRIO
#define TUSB_CFG_OS_TASK_PRIO 0
#endif


static osal_queue_t usbd_queue_hdl;
/*static*/ osal_semaphore_t usbd_control_xfer_sem_hdl; // TODO may need to change to static with wrapper function

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
static tusb_error_t proc_control_request_st(uint8_t rhport, tusb_control_request_t const * const p_request);
static tusb_error_t usbd_main_stk(void);

tusb_error_t usbd_init (void)
{
  #if (TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_DEVICE)
  dcd_init(0);
  #endif

  #if (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_DEVICE)
  dcd_init(1);
  #endif

  //------------- Task init -------------//
  usbd_queue_hdl = osal_queue_create(USBD_TASK_QUEUE_DEPTH, sizeof(usbd_task_event_t));
  VERIFY(usbd_queue_hdl, TUSB_ERROR_OSAL_QUEUE_FAILED);

  usbd_control_xfer_sem_hdl = osal_semaphore_create(1, 0);
  VERIFY(usbd_queue_hdl, TUSB_ERROR_OSAL_SEMAPHORE_FAILED);

  osal_task_create(usbd_task, "usbd", TUC_DEVICE_STACKSIZE, NULL, TUSB_CFG_OS_TASK_PRIO);

  //------------- Descriptor Check -------------//
  ASSERT(tusbd_descriptor_pointers.p_device != NULL && tusbd_descriptor_pointers.p_configuration != NULL, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

  //------------- class init -------------//
  for (uint8_t class_code = TUSB_CLASS_AUDIO; class_code < USBD_CLASS_DRIVER_COUNT; class_code++)
  {
    if ( usbd_class_drivers[class_code].init )
    {
      usbd_class_drivers[class_code].init();
    }
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
  usbd_main_stk();
  OSAL_TASK_END
}

static tusb_error_t usbd_main_stk(void)
{
  static usbd_task_event_t event;

  OSAL_SUBTASK_BEGIN

  tusb_error_t error;
  error = TUSB_ERROR_NONE;

  memclr_(&event, sizeof(usbd_task_event_t));

#if 1
  osal_queue_receive(usbd_queue_hdl, &event, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  STASK_ASSERT_STATUS(error);
#else
  enum { ROUTINE_INTERVAL_MS = 10 };
  osal_queue_receive(usbd_queue_hdl, &event, ROUTINE_INTERVAL_MS, &error);
  if ( error != TUSB_ERROR_NONE )
  {
    // time out, run class routine then
    if ( error == TUSB_ERROR_OSAL_TIMEOUT)
    {
      for (uint8_t class_code = TUSB_CLASS_AUDIO; class_code < USBD_CLASS_DRIVER_COUNT; class_code++)
      {
        if ( usbd_class_drivers[class_code].routine ) usbd_class_drivers[class_code].routine();
      }
    }

    STASK_RETURN(error);
  }
#endif

  if ( USBD_EVENTID_SETUP_RECEIVED == event.event_id )
  {
    STASK_INVOKE( proc_control_request_st(event.rhport, &event.setup_received), error );
  }else if (USBD_EVENTID_XFER_DONE == event.event_id)
  {
    // Call class handling function. Those doest not own the endpoint should check and return
    for (uint8_t class_code = TUSB_CLASS_AUDIO; class_code < USBD_CLASS_DRIVER_COUNT; class_code++)
    {
      if ( usbd_class_drivers[class_code].xfer_cb )
      {
        usbd_class_drivers[class_code].xfer_cb( event.rhport, event.xfer_done.ep_addr, (tusb_event_t) event.sub_event_id, event.xfer_done.xferred_byte);
      }
    }
  }else if (USBD_EVENTID_SOF == event.event_id)
  {
    for (uint8_t class_code = TUSB_CLASS_AUDIO; class_code < USBD_CLASS_DRIVER_COUNT; class_code++)
    {
      if ( usbd_class_drivers[class_code].sof )
      {
        usbd_class_drivers[class_code].sof( event.rhport );
      }
    }
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
tusb_error_t usbd_control_xfer_st(uint8_t rhport, tusb_dir_t dir, uint8_t * buffer, uint16_t length)
{
  OSAL_SUBTASK_BEGIN

  tusb_error_t error;

  // Data
  if ( length )
  {
    dcd_control_xfer(rhport, dir, buffer, length);
    osal_semaphore_wait( usbd_control_xfer_sem_hdl, 100, &error );

    STASK_ASSERT_STATUS( error );
  }

  // Status opposite direction with Zero Length
  // No need to wait for status to complete therefore
  // status phase must not call dcd_control_complete/dcd_xfer_complete
  usbd_control_status(rhport, dir);

  OSAL_SUBTASK_END
}

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
        STASK_INVOKE( usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, (uint8_t*) buffer, len ), error );
      }else
      {
        usbd_control_stall(rhport); // stall unsupported descriptor
      }
    }
    else if (TUSB_REQ_GET_CONFIGURATION == p_request->bRequest )
    {
      memcpy(usbd_enum_buffer, &usbd_devices[rhport].config_num, 1);
      STASK_INVOKE( usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, (uint8_t*) usbd_enum_buffer, 1), error );
    }
    else if ( TUSB_REQ_SET_ADDRESS == p_request->bRequest )
    {
      dcd_set_address(rhport, (uint8_t) p_request->wValue);
      usbd_devices[rhport].state = TUSB_DEVICE_STATE_ADDRESSED;

      #ifndef NRF52840_XXAA // nrf52 auto handle set address, we must not return status
      usbd_control_status(rhport, p_request->bmRequestType_bit.direction);
      #endif
    }
    else if ( TUSB_REQ_SET_CONFIGURATION == p_request->bRequest )
    {
      proc_set_config_req(rhport, (uint8_t) p_request->wValue);
      usbd_control_status(rhport, p_request->bmRequestType_bit.direction);
    }
    else
    {
      usbd_control_stall(rhport); // Stall unsupported request
    }
  }

  //------------- Class/Interface Specific Request -------------//
  else if ( TUSB_REQ_RCPT_INTERFACE == p_request->bmRequestType_bit.recipient)
  {
    static uint8_t class_code;

    class_code = usbd_devices[rhport].interface2class[ u16_low_u8(p_request->wIndex) ];

    // TODO [Custom] TUSB_CLASS_DIAGNOSTIC, vendor etc ...
    if ( (class_code > 0) && (class_code < USBD_CLASS_DRIVER_COUNT) &&
         usbd_class_drivers[class_code].control_request_st )
    {
      STASK_INVOKE( usbd_class_drivers[class_code].control_request_st(rhport, p_request), error );
    }else
    {
      usbd_control_stall(rhport); // Stall unsupported request
    }
  }

  //------------- Endpoint Request -------------//
  else if ( TUSB_REQ_RCPT_ENDPOINT == p_request->bmRequestType_bit.recipient &&
            TUSB_REQ_TYPE_STANDARD == p_request->bmRequestType_bit.type)
  {
    if (TUSB_REQ_CLEAR_FEATURE == p_request->bRequest )
    {
      dcd_edpt_clear_stall(rhport, u16_low_u8(p_request->wIndex) );
      usbd_control_status(rhport, p_request->bmRequestType_bit.direction);
    } else
    {
      usbd_control_stall(rhport); // Stall unsupported request
    }
  }

  //------------- Unsupported Request -------------//
  else
  {
    usbd_control_stall(rhport); // Stall unsupported request
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
  uint8_t const * p_desc_config = tusbd_descriptor_pointers.p_configuration;
  uint8_t const * p_desc = p_desc_config + sizeof(tusb_desc_configuration_t);

  uint16_t const config_total_length = ((tusb_desc_configuration_t*)p_desc_config)->wTotalLength;

  while( p_desc < p_desc_config + config_total_length )
  {
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == p_desc[DESCRIPTOR_OFFSET_TYPE])
    {
      p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // ignore Interface Association
    }else
    {
      ASSERT( TUSB_DESC_INTERFACE == p_desc[DESCRIPTOR_OFFSET_TYPE], TUSB_ERROR_NOT_SUPPORTED_YET );

      uint8_t class_index;
      tusb_desc_interface_t* p_desc_interface = (tusb_desc_interface_t*) p_desc;

      class_index = p_desc_interface->bInterfaceClass;

      ASSERT( class_index != 0 && class_index < USBD_CLASS_DRIVER_COUNT && usbd_class_drivers[class_index].open != NULL, TUSB_ERROR_NOT_SUPPORTED_YET );
      ASSERT( 0 == usbd_devices[rhport].interface2class[p_desc_interface->bInterfaceNumber], TUSB_ERROR_FAILED); // duplicate interface number TODO alternate setting

      usbd_devices[rhport].interface2class[p_desc_interface->bInterfaceNumber] = class_index;

      uint16_t length=0;
      ASSERT_STATUS( usbd_class_drivers[class_index].open( rhport, p_desc_interface, &length ) );

      ASSERT( length >= sizeof(tusb_desc_interface_t), TUSB_ERROR_FAILED );
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

  switch(desc_type)
  {
    case TUSB_DESC_DEVICE:
      desc_data = tusbd_descriptor_pointers.p_device;
      len       = sizeof(tusb_desc_device_t);
    break;

    case TUSB_DESC_CONFIGURATION:
      desc_data = tusbd_descriptor_pointers.p_configuration;
      len       = ((tusb_desc_configuration_t*)tusbd_descriptor_pointers.p_configuration)->wTotalLength;
    break;

    case TUSB_DESC_STRING:
      // windows sometimes ask for string at index 238 !!!
      if ( !(desc_index < 100) ) return 0;

      desc_data = tusbd_descriptor_pointers.p_string_arr[desc_index];
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

  // up to Host's length
  len = min16_of(p_request->wLength, len );
  TU_ASSERT( len <= TUSB_CFG_DEVICE_ENUM_BUFFER_SIZE, 0);

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
      osal_queue_flush(usbd_queue_hdl);
      osal_semaphore_reset(usbd_control_xfer_sem_hdl);
      for (uint8_t class_code = TUSB_CLASS_AUDIO; class_code < USBD_CLASS_DRIVER_COUNT; class_code++)
      {
        if ( usbd_class_drivers[class_code].close ) usbd_class_drivers[class_code].close( rhport );
      }
    break;

    case USBD_BUS_EVENT_SOF:
    {
      usbd_task_event_t task_event =
      {
          .rhport          = rhport,
          .event_id        = USBD_EVENTID_SOF,
      };
      osal_queue_send(usbd_queue_hdl, &task_event);
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
      .event_id        = USBD_EVENTID_SETUP_RECEIVED,
  };

  memcpy(&task_event.setup_received, p_request, sizeof(tusb_control_request_t));
  osal_queue_send(usbd_queue_hdl, &task_event);
}

void dcd_xfer_complete(uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, bool succeeded)
{
  if (ep_addr == 0 )
  {
    (void) rhport;
    (void) xferred_bytes;
    (void) succeeded;

    // Control Transfer
    osal_semaphore_post( usbd_control_xfer_sem_hdl );
  }else
  {
    usbd_task_event_t task_event =
    {
        .rhport         = rhport,
        .event_id     = USBD_EVENTID_XFER_DONE,
        .sub_event_id = succeeded ? TUSB_EVENT_XFER_COMPLETE : TUSB_EVENT_XFER_ERROR
    };

    task_event.xfer_done.ep_addr      = ep_addr;
    task_event.xfer_done.xferred_byte = xferred_bytes;

    osal_queue_send(usbd_queue_hdl, &task_event);
  }
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+

#endif
