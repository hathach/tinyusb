/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020, Ha Thach (tinyusb.org)
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

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define _PINNUM(port, pin)    ((port)*32 + (pin))

// LED
#define LED_PIN               13
#define LED_STATE_ON          0

// Button
#define BUTTON_PIN            25 // button 4
#define BUTTON_STATE_ACTIVE   0

// UART
#define UART_RX_PIN           8
#define UART_TX_PIN           6

// SPI for USB host shield
// Pin is correct but not working probably due to signal incompatible (1.8V 3v3) with MAC3421E !?
//#define MAX3421_SCK_PIN  _PINNUM(1, 15)
//#define MAX3421_MOSI_PIN _PINNUM(1, 13)
//#define MAX3421_MISO_PIN _PINNUM(1, 14)
//#define MAX3421_CS_PIN   _PINNUM(1, 12)
//#define MAX3421_INTR_PIN _PINNUM(1, 11)

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
