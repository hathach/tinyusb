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

/* Host example will get device descriptors of attached devices and print it out via device cdc as follows:
 *    Device 1: ID 046d:c52f SN 11223344
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

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
// Language ID: English
#define LANGUAGE_ID 0x0409

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

static bool is_print[CFG_TUH_DEVICE_MAX+1] = { 0 };
static tusb_desc_device_t descriptor_device[CFG_TUH_DEVICE_MAX+1];

static void print_utf16(uint16_t *temp_buf, size_t buf_len);
static void print_device_info(uint8_t daddr, const tusb_desc_device_t* desc_device);

void led_blinking_task(void);
void cdc_task(void);

#define cdc_printf(...)                                           \
  do {                                                            \
    char _tempbuf[256];                                           \
    char* _bufptr = _tempbuf;                                     \
    uint32_t count = (uint32_t) sprintf(_tempbuf, __VA_ARGS__);   \
    while (count > 0) {                                           \
        uint32_t wr_count = tud_cdc_write(_bufptr, count);        \
        count -= wr_count;                                        \
        _bufptr += wr_count;                                      \
        if (count > 0){                                           \
          tud_task();                                             \
          tud_cdc_write_flush();                                  \
        }                                                         \
    }                                                             \
  } while(0)

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  printf("TinyUSB Host Information -> Device CDC Example\r\n");

  // init device and host stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1) {
    tud_task(); // tinyusb device task
    tuh_task(); // tinyusb host task
    cdc_task();
    led_blinking_task();
  }

  return 0;
}

//--------------------------------------------------------------------+
// Device CDC
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

void cdc_task(void) {
  if (!tud_cdc_connected()) {
    // delay a bit otherwise we can outpace host's terminal. Linux will set LineState (DTR) then Line Coding.
    // If we send data before Linux's terminal set Line Coding, it can be ignored --> missing data with hardware test loop
    board_delay(20);
    return;
  }

  for (uint8_t daddr = 1; daddr <= CFG_TUH_DEVICE_MAX; daddr++) {
    if (tuh_mounted(daddr)) {
      if (is_print[daddr]) {
        is_print[daddr] = false;
        print_device_info(daddr, &descriptor_device[daddr]);
        tud_cdc_write_flush();
      }
    }
  }
}

//--------------------------------------------------------------------+
// Host Get device information
//--------------------------------------------------------------------+
static void print_device_info(uint8_t daddr, const tusb_desc_device_t* desc_device) {
  // Get String descriptor using Sync API
  uint16_t serial[64];
  uint16_t buf[128];
  (void) buf;

  cdc_printf("Device %u: ID %04x:%04x SN ", daddr, desc_device->idVendor, desc_device->idProduct);
  uint8_t xfer_result = tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, serial, sizeof(serial));
  if (XFER_RESULT_SUCCESS != xfer_result) {
    serial[0] = 'n';
    serial[1] = '/';
    serial[2] = 'a';
    serial[3] = 0;
  }
  print_utf16(serial, TU_ARRAY_SIZE(serial));
  cdc_printf("\r\n");

  cdc_printf("Device Descriptor:\r\n");
  cdc_printf("  bLength             %u\r\n"     , desc_device->bLength);
  cdc_printf("  bDescriptorType     %u\r\n"     , desc_device->bDescriptorType);
  cdc_printf("  bcdUSB              %04x\r\n"   , desc_device->bcdUSB);
  cdc_printf("  bDeviceClass        %u\r\n"     , desc_device->bDeviceClass);
  cdc_printf("  bDeviceSubClass     %u\r\n"     , desc_device->bDeviceSubClass);
  cdc_printf("  bDeviceProtocol     %u\r\n"     , desc_device->bDeviceProtocol);
  cdc_printf("  bMaxPacketSize0     %u\r\n"     , desc_device->bMaxPacketSize0);
  cdc_printf("  idVendor            0x%04x\r\n" , desc_device->idVendor);
  cdc_printf("  idProduct           0x%04x\r\n" , desc_device->idProduct);
  cdc_printf("  bcdDevice           %04x\r\n"   , desc_device->bcdDevice);

  cdc_printf("  iManufacturer       %u     "     , desc_device->iManufacturer);
  xfer_result = tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, buf, sizeof(buf));
  if (XFER_RESULT_SUCCESS == xfer_result) {
    print_utf16(buf, TU_ARRAY_SIZE(buf));
  }
  cdc_printf("\r\n");

  cdc_printf("  iProduct            %u     "     , desc_device->iProduct);
  xfer_result = tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, buf, sizeof(buf));
  if (XFER_RESULT_SUCCESS == xfer_result) {
    print_utf16(buf, TU_ARRAY_SIZE(buf));
  }
  cdc_printf("\r\n");

  cdc_printf("  iSerialNumber       %u     "     , desc_device->iSerialNumber);
  cdc_printf((char*)serial); // serial is already to UTF-8
  cdc_printf("\r\n");

  cdc_printf("  bNumConfigurations  %u\r\n"     , desc_device->bNumConfigurations);
}

void tuh_enum_descriptor_device_cb(uint8_t daddr, tusb_desc_device_t const* desc_device) {
  (void) daddr;
  descriptor_device[daddr] = *desc_device; // save device descriptor
}

void tuh_mount_cb(uint8_t daddr) {
  cdc_printf("mounted device %u\r\n", daddr);
  tud_cdc_write_flush();
  is_print[daddr] = true;
}

void tuh_umount_cb(uint8_t daddr) {
  cdc_printf("unmounted device %u\r\n", daddr);
  tud_cdc_write_flush();
  is_print[daddr] = false;
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < blink_interval_ms) {
    return;// not enough time
  }
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// String Descriptor Helper
//--------------------------------------------------------------------+

static void _convert_utf16le_to_utf8(const uint16_t *utf16, size_t utf16_len, uint8_t *utf8, size_t utf8_len) {
  // TODO: Check for runover.
  (void)utf8_len;
  // Get the UTF-16 length out of the data itself.

  for (size_t i = 0; i < utf16_len; i++) {
    uint16_t chr = utf16[i];
    if (chr < 0x80) {
      *utf8++ = chr & 0xffu;
    } else if (chr < 0x800) {
      *utf8++ = (uint8_t)(0xC0 | (chr >> 6 & 0x1F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
    } else {
      // TODO: Verify surrogate.
      *utf8++ = (uint8_t)(0xE0 | (chr >> 12 & 0x0F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 6 & 0x3F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t *buf, size_t len) {
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

static void print_utf16(uint16_t *temp_buf, size_t buf_len) {
  if ((temp_buf[0] & 0xff) == 0) {
    return;// empty
  }
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
  size_t utf8_len = (size_t) _count_utf8_bytes(temp_buf + 1, utf16_len);
  _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t *) temp_buf, sizeof(uint16_t) * buf_len);
  ((uint8_t*) temp_buf)[utf8_len] = '\0';

  cdc_printf((char*) temp_buf);
}
