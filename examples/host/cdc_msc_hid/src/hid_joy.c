/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#include "hid_joy.h"
#include "hid_rip.h"

static uint8_t hid_simple_joysick_count = 0;
static tusb_hid_simple_joysick_t hid_simple_joysicks[HID_MAX_JOYSTICKS];

#define HID_EUSAGE(G, L) ((G << 16) | L)

static bool tuh_hid_joystick_check_usage(uint32_t eusage) {
  // Check outer usage
  switch(eusage) {
    case HID_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_JOYSTICK): return true;
    case HID_EUSAGE(HID_USAGE_PAGE_DESKTOP, HID_USAGE_DESKTOP_GAMEPAD): return true;
    default: return false;
  }
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

