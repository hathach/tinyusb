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

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

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

#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX              2

// 16bit data in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX      2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX              16

// UAC1 Full-Speed endpoint size
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS     48000
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS           TUD_AUDIO_EP_SIZE(false, CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
// UAC2 High-Speed endpoint size
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_HS     96000
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS           TUD_AUDIO_EP_SIZE(true, CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_HS, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX          TU_MAX(CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS)

// AUDIO_FEEDBACK_METHOD_FIFO_COUNT needs buffer size >= 4* EP size to work correctly
// Example read FIFO every 1ms (8 HS frames), so buffer size should be 8 times larger for HS device
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ       TU_MAX(4 * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS, 32 * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS)

// Enable OUT EP
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                 1

// Enable feedback EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP            1

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
