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

#include "mfxstm32l152.h"

#define LED_PORT              GPIOA
#define LED_PIN               GPIO_PIN_4
#define LED_STATE_ON          1

// Tamper push-button
#define BUTTON_PORT           GPIOC
#define BUTTON_PIN            GPIO_PIN_13
#define BUTTON_STATE_ACTIVE   0

// Need to change jumper setting J7 and J8 from RS-232 to STLink
#define UART_DEV              USART1
#define UART_CLK_EN           __HAL_RCC_USART1_CLK_ENABLE
#define UART_GPIO_PORT        GPIOB
#define UART_GPIO_AF          GPIO_AF4_USART1
#define UART_TX_PIN           GPIO_PIN_14
#define UART_RX_PIN           GPIO_PIN_15

// VBUS Sense detection
#define OTG_FS_VBUS_SENSE     1
#define OTG_HS_VBUS_SENSE     0

// USB HS External PHY Pin: CLK, STP, DIR, NXT, D0-D7
#define ULPI_PINS \
  {GPIOA, GPIO_PIN_3 }, {GPIOA, GPIO_PIN_5 }, {GPIOB, GPIO_PIN_0 }, {GPIOB, GPIO_PIN_1 }, \
  {GPIOB, GPIO_PIN_5 }, {GPIOB, GPIO_PIN_10}, {GPIOB, GPIO_PIN_11}, {GPIOB, GPIO_PIN_12}, \
  {GPIOB, GPIO_PIN_13}, {GPIOC, GPIO_PIN_0 }, {GPIOH, GPIO_PIN_4 }, {GPIOI, GPIO_PIN_11}

/* Definition for I2C1 Pins */
#define BUS_I2C1_SCL_PIN                       GPIO_PIN_6
#define BUS_I2C1_SDA_PIN                       GPIO_PIN_7
#define BUS_I2C1_SCL_GPIO_PORT                 GPIOB
#define BUS_I2C1_SDA_GPIO_PORT                 GPIOB
#define BUS_I2C1_SCL_AF                        GPIO_AF4_I2C1
#define BUS_I2C1_SDA_AF                        GPIO_AF4_I2C1

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void SystemClock_Config(void) {
  RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
  RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = { 0 };

  /*!< Supply configuration update enable */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while ( (PWR->D3CR & (PWR_D3CR_VOSRDY)) != PWR_D3CR_VOSRDY ) {}

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

  // PLL1 for System Clock (400Mhz)
  // From H743 eval manual ETM can only work at 50 MHz clock by default because ETM signals
  // are shared with other peripherals. Trace CLK = PLL1R.
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 6; // Trace clock is 400/6 = 66.67 MHz (larger than 50 MHz but work well)
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOMEDIUM;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);


  /* Select PLL as system clock source and configure bus clocks dividers */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_D3PCLK1;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

  /* PLL3 for USB Clock */
  PeriphClkInitStruct.PLL3.PLL3M = 25;
  PeriphClkInitStruct.PLL3.PLL3N = 336;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 7;
  PeriphClkInitStruct.PLL3.PLL3R = 2;

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_PLL3;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

  /*activate CSI clock mondatory for I/O Compensation Cell*/
  __HAL_RCC_CSI_ENABLE();

  /* Enable SYSCFG clock mondatory for I/O Compensation Cell */
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  /* Enables the I/O Compensation Cell */
  HAL_EnableCompensationCell();
}

//--------------------------------------------------------------------+
// MFX
//--------------------------------------------------------------------+
I2C_HandleTypeDef hbus_i2c1 = { .Instance = I2C1};
static MFXSTM32L152_Object_t  mfx_obj = { 0 };
static MFXSTM32L152_IO_Mode_t* mfx_io_drv = NULL;

HAL_StatusTypeDef MX_I2C1_Init(I2C_HandleTypeDef* hI2c, uint32_t timing) {
  hI2c->Init.Timing = timing;
  hI2c->Init.OwnAddress1 = 0;
  hI2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hI2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hI2c->Init.OwnAddress2 = 0;
  hI2c->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hI2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hI2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(hI2c) != HAL_OK) {
    return HAL_ERROR;
  }
  if (HAL_I2CEx_ConfigAnalogFilter(hI2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    return HAL_ERROR;
  }
  if (HAL_I2CEx_ConfigDigitalFilter(hI2c, 0) != HAL_OK) {
    return HAL_ERROR;
  }

  return HAL_OK;
}

int32_t BSP_I2C1_Init(void) {
  // Init I2C
  GPIO_InitTypeDef  gpio_init_structure;
  gpio_init_structure.Pin       = BUS_I2C1_SCL_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_AF_OD;
  gpio_init_structure.Pull      = GPIO_NOPULL;
  gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio_init_structure.Alternate = BUS_I2C1_SCL_AF;
  HAL_GPIO_Init(BUS_I2C1_SCL_GPIO_PORT, &gpio_init_structure);

  gpio_init_structure.Pin       = BUS_I2C1_SDA_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_AF_OD;
  gpio_init_structure.Pull      = GPIO_NOPULL;
  gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio_init_structure.Alternate = BUS_I2C1_SDA_AF;
  HAL_GPIO_Init(BUS_I2C1_SDA_GPIO_PORT, &gpio_init_structure);

  __HAL_RCC_I2C1_CLK_ENABLE();
  __HAL_RCC_I2C1_FORCE_RESET();
  __HAL_RCC_I2C1_RELEASE_RESET();

  if (MX_I2C1_Init(&hbus_i2c1, /*0x10C0ECFF*/ 1890596921) != HAL_OK) {
    return -1;
  }

  return 0;
}

int32_t BSP_I2C1_DeInit(void) {
  return 0;
}

int32_t BSP_I2C1_ReadReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length) {
  if (HAL_OK != HAL_I2C_Mem_Read(&hbus_i2c1, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 10000)) {
    return -1;
  }
  return 0;
}

int32_t BSP_I2C1_WriteReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length) {
  if(HAL_OK != HAL_I2C_Mem_Write(&hbus_i2c1, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 10000)) {
    return -1;
  }
  return 0;
}


static inline void board_init2(void) {
  // Init MFX IO expanding for vbus drive
  BSP_I2C1_Init();

  /* Configure the audio driver */
  MFXSTM32L152_IO_t IOCtx;
  IOCtx.Init        = BSP_I2C1_DeInit;
  IOCtx.DeInit      = BSP_I2C1_DeInit;
  IOCtx.ReadReg     = BSP_I2C1_ReadReg;
  IOCtx.WriteReg    = BSP_I2C1_WriteReg;
  IOCtx.GetTick     = (MFXSTM32L152_GetTick_Func) HAL_GetTick;

  uint8_t i2c_address[] = {0x84, 0x86};
  for(uint8_t i = 0U; i < 2U; i++) {
    uint32_t mfx_id;
    IOCtx.Address = (uint16_t)i2c_address[i];
    if (MFXSTM32L152_RegisterBusIO(&mfx_obj, &IOCtx) != MFXSTM32L152_OK) {
      return;
    }
    if (MFXSTM32L152_ReadID(&mfx_obj, &mfx_id) != MFXSTM32L152_OK) {
      return;
    }

    if ((mfx_id == MFXSTM32L152_ID) || (mfx_id == MFXSTM32L152_ID_2)) {
      if (MFXSTM32L152_Init(&mfx_obj) != MFXSTM32L152_OK) {
        return;
      }
      break;
    }
  }

  mfx_io_drv = &MFXSTM32L152_IO_Driver;

  static MFXSTM32L152_IO_Init_t io_init = { 0 };
  mfx_io_drv->Init(&mfx_obj, &io_init);

  io_init.Pin = MFXSTM32L152_GPIO_PIN_7;
  io_init.Mode = MFXSTM32L152_GPIO_MODE_OUTPUT_PP;
  io_init.Pull = MFXSTM32L152_GPIO_PULLUP;
  mfx_io_drv->Init(&mfx_obj, &io_init); // VBUS[0]

  io_init.Pin = MFXSTM32L152_GPIO_PIN_9;
  mfx_io_drv->Init(&mfx_obj, &io_init); // VBUS[1]

#if 1 // write then read IO7 but it does not seems to change value
  int32_t pin_value;
  pin_value = mfx_io_drv->IO_ReadPin(&mfx_obj, MFXSTM32L152_GPIO_PIN_7);
  TU_LOG1_INT(pin_value);

  mfx_io_drv->IO_WritePin(&mfx_obj, MFXSTM32L152_GPIO_PIN_7, 1);

  pin_value = mfx_io_drv->IO_ReadPin(&mfx_obj, MFXSTM32L152_GPIO_PIN_7);
  TU_LOG1_INT(pin_value);
#endif
}

// vbus drive
void board_vbus_set(uint8_t rhport, bool state) {
  if ( mfx_io_drv ) {
    uint32_t io_pin = (_rhport) ? MFXSTM32L152_GPIO_PIN_9 : MFXSTM32L152_GPIO_PIN_7;
    mfx_io_drv->IO_WritePin(&Io_CompObj, io_pin, _on);
  }
}

#ifdef __cplusplus
 }
#endif

#endif
