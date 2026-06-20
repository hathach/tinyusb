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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "bsp/board_api.h"
#include "tusb.h"

#define USB_PID           (0x4000 | ((CFG_TUD_CDC) ? (1 << 0) : 0) | ((CFG_TUD_MSC) ? (1 << 1) : 0))
#define USB_VID           0xCafe
#define USB_BCD           0x0200

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

    // IAD required for composite CDC + MSC
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

enum {
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_MSC,
  ITF_NUM_TOTAL,
};

// Place bulk endpoints on EP>=8 for MAX32690 class parts (bigger FIFO, DPB-capable).
#if CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY
  #if TU_CHECK_MCU(OPT_MCU_MAX32650, OPT_MCU_MAX32666, OPT_MCU_MAX32690, OPT_MCU_MAX78002)
    // Put bulk on EP>=8 so the 2048/4096-byte FIFOs can back double packet buffering
    #define EPNUM_CDC_NOTIF   0x81
    #define EPNUM_CDC_OUT     0x08
    #define EPNUM_CDC_IN      0x89
    #define EPNUM_MSC_OUT     0x0A
    #define EPNUM_MSC_IN      0x8B
  #else
    #define EPNUM_CDC_NOTIF   0x81
    #define EPNUM_CDC_OUT     0x02
    #define EPNUM_CDC_IN      0x83
    #define EPNUM_MSC_OUT     0x04
    #define EPNUM_MSC_IN      0x85
  #endif
#else
  #define EPNUM_CDC_NOTIF   0x81
  #define EPNUM_CDC_OUT     0x02
  #define EPNUM_CDC_IN      0x82
  #define EPNUM_MSC_OUT     0x03
  #define EPNUM_MSC_IN      0x83
#endif

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN)

static uint8_t const desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

#if TUD_OPT_HIGH_SPEED
static uint8_t const desc_hs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 512),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),
};

static uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

static tusb_desc_device_qualifier_t const desc_device_qualifier = {
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved          = 0x00,
};

uint8_t const *tud_descriptor_device_qualifier_cb(void) {
  return (uint8_t const *) &desc_device_qualifier;
}

uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index) {
  (void) index;
  memcpy(desc_other_speed_config,
         (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration : desc_hs_configuration,
         CONFIG_TOTAL_LEN);
  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;
  return desc_other_speed_config;
}
#endif

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
#if TUD_OPT_HIGH_SPEED
  return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
};

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },
    "TinyUSB",
    "Throughput",
    NULL,
    "TinyUSB CDC",
    "TinyUSB MSC",
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
      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;
      const char *str = string_desc_arr[index];
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
      if (chr_count > max_count) chr_count = max_count;
      for (size_t i = 0; i < chr_count; i++) _desc_str[1 + i] = str[i];
      break;
  }

  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}
