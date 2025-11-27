/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020, Ha Thach (tinyusb.org)
 * Copyright (c) 2023, HiFiPhile
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
   name: STM32 H573i Discovery
   url: https://www.st.com/en/evaluation-tools/stm32h573i-dk.html
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tcpp0203.h"

// VBUS Sense detection
#define OTG_FS_VBUS_SENSE     1
#define OTG_HS_VBUS_SENSE     0

#define PINID_LED          0
#define PINID_BUTTON       1
#define PINID_UART_TX      2
#define PINID_UART_RX      3
#define PINID_TCPP0203_EN  4
#define PINID_I2C_SCL      5
#define PINID_I2C_SDA      6
#define PINID_TCPP0203_INT 7

static board_pindef_t board_pindef[] = {
  { // LED
    .port = GPIOI,
    .pin_init = { .Pin = GPIO_PIN_9, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0 },
    .active_state = 1
  },
  { // Button
    .port = GPIOC,
    .pin_init = { .Pin = GPIO_PIN_13, .Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0 },
    .active_state = 1
  },
  { // UART TX
    .port = GPIOA,
    .pin_init = { .Pin = GPIO_PIN_9, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = GPIO_AF7_USART1 },
    .active_state = 0
  },
  { // UART RX
    .port = GPIOA,
    .pin_init = { .Pin = GPIO_PIN_10, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = GPIO_AF7_USART1 },
    .active_state = 0
  },
  { // TCPP0203 VCC_EN
    .port = GPIOG,
    .pin_init = { .Pin = GPIO_PIN_0, .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW, .Alternate = 0 },
    .active_state = 1
  },
  { // I2C4 SCL
    .port = GPIOB,
    .pin_init = { .Pin = GPIO_PIN_8, .Mode = GPIO_MODE_AF_OD, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = GPIO_AF6_I2C4 },
    .active_state = 0
  },
  { // I2C4 SDA
    .port = GPIOB,
    .pin_init = { .Pin = GPIO_PIN_9, .Mode = GPIO_MODE_AF_OD, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = GPIO_AF6_I2C4 },
    .active_state = 0
  },
  { // TCPP0203 INT
    .port = GPIOG,
    .pin_init = { .Pin = GPIO_PIN_1, .Mode = GPIO_MODE_IT_FALLING, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH, .Alternate = 0 },
    .active_state = 0
  },
};

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  Freq 250MHZ */

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DIGITAL;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 10;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  /** Initializes the CPU, AHB and APB buses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  // Configure CRS clock source
  __HAL_RCC_CRS_CLK_ENABLE();
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
  RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
  RCC_CRSInitStruct.ErrorLimitValue = 34;
  RCC_CRSInitStruct.HSI48CalibrationValue = 32;
  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);

  /* Select HSI48 as USB clock source */
  RCC_PeriphCLKInitTypeDef usb_clk = {0 };
  usb_clk.PeriphClockSelection = RCC_PERIPHCLK_USB;
  usb_clk.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  HAL_RCCEx_PeriphCLKConfig(&usb_clk);

  /* Peripheral clock enable */
  __HAL_RCC_USB_CLK_ENABLE();
}

//--------------------------------------------------------------------+
// USB PD
//--------------------------------------------------------------------+
static I2C_HandleTypeDef i2c_handle = {
  .Instance = I2C4,
  .Init = {
    .Timing = 0x20C0EDFF, // 100kHz @ 250MHz
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
  // Enable TCPP0203 VCC (GPIO already configured in pindef array)
  board_pindef_t* pindef = &board_pindef[PINID_TCPP0203_EN];
  HAL_GPIO_WritePin(pindef->port, pindef->pin_init.Pin, GPIO_PIN_SET);

  // Initialize I2C4 for TCPP0203 (GPIO already configured in pindef array)
  __HAL_RCC_I2C4_CLK_ENABLE();
  __HAL_RCC_I2C4_FORCE_RESET();
  __HAL_RCC_I2C4_RELEASE_RESET();
  if (HAL_I2C_Init(&i2c_handle) != HAL_OK) {
    return HAL_ERROR;
  }

  // Enable interrupt for TCPP0203 FLGn (GPIO already configured in pindef array)
  NVIC_SetPriority(EXTI1_IRQn, 12);
  NVIC_EnableIRQ(EXTI1_IRQn);

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
  if (rhport == 0) {
    TU_ASSERT(TCPP0203_SetGateDriverProvider(&tcpp0203_obj, TCPP0203_GD_PROVIDER_SWITCH_CLOSED) == TCPP0203_OK, );
  }
}

void EXTI1_IRQHandler(void) {
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);
    if (tcpp0203_obj.IsInitialized) {
      TU_ASSERT(TCPP0203_SetPowerMode(&tcpp0203_obj, TCPP0203_POWER_MODE_NORMAL) == TCPP0203_OK, );
      TU_ASSERT(TCPP0203_SetGateDriverProvider(&tcpp0203_obj, TCPP0203_GD_PROVIDER_SWITCH_CLOSED) == TCPP0203_OK, );
    }
}

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
