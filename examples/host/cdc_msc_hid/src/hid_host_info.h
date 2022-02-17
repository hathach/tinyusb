/*
 * The MIT License (MIT)
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

#ifndef _TUSB_HID_HOST_PARSE_H_
#define _TUSB_HID_HOST_PARSE_H_

#include "tusb.h"

#ifdef __cplusplus
 extern "C" {
#endif

// Perhaps this should be derived from other config
#define HID_HOST_MAX_SIMPLE_DEVICES 8

typedef union TU_ATTR_PACKED
{
  uint32_t combined;
  struct TU_ATTR_PACKED
  {
    uint8_t instance      :8;
    uint8_t dev_addr      :8;
    uint16_t in_use       :16;
  } elements;
} tusb_hid_host_info_key_t;

typedef struct tusb_hid_host_info {
  tusb_hid_host_info_key_t key;
  bool has_report_id;
  void (*handler)(struct tusb_hid_host_info* info, const uint8_t* report, uint8_t report_length, uint8_t report_id);
} tusb_hid_host_info_t;

tusb_hid_host_info_t* tuh_hid_get_info(uint8_t dev_addr, uint8_t instance);

tusb_hid_host_info_t* tuh_hid_allocate_info(
  uint8_t dev_addr, 
  uint8_t instance, 
  bool has_report_id,
  void (*handler)(struct tusb_hid_host_info* info, const uint8_t* report, uint8_t report_length, uint8_t report_id));

void tuh_hid_free_info(uint8_t dev_addr, uint8_t instance);

#endif

