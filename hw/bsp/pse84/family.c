/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 TinyUSB contributors
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
   manufacturer: Infineon Technologies
*/

#include "bsp/board_api.h"
#include "tusb.h"

#if CFG_TUSB_OS != OPT_OS_ZEPHYR
#error "hw/bsp/pse84 currently supports Zephyr (-DRTOS=zephyr) only"
#endif

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>

//--------------------------------------------------------------------+
// Devicetree handles
//--------------------------------------------------------------------+
#define USBHS_NODE DT_NODELABEL(usbhs)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});

#if CFG_TUH_ENABLED
// KIT_PSE84_EVAL routes the USB Host signals (Type-A connector J27) through a
// mux (U25) and a VBUS power switch (U21/U22), both gated by USB_HOST_EN
// (P17[5]). The pin is low by default (device USB connected, host VBUS off);
// firmware must drive it high to switch the mux to J27 and enable 5V VBUS_HOST.
// USB_FAULT (P16[4], active-low) reports host VBUS overcurrent.
// Pins are provided via the example's board overlay as a zephyr,user node.
static const struct gpio_dt_spec usb_host_en = GPIO_DT_SPEC_GET_OR(DT_PATH(zephyr_user), host_en_gpios, {0});
static const struct gpio_dt_spec usb_fault = GPIO_DT_SPEC_GET_OR(DT_PATH(zephyr_user), usb_fault_gpios, {0});
#endif

//--------------------------------------------------------------------+
// USB interrupt -> TinyUSB handler
//--------------------------------------------------------------------+
static void usbhs_irq_handler(const void* arg) {
  (void) arg;
  tusb_int_handler(0, true);
}

//--------------------------------------------------------------------+
// Board init
//--------------------------------------------------------------------+
void board_init(void) {
  // LED
  if (gpio_is_ready_dt(&led)) {
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
  }

  // Button
  if (gpio_is_ready_dt(&button)) {
    gpio_pin_configure_dt(&button, GPIO_INPUT);
  }

#if CFG_TUH_ENABLED
  // Enable the USB Host path: drive USB_HOST_EN high (switches mux U25 to J27
  // and turns on VBUS_HOST). Without this the host port is disconnected and
  // unpowered, so no device can be enumerated.
  if (gpio_is_ready_dt(&usb_host_en)) {
    gpio_pin_configure_dt(&usb_host_en, GPIO_OUTPUT_ACTIVE);
  }
  // USB_FAULT is an input (active-low overcurrent flag); configure for reading.
  if (gpio_is_ready_dt(&usb_fault)) {
    gpio_pin_configure_dt(&usb_fault, GPIO_INPUT);
  }
#endif

  // Connect the USBHS controller interrupt to the TinyUSB handler.
  IRQ_CONNECT(DT_IRQ_BY_IDX(USBHS_NODE, 0, irq), DT_IRQ_BY_IDX(USBHS_NODE, 0, priority),
              usbhs_irq_handler, NULL, 0);
  irq_enable(DT_IRQ_BY_IDX(USBHS_NODE, 0, irq));
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+
void board_led_write(bool state) {
  if (gpio_is_ready_dt(&led)) {
    gpio_pin_set_dt(&led, state ? 1 : 0);
  }
}

uint32_t board_button_read(void) {
  if (gpio_is_ready_dt(&button)) {
    return (uint32_t) gpio_pin_get_dt(&button);
  }
  return 0;
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  return -1; // use Zephyr console instead
}

int board_uart_write(void const* buf, int len) {
  (void) buf;
  return len; // use Zephyr console instead
}
