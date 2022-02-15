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
#include "hid.h"
#include "tusb_compiler.h"

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
// Expected usage:
//
//    tuh_hid_rip_state_t pstate;
//    tuh_hid_rip_init_state(&pstate, desc_report, desc_len);
//    const uint8_t *ri;
//    while((ri = tuh_hid_rip_next_item(&pstate)) != NULL)
//    {
//       // ri points to the current hid report item
//       ...
//    }
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

#define HID_RIP_EUSAGE(G, L) ((G << 16) | L)


typedef enum tuh_hid_rip_status {
  HID_RIP_INIT = 0,              // Initial state
  HID_RIP_EOF,                   // No more items
  HID_RIP_TTEM_OK,               // Last item parsed ok
  HID_RIP_ITEM_ERR,              // Issue decoding a single report item
  HID_RIP_STACK_OVERFLOW,        // Too many pushes
  HID_RIP_STACK_UNDERFLOW,       // Too many pops
  HID_RIP_USAGES_OVERFLOW,       // Too many usages
  HID_RIP_COLLECTIONS_OVERFLOW,  // Too many collections
  HID_RIP_COLLECTIONS_UNDERFLOW  // More collection ends than starts
} tuh_hid_rip_status_t;

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
  tuh_hid_rip_status_t status;
} tuh_hid_rip_state_t;


typedef struct
{
  uint8_t  report_id;
  uint16_t usage;
  uint16_t usage_page;

  // TODO still use the endpoint size for now
  // Note: these currently do not include the Report ID byte
  uint16_t in_len;      // length of IN report in bits
  uint16_t out_len;     // length of OUT report in bits
} tuh_hid_report_info_t;


// Initialise a report item descriptor parser
void tuh_hid_rip_init_state(tuh_hid_rip_state_t *state, const uint8_t *report, uint16_t length);

// Move to the next item in the report
//
// returns pointer to next item or null
//
const uint8_t* tuh_hid_rip_next_item(tuh_hid_rip_state_t *state);

// Move to the next short item in the report
//
// returns pointer to next item or null
//
const uint8_t* tuh_hid_rip_next_short_item(tuh_hid_rip_state_t *state);

// Accessor for the current value of a global item
//
// Returns a pointer to the start of the item of null
const uint8_t* tuh_hid_rip_global(tuh_hid_rip_state_t *state, uint8_t tag);

// Accessor for the current value of a local item
//
// Returns a pointer to the start of the item of null
const uint8_t* tuh_hid_rip_local(tuh_hid_rip_state_t *state, uint8_t tag);

// Returns a pointer to the start of the item or NULL for eof
const uint8_t* tuh_hid_rip_current_item(tuh_hid_rip_state_t *state);

// Return report_size * report_count
// 
// Note: this currently does not include the Report ID byte
uint32_t tuh_hid_rip_report_total_size_bits(tuh_hid_rip_state_t *state);

//--------------------------------------------------------------------+
// Report Descriptor Parser
//
// Parse report descriptor into array of report_info struct and return number of reports.
// For complicated report, application should write its own parser.
//--------------------------------------------------------------------+
uint8_t tuh_hid_parse_report_descriptor(tuh_hid_report_info_t* reports_info_arr, uint8_t arr_count, uint8_t const* desc_report, uint16_t desc_len) TU_ATTR_UNUSED;

#endif

