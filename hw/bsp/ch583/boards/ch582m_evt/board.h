/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 TinyUSB contributors
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

/* metadata:
   name: CH582M-EVT evaluation board
   url: https://www.wch-ic.com/products/CH582.html
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

// LED: PB4 on CH582M-EVT
#define LED_PIN               GPIO_Pin_4
#define LED_STATE_ON          0

// Directly reuse BOOT pin as user button
#define BUTTON_PIN            GPIO_Pin_22
#define BUTTON_STATE_ACTIVE   0

// UART: UART1 TX=PA9, RX=PA8
#define CFG_BOARD_UART_BAUDRATE   115200

// Device only on USB1 (rhport 0). CH58x host / USB2 is not supported by the shared usbfs dcd;
// BOARD_TUH_RHPORT is kept commented out to ease re-adding host later.
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT  0
#endif
// #ifndef BOARD_TUH_RHPORT
// #define BOARD_TUH_RHPORT  1
// #endif

#ifdef __cplusplus
}
#endif

#endif
