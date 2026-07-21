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
 */

#include "bsp/board_api.h"
#include "tusb.h"

#include <stdio.h>
#include <time.h>

//--------------------------------------------------------------------+
// Board API
//--------------------------------------------------------------------+

void board_init(void) {
}

void board_init_after_tusb(void) {
  // Raw Gadget must be connected after the TinyUSB device stack is initialized.
  tud_connect();
}

void board_led_write(bool state) {
  (void) state;
}

uint32_t board_button_read(void) {
  return 0;
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;

  return -1;
}

int board_uart_write(void const* buf, int len) {
  if ((buf == NULL) || (len <= 0)) {
    return -1;
  }

  size_t const written = fwrite(buf, 1, (size_t) len, stdout);
  fflush(stdout);

  return (int) written;
}

uint32_t tusb_time_millis_api(void) {
  struct timespec timestamp;

  if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
    return 0;
  }

  uint64_t const milliseconds = ((uint64_t) timestamp.tv_sec * 1000u) +
                                ((uint64_t) timestamp.tv_nsec / 1000000u);

  return (uint32_t) milliseconds;
}
