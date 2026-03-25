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

// MIDI 2.0 Host Receiver Example
//
// Receives UMP from a MIDI 2.0 Device via PIO-USB Host.
// SSD1306 OLED (I2C, 128x64) shows boot checklist then received UMP messages.

#include <stdio.h>
#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "class/midi/midi2_host.h"
#include "class/midi/midi.h"
#include "display.h"

//--------------------------------------------------------------------+
// Checklist state
//--------------------------------------------------------------------+

static checklist_t ck = { 0 };
static uint32_t note_count = 0;
static uint8_t  midi2_idx = 0xFF;  // invalid until mount

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
// UMP Message Type names
//--------------------------------------------------------------------+

static const char* mt_name(uint8_t mt) {
  switch (mt) {
    case 0x0: return "Utility";
    case 0x1: return "System";
    case 0x2: return "M1 CVM";
    case 0x3: return "SysEx7";
    case 0x4: return "M2 CVM";
    case 0x5: return "SysEx8";
    case 0xD: return "Flex";
    case 0xF: return "Stream";
    default:  return "?";
  }
}

//--------------------------------------------------------------------+
// UMP decoder - extract and display MIDI 2.0 messages
//--------------------------------------------------------------------+

static void decode_ump(const uint32_t* words, uint8_t word_count) {
  uint8_t mt = (uint8_t)((words[0] >> 28) & 0x0F);
  char line[64];

  if (mt == 0x4 && word_count >= 2) {
    // MIDI 2.0 Channel Voice Message
    uint8_t status  = (uint8_t)((words[0] >> 20) & 0x0F);
    uint8_t channel = (uint8_t)((words[0] >> 16) & 0x0F);

    switch (status) {
      case 0x9: { // Note On
        uint8_t pitch = (uint8_t)((words[0] >> 8) & 0x7F);
        uint16_t vel  = (uint16_t)((words[1] >> 16) & 0xFFFF);
        char nn[6];
        note_name(pitch, nn, sizeof(nn));
        snprintf(line, sizeof(line), "NoteOn  %s ch%u vel=0x%04X", nn, channel, vel);
        display_log(line, 0x07E0); // green
        note_count++;
        break;
      }
      case 0x8: { // Note Off
        uint8_t pitch = (uint8_t)((words[0] >> 8) & 0x7F);
        char nn[6];
        note_name(pitch, nn, sizeof(nn));
        snprintf(line, sizeof(line), "NoteOff %s ch%u", nn, channel);
        display_log(line, 0x8410); // grey
        break;
      }
      case 0xB: { // CC
        uint8_t idx = (uint8_t)((words[0] >> 8) & 0x7F);
        snprintf(line, sizeof(line), "CC%-3u = 0x%08lX", idx, (unsigned long)words[1]);
        display_log(line, 0x001F); // blue
        break;
      }
      case 0xC: { // Program Change
        uint8_t prog = (uint8_t)((words[1] >> 24) & 0x7F);
        snprintf(line, sizeof(line), "ProgChg %u", prog);
        display_log(line, 0xFFE0); // yellow
        break;
      }
      case 0xE: { // Pitch Bend
        snprintf(line, sizeof(line), "PBend = 0x%08lX", (unsigned long)words[1]);
        display_log(line, 0xF81F); // magenta
        break;
      }
      case 0xD: { // Channel Pressure
        snprintf(line, sizeof(line), "CPress = 0x%08lX", (unsigned long)words[1]);
        display_log(line, 0xFC10); // orange
        break;
      }
      case 0xA: { // Poly Pressure
        uint8_t pitch = (uint8_t)((words[0] >> 8) & 0x7F);
        char nn[6];
        note_name(pitch, nn, sizeof(nn));
        snprintf(line, sizeof(line), "PolyP %s = 0x%08lX", nn, (unsigned long)words[1]);
        display_log(line, 0xFC10);
        break;
      }
      case 0xF: { // Per-Note Management
        uint8_t pitch = (uint8_t)((words[0] >> 8) & 0x7F);
        uint8_t flags = (uint8_t)(words[0] & 0xFF);
        snprintf(line, sizeof(line), "PN-Mgmt note=%u flags=0x%02X", pitch, flags);
        display_log(line, 0x07FF); // cyan
        break;
      }
      default: {
        snprintf(line, sizeof(line), "M2CVM status=0x%X", status);
        display_log(line, 0xFFFF);
        break;
      }
    }
  } else if (mt == 0x0) {
    // Utility (JR Timestamp, NOOP)
    uint8_t status = (uint8_t)((words[0] >> 20) & 0x0F);
    if (status == 0x2) {
      uint16_t ts = words[0] & 0xFFFF;
      snprintf(line, sizeof(line), "JR-TS = 0x%04X", ts);
      display_log(line, 0x8410);
    }
  } else {
    snprintf(line, sizeof(line), "MT=0x%X (%s) w0=0x%08lX",
             mt, mt_name(mt), (unsigned long)words[0]);
    display_log(line, 0xFFFF);
  }
}

//--------------------------------------------------------------------+
// MIDI 2.0 Host Callbacks
//--------------------------------------------------------------------+

void tuh_midi2_descriptor_cb(uint8_t idx, const tuh_midi2_descriptor_cb_t* d) {
  (void)idx;
  ck.descriptor_parsed = true;
  char line[48];

  snprintf(line, sizeof(line), "bcdMSC=0x%02X%02X proto=%s",
           d->bcdMSC_hi, d->bcdMSC_lo,
           d->protocol_version ? "MIDI2" : "MIDI1");
  display_log(line, 0x07E0);

  snprintf(line, sizeof(line), "Cables: RX=%u TX=%u",
           d->rx_cable_count, d->tx_cable_count);
  display_log(line, 0x07E0);

  display_checklist_update(&ck);
}

void tuh_midi2_mount_cb(uint8_t idx, const tuh_midi2_mount_cb_t* m) {
  midi2_idx = idx;
  ck.alt_setting_ok = true;
  ck.mounted = true;

  char line[48];
  snprintf(line, sizeof(line), "Mounted addr=%u alt=%u",
           m->daddr, m->alt_setting_active);
  display_log(line, 0x07E0);

  if (m->protocol_version) {
    display_log("MIDI 2.0 ready", 0x07E0);
  } else {
    display_log("MIDI 1.0 only", 0xFFE0);
  }

  display_checklist_update(&ck);
}

void tuh_midi2_rx_cb(uint8_t idx, uint32_t xferred_bytes) {
  (void)xferred_bytes;
  if (!ck.receiving) {
    ck.receiving = true;
    display_checklist_update(&ck);
  }

  uint32_t words[16];
  while (1) {
    uint32_t n = tuh_midi2_ump_read(idx, words, 16);
    if (n == 0) break;

    // Decode complete UMP messages
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
  ck.device_connected = false;
  ck.descriptor_parsed = false;
  ck.alt_setting_ok = false;
  ck.mounted = false;
  ck.receiving = false;
  note_count = 0;

  display_log("Device disconnected", 0xF800);
  display_checklist_update(&ck);
}

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

int main(void) {
  board_init();

  display_init();

  ck.pwr_on = true;
  display_checklist_update(&ck);

  // Init TinyUSB Host (BSP handles PIO-USB configuration via tuh_configure)
  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_FULL,
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  ck.tusb_init = true;
  ck.bus_active = true;
  display_checklist_update(&ck);

  char dbg[40];
  snprintf(dbg, sizeof(dbg), "RHPORT=%u PIO_USB=%u",
           BOARD_TUH_RHPORT, CFG_TUH_RPI_PIO_USB);
  display_log(dbg, 0xFFE0);  // yellow
  display_status("Waiting for device...");

  uint32_t last_status_ms = 0;
  static uint32_t loop_count = 0;

  while (1) {
    tuh_task();
    loop_count++;

    // Update status every 2 seconds
    uint32_t now = tusb_time_millis_api();
    if (now - last_status_ms > 2000) {
      last_status_ms = now;
      char line[22];
      if (ck.receiving) {
        snprintf(line, sizeof(line), "Notes:%lu", (unsigned long)note_count);
      } else if (ck.mounted) {
        snprintf(line, sizeof(line), "Mounted OK!");
      } else if (ck.device_connected) {
        snprintf(line, sizeof(line), "Dev found, mounting..");
      } else {
        snprintf(line, sizeof(line), "Wait.. t=%lu", (unsigned long)(now/1000));
      }
      display_status(line);
    }
  }

  return 0;
}

// Generic USB device mount/unmount (any class)
void tuh_mount_cb(uint8_t daddr) {
  char line[32];
  uint16_t vid, pid;
  tuh_vid_pid_get(daddr, &vid, &pid);
  snprintf(line, sizeof(line), "USB %04X:%04X a%u", vid, pid, daddr);
  display_log(line, 0x07E0);
  ck.device_connected = true;
  display_checklist_update(&ck);
}

void tuh_umount_cb(uint8_t daddr) {
  (void)daddr;
  display_log("USB disconnected", 0xF800);
  ck.device_connected = false;
  ck.descriptor_parsed = false;
  ck.alt_setting_ok = false;
  ck.mounted = false;
  ck.receiving = false;
  display_checklist_update(&ck);
}
