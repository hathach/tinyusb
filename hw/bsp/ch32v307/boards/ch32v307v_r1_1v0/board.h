/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (tinyusb.org) for Adafruit Industries
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
   name: CH32V307V-R1-1v0
   url: https://github.com/openwch/ch32v307/tree/main/SCHPCB/CH32V307V-R1-1v0
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

// LED: need to wire pin LED1 to PC0 in the J3 header
#define LED_PORT              GPIOC
#define LED_PIN               GPIO_Pin_0
#define LED_STATE_ON          0
#define LED_CLOCK_EN()        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE)

// Button: need to wire pin KEY to PC1 in the J3 header
#define BUTTON_PORT           GPIOC
#define BUTTON_PIN            GPIO_Pin_1
#define BUTTON_STATE_ACTIVE   0
#define BUTTON_CLOCK_EN()     do { } while(0) // same as LED clock, no need to do anything

// TODO UART port

#ifdef __cplusplus
 }
#endif

#endif
