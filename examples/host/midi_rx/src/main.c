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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// STATIC GLOBALS DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);
void midi_host_rx_task(void);

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  printf("TinyUSB Host MIDI Example\r\n");

  // init host stack on configured roothub port
  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  while (1) {
    tuh_task();
    led_blinking_task();
    midi_host_rx_task();
  }

  return 0;
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < interval_ms) return;// not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state;// toggle
}

//--------------------------------------------------------------------+
// MIDI host receive task
//--------------------------------------------------------------------+
void midi_host_rx_task(void) {
  // nothing to do, we just print out received data in callback
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with MIDI interface is mounted.
void tuh_midi_mount_cb(uint8_t idx, uint8_t num_cables_rx, uint16_t num_cables_tx) {
  printf("MIDI Interface Index = %u, Number of RX cables = %u, Number of TX cables = %u\r\n",
          idx, num_cables_rx, num_cables_tx);
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t idx) {
  printf("MIDI Interface Index = %u is unmounted\r\n", idx);
}

void tuh_midi_rx_cb(uint8_t idx, uint32_t num_packets) {
  if (num_packets == 0) {
    return;
  }

  uint8_t cable_num;
  uint8_t buffer[48];
  uint32_t bytes_read = tuh_midi_stream_read(idx, &cable_num, buffer, sizeof(buffer));

  printf("Cable %u rx %lu bytes: ", cable_num, bytes_read);
  for (uint32_t i = 0; i < bytes_read; i++) {
    printf("%02X ", buffer[i]);
  }
  printf("\r\n");
}

void tuh_midi_tx_cb(uint8_t idx) {
  (void) idx;
}
