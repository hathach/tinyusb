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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "class/hid/hid.h"

// English
#define LANGUAGE_ID 0x0409
#define BUF_COUNT   4


tusb_desc_device_t desc_device;

uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = { 0 }; // device address that owns buffer

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);

static void print_utf16(uint16_t *temp_buf, size_t buf_len);
void print_device_descriptor(tuh_xfer_t* xfer);
void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg);

uint8_t* get_hid_buf(uint8_t daddr);
void free_hid_buf(uint8_t daddr);

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  printf("TinyUSB Bare API Example\r\n");

  // init host stack on configured roothub port
  tusb_rhport_init_t host_init = {
      .role = TUSB_ROLE_HOST,
      .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

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
  printf("Device attached, address = %d\r\n", daddr);

  // Get Device Descriptor
  // TODO: invoking control transfer now has issue with mounting hub with multiple devices attached, fix later
  tuh_descriptor_get_device(daddr, &desc_device, 18, print_device_descriptor, 0);
}

/// Invoked when device is unmounted (bus reset/unplugged)
void tuh_umount_cb(uint8_t daddr) {
  printf("Device removed, address = %d\r\n", daddr);
  free_hid_buf(daddr);
}

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
void print_device_descriptor(tuh_xfer_t *xfer) {
  if (XFER_RESULT_SUCCESS != xfer->result) {
    printf("Failed to get device descriptor\r\n");
    return;
  }

  uint8_t const daddr = xfer->daddr;

  printf("Device %u: ID %04x:%04x\r\n", daddr, desc_device.idVendor, desc_device.idProduct);
  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n"     , desc_device.bLength);
  printf("  bDescriptorType     %u\r\n"     , desc_device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n"   , desc_device.bcdUSB);
  printf("  bDeviceClass        %u\r\n"     , desc_device.bDeviceClass);
  printf("  bDeviceSubClass     %u\r\n"     , desc_device.bDeviceSubClass);
  printf("  bDeviceProtocol     %u\r\n"     , desc_device.bDeviceProtocol);
  printf("  bMaxPacketSize0     %u\r\n"     , desc_device.bMaxPacketSize0);
  printf("  idVendor            0x%04x\r\n" , desc_device.idVendor);
  printf("  idProduct           0x%04x\r\n" , desc_device.idProduct);
  printf("  bcdDevice           %04x\r\n"   , desc_device.bcdDevice);

  // Get String descriptor using Sync API
  uint16_t temp_buf[128];

  printf("  iManufacturer       %u     ", desc_device.iManufacturer);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\r\n");

  printf("  iProduct            %u     ", desc_device.iProduct);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\r\n");

  printf("  iSerialNumber       %u     ", desc_device.iSerialNumber);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\r\n");

  printf("  bNumConfigurations  %u\r\n", desc_device.bNumConfigurations);

  // Get configuration descriptor with sync API
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_configuration_sync(daddr, 0, temp_buf, sizeof(temp_buf))) {
    parse_config_descriptor(daddr, (tusb_desc_configuration_t *) temp_buf);
  }
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

// count total length of an interface
uint16_t count_interface_total_len(tusb_desc_interface_t const* desc_itf, uint8_t itf_count, uint16_t max_len);

void open_hid_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);

// simple configuration parser to open and listen to HID Endpoint IN
void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const *desc_cfg) {
  uint8_t const *desc_end = ((uint8_t const *) desc_cfg) + tu_le16toh(desc_cfg->wTotalLength);
  uint8_t const *p_desc = tu_desc_next(desc_cfg);

  // parse each interfaces
  while (p_desc < desc_end) {
    uint8_t assoc_itf_count = 1;

    // Class will always starts with Interface Association (if any) and then Interface descriptor
    if (TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc)) {
      tusb_desc_interface_assoc_t const *desc_iad = (tusb_desc_interface_assoc_t const *) p_desc;
      assoc_itf_count = desc_iad->bInterfaceCount;

      p_desc = tu_desc_next(p_desc);// next to Interface
    }

    // must be interface from now
    if (TUSB_DESC_INTERFACE != tu_desc_type(p_desc)) { return; }
    tusb_desc_interface_t const *desc_itf = (tusb_desc_interface_t const *) p_desc;

    uint16_t const drv_len = count_interface_total_len(desc_itf, assoc_itf_count, (uint16_t) (desc_end - p_desc));

    // probably corrupted descriptor
    if (drv_len < sizeof(tusb_desc_interface_t)) { return; }

    // only open and listen to HID endpoint IN
    if (desc_itf->bInterfaceClass == TUSB_CLASS_HID) {
      open_hid_interface(dev_addr, desc_itf, drv_len);
    }

    // next Interface or IAD descriptor
    p_desc += drv_len;
  }
}

uint16_t count_interface_total_len(tusb_desc_interface_t const *desc_itf, uint8_t itf_count, uint16_t max_len) {
  uint8_t const *p_desc = (uint8_t const *) desc_itf;
  uint16_t len = 0;

  while (itf_count--) {
    // Next on interface desc
    len += tu_desc_len(desc_itf);
    p_desc = tu_desc_next(p_desc);

    while (len < max_len) {
      // return on IAD regardless of itf count
      if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE_ASSOCIATION) { return len; }

      if ((tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) &&
          ((tusb_desc_interface_t const *) p_desc)->bAlternateSetting == 0) {
        break;
      }

      len += tu_desc_len(p_desc);
      p_desc = tu_desc_next(p_desc);
    }
  }

  return len;
}

//--------------------------------------------------------------------+
// HID Interface
//--------------------------------------------------------------------+

void hid_report_received(tuh_xfer_t* xfer);

void open_hid_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len) {
  // len = interface + hid + n*endpoints
  uint16_t const drv_len = (uint16_t) (sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) +
                                       desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t));

  // corrupted descriptor
  if (max_len < drv_len) { return; }

  uint8_t const *p_desc = (uint8_t const *) desc_itf;

  // HID descriptor
  p_desc = tu_desc_next(p_desc);
  tusb_hid_descriptor_hid_t const *desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  if (HID_DESC_TYPE_HID != desc_hid->bDescriptorType) { return; }

  // Endpoint descriptor
  p_desc = tu_desc_next(p_desc);
  tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) p_desc;

  for (int i = 0; i < desc_itf->bNumEndpoints; i++) {
    if (TUSB_DESC_ENDPOINT != desc_ep->bDescriptorType) { return; }

    if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
      if (!tuh_edpt_open(daddr, desc_ep)) {
        return; // skip if failed to open endpoint
      }

      uint8_t *buf = get_hid_buf(daddr);
      if (!buf) {
        return;// out of memory
      }

      tuh_xfer_t xfer = {
          .daddr = daddr,
          .ep_addr = desc_ep->bEndpointAddress,
          .buflen = 64,
          .buffer = buf,
          .complete_cb = hid_report_received,
          .user_data = (uintptr_t) buf,// since buffer is not available in callback, use user data to store the buffer
      };

      // submit transfer for this EP
      tuh_edpt_xfer(&xfer);

      printf("Listen to [dev %u: ep %02x]\r\n", daddr, desc_ep->bEndpointAddress);
    }

    p_desc = tu_desc_next(p_desc);
    desc_ep = (tusb_desc_endpoint_t const *) p_desc;
  }
}

void hid_report_received(tuh_xfer_t *xfer) {
  // Note: not all field in xfer is available for use (i.e filled by tinyusb stack) in callback to save sram
  // For instance, xfer->buffer is NULL. We have used user_data to store buffer when submitted callback
  uint8_t *buf = (uint8_t *) xfer->user_data;

  if (xfer->result == XFER_RESULT_SUCCESS) {
    printf("[dev %u: ep %02x] HID Report:", xfer->daddr, xfer->ep_addr);
    for (uint32_t i = 0; i < xfer->actual_len; i++) {
      if (i % 16 == 0) {
        printf("\r\n  ");
      }
      printf("%02X ", buf[i]);
    }
    printf("\r\n");
  }

  // continue to submit transfer, with updated buffer
  // other field remain the same
  xfer->buflen = 64;
  xfer->buffer = buf;

  tuh_edpt_xfer(xfer);
}

//--------------------------------------------------------------------+
// Buffer helper
//--------------------------------------------------------------------+

// get an buffer from pool
uint8_t *get_hid_buf(uint8_t daddr) {
  for (size_t i = 0; i < BUF_COUNT; i++) {
    if (buf_owner[i] == 0) {
      buf_owner[i] = daddr;
      return buf_pool[i];
    }
  }

  // out of memory, increase BUF_COUNT
  return NULL;
}

// free all buffer owned by device
void free_hid_buf(uint8_t daddr) {
  for (size_t i = 0; i < BUF_COUNT; i++) {
    if (buf_owner[i] == daddr) buf_owner[i] = 0;
  }
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < interval_ms) {
    return; // not enough time
  }
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state;// toggle
}

//--------------------------------------------------------------------+
// String Descriptor Helper
//--------------------------------------------------------------------+
static void _convert_utf16le_to_utf8(const uint16_t *utf16, size_t utf16_len, uint8_t *utf8, size_t utf8_len) {
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
  if ((temp_buf[0] & 0xff) == 0) return;// empty
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
  size_t utf8_len = (size_t) _count_utf8_bytes(temp_buf + 1, utf16_len);
  _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t *) temp_buf, sizeof(uint16_t) * buf_len);
  ((uint8_t *) temp_buf)[utf8_len] = '\0';

  printf("%s", (char *) temp_buf);
}
