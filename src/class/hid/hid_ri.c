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

#include "hid_ri.h"

#if ((TUSB_OPT_HOST_ENABLED && CFG_TUH_HID) || _UNITY_TEST_)

uint8_t hidri_short_data_length(uint8_t *ri) {
  // 0 -> 0, 1 -> 1, 2 -> 2, 3 -> 4
  return (1 << (*ri & 3)) >> 1;
}

uint8_t hidri_short_type(uint8_t *ri) {
  return (*ri >> 2) & 3;
}

uint8_t hidri_short_tag(uint8_t *ri) {
  return (*ri >> 4) & 15;
}

bool hidri_is_long(uint8_t *ri) {
  return *ri == 0xfe;
}

uint32_t hidri_short_udata32(uint8_t *ri) {
  uint32_t d = 0;
  uint8_t l = hidri_short_data_length(ri++);
  for(uint8_t i = 0; i < l; ++i) d |= ((uint32_t)(*ri++)) << (i << 3);
  return d;
}

int32_t hidri_short_data32(uint8_t *ri) {
  int32_t d = 0;
  uint8_t l = hidri_short_data_length(ri++);
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

uint8_t hidri_long_data_length(uint8_t *ri) {
  return ri[1];
}

uint8_t hidri_long_tag(uint8_t *ri) {
  return ri[2];
}

uint8_t* hidri_long_item_data(uint8_t *ri) {
  return ri + 3;
}

int16_t hidri_size(uint8_t *ri, uint16_t l) {
  // Make sure there is enough room for the header
  if (l < 1) return -1;
  // Calculate the short item length
  uint16_t sl = 1 + hidri_short_data_length(ri);
  // check it fits
  if (l < sl) return -2;
  // Check if we need to worry about a long item
  if (hidri_is_long(ri)) {
    uint16_t ll = hidri_long_data_length(ri);
    uint16_t tl = sl + ll;
    if (l < tl) return -3;
    return tl;
  }
  else {
    return sl;
  }
}

#endif
