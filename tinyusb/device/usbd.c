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
#include "tusb_descriptors.h" // TODO callback include
#include "usbd_dcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
usbd_device_info_t usbd_devices[CONTROLLER_DEVICE_NUMBER];

// TODO fix/compress number of class driver
static usbd_class_driver_t const usbd_class_drivers[TUSB_CLASS_MAPPED_INDEX_START] =
{
#if DEVICE_CLASS_HID
    [TUSB_CLASS_HID] =
    {
        .init            = hidd_init,
        .open            = hidd_open,
        .control_request = hidd_control_request,
        .isr             = hidd_isr,
        .bus_reset       = hidd_bus_reset
    },
#endif

#if TUSB_CFG_DEVICE_MSC
    [TUSB_CLASS_MSC] =
    {
        .init            = mscd_init,
        .open            = mscd_open,
        .control_request = mscd_control_request,
        .isr             = mscd_isr,
        .bus_reset       = mscd_bus_reset
    },
#endif

#if TUSB_CFG_DEVICE_CDC
    [TUSB_CLASS_CDC] =
    {
        .init            = cdcd_init,
        .open            = cdcd_open,
        .control_request = cdcd_control_request,
        .isr             = cdcd_isr,
        .bus_reset       = cdcd_bus_reset
    },
#endif

};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
tusb_error_t usbd_set_configure_received(uint8_t coreid, uint8_t config_number);
tusb_error_t get_descriptor_subtask(uint8_t coreid, tusb_control_request_t * p_request);

//--------------------------------------------------------------------+
// APPLICATION INTERFACE
//--------------------------------------------------------------------+
bool tusbd_is_configured(uint8_t coreid)
{
  return usbd_devices[coreid].state == TUSB_DEVICE_STATE_CONFIGURED;
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

//------------- OSAL Task -------------//
enum {
  USBD_TASK_QUEUE_DEPTH = 8
};

typedef enum {
  USBD_EVENTID_SETUP_RECEIVED = 1
}usbd_eventid_t;

typedef struct {
  uint8_t coreid;
  uint8_t event_id;
  uint8_t reserved[2];
}usbd_task_event_t;

OSAL_TASK_DEF(usbd_task, 150, TUSB_CFG_OS_TASK_PRIO);
OSAL_QUEUE_DEF(usbd_queue_def, USBD_TASK_QUEUE_DEPTH, usbd_task_event_t);

static osal_queue_handle_t usbd_queue_hdl;

tusb_error_t usbd_body_subtask(void)
{
  OSAL_SUBTASK_BEGIN

  tusb_error_t error = TUSB_ERROR_NONE;
  usbd_task_event_t event;

  osal_queue_receive(usbd_queue_hdl, &event, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  SUBTASK_ASSERT_STATUS(error);

  if ( USBD_EVENTID_SETUP_RECEIVED == event.event_id )
  {
    usbd_device_info_t *p_device      = &usbd_devices[event.coreid];
    tusb_control_request_t* p_request = &p_device->control_request;

    //------------- Standard Control such as those in enumeration -------------//
    if( TUSB_REQUEST_RECIPIENT_DEVICE == p_request->bmRequestType_bit.recipient &&
        TUSB_REQUEST_TYPE_STANDARD    == p_request->bmRequestType_bit.type )
    {
      if ( TUSB_REQUEST_GET_DESCRIPTOR == p_request->bRequest )
      {
        error = get_descriptor_subtask(event.coreid, p_request);
      }
      else if ( TUSB_REQUEST_SET_ADDRESS == p_request->bRequest )
      {
        dcd_controller_set_address(event.coreid, (uint8_t) p_request->wValue);
        p_device->state = TUSB_DEVICE_STATE_ADDRESSED;
      }
      else if ( TUSB_REQUEST_SET_CONFIGURATION == p_request->bRequest )
      {
        usbd_set_configure_received(event.coreid, (uint8_t) p_request->wValue);
      }else
      {
        error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
      }
    }
    //------------- Class/Interface Specific Request -------------//
    else if ( TUSB_REQUEST_RECIPIENT_INTERFACE == p_request->bmRequestType_bit.recipient)
    {
      tusb_std_class_code_t class_code = p_device->interface2class[ u16_low_u8(p_request->wIndex) ];

      if ( (TUSB_CLASS_AUDIO <= class_code) && (class_code <= TUSB_CLASS_AUDIO_VIDEO) &&
           usbd_class_drivers[class_code].control_request )
      {
        error = usbd_class_drivers[class_code].control_request(event.coreid, p_request);
      }else
      {
        error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
      }
    }
    //------------- Endpoint Request -------------//
    else if ( TUSB_REQUEST_RECIPIENT_ENDPOINT == p_request->bmRequestType_bit.recipient &&
              TUSB_REQUEST_TYPE_STANDARD == p_request->bmRequestType_bit.type )
    {
      if ( TUSB_REQUEST_CLEAR_FEATURE == p_request->bRequest )
      {
        dcd_pipe_clear_stall(event.coreid, u16_low_u8(p_request->wIndex) );
      } else
      {
        error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
      }
    } else
    {
      error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
    }

    if(TUSB_ERROR_NONE != error)
    { // Response with Protocol Stall if request is not supported
      dcd_pipe_control_stall(event.coreid);
      //    ASSERT(error == TUSB_ERROR_NONE, VOID_RETURN);
    }else
    { // status phase
      dcd_pipe_control_xfer(event.coreid, 1-p_request->bmRequestType_bit.direction, NULL, 0); // zero length
    }
  }

  OSAL_SUBTASK_END
}

// To enable the TASK_ASSERT style (quick return on false condition) in a real RTOS, a task must act as a wrapper
// and is used mainly to call subtasks. Within a subtask return statement can be called freely, the task with
// forever loop cannot have any return at all.
OSAL_TASK_FUNCTION(usbd_task) (void* p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  usbd_body_subtask();

  OSAL_TASK_LOOP_END
}

void usbd_bus_reset(uint32_t coreid)
{
  memclr_(&usbd_devices[coreid], sizeof(usbd_device_info_t));

  for (tusb_std_class_code_t class_code = TUSB_CLASS_AUDIO; class_code <= TUSB_CLASS_AUDIO_VIDEO; class_code++)
  {
    if ( usbd_class_drivers[class_code].bus_reset )
    {
      usbd_class_drivers[class_code].bus_reset( coreid );
    }
  }
}

tusb_error_t usbd_init (void)
{
  ASSERT_STATUS ( dcd_init() );

  //------------- Task init -------------//
  usbd_queue_hdl = osal_queue_create( OSAL_QUEUE_REF(usbd_queue_def) );
  ASSERT_PTR(usbd_queue_hdl, TUSB_ERROR_OSAL_QUEUE_FAILED);
  ASSERT_STATUS( osal_task_create( OSAL_TASK_REF(usbd_task) ));

  //------------- class init -------------//
  for (tusb_std_class_code_t class_code = TUSB_CLASS_AUDIO; class_code <= TUSB_CLASS_AUDIO_VIDEO; class_code++)
  {
    if ( usbd_class_drivers[class_code].init )
    {
      usbd_class_drivers[class_code].init();
    }
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CONTROL REQUEST
//--------------------------------------------------------------------+
// TODO Host (windows) can get HID report descriptor before set configured
// need to open interface before set configured
tusb_error_t usbd_set_configure_received(uint8_t coreid, uint8_t config_number)
{
  dcd_controller_set_configuration(coreid);
  usbd_devices[coreid].state = TUSB_DEVICE_STATE_CONFIGURED;

  //------------- parse configuration & open drivers -------------//
  uint8_t* p_desc_configure = (uint8_t*) &app_tusb_desc_configuration;
  uint8_t* p_desc = p_desc_configure + sizeof(tusb_descriptor_configuration_t);

  while( p_desc < p_desc_configure + ((tusb_descriptor_configuration_t*)p_desc_configure)->wTotalLength )
  {
    ASSERT( TUSB_DESC_TYPE_INTERFACE == p_desc[DESCRIPTOR_OFFSET_TYPE], TUSB_ERROR_NOT_SUPPORTED_YET );

    uint8_t class_index;
    tusb_descriptor_interface_t* p_desc_interface = (tusb_descriptor_interface_t*) p_desc;

    class_index = p_desc_interface->bInterfaceClass;

    ASSERT( class_index != 0 && usbd_class_drivers[class_index].open != NULL, TUSB_ERROR_NOT_SUPPORTED_YET );
    ASSERT( 0 == usbd_devices[coreid].interface2class[p_desc_interface->bInterfaceNumber], TUSB_ERROR_FAILED); // duplicate interface number TODO alternate setting

    usbd_devices[coreid].interface2class[p_desc_interface->bInterfaceNumber] = class_index;

    uint16_t length=0;
    ASSERT_STATUS( usbd_class_drivers[class_index].open( coreid, p_desc_interface, &length ) );

    ASSERT( length >= sizeof(tusb_descriptor_interface_t), TUSB_ERROR_FAILED );
    p_desc += length;
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t get_descriptor_subtask(uint8_t coreid, tusb_control_request_t * p_request)
{
  OSAL_SUBTASK_BEGIN

  static uint8_t const * p_data;
  static uint16_t length;

  tusb_std_descriptor_type_t const desc_type = p_request->wValue >> 8;
  uint8_t const desc_index = u16_low_u8( p_request->wValue );

  if ( TUSB_DESC_TYPE_DEVICE == desc_type )
  {
    p_data = (uint8_t const *) &app_tusb_desc_device;
    length = min16_of( p_request->wLength, sizeof(tusb_descriptor_device_t)) ;
  }
  else if ( TUSB_DESC_TYPE_CONFIGURATION == desc_type )
  {
    p_data = (uint8_t const *) &app_tusb_desc_configuration;
    length = min16_of( p_request->wLength, sizeof(app_tusb_desc_configuration)) ;
  }
  else if ( TUSB_DESC_TYPE_STRING == desc_type )
  {
    if ( ! (desc_index < TUSB_CFG_DEVICE_STRING_DESCRIPTOR_COUNT) ) SUBTASK_EXIT (TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);

    p_data = (uint8_t const *) desc_str_table[desc_index];
    length = min16_of( p_request->wLength, desc_str_table[desc_index]->bLength) ;
  }else
  {
    SUBTASK_EXIT (TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);
  }

  dcd_pipe_control_xfer(coreid, p_request->bmRequestType_bit.direction, p_data, length);

  OSAL_SUBTASK_END
}
//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// DCD Callback API
//--------------------------------------------------------------------+
void usbd_setup_received_isr(uint8_t coreid, tusb_control_request_t * p_request)
{
  usbd_devices[coreid].control_request = (*p_request);

  osal_queue_send(usbd_queue_hdl,
                  &(usbd_task_event_t){
                    .coreid = coreid,
                    .event_id = USBD_EVENTID_SETUP_RECEIVED}
                  );
}

void usbd_xfer_isr(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
//  usbd_device_info_t *p_device = &usbd_devices[edpt_hdl.coreid];
  uint8_t class_index = std_class_code_to_index(edpt_hdl.class_code);

  if (class_index == 0) // Control Transfer
  {

  }else if (usbd_class_drivers[class_index].isr)
  {
    usbd_class_drivers[class_index].isr(edpt_hdl, event, xferred_bytes);
  }else
  {
    ASSERT(false, VOID_RETURN); // something wrong, no one claims the isr's source
  }

}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+

#endif
