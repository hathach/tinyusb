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

#include "fsl_device_registers.h"

// LED
// The Red   LED is on PTE29.
// The Green LED is on PTC4.
// The Blue  LED is on PTE31.
#define LED_PIN_CLOCK         kCLOCK_PortC
#define LED_GPIO              GPIOC
#define LED_PORT              PORTC
#define LED_PIN               4
#define LED_STATE_ON          0

// SW3 button1
#define BUTTON_PIN_CLOCK      kCLOCK_PortE
#define BUTTON_GPIO           GPIOE
#define BUTTON_PORT           PORTE
#define BUTTON_PIN            4
#define BUTTON_STATE_ACTIVE   0

// UART
#define UART_PORT             LPUART0
#define UART_PIN_CLOCK        kCLOCK_PortB
#define UART_PIN_GPIO         GPIOB
#define UART_PIN_PORT         PORTB
#define UART_PIN_RX           16u
#define UART_PIN_TX           17u

#endif /* BOARD_H_ */
