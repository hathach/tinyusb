/*
 * The MIT License (MIT)
 *
 * Copyright 2021 Bridgetek Pte Ltd
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

// Note: This definition file covers all MM900EV1B, MM900EV2B, and MM900EV3B boards.
// Each of these boards has an FT900 device.

#ifdef __cplusplus
 extern "C" {
#endif

#define GPIO_UART0_TX 48
#define GPIO_UART0_RX 49
#define GPIO_ETH_LED0 61
#define GPIO_ETH_LED1 62
#define GPIO_REMOTE_WAKEUP_PIN 18
#define USBD_VBUS_DTC_PIN 3

#define GPIO_REMOTE_WAKEUP

#ifdef __cplusplus
 }
#endif

#endif
