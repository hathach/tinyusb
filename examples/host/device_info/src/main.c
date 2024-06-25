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

/* Host example will get device descriptors of attached devices and print it out via uart/rtt (logger) as follows:
 *    Device 1: ID 046d:c52f
      Device Descriptor:
        bLength             18
        bDescriptorType     1
        bcdUSB              0200
        bDeviceClass        0
        bDeviceSubClass     0
        bDeviceProtocol     0
        bMaxPacketSize0     8
        idVendor            0x046d
        idProduct           0xc52f
        bcdDevice           2200
        iManufacturer       1     Logitech
        iProduct            2     USB Receiver
        iSerialNumber       0
        bNumConfigurations  1
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

// English
#define LANGUAGE_ID 0x0409

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);
static void print_utf16(uint16_t* temp_buf, size_t buf_len);

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  printf("TinyUSB Device Info Example\r\n");

  // init host stack on configured roothub port
  tuh_init(BOARD_TUH_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1) {
    // tinyusb host task
    tuh_task();
    led_blinking_task();
  }

  return 0;
}

/*------------- TinyUSB Callbacks -------------*/

// Invoked when device is mounted (configured)
void tuh_mount_cb(uint8_t daddr) {
  // Get Device Descriptor
  tusb_desc_device_t desc_device;
  uint8_t xfer_result = tuh_descriptor_get_device_sync(daddr, &desc_device, 18);
  if (XFER_RESULT_SUCCESS != xfer_result) {
    printf("Failed to get device descriptor\r\n");
    return;
  }

  uint16_t buf[256];

  printf("Device %u: ID %04x:%04x\r\n", daddr, desc_device.idVendor, desc_device.idProduct);
  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n", desc_device.bLength);
  printf("  bDescriptorType     %u\r\n", desc_device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n", desc_device.bcdUSB);
  printf("  bDeviceClass        %u\r\n", desc_device.bDeviceClass);
  printf("  bDeviceSubClass     %u\r\n", desc_device.bDeviceSubClass);
  printf("  bDeviceProtocol     %u\r\n", desc_device.bDeviceProtocol);
  printf("  bMaxPacketSize0     %u\r\n", desc_device.bMaxPacketSize0);
  printf("  idVendor            0x%04x\r\n", desc_device.idVendor);
  printf("  idProduct           0x%04x\r\n", desc_device.idProduct);
  printf("  bcdDevice           %04x\r\n", desc_device.bcdDevice);

  // Get String descriptor using Sync API

  printf("  iManufacturer       %u     ", desc_device.iManufacturer);
  xfer_result = tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, buf, sizeof(buf));
  if (XFER_RESULT_SUCCESS == xfer_result) {
    print_utf16(buf, TU_ARRAY_SIZE(buf));
  }
  printf("\r\n");

  printf("  iProduct            %u     ", desc_device.iProduct);
  xfer_result = tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, buf, sizeof(buf));
  if (XFER_RESULT_SUCCESS == xfer_result) {
    print_utf16(buf, TU_ARRAY_SIZE(buf));
  }
  printf("\r\n");

  printf("  iSerialNumber       %u     ", desc_device.iSerialNumber);
  xfer_result = tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, buf, sizeof(buf));
  if (XFER_RESULT_SUCCESS == xfer_result) {
    print_utf16(buf, TU_ARRAY_SIZE(buf));
  }
  printf("\r\n");

  printf("  bNumConfigurations  %u\r\n", desc_device.bNumConfigurations);
}

/// Invoked when device is unmounted (bus reset/unplugged)
void tuh_umount_cb(uint8_t daddr) {
  printf("Device removed, address = %d\r\n", daddr);
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// String Descriptor Helper
//--------------------------------------------------------------------+

static void _convert_utf16le_to_utf8(const uint16_t* utf16, size_t utf16_len, uint8_t* utf8, size_t utf8_len) {
  // TODO: Check for runover.
  (void) utf8_len;
  // Get the UTF-16 length out of the data itself.

  for (size_t i = 0; i < utf16_len; i++) {
    uint16_t chr = utf16[i];
    if (chr < 0x80) {
      *utf8++ = chr & 0xffu;
    } else if (chr < 0x800) {
      *utf8++ = (uint8_t) (0xC0 | (chr >> 6 & 0x1F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 0 & 0x3F));
    } else {
      // TODO: Verify surrogate.
      *utf8++ = (uint8_t) (0xE0 | (chr >> 12 & 0x0F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 6 & 0x3F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 0 & 0x3F));
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t* buf, size_t len) {
  size_t total_bytes = 0;
  for (size_t i = 0; i < len; i++) {
    uint16_t chr = buf[i];
    if (chr < 0x80) {
      total_bytes += 1;
    } else if (chr < 0x800) {
      total_bytes += 2;
    } else {
      total_bytes += 3;
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
  return (int) total_bytes;
}

static void print_utf16(uint16_t* temp_buf, size_t buf_len) {
  if ((temp_buf[0] & 0xff) == 0) return;  // empty
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
  size_t utf8_len = (size_t) _count_utf8_bytes(temp_buf + 1, utf16_len);
  _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t*) temp_buf, sizeof(uint16_t) * buf_len);
  ((uint8_t*) temp_buf)[utf8_len] = '\0';

  printf("%s", (char*) temp_buf);
}
