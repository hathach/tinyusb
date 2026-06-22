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

/* metadata:
   name: TM4C1294 LaunchPad
   url: https://www.ti.com/tool/EK-TM4C1294XL
*/

#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "TM4C129.h"

#define BOARD_UART            UART0
#define BOARD_UART_PORT       GPIOA

#define BTN_PORT_CLK          8
#define BOARD_BTN_PORT        GPIOJ
#define BOARD_BTN             0
#define BOARD_BTN_Msk         (1u<<0)
#define BUTTON_STATE_ACTIVE   0

#define LED_PORT_CLK          12
#define LED_PORT              GPION
#define LED_PIN_1             1
#define LED_PIN_2             0
#define LED_STATE_ON          1

#define BOARD_LED_PIN         LED_PIN_2

#define GPIOA                 GPIOA_AHB
#define GPIOB                 GPIOB_AHB
#define GPIOC                 GPIOC_AHB
#define GPIOD                 GPIOD_AHB
#define GPIOE                 GPIOE_AHB
#define GPIOF                 GPIOF_AHB
#define GPIOG                 GPIOG_AHB
#define GPIOH                 GPIOH_AHB
#define GPIOI                 GPIOI_AHB
#define GPIOJ                 GPIOJ_AHB

#define GPIOA_Type            GPIOA_AHB_Type

#ifdef __cplusplus
 }
#endif

#endif
