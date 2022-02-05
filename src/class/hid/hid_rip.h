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

// TODO this probably does not belong in here
#include "hid_host.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// HID Report Description Parser functions
//
// Implementation intended to minimise memory footprint,
// mildly at the expense of performance.
//
// Iterates through report items and manages state rules.
//
// See:
//    https://www.usb.org/sites/default/files/hid1_11.pdf
//    https://eleccelerator.com/usbdescreqparser/
//    https://usb.org/sites/default/files/hut1_2.pdf
//    https://eleccelerator.com/tutorial-about-usb-hid-report-descriptors/
//--------------------------------------------------------------------+

#define HID_REPORT_STACK_SIZE 10
#define HID_REPORT_MAX_USAGES 20
#define HID_REPORT_MAX_COLLECTION_DEPTH 20

typedef struct tuh_hid_rip_state {
  const uint8_t* cursor;
  uint16_t length;
  int16_t item_length;
  uint8_t stack_index;
  uint8_t usage_count;
  uint8_t collections_count;
  const uint8_t* global_items[HID_REPORT_STACK_SIZE][16];
  const uint8_t* local_items[16];
  const uint8_t* collections[HID_REPORT_MAX_COLLECTION_DEPTH];
  uint32_t usages[HID_REPORT_MAX_USAGES];
} tuh_hid_rip_state_t;

// Initialise a report item descriptor parser
void hidrip_init_state(tuh_hid_rip_state_t *state, const uint8_t *report, uint16_t length);

// Move to the next item in the report
//
// returns
//   0 for eof
//  <0 for error
//
int16_t hidrip_next_item(tuh_hid_rip_state_t *state);

// Accessor for the curren value of a global item
//
// Returns a pointer to the start of the item of null
const uint8_t* hidrip_global(tuh_hid_rip_state_t *state, uint8_t tag);

// Accessor for the curren value of a local item
//
// Returns a pointer to the start of the item of null
const uint8_t* hidrip_local(tuh_hid_rip_state_t *state, uint8_t tag);

// Returns a pointer to the start of the last parsed item
const uint8_t* hidrip_current_item(tuh_hid_rip_state_t *state);

// Return report_size * report_count
uint32_t hidrip_report_total_size_bits(tuh_hid_rip_state_t *state);


// Fetch some basic information from the HID report descriptor
//
// Experimental replacement for tuh_hid_parse_report_descriptor
// Possibly implementation in hid_host.c should just be replaced.
// Handy here for now as I can test it and not break the rest of the stack
//
// TODO this probably does not belong in here
uint8_t hidrip_parse_report_descriptor(tuh_hid_report_info_t* report_info_arr, uint8_t arr_count, uint8_t const* desc_report, uint16_t desc_len);




#endif

