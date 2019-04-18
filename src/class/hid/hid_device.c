/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

#ifndef CFG_TUD_HID_BUFSIZE
#define CFG_TUD_HID_BUFSIZE     16
#endif

typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t boot_protocol; // Boot mouse or keyboard
  bool    boot_mode;

  uint16_t reprot_desc_len;
  uint8_t idle_rate;     // Idle Rate = 0 : only send report if there is changes, i.e skip duplication
                         // Idle Rate > 0 : skip duplication, but send at least 1 report every idle rate (in unit of 4 ms).
  uint8_t mouse_button;  // caching button for using with tud_hid_mouse_ API

  CFG_TUSB_MEM_ALIGN uint8_t report_buf[CFG_TUD_HID_BUFSIZE];
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
  return tud_ready() && (ep_in != 0) && !dcd_edpt_busy(TUD_OPT_RHPORT, ep_in);
}

bool tud_hid_report(uint8_t report_id, void const* report, uint8_t len)
{
  TU_VERIFY( tud_hid_ready() && (len < CFG_TUD_HID_BUFSIZE) );

  uint8_t itf = 0;
  hidd_interface_t * p_hid = &_hidd_itf[itf];

  // If report id = 0, skip ID field
  if (report_id)
  {
    p_hid->report_buf[0] = report_id;
    memcpy(p_hid->report_buf+1, report, len);
  }else
  {
    memcpy(p_hid->report_buf, report, len);
  }

  // TODO skip duplication ? and or idle rate
  return dcd_edpt_xfer(TUD_OPT_RHPORT, p_hid->ep_in, p_hid->report_buf, len + (report_id ? 1 : 0) );
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

  // TODO skip duplication ? and or idle rate
  return tud_hid_report(report_id, &report, sizeof(report));
}

#if CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP
bool tud_hid_keyboard_key_press(uint8_t report_id, char ch)
{
  uint8_t keycode[6] = { 0 };
  uint8_t modifier   = 0;

  if ( HID_ASCII_TO_KEYCODE[(uint8_t)ch].shift ) modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
  keycode[0] = HID_ASCII_TO_KEYCODE[(uint8_t)ch].keycode;

  return tud_hid_keyboard_report(report_id, modifier, keycode);
}
#endif // CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP

//--------------------------------------------------------------------+
// MOUSE APPLICATION API
//--------------------------------------------------------------------+
bool tud_hid_mouse_report(uint8_t report_id, uint8_t buttons, int8_t x, int8_t y, int8_t scroll, int8_t pan)
{
  (void) pan;
  hid_mouse_report_t report =
  {
    .buttons = buttons,
    .x       = x,
    .y       = y,
    .wheel   = scroll,
    //.pan     = pan
  };

  uint8_t itf = 0;
  _hidd_itf[itf].mouse_button = buttons;

  return tud_hid_report(report_id, &report, sizeof(report));
}

bool tud_hid_mouse_move(uint8_t report_id, int8_t x, int8_t y)
{
  uint8_t itf = 0;
  uint8_t const button = _hidd_itf[itf].mouse_button;

  return tud_hid_mouse_report(report_id, button, x, y, 0, 0);
}

bool tud_hid_mouse_scroll(uint8_t report_id, int8_t scroll, int8_t pan)
{
  uint8_t itf = 0;
  uint8_t const button = _hidd_itf[itf].mouse_button;

  return tud_hid_mouse_report(report_id, button, 0, 0, scroll, pan);
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

  //------------- HID descriptor -------------//
  p_desc = tu_desc_next(p_desc);
  tusb_hid_descriptor_hid_t const *desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  TU_ASSERT(HID_DESC_TYPE_HID == desc_hid->bDescriptorType);

  //------------- Endpoint Descriptor -------------//
  p_desc = tu_desc_next(p_desc);
  tusb_desc_endpoint_t const *desc_edpt = (tusb_desc_endpoint_t const *) p_desc;
  TU_ASSERT(TUSB_DESC_ENDPOINT == desc_edpt->bDescriptorType);

  TU_ASSERT(dcd_edpt_open(rhport, desc_edpt));

  // TODO support multiple HID interface
  uint8_t itf = 0;
  hidd_interface_t * p_hid = &_hidd_itf[itf];

  if ( desc_itf->bInterfaceSubClass == HID_SUBCLASS_BOOT ) p_hid->boot_protocol = desc_itf->bInterfaceProtocol;

  p_hid->boot_mode = false; // default mode is REPORT
  p_hid->itf_num   = desc_itf->bInterfaceNumber;
  p_hid->ep_in     = desc_edpt->bEndpointAddress;
  p_hid->reprot_desc_len  = desc_hid->wReportLength;

  *p_len = sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + desc_itf->bNumEndpoints*sizeof(tusb_desc_endpoint_t);

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
      usbd_control_xfer(rhport, p_request, (void*) tud_desc_set.hid_report, p_hid->reprot_desc_len);
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

        uint16_t xferlen  = tud_hid_get_report_cb(report_id, (hid_report_type_t) report_type, p_hid->report_buf, p_request->wLength);
        TU_ASSERT( xferlen > 0 );

        usbd_control_xfer(rhport, p_request, p_hid->report_buf, xferlen);
      }
      break;

      case  HID_REQ_CONTROL_SET_REPORT:
        usbd_control_xfer(rhport, p_request, p_hid->report_buf, p_request->wLength);
      break;

      case HID_REQ_CONTROL_SET_IDLE:
        // TODO idle rate of report
        p_hid->idle_rate = tu_u16_high(p_request->wValue);
        usbd_control_status(rhport, p_request);
      break;

      case HID_REQ_CONTROL_GET_IDLE:
        // TODO idle rate of report
        usbd_control_xfer(rhport, p_request, &p_hid->idle_rate, 1);
      break;

      case HID_REQ_CONTROL_GET_PROTOCOL:
      {
        uint8_t protocol = 1-p_hid->boot_mode;   // 0 is Boot, 1 is Report protocol
        usbd_control_xfer(rhport, p_request, &protocol, 1);
      }
      break;

      case HID_REQ_CONTROL_SET_PROTOCOL:
        p_hid->boot_mode = 1 - p_request->wValue; // 0 is Boot, 1 is Report protocol

        if (tud_hid_mode_changed_cb) tud_hid_mode_changed_cb(p_hid->boot_mode);

        usbd_control_status(rhport, p_request);
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
bool hidd_control_request_complete(uint8_t rhport, tusb_control_request_t const * p_request)
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

    tud_hid_set_report_cb(report_id, (hid_report_type_t) report_type, p_hid->report_buf, p_request->wLength);
  }

  return true;
}

bool hidd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  // nothing to do
  (void) rhport;
  (void) ep_addr;
  (void) event;
  (void) xferred_bytes;

  return true;
}


//------------- Ascii to Keycode Lookup -------------//
#if CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP
const hid_ascii_to_keycode_entry_t HID_ASCII_TO_KEYCODE[128] =
{
    {0, 0                     }, // 0x00 Null
    {0, 0                     }, // 0x01
    {0, 0                     }, // 0x02
    {0, 0                     }, // 0x03
    {0, 0                     }, // 0x04
    {0, 0                     }, // 0x05
    {0, 0                     }, // 0x06
    {0, 0                     }, // 0x07
    {0, HID_KEY_BACKSPACE     }, // 0x08 Backspace
    {0, HID_KEY_TAB           }, // 0x09 Horizontal Tab
    {0, HID_KEY_RETURN        }, // 0x0A Line Feed
    {0, 0                     }, // 0x0B
    {0, 0                     }, // 0x0C
    {0, HID_KEY_RETURN        }, // 0x0D Carriage return
    {0, 0                     }, // 0x0E
    {0, 0                     }, // 0x0F
    {0, 0                     }, // 0x10
    {0, 0                     }, // 0x11
    {0, 0                     }, // 0x12
    {0, 0                     }, // 0x13
    {0, 0                     }, // 0x14
    {0, 0                     }, // 0x15
    {0, 0                     }, // 0x16
    {0, 0                     }, // 0x17
    {0, 0                     }, // 0x18
    {0, 0                     }, // 0x19
    {0, 0                     }, // 0x1A
    {0, HID_KEY_ESCAPE        }, // 0x1B Escape
    {0, 0                     }, // 0x1C
    {0, 0                     }, // 0x1D
    {0, 0                     }, // 0x1E
    {0, 0                     }, // 0x1F

    {0, HID_KEY_SPACE         }, // 0x20
    {1, HID_KEY_1             }, // 0x21 !
    {1, HID_KEY_APOSTROPHE    }, // 0x22 "
    {1, HID_KEY_3             }, // 0x23 #
    {1, HID_KEY_4             }, // 0x24 $
    {1, HID_KEY_5             }, // 0x25 %
    {1, HID_KEY_7             }, // 0x26 &
    {0, HID_KEY_APOSTROPHE    }, // 0x27 '
    {1, HID_KEY_9             }, // 0x28 (
    {1, HID_KEY_0             }, // 0x29 )
    {1, HID_KEY_8             }, // 0x2A *
    {1, HID_KEY_EQUAL         }, // 0x2B +
    {0, HID_KEY_COMMA         }, // 0x2C ,
    {0, HID_KEY_MINUS         }, // 0x2D -
    {0, HID_KEY_PERIOD        }, // 0x2E .
    {0, HID_KEY_SLASH         }, // 0x2F /
    {0, HID_KEY_0             }, // 0x30 0
    {0, HID_KEY_1             }, // 0x31 1
    {0, HID_KEY_2             }, // 0x32 2
    {0, HID_KEY_3             }, // 0x33 3
    {0, HID_KEY_4             }, // 0x34 4
    {0, HID_KEY_5             }, // 0x35 5
    {0, HID_KEY_6             }, // 0x36 6
    {0, HID_KEY_7             }, // 0x37 7
    {0, HID_KEY_8             }, // 0x38 8
    {0, HID_KEY_9             }, // 0x39 9
    {1, HID_KEY_SEMICOLON     }, // 0x3A :
    {0, HID_KEY_SEMICOLON     }, // 0x3B ;
    {1, HID_KEY_COMMA         }, // 0x3C <
    {0, HID_KEY_EQUAL         }, // 0x3D =
    {1, HID_KEY_PERIOD        }, // 0x3E >
    {1, HID_KEY_SLASH         }, // 0x3F ?

    {1, HID_KEY_2             }, // 0x40 @
    {1, HID_KEY_A             }, // 0x41 A
    {1, HID_KEY_B             }, // 0x42 B
    {1, HID_KEY_C             }, // 0x43 C
    {1, HID_KEY_D             }, // 0x44 D
    {1, HID_KEY_E             }, // 0x45 E
    {1, HID_KEY_F             }, // 0x46 F
    {1, HID_KEY_G             }, // 0x47 G
    {1, HID_KEY_H             }, // 0x48 H
    {1, HID_KEY_I             }, // 0x49 I
    {1, HID_KEY_J             }, // 0x4A J
    {1, HID_KEY_K             }, // 0x4B K
    {1, HID_KEY_L             }, // 0x4C L
    {1, HID_KEY_M             }, // 0x4D M
    {1, HID_KEY_N             }, // 0x4E N
    {1, HID_KEY_O             }, // 0x4F O
    {1, HID_KEY_P             }, // 0x50 P
    {1, HID_KEY_Q             }, // 0x51 Q
    {1, HID_KEY_R             }, // 0x52 R
    {1, HID_KEY_S             }, // 0x53 S
    {1, HID_KEY_T             }, // 0x55 T
    {1, HID_KEY_U             }, // 0x55 U
    {1, HID_KEY_V             }, // 0x56 V
    {1, HID_KEY_W             }, // 0x57 W
    {1, HID_KEY_X             }, // 0x58 X
    {1, HID_KEY_Y             }, // 0x59 Y
    {1, HID_KEY_Z             }, // 0x5A Z
    {0, HID_KEY_BRACKET_LEFT  }, // 0x5B [
    {0, HID_KEY_BACKSLASH     }, // 0x5C '\'
    {0, HID_KEY_BRACKET_RIGHT }, // 0x5D ]
    {1, HID_KEY_6             }, // 0x5E ^
    {1, HID_KEY_MINUS         }, // 0x5F _

    {0, HID_KEY_GRAVE         }, // 0x60 `
    {0, HID_KEY_A             }, // 0x61 a
    {0, HID_KEY_B             }, // 0x62 b
    {0, HID_KEY_C             }, // 0x63 c
    {0, HID_KEY_D             }, // 0x66 d
    {0, HID_KEY_E             }, // 0x65 e
    {0, HID_KEY_F             }, // 0x66 f
    {0, HID_KEY_G             }, // 0x67 g
    {0, HID_KEY_H             }, // 0x68 h
    {0, HID_KEY_I             }, // 0x69 i
    {0, HID_KEY_J             }, // 0x6A j
    {0, HID_KEY_K             }, // 0x6B k
    {0, HID_KEY_L             }, // 0x6C l
    {0, HID_KEY_M             }, // 0x6D m
    {0, HID_KEY_N             }, // 0x6E n
    {0, HID_KEY_O             }, // 0x6F o
    {0, HID_KEY_P             }, // 0x70 p
    {0, HID_KEY_Q             }, // 0x71 q
    {0, HID_KEY_R             }, // 0x72 r
    {0, HID_KEY_S             }, // 0x73 s
    {0, HID_KEY_T             }, // 0x75 t
    {0, HID_KEY_U             }, // 0x75 u
    {0, HID_KEY_V             }, // 0x76 v
    {0, HID_KEY_W             }, // 0x77 w
    {0, HID_KEY_X             }, // 0x78 x
    {0, HID_KEY_Y             }, // 0x79 y
    {0, HID_KEY_Z             }, // 0x7A z
    {1, HID_KEY_BRACKET_LEFT  }, // 0x7B {
    {1, HID_KEY_BACKSLASH     }, // 0x7C |
    {1, HID_KEY_BRACKET_RIGHT }, // 0x7D }
    {1, HID_KEY_GRAVE         }, // 0x7E ~
    {0, HID_KEY_DELETE        }  // 0x7F Delete
};
#endif // CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP

#endif
