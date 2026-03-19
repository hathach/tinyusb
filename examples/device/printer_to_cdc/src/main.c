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

/* This example demonstrates a USB Printer + CDC composite device.
 * Data received on the Printer interface is forwarded to the CDC serial port,
 * and data received on the CDC serial port is forwarded back to the Printer interface.
 *
 * To test:
 *   1. Flash the device
 *   2. Open a serial terminal on the CDC port (e.g. /dev/ttyACM0)
 *   3. Send data to the printer: echo "hello" > /dev/usb/lp0
 *   4. The data appears on the CDC serial terminal
 *   5. Type in the serial terminal to send data back through the printer TX
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

// -------------------------------------------------------------------+
// Tasks
// -------------------------------------------------------------------+

// Forward data from Printer RX to CDC TX
static void printer_to_cdc_task(void) {
  uint32_t avail = tud_printer_read_available();
  if (avail == 0 || !tud_cdc_write_available()) {
    return;
  }

  uint8_t buf[64];
  uint32_t count = tud_printer_read(buf, TU_MIN(sizeof(buf), tud_cdc_write_available()));
  if (count > 0) {
    tud_cdc_write(buf, count);
    tud_cdc_write_flush();
  }
}

// Forward data from CDC RX to Printer TX
static void cdc_to_printer_task(void) {
  uint32_t avail = tud_printer_write_available();
  if (tud_cdc_available() == 0 || avail == 0) {
    return;
  }

  uint8_t buf[64];
  uint32_t count = tud_cdc_read(buf, TU_MIN(sizeof(buf), avail));
  if (count > 0) {
    tud_printer_write(buf, count);
    tud_printer_write_flush();
  }
}

int main(void) {
  board_init();
  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  board_init_after_tusb();

  while (1) {
    tud_task();             // tinyusb device task
    printer_to_cdc_task();  // forward printer data to CDC
    cdc_to_printer_task();  // forward CDC data to printer
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
  "MDL:Printer to CDC;"
  "CMD:PS;"
  "CLS:PRINTER;";

TU_VERIFY_STATIC(sizeof(printer_device_id) - 1 == 52, "device ID length mismatch");

uint8_t const *tud_printer_get_device_id_cb(uint8_t itf) {
  (void)itf;
  return (uint8_t const *)printer_device_id;
}
