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

#include "bsp/board.h"

#include "sam.h"
#include "hal/include/hal_gpio.h"
#include "hal/include/hal_init.h"
#include "hpl/gclk/hpl_gclk_base.h"
#include "hpl_mclk_config.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
#define LED_PIN      16
#define BUTTON_PIN   (32 + 17)  // pin D2

/* Referenced GCLKs, should be initialized firstly */
#define _GCLK_INIT_1ST 0xFFFFFFFF

/* Not referenced GCLKs, initialized last */
#define _GCLK_INIT_LAST (~_GCLK_INIT_1ST)

void board_init(void)
{
  // Clock init ( follow hpl_init.c )
  hri_nvmctrl_set_CTRLA_RWS_bf(NVMCTRL, 0);

  _osc32kctrl_init_sources();
  _oscctrl_init_sources();
  _mclk_init();
#if _GCLK_INIT_1ST
  _gclk_init_generators_by_fref(_GCLK_INIT_1ST);
#endif
  _oscctrl_init_referenced_generators();
  _gclk_init_generators_by_fref(_GCLK_INIT_LAST);

  // Led init
  gpio_set_pin_direction(LED_PIN, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(LED_PIN, 0);

  // Button init
  gpio_set_pin_direction(BUTTON_PIN, GPIO_DIRECTION_IN);
  gpio_set_pin_pull_mode(BUTTON_PIN, GPIO_PULL_UP);

#if CFG_TUSB_OS  == OPT_OS_NONE
  // 1ms tick timer (samd SystemCoreClock may not correct)
  SysTick_Config(CONF_CPU_FREQUENCY / 1000);
#endif

  /* USB Clock init
   * The USB module requires a GCLK_USB of 48 MHz ~ 0.25% clock
   * for low speed and full speed operation. */
  hri_gclk_write_PCHCTRL_reg(GCLK, USB_GCLK_ID, GCLK_PCHCTRL_GEN_GCLK1_Val | GCLK_PCHCTRL_CHEN);
  hri_mclk_set_AHBMASK_USB_bit(MCLK);
  hri_mclk_set_APBBMASK_USB_bit(MCLK);

  // USB Pin Init
  gpio_set_pin_direction(PIN_PA24, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(PIN_PA24, false);
  gpio_set_pin_pull_mode(PIN_PA24, GPIO_PULL_OFF);
  gpio_set_pin_direction(PIN_PA25, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(PIN_PA25, false);
  gpio_set_pin_pull_mode(PIN_PA25, GPIO_PULL_OFF);

  gpio_set_pin_function(PIN_PA24, PINMUX_PA24H_USB_DM);
  gpio_set_pin_function(PIN_PA25, PINMUX_PA25H_USB_DP);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  gpio_set_pin_level(LED_PIN, state);
}

uint32_t board_button_read(void)
{
  // button is active low
  return gpio_get_pin_level(BUTTON_PIN) ? 0 : 1;
}

#if CFG_TUSB_OS  == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif
