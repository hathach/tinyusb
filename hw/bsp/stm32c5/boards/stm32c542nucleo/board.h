/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, HiFiPhile
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
   name: STM32C542 Nucleo
   url: https://www.st.com/en/evaluation-tools/nucleo-c542rc.html
*/

#ifndef BOARD_H_
#define BOARD_H_

// LED
#define LED_PORT         HAL_GPIOA
#define LED_PIN          HAL_GPIO_PIN_5

// Button
#define BUTTON_PORT           HAL_GPIOC
#define BUTTON_PIN            HAL_GPIO_PIN_13
#define BUTTON_STATE_ACTIVE   1

// Enable UART serial communication with the ST-Link
#define UART_ID               2
#define UART_GPIO_PORT        HAL_GPIOA
#define UART_GPIO_AF          HAL_GPIO_AF_7
#define UART_TX_PIN           HAL_GPIO_PIN_2
#define UART_RX_PIN           HAL_GPIO_PIN_3

static inline void board_clock_init(void) {
  HAL_RCC_HSE_Enable(HAL_RCC_HSE_ON);

  hal_rcc_psi_config_t config_psi;
  config_psi.psi_source = HAL_RCC_PSI_SRC_HSE;
  config_psi.psi_ref = HAL_RCC_PSI_REF_24MHZ;
  config_psi.psi_out = HAL_RCC_PSI_OUT_144MHZ;
  HAL_RCC_PSI_SetConfig(&config_psi);
  HAL_RCC_PSIS_Enable();

  /** Initializes the CPU, AHB and APB busses clocks */
  hal_rcc_bus_clk_config_t config_bus;
  config_bus.hclk_prescaler  = HAL_RCC_HCLK_PRESCALER1;
  config_bus.pclk1_prescaler = HAL_RCC_PCLK_PRESCALER1;
  config_bus.pclk2_prescaler = HAL_RCC_PCLK_PRESCALER1;
  config_bus.pclk3_prescaler = HAL_RCC_PCLK_PRESCALER1;
  HAL_RCC_SetBusClockConfig(&config_bus);

  /** Frequency will be increased */
  HAL_FLASH_ITF_SetLatency(HAL_FLASH, HAL_FLASH_ITF_LATENCY_4);

  HAL_RCC_SetSYSCLKSource(HAL_RCC_SYSCLK_SRC_PSIS);

  HAL_FLASH_ITF_SetProgrammingDelay(HAL_FLASH, HAL_FLASH_ITF_PROGRAM_DELAY_2);

  HAL_UpdateCoreClock();

  HAL_RCC_USART2_SetKernelClkSource(HAL_RCC_USART2_CLK_SRC_PCLK1);

  HAL_RCC_PSIDIV3_Enable();
  HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_PSIDIV3);
}
#endif /* BOARD_H_ */
