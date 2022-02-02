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

#ifndef _TUSB_HID_RIP_H_
#define _TUSB_HID_RIP_H_

#include "tusb.h"
#include "hid_ri.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// HID Report Description Parser functions
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

#define HID_REPORT_STACK_SIZE 10
#define HID_REPORT_MAX_USAGES 20

struct tuh_hid_rip_state {
  uint8_t stack_index;
  uint8_t usage_index;
  uint8_t *global_items[HID_REPORT_STACK_SIZE][16];
  uint8_t *local_items[16];
  uint8_t *usages[HID_REPORT_MAX_USAGES];
};

void hidrip_init_state(struct tuh_hid_rip_state *state);

#endif

