/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   manufacturer: WCH
*/

#include <string.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

#include "ch32v4x7.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USBHS1_IRQHandler(void);
void USBHS2_IRQHandler(void);
void SysTick_Handler(void);

__attribute__((interrupt)) void USBHS1_IRQHandler(void) {
  tusb_int_handler(0, true);
}

__attribute__((interrupt)) void USBHS2_IRQHandler(void) {
  tusb_int_handler(1, true);
}

//--------------------------------------------------------------------+
// Board init
//--------------------------------------------------------------------+
static bool usbhs_clock_init(void) {
  // USBHSPLL feeds the UTMI PHYs of both controllers; its source and reference are writable only while
  // it is off (RM 3.4.12)
  if ((RCC->CTLR & RCC_USBHSPLLRDY) == 0) {
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS1 | RCC_HBPeriph_USBHS2, DISABLE);
    RCC->CTLR &= ~RCC_USBHSPLLON;
    RCC_USBHSPLLCLKConfig(RCC_USBHSPLLCLKSource_HSE);
    RCC_USBHSPLLReferConfig(RCC_USBHSPLLCKREFCLK_25M);
    RCC->CTLR |= RCC_USBHSPLLON;
    for (uint32_t timeout = 1000000u; timeout && !(RCC->CTLR & RCC_USBHSPLLRDY); timeout--) {}
    if ((RCC->CTLR & RCC_USBHSPLLRDY) == 0) {
      return false;
    }
  }

  // the functions, not the RCC_UTMIxON header macros: those are swapped against RM RCC_CFGR2
  RCC_UTMI1cmd(ENABLE);
  RCC_UTMI2cmd(ENABLE);
  RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS1 | RCC_HBPeriph_USBHS2, ENABLE);
  return true;
}

void board_init(void) {
  __disable_irq();

#if CFG_TUSB_OS == OPT_OS_NONE
  NVIC_EnableIRQ(SysTick_IRQn);
  SysTick->CTLR = 0;
  SysTick->SR = 0;
  SysTick->CNT = 0;
  SysTick->CMP = SystemCoreClock / 1000 - 1;
  SysTick->CTLR = 0xF; // enable, interrupt, HCLK source, auto-reload
#endif

  GPIO_InitTypeDef gpio_init = {0};
  gpio_init.GPIO_Speed = GPIO_Speed_High;

  LED_CLOCK_EN();
  gpio_init.GPIO_Pin = LED_PIN;
  gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(LED_PORT, &gpio_init);
  board_led_write(false);

  BUTTON_CLOCK_EN();
  gpio_init.GPIO_Pin = BUTTON_PIN;
  gpio_init.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(BUTTON_PORT, &gpio_init);

  UART_CLOCK_EN();
  gpio_init.GPIO_Pin = UART_TX_PIN;
  gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(UART_TX_PORT, &gpio_init);
  gpio_init.GPIO_Pin = UART_RX_PIN;
  gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(UART_RX_PORT, &gpio_init);

  USART_InitTypeDef uart_init = {0};
  uart_init.USART_BaudRate = CFG_BOARD_UART_BAUDRATE;
  uart_init.USART_WordLength = USART_WordLength_8b;
  uart_init.USART_StopBits = USART_StopBits_1;
  uart_init.USART_Parity = USART_Parity_No;
  uart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  uart_init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
  USART_Init(UART_DEV, &uart_init);
  USART_Cmd(UART_DEV, ENABLE);

  // the USB controllers must not be touched unclocked (RM 3.3.5.2): stop here, tusb_init() follows
  if (!usbhs_clock_init()) {
    const char msg[] = "USBHS PLL failed to lock\r\n";
    const int len = (int) sizeof(msg) - 1;
    for (int sent = 0; sent < len;) {
      sent += board_uart_write(msg + sent, len - sent);
    }
    while (1) {}
  }

  __enable_irq();
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

__attribute__((interrupt)) void SysTick_Handler(void) {
  SysTick->SR = 0;
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+
void board_led_write(bool state) {
  GPIO_WriteBit(LED_PORT, LED_PIN, state == LED_STATE_ON ? Bit_SET : Bit_RESET);
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN);
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  volatile uint32_t const *esig_uid = (volatile uint32_t const *) 0x1FFFF7E8UL;
  uint32_t uid[3] = {esig_uid[0], esig_uid[1], esig_uid[2]};
  const size_t len = tu_min32(max_len, sizeof(uid));
  memcpy(id, uid, len);
  return len;
}

int board_uart_read(uint8_t *buf, int len) {
  int count = 0;
  while (count < len && USART_GetFlagStatus(UART_DEV, USART_FLAG_RXNE) == SET) {
    buf[count++] = (uint8_t) USART_ReceiveData(UART_DEV);
  }
  return count;
}

int board_uart_write(void const *buf, int len) {
  uint8_t const *p = (uint8_t const *) buf;
  int count = 0;
  while (count < len && USART_GetFlagStatus(UART_DEV, USART_FLAG_TXE) == SET) {
    USART_SendData(UART_DEV, p[count++]);
  }
  return count;
}
