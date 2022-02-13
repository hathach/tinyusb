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

static bool tuh_hid_joystick_check_usage(uint32_t eusage)
{
  // Check outer usage
  switch(eusage) {
    case HID_RIP_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_JOYSTICK): return true;
    case HID_RIP_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_GAMEPAD): return true; // TODO not sure about this one
    default: return false;
  }
}

// TODO Test
tusb_hid_simple_joysick_t* tuh_hid_get_simple_joystick(uint8_t hid_instance, uint8_t report_id)
{
  for(uint8_t i = 0; i < HID_MAX_JOYSTICKS; ++i) {
    tusb_hid_simple_joysick_t* simple_joystick = &hid_simple_joysicks[i];
    if (simple_joystick->active && simple_joystick->hid_instance == hid_instance && simple_joystick->report_id == report_id) return simple_joystick;
  }
  return NULL;
}

void tuh_hid_free_simple_joysticks() {
  for(uint8_t i = 0; i < HID_MAX_JOYSTICKS; ++i) {
    hid_simple_joysicks[i].active = false;
  } 
}

// TODO Test
void tuh_hid_free_simple_joystick(uint8_t hid_instance) {
  for(uint8_t i = 0; i < HID_MAX_JOYSTICKS; ++i) {
    tusb_hid_simple_joysick_t* simple_joystick = &hid_simple_joysicks[i];
    if (simple_joystick->hid_instance == hid_instance) simple_joystick->active = false;
  }
}

// TODO Test
tusb_hid_simple_joysick_t* tuh_hid_allocate_simple_joystick(uint8_t hid_instance, uint8_t report_id) {
  for(uint8_t i = 0; i < HID_MAX_JOYSTICKS; ++i) {
    tusb_hid_simple_joysick_t* simple_joystick = &hid_simple_joysicks[i];
    if (!simple_joystick->active) {
      tu_memclr(simple_joystick, sizeof(tusb_hid_simple_joysick_t));
      simple_joystick->active = true;
      simple_joystick->hid_instance = hid_instance;
      simple_joystick->report_id = report_id;
    }
  }
}

// get or create
tusb_hid_simple_joysick_t* tuh_hid_obtain_simple_joystick(uint8_t hid_instance, uint8_t report_id) {
  tusb_hid_simple_joysick_t* jdata = tuh_hid_get_simple_joystick(hid_instance, report_id);
  return jdata ? jdata : tuh_hid_allocate_simple_joystick(hid_instance, report_id);
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
  const uint8_t* ri_usage_page = tuh_hid_rip_global(pstate, RI_GLOBAL_USAGE_PAGE);
  const uint8_t* ri_usage_min = tuh_hid_rip_local(pstate, RI_LOCAL_USAGE_MIN);
  const uint8_t* ri_usage_max = tuh_hid_rip_local(pstate, RI_LOCAL_USAGE_MAX);
    
  // We need to know how the size of the data
  if (ri_report_size == NULL || ri_report_count == NULL || ri_usage_page == NULL) return false;
  
  jdata->report_size = tuh_hid_ri_short_udata32(ri_report_size);
  jdata->report_count = tuh_hid_ri_short_udata32(ri_report_count);
  jdata->report_id = ri_report_id ? tuh_hid_ri_short_udata8(ri_report_id) : 0;
  jdata->logical_min = ri_logical_min ? tuh_hid_ri_short_data32(ri_logical_min) : 0;
  jdata->logical_max = ri_logical_max ? tuh_hid_ri_short_data32(ri_logical_max) : 0;
  jdata->input_flags.byte = tuh_hid_ri_short_udata8(ri_input);
  jdata->usage_page = tuh_hid_ri_short_udata32(ri_usage_page);
  jdata->usage_min = ri_usage_min ? tuh_hid_ri_short_udata32(ri_usage_min) : 0;
  jdata->usage_max = ri_usage_max ? tuh_hid_ri_short_udata32(ri_usage_max) : 0;
  jdata->usage_is_range = (ri_usage_min != NULL) && (ri_usage_max != NULL);
  
  return true;
}

void tuh_hid_joystick_process_usages(
  tuh_hid_rip_state_t *pstate,
  tuh_hid_joystick_data_t* jdata,
  uint32_t bitpos,
  uint8_t hid_instance) 
{
  if (jdata->input_flags.data_const) {
    printf("const bits %d \n", jdata->report_size * jdata->report_count);
    return;
  }
  
  // If there are no specific usages look for a range
  // TODO What is the correct behaviour if there are both?
  if (pstate->usage_count == 0 && !jdata->usage_is_range) {
    printf("no usage - skipping bits %d \n", jdata->report_size * jdata->report_count);
    return;
  }
  
  if (jdata->input_flags.data_const) {
    printf("skipping const bits %d \n", jdata->report_size * jdata->report_count);
    return;
  }

  tusb_hid_simple_joysick_t* simple_joystick = tuh_hid_obtain_simple_joystick(hid_instance, jdata->report_id);
  
  if (simple_joystick == NULL) {
    printf("Failed to allocate joystick for HID instance %d, report ID %d\n", hid_instance, jdata->report_id);
    return;
  }
  
  // TODO Naive, assumes buttons are defined in a range
  if (jdata->usage_is_range) {
    if (jdata->usage_page == HID_USAGE_PAGE_BUTTON) {
      tusb_hid_simple_buttons_t* simple_buttons = &simple_joystick->buttons;
      simple_buttons->start = bitpos;
      simple_buttons->length = jdata->report_count;
      return;
    }
  }

  for (uint8_t i = 0; i < jdata->report_count; ++i) {
    uint32_t eusage = pstate->usages[i < pstate->usage_count ? i : pstate->usage_count - 1];
    switch (eusage) {
      // Seems to be common usage for gamepads.
      // Probably needs a lot more thought...
      case HID_RIP_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_X):
      case HID_RIP_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_Y):
      case HID_RIP_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_Z):
      case HID_RIP_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_RZ):
        printf("Axis\n");
        break;
      
      case HID_RIP_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_HAT_SWITCH): {
        // HAT buttons seem a bit crazy on joypads... 
        // they appear to act like a 4 bit button array, roughly arranged to represent directions.
        // Multiple bits are set if multiple buttons are pressed.
        // There are probably 10^18 ways of describing and interpreting this item alone. Yay.
        tusb_hid_simple_buttons_t* simple_buttons = &simple_joystick->hat_buttons;
        simple_buttons->start = bitpos;
        simple_buttons->length = jdata->report_count * jdata->report_size;
        break;
      }
      default: break;
    }
     

    bitpos += jdata->report_size;
  }

}

uint8_t tuh_hid_joystick_parse_report_descriptor(uint8_t const* desc_report, uint16_t desc_len, uint8_t hid_instance) {
  if (hid_simple_joysick_count < HID_MAX_JOYSTICKS) {
    uint32_t eusage = 0;
    tuh_hid_rip_state_t pstate;
    tuh_hid_rip_init_state(&pstate, desc_report, desc_len);
    const uint8_t *ri;
    while((ri = tuh_hid_rip_next_short_item(&pstate)) != NULL)
    {
      uint8_t const type_and_tag = tuh_hid_ri_short_type_and_tag(ri);

      switch(type_and_tag)
      {
        case HID_RI_TYPE_AND_TAG(RI_TYPE_MAIN, RI_MAIN_INPUT): {
          if (tuh_hid_joystick_check_usage(eusage)) {
            // This is what we care about for the joystick

            // TODO Check the outer usage is joystick/gamepad
            // TODO Keep track of the report id, when it changes move to next joystick definition??
          }
          break;
        }
        case HID_RI_TYPE_AND_TAG(RI_TYPE_LOCAL, RI_LOCAL_USAGE): {
          if (pstate.collections_count == 0) {
            eusage = pstate.usages[pstate.usage_count - 1];
          }
          break;
        }
        default: break;
      }
    }
  }
  return hid_simple_joysick_count;
}

