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
   name: STM32 N6570-DK
   url: https://www.st.com/en/evaluation-tools/stm32n6570-dk.html
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32n657xx.h"
#include "stm32n6xx_ll_exti.h"
#include "stm32n6xx_ll_system.h"
#include "tcpp0203.h"

#define UART_DEV USART1
#define UART_CLK_EN __HAL_RCC_USART1_CLK_ENABLE

#define BOARD_TUD_RHPORT 1

// VBUS Sense detection
#define OTG_FS_VBUS_SENSE 1
#define OTG_HS_VBUS_SENSE 1

#define PINID_LED 0
#define PINID_BUTTON 1
#define PINID_UART_TX 2
#define PINID_UART_RX 3
#define PINID_TCPP0203_EN 4

static board_pindef_t board_pindef[] = {
    {// LED
     .port = GPIOG,
     .pin_init = {.Pin = GPIO_PIN_10, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0},
     .active_state = 1},
    {// Button
     .port = GPIOC,
     .pin_init = {.Pin = GPIO_PIN_13, .Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0},
     .active_state = 1},
    {// UART TX
     .port = GPIOE,
     .pin_init = {.Pin = GPIO_PIN_5, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW, .Alternate = GPIO_AF7_USART1},
     .active_state = 0},
    {// UART RX
     .port = GPIOE,
     .pin_init = {.Pin = GPIO_PIN_6, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW, .Alternate = GPIO_AF7_USART1},
     .active_state = 0},
    {// VBUS input pin used for TCPP0203 EN
     .port = GPIOA,
     .pin_init = {.Pin = GPIO_PIN_4, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0},
     .active_state = 0},
    {
        // I2C SCL for TCPP0203
        .port = GPIOD,
        .pin_init = {.Pin = GPIO_PIN_14, .Mode = GPIO_MODE_AF_OD, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW, .Alternate = GPIO_AF4_I2C2},
    },
    {
        // I2C SDA for TCPP0203
        .port = GPIOD,
        .pin_init = {.Pin = GPIO_PIN_4, .Mode = GPIO_MODE_AF_OD, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW, .Alternate = GPIO_AF4_I2C2},
    },
    {
        // INT for TCPP0203
        .port = GPIOD,
        .pin_init = {.Pin = GPIO_PIN_10, .Mode = GPIO_MODE_IT_FALLING, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0},
    },
};

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  /* Configure the power domain */
  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK) {
    Error_Handler();
  }

  /* Get current CPU/System buses clocks configuration */
  /* and if necessary switch to intermediate HSI clock */
  /* to ensure target clock can be set                 */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
      (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11)) {
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK) {
      Error_Handler();
    }
  }

  /* HSE selected as source (stable clock on Level 0 samples */
  /* PLL1 output = ((HSE/PLLM)*PLLN)/PLLP1/PLLP2             */
  /*             = ((48000000/3)*75)/1/1                     */
  /*             = (16000000*75)/1/1                         */
  /*             = 1200000000 (1200 MHz)                     */
  /* PLL2 off                                                */
  /* PLL3 off                                                */
  /* PLL4 off                                                */

  /* Enable HSE && HSI */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON; /* 48 MHz */

  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL1.PLLM = 3;
  RCC_OscInitStruct.PLL1.PLLN = 75; /* PLL1 VCO = 48/3 * 75 = 1200MHz */
  RCC_OscInitStruct.PLL1.PLLP1 = 1; /* PLL output = PLL VCO frequency / (PLLP1 * PLLP2) */
  RCC_OscInitStruct.PLL1.PLLP2 = 1; /* PLL output = 1200 MHz */
  RCC_OscInitStruct.PLL1.PLLFractional = 0;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    /* Initialization error */
    Error_Handler();
  }

  /* Select PLL1 outputs as CPU and System bus clock source */
  /* CPUCLK = ic1_ck = PLL1 output/ic1_divider = 600 MHz */
  /* SYSCLK = ic2_ck = PLL1 output/ic2_divider = 400 MHz */
  /* Configure the HCLK clock divider */
  /* HCLK =  PLL1 SYSCLK/HCLK divider = 200 MHz */
  /* PCLKx = HCLK / PCLKx divider = 200 MHz */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK4 | RCC_CLOCKTYPE_PCLK5);
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK) {
    /* Initialization Error */
    Error_Handler();
  }

  /** Initializes the peripherals clock
    */
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USBOTGHS1;
  PeriphClkInitStruct.UsbOtgHs1ClockSelection = RCC_USBPHY1REFCLKSOURCE_HSE_DIRECT;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    /* Initialization Error */
    Error_Handler();
  }

  /** Set USB OTG HS PHY1 Reference Clock Source */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USBPHY1;
  PeriphClkInitStruct.UsbPhy1ClockSelection = RCC_USBPHY1REFCLKSOURCE_HSE_DIRECT;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    /* Initialization Error */
    Error_Handler();
  }
}

//--------------------------------------------------------------------+
// USB PD
//--------------------------------------------------------------------+
static I2C_HandleTypeDef i2c_handle = {
    .Instance = I2C2,
    .Init = {
        .Timing = 0x20C0EDFF,
        .OwnAddress1 = 0,
        .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
        .DualAddressMode = I2C_DUALADDRESS_DISABLE,
        .OwnAddress2 = 0,
        .OwnAddress2Masks = I2C_OA2_NOMASK,
        .GeneralCallMode = I2C_GENERALCALL_DISABLE,
        .NoStretchMode = I2C_NOSTRETCH_DISABLE,
    }};
static TCPP0203_Object_t tcpp0203_obj = {0};

int32_t board_tcpp0203_init(void) {
  board_pindef_t *pindef = &board_pindef[PINID_TCPP0203_EN];
  HAL_GPIO_WritePin(pindef->port, pindef->pin_init.Pin, GPIO_PIN_SET);

  __HAL_RCC_I2C2_CLK_ENABLE();
  __HAL_RCC_I2C2_FORCE_RESET();
  __HAL_RCC_I2C2_RELEASE_RESET();
  if (HAL_I2C_Init(&i2c_handle) != HAL_OK) {
    return HAL_ERROR;
  }

  NVIC_SetPriority(EXTI10_IRQn, 12);
  NVIC_EnableIRQ(EXTI10_IRQn);

  return 0;
}

int32_t board_tcpp0203_deinit(void) {
  return 0;
}

int32_t i2c_readreg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length) {
  TU_ASSERT(HAL_OK == HAL_I2C_Mem_Read(&i2c_handle, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 10000));
  return 0;
}

int32_t i2c_writereg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length) {
  TU_ASSERT(HAL_OK == HAL_I2C_Mem_Write(&i2c_handle, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 10000));
  return 0;
}

static inline void board_init2(void) {
  TCPP0203_IO_t io_ctx;

  io_ctx.Address = TCPP0203_I2C_ADDRESS_X68;
  io_ctx.Init = board_tcpp0203_init;
  io_ctx.DeInit = board_tcpp0203_deinit;
  io_ctx.ReadReg = i2c_readreg;
  io_ctx.WriteReg = i2c_writereg;

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

void EXTI10_IRQHandler(void) {
  __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_10);
  if (tcpp0203_obj.IsInitialized) {
    TU_ASSERT(TCPP0203_SetPowerMode(&tcpp0203_obj, TCPP0203_POWER_MODE_NORMAL) == TCPP0203_OK, );
    TU_ASSERT(TCPP0203_SetGateDriverProvider(&tcpp0203_obj, TCPP0203_GD_PROVIDER_SWITCH_CLOSED) == TCPP0203_OK, );
  }
}

#ifdef __cplusplus
}
#endif

#endif
