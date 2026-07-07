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

#define USB_VID   0xCafe
#define USB_PID   0x4005
#define USB_BCD   0x0200

// EP0 size as reported in the FS/HS device descriptor: a SuperSpeed-capable build sets
// CFG_TUD_ENDPOINT0_SIZE to 512, which only applies to the SS device descriptor (encoded as 2^9)
#define EP0_SIZE_FSHS   (CFG_TUD_ENDPOINT0_SIZE > 64 ? 64 : CFG_TUD_ENDPOINT0_SIZE)

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
static tusb_desc_device_t const desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = USB_BCD,

  // Use Interface Association Descriptor (IAD) for CDC
  // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = EP0_SIZE_FSHS,

  .idVendor           = USB_VID,
  .idProduct          = USB_PID,
  .bcdDevice          = 0x0100,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

#if TUD_OPT_SUPER_SPEED
// SuperSpeed device descriptor: bcdUSB >= 3.0 and bMaxPacketSize0 is an exponent (2^9 = 512)
static tusb_desc_device_t const desc_device_ss = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0320,

  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = 9,

  .idVendor           = USB_VID,
  .idProduct          = USB_PID,
  .bcdDevice          = 0x0100,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};
#endif

uint8_t const *tud_descriptor_device_cb(void) {
#if TUD_OPT_SUPER_SPEED
  if (tud_speed_get() == TUSB_SPEED_SUPER) {
    return (uint8_t const *) &desc_device_ss;
  }
#endif
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

// Endpoint numbers
#if CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY
  #if TU_CHECK_MCU(OPT_MCU_MAX32650, OPT_MCU_MAX32666, OPT_MCU_MAX32690, OPT_MCU_MAX78002)
    // Put bulk on EP>=8 so the 2048/4096-byte FIFOs can back double packet buffering
    #define EPNUM_CDC_NOTIF   0x81
    #define EPNUM_CDC_OUT     0x08
    #define EPNUM_CDC_IN      0x89
    #define EPNUM_PRINTER_OUT 0x0A
    #define EPNUM_PRINTER_IN  0x8B
  #else
    #define EPNUM_CDC_NOTIF   0x81
    #define EPNUM_CDC_OUT     0x02
    #define EPNUM_CDC_IN      0x83
    #define EPNUM_PRINTER_OUT 0x04
    #define EPNUM_PRINTER_IN  0x85
  #endif
#else
  #define EPNUM_CDC_NOTIF   0x81
  #define EPNUM_CDC_OUT     0x02
  #define EPNUM_CDC_IN      0x82
  #define EPNUM_PRINTER_OUT 0x03
  #define EPNUM_PRINTER_IN  0x83
#endif

// full speed configuration
static uint8_t const desc_fs_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

  // Interface number, string index, EP Bulk Out address, EP Bulk In address, EP size
  TUD_PRINTER_DESCRIPTOR(ITF_NUM_PRINTER, 5, EPNUM_PRINTER_OUT, EPNUM_PRINTER_IN, 64),
};

#if TUD_OPT_HIGH_SPEED
// high speed configuration
static uint8_t const desc_hs_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 512),

  TUD_PRINTER_DESCRIPTOR(ITF_NUM_PRINTER, 5, EPNUM_PRINTER_OUT, EPNUM_PRINTER_IN, 512),
};

// other speed configuration
static uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
static tusb_desc_device_qualifier_t const desc_device_qualifier = {
  .bLength            = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB             = USB_BCD,

  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,

  .bMaxPacketSize0    = EP0_SIZE_FSHS,
  .bNumConfigurations = 0x01,
  .bReserved          = 0x00
};

uint8_t const *tud_descriptor_device_qualifier_cb(void) {
  return (uint8_t const *) &desc_device_qualifier;
}

uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index) {
  (void) index;

  // if link speed is high return fullspeed config, and vice versa
  memcpy(desc_other_speed_config,
         (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration : desc_hs_configuration,
         CONFIG_TOTAL_LEN);

  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

  return desc_other_speed_config;
}

#endif // TUD_OPT_HIGH_SPEED

#if TUD_OPT_SUPER_SPEED
// Bulk endpoint burst capability advertised in the endpoint companions (bMaxBurst = bursts-1).
// Must not exceed what the dcd supports (WCH CH56x: CFG_TUD_WCH_USB30_MAX_BURST)
#ifndef CFG_EXAMPLE_SS_BULK_MAXBURST
  #ifdef CFG_TUD_WCH_USB30_MAX_BURST
    #define CFG_EXAMPLE_SS_BULK_MAXBURST (CFG_TUD_WCH_USB30_MAX_BURST - 1)
  #else
    #define CFG_EXAMPLE_SS_BULK_MAXBURST 0
  #endif
#endif

// Per USB specs: SuperSpeed devices must report a BOS descriptor and every endpoint
// descriptor must be followed by an endpoint companion descriptor

#define CONFIG_SS_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_SS_DESC_LEN + TUD_PRINTER_SS_DESC_LEN)

// superspeed configuration
static uint8_t const desc_ss_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_SS_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_SS_TOTAL_LEN, 0x00, 96),

  // Interface number, string index, EP notification address and size, EP data address (out, in), bulk max burst
  TUD_CDC_SS_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, CFG_EXAMPLE_SS_BULK_MAXBURST),

  // Interface number, string index, EP Bulk Out address, EP Bulk In address, bulk max burst
  TUD_PRINTER_SS_DESCRIPTOR(ITF_NUM_PRINTER, 5, EPNUM_PRINTER_OUT, EPNUM_PRINTER_IN, CFG_EXAMPLE_SS_BULK_MAXBURST),
};

// BOS descriptor: USB 2.0 extension (LPM) + SuperSpeed device capability
#define BOS_TOTAL_LEN  (TUD_BOS_DESC_LEN + TUD_BOS_USB20_EXT_DESC_LEN + TUD_BOS_SUPERSPEED_DESC_LEN)

static uint8_t const desc_bos[] = {
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),
  // LPM capable
  TUD_BOS_USB20_EXT_DESCRIPTOR(0x00000002),
  // no LTM, HS + Gen1 SS supported, fully functional from HS, U1/U2 exit latency
  TUD_BOS_SUPERSPEED_DESCRIPTOR(0x00, 0x000C, 2, 0x0A, 0x07FF),
};

// Invoked when received GET BOS DESCRIPTOR request
uint8_t const *tud_descriptor_bos_cb(void) {
  return desc_bos;
}
#endif // superspeed

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;

#if TUD_OPT_SUPER_SPEED
  if (tud_speed_get() == TUSB_SPEED_SUPER) {
    return desc_ss_configuration;
  }
#endif

#if TUD_OPT_HIGH_SPEED
  return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_CDC,
  STRID_PRINTER,
};

static char const *string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 }, // 0: supported language is English (0x0409)
  "TinyUSB",                     // 1: Manufacturer
  "TinyUSB Device",              // 2: Product
  NULL,                          // 3: Serial, use unique ID if possible
  "TinyUSB CDC",                 // 4: CDC Interface
  "TinyUSB Printer",             // 5: Printer Interface
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
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
      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) { return NULL; }

      const char *str = string_desc_arr[index];

      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
      if (chr_count > max_count) { chr_count = max_count; }

      for (size_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}
