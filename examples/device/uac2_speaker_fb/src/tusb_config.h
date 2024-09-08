/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Jerzy Kasenberg
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

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "usb_descriptors.h"

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

// It's recommended to disable debug unless for control requests debugging,
// as the extra time needed will impact data stream !
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

/* (Needed for Full-Speed only)
 * Enable host OS guessing to workaround UAC2 compatibility issues between Windows and OS X
 * The default configuration only support Windows and Linux, enable this option for OS X
 * support. Otherwise if you don't need Windows support you can make OS X's configuration as
 * default.
 */
#define CFG_QUIRK_OS_GUESSING   1

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

// Expose audio class debug information via HID interface
#ifndef CFG_AUDIO_DEBUG
#define CFG_AUDIO_DEBUG           1
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

#define CFG_TUD_HID_EP_BUFSIZE    64

//------------- CLASS -------------//
#define CFG_TUD_AUDIO             1

#if CFG_AUDIO_DEBUG
#define CFG_TUD_HID               1
#else
#define CFG_TUD_HID               0
#endif

#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

//--------------------------------------------------------------------
// AUDIO CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                                TUD_AUDIO_SPEAKER_STEREO_FB_DESC_LEN

// Enable if Full-Speed on OSX, also set feedback EP size to 3
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION              0

// Audio format type I specifications
#if defined(__RX__)
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE                         48000
#else
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE                         96000
#endif

#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX                           2

// 16bit in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX                   2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX                           16

// EP and buffer size - for isochronous EP´s, the buffer and EP size are equal (different sizes would not make sense)
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                                  1

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX        TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ     (TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX // Example read FIFO every 1ms, so it should be 8 times larger for HS device

// Enable feedback EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                             1

// Number of Standard AS Interface Descriptors (4.9.1) defined per audio function - this is required to be able to remember the current alternate settings of these interfaces - We restrict us here to have a constant number for all audio functions (which means this has to be the maximum number of AS interfaces an audio function has and a second audio function with less AS interfaces just wastes a few bytes)
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 	                             1

// Size of control request buffer
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ	                         64

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
