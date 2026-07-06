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
// Device Descriptors
//--------------------------------------------------------------------+
static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00, // per-interface class
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = 0x4010,
    .bcdDevice          = 0x0100 | USBTEST_TIER,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
uint8_t const* tud_descriptor_device_cb(void) {
  return (uint8_t const*) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

// Interface must be number 0: testusb -D issues its ioctls against interface 0
enum {
  ITF_NUM_VENDOR = 0,
  ITF_NUM_TOTAL
};

// Vendor interface, Gadget-Zero style altsettings: alt 0 carries no endpoints (an
// isochronous endpoint must not claim bandwidth in the default altsetting, USB 2.0
// 5.6.3), alt 1 carries bulk + interrupt + isochronous IN/OUT. The host usbtest
// driver skips altsettings without pipes and selects alt 1 itself. No TUD_ macro
// covers this layout, hand-rolled.
#define USBTEST_DESC_LEN  (9 + 9 + 6*7)
#define USBTEST_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _bulk_mps, _intout, _intin, _int_mps, _int_interval, _isoout, _isoin, _iso_mps, _iso_interval) \
  /* alt 0: zero bandwidth, no endpoints */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, _stridx,\
  /* alt 1: full source/sink set */\
  9, TUSB_DESC_INTERFACE, _itfnum, 1, 6, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, _stridx,\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_bulk_mps), 0,\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_bulk_mps), 0,\
  7, TUSB_DESC_ENDPOINT, _intout, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_int_mps), _int_interval,\
  7, TUSB_DESC_ENDPOINT, _intin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_int_mps), _int_interval,\
  7, TUSB_DESC_ENDPOINT, _isoout, (uint8_t)(TUSB_XFER_ISOCHRONOUS | (uint8_t)(TUSB_ISO_EP_ATT_ASYNCHRONOUS)), U16_TO_U8S_LE(_iso_mps), _iso_interval,\
  7, TUSB_DESC_ENDPOINT, _isoin, (uint8_t)(TUSB_XFER_ISOCHRONOUS | (uint8_t)(TUSB_ISO_EP_ATT_ASYNCHRONOUS)), U16_TO_U8S_LE(_iso_mps), _iso_interval

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + USBTEST_DESC_LEN)

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
  // LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
  // 0 control, 1 Interrupt, 2 Bulk, 3 Iso, 4 Interrupt etc ...
  #define EPNUM_BULK_OUT 0x02
  #define EPNUM_BULK_IN  0x85
  #define EPNUM_INT_OUT  0x01
  #define EPNUM_INT_IN   0x84
  #define EPNUM_ISO_OUT  0x03
  #define EPNUM_ISO_IN   0x86

#elif CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY
  // MCUs that don't support a same endpoint number with different direction IN and OUT
  //    e.g EP1 OUT & EP1 IN cannot exist together
  #if TU_CHECK_MCU(OPT_MCU_MAX32650, OPT_MCU_MAX32666, OPT_MCU_MAX32690, OPT_MCU_MAX78002)
    // Put bulk on EP>=8 so the 2048/4096-byte FIFOs can back double packet buffering
    #define EPNUM_BULK_OUT 0x08
    #define EPNUM_BULK_IN  0x89
    #define EPNUM_INT_OUT  0x02
    #define EPNUM_INT_IN   0x83
    #define EPNUM_ISO_OUT  0x04
    #define EPNUM_ISO_IN   0x85
  #else
    #define EPNUM_BULK_OUT 0x01
    #define EPNUM_BULK_IN  0x82
    #define EPNUM_INT_OUT  0x03
    #define EPNUM_INT_IN   0x84
    #define EPNUM_ISO_OUT  0x05
    #define EPNUM_ISO_IN   0x86
  #endif

#elif CFG_TUSB_MCU == OPT_MCU_NRF5X
  // nRF5x: ISO endpoints are hardware-fixed to EP8 (ISOOUT/ISOIN)
  #define EPNUM_BULK_OUT 0x01
  #define EPNUM_BULK_IN  0x81
  #define EPNUM_INT_OUT  0x02
  #define EPNUM_INT_IN   0x82
  #define EPNUM_ISO_OUT  0x08
  #define EPNUM_ISO_IN   0x88

#else
  #define EPNUM_BULK_OUT 0x01
  #define EPNUM_BULK_IN  0x81
  #define EPNUM_INT_OUT  0x02
  #define EPNUM_INT_IN   0x82
  #define EPNUM_ISO_OUT  0x03
  #define EPNUM_ISO_IN   0x83
#endif

static uint8_t const desc_fs_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Interface number, string index, bulk out/in + mps, int out/in + mps + interval, iso out/in + mps + interval
  USBTEST_DESCRIPTOR(ITF_NUM_VENDOR, 4, EPNUM_BULK_OUT, 0x80 | EPNUM_BULK_IN, 64,
                     EPNUM_INT_OUT, 0x80 | EPNUM_INT_IN, USBTEST_INT_EP_MPS_FS, 1,
                     EPNUM_ISO_OUT, 0x80 | EPNUM_ISO_IN, USBTEST_ISO_EP_MPS_FS, 1)
};

#if TUD_OPT_HIGH_SPEED
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

static uint8_t const desc_hs_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Interface number, string index, bulk out/in + mps, int out/in + mps + interval, iso out/in + mps + interval (1 ms)
  USBTEST_DESCRIPTOR(ITF_NUM_VENDOR, 4, EPNUM_BULK_OUT, 0x80 | EPNUM_BULK_IN, 512,
                     EPNUM_INT_OUT, 0x80 | EPNUM_INT_IN, USBTEST_INT_EP_MPS_HS, 4,
                     EPNUM_ISO_OUT, 0x80 | EPNUM_ISO_IN, USBTEST_ISO_EP_MPS_HS, 4)
};

// other speed configuration
static uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
static tusb_desc_device_qualifier_t const desc_device_qualifier = {
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = 0x0200,

    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
uint8_t const* tud_descriptor_device_qualifier_cb(void) {
  return (uint8_t const*) &desc_device_qualifier;
}

// Invoked when received GET OTHER SPEED CONFIGURATION DESCRIPTOR request
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index) {
  (void) index; // for multiple configurations

  // if link speed is high return fullspeed config, and vice versa
  // Note: the descriptor type is OTHER_SPEED_CONFIG instead of CONFIG
  memcpy(desc_other_speed_config,
         (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration : desc_hs_configuration,
         CONFIG_TOTAL_LEN);

  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

  return desc_other_speed_config;
}
#endif // highspeed

// Invoked when received GET CONFIGURATION DESCRIPTOR
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// String Descriptor Index
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
};

// array of pointer to string descriptors
static char const* string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "TinyUSB",                     // 1: Manufacturer
  "TinyUSB usbtest",             // 2: Product
  NULL,                          // 3: Serials will use unique ID if possible
  "TinyUSB usbtest source/sink"  // 4: Vendor Interface
};

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  switch (index) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

      const char* str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
      if (chr_count > max_count) chr_count = max_count;

      // Convert ASCII string into UTF-16
      for (size_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}
