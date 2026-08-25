/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
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

#ifndef PY32F071_HAL_CONF_H_
#define PY32F071_HAL_CONF_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED

#if !defined(HSI_VALUE)
  #define HSI_VALUE            ((uint32_t) 8000000)
#endif

#if !defined(HSE_VALUE)
  #define HSE_VALUE            ((uint32_t) 24000000)
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT  ((uint32_t) 200)
#endif

#if !defined(LSI_VALUE)
  #define LSI_VALUE            ((uint32_t) 32768)
#endif

#if !defined(LSE_VALUE)
  #define LSE_VALUE            ((uint32_t) 32768)
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT  ((uint32_t) 5000)
#endif

#define VDD_VALUE              ((uint32_t) 3300)
#define TICK_INT_PRIORITY      ((uint32_t) 3)
#define USE_RTOS               0
#define PREFETCH_ENABLE        0

#ifdef HAL_MODULE_ENABLED
  #include "py32f0xx_hal.h"
#endif

#ifdef HAL_RCC_MODULE_ENABLED
  #include "py32f071_hal_rcc.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
  #include "py32f071_hal_flash.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "py32f071_hal_gpio.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
  #include "py32f071_hal_pwr.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "py32f071_hal_cortex.h"
#endif

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
  #define assert_param(expr) ((expr) ? (void) 0U : assert_failed((uint8_t *) __FILE__, __LINE__))
#else
  #define assert_param(expr) ((void) 0U)
#endif

#ifdef __cplusplus
 }
#endif

#endif
