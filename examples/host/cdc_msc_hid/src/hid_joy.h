/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#ifndef _TUSB_HID_JOY_H_
#define _TUSB_HID_JOY_H_

#include "tusb.h"

#ifdef __cplusplus
 extern "C" {
#endif

#define HID_MAX_JOYSTICKS 2

typedef struct {
  union TU_ATTR_PACKED
  {
    uint8_t byte;
    struct TU_ATTR_PACKED
    {
        bool is_signed    : 1;
        bool byte_aligned : 1; // TODO More efficient fetch from report if set
    };
  } flags;
  uint8_t start;
  uint8_t length;
} tusb_hid_simple_axis_t;

typedef struct {
  uint8_t start;
  uint8_t length;
} tusb_hid_simple_buttons_t;

// Very simple representation of a joystick to try and map to
// (and this will be quite tricky enough).
typedef struct {
  uint8_t instance;
  uint8_t report_id;
  tusb_hid_simple_axis_t axis_x1;
  tusb_hid_simple_axis_t axis_y1;
  tusb_hid_simple_axis_t axis_x2;
  tusb_hid_simple_axis_t axis_y2;
  tusb_hid_simple_buttons_t hat_buttons;
  tusb_hid_simple_buttons_t buttons;
} tusb_hid_simple_joysick_t;


uint8_t tuh_hid_joystick_parse_report_descriptor(uint8_t const* desc_report, uint16_t desc_len);

#endif
