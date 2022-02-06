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

#ifndef _TUSB_HID_RI_H_
#define _TUSB_HID_RI_H_

#include "tusb.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// HID Report Description Item functions
//
// Implementation intended to minimise memory footprint,
// mildly at the expense of performance.
//
// See:
//    https://www.usb.org/sites/default/files/hid1_11.pdf
//    https://eleccelerator.com/usbdescreqparser/
//    https://usb.org/sites/default/files/hut1_2.pdf
//    https://eleccelerator.com/tutorial-about-usb-hid-report-descriptors/
//--------------------------------------------------------------------+

// Get the length of a short item
uint8_t tuh_hid_ri_short_data_length(const uint8_t *ri);

// Get the type of a short item
uint8_t tuh_hid_ri_short_type(const uint8_t *ri);

// Get the tag from a short item
uint8_t tuh_hid_ri_short_tag(const uint8_t *ri);

// Test if the item is a long item
bool tuh_hid_ri_is_long(const uint8_t *ri);

// Get the short item data unsigned
uint32_t tuh_hid_ri_short_udata32(const uint8_t *ri);

// Get the short item data unsigned
uint8_t tuh_hid_ri_short_udata8(const uint8_t *ri);

// Get the short item data signed (with sign extend to uint32)
int32_t tuh_hid_ri_short_data32(const uint8_t *ri);

// Get the data length of a long item
uint8_t tuh_hid_ri_long_data_length(const uint8_t *ri);

// Get the tag from a long item
uint8_t tuh_hid_ri_long_tag(const uint8_t *ri);

// Get a pointer to the data in a long item
const uint8_t* tuh_hid_ri_long_item_data(const uint8_t *ri);

// Get the size of the item in bytes
//
// Important:
//   To prevent buffer overflow call this before accessing short/long data
//   and check the return code for eof or error.
//
//  return values:
//     0 -> eof,
//    -1 -> missing short bytes,
//    -2 -> missing long bytes
int16_t tuh_hid_ri_size(const uint8_t *ri, uint16_t l);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_HID_HOST_H_ */
