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

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"


//--------------------------------------------------------------------+
// Report definitions
//--------------------------------------------------------------------+

// Values of the string descriptors. Order must match the order defined by STRING_DESCRIPTOR_INDICES.
const char *STRING_DESCRIPTOR_VALUES[] = {
  (const char[]){0x09, 0x04}, // 0: Supported language is English (0x0409)
  "TinyUSB",                  // 1: Manufacturer
  "TinyUSB Device",           // 2: Product
  NULL,                       // 3: Serial number, will use unique ID from the Pi Pico board hardware
  "Config1",                  // 4: Configuration
  "Hid1",                     // 5: HID interface
  "Print1",                   // 6: Printer interface
};

uint8_t HID_REPORT_DESCRIPTOR[] = {TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))};

uint8_t CONFIG_INTERFACE_ENDPOINT_DESCRIPTOR[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // HID:
  // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
  TUD_HID_DESCRIPTOR(ITF_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(HID_REPORT_DESCRIPTOR), EPADDR_HID,
                     CFG_TUD_HID_EP_BUFSIZE, 5),

  // Printer:
  // Interface number, string index, EP Bulk Out address, EP Bulk In address, EP size
  TUD_PRINTER_DESCRIPTOR(ITF_PRINTER, 0, EPADDR_PRINTER_OUT, EPADDR_PRINTER_IN, CFG_TUD_PRINTER_EP_BUFSIZE)};

static const tusb_desc_device_t DEVICE_DESCRIPTOR = {
  .bLength         = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB          = USB_BCD,
  .bDeviceClass    = 0x00, // Define class at interface level
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor  = USB_VID,
  .idProduct = USB_PID,
  .bcdDevice = 0x0100,

  .iManufacturer = STR_MANUFACTURER,
  .iProduct      = STR_PRODUCT,
  .iSerialNumber = STR_SERIAL,

  .bNumConfigurations = 0x01
};


//--------------------------------------------------------------------+
// TinyUSB callbacks (descriptor requests)
//--------------------------------------------------------------------+

// TinyUSB GET HID REPORT DESCRIPTOR callback.
const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void)instance;
  return HID_REPORT_DESCRIPTOR;
}

// TinyUSB GET CONFIGURATION DESCRIPTOR callback.
const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return CONFIG_INTERFACE_ENDPOINT_DESCRIPTOR;
}

// TinyUSB GET DEVICE DESCRIPTOR callback.
const uint8_t *tud_descriptor_device_cb(void) {
  return (const uint8_t *)&DEVICE_DESCRIPTOR;
}

// Storage buffer array for string descriptor to be sent to host.
static uint16_t string_descriptor_buffer[STRING_DESCRIPTOR_MAX_LENGTH + 1];

// TinyUSB GET STRING DESCRIPTOR callback.
const uint16_t *tud_descriptor_string_cb(uint8_t index, [[maybe_unused]] uint16_t langid) {
  size_t utf16_string_length;

  if (index == LANGID) {
    // langid is not a string as in a series of characters: language ID code is binary, 2 bytes
    memcpy(string_descriptor_buffer + 1, STRING_DESCRIPTOR_VALUES[LANGID], 2);
    utf16_string_length = 1; // 2 bytes = 1 UTF16 word

  } else if (index == STR_SERIAL) {
    // serialnumber is generated from pi pico: see note in STRING_DESCRIPTOR_VALUES definition
    utf16_string_length = board_usb_get_serial(string_descriptor_buffer + 1, STRING_DESCRIPTOR_MAX_LENGTH);

  } else if (index < STRING_COUNT) {
    // Get adequate descriptor string
    const char *str     = STRING_DESCRIPTOR_VALUES[index];
    utf16_string_length = strlen(str);
    if (utf16_string_length > STRING_DESCRIPTOR_MAX_LENGTH) {
      utf16_string_length = STRING_DESCRIPTOR_MAX_LENGTH;
    }
    // Convert ASCII string from memory (char*) to UTF16 (for buffer),
    // store in buffer with 1 UTF16 word offset (2 bytes, for buffer header)
    for (size_t i = 0; i < utf16_string_length; i++) {
      string_descriptor_buffer[i + 1] = str[i];
    }

  } else {
    return NULL;
  }

  // Set buffer header:
  // byte 1 - buffer length in bytes (including header)
  // byte 0 - string descriptor type (0x03).
  string_descriptor_buffer[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * utf16_string_length + 2));

  return string_descriptor_buffer;
}
