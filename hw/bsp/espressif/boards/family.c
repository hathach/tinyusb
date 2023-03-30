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

#include "bsp/board.h"
#include "board.h"

#include "esp_rom_gpio.h"
#include "hal/gpio_ll.h"
#include "hal/usb_hal.h"
#include "soc/usb_periph.h"

#include "driver/rmt.h"

#if ESP_IDF_VERSION_MAJOR > 4
  #include "esp_private/periph_ctrl.h"
#else
  #include "driver/periph_ctrl.h"
#endif

#ifdef NEOPIXEL_PIN
#include "led_strip.h"
static led_strip_t *strip;
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

static void configure_pins(usb_hal_context_t *usb);

// Initialize on-board peripherals : led, button, uart and USB
void board_init(void)
{

#ifdef NEOPIXEL_PIN
  #ifdef NEOPIXEL_POWER_PIN
  gpio_reset_pin(NEOPIXEL_POWER_PIN);
  gpio_set_direction(NEOPIXEL_POWER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(NEOPIXEL_POWER_PIN, NEOPIXEL_POWER_STATE);
  #endif

  // WS2812 Neopixel driver with RMT peripheral
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX(NEOPIXEL_PIN, RMT_CHANNEL_0);
  config.clk_div = 2; // set counter clock to 40MHz

  rmt_config(&config);
  rmt_driver_install(config.channel, 0, 0);

  led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(1, (led_strip_dev_t) config.channel);
  strip = led_strip_new_rmt_ws2812(&strip_config);
  strip->clear(strip, 100); // off led
#endif

  // Button
  esp_rom_gpio_pad_select_gpio(BUTTON_PIN);
  gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_PIN, BUTTON_STATE_ACTIVE ? GPIO_PULLDOWN_ONLY : GPIO_PULLUP_ONLY);

  // USB Controller Hal init
  periph_module_reset(PERIPH_USB_MODULE);
  periph_module_enable(PERIPH_USB_MODULE);

  usb_hal_context_t hal = {
    .use_external_phy = false // use built-in PHY
  };
  usb_hal_init(&hal);
  configure_pins(&hal);
}

static void configure_pins(usb_hal_context_t *usb)
{
  /* usb_periph_iopins currently configures USB_OTG as USB Device.
   * Introduce additional parameters in usb_hal_context_t when adding support
   * for USB Host.
   */
  for (const usb_iopin_dsc_t *iopin = usb_periph_iopins; iopin->pin != -1; ++iopin) {
    if ((usb->use_external_phy) || (iopin->ext_phy_only == 0)) {
      esp_rom_gpio_pad_select_gpio(iopin->pin);
      if (iopin->is_output) {
        esp_rom_gpio_connect_out_signal(iopin->pin, iopin->func, false, false);
      } else {
        esp_rom_gpio_connect_in_signal(iopin->pin, iopin->func, false);
#if ESP_IDF_VERSION_MAJOR > 4
        if ((iopin->pin != GPIO_MATRIX_CONST_ZERO_INPUT) && (iopin->pin != GPIO_MATRIX_CONST_ONE_INPUT))
#else
        if ((iopin->pin != GPIO_FUNC_IN_LOW) && (iopin->pin != GPIO_FUNC_IN_HIGH))
#endif
        {
          gpio_ll_input_enable(&GPIO, iopin->pin);
        }
      }
      esp_rom_gpio_pad_unhold(iopin->pin);
    }
  }
  if (!usb->use_external_phy) {
    gpio_set_drive_capability(USBPHY_DM_NUM, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(USBPHY_DP_NUM, GPIO_DRIVE_CAP_3);
  }
}

// Turn LED on or off
void board_led_write(bool state)
{
#ifdef NEOPIXEL_PIN
  strip->set_pixel(strip, 0, (state ? 0x88 : 0x00), 0x00, 0x00);
  strip->refresh(strip, 100);
#endif
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
