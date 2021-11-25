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

enum
{
  VENDOR_REQUEST_WEBUSB = 1,
  VENDOR_REQUEST_MICROSOFT = 2
};

extern uint8_t const desc_ms_os_20[];

enum
{
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_VENDOR,
  ITF_NUM_KEYBOARD,
  ITF_NUM_MOUSE,
  ITF_NUM_TOTAL
};

enum
{
  HID_INSTANCE_KEYBOARD = 0,
  HID_INSTANCE_MOUSE,
};

enum
{
  CDC_INSTANCE_1 = 0,
};

enum
{
  VENDOR_INSTANCE_1 = 0,
};


#endif /* USB_DESCRIPTORS_H_ */
