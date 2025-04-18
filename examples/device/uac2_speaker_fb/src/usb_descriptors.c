/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 HiFiPhile
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
#include "common_types.h"

#ifdef CFG_QUIRK_OS_GUESSING
#include "quirk_os_guessing.h"
#endif

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]     AUDIO | MIDI | HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
    _PID_MAP(MIDI, 3) | _PID_MAP(AUDIO, 4) | _PID_MAP(VENDOR, 5) )

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0201,

    // Use Interface Association Descriptor (IAD) for Audio
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
#if CFG_QUIRK_OS_GUESSING
  quirk_os_guessing_desc_device_cb();
#endif
  return (uint8_t const *)&desc_device;
}

#if CFG_AUDIO_DEBUG
//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report[] =
{
  HID_USAGE_PAGE_N ( HID_USAGE_PAGE_VENDOR, 2   ),\
  HID_USAGE        ( 0x01                       ),\
  HID_COLLECTION   ( HID_COLLECTION_APPLICATION ),\
  HID_USAGE       ( 0x02                                   ),\
  HID_LOGICAL_MIN ( 0x00                                   ),\
  HID_LOGICAL_MAX_N ( 0xff, 2                              ),\
  HID_REPORT_SIZE ( 8                                      ),\
  HID_REPORT_COUNT( sizeof(audio_debug_info_t)             ),\
  HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
  HID_COLLECTION_END
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
{
  (void) itf;
  return desc_hid_report;
}
#endif

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#if CFG_AUDIO_DEBUG
  #define CONFIG_TOTAL_LEN    	(TUD_CONFIG_DESC_LEN + TUD_AUDIO_SPEAKER_STEREO_FB_DESC_LEN + TUD_HID_DESC_LEN)
#else
  #define CONFIG_TOTAL_LEN    	(TUD_CONFIG_DESC_LEN + TUD_AUDIO_SPEAKER_STEREO_FB_DESC_LEN)
#endif

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
  // LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
  // 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
  #define EPNUM_AUDIO_FB    0x03
  #define EPNUM_AUDIO_OUT   0x03
  #define EPNUM_DEBUG       0x04

#elif CFG_TUSB_MCU == OPT_MCU_NRF5X
  // ISO endpoints for NRF5x are fixed to 0x08 (0x88)
  #define EPNUM_AUDIO_FB    0x08
  #define EPNUM_AUDIO_OUT   0x08
  #define EPNUM_DEBUG       0x01

#elif defined(TUD_ENDPOINT_ONE_DIRECTION_ONLY)
  // MCUs that don't support a same endpoint number with different direction IN and OUT defined in tusb_mcu.h
  //    e.g EP1 OUT & EP1 IN cannot exist together
  #define EPNUM_AUDIO_FB    0x01
  #define EPNUM_AUDIO_OUT   0x02
  #define EPNUM_DEBUG       0x03

#else
  #define EPNUM_AUDIO_FB    0x01
  #define EPNUM_AUDIO_OUT   0x01
  #define EPNUM_DEBUG       0x02
#endif

uint8_t const desc_configuration_default[] =
{
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // Interface number, string index, byte per sample, bit per sample, EP Out, EP size, EP feedback, feedback EP size,
    TUD_AUDIO_SPEAKER_STEREO_FB_DESCRIPTOR(0, 4, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX, EPNUM_AUDIO_OUT, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX, EPNUM_AUDIO_FB | 0x80, 4),

#if CFG_AUDIO_DEBUG
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_DEBUG, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_DEBUG | 0x80, CFG_TUD_HID_EP_BUFSIZE, 7)
#endif
};

#if CFG_QUIRK_OS_GUESSING
// OS X needs 3 bytes feedback endpoint on FS
uint8_t const desc_configuration_osx_fs[] =
{
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // Interface number, string index, byte per sample, bit per sample, EP Out, EP size, EP feedback, feedback EP size,
    TUD_AUDIO_SPEAKER_STEREO_FB_DESCRIPTOR(0, 4, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX, EPNUM_AUDIO_OUT, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX, EPNUM_AUDIO_FB | 0x80, 3),

#if CFG_AUDIO_DEBUG
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_DEBUG, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_DEBUG | 0x80, CFG_TUD_HID_EP_BUFSIZE, 7)
#endif
};
#endif

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void)index; // for multiple configurations

#if CFG_QUIRK_OS_GUESSING
  quirk_os_guessing_desc_configuration_cb();
  if(tud_speed_get() == TUSB_SPEED_FULL && quirk_os_guessing_get() == QUIRK_OS_GUESSING_OSX) {
    return desc_configuration_osx_fs;
  }
#endif
  return desc_configuration_default;
}

//--------------------------------------------------------------------+
// BOS Descriptor, required for OS guessing quirk
//--------------------------------------------------------------------+

#define TUD_BOS_USB20_EXT_DESC_LEN  7

#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_USB20_EXT_DESC_LEN)

// BOS Descriptor is required for webUSB
uint8_t const desc_bos[] =
{
  // total length, number of device caps
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),

  // USB 2.0 Extension Descriptor
  0x07, TUSB_DESC_DEVICE_CAPABILITY, DEVICE_CAPABILITY_USB20_EXTENSION, 0x00, 0x00, 0x00,0x00
};

uint8_t const * tud_descriptor_bos_cb(void)
{
#if CFG_QUIRK_OS_GUESSING
  quirk_os_guessing_desc_bos_cb();
#endif
  return desc_bos;
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
char const *string_desc_arr[] =
{
  (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
  "TinyUSB",                      // 1: Manufacturer
  "TinyUSB Speaker",              // 2: Product
  NULL,                           // 3: Serials will use unique ID if possible
  "UAC2 Speaker",                 // 4: Audio Interface
};

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

#if CFG_QUIRK_OS_GUESSING
  quirk_os_guessing_desc_string_cb();
#endif

  switch ( index ) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      if ( !(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) ) return NULL;

      const char *str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
      if ( chr_count > max_count ) chr_count = max_count;

      // Convert ASCII string into UTF-16
      for ( size_t i = 0; i < chr_count; i++ ) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}
