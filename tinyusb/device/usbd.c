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

#if (TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_DEVICE)
  dcd_controller_connect(0);
#endif

#if (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_DEVICE)
  dcd_controller_connect(1);
#endif

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
tusb_error_t usbh_set_configure_received(uint8_t coreid, uint8_t config_number)
{
  dcd_controller_set_configuration(coreid, config_number);
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

    //      usbh_devices[new_addr].flag_supported_class |= BIT_(class_index);
    p_desc += length;
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t std_get_descriptor(uint8_t coreid, tusb_control_request_t * p_request)
{
  tusb_std_descriptor_type_t const desc_type = p_request->wValue >> 8;
  uint8_t const desc_index = u16_low_u8( p_request->wValue );

  switch ( desc_type )
  {
    case TUSB_DESC_TYPE_DEVICE:
      dcd_pipe_control_xfer(coreid, TUSB_DIR_DEV_TO_HOST, &app_tusb_desc_device,
                            min16_of( p_request->wLength, sizeof(tusb_descriptor_device_t)) );
    break;

    case TUSB_DESC_TYPE_CONFIGURATION:
      dcd_pipe_control_xfer(coreid, TUSB_DIR_DEV_TO_HOST, &app_tusb_desc_configuration,
                            min16_of( p_request->wLength, sizeof(app_tusb_desc_configuration)) );
    break;

    case TUSB_DESC_TYPE_STRING:
      dcd_pipe_control_xfer(coreid, TUSB_DIR_DEV_TO_HOST, desc_str_table[desc_index], desc_str_table[desc_index]->bLength);
    break;

    default:
      return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
  }

  return TUSB_ERROR_NONE;
}

void usbd_setup_received_isr(uint8_t coreid, tusb_control_request_t * p_request)
{
  usbd_device_info_t *p_device = &usbd_devices[coreid];
  tusb_error_t error = TUSB_ERROR_NONE;

  switch(p_request->bmRequestType_bit.recipient)
  {
    //------------- Standard Control such as those in enumeration -------------//
    case TUSB_REQUEST_RECIPIENT_DEVICE:
      if (p_request->bmRequestType_bit.type != TUSB_REQUEST_TYPE_STANDARD)
      {
        error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
      }else
      {
        switch ( p_request->bRequest )
        {
          case TUSB_REQUEST_GET_DESCRIPTOR:
            error = std_get_descriptor(coreid, p_request);
            break;

          case TUSB_REQUEST_SET_ADDRESS:
            dcd_controller_set_address(coreid, (uint8_t) p_request->wValue);
            usbd_devices[coreid].state = TUSB_DEVICE_STATE_ADDRESSED;

            dcd_pipe_control_xfer(coreid, TUSB_DIR_HOST_TO_DEV, NULL, 0); // zero length
            break;

          case TUSB_REQUEST_SET_CONFIGURATION:
            usbh_set_configure_received(coreid, (uint8_t) p_request->wValue);

            dcd_pipe_control_xfer(coreid, TUSB_DIR_HOST_TO_DEV, NULL, 0); // zero length
            break;

          default: error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT; break;
        }
      }
    break;

    //------------- Class/Interface Specific Reqequest -------------//
    case TUSB_REQUEST_RECIPIENT_INTERFACE:
    {
      tusb_std_class_code_t class_code = p_device->interface2class[ u16_low_u8(p_request->wIndex) ];
      ASSERT_INT_WITHIN(TUSB_CLASS_AUDIO, TUSB_CLASS_AUDIO_VIDEO, class_code, VOID_RETURN);

      if ( usbd_class_drivers[class_code].control_request )
      {
        error = usbd_class_drivers[class_code].control_request(coreid, p_request);
      }
    }
    break;

    //------------- Endpoint Request -------------//
    case TUSB_REQUEST_RECIPIENT_ENDPOINT:
      if (p_request->bmRequestType_bit.type != TUSB_REQUEST_TYPE_STANDARD)
      {
        error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
        break;
      }

      switch ( p_request->bRequest )
      {
        case TUSB_REQUEST_CLEAR_FEATURE:
          dcd_pipe_clear_stall(coreid, u16_low_u8(p_request->wIndex) );
          dcd_pipe_control_xfer(coreid, TUSB_DIR_HOST_TO_DEV, NULL, 0); // zero length
        break;

        default: error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT; break;
      }
    break;

    default: error = TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT; break;
  }

  if(TUSB_ERROR_NONE != error)
  { // Response with Protocol Stall if request is not supported
    dcd_pipe_control_stall(coreid);
    ASSERT(error == TUSB_ERROR_NONE, VOID_RETURN);
  }
}


//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USBD-DCD API
//--------------------------------------------------------------------+
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
