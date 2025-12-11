/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to do so, subject to the
 * following conditions:
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
   name: SAME70 Xplained
   manufacturer: Microchip
   url: https://www.microchip.com/en-us/development-tool/atsame70-xpld
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LED_PIN GPIO(GPIO_PORTC, 8)
#define LED_STATE_ON 1
#define LED_PORT_CLOCK ID_PIOC

#define BUTTON_PIN GPIO(GPIO_PORTA, 11)
#define BUTTON_STATE_ACTIVE 0
#define BUTTON_PORT_CLOCK ID_PIOA

#define UART_TX_PIN GPIO(GPIO_PORTB, 4)
#define UART_TX_FUNCTION MUX_PB4D_USART1_TXD1
#define UART_RX_PIN GPIO(GPIO_PORTA, 21)
#define UART_RX_FUNCTION MUX_PA21A_USART1_RXD1
#define UART_PORT_CLOCK ID_USART1
#define BOARD_USART USART1

static inline void board_vbus_set(uint8_t rhport, bool state) {
  (void) rhport;
  (void) state;
}

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
