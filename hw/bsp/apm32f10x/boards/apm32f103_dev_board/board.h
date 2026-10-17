/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
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

/* metadata:
   name: APM32F103 Dev Board
   url: https://www.geehy.com
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define LED_PORT              GPIOE
#define LED_PIN               GPIO_PIN_5
#define LED_STATE_ON          0
#define LED_GPIO_CLK_EN()     RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOE)

#define BUTTON_PORT           GPIOA
#define BUTTON_PIN            GPIO_PIN_1
#define BUTTON_STATE_ACTIVE   1
#define BUTTON_GPIO_CLK_EN()  RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOA)

//--------------------------------------------------------------------+
// UART (USART1: PA9 = TX, PA10 = RX)
//--------------------------------------------------------------------+
#define UART_ID               1
#define UART_TX_PORT          GPIOA
#define UART_TX_PIN           GPIO_PIN_9
#define UART_RX_PORT          GPIOA
#define UART_RX_PIN           GPIO_PIN_10
#define UART_GPIO_CLK_EN()    RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOA)

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
