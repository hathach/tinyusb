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
 * ceedling test:pattern[hid_joy]
 */

#include "hid_joy.h"

static uint8_t hid_simple_joysick_count = 0;
static tusb_hid_simple_joysick_t hid_simple_joysicks[HID_MAX_JOYSTICKS];

#define HID_EUSAGE(G, L) ((G << 16) | L)

static bool tuh_hid_joystick_check_usage(uint32_t eusage) {
  // Check outer usage
  switch(eusage) {
    case HID_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_JOYSTICK): return true;
    case HID_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_GAMEPAD): return true; // TODO not sure about this one
    default: return false;
  }
}



// Fetch some data from the HID parser
//
// The data fetched here may be relevant to multiple usage items
//
// returns false if obviously not of interest
bool tuh_hid_joystick_get_data(
  tuh_hid_rip_state_t *pstate,     // The current HID report parser state
  const uint8_t* ri_input,         // Pointer to the input item we have arrived at
  tuh_hid_joystick_data_t* jdata)  // Data structure to complete
{
  const uint8_t* ri_report_size = tuh_hid_rip_global(pstate, RI_GLOBAL_REPORT_SIZE);
  const uint8_t* ri_report_count = tuh_hid_rip_global(pstate, RI_GLOBAL_REPORT_COUNT);
  const uint8_t* ri_report_id = tuh_hid_rip_global(pstate, RI_GLOBAL_REPORT_ID);
  const uint8_t* ri_logical_min = tuh_hid_rip_global(pstate, RI_GLOBAL_LOGICAL_MIN);
  const uint8_t* ri_logical_max = tuh_hid_rip_global(pstate, RI_GLOBAL_LOGICAL_MAX);

  // We need to know how the size of the data
  if (ri_report_size == NULL || ri_report_count == NULL) return false;
  
  jdata->report_size = tuh_hid_ri_short_udata32(ri_report_size);
  jdata->report_count = tuh_hid_ri_short_udata32(ri_report_count);
  jdata->report_id = ri_report_id ? tuh_hid_ri_short_udata8(ri_report_id) : 0;
  jdata->logical_min = ri_logical_min ? tuh_hid_ri_short_data32(ri_logical_min) : 0;
  jdata->logical_max = ri_logical_max ? tuh_hid_ri_short_data32(ri_logical_max) : 0;
  jdata->input_flags.byte = tuh_hid_ri_short_udata8(ri_input);
  
  return true;
}

uint8_t tuh_hid_joystick_parse_report_descriptor(uint8_t const* desc_report, uint16_t desc_len) {
  if (hid_simple_joysick_count < HID_MAX_JOYSTICKS) {
    uint32_t eusage = 0;
    tuh_hid_rip_state_t pstate;
    tuh_hid_rip_init_state(&pstate, desc_report, desc_len);
    const uint8_t *ri;
    while((ri = tuh_hid_rip_next_item(&pstate)) != NULL)
    {
      if (!tuh_hid_ri_is_long(ri)) 
      {
        uint8_t const tag  = tuh_hid_ri_short_tag(ri);
        uint8_t const type = tuh_hid_ri_short_type(ri);
        switch(type)
        {
          case RI_TYPE_MAIN: {
            switch (tag)
            {
              case RI_MAIN_INPUT: {
                if (tuh_hid_joystick_check_usage(eusage)) {
                  // This is what we care about for the joystick

                  // TODO Check the outer usage is joystick/gamepad
                  // TODO Keep track of the report id, when it changes move to next joystick definition??
                }
                break;
              }
              default: break;
            }
            break;
          }
          case RI_TYPE_LOCAL: {
            switch(tag)
            {
              case RI_LOCAL_USAGE: {
                if (pstate.collections_count == 0) {
                  eusage = pstate.usages[pstate.usage_count - 1];
                }
                break;
              }
              default: break;
            }
            break;
          }
          default: break;
        }
      }
    }
  }
  return hid_simple_joysick_count;
}

