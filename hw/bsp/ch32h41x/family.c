/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, TinyUSB contributors
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
   manufacturer: WCH
*/

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "ch32h417_usart.h"

#include "bsp/board_api.h"
#include "board.h"

// Keep interrupt entry points distinct: IPA/LTO can fold identical handlers into
// calls to another interrupt function, whose mret would bypass this frame.
#define CH32H41X_IT_ATTR __attribute__((interrupt("machine"), used, noipa))

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
CH32H41X_IT_ATTR void USBSS_IRQHandler(void);
CH32H41X_IT_ATTR void USBSS_LINK_IRQHandler(void);
CH32H41X_IT_ATTR void SysTick0_Handler(void);

CH32H41X_IT_ATTR void USBSS_IRQHandler(void) {
  tud_int_handler(0);
}

CH32H41X_IT_ATTR void USBSS_LINK_IRQHandler(void) {
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

static bool board_uart_ready;

static void uart_init(void) {
  GPIO_InitTypeDef gpio_init = { 0 };
  USART_InitTypeDef usart_init = { 0 };

  RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_USART1 | RCC_HB2Periph_GPIOA, ENABLE);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF7);

  gpio_init.GPIO_Pin = GPIO_Pin_9;
  gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
  gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &gpio_init);

  usart_init.USART_BaudRate = CFG_BOARD_UART_BAUDRATE;
  usart_init.USART_WordLength = USART_WordLength_8b;
  usart_init.USART_StopBits = USART_StopBits_1;
  usart_init.USART_Parity = USART_Parity_No;
  usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  usart_init.USART_Mode = USART_Mode_Tx;
  USART_Init(USART1, &usart_init);
  USART_Cmd(USART1, ENABLE);

  board_uart_ready = true;
}

#if CFG_TUSB_OS == OPT_OS_NONE
static uint32_t systick_config(uint32_t ticks) {
  NVIC_EnableIRQ(SysTick0_IRQn);
  SysTick0->CTLR = 0;
  SysTick0->ISR = 0;
  SysTick0->CNT = 0;
  SysTick0->CMP = ticks - 1;
  SysTick0->CTLR = 0x0F;
  return 0;
}
#endif

void board_init(void) {
  __disable_irq();

  SystemInit();
  SystemAndCoreClockUpdate();

#if CFG_TUSB_DEBUG || (defined(CFG_TUD_WCH_USBSS_DEBUG) && CFG_TUD_WCH_USBSS_DEBUG)
  uart_init();
#endif

#if CFG_TUSB_OS == OPT_OS_NONE
  systick_config(SystemCoreClock / 1000);
#endif

  __enable_irq();
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

CH32H41X_IT_ATTR void SysTick0_Handler(void) {
  SysTick0->ISR = 0;
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
  (void) state;
}

uint32_t board_button_read(void) {
  return 0;
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const* buf, int len) {
  if (!board_uart_ready) {
    return -1;
  }

  uint8_t const *p = (uint8_t const *) buf;

  for (int i = 0; i < len; i++) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
    }
    USART_SendData(USART1, p[i]);
  }

  return len;
}
