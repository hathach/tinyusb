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
#include "common/common.h"
#include "hid_device.h"

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

  endpoint_handle_t ept_handle;
  uint8_t interface_number;
}hidd_interface_t;

typedef struct {
  hidd_interface_t * const p_interface;
  void (* const mounted_cb) (uint8_t coreid);
  void (* const unmounted_cb) (uint8_t coreid);
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
        .mounted_cb    = tusbd_hid_keyboard_mounted_cb,
        .unmounted_cb  = tusbd_hid_keyboard_unmounted_cb,
        .xfer_cb       = tusbd_hid_keyboard_cb,
        .get_report_cb = tusbd_hid_keyboard_get_report_cb,
        .set_report_cb = tusbd_hid_keyboard_set_report_cb
    },
#endif

#if TUSB_CFG_DEVICE_HID_MOUSE
    [HID_PROTOCOL_MOUSE] =
    {
        .p_interface   = &moused_data,
        .mounted_cb    = tusbd_hid_mouse_mounted_cb,
        .unmounted_cb  = tusbd_hid_mouse_unmounted_cb,
        .xfer_cb       = tusbd_hid_mouse_cb,
        .get_report_cb = tusbd_hid_mouse_get_report_cb,
        .set_report_cb = tusbd_hid_mouse_set_report_cb
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

bool tusbd_hid_keyboard_is_busy(uint8_t coreid)
{
  return dcd_pipe_is_busy(keyboardd_data.ept_handle);
}

tusb_error_t tusbd_hid_keyboard_send(uint8_t coreid, hid_keyboard_report_t const *p_report)
{
  ASSERT(tusbd_is_configured(coreid), TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED);

  hidd_interface_t * p_kbd = &keyboardd_data; // TODO &keyboardd_data[coreid];

  ASSERT_STATUS( dcd_pipe_xfer(p_kbd->ept_handle, (void*) p_report, sizeof(hid_keyboard_report_t), true) ) ;

  return TUSB_ERROR_NONE;
}
#endif

//--------------------------------------------------------------------+
// MOUSE APPLICATION API
//--------------------------------------------------------------------+
#if TUSB_CFG_DEVICE_HID_MOUSE
STATIC_VAR hidd_interface_t moused_data;

bool tusbd_hid_mouse_is_busy(uint8_t coreid)
{
  return dcd_pipe_is_busy(moused_data.ept_handle);
}

tusb_error_t tusbd_hid_mouse_send(uint8_t coreid, hid_mouse_report_t const *p_report)
{
  ASSERT(tusbd_is_configured(coreid), TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED);

  hidd_interface_t * p_mouse = &moused_data; // TODO &keyboardd_data[coreid];

  ASSERT_STATUS( dcd_pipe_xfer(p_mouse->ept_handle, (void*) p_report, sizeof(hid_mouse_report_t), true) ) ;

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

void hidd_close(uint8_t coreid)
{
  for(uint8_t i=0; i<HIDD_NUMBER_OF_SUBCLASS; i++)
  {
    interface_clear(hidd_class_driver[i].p_interface);
    if ( hidd_class_driver[i].unmounted_cb ) hidd_class_driver[i].unmounted_cb(coreid);
  }
}

tusb_error_t hidd_control_request_subtask(uint8_t coreid, tusb_control_request_t const * p_request)
{
  uint8_t subclass_idx;
  for(subclass_idx=0; subclass_idx<HIDD_NUMBER_OF_SUBCLASS; subclass_idx++)
  {
    hidd_interface_t * const p_interface = hidd_class_driver[subclass_idx].p_interface;
    if ( (p_interface != NULL) && (p_request->wIndex == p_interface->interface_number) ) break;
  }

  ASSERT(subclass_idx < HIDD_NUMBER_OF_SUBCLASS, TUSB_ERROR_FAILED);

  hidd_class_driver_t const * const p_driver = &hidd_class_driver[subclass_idx];
  hidd_interface_t* const p_hid = p_driver->p_interface;

  //------------- STD Request -------------//
  if (p_request->bmRequestType_bit.type == TUSB_REQUEST_TYPE_STANDARD)
  {
    uint8_t const desc_type  = u16_high_u8(p_request->wValue);
    uint8_t const desc_index = u16_low_u8 (p_request->wValue);

    (void) desc_index;

    ASSERT ( p_request->bRequest == TUSB_REQUEST_GET_DESCRIPTOR && desc_type == HID_DESC_TYPE_REPORT,
             TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);
    ASSERT ( p_hid->report_length <= HIDD_BUFFER_SIZE, TUSB_ERROR_NOT_ENOUGH_MEMORY);

    memcpy(m_hid_buffer, p_hid->p_report_desc, p_hid->report_length); // to allow report descriptor not to be in USBRAM
    dcd_pipe_control_xfer(coreid, TUSB_DIR_DEV_TO_HOST, m_hid_buffer, p_hid->report_length, false);
  }
  //------------- Class Specific Request -------------//
  else if (p_request->bmRequestType_bit.type == TUSB_REQUEST_TYPE_CLASS)
  {
    OSAL_SUBTASK_BEGIN

    if( (HID_REQUEST_CONTROL_GET_REPORT == p_request->bRequest) && (p_driver->get_report_cb != NULL) )
    {
      // wValue = Report Type | Report ID
      void* p_buffer = NULL;

      uint16_t actual_length = p_driver->get_report_cb(coreid, (hid_request_report_type_t) u16_high_u8(p_request->wValue),
                                                       &p_buffer, p_request->wLength);
      SUBTASK_ASSERT( p_buffer != NULL && actual_length > 0 );

      dcd_pipe_control_xfer(coreid, (tusb_direction_t) p_request->bmRequestType_bit.direction, p_buffer, actual_length, false);
    }
    else if ( (HID_REQUEST_CONTROL_SET_REPORT == p_request->bRequest) && (p_driver->set_report_cb != NULL) )
    {
      //        return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT; // TODO test STALL control out endpoint (with mouse+keyboard)
      // wValue = Report Type | Report ID
      tusb_error_t error;

      dcd_pipe_control_xfer(coreid, (tusb_direction_t) p_request->bmRequestType_bit.direction, m_hid_buffer, p_request->wLength, true);

      osal_semaphore_wait(usbd_control_xfer_sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // wait for control xfer complete
      SUBTASK_ASSERT_STATUS(error);

      p_driver->set_report_cb(coreid, (hid_request_report_type_t) u16_high_u8(p_request->wValue),
                              m_hid_buffer, p_request->wLength);
    }
    else if (HID_REQUEST_CONTROL_SET_IDLE == p_request->bRequest)
    {
      // uint8_t idle_rate = u16_high_u8(p_request->wValue);
    }else
    {
//      HID_REQUEST_CONTROL_GET_IDLE:
//      HID_REQUEST_CONTROL_GET_PROTOCOL:
//      HID_REQUEST_CONTROL_SET_PROTOCOL:
      return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
    }

    OSAL_SUBTASK_END
  }else
  {
    return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t hidd_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length)
{
  uint8_t const *p_desc = (uint8_t const *) p_interface_desc;

  //------------- HID descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_hid_descriptor_hid_t const *p_desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  ASSERT_INT(HID_DESC_TYPE_HID, p_desc_hid->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

  //------------- Endpoint Descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_descriptor_endpoint_t const *p_desc_endpoint = (tusb_descriptor_endpoint_t const *) p_desc;
  ASSERT_INT(TUSB_DESC_TYPE_ENDPOINT, p_desc_endpoint->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

  if (p_interface_desc->bInterfaceSubClass == HID_SUBCLASS_BOOT)
  {
    switch(p_interface_desc->bInterfaceProtocol)
    {
      case HID_PROTOCOL_KEYBOARD:
      case HID_PROTOCOL_MOUSE:
      {
        hidd_class_driver_t const * const p_driver = &hidd_class_driver[p_interface_desc->bInterfaceProtocol];
        hidd_interface_t * const p_hid = p_driver->p_interface;

        ASSERT_PTR(p_hid, TUSB_ERROR_FAILED);

        p_hid->ept_handle       = dcd_pipe_open(coreid, p_desc_endpoint, p_interface_desc->bInterfaceClass);
        ASSERT( endpointhandle_is_valid(p_hid->ept_handle), TUSB_ERROR_DCD_FAILED);

        p_hid->interface_number = p_interface_desc->bInterfaceNumber;
        p_hid->p_report_desc    = (p_interface_desc->bInterfaceProtocol == HID_PROTOCOL_KEYBOARD) ? tusbd_descriptor_pointers.p_hid_keyboard_report : tusbd_descriptor_pointers.p_hid_mouse_report;
        p_hid->report_length    = p_desc_hid->wReportLength;

        ASSERT_PTR(p_hid->p_report_desc, TUSB_ERROR_DESCRIPTOR_CORRUPTED);
        p_driver->mounted_cb(coreid);
      }
      break;

      default: // TODO unknown, unsupported protocol --> skip this interface
        return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
    }
    *p_length = sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t);
  }else
  {
    // open generic
    *p_length = 0;
    return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
  }
  return TUSB_ERROR_NONE;
}

tusb_error_t hidd_xfer_cb(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
  for(uint8_t i=0; i<HIDD_NUMBER_OF_SUBCLASS; i++)
  {
    hidd_interface_t * const p_interface = hidd_class_driver[i].p_interface;
    if ( (p_interface != NULL) && endpointhandle_is_equal(edpt_hdl, p_interface->ept_handle) )
    {
      hidd_class_driver[i].xfer_cb(edpt_hdl.coreid, event, xferred_bytes);
    }
  }

  return TUSB_ERROR_NONE;
}

#endif
