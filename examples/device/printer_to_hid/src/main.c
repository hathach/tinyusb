/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

// -------------------------------------------------------------------+
// Variables
// -------------------------------------------------------------------+

// next keycode to send on usb/hid
static uint8_t next_keycode = 0;
// next key modifiers to send on usb/hid
static uint8_t next_modifiers = 0;
// whether the next usb/hid report must be NULL to release the last keystroke
static bool next_keycode_is_release = false;


// -------------------------------------------------------------------+
// Tasks
// -------------------------------------------------------------------+

// Every 10ms, we will place HID data in the usb/hid endpoint, ready to
// sent to the host when required. The tasks will read the keycode in
// next_keycode and place it in the HID report, then set next_is_null
// such that the key is released by the next report. This seem to help
// stroking the same key twice when the character is repeated in the data.
static void hid_tx_task(void) {
  // Poll every 10ms
  const uint32_t  interval_ms = 10;
  static uint32_t start_ms    = 0;

  if (!tud_hid_ready()) {
    return;
  }

  if (tusb_time_millis_api() - start_ms < interval_ms) {
    return; // not enough time
  }
  start_ms += interval_ms;

  if (next_keycode_is_release || next_keycode == 0) {
    tud_hid_keyboard_report(1, 0, NULL);
    next_keycode_is_release = false;
    return;
  }

  uint8_t keycode_array[6] = {0};
  keycode_array[0]         = next_keycode;
  tud_hid_keyboard_report(1, next_modifiers, keycode_array);
  next_keycode_is_release = true;
  next_keycode            = 0;
}

// Read one byte from printer FIFO and translate to HID keycode.
// Only a-zA-Z0-9 are translated; everything else becomes space.
static void printer_to_hid_task(void) {
  if (next_keycode != 0) {
    return; // previous key not yet sent
  }

  uint8_t ch;
  if (tud_printer_read(&ch, 1) == 0) {
    return; // no data available
  }

  uint8_t m = 0;
  if ('a' <= ch && ch <= 'z') {
    ch = (uint8_t)(ch - 'a' + HID_KEY_A);
  } else if ('A' <= ch && ch <= 'Z') {
    ch = (uint8_t)(ch - 'A' + HID_KEY_A);
    m = KEYBOARD_MODIFIER_LEFTSHIFT;
  } else if ('1' <= ch && ch <= '9') {
    ch = (uint8_t)(ch - '1' + HID_KEY_1);
  } else if (ch == '0') {
    ch = HID_KEY_0;
  } else {
    ch = HID_KEY_SPACE;
  }

  next_keycode   = ch;
  next_modifiers = m;
}

int main(void) {
  board_init();
  // init device and host stack on configured roothub port
  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  board_init_after_tusb();

  while (1) {
    tud_task();            // tinyusb device task
    printer_to_hid_task(); // read printer data and translate to HID keycodes
    hid_tx_task();         // send keycodes to host via HID
  }
}


//--------------------------------------------------------------------+
// Printer callbacks
//--------------------------------------------------------------------+

void tud_printer_rx_cb(uint8_t itf) {
  (void)itf;
}

// IEEE 1284 Device ID: first 2 bytes are big-endian total length (including the 2 length bytes).
// The rest is the Device ID string using standard abbreviated keys.
static const char printer_device_id[] =
  "\x00\x34" // total length = 52 = 0x0034 (big-endian)
  "MFG:TinyUSB;"
  "MDL:Printer to HID;"
  "CMD:PS;"
  "CLS:PRINTER;";

TU_VERIFY_STATIC(sizeof(printer_device_id) - 1 == 52, "device ID length mismatch");

uint8_t const *tud_printer_get_device_id_cb(uint8_t itf) {
  (void)itf;
  return (uint8_t const *)printer_device_id;
}


//--------------------------------------------------------------------+
// HID callbacks
//--------------------------------------------------------------------+

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, const uint8_t *buffer,
                           uint16_t bufsize) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)bufsize;
  return;
}
