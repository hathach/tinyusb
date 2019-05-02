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
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
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
uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(REPORT_ID_KEYBOARD), ),
  TUD_HID_REPORT_DESC_MOUSE   ( HID_REPORT_ID(REPORT_ID_MOUSE), )
};

//------------- Configuration Descriptor -------------//
enum
{
  ITF_NUM_HID,
  ITF_NUM_TOTAL
};

enum
{
  CONFIG_TOTAL_LEN = TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN
};

// Use Endpoint 2 instead of 1 due to NXP MCU
// LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
// 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
#define EPNUM_HID   0x01

uint8_t const desc_configuration[] =
{
  // Config: self-powered with remote wakeup support, max power up to 100 mA
  TUD_CONFIG_DESCRIPTOR(ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_PROTOCOL_KEYBOARD, sizeof(desc_hid_report), 0x84, 16, 10)
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
};

// tud_desc_set is required by tinyusb stack
tud_desc_set_t tud_desc_set =
{
  .device       = &desc_device,
  .config       = desc_configuration,

  .string_arr   = (uint8_t const **) string_desc_arr,
  .string_count = sizeof(string_desc_arr)/sizeof(string_desc_arr[0]),
  .hid_report   = desc_hid_report,
};
