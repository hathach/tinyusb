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

/* metadata:
   name: STM32 H7S3L8 Nucleo
   url: https://www.st.com/en/evaluation-tools/nucleo-h7s3l8.html
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "tcpp0203.h"
#include "stm32h7rsxx_ll_exti.h"
#include "stm32h7rsxx_ll_system.h"

#define UART_DEV              USART3
#define UART_CLK_EN           __HAL_RCC_USART3_CLK_ENABLE

// VBUS Sense detection
#define OTG_FS_VBUS_SENSE     1
#define OTG_HS_VBUS_SENSE     0

#define PINID_LED      0
#define PINID_BUTTON   1
#define PINID_UART_TX  2
#define PINID_UART_RX  3
#define PINID_TCPP0203_EN  4

static board_pindef_t board_pindef[] = {
  { // LED
    .port = GPIOD,
    .pin_init = { .Pin = GPIO_PIN_10, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0 },
    .active_state = 1
  },
  { // Button
    .port = GPIOC,
    .pin_init = { .Pin = GPIO_PIN_13, .Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0 },
    .active_state = 1
  },
  { // UART TX
    .port = GPIOD,
    .pin_init = { .Pin = GPIO_PIN_8, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = GPIO_AF7_USART3 },
    .active_state = 0
  },
  { // UART RX
    .port = GPIOD,
    .pin_init = { .Pin = GPIO_PIN_9, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = GPIO_AF7_USART3 },
    .active_state = 0
  },
  { // VBUS input pin used for TCPP0203 EN
    .port = GPIOM,
    .pin_init = { .Pin = GPIO_PIN_9, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0 },
    .active_state = 0
  },
  { // I2C SCL for TCPP0203
    .port = GPIOA,
    .pin_init = { .Pin = GPIO_PIN_8, .Mode = GPIO_MODE_AF_OD, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = GPIO_AF4_I2C3 },
  },
  { // I2C SDA for TCPP0203
    .port = GPIOA,
    .pin_init = { .Pin = GPIO_PIN_9, .Mode = GPIO_MODE_AF_OD, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = GPIO_AF4_I2C3 },
  },
  { // INT for TCPP0203
    .port = GPIOM,
    .pin_init = { .Pin = GPIO_PIN_8, .Mode = GPIO_MODE_IT_FALLING, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0 },
  },
};

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_CSI;
  RCC_OscInitStruct.CSIState = RCC_CSI_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL1.PLLM = 12;
  RCC_OscInitStruct.PLL1.PLLN = 300;
  RCC_OscInitStruct.PLL1.PLLP = 1;
  RCC_OscInitStruct.PLL1.PLLQ = 2;
  RCC_OscInitStruct.PLL1.PLLR = 2;
  RCC_OscInitStruct.PLL1.PLLS = 2;
  RCC_OscInitStruct.PLL1.PLLT = 2;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL2.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL2.PLLM = 12;
  RCC_OscInitStruct.PLL2.PLLN = 200;
  RCC_OscInitStruct.PLL2.PLLP = 2;
  RCC_OscInitStruct.PLL2.PLLQ = 2;
  RCC_OscInitStruct.PLL2.PLLR = 2;
  RCC_OscInitStruct.PLL2.PLLS = 2;
  RCC_OscInitStruct.PLL2.PLLT = 2;
  RCC_OscInitStruct.PLL2.PLLFractional = 0;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK4|RCC_CLOCKTYPE_PCLK5;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USBPHYC;
  PeriphClkInit.UsbPhycClockSelection = RCC_USBPHYCCLKSOURCE_HSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_SBS_CLK_ENABLE();
}

//--------------------------------------------------------------------+
// USB PD
//--------------------------------------------------------------------+
static I2C_HandleTypeDef i2c_handle = {
  .Instance = I2C3,
  .Init = {
    .Timing = 0x20C0EDFF,
    .OwnAddress1 = 0,
    .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
    .DualAddressMode = I2C_DUALADDRESS_DISABLE,
    .OwnAddress2 = 0,
    .OwnAddress2Masks = I2C_OA2_NOMASK,
    .GeneralCallMode = I2C_GENERALCALL_DISABLE,
    .NoStretchMode = I2C_NOSTRETCH_DISABLE,
  }
};
static TCPP0203_Object_t tcpp0203_obj = { 0 };

int32_t board_tcpp0203_init(void) {
  board_pindef_t* pindef = &board_pindef[PINID_TCPP0203_EN];
  HAL_GPIO_WritePin(pindef->port, pindef->pin_init.Pin, GPIO_PIN_SET);

  __HAL_RCC_I2C3_CLK_ENABLE();
  __HAL_RCC_I2C3_FORCE_RESET();
  __HAL_RCC_I2C3_RELEASE_RESET();
  if (HAL_I2C_Init(&i2c_handle) != HAL_OK) {
    return HAL_ERROR;
  }

  NVIC_SetPriority(EXTI8_IRQn, 12);
  NVIC_EnableIRQ(EXTI8_IRQn);

  return 0;
}

int32_t board_tcpp0203_deinit(void) {
  return 0;
}

int32_t i2c_readreg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length) {
  TU_ASSERT (HAL_OK == HAL_I2C_Mem_Read(&i2c_handle, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 10000));
  return 0;
}

int32_t i2c_writereg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length) {
  TU_ASSERT(HAL_OK == HAL_I2C_Mem_Write(&i2c_handle, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 10000));
  return 0;
}

static inline void board_init2(void) {
  TCPP0203_IO_t            io_ctx;

  io_ctx.Address     = TCPP0203_I2C_ADDRESS_X68;
  io_ctx.Init        = board_tcpp0203_init;
  io_ctx.DeInit      = board_tcpp0203_deinit;
  io_ctx.ReadReg     = i2c_readreg;
  io_ctx.WriteReg    = i2c_writereg;

  TU_ASSERT(TCPP0203_RegisterBusIO(&tcpp0203_obj, &io_ctx) == TCPP0203_OK, );

  TU_ASSERT(TCPP0203_Init(&tcpp0203_obj) == TCPP0203_OK, );

  TU_ASSERT(TCPP0203_SetPowerMode(&tcpp0203_obj, TCPP0203_POWER_MODE_NORMAL) == TCPP0203_OK, );
}

void board_vbus_set(uint8_t rhport, bool state) {
  (void) state;
  if (rhport == 1) {
    TU_ASSERT(TCPP0203_SetGateDriverProvider(&tcpp0203_obj, TCPP0203_GD_PROVIDER_SWITCH_CLOSED) == TCPP0203_OK, );
  }
}

void EXTI8_IRQHandler(void) {
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_8);
    if (tcpp0203_obj.IsInitialized) {
      TU_ASSERT(TCPP0203_SetPowerMode(&tcpp0203_obj, TCPP0203_POWER_MODE_NORMAL) == TCPP0203_OK, );
      TU_ASSERT(TCPP0203_SetGateDriverProvider(&tcpp0203_obj, TCPP0203_GD_PROVIDER_SWITCH_CLOSED) == TCPP0203_OK, );
    }
}

#ifdef __cplusplus
 }
#endif

#endif
