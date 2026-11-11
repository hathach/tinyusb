/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   name: CH32V407VET6-EVT-R0
   url: https://www.wch-ic.com/products/CH32V407.html
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

// LED: wire LED1 of header P1 to PC0
#define LED_PORT         GPIOC
#define LED_PIN          GPIO_Pin_0
#define LED_STATE_ON     0
#define LED_CLOCK_EN()   RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOC, ENABLE)

// Button: wire KEY of header P1 to PC1
#define BUTTON_PORT         GPIOC
#define BUTTON_PIN          GPIO_Pin_1
#define BUTTON_STATE_ACTIVE 0
#define BUTTON_CLOCK_EN()   RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOC, ENABLE)

// UART: USART1 TX PA9 / RX PA10, routed to the WCH-Link VCP on header P13
#define UART_DEV       USART1
#define UART_CLOCK_EN() RCC_PB2PeriphClockCmd(RCC_PB2Periph_USART1 | RCC_PB2Periph_GPIOA, ENABLE)
#define UART_TX_PORT   GPIOA
#define UART_TX_PIN    GPIO_Pin_9
#define UART_RX_PORT   GPIOA
#define UART_RX_PIN    GPIO_Pin_10

#ifdef __cplusplus
}
#endif

#endif
