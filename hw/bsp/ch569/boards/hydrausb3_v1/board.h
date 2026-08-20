/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   name: HydraUSB3 v1
   url: https://github.com/hydrausb3/hydrausb3_hw
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

// ULED: PB22, active high
#define LED_PIN               GPIO_Pin_22
#define LED_STATE_ON          1

// UBTN: PB23 (floating input on the board, polarity assumed active high - family.c enables an
// internal pull to the inactive level)
#define BUTTON_PIN            GPIO_Pin_23
#define BUTTON_STATE_ACTIVE   1

// UART1: TXD1 = PA8, RXD1 = PA7
#define CFG_BOARD_UART_BAUDRATE   115200

// Single device rhport 0: USB3.0 SuperSpeed (SPEED=super) or USB2.0 HighSpeed (SPEED=high)
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT  0
#endif

#ifdef __cplusplus
}
#endif

#endif
