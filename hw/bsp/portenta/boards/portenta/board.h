/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stm32h7xx_hal_rtc_ex.h>

#define LED_PORT              GPIOK
#define LED_PIN               GPIO_PIN_7
#define LED_STATE_ON          1

// Tamper push-button
#define BUTTON_PORT           GPIOC
#define BUTTON_PIN            GPIO_PIN_3
#define BUTTON_STATE_ACTIVE   0

// Need to change jumper setting J7 and J8 from RS-232 to STLink
#define UART_DEV              USART1
#define UART_CLK_EN           __HAL_RCC_USART1_CLK_ENABLE
#define UART_GPIO_PORT        GPIOA
#define UART_GPIO_AF          GPIO_AF4_USART1
#define UART_TX_PIN           GPIO_PIN_9
#define UART_RX_PIN           GPIO_PIN_10

// VBUS Sense detection
#define OTG_FS_VBUS_SENSE     1
#define OTG_HS_VBUS_SENSE     0

// USB HS External PHY Pin: CLK, STP, DIR, NXT, D0-D7
#define ULPI_PINS \
  {GPIOA, GPIO_PIN_3 }, {GPIOA, GPIO_PIN_5 }, {GPIOB, GPIO_PIN_0 }, {GPIOB, GPIO_PIN_1 }, \
  {GPIOB, GPIO_PIN_5 }, {GPIOB, GPIO_PIN_10}, {GPIOB, GPIO_PIN_11}, {GPIOB, GPIO_PIN_12}, \
  {GPIOB, GPIO_PIN_13}, {GPIOC, GPIO_PIN_0 }, {GPIOI, GPIO_PIN_11 }, {GPIOH, GPIO_PIN_4 }

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void board_stm32h7_clock_init(void)
{
  RCC_OscInitTypeDef       RCC_OscInitStruct = {};
  RCC_ClkInitTypeDef       RCC_ClkInitStruct = {};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {};

  /* Supply configuration update enable */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  /* Configure the main internal regulator output voltage */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0); // 480MHz

  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /* Macro to configure the PLL clock source */
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
  /* Initializes the CPU, AHB and APB busses clocks */
  RCC_OscInitStruct.OscillatorType
    = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 16;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  /* Initializes the CPU, AHB and APB busses clocks */
  RCC_ClkInitStruct.ClockType
    = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1
      | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
  
  PeriphClkInitStruct.PLL2.PLL2M = 5;
  PeriphClkInitStruct.PLL2.PLL2N = 80;  
  PeriphClkInitStruct.PLL2.PLL2P = 2;  
  PeriphClkInitStruct.PLL2.PLL2Q = 2; 
  PeriphClkInitStruct.PLL2.PLL2R = 2; 
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  // PLL 3
  PeriphClkInitStruct.PLL3.PLL3M = 5;
  PeriphClkInitStruct.PLL3.PLL3N = 160;
  PeriphClkInitStruct.PLL3.PLL3P = 2; 
  PeriphClkInitStruct.PLL3.PLL3Q = 5;
  PeriphClkInitStruct.PLL3.PLL3R = 2; 
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_PLL2;
  PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
  PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
  PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
  PeriphClkInitStruct.Sai23ClockSelection = RCC_SAI23CLKSOURCE_PLL3;
  PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;
  PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
  PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
  PeriphClkInitStruct.I2c123ClockSelection = RCC_I2C123CLKSOURCE_D2PCLK1;
  PeriphClkInitStruct.I2c4ClockSelection = RCC_I2C4CLKSOURCE_PLL3;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL3;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

}

static inline void reboot1200bps(void)
{
  RTC_HandleTypeDef RTCHandle;
  RTCHandle.Instance = RTC;
  HAL_RTCEx_BKUPWrite(&RTCHandle, RTC_BKP_DR0, 0xDF59);
  NVIC_SystemReset();
}

static inline void board_stm32h7_post_init(void)
{
  // For this board does nothing
}

#ifdef __cplusplus
 }
#endif

#endif
