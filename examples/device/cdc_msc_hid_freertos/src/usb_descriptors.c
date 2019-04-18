/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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
 * This file is part of the TinyUSB stack.
 */

#include "tusb.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) )

//------------- Device Descriptors -------------//
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

  #if CFG_TUD_CDC
    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  #else
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
  #endif

    .bMaxPacketSize0    = CFG_TUD_ENDOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

//------------- HID Report Descriptor -------------//
enum
{
  REPORT_ID_KEYBOARD = 1,
  REPORT_ID_MOUSE
};

uint8_t const desc_hid_report[] =
{
  HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(REPORT_ID_KEYBOARD), ),
  HID_REPORT_DESC_MOUSE   ( HID_REPORT_ID(REPORT_ID_MOUSE), )
};

//------------- Configuration Descriptor -------------//
enum
{
  #if CFG_TUD_CDC
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
  #endif

  #if CFG_TUD_MSC
    ITF_NUM_MSC,
  #endif

  #if CFG_TUD_HID
    ITF_NUM_HID,
  #endif

    ITF_NUM_TOTAL
};

enum
{
  CONFIG_DESC_LEN = sizeof(tusb_desc_configuration_t) + CFG_TUD_CDC*TUD_CDC_DESC_LEN + CFG_TUD_MSC*TUD_MSC_DESC_LEN + CFG_TUD_HID*TUD_HID_DESC_LEN
};

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
  // LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
  // 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
  // Note: since CDC EP ( 1 & 2), HID (4) are spot-on, thus we only need to force
  // endpoint number for MSC to 5
  #define EPNUM_MSC   0x05
#else
  #define EPNUM_MSC   0x03
#endif

uint8_t const desc_configuration[] =
{
  // Config: self-powered with remote wakeup support, max power up to 100 mA
  TUD_CONFIG_DESCRIPTOR(ITF_NUM_TOTAL, 0, CONFIG_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

#if CFG_TUD_CDC
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, 0x81, 8, 0x02, 0x82, 64),
#endif

#if CFG_TUD_MSC
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC, 0x80 | EPNUM_MSC, 64), // highspeed 512
#endif

#if CFG_TUD_HID
  TUD_HID_DESCRIPTOR(ITF_NUM_HID, 6, HID_PROTOCOL_KEYBOARD, sizeof(desc_hid_report), 0x84, 16, 10)
#endif
};

//------------- String Descriptors -------------//
// array of pointer to string descriptors
uint16_t const * const string_desc_arr [] =
{
  // 0: is supported language = English
  TUD_DESC_STRCONV(0x0409),

  // 1: Manufacturer
  TUD_DESC_STRCONV('t', 'i', 'n', 'y', 'u', 's', 'b', '.', 'o', 'r', 'g'),

  // 2: Product
  TUD_DESC_STRCONV('t', 'i', 'n', 'y', 'u', 's', 'b', ' ', 'd', 'e', 'v', 'i', 'c', 'e'),

  // 3: Serials, should use chip ID
  TUD_DESC_STRCONV('1', '2', '3', '4', '5', '6'),

  // 4: CDC Interface
  TUD_DESC_STRCONV('t','u','s','b',' ','c','d','c'),

  // 5: MSC Interface
  TUD_DESC_STRCONV('t','u','s','b',' ','m','s','c'),

  // 6: HID
  TUD_DESC_STRCONV('t','u','s','b',' ','h','i','d')
};

// tud_desc_set is required by tinyusb stack
tud_desc_set_t tud_desc_set =
{
    .device     = &desc_device,
    .config     = desc_configuration,

    .string_arr   = (uint8_t const **) string_desc_arr,
    .string_count = sizeof(string_desc_arr)/sizeof(string_desc_arr[0]),

    .hid_report = desc_hid_report,
};
