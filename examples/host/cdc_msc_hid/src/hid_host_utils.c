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
 * Test me with:
 * 
 * ceedling test:pattern[hid_host_utils]
 */

#include "hid_host_utils.h"

uint32_t tuh_hid_report_bits_u32(uint8_t const* report, uint16_t start, uint16_t length)
{
  const int16_t bit_offset_start = start & 7;
  const int16_t l = length + bit_offset_start;
  const uint8_t *p = report + (start >> 3);
  uint32_t acc = ((uint32_t)*p++) >> bit_offset_start;
  for(uint16_t i = 1; (i << 3) < l; ++i) {
    acc |= ((uint32_t)*p++) << ((i << 3) - bit_offset_start);
  }
  const uint32_t m = (((uint32_t)1) << length) - 1;
  return acc & m;
}

int32_t tuh_hid_report_bits_i32(uint8_t const* report, uint16_t start, uint16_t length)
{
  const int16_t bit_offset_start = start & 7;
  const int16_t l = length + bit_offset_start;
  const uint8_t *p = report + (start >> 3);
  uint32_t acc = ((uint32_t)*p++) >> bit_offset_start;
  for(uint16_t i = 1; (i << 3) < l; ++i) {
    acc |= ((uint32_t)*p++) << ((i << 3) - bit_offset_start);
  }
  const uint32_t lp0 = ((uint32_t)1) << (length - 1);
  const uint32_t lp1 = lp0 << 1;
  // Mask or sign extend
  return acc & lp0 ? acc | -lp1 : acc & (lp1 - 1);
}

// Helper to get some bytes from a HID report as an unsigned 32 bit number
uint32_t tuh_hid_report_bytes_u32(uint8_t const* report, uint16_t start, uint16_t length)
{
  const uint8_t *p = report + start;
  if (length == 1) return (uint32_t)*p;
  uint32_t acc = 0;
  for(uint16_t i = 0; i < length; ++i) {
    acc |= ((uint32_t)*p++) << (i << 3);
  }
  return acc;   
}

// Helper to get some bytes from a HID report as a signed 32 bit number
int32_t tuh_hid_report_bytes_i32(uint8_t const* report, uint16_t start, uint16_t length)
{
  const uint8_t *p = report + start;
  if (length == 1) return (int32_t)(int8_t)*p;
  uint32_t acc = 0;
  for(uint16_t i = 0; i < length; ++i) {
    acc |= ((uint32_t)*p++) << (i << 3);
  }
  const uint32_t lp0 = ((uint32_t)1) << ((length << 3) - 1);
  const uint32_t lp1 = lp0 << 1;
  // sign extend
  return acc & lp0 ? acc | -lp1 : acc;  
}

// Helper to get a value from a HID report
int32_t tuh_hid_report_i32(const uint8_t* report, uint16_t start, uint16_t length, bool is_signed) 
{
  if (length == 0) return 0;
  if ((start | length) & 7) {
    return is_signed ?
      tuh_hid_report_bits_i32(report, start, length):
      (int32_t)tuh_hid_report_bits_u32(report, start, length);
  }
  else {
    return is_signed ?
      tuh_hid_report_bytes_i32(report, start >> 3, length >> 3):
      (int32_t)tuh_hid_report_bytes_u32(report, start >> 3, length >> 3);   
  }
}

