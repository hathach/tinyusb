/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to do so, subject to the
 * following conditions:
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
   manufacturer: Microchip
*/

#include "bsp/board_api.h"
#include "sam.h"

#include "hal/include/hal_gpio.h"
#include "hal/include/hal_init.h"
#include "hal/include/hal_usart_async.h"
#include "hpl/pmc/hpl_pmc.h"
#include "hpl/usart/hpl_usart_base.h"
#include "peripheral_clk_config.h"

static inline void board_vbus_set(uint8_t rhport, bool state);
void _init(void);
#include "board.h"

#ifndef LED_STATE_ON
  #define LED_STATE_ON 1
#endif

#ifndef LED_PORT_CLOCK
  #define LED_PORT_CLOCK ID_PIOA
#endif

#ifndef BUTTON_PORT_CLOCK
  #define BUTTON_PORT_CLOCK ID_PIOA
#endif

#ifndef UART_PORT_CLOCK
  #define UART_PORT_CLOCK ID_USART1
#endif

#ifndef BOARD_USART
  #define BOARD_USART USART1
#endif

#ifndef BOARD_UART_DESCRIPTOR
  #define BOARD_UART_DESCRIPTOR edbg_com
#endif

#ifndef BOARD_UART_BUFFER
  #define BOARD_UART_BUFFER edbg_com_buffer
#endif

#ifndef BUTTON_STATE_ACTIVE
  #define BUTTON_STATE_ACTIVE 0
#endif

#ifndef UART_TX_FUNCTION
  #define UART_TX_FUNCTION MUX_PB4D_USART1_TXD1
#endif

#ifndef UART_RX_FUNCTION
  #define UART_RX_FUNCTION MUX_PA21A_USART1_RXD1
#endif

#ifndef UART_BUFFER_SIZE
  #define UART_BUFFER_SIZE 64
#endif

#define LED_STATE_OFF (1 - LED_STATE_ON)

static struct usart_async_descriptor BOARD_UART_DESCRIPTOR;
static uint8_t BOARD_UART_BUFFER[UART_BUFFER_SIZE];
static volatile bool uart_busy = false;

static void tx_complete_cb(const struct usart_async_descriptor *const io_descr) {
  (void) io_descr;
  uart_busy = false;
}

void board_init(void) {
  init_mcu();

  /* Disable Watchdog */
  hri_wdt_set_MR_WDDIS_bit(WDT);

#ifdef LED_PIN
  _pmc_enable_periph_clock(LED_PORT_CLOCK);
  gpio_set_pin_level(LED_PIN, LED_STATE_OFF);
  gpio_set_pin_direction(LED_PIN, GPIO_DIRECTION_OUT);
  gpio_set_pin_function(LED_PIN, GPIO_PIN_FUNCTION_OFF);
#endif

#ifdef BUTTON_PIN
  _pmc_enable_periph_clock(BUTTON_PORT_CLOCK);
  gpio_set_pin_direction(BUTTON_PIN, GPIO_DIRECTION_IN);
  gpio_set_pin_pull_mode(BUTTON_PIN, BUTTON_STATE_ACTIVE ? GPIO_PULL_DOWN : GPIO_PULL_UP);
  gpio_set_pin_function(BUTTON_PIN, GPIO_PIN_FUNCTION_OFF);
#endif

  _pmc_enable_periph_clock(UART_PORT_CLOCK);
  gpio_set_pin_function(UART_RX_PIN, UART_RX_FUNCTION);
  gpio_set_pin_function(UART_TX_PIN, UART_TX_FUNCTION);

  usart_async_init(&BOARD_UART_DESCRIPTOR, BOARD_USART, BOARD_UART_BUFFER, sizeof(BOARD_UART_BUFFER), _usart_get_usart_async());
  usart_async_set_baud_rate(&BOARD_UART_DESCRIPTOR, CFG_BOARD_UART_BAUDRATE);
  usart_async_register_callback(&BOARD_UART_DESCRIPTOR, USART_ASYNC_TXC_CB, tx_complete_cb);
  usart_async_enable(&BOARD_UART_DESCRIPTOR);

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer (SystemCoreClock may not be correct after init)
  SysTick_Config(CONF_CPU_FREQUENCY / 1000);
#endif

  // Enable USB clock
  _pmc_enable_periph_clock(ID_USBHS);

#if CFG_TUH_ENABLED
  board_vbus_set(0, true);
#endif
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USBHS_Handler(void) {
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
#ifdef LED_PIN
  gpio_set_pin_level(LED_PIN, state ? LED_STATE_ON : LED_STATE_OFF);
#else
  (void) state;
#endif
}

uint32_t board_button_read(void) {
#ifdef BUTTON_PIN
  return BUTTON_STATE_ACTIVE == gpio_get_pin_level(BUTTON_PIN);
#else
  return 0;
#endif
}

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const *buf, int len) {
  while (uart_busy) {}
  uart_busy = true;

  io_write(&BOARD_UART_DESCRIPTOR.io, buf, len);
  return len;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t board_millis(void) {
  return system_ticks;
}
#endif

void _init(void) {
}
