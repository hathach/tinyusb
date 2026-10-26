/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ha Thach (tinyusb.org)
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
   name: Pico 2 ETM Trace Carrier
   url: https://github.com/hathach/pcb/tree/main/pico2_trace_motherboard
*/

// Raspberry Pi Pico 2 seated on the "pico2 trace motherboard" carrier: a
// MIPI-20 Cortex Debug+ETM adapter (SWD + 4-bit trace) plus a TinyUSB test
// bench. Same RP2350 module as raspberry_pi_pico2, different pin map: the
// carrier keeps GP1-5 free for TRACECLK/TRACEDATA0-3 and moves the console,
// LED, button and USB control pins out of the way.
//
// Carrier pin map (only the pins the BSP uses are defined below):
//   0      GND guard (JP2)            1      TRACECLK
//   2-5    TRACEDATA0-3               6      GND guard (JP3)
//   8/9    I2C0 SDA/SCL (STEMMA-QT)   10     user LED
//   11     device D+ pull-up enable   12/13  UART0 TX/RX (console)
//   14     user button (to GND, unused - BSP uses BOOTSEL)
//   15     host VBUS fault
//   16     native VBUS-detect tap     17     host VBUS enable
//   18/19  PIO-USB device D+/D- (J9)  20/21  PIO-USB host D+/D- (J5)
//   26     VBUS current sense (ADC)   27     J9 device VBUS-detect

#ifndef TUSB_BOARD_H
#define TUSB_BOARD_H

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// LED, UART (button: the family BSP uses BOOTSEL, like every rp2040 board)
//--------------------------------------------------------------------+
#define LED_PIN               10
#define LED_STATE_ON          1

// console is on GP12/13, NOT the pico default GP0/1: GP1 is TRACECLK, so the
// console stays full-duplex while tracing
#define UART_DEV              0     // uart0 (index, see uart_get_instance)
#define UART_TX_PIN           12
#define UART_RX_PIN           13

//--------------------------------------------------------------------+
// PIO_USB
//--------------------------------------------------------------------+
// host port J5 (USB-A): D+ = GP20, D- = GP21, load switch enable = GP17
#define PICO_DEFAULT_PIO_USB_DP_PIN       20
#define PICO_DEFAULT_PIO_USB_VBUSEN_PIN   17
#define PICO_DEFAULT_PIO_USB_VBUSEN_STATE 1

#ifdef __cplusplus
 }
#endif

#endif
