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

#if !CFG_TUD_HID_KEYBOARD && CFG_TUD_HID_KEYBOARD_BOOT
#error CFG_TUD_HID_KEYBOARD must be enabled
#endif

#if !CFG_TUD_HID_MOUSE && CFG_TUD_HID_MOUSE_BOOT
#error CFG_TUD_HID_MOUSE must be enabled
#endif


//--------------------------------------------------------------------+
// HID GENERIC API
//--------------------------------------------------------------------+
bool tud_hid_generic_ready(void);
bool tud_hid_generic_report(uint8_t report_id, void const* report, uint8_t len);

/*------------- Callbacks (Weak is optional) -------------*/
uint16_t tud_hid_generic_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);
void     tud_hid_generic_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

//--------------------------------------------------------------------+
// KEYBOARD API
//--------------------------------------------------------------------+
#if CFG_TUD_HID_KEYBOARD
/** \addtogroup ClassDriver_HID_Keyboard Keyboard
 *  @{ */
/** \defgroup Keyboard_Device Device
 *  @{ */

/** Check if the interface is ready to use
 * \returns true if ready, otherwise interface may not be mounted or still busy transferring data
 * \note    Application must not perform any action if the interface is not ready
 */
bool tud_hid_keyboard_ready(void);
bool tud_hid_keyboard_is_boot_protocol(void);

bool tud_hid_keyboard_keycode(uint8_t modifier, uint8_t keycode[6]);

static inline bool tud_hid_keyboard_key_release(void) { return tud_hid_keyboard_keycode(0, NULL); }

#if CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP
bool tud_hid_keyboard_key_press(char ch);

typedef struct{
  uint8_t shift;
  uint8_t keycode;
}hid_ascii_to_keycode_entry_t;
extern const hid_ascii_to_keycode_entry_t HID_ASCII_TO_KEYCODE[128];
#endif

#endif

/*------------- Callbacks (Weak is optional) -------------*/

/** Callback invoked when USB host request \ref HID_REQ_CONTROL_GET_REPORT.
 * \param[in]   report_type specify which report (INPUT, OUTPUT, FEATURE) that host requests
 * \param[out]  buffer data that application need to update, value must be accessible by USB controller (see \ref CFG_TUSB_MEM_SECTION)
 * \param[in]   reqlen  number of bytes that host requested
 * \retval      non-zero Actual number of bytes in the response's buffer.
 * \retval      zero  indicates the current request is not supported. Tinyusb device stack will reject the request by
 *              sending STALL in the data phase.
 * \note        After this callback, the request is silently executed by the tinyusb stack, thus
 *              the completion of this control request will not be reported to application.
 *              For Keyboard, USB host often uses this to turn on/off the LED for CAPLOCKS, NUMLOCK (\ref hid_keyboard_led_bm_t)
 */
ATTR_WEAK uint16_t tud_hid_keyboard_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);

/** Callback invoked when USB host request \ref HID_REQ_CONTROL_SET_REPORT.
 * \param[in]   report_type specify which report (INPUT, OUTPUT, FEATURE) that host requests
 * \param[in]   buffer  containing the report's data
 * \param[in]   bufsize  number of bytes in the \a buffer
 * \note        By the time this callback is invoked, the USB control transfer is already completed in the hardware side.
 *              Application are free to handle data at its own will.
 */
ATTR_WEAK void tud_hid_keyboard_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);


//ATTR_WEAK void tud_hid_keyboard_set_protocol_cb(bool boot_protocol);

/** @} */
/** @} */

//--------------------------------------------------------------------+
// MOUSE API
//--------------------------------------------------------------------+
#if CFG_TUD_HID_MOUSE
/** \addtogroup ClassDriver_HID_Mouse Mouse
 *  @{ */
/** \defgroup Mouse_Device Device
 *  @{ */

/** \brief      Check if the interface is currently busy or not
 * \retval      true if the interface is busy meaning the stack is still transferring/waiting data from/to host
 * \retval      false if the interface is not busy meaning the stack successfully transferred data from/to host
 * \note        This function is primarily used for polling/waiting result after \ref tusbd_hid_mouse_send.
 */
bool tud_hid_mouse_ready(void);
bool tud_hid_mouse_is_boot_protocol(void);

bool tud_hid_mouse_data(uint8_t buttons, int8_t x, int8_t y, int8_t scroll, int8_t pan);

bool tud_hid_mouse_move(int8_t x, int8_t y);
bool tud_hid_mouse_scroll(int8_t vertical, int8_t horizontal);

static inline bool tud_hid_mouse_button_press(uint8_t buttons)
{
  return tud_hid_mouse_data(buttons, 0, 0, 0, 0);
}

static inline bool tud_hid_mouse_button_release(void)
{
  return tud_hid_mouse_data(0, 0, 0, 0, 0);
}

/*------------- Callbacks (Weak is optional) -------------*/

/**
 * Callback function that is invoked when USB host request \ref HID_REQ_CONTROL_GET_REPORT.
 * \param[in]   report_type specify which report (INPUT, OUTPUT, FEATURE) that host requests
 * \param[out]  buffer  buffer that application need to update, value must be accessible by USB controller (see \ref CFG_TUSB_MEM_SECTION)
 * \param[in]   reqlen  number of bytes that host requested
 * \retval      non-zero Actual number of bytes in the response's buffer.
 * \retval      zero  indicates the current request is not supported. Tinyusb device stack will reject the request by
 *              sending STALL in the data phase.
 * \note        After this callback, the request is silently executed by the tinyusb stack, thus
 *              the completion of this control request will not be reported to application
 */
ATTR_WEAK uint16_t tud_hid_mouse_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);

/**
 * Callback function that is invoked when USB host request \ref HID_REQ_CONTROL_SET_REPORT.
 * \param[in]   report_type specify which report (INPUT, OUTPUT, FEATURE) that host requests
 * \param[in]   buffer buffer containing the report's data
 * \param[in]   bufsize  number of bytes in the \a p_report_data
 * \note        By the time this callback is invoked, the USB control transfer is already completed in the hardware side.
 *              Application are free to handle data at its own will.
 */
ATTR_WEAK void tud_hid_mouse_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

//ATTR_WEAK void tud_hid_mouse_set_protocol_cb(bool boot_protocol);

#endif


//--------------------------------------------------------------------+
// HID Report Descriptor Template
//--------------------------------------------------------------------+
/* These template should be used as follow
 * - Only 1 report : no parameter
 *      uint8_t report_desc[] = { ID_REPORT_DESC_KEYBOARD() };
 *
 * - Multiple Reports: "HID_REPORT_ID(ID)," must be passed to template
 *      uint8_t report_desc[] = {
 *          ID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(1) ,) ,
 *          HID_REPORT_DESC_MOUSE  ( HID_REPORT_ID(2) ,)
 *      };
 */

/*------------- Keyboard Descriptor Template -------------*/
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

/*------------- Mouse Descriptor Template -------------*/
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

//------------- Consumer Control Report Template -------------//
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

//------------- System Control Report Template -------------//
/* 0x00 - do nothing
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

//------------- Gamepad Report Template -------------//
// Gamepad with 16 buttons and 2 joysticks
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
// INTERNAL API
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

