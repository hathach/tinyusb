/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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
   manufacturer: HPMicro
*/

#include "bsp/board_api.h"
#include "board.h"
#include "hpm_clock_drv.h"
#include "hpm_uart_drv.h"
#include "hpm_gpio_drv.h"
#include "hpm_romapi.h"


//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

// Initialize on-board peripherals : led, button, uart and USB
void board_init(void) {
  board_init_clock();
  board_init_pmp();

  board_init_usb(HPM_USB0);
#ifdef HPM_USB1
  board_init_usb(HPM_USB1);
#endif

  board_init_console();
  board_init_gpio_pins();
  board_init_led_pins();
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
SDK_DECLARE_EXT_ISR_M(IRQn_USB0, isr_usb0)
void isr_usb0(void) {
  tusb_int_handler(0, true);
}

#ifdef HPM_USB1_BASE
SDK_DECLARE_EXT_ISR_M(IRQn_USB1, isr_usb1)
void isr_usb1(void) {
  tusb_int_handler(1, true);
}
#endif

void board_led_write(bool state) {
  if (state) {
    gpio_write_pin(BOARD_LED_GPIO_CTRL, BOARD_LED_GPIO_INDEX, BOARD_LED_GPIO_PIN, BOARD_LED_ON_LEVEL);
  } else {
    gpio_write_pin(BOARD_LED_GPIO_CTRL, BOARD_LED_GPIO_INDEX, BOARD_LED_GPIO_PIN, BOARD_LED_OFF_LEVEL);
  }
}

uint32_t board_button_read(void) {
  return (gpio_read_pin(BOARD_APP_GPIO_CTRL, BOARD_APP_GPIO_INDEX, BOARD_APP_GPIO_PIN) == BOARD_BUTTON_PRESSED_VALUE) ? 1 : 0;
}

// Get characters from UART. Return number of read bytes
int board_uart_read(uint8_t *buf, int len) {
  int        count = 0;
  hpm_stat_t status;

  while (count < len) {
    status = uart_try_receive_byte((UART_Type *)BOARD_CONSOLE_UART_BASE, (uint8_t *)&buf[count]);
    if (status == status_success) {
      count++;
    } else {
      break;
    }
  }

  return count;
}

// Send characters to UART. Return number of sent bytes
int board_uart_write(void const *buf, int len) {
  uart_send_data((UART_Type *)BOARD_CONSOLE_UART_BASE, (uint8_t const *)buf, len);

  return len;
}

#if CFG_TUSB_OS == OPT_OS_NONE
// Get current milliseconds, must be implemented when no RTOS is used
uint32_t tusb_time_millis_api(void) {
  return (hpm_csr_get_core_cycle() / clock_get_core_clock_ticks_per_ms());
}

#endif
