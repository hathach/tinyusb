/**************************************************************************/
/*!
    @file     tusb_config.h
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

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#include "tusb_option.h"
#include "bsp/board.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
  #error CFG_TUSB_MCU must be defined
#endif

#if CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_LPC18XX
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_HOST | OPT_MODE_HIGH_SPEED)
#else
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE
#endif

#define CFG_TUSB_DEBUG              2
#define CFG_TUSB_OS                 OPT_OS_NONE

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          ATTR_ALIGNED(4)
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUH_HUB               1
#define CFG_TUH_CDC               1
#define CFG_TUSB_HOST_HID_KEYBOARD      0
#define CFG_TUSB_HOST_HID_MOUSE         0
#define CFG_TUSB_HOST_HID_GENERIC       0 // (not yet supported)
#define CFG_TUH_MSC               0

#define CFG_TUSB_HOST_DEVICE_MAX        (CFG_TUH_HUB ? 5 : 1) // normal hub has 4 ports

//------------- CLASS -------------//
#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_CUSTOM_CLASS        0

#define CFG_TUD_HID                 0
#define CFG_TUD_HID_KEYBOARD        0
#define CFG_TUD_HID_MOUSE           0

//--------------------------------------------------------------------
// CDC
//--------------------------------------------------------------------

// FIFO size of CDC TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE      64
#define CFG_TUD_CDC_TX_BUFSIZE      64

//--------------------------------------------------------------------
// MSC
//--------------------------------------------------------------------
// Number of supported Logical Unit Number (At least 1)
#define CFG_TUD_MSC_MAXLUN          1

// Buffer size of Device Mass storage
#define CFG_TUD_MSC_BUFSIZE         512

// Vendor name included in Inquiry response, max 8 bytes
#define CFG_TUD_MSC_VENDOR          "tinyusb"

// Product name included in Inquiry response, max 16 bytes
#define CFG_TUD_MSC_PRODUCT         "tusb msc"

// Product revision string included in Inquiry response, max 4 bytes
#define CFG_TUD_MSC_PRODUCT_REV     "1.0"

//--------------------------------------------------------------------
// HID
//--------------------------------------------------------------------

/* Use the HID_ASCII_TO_KEYCODE lookup if CFG_TUD_HID_KEYBOARD is enabled.
 * This will occupies 256 bytes of ROM. It will also enable the use of 2 extra APIs
 * - tud_hid_keyboard_send_char()
 * - tud_hid_keyboard_send_string()
 */
#define CFG_TUD_HID_ASCII_TO_KEYCODE_LOOKUP 1


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
