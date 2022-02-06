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
 * This file is part of the TinyUSB stack.
 */
#include "tusb_option.h"

#if ((TUSB_OPT_HOST_ENABLED && CFG_TUH_HID) || _UNITY_TEST_)

#include "hid_ri.h"

uint8_t tuh_hid_ri_short_data_length(const uint8_t *ri) {
  // 0 -> 0, 1 -> 1, 2 -> 2, 3 -> 4
  return (1 << (*ri & 3)) >> 1;
}

uint8_t tuh_hid_ri_short_type(const uint8_t *ri) {
  return (*ri >> 2) & 3;
}

uint8_t tuh_hid_ri_short_tag(const uint8_t *ri) {
  return (*ri >> 4) & 15;
}

bool tuh_hid_ri_is_long(const uint8_t *ri) {
  return *ri == 0xfe;
}

uint32_t tuh_hid_ri_short_udata32(const uint8_t *ri) {
  uint32_t d = 0;
  uint8_t l = tuh_hid_ri_short_data_length(ri++);
  for(uint8_t i = 0; i < l; ++i) d |= ((uint32_t)(*ri++)) << (i << 3);
  return d;
}

uint8_t tuh_hid_ri_short_udata8(const uint8_t *ri) {
  return tuh_hid_ri_short_data_length(ri) > 0 ? ri[1] : 0;
}

int32_t tuh_hid_ri_short_data32(const uint8_t *ri) {
  int32_t d = 0;
  uint8_t l = tuh_hid_ri_short_data_length(ri++);
  bool negative = false;
  for(uint8_t i = 0; i < 4; ++i) {
    if (i < l) {
      uint32_t b = *ri++;
      d |= b << (i << 3);
      negative = ((b >> 7) == 1);
    }
    else if (negative) {
      d |= 0xff << (i << 3);
    }
  }
  return d;
}

uint8_t tuh_hid_ri_long_data_length(const uint8_t *ri) {
  return ri[1];
}

uint8_t tuh_hid_ri_long_tag(const uint8_t *ri) {
  return ri[2];
}

const uint8_t* tuh_hid_ri_long_item_data(const uint8_t *ri) {
  return ri + 3;
}

int16_t tuh_hid_ri_size(const uint8_t *ri, uint16_t l) {
  // Make sure there is enough room for the header
  if (l < 1) return 0;
  // Calculate the short item length
  uint16_t sl = 1 + tuh_hid_ri_short_data_length(ri);
  // check it fits
  if (l < sl) return -1;
  // Check if we need to worry about a long item
  if (tuh_hid_ri_is_long(ri)) {
    uint16_t ll = tuh_hid_ri_long_data_length(ri);
    uint16_t tl = sl + ll;
    if (l < tl) return -2;
    return tl;
  }
  else {
    return sl;
  }
}

#endif
