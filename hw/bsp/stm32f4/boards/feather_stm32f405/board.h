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
   name: Adafruit Feather STM32F405
   url: https://www.adafruit.com/product/4382
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define UART_ID       1
#define PINID_LED     0
#define PINID_UART_TX 1
#define PINID_UART_RX 2
#define VBUS_SENSE_EN 0

/* 板载外设引脚定义 */
static board_pindef_t board_pindef[] = {
  {// LED（指示灯）
   .port = GPIOA,
   .pin_init =
     {
       .Pin       = GPIO_PIN_2,
       .Mode      = GPIO_MODE_OUTPUT_PP,
       .Pull      = GPIO_PULLDOWN,
       .Speed     = GPIO_SPEED_HIGH,
       .Alternate = 0,
     },
   .active_state = 1},
  {// UART TX（串口发送）
   .port = GPIOA,
   .pin_init =
     {
       .Pin       = GPIO_PIN_9,
       .Mode      = GPIO_MODE_AF_PP,
       .Pull      = GPIO_PULLUP,
       .Speed     = GPIO_SPEED_HIGH,
       .Alternate = GPIO_AF7_USART1,
     },
   .active_state = 0},
  {// UART RX（串口接收）
   .port = GPIOA,
   .pin_init =
     {
       .Pin       = GPIO_PIN_10,
       .Mode      = GPIO_MODE_AF_PP,
       .Pull      = GPIO_PULLUP,
       .Speed     = GPIO_SPEED_HIGH,
       .Alternate = GPIO_AF7_USART1,
     },
   .active_state = 0},
};

//--------------------------------------------------------------------+
// RCC 时钟配置（HSE + PLL → 168 MHz）
//--------------------------------------------------------------------+
static inline void board_clock_init(void) {
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* 使能电源控制时钟 */
  __HAL_RCC_PWR_CLK_ENABLE();

  // 配置电压调节器为 Scale 1 模式（系统频率 ≤ 168 MHz 时需要）电压调节等级需根据系统频率查阅芯片数据手册
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1); // 电压调节等级

  /* 使能外部高速晶振（HSE）并配置 PLL */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE; // 外部高速晶振
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;             // 使能外部高速晶振
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;             // 使能主 PLL
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;      // 外部高速晶振作为 PLL 输入源
  RCC_OscInitStruct.PLL.PLLM       = HSE_VALUE / 1000000;    // PLL 输入分频 → 1 MHz
  RCC_OscInitStruct.PLL.PLLN       = 336;                    // PLL 倍频 → 336 MHz
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;          // 主 PLL 输出分频 → 168 MHz
  RCC_OscInitStruct.PLL.PLLQ       = 7;                      // USB/音频 PLL 输出 → 48 MHz
  HAL_RCC_OscConfig(&RCC_OscInitStruct);                     // 配置时钟源

  // 选择 PLL 作为系统时钟源，配置 HCLK、PCLK1、PCLK2 分频器
  RCC_ClkInitStruct.ClockType =
    (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2); // 选择要配置的时钟
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK; // 选择主 PLL 作为系统时钟源
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;         // HCLK = SYSCLK / 1 = 168 MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;           // PCLK1 = HCLK / 4 = 42 MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;           // PCLK2 = HCLK / 2 = 84 MHz
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);   // 配置时钟分频器
}

static inline void board_vbus_set(uint8_t rhport, bool state) {
  (void)rhport;
  (void)state;
#if defined(UART_ID) && defined(PINID_UART_TX) && defined(PINID_UART_RX)
  /* Re-init UART pins after USB setup to restore PA9/PA10 UART function */
  HAL_GPIO_Init(board_pindef[PINID_UART_TX].port, &board_pindef[PINID_UART_TX].pin_init);
  HAL_GPIO_Init(board_pindef[PINID_UART_RX].port, &board_pindef[PINID_UART_RX].pin_init);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
