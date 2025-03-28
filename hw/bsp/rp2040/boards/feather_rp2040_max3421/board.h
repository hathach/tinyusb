/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ha Thach (tinyusb.org)
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

#ifndef TUSB_BOARD_H
#define TUSB_BOARD_H

#ifdef __cplusplus
 extern "C" {
#endif

// UART and LED are already defined in pico-sdk board

//--------------------------------------------------------------------+
// PIO_USB
//--------------------------------------------------------------------+

#define PICO_DEFAULT_PIO_USB_DP_PIN       16
#define PICO_DEFAULT_PIO_USB_VBUSEN_PIN   18
#define PICO_DEFAULT_PIO_USB_VBUSEN_STATE 1

//--------------------------------------------------------------------
// USB Host MAX3421E
//--------------------------------------------------------------------

#ifdef PICO_DEFAULT_SPI
#define MAX3421_SPI      PICO_DEFAULT_SPI // sdk v2
#else
#define MAX3421_SPI      PICO_DEFAULT_SPI_INSTANCE // sdk v1
#endif

#define MAX3421_SCK_PIN  PICO_DEFAULT_SPI_SCK_PIN
#define MAX3421_MOSI_PIN PICO_DEFAULT_SPI_TX_PIN
#define MAX3421_MISO_PIN PICO_DEFAULT_SPI_RX_PIN
#define MAX3421_CS_PIN   10
#define MAX3421_INTR_PIN 9

#ifdef __cplusplus
 }
#endif

#endif
