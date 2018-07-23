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

#if (TUSB_OPT_DEVICE_ENABLED && TUD_OPT_HID_ENABLED)

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

// Max report len is keyboard's one with 8 byte + 1 byte report id
#define REPORT_BUFSIZE      9

typedef struct {
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t idle_rate;

  uint8_t  report_id;
  uint16_t report_len;
  uint8_t const * report_desc;

  // class specific control request
  uint16_t (*get_report_cb) (hid_report_type_t type, uint8_t* buffer, uint16_t reqlen);
  void     (*set_report_cb) (hid_report_type_t type, uint8_t const* buffer, uint16_t bufsize);

  CFG_TUSB_MEM_ALIGN uint8_t report_buf[REPORT_BUFSIZE];
}hidd_interface_t;

#if CFG_TUD_HID_BOOT_PROTOCOL

CFG_TUSB_ATTR_USBRAM static hidd_interface_t _kbd_itf;
CFG_TUSB_ATTR_USBRAM static hidd_interface_t _mse_itf;

#else

CFG_TUSB_ATTR_USBRAM static hidd_interface_t _composite_itf;

#endif

//--------------------------------------------------------------------+
// KEYBOARD APPLICATION API
//--------------------------------------------------------------------+
#if CFG_TUD_HID_KEYBOARD
bool tud_hid_keyboard_busy(void)
{
  return dcd_edpt_busy(TUD_OPT_RHPORT, _kbd_itf.ep_in);
}

tusb_error_t tud_hid_keyboard_send(hid_keyboard_report_t const *p_report)
{
  VERIFY(tud_mounted(), TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED);

  hidd_interface_t * p_kbd = &_kbd_itf;

  TU_ASSERT( dcd_edpt_xfer(TUD_OPT_RHPORT, p_kbd->ep_in, (void*) p_report, sizeof(hid_keyboard_report_t)), TUSB_ERROR_DCD_EDPT_XFER ) ;

  return TUSB_ERROR_NONE;
}
#endif

//--------------------------------------------------------------------+
// MOUSE APPLICATION API
//--------------------------------------------------------------------+
#if CFG_TUD_HID_MOUSE
bool tud_hid_mouse_is_busy(void)
{
  return dcd_edpt_busy(TUD_OPT_RHPORT, _mse_itf.ep_in);
}

tusb_error_t tud_hid_mouse_send(hid_mouse_report_t const *p_report)
{
  VERIFY(tud_mounted(), TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED);

  hidd_interface_t * p_mouse = &_mse_itf;

  TU_ASSERT( dcd_edpt_xfer(TUD_OPT_RHPORT, p_mouse->ep_in, (void*) p_report, sizeof(hid_mouse_report_t)), TUSB_ERROR_DCD_EDPT_XFER ) ;

  return TUSB_ERROR_NONE;
}
#endif

static inline hidd_interface_t* get_interface_by_edpt(uint8_t ep_addr)
{
  return ( ep_addr == _kbd_itf.ep_in ) ? &_kbd_itf :
         ( ep_addr == _mse_itf.ep_in ) ? &_mse_itf : NULL;
}

static inline hidd_interface_t* get_interface_by_number(uint8_t itf_num)
{
  return ( itf_num == _kbd_itf.itf_num ) ? &_kbd_itf :
         ( itf_num == _mse_itf.itf_num ) ? &_mse_itf : NULL;
}

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void hidd_init(void)
{
  hidd_reset(TUD_OPT_RHPORT);
}

void hidd_reset(uint8_t rhport)
{
  #if CFG_TUD_HID_MOUSE
  varclr_(&_mse_itf);
  #endif

  #if CFG_TUD_HID_KEYBOARD
  varclr_(&_kbd_itf);
  #endif
}

tusb_error_t hidd_open(uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t *p_length)
{
  uint8_t const *p_desc = (uint8_t const *) desc_itf;

  //------------- HID descriptor -------------//
  p_desc += p_desc[DESC_OFFSET_LEN];
  tusb_hid_descriptor_hid_t const *desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  TU_ASSERT(HID_DESC_TYPE_HID == desc_hid->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

  //------------- Endpoint Descriptor -------------//
  p_desc += p_desc[DESC_OFFSET_LEN];
  tusb_desc_endpoint_t const *desc_edpt = (tusb_desc_endpoint_t const *) p_desc;
  TU_ASSERT(TUSB_DESC_ENDPOINT == desc_edpt->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

  if (desc_itf->bInterfaceSubClass == HID_SUBCLASS_BOOT)
  {
#if CFG_TUD_HID_BOOT_PROTOCOL
    if ( (desc_itf->bInterfaceProtocol != HID_PROTOCOL_KEYBOARD) && (desc_itf->bInterfaceProtocol != HID_PROTOCOL_MOUSE) )
    {
      // unknown, unsupported protocol
      return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
    }else
    {
      hidd_interface_t * p_hid = NULL;

      #if CFG_TUD_HID_KEYBOARD
      if (desc_itf->bInterfaceProtocol == HID_PROTOCOL_KEYBOARD)
      {
        p_hid = &_kbd_itf;
        p_hid->report_desc   = tud_desc_set.hid_report.boot_keyboard;
        p_hid->get_report_cb = tud_hid_keyboard_get_report_cb;
        p_hid->set_report_cb = tud_hid_keyboard_set_report_cb;
      }
      #endif

      #if CFG_TUD_HID_MOUSE
      if (desc_itf->bInterfaceProtocol == HID_PROTOCOL_MOUSE)
      {
        p_hid = &_mse_itf;
        p_hid->report_desc   = tud_desc_set.hid_report.boot_mouse;
        p_hid->get_report_cb = tud_hid_mouse_get_report_cb;
        p_hid->set_report_cb = tud_hid_mouse_set_report_cb;
      }
      #endif

      TU_ASSERT(p_hid, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);
      VERIFY(p_hid->report_desc, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

      TU_ASSERT( dcd_edpt_open(rhport, desc_edpt), TUSB_ERROR_DCD_FAILED );

      p_hid->report_len = desc_hid->wReportLength;
      p_hid->itf_num    = desc_itf->bInterfaceNumber;
      p_hid->ep_in      = desc_edpt->bEndpointAddress;
      p_hid->report_id  = 0;

      *p_length = sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_desc_endpoint_t);
    }
#else
    return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
#endif
  }
  else
  {
    // TODO HID generic
    // TODO parse report ID for keyboard, mouse
    *p_length = 0;
    return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t hidd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request)
{
  hidd_interface_t* p_hid = get_interface_by_number( (uint8_t) p_request->wIndex );
  TU_ASSERT(p_hid, TUSB_ERROR_FAILED);

  OSAL_SUBTASK_BEGIN

  //------------- STD Request -------------//
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD)
  {
    uint8_t const desc_type  = u16_high_u8(p_request->wValue);
    uint8_t const desc_index = u16_low_u8 (p_request->wValue);

    (void) desc_index;

    if (p_request->bRequest == TUSB_REQ_GET_DESCRIPTOR && desc_type == HID_DESC_TYPE_REPORT)
    {
      STASK_ASSERT ( p_hid->report_len <= CFG_TUD_CTRL_BUFSIZE );

      // use device control buffer (in USB SRAM)
      memcpy(_usbd_ctrl_buf, p_hid->report_desc, p_hid->report_len);

      usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _usbd_ctrl_buf, p_hid->report_len);
    }else
    {
      dcd_control_stall(rhport);
    }
  }
  //------------- Class Specific Request -------------//
  else if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS)
  {
    if( HID_REQ_CONTROL_GET_REPORT == p_request->bRequest )
    {
      // wValue = Report Type | Report ID
      uint8_t const report_type = u16_high_u8(p_request->wValue);
      uint8_t const report_id   = u16_low_u8(p_request->wValue);

      // Composite interface need to determine it is Keyboard, Mouse or Gamepad
      if ( report_id > 0 )
      {

      }

      uint16_t xferlen;
      if ( p_hid->get_report_cb )
      {
        xferlen = p_hid->get_report_cb((hid_report_type_t) report_type, p_hid->report_buf, p_request->wLength);
      }else
      {
        xferlen = p_request->wLength;
        // re-use report_buf -> report has no change
      }

      STASK_ASSERT( xferlen > 0 );
      usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, p_hid->report_buf, xferlen);
    }
    else if ( HID_REQ_CONTROL_SET_REPORT == p_request->bRequest )
    {
      // wValue = Report Type | Report ID
      usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _usbd_ctrl_buf, p_request->wLength);

      if ( p_hid->set_report_cb )
      {
        p_hid->set_report_cb(u16_high_u8(p_request->wValue), _usbd_ctrl_buf, p_request->wLength);
      }
    }
    else if (HID_REQ_CONTROL_SET_IDLE == p_request->bRequest)
    {
      p_hid->idle_rate = u16_high_u8(p_request->wValue);
      dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
    }
    else if (HID_REQ_CONTROL_GET_IDLE == p_request->bRequest)
    {
      _usbd_ctrl_buf[0] = p_hid->idle_rate;
      usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _usbd_ctrl_buf, 1);
    }
    else if (HID_REQ_CONTROL_GET_PROTOCOL == p_request->bRequest )
    {
      _usbd_ctrl_buf[0] = 1 - CFG_TUD_HID_BOOT_PROTOCOL; // 0 is Boot, 1 is Report protocol
      usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _usbd_ctrl_buf, 1);
    }
    else
    {
//      HID_REQ_CONTROL_SET_PROTOCOL:
      dcd_control_stall(rhport);
    }
  }else
  {
    dcd_control_stall(rhport);
  }

  OSAL_SUBTASK_END
}

tusb_error_t hidd_xfer_cb(uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  // nothing to do
  return TUSB_ERROR_NONE;
}

#endif
