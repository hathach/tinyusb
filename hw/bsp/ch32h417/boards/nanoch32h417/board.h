/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   name: nanoCH32H417
   url: https://github.com/wuxx/nanoCH32H417
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

// Green LED D2: PC2, active low (anode to VDDIO through 1K). The blue LED D1 is on PC3.
#define LED_PORT              GPIOC
#define LED_PIN               GPIO_Pin_2
#define LED_STATE_ON          0
#define LED_CLOCK             RCC_HB2Periph_GPIOC

// No user button on this board (S2/S3 belong to the on-board WCH-LinkE)

// USART1 TX = PA9, RX = PA10, wired to the on-board WCH-LinkE virtual COM port
#ifndef CFG_BOARD_UART_BAUDRATE
#define CFG_BOARD_UART_BAUDRATE   115200
#endif

// Single device rhport 0: USB3.0 SuperSpeed (SPEED=super) or USB2.0 HighSpeed (SPEED=high),
// both on the USB 3.0 Type-A receptacle (the USB-C port is the full-speed OTG/PD controller,
// not supported)
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT  0
#endif

#ifdef __cplusplus
}
#endif

#endif
