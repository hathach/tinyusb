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

// This example runs both host and device concurrently. The USB host looks for
// any HID device with reports that are 8 bytes long and then assumes they are
// keyboard reports. It translates the keypresses of the reports to ASCII and
// transmits it over CDC to the device's host.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void cdc_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    tuh_task(); // tinyusb host task
    led_blinking_task();

    cdc_task();
  }

  return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// Host callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  (void)desc_report;
  (void)desc_len;
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
  printf("VID = %04x, PID = %04x\r\n", vid, pid);

  // Receive any report and treat it like a keyboard.
  // tuh_hid_report_received_cb() will be invoked when report is available
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

const char* numbers = "0123456789";

// Uncomment if you use colemak and need to remap keys (like @tannewt.)
// const uint8_t colemak[77] = {
//    0,  0,  0,  0,  0,  0,  0, 22,
//    9, 23,  7,  0, 24, 17,  8, 12,
//    0, 14, 28, 51,  0, 19, 21, 10,
//   15,  0,  0,  0, 13,  0,  0,  0,
//    0,  0,  0,  0,  0,  0,  0,  0,
//    0,  0,  0,  0,  0,  0,  0,  0,
//    0,  0,  0, 18,  0,  0,  0,  0,
//    0,  0,  0,  0,  0,  0,  0,  0,
//    0,  0,  0,  0,  0,  0,  0,  0,
//    0,  0,  0,  0,  0
// };

// This is the reverse mapping of the US key layout in Adafruit_CircuitPython_HID.
const char* ascii = "\0\0\0\0abcdefghijklmnopqrstuvwxyz1234567890\n\x1b\x08\t -=[]\\\x00;\'`,./\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x7f";
const char* shifted = "\0\0\0\0ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()\0\0\0\0\0_+{}|\0:\"~<>?\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
// Bitmask of pressed keys. We use the current modifier state.
uint32_t last_state[8];

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  if (len != 8 && len < 10) {
    tud_cdc_write("report len: ", 12);
    tud_cdc_write(numbers + len, 1);
    tud_cdc_write("\r\n", 2);
    tud_cdc_write_flush();
  }
  if (len != 8) {
    // Don't request a new report for a wrong sized endpoint.
    return;
  }
  uint8_t modifiers = report[0];
  bool flush = false;
  for (int i = 2; i < 8; i++) {
    if (report[i] == 0) {
      continue;
    }
    uint8_t down = report[i];
    uint32_t mask = 1 << (down % 32);
    bool was_down = (last_state[down / 32] & mask) != 0;
    // Only map keycodes 0 - 76.
    if (!was_down && down < 77) {
      const char* layer = ascii;
      // Check shift bits
      if ((modifiers & 0x22) != 0) {
        layer = shifted;
      }
      // Map the key code for Colemak layout so @tannewt can type.
      // uint8_t colemak_key_code = colemak[down];
      // if (colemak_key_code != 0) {
      //   down = colemak_key_code;
      // }
      char c = layer[down];
      if (c == '\0') {
        continue;
      }
      if (c == '\n') {
        tud_cdc_write("\r", 1);
      }
      tud_cdc_write(&c, 1);
      flush = true;
    }
  }
  if (flush) {
    tud_cdc_write_flush();
  }
  // Now update last_state
  memset(last_state, 0, 8 * sizeof(uint32_t));
  for (int i = 2; i < 8; i++) {
    if (report[i] == 0) {
      continue;
    }
    uint8_t down = report[i];
    last_state[down / 32] |= 1 << (down % 32);
  }
  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}



//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
  // connected() check for DTR bit
  // Most but not all terminal client set this when making connection
  // if ( tud_cdc_connected() )
  {
    // connected and there are data available
    if ( tud_cdc_available() )
    {
      // read datas
      char buf[64];
      uint32_t count = tud_cdc_read(buf, sizeof(buf));
      (void) count;

      // Echo back
      // Note: Skip echo by commenting out write() and write_flush()
      // for throughput test e.g
      //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
      tud_cdc_write(buf, count);
      tud_cdc_write_flush();
    }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;
  (void) rts;

  // TODO set some indicator
  if ( dtr )
  {
    // Terminal connected
  }else
  {
    // Terminal disconnected
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
