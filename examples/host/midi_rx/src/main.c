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

#if CFG_TUH_MIDI

//--------------------------------------------------------------------+
// STATIC GLOBALS DECLARATION
//--------------------------------------------------------------------+
static uint8_t midi_dev_addr = 0;

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

#endif

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
  // device must be attached and have at least one endpoint ready to receive a message
  if (!midi_dev_addr || !tuh_midi_configured(midi_dev_addr)) {
    return;
  }
  if (tuh_midi_get_num_rx_cables(midi_dev_addr) < 1) {
    return;
  }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with MIDI interface is mounted.
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t num_cables_rx, uint16_t num_cables_tx) {
  (void) num_cables_rx;
  (void) num_cables_tx;
  midi_dev_addr = dev_addr;
  TU_LOG1("MIDI device address = %u, Number of RX cables = %u, Number of TX cables = %u\r\n",
          dev_addr, num_cables_rx, num_cables_tx);
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr) {
  (void) dev_addr;
  midi_dev_addr = 0;
  TU_LOG1("MIDI device address = %d is unmounted\r\n", dev_addr);
}

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets) {
  if (midi_dev_addr != dev_addr) {
    return;
  }

  if (num_packets == 0) {
    return;
  }

  uint8_t cable_num;
  uint8_t buffer[48];
  uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, sizeof(buffer));
  (void) bytes_read;

  TU_LOG1("Read bytes %lu cable %u", bytes_read, cable_num);
  TU_LOG1_MEM(buffer, bytes_read, 2);
}

void tuh_midi_tx_cb(uint8_t dev_addr) {
  (void) dev_addr;
}
