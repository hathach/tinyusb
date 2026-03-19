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
   name: LPCXpresso54628
   url: https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-boards/lpcxpresso54628-development-board:OM13098
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

// IOCON pin mux
#define IOCON_PIO_DIGITAL_EN     0x0100u   // Enables digital function
#define IOCON_PIO_FUNC0          0x00u
#define IOCON_PIO_FUNC1          0x01u   // Selects pin function 1
#define IOCON_PIO_FUNC7          0x07u   // Selects pin function 7
#define IOCON_PIO_INPFILT_OFF    0x0200u // Input filter disabled
#define IOCON_PIO_INV_DI         0x00u   // Input function is not inverted
#define IOCON_PIO_MODE_INACT     0x00u   // No addition pin function
#define IOCON_PIO_MODE_PULLUP    0x10u
#define IOCON_PIO_OPENDRAIN_DI   0x00u   // Open drain is disabled
#define IOCON_PIO_SLEW_STANDARD  0x00u   // Standard mode, output slew rate control is enabled

// LED
#define LED_PORT              2
#define LED_PIN               2
#define LED_STATE_ON          0

// WAKE button
#define BUTTON_PORT           1
#define BUTTON_PIN            1
#define BUTTON_STATE_ACTIVE   0

// UART
#define UART_DEV              USART0
#define UART_RX_PINMUX        0, 29, IOCON_PIO_DIG_FUNC1_EN
#define UART_TX_PINMUX        0, 30, IOCON_PIO_DIG_FUNC1_EN

// USB0 VBUS
#define USB0_VBUS_PINMUX      0, 22, IOCON_PIO_DIG_FUNC7_EN

// XTAL
//#define XTAL0_CLK_HZ          (16 * 1000 * 1000U)

#ifdef __cplusplus
 }
#endif

#endif
