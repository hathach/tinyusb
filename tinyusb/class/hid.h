/*
 * hid.h
 *
 *  Created on: Nov 27, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
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
 * This file is part of the tinyUSB stack.
 */

/** \file
 *  \brief HID Class Driver
 *
 *  \note TBD
 */

/** 
 *  \addtogroup Group_ClassDriver Class Driver
 *  @{
 *  \defgroup Group_HID Human Interface Device
 *  @{
 */

#ifndef _TUSB_HID_H_
#define _TUSB_HID_H_

#ifdef __cplusplus
 extern "C" {
#endif

 // TODO refractor
#include "common/common.h"

#ifdef TUSB_CFG_DEVICE
  #include "device/dcd.h"
  #include "hid_device.h"
#endif

#ifdef TUSB_CFG_HOST
  #include "host/usbd_host.h"
  #include "hid_host.h"
#endif

/** \struct tusb_mouse_report_t
 *  \brief Standard HID Boot Protocol Mouse Report.
 *
 *  Type define for a standard Boot Protocol Mouse report
 */
typedef ATTR_PREPACKED struct
{
  uint8_t buttons; /**< buttons mask for currently pressed buttons in the mouse. */
  int8_t  x; /**< Current delta x movement of the mouse. */
  int8_t  y; /**< Current delta y movement on the mouse. */
} ATTR_PACKED tusb_mouse_report_t;

/** \struct tusb_keyboard_report_t
 *  \brief Standard HID Boot Protocol Keyboard Report.
 *
 *  Type define for a standard Boot Protocol Keyboard report
 */
typedef ATTR_PREPACKED struct
{
  uint8_t modifier; /**< Keyboard modifier byte, indicating pressed modifier keys (a combination of HID_KEYBOARD_MODIFER_* masks). */
  uint8_t reserved; /**< Reserved for OEM use, always set to 0. */
  uint8_t keycode[6]; /**< Key codes of the currently pressed keys. */
} ATTR_PACKED tusb_keyboard_report_t;

/** \enum USB_HID_MOUSE_BUTTON_CODE
 * \brief buttons codes for HID mouse
 */
enum USB_HID_MOUSE_BUTTON_CODE
{
	HID_MOUSEBUTTON_RIGHT = 0,
	HID_MOUSEBUTTON_LEFT = 1,
	HID_MOUSEBUTTON_MIDDLE = 2
};

/** \enum USB_HID_KB_KEYMODIFIER_CODE
 * \brief KB modifier codes for HID KB
 */
enum TUSB_KEYBOARD_MODIFIER_CODE
{
	TUSB_KEYBOARD_MODIFIER_LEFTCTRL   = BIN8(00000001),
	TUSB_KEYBOARD_MODIFIER_LEFTSHIFT  = BIN8(00000010),
	TUSB_KEYBOARD_MODIFIER_LEFTALT    = BIN8(00000100),
	TUSB_KEYBOARD_MODIFIER_LEFTGUI    = BIN8(00001000),
	TUSB_KEYBOARD_MODIFIER_RIGHTCTRL  = BIN8(00010000),
	TUSB_KEYBOARD_MODIFIER_RIGHTSHIFT = BIN8(00100000),
	TUSB_KEYBOARD_MODIFIER_RIGHTALT   = BIN8(01000000),
	TUSB_KEYBOARD_MODIFIER_RIGHTGUI   = BIN8(10000000)
};

enum TUSB_KEYBOARD_KEYCODE
{
  TUSB_KEYBOARD_KEYCODE_a = 0x04,
  TUSB_KEYBOARD_KEYCODE_z = 0x1d,

  TUSB_KEYBOARD_KEYCODE_1 = 0x1e,
  TUSB_KEYBOARD_KEYCODE_0 = 0x27
  // TODO complete keycode table
};

/** \enum USB_HID_LOCAL_CODE
 * \brief Local Country code for HID
 */
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
 }
#endif

#endif /* _TUSB_HID_H__ */

/// @}
/// @}
