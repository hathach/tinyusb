/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Scott Shawcroft for Adafruit Industries
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
 */

/* This example prints out info about the enumerated devices. */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);

static xfer_result_t _get_string_result;

static bool _transfer_done_cb(uint8_t daddr, tusb_control_request_t const *request, xfer_result_t result) {
    (void)daddr;
    (void)request;
    _get_string_result = result;
    return true;
}

static void _convert_utf16le_to_utf8(const uint16_t *utf16, size_t utf16_len, uint8_t *utf8, size_t utf8_len) {
    // TODO: Check for runover.
    (void)utf8_len;
    // Get the UTF-16 length out of the data itself.

    for (size_t i = 0; i < utf16_len; i++) {
        uint16_t chr = utf16[i];
        if (chr < 0x80) {
            *utf8++ = chr & 0xff;
        } else if (chr < 0x800) {
            *utf8++ = (uint8_t)(0xC0 | (chr >> 6 & 0x1F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
        } else {
            // TODO: Verify surrogate.
            *utf8++ = (uint8_t)(0xE0 | (chr >> 12 & 0x0F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 6 & 0x3F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
        }
        // TODO: Handle UTF-16 code points that take two entries.
    }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t *buf, size_t len) {
    size_t total_bytes = 0;
    for (size_t i = 0; i < len; i++) {
        uint16_t chr = buf[i];
        if (chr < 0x80) {
            total_bytes += 1;
        } else if (chr < 0x800) {
            total_bytes += 2;
        } else {
            total_bytes += 3;
        }
        // TODO: Handle UTF-16 code points that take two entries.
    }
    return total_bytes;
}

static void _wait_and_convert(uint16_t *temp_buf, size_t buf_len) {
    while (_get_string_result == 0xff) {
        tuh_task();
    }
    size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
    size_t utf8_len = _count_utf8_bytes(temp_buf + 1, utf16_len);
    _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t *) temp_buf, sizeof(uint16_t) * buf_len);
    ((uint8_t*) temp_buf)[utf8_len] = '\0';
}

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  printf("TinyUSB Host Device Info Example\r\n");

  tusb_init();

  uint32_t interval_ms = 5000;
  uint32_t start_time = 0;

  while (1)
  {
    // tinyusb host task
    tuh_task();
    led_blinking_task();

    if (board_millis() - start_time < interval_ms) {
      continue;
    }
    start_time = board_millis();
    // Brute force check every device address to see if it is active.
    for (int i = 1; i < CFG_TUH_DEVICE_MAX + CFG_TUH_HUB + 1; i++) {
      if (!tuh_ready(i)) {
        continue;
      }
      uint16_t vid;
      uint16_t pid;
      tuh_vid_pid_get(i, &vid, &pid);
      printf("%d vid %04x pid %04x\r\n", i, vid, pid);

      _get_string_result = 0xff;
      uint16_t temp_buf[127];
      if (tuh_descriptor_string_serial_get(i, 0, temp_buf, TU_ARRAY_SIZE(temp_buf), _transfer_done_cb)) {
        _wait_and_convert(temp_buf, TU_ARRAY_SIZE(temp_buf));
        printf("Serial: %s\r\n", (const char*) temp_buf);
      }

      _get_string_result = 0xff;
      temp_buf[0] = 0;
      if (tuh_descriptor_string_product_get(i, 0, temp_buf, TU_ARRAY_SIZE(temp_buf), _transfer_done_cb)) {
        _wait_and_convert(temp_buf, TU_ARRAY_SIZE(temp_buf));
        printf("Product: %s\r\n", (const char*) temp_buf);
      }

      _get_string_result = 0xff;
      temp_buf[0] = 0;
      if (tuh_descriptor_string_manufacturer_get(i, 0, temp_buf, TU_ARRAY_SIZE(temp_buf), _transfer_done_cb)) {
        _wait_and_convert(temp_buf, TU_ARRAY_SIZE(temp_buf));
        printf("Manufacturer: %s\r\n", (const char*) temp_buf);
      }
    }
    printf("\n");
  }

  return 0;
}


//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
