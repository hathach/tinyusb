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

#include "tusb.h"
#include "usb_descriptors.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]  VIDEO | AUDIO | MIDI | HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
    _PID_MAP(MIDI, 3) | _PID_MAP(AUDIO, 4) | _PID_MAP(VIDEO, 5) | _PID_MAP(VENDOR, 6) )

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for Video
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
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#if defined(CFG_EXAMPLE_VIDEO_READONLY) && !defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
# if 1 == CFG_TUD_VIDEO_STREAMING_BULK
#  define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_VIDEO_CAPTURE_DESC_MJPEG_BULK_LEN)
# else
#  define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_VIDEO_CAPTURE_DESC_MJPEG_LEN)
# endif
#else
# if 1 == CFG_TUD_VIDEO_STREAMING_BULK
#  define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_VIDEO_CAPTURE_DESC_UNCOMPR_BULK_LEN)
# else
#  define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_VIDEO_CAPTURE_DESC_UNCOMPR_LEN)
# endif
#endif

#if TU_CHECK_MCU(OPT_MCU_LPC175X_6X, OPT_MCU_LPC177X_8X, OPT_MCU_LPC40XX)
  // LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
  // 0 control, 1 In, 2 Bulk, 3 Iso, 4 In, 5 Bulk etc ...
#if 1 == CFG_TUD_VIDEO_STREAMING_BULK
  #define EPNUM_VIDEO_IN    0x82
#else
  #define EPNUM_VIDEO_IN    0x83
#endif

#elif TU_CHECK_MCU(OPT_MCU_NRF5X)
  // nRF5x ISO can only be endpoint 8
  #define EPNUM_VIDEO_IN    0x88

#else
  #define EPNUM_VIDEO_IN    0x81

#endif

uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 500),

  // IAD for Video Control
#if defined(CFG_EXAMPLE_VIDEO_READONLY) && !defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
# if 1 == CFG_TUD_VIDEO_STREAMING_BULK
  TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG_BULK(4, EPNUM_VIDEO_IN,
                                          FRAME_WIDTH, FRAME_HEIGHT, FRAME_RATE,
                                          64)
# else
  TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG(4, EPNUM_VIDEO_IN,
                                     FRAME_WIDTH, FRAME_HEIGHT, FRAME_RATE,
                                     CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE)
# endif
#else
# if 1 == CFG_TUD_VIDEO_STREAMING_BULK
  TUD_VIDEO_CAPTURE_DESCRIPTOR_UNCOMPR_BULK(4, EPNUM_VIDEO_IN,
                                            FRAME_WIDTH, FRAME_HEIGHT, FRAME_RATE,
                                            64)
# else
  TUD_VIDEO_CAPTURE_DESCRIPTOR_UNCOMPR(4, EPNUM_VIDEO_IN,
                                       FRAME_WIDTH, FRAME_HEIGHT, FRAME_RATE,
                                       CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE)
# endif
#endif
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "TinyUSB",                     // 1: Manufacturer
  "TinyUSB Device",              // 2: Product
  "123456",                      // 3: Serials, should use chip ID
  "TinyUSB UVC",                 // 4: UVC Interface
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = (uint8_t) strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8 ) | (2*chr_count + 2));

  return _desc_str;
}
