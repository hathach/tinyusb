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
   name: RX65N Target Board
   url: https://www.renesas.com/en/products/microcontrollers-microprocessors/rx-32-bit-performance-efficiency-mcus/rtk5rx65n0c00000br-target-board-rx65n
*/

#ifndef BOARD_H_
#define BOARD_H_

#include "iodefine.h"

#ifdef __cplusplus
 extern "C" {
#endif

// LED: PD6, active low (open-drain)
#define BOARD_LED_WRITE(state)    (PORTD.PODR.BIT.B6 = (state) ? 0 : 1)

// Button: PB1, active low
#define BOARD_BUTTON_READ()       (PORTB.PIDR.BIT.B1 ? 0 : 1)

// UART: SCI5
#define BOARD_UART_SCI            SCI5
#define BOARD_SCI_TXI_HANDLER     INT_Excep_SCI5_TXI5
#define BOARD_SCI_TEI_HANDLER     INT_Excep_ICU_GROUPBL0  // SCI5 TEI uses group interrupt
#define BOARD_SCI_RXI_HANDLER     INT_Excep_SCI5_RXI5

// USB interrupt handler (software configurable vector)
#define IRQ_USB0_USBI0            62
#define SLIBR_USBI0               SLIBR185
#define IR_USB0_USBI0             IR_PERIB_INTB185
#define IER_USB0_USBI0            IER_PERIB_INTB185
#define IEN_USB0_USBI0            IEN_PERIB_INTB185
#define IPR_USB0_USBI0            IPR_PERIB_INTB185
#define INT_Excep_USB0_USBI0      INT_Excep_PERIB_INTB185
#define BOARD_USB_IRQ_HANDLER     INT_Excep_USB0_USBI0

// Clocks
#define BOARD_PCLK                60000000
#define BOARD_CPUCLK              120000000

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
