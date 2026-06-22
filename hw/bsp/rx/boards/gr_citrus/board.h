/*
* The MIT License (MIT)
 *
 * Copyright (c) 2020, Ha Thach (tinyusb.org)
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
   name: GR Citrus
   url: https://www.renesas.com/en/products/gadget-renesas/boards/gr-citrus
*/

#ifndef BOARD_H_
#define BOARD_H_

#include "iodefine.h"

#ifdef __cplusplus
 extern "C" {
#endif

// LED: PA0, active high
#define BOARD_LED_WRITE(state)    (PORTA.PODR.BIT.B0 = (state) ? 1 : 0)

// No user button
#define BOARD_BUTTON_READ()       0

// UART: SCI0
#define BOARD_UART_SCI            SCI0
#define BOARD_SCI_TXI_HANDLER     INT_Excep_SCI0_TXI0
#define BOARD_SCI_TEI_HANDLER     INT_Excep_SCI0_TEI0
#define BOARD_SCI_RXI_HANDLER     INT_Excep_SCI0_RXI0

// USB interrupt handler
#define BOARD_USB_IRQ_HANDLER     INT_Excep_USB0_USBI0

// Clocks
#define BOARD_PCLK                48000000
#define BOARD_CPUCLK              96000000

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
