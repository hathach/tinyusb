/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
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
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

#ifndef CFG_TUH_MEM_SECTION
#define CFG_TUH_MEM_SECTION
#endif

#ifndef CFG_TUH_MEM_ALIGN
#define CFG_TUH_MEM_ALIGN     __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------+
// Host Configuration
// Adafruit Feather RP2040 USB Host: PIO-USB on GP16/GP17 (USB-A Host port)
//--------------------------------------------------------------------+

#define CFG_TUH_ENABLED       1

// PIO-USB Host on rhport 1 (USB-A connector on GP16/GP17)
#define CFG_TUH_RPI_PIO_USB   1
#define BOARD_TUH_RHPORT      1

#ifndef BOARD_TUH_MAX_SPEED
#define BOARD_TUH_MAX_SPEED   OPT_MODE_FULL_SPEED
#endif

#define CFG_TUH_MAX_SPEED     BOARD_TUH_MAX_SPEED

//--------------------------------------------------------------------+
// Driver Configuration
//--------------------------------------------------------------------+

#define CFG_TUH_ENUMERATION_BUFSIZE 256

#define CFG_TUH_HUB                0
#define CFG_TUH_DEVICE_MAX         1

// MIDI 2.0 Host
#define CFG_TUH_MIDI2              1
#define CFG_TUH_MIDI2_RX_BUFSIZE   512
#define CFG_TUH_MIDI2_TX_BUFSIZE   512

#ifdef __cplusplus
}
#endif

#endif
