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

#include "stm32f2xx_hal.h"
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
void OTG_FS_IRQHandler(void) {
  tusb_int_handler(0, true);
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

// F2 uses old USART IP (SR/DR)
void USARTn_IRQHandler(void) {
  uint32_t sr = USARTn->SR;
  if (sr & USART_SR_RXNE) {
    uint8_t byte = (uint8_t) USARTn->DR;
    tu_fifo_write(&uart_rx_ff, &byte);
  }
  // Reading DR clears RXNE. OR is cleared by reading SR then DR (already done).
}
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

// enable all LED, Button, Uart, USB clock
static void all_rcc_clk_enable(void) {
  __HAL_RCC_GPIOA_CLK_ENABLE();  // USB D+, D-
  __HAL_RCC_GPIOB_CLK_ENABLE();  // LED
  __HAL_RCC_GPIOC_CLK_ENABLE();  // Button
}

void board_init(void) {
  SystemClock_Config();

  #if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
  #elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;
  #endif

  all_rcc_clk_enable();

  GPIO_InitTypeDef GPIO_InitStruct;

  // LED
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  board_led_write(false);

  // Button
  GPIO_InitStruct.Pin = BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

  /* Configure DM DP Pins */
  GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Configure VBUS Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Configure ID pin */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Enable USB FS Clocks */
  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

#if CFG_TUD_ENABLED
  // Enable VBUS sense (B device) via pin PA9
  tud_configure_dwc2_t cfg = CFG_TUD_CONFIGURE_DWC2_DEFAULT;
  cfg.vbus_sensing = true;
  tud_configure(0, TUD_CFGID_DWC2, &cfg);
#endif

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
    if (UartHandle.Instance->SR & USART_SR_TXE) {
      UartHandle.Instance->DR = p[count];
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
  __asm("BKPT #0\n");
}

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void) {
}
