/**************************************************************************/
/*!
    @file     hid_device.h
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
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#ifndef _TUSB_HID_DEVICE_H_
#define _TUSB_HID_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "hid.h"

#ifdef __cplusplus
 extern "C" {
#endif


//--------------------------------------------------------------------+
// KEYBOARD APPLICATION API
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Keyboard Keyboard
 *  @{ */
/** \defgroup Keyboard_Device Device
 *  @{ */

/** Check if the interface is ready to use
 * \returns true if ready, otherwise interface may not be mounted or still busy transferring data
 * \note    Application must not perform any action if the interface is not ready
 */
bool tud_hid_keyboard_ready(void);

bool tud_hid_keyboard_keycode(uint8_t modifier, uint8_t keycode[6]);

static inline bool tud_hid_keyboard_key_release(void) { return tud_hid_keyboard_keycode(0, NULL); }

#if CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP
bool tud_hid_keyboard_key_press(char ch);
bool tud_hid_keyboard_key_sequence(const char* str, uint32_t interval_ms);

typedef struct{
  uint8_t shift;
  uint8_t keycode;
}hid_ascii_to_keycode_entry_t;
extern const hid_ascii_to_keycode_entry_t HID_ASCII_TO_KEYCODE[128];
#endif

/*------------- Callbacks -------------*/

/** \brief      Callback function that is invoked when USB host request \ref HID_REQUEST_CONTROL_GET_REPORT
 *              via control endpoint.
 * \param[in]   report_type specify which report (INPUT, OUTPUT, FEATURE) that host requests
 * \param[out]  buffer data that application need to update, value must be accessible by USB controller (see \ref CFG_TUSB_ATTR_USBRAM)
 * \param[in]   reqlen  number of bytes that host requested
 * \retval      non-zero Actual number of bytes in the response's buffer.
 * \retval      zero  indicates the current request is not supported. Tinyusb device stack will reject the request by
 *              sending STALL in the data phase.
 * \note        After this callback, the request is silently executed by the tinyusb stack, thus
 *              the completion of this control request will not be reported to application.
 *              For Keyboard, USB host often uses this to turn on/off the LED for CAPLOCKS, NUMLOCK (\ref hid_keyboard_led_bm_t)
 */
ATTR_WEAK uint16_t tud_hid_keyboard_get_report_cb(hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);

/** \brief      Callback function that is invoked when USB host request \ref HID_REQUEST_CONTROL_SET_REPORT
 *              via control endpoint.
 * \param[in]   report_type specify which report (INPUT, OUTPUT, FEATURE) that host requests
 * \param[in]   buffer  containing the report's data
 * \param[in]   bufsize  number of bytes in the \a buffer
 * \note        By the time this callback is invoked, the USB control transfer is already completed in the hardware side.
 *              Application are free to handle data at its own will.
 */
ATTR_WEAK void tud_hid_keyboard_set_report_cb(hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

/** @} */
/** @} */

//--------------------------------------------------------------------+
// MOUSE APPLICATION API
//--------------------------------------------------------------------+
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

/*------------- Callbacks -------------*/

/** \brief      Callback function that is invoked when USB host request \ref HID_REQUEST_CONTROL_GET_REPORT
 *              via control endpoint.
 * \param[in]   report_type specify which report (INPUT, OUTPUT, FEATURE) that host requests
 * \param[out]  buffer buffer that application need to update, value must be accessible by USB controller (see \ref CFG_TUSB_ATTR_USBRAM)
 * \param[in]   reqlen  number of bytes that host requested
 * \retval      non-zero Actual number of bytes in the response's buffer.
 * \retval      zero  indicates the current request is not supported. Tinyusb device stack will reject the request by
 *              sending STALL in the data phase.
 * \note        After this callback, the request is silently executed by the tinyusb stack, thus
 *              the completion of this control request will not be reported to application
 */
ATTR_WEAK uint16_t tud_hid_mouse_get_report_cb(hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);

/** \brief      Callback function that is invoked when USB host request \ref HID_REQUEST_CONTROL_SET_REPORT
 *              via control endpoint.
 * \param[in]   report_type specify which report (INPUT, OUTPUT, FEATURE) that host requests
 * \param[in]   buffer buffer containing the report's data
 * \param[in]   bufsize  number of bytes in the \a p_report_data
 * \note        By the time this callback is invoked, the USB control transfer is already completed in the hardware side.
 *              Application are free to handle data at its own will.
 */
ATTR_WEAK void tud_hid_mouse_set_report_cb(hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

/** @} */
/** @} */



//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void hidd_init(void);
tusb_error_t hidd_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length);
tusb_error_t hidd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request);
tusb_error_t hidd_xfer_cb(uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes);
void hidd_reset(uint8_t rhport);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HID_DEVICE_H_ */


