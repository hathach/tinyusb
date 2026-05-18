/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
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

// Minimal USB-MIDI 2.0 host example.
// Receives UMP from any MIDI 2.0 device, prints each packet to stdout.

#include <stdio.h>
#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "class/midi/midi2_host.h"

//--------------------------------------------------------------------+
// State
//--------------------------------------------------------------------+

static uint8_t midi2_idx = 0xFF;

//--------------------------------------------------------------------+
// UMP printer - shows MT and word(s) in hex; decodes Channel Voice
//--------------------------------------------------------------------+

static void print_ump(const uint32_t* words, uint8_t wc) {
  uint8_t mt    = (uint8_t)((words[0] >> 28) & 0x0F);
  uint8_t group = (uint8_t)((words[0] >> 24) & 0x0F);

  if (mt == 0x4 && wc >= 2) {
    // MIDI 2.0 Channel Voice
    uint8_t status  = (uint8_t)((words[0] >> 20) & 0x0F);
    uint8_t channel = (uint8_t)((words[0] >> 16) & 0x0F);
    uint8_t data1   = (uint8_t)((words[0] >> 8)  & 0x7F);
    switch (status) {
      case 0x9:
        printf("[g%u ch%u] M2 NoteOn  n=%u vel=%04X attr=%04X\r\n",
               group, channel, data1,
               (unsigned)((words[1] >> 16) & 0xFFFF),
               (unsigned)(words[1] & 0xFFFF));
        break;
      case 0x8:
        printf("[g%u ch%u] M2 NoteOff n=%u vel=%04X\r\n",
               group, channel, data1,
               (unsigned)((words[1] >> 16) & 0xFFFF));
        break;
      case 0xB:
        printf("[g%u ch%u] M2 CC#%u = %08lX\r\n",
               group, channel, data1, (unsigned long)words[1]);
        break;
      case 0xC:
        printf("[g%u ch%u] M2 ProgChg %u\r\n",
               group, channel, (unsigned)((words[1] >> 24) & 0x7F));
        break;
      case 0xD:
        printf("[g%u ch%u] M2 ChanPress %08lX\r\n",
               group, channel, (unsigned long)words[1]);
        break;
      case 0xE:
        printf("[g%u ch%u] M2 PitchBend %08lX\r\n",
               group, channel, (unsigned long)words[1]);
        break;
      default:
        printf("[g%u ch%u] M2 status=0x%X w0=%08lX w1=%08lX\r\n",
               group, channel, status,
               (unsigned long)words[0], (unsigned long)words[1]);
        break;
    }
  } else if (mt == 0x2 && wc == 1) {
    // MIDI 1.0 Channel Voice
    uint8_t status = (uint8_t)((words[0] >> 16) & 0xFF);
    uint8_t data1  = (uint8_t)((words[0] >> 8)  & 0x7F);
    uint8_t data2  = (uint8_t)(words[0] & 0x7F);
    printf("[g%u] M1 %02X %02X %02X\r\n", group, status, data1, data2);
  } else {
    printf("UMP MT=0x%X wc=%u w0=%08lX\r\n",
           mt, wc, (unsigned long)words[0]);
  }
}

//--------------------------------------------------------------------+
// MIDI 2.0 Host Callbacks
//--------------------------------------------------------------------+

void tuh_midi2_descriptor_cb(uint8_t idx, const tuh_midi2_descriptor_cb_t* d) {
  (void)idx;
  printf("MIDI2 descriptor: %s, RX cables=%u TX cables=%u\r\n",
         d->protocol_version ? "MIDI 2.0" : "MIDI 1.0",
         d->rx_cable_count, d->tx_cable_count);
}

void tuh_midi2_mount_cb(uint8_t idx, const tuh_midi2_mount_cb_t* m) {
  midi2_idx = idx;
  printf("MIDI2 mounted: idx=%u protocol=%s\r\n",
         idx, m->protocol_version ? "MIDI 2.0" : "MIDI 1.0");
}

void tuh_midi2_rx_cb(uint8_t idx, uint32_t xferred_bytes) {
  (void)xferred_bytes;

  uint32_t words[16];
  while (1) {
    uint32_t n = tuh_midi2_ump_read(idx, words, 16);
    if (n == 0) break;

    uint32_t i = 0;
    while (i < n) {
      uint8_t mt = (uint8_t)((words[i] >> 28) & 0x0F);
      uint8_t wc = midi2_ump_word_count(mt);
      if (i + wc > n) break;
      print_ump(&words[i], wc);
      i += wc;
    }
  }
}

void tuh_midi2_tx_cb(uint8_t idx, uint32_t xferred_bytes) {
  (void)idx; (void)xferred_bytes;
}

void tuh_midi2_umount_cb(uint8_t idx) {
  (void)idx;
  midi2_idx = 0xFF;
  printf("MIDI2 unmounted\r\n");
}

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

int main(void) {
  board_init();

  printf("\r\nTinyUSB Host MIDI 2.0 Example\r\n");

  tusb_rhport_init_t host_init = {
    .role  = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO,
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  while (1) {
    tuh_task();
  }

  return 0;
}
