/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   name: CH32H417QEU6-R0 EVT
   url: https://www.wch-ic.com/products/CH32H417.html
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

// LED1: jumper P8-27 (PE2) to P8-29 (LED1), active low (anode to VDDIO through 1K)
#define LED_PORT              GPIOE
#define LED_PIN               GPIO_Pin_2
#define LED_STATE_ON          0
#define LED_CLOCK             RCC_HB2Periph_GPIOE

// KEY (S2): optional jumper P5-2 (KEY) to P5-4 (PB6), active low
#define BUTTON_PORT           GPIOB
#define BUTTON_PIN            GPIO_Pin_6
#define BUTTON_STATE_ACTIVE   0
#define BUTTON_CLOCK          RCC_HB2Periph_GPIOB

// USART1 TX = PA9 (P5-32), RX = PA10 (P5-30), wired to an external WCH-LinkE virtual COM port
#ifndef CFG_BOARD_UART_BAUDRATE
#define CFG_BOARD_UART_BAUDRATE   115200
#endif

// Single device rhport 0: USB3.0 SuperSpeed (SPEED=super) or USB2.0 HighSpeed (SPEED=high),
// both on the USB 3.0 Type-A receptacle P2 (P1 Type-C and P4 are the full-speed OTG port,
// not supported). SDI shares the USB2 pair (P3-5 SWDIO = PB9 = DM, P3-7 SWCLK = PB8 = DP): wire
// the LinkE SDI only to bootstrap or recover with P2 unplugged, and reflash over the USART1
// loader otherwise (see family.c). LinkE RST to P6-8 (NRST) enables wlink pin-rst erase.
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT  0
#endif

#ifdef __cplusplus
}
#endif

#endif
