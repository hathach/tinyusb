/**************************************************************************/
/*!
    @file     hid_device.c
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

#if (MODE_DEVICE_SUPPORTED && DEVICE_CLASS_HID)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/tusb_common.h"
#include "hid_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
enum {
  HIDD_NUMBER_OF_SUBCLASS = 3,
  HIDD_BUFFER_SIZE = 128
};

typedef struct {
  uint8_t const * p_report_desc;
  uint16_t report_length;

  uint8_t edpt_addr;
  uint8_t interface_number;
}hidd_interface_t;

typedef struct {
  hidd_interface_t * const p_interface;
  void (* const xfer_cb) (uint8_t, tusb_event_t, uint32_t);
  uint16_t (* const get_report_cb) (uint8_t, hid_request_report_type_t, void**, uint16_t );
  void (* const set_report_cb) (uint8_t, hid_request_report_type_t, uint8_t[], uint16_t);
}hidd_class_driver_t;

extern ATTR_WEAK hidd_interface_t keyboardd_data;
extern ATTR_WEAK hidd_interface_t moused_data;

static hidd_class_driver_t const hidd_class_driver[HIDD_NUMBER_OF_SUBCLASS] =
{
//    [HID_PROTOCOL_NONE]  = for HID Generic

#if TUSB_CFG_DEVICE_HID_KEYBOARD
    [HID_PROTOCOL_KEYBOARD] =
    {
        .p_interface   = &keyboardd_data,
        .xfer_cb       = tud_hid_keyboard_cb,
        .get_report_cb = tud_hid_keyboard_get_report_cb,
        .set_report_cb = tud_hid_keyboard_set_report_cb
    },
#endif

#if TUSB_CFG_DEVICE_HID_MOUSE
    [HID_PROTOCOL_MOUSE] =
    {
        .p_interface   = &moused_data,
        .xfer_cb       = tud_hid_mouse_cb,
        .get_report_cb = tud_hid_mouse_get_report_cb,
        .set_report_cb = tud_hid_mouse_set_report_cb
    }
#endif
};

// internal buffer for transferring data
TUSB_CFG_ATTR_USBRAM STATIC_VAR uint8_t m_hid_buffer[ HIDD_BUFFER_SIZE ];

//--------------------------------------------------------------------+
// KEYBOARD APPLICATION API
//--------------------------------------------------------------------+
#if TUSB_CFG_DEVICE_HID_KEYBOARD
STATIC_VAR hidd_interface_t keyboardd_data;

bool tud_hid_keyboard_busy(uint8_t rhport)
{
  return dcd_edpt_busy(rhport, keyboardd_data.edpt_addr);
}

tusb_error_t tud_hid_keyboard_send(uint8_t rhport, hid_keyboard_report_t const *p_report)
{
  VERIFY(tud_mounted(), TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED);

  hidd_interface_t * p_kbd = &keyboardd_data; // TODO &keyboardd_data[rhport];

  TU_ASSERT( dcd_edpt_xfer(rhport, p_kbd->edpt_addr, (void*) p_report, sizeof(hid_keyboard_report_t)), TUSB_ERROR_DCD_EDPT_XFER ) ;

  return TUSB_ERROR_NONE;
}
#endif

//--------------------------------------------------------------------+
// MOUSE APPLICATION API
//--------------------------------------------------------------------+
#if TUSB_CFG_DEVICE_HID_MOUSE
STATIC_VAR hidd_interface_t moused_data;

bool tud_hid_mouse_is_busy(uint8_t rhport)
{
  return dcd_edpt_busy(rhport, moused_data.edpt_addr);
}

tusb_error_t tud_hid_mouse_send(uint8_t rhport, hid_mouse_report_t const *p_report)
{
  VERIFY(tud_mounted(), TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED);

  hidd_interface_t * p_mouse = &moused_data; // TODO &keyboardd_data[rhport];

  TU_ASSERT( dcd_edpt_xfer(rhport, p_mouse->edpt_addr, (void*) p_report, sizeof(hid_mouse_report_t)), TUSB_ERROR_DCD_EDPT_XFER ) ;

  return TUSB_ERROR_NONE;
}
#endif

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
static void interface_clear(hidd_interface_t * p_interface)
{
  if ( p_interface != NULL )
  {
    memclr_(p_interface, sizeof(hidd_interface_t));
    p_interface->interface_number = INTERFACE_INVALID_NUMBER;
  }
}

void hidd_init(void)
{
  for(uint8_t i=0; i<HIDD_NUMBER_OF_SUBCLASS; i++)
  {
    interface_clear( hidd_class_driver[i].p_interface );
  }
}

void hidd_close(uint8_t rhport)
{
  for(uint8_t i=0; i<HIDD_NUMBER_OF_SUBCLASS; i++)
  {
    interface_clear(hidd_class_driver[i].p_interface);
  }
}

tusb_error_t hidd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request)
{
  uint8_t subclass_idx;
  for(subclass_idx=0; subclass_idx<HIDD_NUMBER_OF_SUBCLASS; subclass_idx++)
  {
    hidd_interface_t * const p_interface = hidd_class_driver[subclass_idx].p_interface;
    if ( (p_interface != NULL) && (p_request->wIndex == p_interface->interface_number) ) break;
  }

  TU_ASSERT(subclass_idx < HIDD_NUMBER_OF_SUBCLASS, TUSB_ERROR_FAILED);

  hidd_class_driver_t const * const p_driver = &hidd_class_driver[subclass_idx];
  hidd_interface_t* const p_hid = p_driver->p_interface;

  OSAL_SUBTASK_BEGIN

  tusb_error_t err;

  //------------- STD Request -------------//
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD)
  {
    uint8_t const desc_type  = u16_high_u8(p_request->wValue);
    uint8_t const desc_index = u16_low_u8 (p_request->wValue);

    (void) desc_index;

    if (p_request->bRequest == TUSB_REQ_GET_DESCRIPTOR && desc_type == HID_DESC_TYPE_REPORT)
    {
      STASK_ASSERT ( p_hid->report_length <= HIDD_BUFFER_SIZE );

      // copy to allow report descriptor not to be in USBRAM
      memcpy(m_hid_buffer, p_hid->p_report_desc, p_hid->report_length);

      STASK_INVOKE( usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, m_hid_buffer, p_hid->report_length), err );
    }else
    {
      dcd_control_stall(rhport);
    }
  }
  //------------- Class Specific Request -------------//
  else if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS)
  {
    if( (HID_REQUEST_CONTROL_GET_REPORT == p_request->bRequest) && (p_driver->get_report_cb != NULL) )
    {
      // wValue = Report Type | Report ID
      void* p_buffer = NULL;

      uint16_t actual_length = p_driver->get_report_cb(rhport, (hid_request_report_type_t) u16_high_u8(p_request->wValue),
                                                       &p_buffer, p_request->wLength);
      STASK_ASSERT( p_buffer != NULL && actual_length > 0 );

      STASK_INVOKE( usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, p_buffer, actual_length), err );
    }
    else if ( (HID_REQUEST_CONTROL_SET_REPORT == p_request->bRequest) && (p_driver->set_report_cb != NULL) )
    {
      //        return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT; // TODO test STALL control out endpoint (with mouse+keyboard)
      // wValue = Report Type | Report ID
      STASK_INVOKE( usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, m_hid_buffer, p_request->wLength), err );
      STASK_ASSERT_ERR(err);

      p_driver->set_report_cb(rhport, u16_high_u8(p_request->wValue), m_hid_buffer, p_request->wLength);
    }
    else if (HID_REQUEST_CONTROL_SET_IDLE == p_request->bRequest)
    {
      // uint8_t idle_rate = u16_high_u8(p_request->wValue);
      dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
    }else
    {
//      HID_REQUEST_CONTROL_GET_IDLE:
//      HID_REQUEST_CONTROL_GET_PROTOCOL:
//      HID_REQUEST_CONTROL_SET_PROTOCOL:
      dcd_control_stall(rhport);
    }
  }else
  {
    dcd_control_stall(rhport);
  }

  OSAL_SUBTASK_END
}

tusb_error_t hidd_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length)
{
  uint8_t const *p_desc = (uint8_t const *) p_interface_desc;

  //------------- HID descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_hid_descriptor_hid_t const *p_desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  TU_ASSERT(HID_DESC_TYPE_HID == p_desc_hid->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

  //------------- Endpoint Descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_desc_endpoint_t const *p_desc_endpoint = (tusb_desc_endpoint_t const *) p_desc;
  TU_ASSERT(TUSB_DESC_ENDPOINT == p_desc_endpoint->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

  if (p_interface_desc->bInterfaceSubClass == HID_SUBCLASS_BOOT)
  {
    switch(p_interface_desc->bInterfaceProtocol)
    {
      case HID_PROTOCOL_KEYBOARD:
      case HID_PROTOCOL_MOUSE:
      {
        hidd_class_driver_t const * const p_driver = &hidd_class_driver[p_interface_desc->bInterfaceProtocol];
        hidd_interface_t * const p_hid = p_driver->p_interface;

        VERIFY(p_hid, TUSB_ERROR_FAILED);

        VERIFY( dcd_edpt_open(rhport, p_desc_endpoint), TUSB_ERROR_DCD_FAILED );

        p_hid->edpt_addr = p_desc_endpoint->bEndpointAddress;

        p_hid->interface_number = p_interface_desc->bInterfaceNumber;
        p_hid->p_report_desc    = (p_interface_desc->bInterfaceProtocol == HID_PROTOCOL_KEYBOARD) ? tusbd_descriptor_pointers.p_hid_keyboard_report : tusbd_descriptor_pointers.p_hid_mouse_report;
        p_hid->report_length    = p_desc_hid->wReportLength;

        VERIFY(p_hid->p_report_desc, TUSB_ERROR_DESCRIPTOR_CORRUPTED);
      }
      break;

      default: // TODO unknown, unsupported protocol --> skip this interface
        return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
    }
    *p_length = sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_desc_endpoint_t);
  }else
  {
    // open generic
    *p_length = 0;
    return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
  }
  return TUSB_ERROR_NONE;
}

tusb_error_t hidd_xfer_cb(uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  for(uint8_t i=0; i<HIDD_NUMBER_OF_SUBCLASS; i++)
  {
    hidd_interface_t * const p_interface = hidd_class_driver[i].p_interface;
    if ( (p_interface != NULL) && (edpt_addr == p_interface->edpt_addr) )
    {
      hidd_class_driver[i].xfer_cb(rhport, event, xferred_bytes);
    }
  }

  return TUSB_ERROR_NONE;
}

#endif
