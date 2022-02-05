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

#include "hid_rip.h"
#include "hid.h"

void hidrip_init_state(tuh_hid_rip_state_t *state, const uint8_t *report, uint16_t length)
{
  state->cursor = report;
  state->length = length;
  state->item_length = 0;
  state->stack_index = 0;
  state->usage_count = 0;
  state->collections_count = 0;
  memset(&state->global_items, 0, sizeof(uint8_t*) * HID_REPORT_STACK_SIZE * 16);
  memset(&state->local_items, 0, sizeof(uint8_t*) * 16);
}

const uint8_t* hidrip_next_item(tuh_hid_rip_state_t *state) 
{
  // Already at eof
  if (state->length == 0) return NULL;
   
  const uint8_t *ri = state->cursor;
  int16_t il = state->item_length;

  // Previous error encountered so do nothing
  if (il < 0) return NULL;
  
  if (il > 0 && hidri_short_type(ri) == RI_TYPE_MAIN) {
    // Clear down local state after a main item
    memset(&state->local_items, 0, sizeof(uint8_t*) * 16);
    state->usage_count = 0;
  }
  
  // Advance to the next report item
  ri += il;
  state->cursor = ri;
  state->length -= il;
  
  // Normal eof
  if (state->length == 0) {
     state->item_length = 0;
     return NULL;
  }
  
  // Check the report item is valid
  state->item_length = il = hidri_size(ri, state->length);
  
  if (il <= 0) {
    // record error somewhere
    return NULL;
  }

  if (il > 0 && !hidri_is_long(ri)) { // For now ignore long items.
    uint8_t short_type = hidri_short_type(ri);
    uint8_t short_tag = hidri_short_tag(ri);
    switch (short_type) {
      case RI_TYPE_GLOBAL:
        state->global_items[state->stack_index][short_tag] = ri;
        switch (short_tag) {
          case RI_GLOBAL_PUSH:
            if (++state->stack_index == HID_REPORT_STACK_SIZE) {
              return NULL; // TODO enum? Stack overflow
            }
            memcpy(&state->global_items[state->stack_index], &state->global_items[state->stack_index - 1], sizeof(uint8_t*) * 16);
            break;
          case RI_GLOBAL_POP:
            if (state->stack_index-- == 0) {
              return NULL; // TODO enum? Stack underflow
            }
            break;
          default:
            break;
        }
        break;
      case RI_TYPE_LOCAL:
        switch(short_tag) {
          case RI_LOCAL_USAGE: {
            uint32_t usage = hidri_short_udata32(ri);
            if (hidri_short_data_length(ri) <= 2) {
              const uint8_t* usage_page_item = state->global_items[state->stack_index][RI_GLOBAL_USAGE_PAGE];
              uint32_t usage_page = usage_page_item ? hidri_short_udata32(usage_page_item) : 0;
              usage |= usage_page << 16;
            }
            if (state->usage_count == HID_REPORT_MAX_USAGES) {
              return NULL; // TODO enum? Max usages overflow
            }
            state->usages[state->usage_count++] = usage;
            break;
          }
          default:
            state->local_items[short_tag] = ri;
            break;
        }
        break;
      case RI_TYPE_MAIN: {
        switch(short_tag) {
          case RI_MAIN_COLLECTION: {
            if (state->collections_count == HID_REPORT_MAX_COLLECTION_DEPTH) {
              return NULL; // TODO enum? Max collections overflow
            } 
            state->collections[state->collections_count++] = ri;
            break;
          }
          case RI_MAIN_COLLECTION_END:
            if (state->collections_count-- == 0) {
              return NULL; // TODO enum? Max collections underflow
            }
            break;
          default:
            break;
        }
        break;
      }
      default:
        break;
    }
  }
  return ri;
}

const uint8_t* hidrip_global(tuh_hid_rip_state_t *state, uint8_t tag)
{
  return state->global_items[state->stack_index][tag];
}

const uint8_t* hidrip_local(tuh_hid_rip_state_t *state, uint8_t tag)
{
  return state->local_items[tag];
}

const uint8_t* hidrip_current_item(tuh_hid_rip_state_t *state)
{
  return state->cursor;
}

uint32_t hidrip_report_total_size_bits(tuh_hid_rip_state_t *state) 
{
  const uint8_t* ri_report_size = hidrip_global(state, RI_GLOBAL_REPORT_SIZE);
  const uint8_t* ri_report_count = hidrip_global(state, RI_GLOBAL_REPORT_COUNT);
  
  if (ri_report_size != NULL && ri_report_count != NULL) 
  {
    uint32_t report_size = hidri_short_udata32(ri_report_size);
    uint32_t report_count = hidri_short_udata32(ri_report_count);
    return report_size * report_count;
  }
  else
  {
    TU_LOG2("HID report parser cannot calc total report size in bits\r\n");
    return 0;
  }
}

#endif


