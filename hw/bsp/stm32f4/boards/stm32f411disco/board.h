/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020, Ha Thach (tinyusb.org)
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
   name: STM32 F411 Discovery
   url: https://www.st.com/en/evaluation-tools/32f411ediscovery.html
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define UART_DEV              USART2

#define PINID_LED      0
#define PINID_BUTTON   1
#define PINID_UART_TX  2
#define PINID_UART_RX  3
#define PINID_VBUS0_EN 4

static board_pindef_t board_pindef[] = {
  { // LED
    .port = GPIOD,
    .pin_init = { .Pin = GPIO_PIN_13, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_HIGH, .Alternate = 0 },
    .active_state = 1
  },
  { // BUTTON
    .port = GPIOA,
    .pin_init = { .Pin = GPIO_PIN_0, .Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_HIGH, .Alternate = 0 },
    .active_state = 1
  },
  { // UART TX
    .port = GPIOA,
    .pin_init = { .Pin = GPIO_PIN_2, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_HIGH, .Alternate = GPIO_AF7_USART2 },
    .active_state = 0
  },
  { // UART RX
    .port = GPIOA,
    .pin_init = { .Pin = GPIO_PIN_3, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_HIGH, .Alternate = GPIO_AF7_USART2 },
    .active_state = 0
  },
  { // VBUS0 EN
    .port = GPIOC,
    .pin_init = { .Pin = GPIO_PIN_0, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_HIGH, .Alternate = 0 },
    .active_state = 0
  }
};

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void board_clock_init(void) {
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = HSE_VALUE/1000000;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

  // Enable clocks for UART
  __HAL_RCC_USART2_CLK_ENABLE();
}

static inline void board_vbus_sense_init(uint8_t rhport) {
  // Enable VBUS sense (B device) via pin PA9
  if (rhport == 0) {
    USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_NOVBUSSENS;
    USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBUSBSEN;
  }
}

static inline void board_vbus_set(uint8_t rhport, bool state) {
  if (rhport == 0) {
    board_pindef_t* pindef = &board_pindef[PINID_VBUS0_EN];
    HAL_GPIO_WritePin(pindef->port, pindef->pin_init.Pin, state == pindef->active_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
}

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
