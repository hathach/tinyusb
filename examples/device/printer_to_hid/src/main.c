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
// Variables controlled with USB endpoint callbacks
// -------------------------------------------------------------------+

// usb interface pointer
uint8_t printer_itf = 0;
// pendings bytes in usb endpoint buffer ; must process these bytes
size_t pending_bytes_on_usb_ep = 0;


// -------------------------------------------------------------------+
// Variables controlled locally
// -------------------------------------------------------------------+

// local data buffer (copy data from usb endpoint into this buffer) ; acts as fifo
uint8_t data_buffer[16] = {0};
// write offset for usb/printer incoming data to data_buffer
size_t data_rx_offset = 0;
// read offset for usb/hid outgoing data from data_buffer
size_t data_tx_offset = 0;
// available space in data_buffer
size_t data_available = sizeof(data_buffer);

// next keycode to send on usb/hid
uint8_t next_keycode = 0;
// next key modifiers to send on usb/hid
uint8_t next_modifiers = 0;
// whether the next usb/hid report must be NULL to release the last keystroke
uint8_t next_keycode_is_release = false;


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

  if (board_millis() - start_ms < interval_ms) {
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

// Whenever there are pendings bytes on the USB endpoint, we will pull them from the
// endpoint buffer and write then in the local data buffer. We must take care to not
// overwrite local data that is not processed yet, so we use data_buffer as a fifo.
// We do not have to take care of reading correctly from the endpoint buffer, as all
// is done well by tud_printer_n_read().
static void printer_rx_task(void) {

  if (pending_bytes_on_usb_ep == 0) {
    return;
  }

  size_t len1 = data_available;
  size_t len2 = 0;
  if (data_rx_offset + len1 > sizeof(data_buffer)) {
    len2 = len1 - (sizeof(data_buffer) - data_rx_offset);
    len1 = sizeof(data_buffer) - data_rx_offset;
  }
  uint32_t count = tud_printer_n_read(printer_itf, data_buffer + data_rx_offset, len1);
  if (len2 > 0) {
    count += tud_printer_n_read(printer_itf, data_buffer, len2);
  }

  if (count == 0) {
    return;
  }

  data_available -= count;
  pending_bytes_on_usb_ep -= count;
  data_rx_offset = (data_rx_offset + count) % sizeof(data_buffer);
}

// The HID keycodes are not binary mapped like UTF8 codes. If we want to send the
// data received as a usb/printer, we have to translate the binary data for the
// usb/hid interface. Note that the simple mapping below will be valid only for
// hosts expecting hid data from a QWERTY keyboard. Also note that only a-zA-Z0-9
// characters are converted, for simplicity of the example. Other characters are
// converted to spaces.
static void translation_task(void) {
  if (data_tx_offset != data_rx_offset || data_available == 0) {
    // If data_tx_offset and data_rx_offset have different values, then we
    // can proceed: translate, prepare for TX, and advance data_tx_offset.
    //
    // If the buffer is full (data_available == 0), then we must also
    // translate data and prepare it for TX. But the data_tx_offset and
    // data_rx_offset will have the same value, since RX caught up to TX.
    // Hence the OR.

    // Translate UTF8 to HID keystroke
    char    c = data_buffer[data_tx_offset];
    uint8_t m = 0;
    if ('a' <= c && c <= 'z') {
      c -= 'a';
      c += HID_KEY_A;
    } else if ('A' <= c && c <= 'Z') {
      c -= 'a';
      c += HID_KEY_A;
      m = KEYBOARD_MODIFIER_LEFTSHIFT;
    } else if ('1' <= c && c <= '9') {
      c -= '1';
      c += HID_KEY_1;
    } else if (c == '0') {
      c = HID_KEY_0;
    } else {
      c = HID_KEY_SPACE;
    }

    // Proceed only if there are no characters pending for TX
    if (next_keycode == 0) {
      // Prepare next keystroke with translated data
      next_keycode   = c;
      next_modifiers = m;
      // Increment read offset
      data_tx_offset += 1;
      data_tx_offset %= sizeof(data_buffer);
      data_available += 1;
    }
  }
}

int main(void) {
  board_init();
  // init device and host stack on configured roothub port
  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  board_init_after_tusb();

  while (1) {
    tud_task();         // tinyusb device task
    printer_rx_task();  // read data sent by host on our printer interface
    translation_task(); // translate printer's UTF8 to HID keycodes
    hid_tx_task();      // send data to host with our HID interface
    if (pending_bytes_on_usb_ep > 0) {
      board_led_on();
    } else {
      board_led_off();
    }
  }
}


//--------------------------------------------------------------------+
// Printer callbacks
//--------------------------------------------------------------------+

void tud_printer_rx_cb(uint8_t itf, size_t n) {
  printer_itf = itf;            // get interface from which to read endpoint buffer
  pending_bytes_on_usb_ep += n; // count pending bytes, counter must decrement when reading from the endpoint buffer
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
