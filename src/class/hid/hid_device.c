/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_HID)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/tusb_common.h"
#include "hid_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;        // optional Out endpoint
  uint8_t boot_protocol; // Boot mouse or keyboard
  bool    boot_mode;     // default = false (Report)
  uint8_t idle_rate;     // up to application to handle idle rate
  uint16_t reprot_desc_len;

  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_HID_BUFSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_HID_BUFSIZE];

}hidd_interface_t;

CFG_TUSB_MEM_SECTION static hidd_interface_t _hidd_itf[CFG_TUD_HID];

/*------------- Helpers -------------*/
static inline hidd_interface_t* get_interface_by_itfnum(uint8_t itf_num)
{
  for (uint8_t i=0; i < CFG_TUD_HID; i++ )
  {
    if ( itf_num == _hidd_itf[i].itf_num ) return &_hidd_itf[i];
  }

  return NULL;
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_hid_ready(void)
{
  uint8_t itf = 0;
  uint8_t const ep_in = _hidd_itf[itf].ep_in;
  return tud_ready() && (ep_in != 0) && !usbd_edpt_busy(TUD_OPT_RHPORT, ep_in);
}

bool tud_hid_report(uint8_t report_id, void const* report, uint8_t len)
{
  TU_VERIFY( tud_hid_ready() );

  uint8_t itf = 0;
  hidd_interface_t * p_hid = &_hidd_itf[itf];

  if (report_id)
  {
    len = tu_min8(len, CFG_TUD_HID_BUFSIZE-1);

    p_hid->epin_buf[0] = report_id;
    memcpy(p_hid->epin_buf+1, report, len);
    len++;
  }else
  {
    // If report id = 0, skip ID field
    len = tu_min8(len, CFG_TUD_HID_BUFSIZE);
    memcpy(p_hid->epin_buf, report, len);
  }

  return usbd_edpt_xfer(TUD_OPT_RHPORT, p_hid->ep_in, p_hid->epin_buf, len);
}

bool tud_hid_boot_mode(void)
{
  uint8_t itf = 0;
  return _hidd_itf[itf].boot_mode;
}

//--------------------------------------------------------------------+
// KEYBOARD API
//--------------------------------------------------------------------+
bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, uint8_t keycode[6])
{
  hid_keyboard_report_t report;

  report.modifier = modifier;

  if ( keycode )
  {
    memcpy(report.keycode, keycode, 6);
  }else
  {
    tu_memclr(report.keycode, 6);
  }

  return tud_hid_report(report_id, &report, sizeof(report));
}

//--------------------------------------------------------------------+
// MOUSE APPLICATION API
//--------------------------------------------------------------------+
bool tud_hid_mouse_report(uint8_t report_id, uint8_t buttons, int8_t x, int8_t y, int8_t vertical, int8_t horizontal)
{
  hid_mouse_report_t report =
  {
    .buttons = buttons,
    .x       = x,
    .y       = y,
    .wheel   = vertical,
    .pan     = horizontal
  };

  return tud_hid_report(report_id, &report, sizeof(report));
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
  (void) rhport;
  tu_memclr(_hidd_itf, sizeof(_hidd_itf));
}

bool hidd_open(uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t *p_len)
{
  uint8_t const *p_desc = (uint8_t const *) desc_itf;

  // TODO support multiple HID interface
  uint8_t const itf = 0;
  hidd_interface_t * p_hid = &_hidd_itf[itf];

  //------------- HID descriptor -------------//
  p_desc = tu_desc_next(p_desc);
  tusb_hid_descriptor_hid_t const *desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  TU_ASSERT(HID_DESC_TYPE_HID == desc_hid->bDescriptorType);

  //------------- Endpoint Descriptor -------------//
  p_desc = tu_desc_next(p_desc);
  TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, desc_itf->bNumEndpoints, TUSB_XFER_INTERRUPT, &p_hid->ep_out, &p_hid->ep_in));

  if ( desc_itf->bInterfaceSubClass == HID_SUBCLASS_BOOT ) p_hid->boot_protocol = desc_itf->bInterfaceProtocol;

  p_hid->boot_mode = false; // default mode is REPORT
  p_hid->itf_num   = desc_itf->bInterfaceNumber;
  p_hid->reprot_desc_len  = desc_hid->wReportLength;

  *p_len = sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + desc_itf->bNumEndpoints*sizeof(tusb_desc_endpoint_t);

  // Prepare for output endpoint
  if (p_hid->ep_out) TU_ASSERT(usbd_edpt_xfer(rhport, p_hid->ep_out, p_hid->epout_buf, sizeof(p_hid->epout_buf)));

  return true;
}

// Handle class control request
// return false to stall control endpoint (e.g unsupported request)
bool hidd_control_request(uint8_t rhport, tusb_control_request_t const * p_request)
{
  hidd_interface_t* p_hid = get_interface_by_itfnum( (uint8_t) p_request->wIndex );
  TU_ASSERT(p_hid);

  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD)
  {
    //------------- STD Request -------------//
    uint8_t const desc_type  = tu_u16_high(p_request->wValue);
    uint8_t const desc_index = tu_u16_low (p_request->wValue);
    (void) desc_index;

    if (p_request->bRequest == TUSB_REQ_GET_DESCRIPTOR && desc_type == HID_DESC_TYPE_REPORT)
    {
      uint8_t const * desc_report = tud_hid_descriptor_report_cb();
      tud_control_xfer(rhport, p_request, (void*) desc_report, p_hid->reprot_desc_len);
    }else
    {
      return false; // stall unsupported request
    }
  }
  else if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS)
  {
    //------------- Class Specific Request -------------//
    switch( p_request->bRequest )
    {
      case HID_REQ_CONTROL_GET_REPORT:
      {
        // wValue = Report Type | Report ID
        uint8_t const report_type = tu_u16_high(p_request->wValue);
        uint8_t const report_id   = tu_u16_low(p_request->wValue);

        uint16_t xferlen  = tud_hid_get_report_cb(report_id, (hid_report_type_t) report_type, p_hid->epin_buf, p_request->wLength);
        TU_ASSERT( xferlen > 0 );

        tud_control_xfer(rhport, p_request, p_hid->epin_buf, xferlen);
      }
      break;

      case  HID_REQ_CONTROL_SET_REPORT:
        tud_control_xfer(rhport, p_request, p_hid->epout_buf, p_request->wLength);
      break;

      case HID_REQ_CONTROL_SET_IDLE:
        p_hid->idle_rate = tu_u16_high(p_request->wValue);
        if ( tud_hid_set_idle_cb )
        {
          // stall request if callback return false
          if ( !tud_hid_set_idle_cb(p_hid->idle_rate) ) return false;
        }

        tud_control_status(rhport, p_request);
      break;

      case HID_REQ_CONTROL_GET_IDLE:
        // TODO idle rate of report
        tud_control_xfer(rhport, p_request, &p_hid->idle_rate, 1);
      break;

      case HID_REQ_CONTROL_GET_PROTOCOL:
      {
        uint8_t protocol = 1-p_hid->boot_mode;   // 0 is Boot, 1 is Report protocol
        tud_control_xfer(rhport, p_request, &protocol, 1);
      }
      break;

      case HID_REQ_CONTROL_SET_PROTOCOL:
        p_hid->boot_mode = 1 - p_request->wValue; // 0 is Boot, 1 is Report protocol

        if (tud_hid_boot_mode_cb) tud_hid_boot_mode_cb(p_hid->boot_mode);

        tud_control_status(rhport, p_request);
      break;

      default: return false; // stall unsupported request
    }
  }else
  {
    return false; // stall unsupported request
  }

  return true;
}

// Invoked when class request DATA stage is finished.
// return false to stall control endpoint (e.g Host send non-sense DATA)
bool hidd_control_complete(uint8_t rhport, tusb_control_request_t const * p_request)
{
  (void) rhport;
  hidd_interface_t* p_hid = get_interface_by_itfnum( (uint8_t) p_request->wIndex );
  TU_ASSERT(p_hid);

  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
      p_request->bRequest == HID_REQ_CONTROL_SET_REPORT)
  {
    // wValue = Report Type | Report ID
    uint8_t const report_type = tu_u16_high(p_request->wValue);
    uint8_t const report_id   = tu_u16_low(p_request->wValue);

    tud_hid_set_report_cb(report_id, (hid_report_type_t) report_type, p_hid->epout_buf, p_request->wLength);
  }

  return true;
}

bool hidd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;

  // TODO support multiple HID interface
  uint8_t const itf = 0;
  hidd_interface_t * p_hid = &_hidd_itf[itf];

  if (ep_addr == p_hid->ep_out)
  {
    tud_hid_set_report_cb(0, HID_REPORT_TYPE_INVALID, p_hid->epout_buf, xferred_bytes);
    TU_ASSERT(usbd_edpt_xfer(rhport, p_hid->ep_out, p_hid->epout_buf, sizeof(p_hid->epout_buf)));
  }

  return true;
}

#endif
