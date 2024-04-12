/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024, HorrorTroll (<https://github.com/HorrorTroll>)
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

// LED
#define LED_PORT              GPIOC
#define LED_PIN               GPIO_PINS_5
#define LED_STATE_ON          0

// Button
#define BUTTON_PORT           GPIOA
#define BUTTON_PIN            GPIO_PINS_0
#define BUTTON_STATE_ACTIVE   1

// Enable PA2 as the debug log UART
//#define USART_DEV             USART2
//#define USART_CLK_EN          CRM_USART2_PERIPH_CLOCK
//#define USART_GPIO_PORT       GPIOA
//#define USART_TX_PIN          GPIO_PINS_2
//#define USART_RX_PIN          GPIO_PINS_3

//--------------------------------------------------------------------+
// CRM Clock
//--------------------------------------------------------------------+
static inline void board_clock_init(void) {
  /* reset crm */
  crm_reset();

  /* config flash psr register */
  flash_psr_set(FLASH_WAIT_CYCLE_4);

  /* enable hick */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_HICK, TRUE);

   /* wait till hick is ready */
  while(crm_flag_get(CRM_HICK_STABLE_FLAG) != SET)
  {
  }

  /* config pll clock resource */
  crm_pll_config(CRM_PLL_SOURCE_HICK, CRM_PLL_MULT_36);

  /* enable pll */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);

  /* wait till pll is ready */
  while(crm_flag_get(CRM_PLL_STABLE_FLAG) != SET)
  {
  }

  /* config ahbclk */
  crm_ahb_div_set(CRM_AHB_DIV_1);

  /* config apb2clk */
  crm_apb2_div_set(CRM_APB2_DIV_2);

  /* config apb1clk */
  crm_apb1_div_set(CRM_APB1_DIV_2);

  /* enable auto step mode */
  crm_auto_step_mode_enable(TRUE);

  /* select pll as system clock source */
  crm_sysclk_switch(CRM_SCLK_PLL);

  /* wait till pll is used as system clock source */
  while(crm_sysclk_switch_status_get() != CRM_SCLK_PLL)
  {
  }

  /* disable auto step mode */
  crm_auto_step_mode_enable(FALSE);

  /* config usbclk from pll */
  crm_usb_clock_div_set(CRM_USB_DIV_3);
  crm_usb_clock_source_select(CRM_USB_CLOCK_SOURCE_PLL);

  /* update system_core_clock global variable */
  system_core_clock_update();
}

static inline void board_vbus_sense_init(void)
{
  OTG1_GLOBAL->gccfg_bit.pwrdown = TRUE;
  OTG1_GLOBAL->gccfg_bit.vbusig = TRUE;
  OTG1_GLOBAL->gccfg_bit.avalidsesen = FALSE;
  OTG1_GLOBAL->gccfg_bit.bvalidsesen = FALSE;
}

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
