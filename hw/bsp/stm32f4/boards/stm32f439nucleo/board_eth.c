/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Andrew Leech
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
 */

/*
 * Board-specific Ethernet MspInit for the STM32F439ZI / F429ZI Nucleo-144.
 *
 * Compiled into the example only when family_add_eth() is called from
 * the example's CMakeLists. HAL_ETH_Init() invokes HAL_ETH_MspInit()
 * (a weak symbol) automatically; the example's lwIP netif glue does not
 * need to know about the pin map.
 *
 * RMII pin map (LAN8742A, MII/RMII select via SYSCFG_PMC):
 *   RMII_REF_CLK  - PA1
 *   RMII_MDIO     - PA2
 *   RMII_MDC      - PC1
 *   RMII_CRS_DV   - PA7
 *   RMII_RXD0     - PC4
 *   RMII_RXD1     - PC5
 *   RMII_TX_EN    - PG11
 *   RMII_TXD0     - PG13
 *   RMII_TXD1     - PB13
 */

#include "stm32f4xx_hal.h"

#ifdef HAL_ETH_MODULE_ENABLED

void HAL_ETH_MspInit(ETH_HandleTypeDef *heth) {
  (void) heth;
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  gpio.Speed     = GPIO_SPEED_HIGH;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Alternate = GPIO_AF11_ETH;

  // Port A: PA1 (REF_CLK), PA2 (MDIO), PA7 (CRS_DV)
  gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
  HAL_GPIO_Init(GPIOA, &gpio);

  // Port B: PB13 (TXD1)
  gpio.Pin = GPIO_PIN_13;
  HAL_GPIO_Init(GPIOB, &gpio);

  // Port C: PC1 (MDC), PC4 (RXD0), PC5 (RXD1)
  gpio.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
  HAL_GPIO_Init(GPIOC, &gpio);

  // Port G: PG11 (TX_EN), PG13 (TXD0)
  gpio.Pin = GPIO_PIN_11 | GPIO_PIN_13;
  HAL_GPIO_Init(GPIOG, &gpio);

  // SYSCFG (for SYSCFG->PMC MII/RMII select) and the ETH peripheral
  // clock. HAL_ETH_Init's MII/RMII bit write into SYSCFG->PMC requires
  // the SYSCFG clock to already be enabled.
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_ETH_CLK_ENABLE();
}

#endif // HAL_ETH_MODULE_ENABLED
