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

#include "sam.h"
#include "bsp/board.h"
#include "board.h"

#include "hal/include/hal_gpio.h"
#include "hal/include/hal_init.h"
#include "hri/hri_nvmctrl_d21.h"

#include "hpl/gclk/hpl_gclk_base.h"
#include "hpl_pm_config.h"
#include "hpl/pm/hpl_pm_base.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_Handler(void)
{
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// UART support
//--------------------------------------------------------------------+
static void uart_init(void);

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

/* Referenced GCLKs, should be initialized firstly */
#define _GCLK_INIT_1ST (1 << 0 | 1 << 1)

/* Not referenced GCLKs, initialized last */
#define _GCLK_INIT_LAST (~_GCLK_INIT_1ST)

void board_init(void)
{
  // Clock init ( follow hpl_init.c )
  hri_nvmctrl_set_CTRLB_RWS_bf(NVMCTRL, 2);

  _pm_init();
  _sysctrl_init_sources();
#if _GCLK_INIT_1ST
  _gclk_init_generators_by_fref(_GCLK_INIT_1ST);
#endif
  _sysctrl_init_referenced_generators();
  _gclk_init_generators_by_fref(_GCLK_INIT_LAST);

  // Update SystemCoreClock since it is hard coded with asf4 and not correct
  // Init 1ms tick timer (samd SystemCoreClock may not correct)
  SystemCoreClock = CONF_CPU_FREQUENCY;
#if CFG_TUSB_OS  == OPT_OS_NONE
  SysTick_Config(CONF_CPU_FREQUENCY / 1000);
#endif

  // Led init
#ifdef LED_PIN
  gpio_set_pin_direction(LED_PIN, GPIO_DIRECTION_OUT);
  board_led_write(false);
#endif

  // Button init
#ifdef BUTTON_PIN
  gpio_set_pin_direction(BUTTON_PIN, GPIO_DIRECTION_IN);
  gpio_set_pin_pull_mode(BUTTON_PIN, BUTTON_STATE_ACTIVE ? GPIO_PULL_DOWN : GPIO_PULL_UP);
#endif

  uart_init();

#if CFG_TUSB_OS  == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  /* USB Clock init
   * The USB module requires a GCLK_USB of 48 MHz ~ 0.25% clock
   * for low speed and full speed operation. */
  _pm_enable_bus_clock(PM_BUS_APBB, USB);
  _pm_enable_bus_clock(PM_BUS_AHB, USB);
  _gclk_enable_channel(USB_GCLK_ID, GCLK_CLKCTRL_GEN_GCLK0_Val);

  // USB Pin Init
  gpio_set_pin_direction(PIN_PA24, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(PIN_PA24, false);
  gpio_set_pin_pull_mode(PIN_PA24, GPIO_PULL_OFF);
  gpio_set_pin_direction(PIN_PA25, GPIO_DIRECTION_OUT);
  gpio_set_pin_level(PIN_PA25, false);
  gpio_set_pin_pull_mode(PIN_PA25, GPIO_PULL_OFF);

  gpio_set_pin_function(PIN_PA24, PINMUX_PA24G_USB_DM);
  gpio_set_pin_function(PIN_PA25, PINMUX_PA25G_USB_DP);

  // Output 500hz PWM on D12 (PA19 - TCC0 WO[3]) so we can validate the GCLK0 clock speed with a Saleae.
  _pm_enable_bus_clock(PM_BUS_APBC, TCC0);
  TCC0->PER.bit.PER = 48000000 / 1000;
  TCC0->CC[3].bit.CC = 48000000 / 2000;
  TCC0->CTRLA.bit.ENABLE = true;

  gpio_set_pin_function(PIN_PA19, PINMUX_PA19F_TCC0_WO3);
  _gclk_enable_channel(TCC0_GCLK_ID, GCLK_CLKCTRL_GEN_GCLK0_Val);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  (void)state;
#ifdef LED_PIN
  gpio_set_pin_level(LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
#endif
}

uint32_t board_button_read(void)
{
#ifdef BUTTON_PIN
  return BUTTON_STATE_ACTIVE == gpio_get_pin_level(BUTTON_PIN);
#else
  return 0;
#endif
}

#if defined(UART_SERCOM)

#define BOARD_SERCOM2(n)  SERCOM ## n
#define BOARD_SERCOM(n) BOARD_SERCOM2(n)

static void uart_init(void)
{
#if UART_SERCOM == 0
  gpio_set_pin_function(PIN_PA06, PINMUX_PA06D_SERCOM0_PAD2);
  gpio_set_pin_function(PIN_PA07, PINMUX_PA07D_SERCOM0_PAD3);

  // setup clock (48MHz)
  _pm_enable_bus_clock(PM_BUS_APBC, SERCOM0);
  _gclk_enable_channel(SERCOM0_GCLK_ID_CORE, GCLK_CLKCTRL_GEN_GCLK0_Val);

  SERCOM0->USART.CTRLA.bit.SWRST = 1; /* reset SERCOM & enable config */
  while(SERCOM0->USART.SYNCBUSY.bit.SWRST);

  SERCOM0->USART.CTRLA.reg  =  /* CMODE = 0 -> async, SAMPA = 0, FORM = 0 -> USART frame, SMPR = 0 -> arithmetic baud rate */
    SERCOM_USART_CTRLA_SAMPR(1) | /* 0 = 16x / arithmetic baud rate, 1 = 16x / fractional baud rate */
//    SERCOM_USART_CTRLA_FORM(0) | /* 0 = USART Frame, 2 = LIN Master */
    SERCOM_USART_CTRLA_DORD | /* LSB first */
    SERCOM_USART_CTRLA_MODE(1) | /* 0 = Asynchronous, 1 = USART with internal clock */
    SERCOM_USART_CTRLA_RXPO(3) | /* pad 3 */
    SERCOM_USART_CTRLA_TXPO(1);  /* pad 2 */

  SERCOM0->USART.CTRLB.reg =
    SERCOM_USART_CTRLB_TXEN | /* tx enabled */
    SERCOM_USART_CTRLB_RXEN;  /* rx enabled */

  SERCOM0->USART.BAUD.reg = SERCOM_USART_BAUD_FRAC_FP(0) | SERCOM_USART_BAUD_FRAC_BAUD(26);

  SERCOM0->USART.CTRLA.bit.ENABLE = 1; /* activate SERCOM */
  while(SERCOM0->USART.SYNCBUSY.bit.ENABLE); /* wait for SERCOM to be ready */
#endif
}

static inline void uart_send_buffer(uint8_t const *text, size_t len)
{
  for (size_t i = 0; i < len; ++i) {
    BOARD_SERCOM(UART_SERCOM)->USART.DATA.reg = text[i];
    while((BOARD_SERCOM(UART_SERCOM)->USART.INTFLAG.reg & SERCOM_USART_INTFLAG_TXC) == 0);
  }
}

static inline void uart_send_str(const char* text)
{
  while (*text) {
    BOARD_SERCOM(UART_SERCOM)->USART.DATA.reg = *text++;
    while((BOARD_SERCOM(UART_SERCOM)->USART.INTFLAG.reg & SERCOM_USART_INTFLAG_TXC) == 0);
  }
}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
  if (len < 0) {
    uart_send_str(buf);
  } else {
    uart_send_buffer(buf, len);
  }
  return len;
}

#else // ! defined(UART_SERCOM)
static void uart_init(void)
{

}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
  (void) buf; (void) len;
  return 0;
}
#endif

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
