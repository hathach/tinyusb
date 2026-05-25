/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
   manufacturer: STMicroelectronics
*/

#include "stm32f3xx_hal.h"
#include "bsp/board_api.h"
#include "common/tusb_fifo.h"
#include "board.h"

#ifdef UART_ID
  #if UART_ID == 1
    #define USARTn            USART1
    #define USARTn_IRQn       USART1_IRQn
    #define USARTn_IRQHandler USART1_IRQHandler
    #define UARTn_CLK_ENABLE  __HAL_RCC_USART1_CLK_ENABLE
  #elif UART_ID == 2
    #define USARTn            USART2
    #define USARTn_IRQn       USART2_IRQn
    #define USARTn_IRQHandler USART2_IRQHandler
    #define UARTn_CLK_ENABLE  __HAL_RCC_USART2_CLK_ENABLE
  #elif UART_ID == 3
    #define USARTn            USART3
    #define USARTn_IRQn       USART3_IRQn
    #define USARTn_IRQHandler USART3_IRQHandler
    #define UARTn_CLK_ENABLE  __HAL_RCC_USART3_CLK_ENABLE
  #endif
#endif

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// USB defaults to using interrupts 19, 20 and 42, however, this BSP sets the
// SYSCFG_CFGR1.USB_IT_RMP bit remapping interrupts to 74, 75 and 76.

// FIXME: Do all three need to be handled, or just the LP one?
// USB high-priority interrupt (Channel 74): Triggered only by a correct
// transfer event for isochronous and double-buffer bulk transfer to reach
// the highest possible transfer rate.
void USB_HP_IRQHandler(void) {
  tud_int_handler(0);
}

// USB low-priority interrupt (Channel 75): Triggered by all USB events
// (Correct transfer, USB reset, etc.). The firmware has to check the
// interrupt source before serving the interrupt.
void USB_LP_IRQHandler(void) {
  tud_int_handler(0);
}

// USB wakeup interrupt (Channel 76): Triggered by the wakeup event from the USB
// Suspend mode.
void USBWakeUp_RMP_IRQHandler(void) {
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#ifdef UART_ID
static UART_HandleTypeDef UartHandle = {
  .Instance = USARTn,
  .Init = {
    .BaudRate     = CFG_BOARD_UART_BAUDRATE,
    .WordLength   = UART_WORDLENGTH_8B,
    .StopBits     = UART_STOPBITS_1,
    .Parity       = UART_PARITY_NONE,
    .HwFlowCtl    = UART_HWCONTROL_NONE,
    .Mode         = UART_MODE_TX_RX,
    .OverSampling = UART_OVERSAMPLING_16,
  }
};

// RX ring buffer via RXNE interrupt
static uint8_t   uart_rx_ff_buf[32];
static tu_fifo_t uart_rx_ff;

// F3 uses new USART IP (ISR/RDR/TDR/ICR) — same as F7
void USARTn_IRQHandler(void) {
  uint32_t isr = USARTn->ISR;
  if (isr & USART_ISR_RXNE) {
    uint8_t byte = (uint8_t) USARTn->RDR;
    tu_fifo_write(&uart_rx_ff, &byte);
  }
  if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) {
    USARTn->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
  }
}
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+


void board_init(void) {
  SystemClock_Config();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;
#endif

  // Remap the USB interrupts
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_REMAPINTERRUPT_USB_ENABLE();

  // LED
  __HAL_RCC_GPIOE_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  // Button
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitStruct.Pin = BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

  /* Configure USB DM and DP pins */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_USB;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Enable USB clock
  __HAL_RCC_USB_CLK_ENABLE();

#ifdef UART_ID
  HAL_UART_Init(&UartHandle);
  tu_fifo_config(&uart_rx_ff, uart_rx_ff_buf, sizeof(uart_rx_ff_buf), false);
  USARTn->CR1 |= USART_CR1_RXNEIE;
  NVIC_SetPriority(USARTn_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
  NVIC_EnableIRQ(USARTn_IRQn);
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  GPIO_PinState pin_state = (GPIO_PinState) (state ? LED_STATE_ON : (1 - LED_STATE_ON));
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, pin_state);
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void) max_len;
  volatile uint32_t * stm32_uuid = (volatile uint32_t *) UID_BASE;
  uint32_t* id32 = (uint32_t*) (uintptr_t) id;
  uint8_t const len = 12;

  id32[0] = stm32_uuid[0];
  id32[1] = stm32_uuid[1];
  id32[2] = stm32_uuid[2];

  return len;
}

int board_uart_read(uint8_t* buf, int len) {
#ifdef UART_ID
  return (int) tu_fifo_read_n(&uart_rx_ff, buf, (uint16_t) len);
#else
  (void) buf; (void) len;
  return 0;
#endif
}

int board_uart_write(void const* buf, int len) {
#ifdef UART_ID
  const uint8_t *p = (const uint8_t *) buf;
  int count = 0;
  while (count < len) {
    if (__HAL_UART_GET_FLAG(&UartHandle, UART_FLAG_TXE)) {
      UartHandle.Instance->TDR = p[count];
      count++;
    } else {
      break;
    }
  }
  return count;
#else
  (void) buf; (void) len;
  return -1;
#endif
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  HAL_IncTick();
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}

#endif

void HardFault_Handler(void) {
  asm("bkpt 0");
}

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void) {

}
