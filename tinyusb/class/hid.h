/*
 * hid.h
 *
 *  Created on: Nov 27, 2012
 *      Author: hathach (thachha@live.com)
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (thachha@live.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#ifndef _TUSB_HID_H_
#define _TUSB_HID_H_

 // TODO refractor
#include "common/common.h"
#include "device/dcd.h"


/** \brief Standard HID Boot Protocol Mouse Report.
 *
 *  Type define for a standard Boot Protocol Mouse report
 */
typedef PRE_PACK struct
{
  uint8_t Button; /**< Button mask for currently pressed buttons in the mouse. */
  int8_t  X; /**< Current delta X movement of the mouse. */
  int8_t  Y; /**< Current delta Y movement on the mouse. */
} POST_PACK USB_HID_MouseReport_t;

/** \brief Standard HID Boot Protocol Keyboard Report.
 *
 *  Type define for a standard Boot Protocol Keyboard report
 */
typedef PRE_PACK struct
{
  uint8_t Modifier; /**< Keyboard modifier byte, indicating pressed modifier keys (a combination of HID_KEYBOARD_MODIFER_* masks). */
  uint8_t Reserved; /**< Reserved for OEM use, always set to 0. */
  uint8_t KeyCode[6]; /**< Key codes of the currently pressed keys. */
} POST_PACK USB_HID_KeyboardReport_t;

/* Button codes for HID mouse */
enum USB_HID_MOUSE_BUTTON_CODE
{
	HID_MOUSEBUTTON_RIGHT = 0,
	HID_MOUSEBUTTON_LEFT = 1,
	HID_MOUSEBUTTON_MIDDLE = 2
};

/* KB modifier codes for HID KB */
enum USB_HID_KB_KEYMODIFIER_CODE
{
	HID_KEYMODIFIER_LEFTCTRL = 0,
	HID_KEYMODIFIER_LEFTSHIFT,
	HID_KEYMODIFIER_LEFTALT,
	HID_KEYMODIFIER_LEFTGUI,
	HID_KEYMODIFIER_RIGHTCTRL,
	HID_KEYMODIFIER_RIGHTSHIFT,
	HID_KEYMODIFIER_RIGHTALT,
	HID_KEYMODIFIER_RIGHTGUI
};

enum USB_HID_LOCAL_CODE
{
  HID_Local_NotSupported = 0,
  HID_Local_Arabic,
  HID_Local_Belgian,
  HID_Local_Canadian_Bilingual,
  HID_Local_Canadian_French,
  HID_Local_Czech_Republic,
  HID_Local_Danish,
  HID_Local_Finnish,
  HID_Local_French,
  HID_Local_German,
  HID_Local_Greek,
  HID_Local_Hebrew,
  HID_Local_Hungary,
  HID_Local_International,
  HID_Local_Italian,
  HID_Local_Japan_Katakana,
  HID_Local_Korean,
  HID_Local_Latin_American,
  HID_Local_Netherlands_Dutch,
  HID_Local_Norwegian,
  HID_Local_Persian_Farsi,
  HID_Local_Poland,
  HID_Local_Portuguese,
  HID_Local_Russia,
  HID_Local_Slovakia,
  HID_Local_Spanish,
  HID_Local_Swedish,
  HID_Local_Swiss_French,
  HID_Local_Swiss_German,
  HID_Local_Switzerland,
  HID_Local_Taiwan,
  HID_Local_Turkish_Q,
  HID_Local_UK,
  HID_Local_US,
  HID_Local_Yugoslavia,
  HID_Local_Turkish_F
};

#ifdef __cplusplus
 extern "C" {
#endif

TUSB_Error_t usb_hid_init(USBD_HANDLE_T hUsb, USB_INTERFACE_DESCRIPTOR const *const pIntfDesc, uint8_t const * const pHIDReportDesc, uint32_t ReportDescLength, uint32_t* mem_base, uint32_t* mem_size) ATTR_NON_NULL;
TUSB_Error_t usb_hid_configured(USBD_HANDLE_T hUsb);

TUSB_Error_t usb_hid_keyboard_sendKeys(uint8_t modifier, uint8_t keycodes[], uint8_t numkey) ATTR_NON_NULL;
TUSB_Error_t usb_hid_mouse_send(uint8_t buttons, int8_t x, int8_t y);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HID_H__ */
