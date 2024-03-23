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

#include "bsp/board_api.h"
#include "board.h"

#include "esp_rom_gpio.h"
#include "esp_mac.h"
#include "hal/gpio_ll.h"
#include "hal/usb_hal.h"
#include "soc/usb_periph.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#if ESP_IDF_VERSION_MAJOR > 4
  #include "esp_private/periph_ctrl.h"
#else
  #include "driver/periph_ctrl.h"
#endif

// Note; current code use UART0 can cause device to reset while monitoring
#define USE_UART  0
#define UART_ID  UART_NUM_0

#ifdef NEOPIXEL_PIN
#include "led_strip.h"
static led_strip_handle_t led_strip;
#endif

#if CFG_TUH_ENABLED && CFG_TUH_MAX3421
#include "driver/spi_master.h"
static void max3421_init(void);
#endif

static void configure_pins(usb_hal_context_t* usb);

//--------------------------------------------------------------------+
// Implementation
//--------------------------------------------------------------------+

// Initialize on-board peripherals : led, button, uart and USB
void board_init(void) {
#if USE_UART
  // uart init
  uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
  uart_driver_install(UART_ID, 1024, 0, 0, NULL, 0);
  uart_param_config(UART_ID, &uart_config);
#endif

#ifdef NEOPIXEL_PIN
  #ifdef NEOPIXEL_POWER_PIN
  gpio_reset_pin(NEOPIXEL_POWER_PIN);
  gpio_set_direction(NEOPIXEL_POWER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(NEOPIXEL_POWER_PIN, NEOPIXEL_POWER_STATE);
  #endif

  // WS2812 Neopixel driver with RMT peripheral
  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
      .resolution_hz = 10 * 1000 * 1000,  // RMT counter clock frequency, default = 10 Mhz
      .flags.with_dma = false,        // DMA feature is available on ESP target like ESP32-S3
  };

  led_strip_config_t strip_config = {
      .strip_gpio_num = NEOPIXEL_PIN,           // The GPIO that connected to the LED strip's data line
      .max_leds = 1,                            // The number of LEDs in the strip,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
      .led_model = LED_MODEL_WS2812,            // LED strip model
      .flags.invert_out = false,                // whether to invert the output signal
  };

  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  led_strip_clear(led_strip); // off
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

#if CFG_TUH_ENABLED && CFG_TUH_MAX3421
  max3421_init();
#endif
}

static void configure_pins(usb_hal_context_t* usb) {
  /* usb_periph_iopins currently configures USB_OTG as USB Device.
   * Introduce additional parameters in usb_hal_context_t when adding support
   * for USB Host. */
  for (const usb_iopin_dsc_t* iopin = usb_periph_iopins; iopin->pin != -1; ++iopin) {
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

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  // use factory default MAC as serial ID
  esp_efuse_mac_get_default(id);
  return 6;
}

void board_led_write(bool state) {
#ifdef NEOPIXEL_PIN
  led_strip_set_pixel(led_strip, 0, state ? 0x08 : 0x00, 0x00, 0x00);
  led_strip_refresh(led_strip);
#endif
}

// Get the current state of button
// a '1' means active (pressed), a '0' means inactive.
uint32_t board_button_read(void) {
  return gpio_get_level(BUTTON_PIN) == BUTTON_STATE_ACTIVE;
}

// Get characters from UART
int board_uart_read(uint8_t* buf, int len) {
#if USE_UART
  return uart_read_bytes(UART_ID, buf, len, 0);
#else
  return -1;
#endif
}

// Send characters to UART
int board_uart_write(void const* buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_getchar(void) {
  uint8_t c = 0;
  return board_uart_read(&c, 1) > 0 ? (int) c : (-1);
}

//--------------------------------------------------------------------+
// API: SPI transfer with MAX3421E, must be implemented by application
//--------------------------------------------------------------------+
#if CFG_TUH_ENABLED && defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421

static spi_device_handle_t max3421_spi;
SemaphoreHandle_t max3421_intr_sem;

static void IRAM_ATTR max3421_isr_handler(void* arg) {
  (void) arg; // arg is gpio num
  gpio_set_level(13, 1);

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(max3421_intr_sem, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }

  gpio_set_level(13, 0);
}

static void max3421_intr_task(void* param) {
  (void) param;

  while (1) {
    xSemaphoreTake(max3421_intr_sem, portMAX_DELAY);
    tuh_int_handler(BOARD_TUH_RHPORT, false);
  }
}

static void max3421_init(void) {
  // CS pin
  gpio_set_direction(MAX3421_CS_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(MAX3421_CS_PIN, 1);

  // SPI
  spi_bus_config_t buscfg = {
      .miso_io_num = MAX3421_MISO_PIN,
      .mosi_io_num = MAX3421_MOSI_PIN,
      .sclk_io_num = MAX3421_SCK_PIN,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,
      .max_transfer_sz = 1024
  };
  ESP_ERROR_CHECK(spi_bus_initialize(MAX3421_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t max3421_cfg = {
      .mode = 0,
      .clock_speed_hz = 26000000,
      .spics_io_num = -1, // manual control CS
      .queue_size = 1
  };
  ESP_ERROR_CHECK(spi_bus_add_device(MAX3421_SPI_HOST, &max3421_cfg, &max3421_spi));

  // debug
  gpio_set_direction(13, GPIO_MODE_OUTPUT);
  gpio_set_level(13, 0);

  // Interrupt pin
  max3421_intr_sem = xSemaphoreCreateBinary();
  xTaskCreate(max3421_intr_task, "max3421 intr", 2048, NULL, configMAX_PRIORITIES - 2, NULL);

  gpio_set_direction(MAX3421_INTR_PIN, GPIO_MODE_INPUT);
  gpio_set_intr_type(MAX3421_INTR_PIN, GPIO_INTR_NEGEDGE);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(MAX3421_INTR_PIN, max3421_isr_handler, NULL);
}

void tuh_max3421_int_api(uint8_t rhport, bool enabled) {
  (void) rhport;
  if (enabled) {
    gpio_intr_enable(MAX3421_INTR_PIN);
  } else {
    gpio_intr_disable(MAX3421_INTR_PIN);
  }
}

void tuh_max3421_spi_cs_api(uint8_t rhport, bool active) {
  (void) rhport;
  gpio_set_level(MAX3421_CS_PIN, active ? 0 : 1);
}

bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const* tx_buf, uint8_t* rx_buf, size_t xfer_bytes) {
  (void) rhport;

  if (tx_buf == NULL) {
    // fifo read, transmit rx_buf as dummy
    tx_buf = rx_buf;
  }

  // length in bits
  size_t const len_bits = xfer_bytes << 3;

  spi_transaction_t xact = {
      .length = len_bits,
      .rxlength = rx_buf ? len_bits : 0,
      .tx_buffer = tx_buf,
      .rx_buffer = rx_buf
  };

  ESP_ERROR_CHECK(spi_device_transmit(max3421_spi, &xact));
  return true;
}

#endif
