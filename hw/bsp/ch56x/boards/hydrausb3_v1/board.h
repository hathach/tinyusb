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
 */

/* metadata:
   name: HydraUSB3 v1
   url: https://github.com/hydrausb3/hydrausb3_hw
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

// ULED: PB22, active high
#define LED_PIN               GPIO_Pin_22
#define LED_STATE_ON          1

// UBTN: PB23 (floating input on the board, polarity assumed active high - family.c enables an
// internal pull to the inactive level)
#define BUTTON_PIN            GPIO_Pin_23
#define BUTTON_STATE_ACTIVE   1

// UART1: TXD1 = PA8, RXD1 = PA7
#define CFG_BOARD_UART_BAUDRATE   115200

// Single device rhport 0: USB3.0 SuperSpeed (SPEED=super) or USB2.0 HighSpeed (SPEED=high)
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT  0
#endif

#ifdef __cplusplus
}
#endif

#endif
