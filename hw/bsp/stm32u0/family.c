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

#include "stm32u0xx_hal.h"
#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_DRD_FS_IRQHandler(void) {
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#ifdef UART_DEV
UART_HandleTypeDef UartHandle;
#endif

void board_init(void) {
  board_stm32u0_clock_init();

  // Enable All GPIOs clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
#ifdef GPIOD
  __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
#ifdef GPIOE
  __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
#ifdef GPIOF
  __HAL_RCC_GPIOF_CLK_ENABLE();
#endif
#ifdef GPIOG
  __HAL_RCC_GPIOG_CLK_ENABLE();
#endif
#ifdef GPIOH
  __HAL_RCC_GPIOH_CLK_ENABLE();
#endif
  __HAL_RCC_PWR_CLK_ENABLE();

  // LED
  GPIO_InitTypeDef  GPIO_InitStruct;
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  // Button
  GPIO_InitStruct.Pin = BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = BUTTON_STATE_ACTIVE ? GPIO_PULLDOWN : GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

#ifdef UART_DEV
  // UART
  GPIO_InitStruct.Pin       = UART_TX_PIN | UART_RX_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = UART_GPIO_AF;
  HAL_GPIO_Init(UART_GPIO_PORT, &GPIO_InitStruct);

  UART_CLK_EN();
  UartHandle.Instance        = UART_DEV;
  UartHandle.Init.BaudRate   = CFG_BOARD_UART_BAUDRATE;
  UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
  UartHandle.Init.StopBits   = UART_STOPBITS_1;
  UartHandle.Init.Parity     = UART_PARITY_NONE;
  UartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
  UartHandle.Init.Mode       = UART_MODE_TX_RX;
  UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&UartHandle);
#endif

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_DRD_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  // USB Pins TODO double check USB clock and pin setup
  // Configure USB DM and DP pins. This is optional, and maintained only for user guidance.
  GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_USB;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Enable VDDUSB
  HAL_PWREx_EnableVddUSB();
  // USB Clock enable
  __HAL_RCC_USB_CLK_ENABLE();

  board_vbus_sense_init();
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  GPIO_PinState pin_state = (GPIO_PinState) (state ? LED_STATE_ON : (1-LED_STATE_ON));
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, pin_state);
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len) {
#ifdef UART_DEV
  (void) buf; (void) len;
  return 0;
#else
  return 0;
#endif
}

int board_uart_write(void const * buf, int len) {
#ifdef UART_DEV
  HAL_UART_Transmit(&UartHandle, (uint8_t*)(uintptr_t) buf, len, 0xffff);
  return len;
#else
  (void) buf; (void) len;
  return 0;
#endif
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t board_millis(void) {
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
