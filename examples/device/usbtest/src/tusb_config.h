/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

// Espressif IDF requires "freertos/" prefix in include path
#ifdef ESP_PLATFORM
#define CFG_TUSB_OS_INC_PATH  freertos/
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

// Enable Device stack
#define CFG_TUD_ENABLED       1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

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
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//------------- CLASS -------------//
#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_HID               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            1

// Non-buffered mode: every transfer is submitted with an exact length so the
// host never sees an unexpected short packet or ZLP mid-transfer, which the
// usbtest data-integrity cases treat as failure.
#define CFG_TUD_VENDOR_RX_BUFSIZE     0
#define CFG_TUD_VENDOR_TX_BUFSIZE     0

// App re-arms RX itself: required to recover the sink after halt tests
// (SET_FEATURE/CLEAR_FEATURE endpoint halt) where no completion ever fires.
#define CFG_TUD_VENDOR_RX_MANUAL_XFER 1

// Multi-packet IN transfers; must be a multiple of bulk MPS at both speeds (64/512).
// LPC11/13 (ip3511 FS) keep endpoint buffers in a dedicated 2 KB USB RAM: a 2048 B bulk epbuf
// overflows it once the int/iso buffers join, so those parts use 512 (= 8 FS packets).
#if TU_CHECK_MCU(OPT_MCU_LPC11UXX, OPT_MCU_LPC13XX)
#define CFG_TUD_VENDOR_TX_EPSIZE      512
#else
#define CFG_TUD_VENDOR_TX_EPSIZE      2048
#endif

// Interrupt IN/OUT source/sink pair (usbtest cases 25/26). Buffer sizes track the
// per-speed endpoint max packet size (see usb_descriptors.c) so full-speed builds
// don't over-allocate the scarce USB DMA section.
#define CFG_TUD_VENDOR_EP_INT_OUT          1
#define CFG_TUD_VENDOR_EP_INT_IN           1
#define CFG_TUD_VENDOR_EP_INT_OUT_BUFSIZE  (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)

// Isochronous IN/OUT source/sink pair (usbtest cases 15/16/22/23), placed in
// altsetting 1: alt 0 has no endpoints so no iso bandwidth is claimed by default
#define CFG_TUD_VENDOR_EP_ISO_OUT          1
#define CFG_TUD_VENDOR_EP_ISO_IN           1
#define CFG_TUD_VENDOR_EP_ISO_OUT_BUFSIZE  (TUD_OPT_HIGH_SPEED ? 512 : 128)
#define CFG_TUD_VENDOR_EP_ISO_IN_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 128)
#define CFG_TUD_VENDOR_ALT_SETTINGS        1

// CH32V20X fsdev port has only 512 B PMA and single-buffered iso can't keep the iso IN endpoint
// fed under load. Double-buffer iso; the descriptor drops iso mps to 32 there so 2x32 = 64 B/ep
// keeps the same PMA budget (see usb_descriptors.h). Other fsdev parts have room and stay single.
#if CFG_TUSB_MCU == OPT_MCU_CH32V20X
#define CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP  1
#endif

#ifdef __cplusplus
 }
#endif

#endif /* TUSB_CONFIG_H_ */
