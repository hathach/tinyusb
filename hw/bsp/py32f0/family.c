/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
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
   manufacturer: Puya
*/

#include "py32f0xx_hal.h"
#include "bsp/board_api.h"
#include "board.h"

void USB_IRQHandler(void) {
  tud_int_handler(0);
}

void USBD_IRQHandler(void) {
  tud_int_handler(0);
}

void HAL_MspInit(void) {
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
}

void board_init(void) {
  HAL_Init();
  board_py32f0_clock_init();

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  SysTick->CTRL &= ~1U;
  NVIC_SetPriority(USB_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  GPIO_InitTypeDef GPIO_InitStruct;

  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
  board_led_write(false);

  GPIO_InitStruct.Pin = BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

  __HAL_RCC_USB_CLK_ENABLE();
}

void board_led_write(bool state) {
  GPIO_PinState pin_state = (GPIO_PinState) (state ? LED_STATE_ON : (1 - LED_STATE_ON));
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, pin_state);
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void) max_len;
  volatile uint32_t * py32_uuid = (volatile uint32_t *) UID_BASE;
  uint32_t* id32 = (uint32_t*) (uintptr_t) id;
  uint8_t const len = 12;

  id32[0] = py32_uuid[0];
  id32[1] = py32_uuid[1];
  id32[2] = py32_uuid[2];

  return len;
}

int board_uart_read(uint8_t *buf, int len) {
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const *buf, int len) {
  (void) buf; (void) len;
  return -1;
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

void _init(void);
void _init(void) {
}
