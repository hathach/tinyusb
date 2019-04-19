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

#ifndef _TUSB_HID_DEVICE_H_
#define _TUSB_HID_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "hid.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Default Configure & Validation
//--------------------------------------------------------------------+
#ifndef CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP
#define CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP 0
#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Check if the interface is ready to use
bool tud_hid_ready(void);

// Check if current mode is Boot (true) or Report (false)
bool tud_hid_boot_mode(void);

// Send report to host
bool tud_hid_report(uint8_t report_id, void const* report, uint8_t len);

/*------------- Callbacks (Weak is optional) -------------*/

// Invoked when receiving GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);

// Invoked when receiving SET_REPORT control request
void tud_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

// Invoked when host switch mode Boot <-> Report via SET_PROTOCOL request
void tud_hid_mode_changed_cb(uint8_t boot_mode) ATTR_WEAK;

//--------------------------------------------------------------------+
// KEYBOARD API
// Convenient helper to send keyboard report if application use standard/boot
// layout report as defined by hid_keyboard_report_t
//--------------------------------------------------------------------+

bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, uint8_t keycode[6]);

static inline bool tud_hid_keyboard_key_release(uint8_t report_id)
{
  return tud_hid_keyboard_report(report_id, 0, NULL);
}

#if CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP
bool tud_hid_keyboard_key_press(uint8_t report_id, char ch);

typedef struct{
  uint8_t shift;
  uint8_t keycode;
}hid_ascii_to_keycode_entry_t;
extern const hid_ascii_to_keycode_entry_t HID_ASCII_TO_KEYCODE[128];
#endif

//--------------------------------------------------------------------+
// MOUSE API
// Convenient helper to send mouse report if application use standard/boot
// layout report as defined by hid_mouse_report_t
//--------------------------------------------------------------------+

bool tud_hid_mouse_report(uint8_t report_id, uint8_t buttons, int8_t x, int8_t y, int8_t scroll, int8_t pan);
bool tud_hid_mouse_move(uint8_t report_id, int8_t x, int8_t y);
bool tud_hid_mouse_scroll(uint8_t report_id, int8_t scroll, int8_t pan);

static inline bool tud_hid_mouse_button_press(uint8_t report_id, uint8_t buttons)
{
  return tud_hid_mouse_report(report_id, buttons, 0, 0, 0, 0);
}

static inline bool tud_hid_mouse_button_release(uint8_t report_id)
{
  return tud_hid_mouse_report(report_id, 0, 0, 0, 0, 0);
}

/* --------------------------------------------------------------------+
 * HID Report Descriptor Template
 *
 * Convenient for declaring popular HID device (keyboard, mouse, consumer,
 * gamepad etc...). Templates take "HID_REPORT_ID(n)," as input, leave
 * empty if multiple reports is not used
 *
 * - Only 1 report: no parameter
 *      uint8_t const report_desc[] = { HID_REPORT_DESC_KEYBOARD() };
 *
 * - Multiple Reports: "HID_REPORT_ID(ID)," must be passed to template
 *      uint8_t const report_desc[] =
 *      {
 *          HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(1), ) ,
 *          HID_REPORT_DESC_MOUSE   ( HID_REPORT_ID(2), )
 *      };
 *--------------------------------------------------------------------*/

// Keyboard Report Descriptor Template
#define HID_REPORT_DESC_KEYBOARD(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                    ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD )                    ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )                    ,\
    /* 8 bits Modifier Keys (Shfit, Control, Alt) */ \
    __VA_ARGS__ \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD )                     ,\
      HID_USAGE_MIN    ( 224                                    )  ,\
      HID_USAGE_MAX    ( 231                                    )  ,\
      HID_LOGICAL_MIN  ( 0                                      )  ,\
      HID_LOGICAL_MAX  ( 1                                      )  ,\
      HID_REPORT_COUNT ( 8                                      )  ,\
      HID_REPORT_SIZE  ( 1                                      )  ,\
      HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )  ,\
      /* 8 bit reserved */ \
      HID_REPORT_COUNT ( 1                                      )  ,\
      HID_REPORT_SIZE  ( 8                                      )  ,\
      HID_INPUT        ( HID_CONSTANT                           )  ,\
    /* 6-byte Keycodes */ \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD )                     ,\
      HID_USAGE_MIN    ( 0                                   )     ,\
      HID_USAGE_MAX    ( 255                                 )     ,\
      HID_LOGICAL_MIN  ( 0                                   )     ,\
      HID_LOGICAL_MAX  ( 255                                 )     ,\
      HID_REPORT_COUNT ( 6                                   )     ,\
      HID_REPORT_SIZE  ( 8                                   )     ,\
      HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE )     ,\
    /* 5-bit LED Indicator Kana | Compose | ScrollLock | CapsLock | NumLock */ \
    HID_USAGE_PAGE  ( HID_USAGE_PAGE_LED                   )       ,\
      HID_USAGE_MIN    ( 1                                       ) ,\
      HID_USAGE_MAX    ( 5                                       ) ,\
      HID_REPORT_COUNT ( 5                                       ) ,\
      HID_REPORT_SIZE  ( 1                                       ) ,\
      HID_OUTPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE  ) ,\
      /* led padding */ \
      HID_REPORT_COUNT ( 1                                       ) ,\
      HID_REPORT_SIZE  ( 3                                       ) ,\
      HID_OUTPUT       ( HID_CONSTANT                            ) ,\
  HID_COLLECTION_END \

// Mouse Report Descriptor Template
#define HID_REPORT_DESC_MOUSE(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP      )                    ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_MOUSE     )                    ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION  )                    ,\
    __VA_ARGS__ \
    HID_USAGE      ( HID_USAGE_DESKTOP_POINTER )                    ,\
    HID_COLLECTION ( HID_COLLECTION_PHYSICAL   )                    ,\
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_BUTTON  )                    ,\
        HID_USAGE_MIN    ( 1                                      ) ,\
        HID_USAGE_MAX    ( 3                                      ) ,\
        HID_LOGICAL_MIN  ( 0                                      ) ,\
        HID_LOGICAL_MAX  ( 1                                      ) ,\
        /* Left, Right, Middle, Backward, Forward mouse buttons */   \
        HID_REPORT_COUNT ( 3                                      ) ,\
        HID_REPORT_SIZE  ( 1                                      ) ,\
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
        /* 3 bit padding */ \
        HID_REPORT_COUNT ( 1                                      ) ,\
        HID_REPORT_SIZE  ( 5                                      ) ,\
        HID_INPUT        ( HID_CONSTANT                           ) ,\
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP )                    ,\
        /* X, Y position [-127, 127] */ \
        HID_USAGE        ( HID_USAGE_DESKTOP_X                    ) ,\
        HID_USAGE        ( HID_USAGE_DESKTOP_Y                    ) ,\
        HID_LOGICAL_MIN  ( 0x81                                   ) ,\
        HID_LOGICAL_MAX  ( 0x7f                                   ) ,\
        HID_REPORT_COUNT ( 2                                      ) ,\
        HID_REPORT_SIZE  ( 8                                      ) ,\
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_RELATIVE ) ,\
        /* Mouse scroll [-127, 127] */ \
        HID_USAGE       ( HID_USAGE_DESKTOP_WHEEL                )  ,\
        HID_LOGICAL_MIN ( 0x81                                   )  ,\
        HID_LOGICAL_MAX ( 0x7f                                   )  ,\
        HID_REPORT_COUNT( 1                                      )  ,\
        HID_REPORT_SIZE ( 8                                      )  ,\
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_RELATIVE )  ,\
    HID_COLLECTION_END                                              ,\
  HID_COLLECTION_END \

// Consumer Control Report Descriptor Template
#define HID_REPORT_DESC_CONSUMER(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_CONSUMER    )              ,\
  HID_USAGE      ( HID_USAGE_CONSUMER_CONTROL )              ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )              ,\
    __VA_ARGS__ \
    HID_LOGICAL_MIN  ( 0x00                                ) ,\
    HID_LOGICAL_MAX_N( 0x03FF, 2                           ) ,\
    HID_USAGE_MIN    ( 0x00                                ) ,\
    HID_USAGE_MAX_N  ( 0x03FF, 2                           ) ,\
    HID_REPORT_COUNT ( 1                                   ) ,\
    HID_REPORT_SIZE  ( 16                                  ) ,\
    HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ) ,\
  HID_COLLECTION_END \

/* System Control Report Descriptor Template
 * 0x00 - do nothing
 * 0x01 - Power Off
 * 0x02 - Standby
 * 0x04 - Wake Host
 */
#define HID_REPORT_DESC_SYSTEM_CONTROL(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP           )        ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_SYSTEM_CONTROL )        ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION       )        ,\
    __VA_ARGS__ \
    /* 2 bit system power control */ \
    HID_LOGICAL_MIN  ( 1                                   ) ,\
    HID_LOGICAL_MAX  ( 3                                   ) ,\
    HID_REPORT_COUNT ( 1                                   ) ,\
    HID_REPORT_SIZE  ( 2                                   ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_SYSTEM_SLEEP      ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_SYSTEM_POWER_DOWN ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_SYSTEM_WAKE_UP    ) ,\
    HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ) ,\
    /* 6 bit padding */ \
    HID_REPORT_COUNT ( 1                                   ) ,\
    HID_REPORT_SIZE  ( 6                                   ) ,\
    HID_INPUT        ( HID_CONSTANT                        ) ,\
  HID_COLLECTION_END \

// Gamepad Report Descriptor Template
// with 16 buttons and 2 joysticks with following layout
// | Button Map (2 bytes) |  X | Y | Z | Rz
#define HID_REPORT_DESC_GAMEPAD(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )        ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD  )        ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )        ,\
    __VA_ARGS__ \
    /* 16 bit Button Map */ \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_BUTTON                  ) ,\
    HID_USAGE_MIN    ( 1                                      ) ,\
    HID_USAGE_MAX    ( 16                                     ) ,\
    HID_LOGICAL_MIN  ( 0                                      ) ,\
    HID_LOGICAL_MAX  ( 1                                      ) ,\
    HID_REPORT_COUNT ( 16                                     ) ,\
    HID_REPORT_SIZE  ( 1                                      ) ,\
    HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE    ) ,\
    /* X, Y, Z, Rz (min -127, max 127 ) */ \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_DESKTOP                 ) ,\
    HID_LOGICAL_MIN  ( 0x81                                   ) ,\
    HID_LOGICAL_MAX  ( 0x7f                                   ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_X                    ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_Y                    ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_Z                    ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_RZ                   ) ,\
    HID_REPORT_COUNT ( 4                                      ) ,\
    HID_REPORT_SIZE  ( 8                                      ) ,\
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
  HID_COLLECTION_END \

/** @} */
/** @} */

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void hidd_init(void);
bool hidd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t *p_length);
bool hidd_control_request(uint8_t rhport, tusb_control_request_t const * p_request);
bool hidd_control_request_complete (uint8_t rhport, tusb_control_request_t const * p_request);
bool hidd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void hidd_reset(uint8_t rhport);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HID_DEVICE_H_ */

