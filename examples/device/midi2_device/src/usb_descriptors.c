/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
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
 */

#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "class/audio/audio.h"
#include "class/midi/midi.h"

// EP0 size as reported in the FS/HS device descriptor: a SuperSpeed-capable build sets
// CFG_TUD_ENDPOINT0_SIZE to 512, which only applies to the SS device descriptor (encoded as 2^9)
#define EP0_SIZE_FSHS   ((uint8_t)(CFG_TUD_ENDPOINT0_SIZE > 64 ? 64 : CFG_TUD_ENDPOINT0_SIZE))

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

static tusb_desc_device_t const desc_device = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = 0x00,
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,
  .bMaxPacketSize0 = EP0_SIZE_FSHS,

  .idVendor = 0xcafe,
  .idProduct = 0x4062,  // MIDI 2.0 Device
  .bcdDevice = 0x0100,

  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,

  .bNumConfigurations = 0x01
};

#if TUD_OPT_SUPER_SPEED
// SuperSpeed device descriptor: bcdUSB >= 3.0 and bMaxPacketSize0 is an exponent (2^9 = 512)
static tusb_desc_device_t const desc_device_ss = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0320,
  .bDeviceClass = 0x00,
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,
  .bMaxPacketSize0 = 9,

  .idVendor = 0xcafe,
  .idProduct = 0x4062,  // MIDI 2.0 Device
  .bcdDevice = 0x0100,

  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,

  .bNumConfigurations = 0x01
};
#endif

uint8_t const * tud_descriptor_device_cb(void) {
#if TUD_OPT_SUPER_SPEED
  if (tud_speed_get() == TUSB_SPEED_SUPER) {
    return (uint8_t const *) &desc_device_ss;
  }
#endif
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor - MIDI 2.0
//--------------------------------------------------------------------+

enum {
  ITF_NUM_MIDI2 = 0,       // Audio Control interface
  ITF_NUM_MIDI2_STREAMING,  // MIDI Streaming interface
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI2_DESC_LEN)

// Endpoint addresses
#define EPNUM_MIDI2_OUT 0x01
#if CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY
#define EPNUM_MIDI2_IN 0x82
#else
#define EPNUM_MIDI2_IN 0x81
#endif

static uint8_t const desc_fs_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // MIDI 2.0 Interface, two Group Terminal Blocks associated per direction:
  //   bulk OUT (host to device) carries the input block (ID 2)
  //   bulk IN  (device to host) carries the output block (ID 1)
  // Alt Setting 0 (MIDI 1.0)
  TUD_MIDI_DESC_HEAD(ITF_NUM_MIDI2, 0, 1),
  TUD_MIDI_DESC_JACK_DESC(1, 0),
  TUD_MIDI_DESC_EP(EPNUM_MIDI2_OUT, 64, 1),
  TUD_MIDI_JACKID_IN_EMB(1),
  TUD_MIDI_DESC_EP(EPNUM_MIDI2_IN, 64, 1),
  TUD_MIDI_JACKID_OUT_EMB(1),
  // Alt Setting 1 (UMP)
  TUD_MIDI2_DESC_ALT1_HEAD(ITF_NUM_MIDI2, 0),
  TUD_MIDI2_DESC_ALT1_EP(EPNUM_MIDI2_OUT, 64, 1, 2),
  TUD_MIDI2_DESC_ALT1_EP(EPNUM_MIDI2_IN, 64, 1, 1)
};

#if TUD_OPT_HIGH_SPEED
// high speed configuration: identical layout but bulk endpoints must report the 512-byte
// high-speed max packet size (tu_edpt_validate rejects a 64-byte bulk EP at high speed)
static uint8_t const desc_hs_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Alt Setting 0 (MIDI 1.0)
  TUD_MIDI_DESC_HEAD(ITF_NUM_MIDI2, 0, 1),
  TUD_MIDI_DESC_JACK_DESC(1, 0),
  TUD_MIDI_DESC_EP(EPNUM_MIDI2_OUT, 512, 1),
  TUD_MIDI_JACKID_IN_EMB(1),
  TUD_MIDI_DESC_EP(EPNUM_MIDI2_IN, 512, 1),
  TUD_MIDI_JACKID_OUT_EMB(1),
  // Alt Setting 1 (UMP)
  TUD_MIDI2_DESC_ALT1_HEAD(ITF_NUM_MIDI2, 0),
  TUD_MIDI2_DESC_ALT1_EP(EPNUM_MIDI2_OUT, 512, 1, 2),
  TUD_MIDI2_DESC_ALT1_EP(EPNUM_MIDI2_IN, 512, 1, 1)
};
#endif

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

#define CONFIG_SS_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_SS_DESC_LEN + TUD_MIDI2_DESC_ALT1_HEAD_LEN + TUD_MIDI2_DESC_ALT1_EP_SS_LEN(1) * 2)

// superspeed configuration
static uint8_t const desc_ss_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_SS_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_SS_TOTAL_LEN, 0x00, 96),

  // MIDI 2.0 Interface, two Group Terminal Blocks associated per direction:
  //   bulk OUT (host to device) carries the input block (ID 2)
  //   bulk IN  (device to host) carries the output block (ID 1)
  // Alt Setting 0 (MIDI 1.0): the MS header length must count the endpoint companions too
  TUD_MIDI_DESC_HEAD_EPLEN(ITF_NUM_MIDI2, 0, 1, TUD_MIDI_DESC_EP_SS_LEN(1)),
  TUD_MIDI_DESC_JACK_DESC(1, 0),
  TUD_MIDI_DESC_EP_SS(EPNUM_MIDI2_OUT, TUSB_EPSIZE_BULK_SS, 1, CFG_EXAMPLE_SS_BULK_MAXBURST),
  TUD_MIDI_JACKID_IN_EMB(1),
  TUD_MIDI_DESC_EP_SS(EPNUM_MIDI2_IN, TUSB_EPSIZE_BULK_SS, 1, CFG_EXAMPLE_SS_BULK_MAXBURST),
  TUD_MIDI_JACKID_OUT_EMB(1),
  // Alt Setting 1 (UMP)
  TUD_MIDI2_DESC_ALT1_HEAD(ITF_NUM_MIDI2, 0),
  TUD_MIDI2_DESC_ALT1_EP_SS(EPNUM_MIDI2_OUT, CFG_EXAMPLE_SS_BULK_MAXBURST, 1, 2),
  TUD_MIDI2_DESC_ALT1_EP_SS(EPNUM_MIDI2_IN, CFG_EXAMPLE_SS_BULK_MAXBURST, 1, 1)
};

// BOS descriptor: USB 2.0 extension (LPM) + SuperSpeed device capability
#define BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_USB20_EXT_DESC_LEN + TUD_BOS_SUPERSPEED_DESC_LEN)

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

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;

#if TUD_OPT_SUPER_SPEED
  if (tud_speed_get() == TUSB_SPEED_SUPER) {
    return desc_ss_configuration;
  }
#endif

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

#if TUD_OPT_HIGH_SPEED
// A device reporting bcdUSB 0x0200 must answer GET_DESCRIPTOR(DEVICE_QUALIFIER) and
// OTHER_SPEED_CONFIGURATION (USB 2.0 section 9.6.2): without these callbacks usbd's weak stubs
// return NULL and stall EP0, which a high-speed-capable device may not do.
static uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

static tusb_desc_device_qualifier_t const desc_device_qualifier = {
  .bLength = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB = 0x0200,

  .bDeviceClass = 0x00,
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,

  .bMaxPacketSize0 = EP0_SIZE_FSHS,
  .bNumConfigurations = 0x01,
  .bReserved = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
uint8_t const *tud_descriptor_device_qualifier_cb(void) {
  return (uint8_t const *) &desc_device_qualifier;
}

// Invoked when received GET OTHER SPEED CONFIGURATION DESCRIPTOR request
uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index) {
  (void) index; // for multiple configurations

  // if link speed is high return fullspeed config, and vice versa
  // Note: the descriptor type is OTHER_SPEED_CONFIG instead of CONFIG
  memcpy(desc_other_speed_config,
         (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration : desc_hs_configuration,
         CONFIG_TOTAL_LEN);
  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;
  return desc_other_speed_config;
}
#endif // TUD_OPT_HIGH_SPEED

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER = 1,
  STRID_PRODUCT = 2,
  STRID_SERIAL = 3,
};

static char const *string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 },  // 0: Language
  "TinyUSB",                      // 1: Manufacturer
  "TinyUSB MIDI 2.0",             // 2: Product
  NULL,                           // 3: Serial
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  switch ( index ) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
        return NULL;
      }

      const char *str = string_desc_arr[index];
      chr_count = strlen(str);
      const size_t max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
      if ( chr_count > max_count ) {
        chr_count = max_count;
      }

      for ( size_t i = 0; i < chr_count; i++ ) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}
