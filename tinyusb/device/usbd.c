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
static device_class_driver_t const usbd_class_drivers[TUSB_CLASS_MAPPED_INDEX_START] =
{
#if DEVICE_CLASS_HID
    [TUSB_CLASS_HID] = {
        .init            = hidd_init,
        .control_request = hidd_control_request,
    },
#endif
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static tusb_error_t usbd_string_descriptor_init(void);

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
void usbd_bus_reset(uint32_t coreid)
{
  memclr_(usbd_devices, sizeof(usbd_device_info_t)*CONTROLLER_DEVICE_NUMBER);
}

void std_get_descriptor(uint8_t coreid)
{
  tusb_std_descriptor_type_t const desc_type = usbd_devices[coreid].setup_packet.wValue >> 8;
  uint8_t const desc_index = u16_low_u8( usbd_devices[coreid].setup_packet.wValue );
  switch ( desc_type )
  {
    case TUSB_DESC_TYPE_DEVICE:
      dcd_pipe_control_write(coreid, &app_tusb_desc_device, sizeof(tusb_descriptor_device_t));
    break;

    case TUSB_DESC_TYPE_CONFIGURATION:
    {
      uint16_t const requested_length = min16_of(usbd_devices[coreid].setup_packet.wLength, sizeof(app_tusb_desc_configuration)-1);
      ASSERT(requested_length <= TUSB_CFG_DEVICE_CONTROL_PACKET_SIZE, (void)0 ); // multiple packets requires a task a-like

      dcd_pipe_control_write(coreid, &app_tusb_desc_configuration,
                             requested_length);
    }
    break;

    case TUSB_DESC_TYPE_STRING:
    {
      uint8_t *p_string = (uint8_t*) &app_tusb_desc_strings;
      for(uint8_t index =0; index < desc_index; index++)
      {
        p_string += (*p_string);
      }
      dcd_pipe_control_write(coreid, p_string, *p_string);
    }
    break;

    default:
//      ASSERT(false, (void) 0); // descriptors that is not supported yet
    return;
  }
}

void usbd_setup_received(uint8_t coreid)
{
  usbd_device_info_t *p_device = &usbd_devices[coreid];

  // check device configured TODO
  if ( p_device->setup_packet.bmRequestType_bit.recipient == TUSB_REQUEST_RECIPIENT_INTERFACE)
  {
    // TODO detect which class
//    ASSERT( p_device->setup_packet.wIndex < USBD_MAX_INTERFACE, (void) 0);
//    tusb_std_class_code_t const class_code = usbd_devices[coreid].interface2class[ p_device->setup_packet.wIndex ];
//    if ( usbd_class_drivers[class_code].control_request != NULL )
//    {
//      usbd_class_drivers[class_code].control_request(coreid, &p_device->setup_packet);
//
//    }
    hidd_control_request(coreid, &p_device->setup_packet);
  }else
  {
    switch ( p_device->setup_packet.bRequest )
    {
      case TUSB_REQUEST_GET_DESCRIPTOR:
        std_get_descriptor(coreid);
      break;

      case TUSB_REQUEST_SET_ADDRESS:
        p_device->address = (uint8_t) p_device->setup_packet.wValue;
        dcd_device_set_address(coreid, p_device->address);
        usbd_devices[coreid].state = TUSB_DEVICE_STATE_ADDRESSED;
      break;

      case TUSB_REQUEST_SET_CONFIGURATION:
        dcd_device_set_configuration(coreid, (uint8_t) p_device->setup_packet.wValue);
        usbd_devices[coreid].state = TUSB_DEVICE_STATE_CONFIGURED;
      break;

      default:
        return;
    }
  }

  if (p_device->setup_packet.bmRequestType_bit.direction == TUSB_DIR_HOST_TO_DEV)
  {
    dcd_pipe_control_write_zero_length(coreid);
  }
}

tusb_error_t usbd_init (void)
{
  ASSERT_STATUS ( usbd_string_descriptor_init() );

  ASSERT_STATUS ( dcd_init() );

  uint16_t length = 0;

  #if TUSB_CFG_DEVICE_HID_KEYBOARD
  tusb_descriptor_interface_t const * p_interface = &app_tusb_desc_configuration.keyboard_interface;
  ASSERT_STATUS( hidd_init(0, p_interface, &length) );
  usbd_devices[0].interface2class[p_interface->bInterfaceNumber] = p_interface->bInterfaceClass;
  #endif

  #if TUSB_CFG_DEVICE_HID_MOUSE
  ASSERT_STATUS( hidd_init(0, &app_tusb_desc_configuration.mouse_interface, &length) );
  #endif

  usbd_bus_reset(0);

  #ifndef _TEST_
  hal_interrupt_enable(0); // TODO USB1
  #endif

  dcd_controller_connect(0);  // TODO USB1

  return TUSB_ERROR_NONE;
}



//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
tusb_error_t usbd_pipe_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interfacae, tusb_descriptor_endpoint_t const * p_endpoint_desc)
{
  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// callback from DCD ISR
//--------------------------------------------------------------------+
void usbd_isr(uint8_t coreid, tusb_event_t event)
{
  switch(event)
  {
    case TUSB_EVENT_BUS_RESET:
      usbd_bus_reset(coreid);
    break;

    case TUSB_EVENT_SETUP_RECEIVED:
      usbd_setup_received(coreid);
    break;

    default:
      ASSERT(false, (void) 0);
    break;
  }
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
static tusb_error_t usbd_string_descriptor_init(void)
{
  ASSERT_INT( STRING_LEN_BYTE2UNICODE(sizeof(TUSB_CFG_DEVICE_STRING_MANUFACTURER)-1),
             app_tusb_desc_strings.manufacturer.bLength, TUSB_ERROR_USBD_DESCRIPTOR_STRING);

  ASSERT_INT( STRING_LEN_BYTE2UNICODE(sizeof(TUSB_CFG_DEVICE_STRING_PRODUCT)-1)     ,
             app_tusb_desc_strings.product.bLength     , TUSB_ERROR_USBD_DESCRIPTOR_STRING);

  ASSERT_INT( STRING_LEN_BYTE2UNICODE(sizeof(TUSB_CFG_DEVICE_STRING_SERIAL)-1)      ,
              app_tusb_desc_strings.serial.bLength      , TUSB_ERROR_USBD_DESCRIPTOR_STRING);

  for(uint32_t i=0; i < sizeof(TUSB_CFG_DEVICE_STRING_MANUFACTURER)-1; i++)
  {
    app_tusb_desc_strings.manufacturer.unicode_string[i] = (uint16_t) TUSB_CFG_DEVICE_STRING_MANUFACTURER[i];
  }

  for(uint32_t i=0; i < sizeof(TUSB_CFG_DEVICE_STRING_PRODUCT)-1; i++)
  {
    app_tusb_desc_strings.product.unicode_string[i] = (uint16_t) TUSB_CFG_DEVICE_STRING_PRODUCT[i];
  }

  for(uint32_t i=0; i < sizeof(TUSB_CFG_DEVICE_STRING_SERIAL)-1; i++)
  {
    app_tusb_desc_strings.serial.unicode_string[i] = (uint16_t) TUSB_CFG_DEVICE_STRING_SERIAL[i];
  }

  return TUSB_ERROR_NONE;
}

#endif
