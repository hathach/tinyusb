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

#include "../board.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "hal/usb_hal.h"
#include "soc/usb_periph.h"
#include "soc/usb_wrap_struct.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32s2/rom/usb/usb_persist.h"

#include "driver/rmt.h"
#include "led_strip/include/led_strip.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

#define LED_PIN               45

#define BUTTON_PIN            0
#define BUTTON_STATE_ACTIVE   0

static led_strip_t *strip;

static void configure_pins(usb_hal_context_t *usb)
{
    /* usb_periph_iopins currently configures USB_OTG as USB Device.
     * Introduce additional parameters in usb_hal_context_t when adding support
     * for USB Host.
     */
    for (const usb_iopin_dsc_t* iopin = usb_periph_iopins; iopin->pin != -1; ++iopin) {
        if ((usb->use_external_phy) || (iopin->ext_phy_only == 0)) {
            gpio_pad_select_gpio(iopin->pin);
            if (iopin->is_output) {
                gpio_matrix_out(iopin->pin, iopin->func, false, false);
            } else {
                gpio_matrix_in(iopin->pin, iopin->func, false);
                gpio_pad_input_enable(iopin->pin);
            }
            gpio_pad_unhold(iopin->pin);
        }
    }

    if (!usb->use_external_phy) {
        gpio_set_drive_capability(USBPHY_DP_NUM, GPIO_DRIVE_CAP_3);
        gpio_set_drive_capability(USBPHY_DM_NUM, GPIO_DRIVE_CAP_3);
    }
}

// Initialize on-board peripherals : led, button, uart and USB
void board_init(void)
{
  // WS2812 Neopixel driver with RMT peripheral
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LED_PIN, RMT_CHANNEL_0);
  config.clk_div = 2; // set counter clock to 40MHz

  rmt_config(&config);
  rmt_driver_install(config.channel, 0, 0);

  led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(1, (led_strip_dev_t) config.channel);
  strip = led_strip_new_rmt_ws2812(&strip_config);
  strip->clear(strip, 100); // off led

  // Button
  gpio_pad_select_gpio(BUTTON_PIN);
  gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_PIN, BUTTON_STATE_ACTIVE ? GPIO_PULLDOWN_ONLY : GPIO_PULLUP_ONLY);

  if (USB_WRAP.date.val == USBDC_PERSIST_ENA) {
    // Enable USB/IO_MUX peripheral reset, if coming from persistent reboot
    REG_CLR_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_IO_MUX_RESET_DISABLE);
    REG_CLR_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_USB_RESET_DISABLE);
  } else {
    // Reset USB module
    periph_module_reset(PERIPH_USB_MODULE);
    periph_module_enable(PERIPH_USB_MODULE);
  }

  // USB Controller Hal init
  usb_hal_context_t hal = {
    .use_external_phy = false // use built-in PHY
  };
  usb_hal_init(&hal);
  configure_pins(&hal);
}

// Turn LED on or off
void board_led_write(bool state)
{
  strip->set_pixel(strip, 0, (state ? 0x88 : 0x00), 0x00, 0x00);
  strip->refresh(strip, 100);
}

// Get the current state of button
// a '1' means active (pressed), a '0' means inactive.
uint32_t board_button_read(void)
{
  return gpio_get_level(BUTTON_PIN) == BUTTON_STATE_ACTIVE;
}

// Get characters from UART
int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

// Send characters to UART
int board_uart_write(void const * buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

