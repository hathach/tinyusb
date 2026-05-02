/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
   manufacturer: Geehy
*/

#include "apm32f0xx.h"
#include "apm32f0xx_rcm.h"
#include "apm32f0xx_gpio.h"
#include "apm32f0xx_misc.h"
#include "apm32f0xx_crs.h"
#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USBD_IRQHandler(void) {
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// Board Init
//--------------------------------------------------------------------+
void board_init(void) {
  // Enable HSI48 for USB clock
  RCM_EnableHSI48();
  while (RCM_ReadStatusFlag(RCM_FLAG_HSI48RDY) == RESET) {}

  // Select HSI48 as USB clock source
  RCM_ConfigUSBCLK(RCM_USBCLK_HSI48);

  // Enable CRS for automatic HSI48 calibration from USB SOF
  RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_CRS);
  CRS_ConfigSynchronizationSource(CRS_SYNC_SOURCE_USB);
  CRS_EnableAutomaticCalibration();
  CRS_EnableFrequencyErrorCounter();

  // Enable USB peripheral clock
  RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_USB);

  // SysTick 1ms tick
  SysTick_Config(SystemCoreClock / 1000);

  // LED
  LED_GPIO_CLK_EN();
  GPIO_Config_T gpio_config;
  GPIO_ConfigStructInit(&gpio_config);
  gpio_config.pin = LED_PIN;
  gpio_config.mode = GPIO_MODE_OUT;
  gpio_config.outtype = GPIO_OUT_TYPE_PP;
  gpio_config.speed = GPIO_SPEED_50MHz;
  GPIO_Config(LED_PORT, &gpio_config);

  board_led_write(false);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+
void board_led_write(bool state) {
  if (state ^ (!LED_STATE_ON)) {
    GPIO_SetBit(LED_PORT, LED_PIN);
  } else {
    GPIO_ClearBit(LED_PORT, LED_PIN);
  }
}

uint32_t board_button_read(void) {
  return 0;
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void) max_len;
  volatile uint32_t *apm32_uuid = ((volatile uint32_t *) 0x1FFFF7AC);
  uint32_t *id32 = (uint32_t *) (uintptr_t) id;
  uint8_t const len = 12;

  id32[0] = apm32_uuid[0];
  id32[1] = apm32_uuid[1];
  id32[2] = apm32_uuid[2];

  return len;
}

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}

void SVC_Handler(void) {
}

void PendSV_Handler(void) {
}
#endif

void HardFault_Handler(void) {
  __asm("BKPT #0\n");
}

void _init(void) {
}
