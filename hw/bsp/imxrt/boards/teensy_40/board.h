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
#define BOARD_FLASH_SIZE (2 * 1024 * 1024)

// LED
#define LED_PINMUX            IOMUXC_GPIO_B0_03_GPIO2_IO03 // D13
#define LED_PORT              GPIO2
#define LED_PIN               3
#define LED_STATE_ON          0

// no button
#define BUTTON_PINMUX         IOMUXC_GPIO_B0_01_GPIO2_IO01 // D12
#define BUTTON_PORT           GPIO2
#define BUTTON_PIN            1
#define BUTTON_STATE_ACTIVE   0

// UART
#define UART_PORT             LPUART6
#define UART_RX_PINMUX        IOMUXC_GPIO_AD_B0_03_LPUART6_RX  // D0
#define UART_TX_PINMUX        IOMUXC_GPIO_AD_B0_02_LPUART6_TX  // D1

#endif /* BOARD_H_ */
