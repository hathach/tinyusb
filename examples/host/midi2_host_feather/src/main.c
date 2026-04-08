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

// MIDI 2.0 Host Example: Adafruit Feather RP2040 USB Host
//
// Receives UMP from a MIDI 2.0 Device via PIO-USB (GP16/GP17).
// SSD1306 OLED (I2C1, GP2/GP3) shows splash, spinner, then live UMP messages.

#include <stdio.h>
#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "hardware/gpio.h"
#include "class/midi/midi2_host.h"
#include "class/midi/midi.h"
#include "display.h"

//--------------------------------------------------------------------+
// State
//--------------------------------------------------------------------+

static uint32_t note_count = 0;
static uint8_t  midi2_idx = 0xFF;
static bool     device_connected = false;
static bool     mounted = false;

//--------------------------------------------------------------------+
// Note name helper
//--------------------------------------------------------------------+

static const char* NOTE_NAMES[] = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static void note_name(uint8_t pitch, char* buf, size_t len) {
  int octave = (pitch / 12) - 1;
  snprintf(buf, len, "%s%d", NOTE_NAMES[pitch % 12], octave);
}

//--------------------------------------------------------------------+
// UMP decoder
//--------------------------------------------------------------------+

static void decode_ump(const uint32_t* words, uint8_t word_count) {
  uint8_t mt = (uint8_t)((words[0] >> 28) & 0x0F);
  char line[64];

  if (mt == 0x4 && word_count >= 2) {
    uint8_t status  = (uint8_t)((words[0] >> 20) & 0x0F);
    uint8_t channel = (uint8_t)((words[0] >> 16) & 0x0F);

    switch (status) {
      case 0x9: {
        uint8_t pitch = (uint8_t)((words[0] >> 8) & 0x7F);
        uint16_t vel  = (uint16_t)((words[1] >> 16) & 0xFFFF);
        char nn[6];
        note_name(pitch, nn, sizeof(nn));
        snprintf(line, sizeof(line), "NoteOn  %s ch%u v%04X", nn, channel, vel);
        display_log(line, 0x07E0);
        note_count++;
        break;
      }
      case 0x8: {
        uint8_t pitch = (uint8_t)((words[0] >> 8) & 0x7F);
        char nn[6];
        note_name(pitch, nn, sizeof(nn));
        snprintf(line, sizeof(line), "NoteOff %s ch%u", nn, channel);
        display_log(line, 0x8410);
        break;
      }
      case 0xB: {
        uint8_t idx = (uint8_t)((words[0] >> 8) & 0x7F);
        snprintf(line, sizeof(line), "CC%-3u %08lX", idx, (unsigned long)words[1]);
        display_log(line, 0x001F);
        break;
      }
      case 0xC: {
        uint8_t prog = (uint8_t)((words[1] >> 24) & 0x7F);
        snprintf(line, sizeof(line), "ProgChg %u", prog);
        display_log(line, 0xFFE0);
        break;
      }
      case 0xE: {
        snprintf(line, sizeof(line), "PBend %08lX", (unsigned long)words[1]);
        display_log(line, 0xF81F);
        break;
      }
      case 0xD: {
        snprintf(line, sizeof(line), "CPress %08lX", (unsigned long)words[1]);
        display_log(line, 0xFC10);
        break;
      }
      case 0xA: {
        uint8_t pitch = (uint8_t)((words[0] >> 8) & 0x7F);
        char nn[6];
        note_name(pitch, nn, sizeof(nn));
        snprintf(line, sizeof(line), "PolyP %s %08lX", nn, (unsigned long)words[1]);
        display_log(line, 0xFC10);
        break;
      }
      case 0xF: {
        uint8_t pitch = (uint8_t)((words[0] >> 8) & 0x7F);
        uint8_t flags = (uint8_t)(words[0] & 0xFF);
        snprintf(line, sizeof(line), "PN-Mgmt n%u f%02X", pitch, flags);
        display_log(line, 0x07FF);
        break;
      }
      default: {
        snprintf(line, sizeof(line), "M2CVM s=0x%X", status);
        display_log(line, 0xFFFF);
        break;
      }
    }
  } else if (mt == 0x0) {
    uint8_t status = (uint8_t)((words[0] >> 20) & 0x0F);
    if (status == 0x2) {
      uint16_t ts = words[0] & 0xFFFF;
      snprintf(line, sizeof(line), "JR-TS %04X", ts);
      display_log(line, 0x8410);
    }
  } else {
    snprintf(line, sizeof(line), "MT=0x%X w0=%08lX",
             mt, (unsigned long)words[0]);
    display_log(line, 0xFFFF);
  }
}

//--------------------------------------------------------------------+
// MIDI 2.0 Host Callbacks
//--------------------------------------------------------------------+

void tuh_midi2_descriptor_cb(uint8_t idx, const tuh_midi2_descriptor_cb_t* d) {
  (void)idx;
  char line[48];
  snprintf(line, sizeof(line), "%s RX=%u TX=%u",
           d->protocol_version ? "MIDI2" : "MIDI1",
           d->rx_cable_count, d->tx_cable_count);
  display_log(line, 0x07E0);
}

void tuh_midi2_mount_cb(uint8_t idx, const tuh_midi2_mount_cb_t* m) {
  midi2_idx = idx;
  mounted = true;

  display_live_begin();

  if (m->protocol_version) {
    display_log("MIDI 2.0 ready", 0x07E0);
  } else {
    display_log("MIDI 1.0 device", 0xFFE0);
  }
  display_status("Receiving...");
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
      decode_ump(&words[i], wc);
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
  device_connected = false;
  mounted = false;
  note_count = 0;
  display_log("Disconnected", 0xF800);
  display_status("Waiting...");
}

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

int main(void) {
  board_init();

  // Enable 5V to USB-A port (Feather RP2040 USB Host: GP18)
  gpio_init(18);
  gpio_set_dir(18, GPIO_OUT);
  gpio_put(18, 1);

  display_init();
  sleep_ms(1500);

  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_FULL,
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  uint32_t last_status_ms = 0;

  while (1) {
    tuh_task();

    uint32_t now = tusb_time_millis_api();

    if (!mounted) {
      // Spinner while waiting for device
      if (now - last_status_ms > 200) {
        last_status_ms = now;
        display_connecting(now);
      }
    } else {
      // Update note count every 2 seconds
      if (now - last_status_ms > 2000) {
        last_status_ms = now;
        char line[22];
        snprintf(line, sizeof(line), "Notes: %lu", (unsigned long)note_count);
        display_status(line);
      }
    }
  }

  return 0;
}

// Generic USB device mount/unmount
void tuh_mount_cb(uint8_t daddr) {
  (void)daddr;
  device_connected = true;
}

void tuh_umount_cb(uint8_t daddr) {
  (void)daddr;
  device_connected = false;
  mounted = false;
}
