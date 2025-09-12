/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025, Dalton Caron
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
    name: STM32 NUCLEO-WBA65RI
    url: https://www.st.com/en/evaluation-tools/nucleo-wba65ri.html
 */
#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

    // LED (user LED 1)
#define LED_CLK_EN            __HAL_RCC_GPIOD_CLK_ENABLE
#define LED_PORT              GPIOD
#define LED_PIN               GPIO_PIN_8
#define LED_STATE_ON          1

// Button (user button 1)
#define BUTTON_CLK_EN         __HAL_RCC_GPIOC_CLK_ENABLE
#define BUTTON_PORT           GPIOC
#define BUTTON_PIN            GPIO_PIN_13
#define BUTTON_STATE_ACTIVE   0

// USART (on STM link USB connector)
#define USART_GPIO_CLK_EN     __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE
#define USART_DEV             USART3
#define USART_CLK_EN          __HAL_RCC_USART3_CLK_ENABLE
#define USART_TX_GPIO_PORT    GPIOA
#define USART_RX_GPIO_PORT    GPIOA
#define USART_GPIO_AF         GPIO_AF8_USART3
#define USART_TX_PIN          GPIO_PIN_7
#define USART_RX_PIN          GPIO_PIN_5

// USB Pins
// These pints are only used for USB and must be in analog mode when not used.
// They are by default configured for USB operation after reset.
#define USB_DP_PORT           GPIOD
#define USB_DP_PIN            GPIO_PIN_6

#define USB_DM_PORT           GPIOD
#define USB_DM_PIN            GPIO_PIN_7

//--------------------------------------------------------------------+
//    The system clock is configured as follows:
//        System Clock source       = PLL (HSE, crystal)
//        SYSCLK (CPU Clock)        = 64 MHz
//        HCLK (AXI and AHB Clocks) = 64 MHz
//        AHB Prescaler             = 1
//        APB1 Prescaler            = 1 (APB3 Clock = 64MHz)
//        APB2 Prescaler            = 1 (APB1 Clock = 64MHz)
//        APB7 Prescaler            = 1 (APB2 Clock = 64MHz)
//        HPRE5 Prescaler           = 2 (AHB5 Clock = 32MHz)
//        HSE Frequency (Hz)        = 32 MHz
//        PLL_M                     = 2
//        PLL_N                     = 8
//        PLL_P                     = 2
//        PLL_Q                     = 2
//        PLL_R                     = 2
//        VDD (V)                   = 3.3 V
//        Flash Latency             = 1  Wait States
//--------------------------------------------------------------------+
static void board_system_clock_config( void )
{
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    ( void ) HAL_PWREx_ConfigSupply( PWR_LDO_SUPPLY );

    // Must be in voltage scaling mode 1 for the OTG USB HS peripheral to function
    ( void ) HAL_PWREx_ControlVoltageScaling( PWR_REGULATOR_VOLTAGE_SCALE1 );

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEDiv = RCC_HSE_DIV1;
    RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL1.PLLM = 2;
    RCC_OscInitStruct.PLL1.PLLN = 8;
    RCC_OscInitStruct.PLL1.PLLP = 2;
    RCC_OscInitStruct.PLL1.PLLQ = 2;
    RCC_OscInitStruct.PLL1.PLLR = 2;
    RCC_OscInitStruct.PLL1.PLLFractional = 0;
    ( void ) HAL_RCC_OscConfig( &RCC_OscInitStruct );

    RCC_ClkInitStruct.ClockType = ( RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
        | RCC_CLOCKTYPE_PCLK7 | RCC_CLOCKTYPE_HCLK5 );
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB7CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.AHB5_PLL1_CLKDivider = RCC_SYSCLK_PLL1_DIV2;
    RCC_ClkInitStruct.AHB5_HSEHSI_CLKDivider = RCC_SYSCLK_HSEHSI_DIV1;

    ( void ) HAL_RCC_ClockConfig( &RCC_ClkInitStruct, FLASH_LATENCY_1 );

    ( void ) SystemCoreClockUpdate();

    ( void ) HAL_ICACHE_Enable();
}

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
