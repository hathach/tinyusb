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
   name: nanoCH32H417
   url: https://github.com/wuxx/nanoCH32H417
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

// Green LED D2: PC2, active low (anode to VDDIO through 1K). The blue LED D1 is on PC3.
#define LED_PORT              GPIOC
#define LED_PIN               GPIO_Pin_2
#define LED_STATE_ON          0
#define LED_CLOCK             RCC_HB2Periph_GPIOC

// No user button on this board (S2/S3 belong to the on-board WCH-LinkE)

// USART1 TX = PA9, RX = PA10, wired to the on-board WCH-LinkE virtual COM port
#define CFG_BOARD_UART_BAUDRATE   115200

// Single device rhport 0: USB3.0 SuperSpeed (SPEED=super) or USB2.0 HighSpeed (SPEED=high),
// both on the USB 3.0 Type-A receptacle (the USB-C port is the full-speed OTG/PD controller,
// not supported)
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT  0
#endif

#ifdef __cplusplus
}
#endif

#endif
