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

#ifndef _TUSB_HID_HOST_UTILS_H_
#define _TUSB_HID_HOST_UTILS_H_

#include "tusb.h"
#include "class/hid/hid_rip.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Helpers for extracting values from a HID Report
//--------------------------------------------------------------------+

// Helper to get some bits from a HID report as an unsigned 32 bit number
uint32_t tuh_hid_report_bits_u32(uint8_t const* report, uint16_t start, uint16_t length);

// Helper to get some bits from a HID report as a signed 32 bit number
int32_t tuh_hid_report_bits_i32(uint8_t const* report, uint16_t start, uint16_t length);

// Helper to get some bytes from a HID report as an unsigned 32 bit number
uint32_t tuh_hid_report_bytes_u32(uint8_t const* report, uint16_t start, uint16_t length);

// Helper to get some bytes from a HID report as a signed 32 bit number
int32_t tuh_hid_report_bytes_i32(uint8_t const* report, uint16_t start, uint16_t length);

// Helper to get a value from a HID report
// Not suitable if the value can be > 2^31
// Chooses between bytewise and bitwise fetches
int32_t tuh_hid_report_i32(const uint8_t* report, uint16_t start, uint16_t length, bool is_signed);

#endif

