/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#ifdef PICO_DEFAULT_LED_PIN
#define LED_PIN               PICO_DEFAULT_LED_PIN
#define LED_STATE_ON          (!(PICO_DEFAULT_LED_PIN_INVERTED))
#endif

// Button pin is BOOTSEL which is flash CS pin
#define BUTTON_BOOTSEL
#define BUTTON_STATE_ACTIVE   0

#if defined(PICO_DEFAULT_UART_TX_PIN) && defined(PICO_DEFAULT_UART_RX_PIN) && defined(PICO_DEFAULT_UART)
#define UART_DEV              PICO_DEFAULT_UART
#define UART_TX_PIN           PICO_DEFAULT_UART_TX_PIN
#define UART_RX_PIN           PICO_DEFAULT_UART_RX_PIN
#endif

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
