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
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include "bsp/board_api.h"
#include "tusb.h"

#define USB_VID 0xCafe // unassigned vendor id
#define USB_PID 0x4004 // random product id
#define USB_BCD 0x0200 // binary coded version: 2.00

// Configuration
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_PRINTER_DESC_LEN)

// HID interface endpoints
#define EPADDR_HID 0x81 // Interrupt In, MSB must be 1
// Printer interface endpoints
#define EPADDR_PRINTER_OUT 0x01 // Bulk Out, MSB must be 0
#define EPADDR_PRINTER_IN  0x82 // Bulk In, MSB must be 1

// HID report ID
#define REPORT_ID_KEYBOARD 1

// The maximum length of the string that will be sent to the host via the STRING DESCRIPTOR. Note that the
// string descriptor itself is two bytes wider than the string.
#define STRING_DESCRIPTOR_MAX_LENGTH 32

//--------------------------------------------------------------------+
// Configuration, interface, endpoint descriptors
//--------------------------------------------------------------------+

enum {
  ITF_HID,
  ITF_PRINTER,
  ITF_COUNT,
};

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

enum {
  LANGID = 0,
  STR_MANUFACTURER,
  STR_PRODUCT,
  STR_SERIAL,
  STR_CONFIGURATION,
  STR_HID_INTERFACE,
  STR_PRINTER_INTERFACE,
  STRING_COUNT,
};

#endif /* USB_DESCRIPTORS_H_ */
