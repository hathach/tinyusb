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

#ifndef _TUSB_HID_JOY_H_
#define _TUSB_HID_JOY_H_

#include "tusb.h"
#include "class/hid/hid_rip.h"

#ifdef __cplusplus
 extern "C" {
#endif

#define HID_MAX_JOYSTICKS 4

typedef union TU_ATTR_PACKED
{
  uint8_t byte;
  struct TU_ATTR_PACKED
  {
      bool data_const          : 1;
      bool array_variable      : 1;
      bool absolute_relative   : 1;
      bool nowrap_wrap         : 1;
      bool linear_nonlinear    : 1;
      bool prefered_noprefered : 1;
      bool nonull_null         : 1;
  };
} tusb_hid_ri_intput_flags_t;
  
typedef struct {
  union TU_ATTR_PACKED
  {
    uint8_t byte;
    struct TU_ATTR_PACKED
    {
        bool is_signed    : 1;
    };
  } flags;
  uint16_t start;
  uint16_t length;
  int32_t logical_min;
  int32_t logical_max;
} tusb_hid_simple_axis_t;

typedef struct {
  uint16_t start;
  uint16_t length;
} tusb_hid_simple_buttons_t;

// Very simple representation of a joystick to try and map to
// (and this will be quite tricky enough).
typedef struct {
  int32_t x1;
  int32_t y1;
  int32_t x2;
  int32_t y2;
  int32_t hat;
  uint32_t buttons;
} tusb_hid_simple_joysick_values_t;

// Simple joystick definitions and values
typedef struct {
  uint8_t hid_instance;
  uint8_t report_id;
  uint8_t report_length; // requied report length in bytes
  bool active;
  bool has_values;
  tusb_hid_simple_axis_t axis_x1;
  tusb_hid_simple_axis_t axis_y1;
  tusb_hid_simple_axis_t axis_x2;
  tusb_hid_simple_axis_t axis_y2;
  tusb_hid_simple_axis_t hat;
  tusb_hid_simple_buttons_t buttons;
  tusb_hid_simple_joysick_values_t values;
} tusb_hid_simple_joysick_t;

// Intermediate data structure used while parsing joystick HID report descriptors
typedef struct {
  uint32_t report_size; 
  uint32_t report_count;
  uint32_t logical_min;
  uint32_t logical_max;
  uint16_t usage_page;
  uint16_t usage_min;
  uint16_t usage_max;
  uint8_t report_id;
  tusb_hid_ri_intput_flags_t input_flags;
  bool usage_is_range;
} tuh_hid_joystick_data_t;

// Fetch some data from the HID parser
//
// The data fetched here may be relevant to multiple usage items
//
// returns false if obviously not of interest
bool tuh_hid_joystick_get_data(
  tuh_hid_rip_state_t *pstate,     // The current HID report parser state
  const uint8_t* ri_input,         // Pointer to the input item we have arrived at
  tuh_hid_joystick_data_t* jdata   // Data structure to complete
);

// Process the HID descriptor usages
// These are handled when an 'input' item is encountered.
void tuh_hid_joystick_process_usages(
  tuh_hid_rip_state_t *pstate,
  tuh_hid_joystick_data_t* jdata,
  uint32_t bitpos,
  uint8_t hid_instance
);

// Parse a HID report description for a joystick
uint8_t tuh_hid_joystick_parse_report_descriptor(uint8_t const* desc_report, uint16_t desc_len, uint8_t hid_instance);

// Fetch a previously allocated simple joystick
tusb_hid_simple_joysick_t* tuh_hid_get_simple_joystick(uint8_t hid_instance, uint8_t report_id);

// Free a previously allocated simple joystick
void tuh_hid_free_simple_joysticks_for_instance(uint8_t hid_instance);

// Free all previously allocated simple joysticks
void tuh_hid_free_simple_joysticks();

// Allocate a new simple joystick
tusb_hid_simple_joysick_t* tuh_hid_allocate_simple_joystick(uint8_t hid_instance, uint8_t report_id);

// If it exists, return an exisiting simple joystick, else allocate a new one
tusb_hid_simple_joysick_t* tuh_hid_obtain_simple_joystick(uint8_t hid_instance, uint8_t report_id);

// Process a HID report
// The report pointer should be advanced beyond the report ID byte.
// The length should not include the report ID byte.
// The length should be in bytes.
void tusb_hid_simple_joysick_process_report(tusb_hid_simple_joysick_t* simple_joystick, const uint8_t* report, uint8_t report_length);

// Send an axis and button report to stdout
//
// e.g.
//  hid=  0, report_id=  0, x1=   0, y1=   0, x2= 127, y2= 127, hat=F, buttons=0008
//
void tusb_hid_print_simple_joysick_report(tusb_hid_simple_joysick_t* simple_joystick);

#endif
