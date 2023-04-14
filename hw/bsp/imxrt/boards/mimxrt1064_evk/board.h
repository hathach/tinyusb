/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Ha Thach (tinyusb.org)
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

// required since iMX RT10xx SDK include this file for board size
#define BOARD_FLASH_SIZE (0x400000U)

// LED
#define LED_PINMUX            IOMUXC_GPIO_AD_B0_09_GPIO1_IO09
#define LED_PORT              GPIO1
#define LED_PIN               9
#define LED_STATE_ON          0

// SW8 button
#define BUTTON_PINMUX         IOMUXC_SNVS_WAKEUP_GPIO5_IO00
#define BUTTON_PORT           GPIO5
#define BUTTON_PIN            0
#define BUTTON_STATE_ACTIVE   0

// UART
#define UART_PORT             LPUART1
#define UART_RX_PINMUX        IOMUXC_GPIO_AD_B0_13_LPUART1_RX
#define UART_TX_PINMUX        IOMUXC_GPIO_AD_B0_12_LPUART1_TX


#endif /* BOARD_H_ */
