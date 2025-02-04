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
   name: LPCXpresso18s37
   url: https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-boards/lpcxpresso18s37-development-board:OM13076
*/

#ifndef BOARD_H_
#define BOARD_H_

// Note: For USB Host demo, install JP4
// WARNING: don't install JP4 when running as device

#ifdef __cplusplus
 extern "C" {
#endif

// LED Red
#define LED_PORT      3
#define LED_PIN       7

// ISP Button
#define BUTTON_PORT   0
#define BUTTON_PIN    7

#define UART_DEV      LPC_USART0

static inline void board_lpc18_pinmux(void)
{
  const PINMUX_GRP_T pinmuxing[] =
  {
    // LEDs
    { 0x6, 9 , SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP | SCU_MODE_FUNC0 },
    { 0x6, 11, SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP | SCU_MODE_FUNC0 },

    // Button
    { 0x2, 7, SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0 },

    // UART
    { 0x06, 4, SCU_MODE_PULLDOWN | SCU_MODE_FUNC2 },
    { 0x02, 1, SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC1 },

    // USB0
    //{ 0x6, 3, SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC1 },		                // P6_3 USB0_PWR_EN, USB0 VBus function

    // USB1
    //{ 0x9, 5, SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC2 },			              // P9_5 USB1_VBUS_EN, USB1 VBus function
    //{ 0x2, 5, SCU_MODE_INACT  | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2 }, // P2_5 USB1_VBUS, MUST CONFIGURE THIS SIGNAL FOR USB1 NORMAL OPERATION
    {0x2, 5, SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP | SCU_MODE_FUNC4 },
  };

  Chip_SCU_SetPinMuxing(pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
}

#ifdef __cplusplus
 }
#endif

#endif
