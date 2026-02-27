/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023, Ha Thach (tinyusb.org)
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
   name: STM32 U5a5 Nucleo
   url: https://www.st.com/en/evaluation-tools/nucleo-u5a5zj-q.html
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32u5xx_ll_tim.h"

// LED GREEN
#define LED_PORT GPIOC
#define LED_PIN GPIO_PIN_7
#define LED_STATE_ON 1

// BUTTON
#define BUTTON_PORT GPIOC
#define BUTTON_PIN GPIO_PIN_13
#define BUTTON_STATE_ACTIVE 1

// UART Enable for STLink VCOM
#define UART_DEV USART1
#define UART_CLK_EN __HAL_RCC_USART1_CLK_ENABLE
#define UART_GPIO_PORT GPIOA
#define UART_GPIO_AF GPIO_AF7_USART1
#define UART_TX_PIN GPIO_PIN_9
#define UART_RX_PIN GPIO_PIN_10

#define VBUS_SENSE_EN 0

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+

static inline void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_EnableVddA();

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = 8;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                | RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);

  // USB Clock
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  RCC_PeriphCLKInitTypeDef usb_clk_init = { 0};
  usb_clk_init.PeriphClockSelection = RCC_PERIPHCLK_USBPHY;
  usb_clk_init.UsbPhyClockSelection = RCC_USBPHYCLKSOURCE_HSE;
  if (HAL_RCCEx_PeriphCLKConfig(&usb_clk_init) != HAL_OK) {
    Error_Handler();
  }

  /** Set the OTG PHY reference clock selection
  */
  HAL_SYSCFG_SetOTGPHYReferenceClockSelection(SYSCFG_OTG_HS_PHY_CLK_SELECT_1);

  // USART clock
  RCC_PeriphCLKInitTypeDef uart_clk_init = { 0};
  uart_clk_init.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  uart_clk_init.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  if (HAL_RCCEx_PeriphCLKConfig(&uart_clk_init) != HAL_OK) {
    Error_Handler();
  }
}

static inline void SystemPower_Config(void) {
  HAL_PWREx_EnableVddIO2();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK) {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

static inline void board_vbus_sense_init(void) {
  /* ADC config */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADCDAC;
  PeriphClkInit.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_HSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_RCC_ADC12_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  ADC_HandleTypeDef hadc1;
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_14B;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T1_TRGO;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) {
    Error_Handler();
  }

  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_68CYCLES;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
    Error_Handler();
  }
  HAL_NVIC_EnableIRQ(ADC1_2_IRQn);

  /* TIM1 init for TRGO */
  __HAL_RCC_TIM1_CLK_ENABLE();
  TIM_HandleTypeDef htim1;
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 159;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  LL_TIM_SetTriggerOutput(htim1.Instance, LL_TIM_TRGO_UPDATE);

  HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
  HAL_ADC_Start_IT(&hadc1);
  HAL_TIM_Base_Start(&htim1);
}

void ADC1_2_IRQHandler(void) {
  if(LL_ADC_IsActiveFlag_EOC(ADC1) != 0) {
    /* Clear flag ADC group regular end of unitary conversion */
    LL_ADC_ClearFlag_EOC(ADC1);
    /* ADC code = 4.5V * R2 / (R1 + R2) * (2^14) / 3.3V
     * with R1 = 330kOhm and R2 = 50kOhm
     */
    const uint32_t threshold = 4500 * 50 / (50 + 330) * 16384 / 3300;
    if((ADC1->DR > threshold) && (USB_OTG_HS->GOTGCTL & USB_OTG_GOTGCTL_BVALOEN)) {
      USB_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
      USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBVALOVAL;
    } else {
      USB_OTG_HS->GOTGCTL &= ~USB_OTG_GOTGCTL_BVALOVAL;
      USB_OTG_HS->GCCFG &= ~USB_OTG_GCCFG_VBVALOVAL;
    }
  }
  if(LL_ADC_IsActiveFlag_OVR(ADC1) != 0) {
    /* Clear flag ADC group regular overrun */
    LL_ADC_ClearFlag_OVR(ADC1);
  }
}
#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
