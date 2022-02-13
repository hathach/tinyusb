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

void tuh_hid_rip_init_state(tuh_hid_rip_state_t *state, const uint8_t *report, uint16_t length)
{
  state->cursor = report;
  state->length = length;
  state->item_length = 0;
  state->stack_index = 0;
  state->usage_count = 0;
  state->collections_count = 0;
  tu_memclr(&state->global_items, sizeof(uint8_t*) * HID_REPORT_STACK_SIZE * 16);
  tu_memclr(&state->local_items, sizeof(uint8_t*) * 16);
  state->status = HID_RIP_INIT;
}

const uint8_t* tuh_hid_rip_next_item(tuh_hid_rip_state_t *state) 
{
  // Already at eof
  if (state->length == 0) return NULL;
   
  const uint8_t *ri = state->cursor;
  int16_t il = state->item_length;

  // Previous error encountered so do nothing
  if (il < 0) return NULL;
  
  if (il > 0 && tuh_hid_ri_short_type(ri) == RI_TYPE_MAIN) {
    // Clear down local state after a main item
    tu_memclr(&state->local_items, sizeof(uint8_t*) * 16);
    state->usage_count = 0;
  }
  
  // Advance to the next report item
  ri += il;
  state->cursor = ri;
  state->length -= il;
  
  // Normal eof
  if (state->length == 0) {
     state->item_length = 0;
     state->status = HID_RIP_EOF;
     return NULL;
  }
  
  // Check the report item is valid
  state->item_length = il = tuh_hid_ri_size(ri, state->length);
  
  if (il <= 0) {
    state->status = HID_RIP_ITEM_ERR;
    TU_LOG2("HID Report item parser: Attempt to read HID Report item returned %d\r\n", il);
    return NULL;
  }

  if (il > 0 && !tuh_hid_ri_is_long(ri)) { // For now ignore long items.
    uint8_t short_type = tuh_hid_ri_short_type(ri);
    uint8_t short_tag = tuh_hid_ri_short_tag(ri);
    switch (short_type) {
      case RI_TYPE_GLOBAL:
        state->global_items[state->stack_index][short_tag] = ri;
        switch (short_tag) {
          case RI_GLOBAL_PUSH:
            if (++state->stack_index == HID_REPORT_STACK_SIZE) {
              state->status = HID_RIP_STACK_OVERFLOW;
              TU_LOG2("HID Report item parser: stack overflow\r\n");
              return NULL;
            }
            memcpy(&state->global_items[state->stack_index], &state->global_items[state->stack_index - 1], sizeof(uint8_t*) * 16);
            break;
          case RI_GLOBAL_POP:
            if (state->stack_index-- == 0) {
              state->status = HID_RIP_STACK_UNDERFLOW;
              TU_LOG2("HID Report item parser: stack underflow\r\n");              
              return NULL;
            }
            break;
          default:
            break;
        }
        break;
      case RI_TYPE_LOCAL:
        switch(short_tag) {
          case RI_LOCAL_USAGE: {
            uint32_t usage = tuh_hid_ri_short_udata32(ri);
            if (tuh_hid_ri_short_data_length(ri) <= 2) {
              const uint8_t* usage_page_item = state->global_items[state->stack_index][RI_GLOBAL_USAGE_PAGE];
              uint32_t usage_page = usage_page_item ? tuh_hid_ri_short_udata32(usage_page_item) : 0;
              usage |= usage_page << 16;
            }
            if (state->usage_count == HID_REPORT_MAX_USAGES) {
              state->status = HID_RIP_USAGES_OVERFLOW;
              TU_LOG2("HID Report item parser: usage overflow\r\n");              
              return NULL;
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
              state->status = HID_RIP_COLLECTIONS_OVERFLOW;
              TU_LOG2("HID Report item parser: collections overflow\r\n");
              return NULL;
            } 
            state->collections[state->collections_count++] = ri;
            break;
          }
          case RI_MAIN_COLLECTION_END:
            if (state->collections_count-- == 0) {
              state->status = HID_RIP_COLLECTIONS_UNDERFLOW;
              TU_LOG2("HID Report item parser: collections underflow\r\n");
              return NULL;
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
  state->status = HID_RIP_TTEM_OK;
  return ri;
}

const uint8_t* tuh_hid_rip_next_short_item(tuh_hid_rip_state_t *state) 
{
  const uint8_t* ri;
  while((ri = tuh_hid_rip_next_item(state)) != NULL) if (!tuh_hid_ri_is_long(ri)) break;
  return ri;
}

const uint8_t* tuh_hid_rip_global(tuh_hid_rip_state_t *state, uint8_t tag)
{
  return state->global_items[state->stack_index][tag];
}

const uint8_t* tuh_hid_rip_local(tuh_hid_rip_state_t *state, uint8_t tag)
{
  return state->local_items[tag];
}

const uint8_t* tuh_hid_rip_current_item(tuh_hid_rip_state_t *state)
{
  return state->cursor;
}

uint32_t tuh_hid_rip_report_total_size_bits(tuh_hid_rip_state_t *state) 
{
  const uint8_t* ri_report_size = tuh_hid_rip_global(state, RI_GLOBAL_REPORT_SIZE);
  const uint8_t* ri_report_count = tuh_hid_rip_global(state, RI_GLOBAL_REPORT_COUNT);
  
  if (ri_report_size != NULL && ri_report_count != NULL) 
  {
    uint32_t report_size = tuh_hid_ri_short_udata32(ri_report_size);
    uint32_t report_count = tuh_hid_ri_short_udata32(ri_report_count);
    return report_size * report_count;
  }
  else
  {
    TU_LOG2("HID report parser cannot calc total report size in bits\r\n");
    return 0;
  }
}

uint32_t tuh_hid_report_bits_u32(uint8_t const* report, uint16_t start, uint16_t length)
{
  const int16_t bit_offset_start = start & 7;
  const int16_t l = length + bit_offset_start;
  const uint8_t *p = report + (start >> 3);
  uint32_t acc = ((uint32_t)*p++) >> bit_offset_start;
  for(uint16_t i = 1; (i << 3) < l; ++i) {
    acc |= ((uint32_t)*p++) << ((i << 3) - bit_offset_start);
  }
  const uint32_t m = (((uint32_t)1) << length) - 1;
  return acc & m;
}

int32_t tuh_hid_report_bits_i32(uint8_t const* report, uint16_t start, uint16_t length)
{
  const int16_t bit_offset_start = start & 7;
  const int16_t l = length + bit_offset_start;
  const uint8_t *p = report + (start >> 3);
  uint32_t acc = ((uint32_t)*p++) >> bit_offset_start;
  for(uint16_t i = 1; (i << 3) < l; ++i) {
    acc |= ((uint32_t)*p++) << ((i << 3) - bit_offset_start);
  }
  const uint32_t lp0 = ((uint32_t)1) << (length - 1);
  const uint32_t lp1 = lp0 << 1;
  // Mask or sign extend
  return acc & lp0 ? acc | -lp1 : acc & (lp1 - 1);
}

// Helper to get some bytes from a HID report as an unsigned 32 bit number
uint32_t tuh_hid_report_bytes_u32(uint8_t const* report, uint16_t start, uint16_t length)
{
  const uint8_t *p = report + start;
  uint32_t acc = 0;
  for(uint16_t i = 0; i < length; ++i) {
    acc |= (uint32_t)*p++;
  }
  return acc;   
}

// Helper to get some bytes from a HID report as a signed 32 bit number
int32_t tuh_hid_report_bytes_i32(uint8_t const* report, uint16_t start, uint16_t length)
{
  const uint8_t *p = report + start;
  uint32_t acc = 0;
  for(uint16_t i = 0; i < length; ++i) {
    acc |= (uint32_t)*p++;
  }
  const uint32_t lp0 = ((uint32_t)1) << (length + 2);
  const uint32_t lp1 = lp0 << 1;
  // sign extend
  return acc & lp0 ? acc | -lp1 : acc;  
}

// Helper to get a value from a HID report
int32_t tuh_hid_report_i32(const uint8_t* report, uint16_t start, uint16_t length, bool is_signed) 
{
  if (length == 0) return 0;
  if ((start | length) & 7) {
    return is_signed ?
      tuh_hid_report_bits_i32(report, start, length):
      (int32_t)tuh_hid_report_bits_u32(report, start, length);
  }
  else {
    return is_signed ?
      tuh_hid_report_bytes_i32(report, start >> 3, length >> 3):
      (int32_t)tuh_hid_report_bytes_u32(report, start >> 3, length >> 3);   
  }
}

//--------------------------------------------------------------------+
// Report Descriptor Parser
//--------------------------------------------------------------------+
uint8_t tuh_hid_parse_report_descriptor(tuh_hid_report_info_t* report_info_arr, uint8_t arr_count, uint8_t const* desc_report, uint16_t desc_len) 
{
  // Prepare the summary array
  tu_memclr(report_info_arr, arr_count*sizeof(tuh_hid_report_info_t));
  uint8_t report_num = 0;
  uint16_t usage = 0;
  uint16_t usage_page = 0;
  
  tuh_hid_report_info_t* info = report_info_arr;
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, desc_report, desc_len);
  const uint8_t *ri;
  while((ri = tuh_hid_rip_next_short_item(&pstate)) != NULL)
  {
    uint8_t const type_and_tag = tuh_hid_ri_short_type_and_tag(ri);
    
    switch(type_and_tag)
    {
      case HID_RI_TYPE_AND_TAG(RI_TYPE_MAIN, RI_MAIN_INPUT): {
        info->in_len += tuh_hid_rip_report_total_size_bits(&pstate);
        info->usage = usage;
        info->usage_page = usage_page;
        break;
      }
      case HID_RI_TYPE_AND_TAG(RI_TYPE_MAIN, RI_MAIN_OUTPUT): {
        info->out_len += tuh_hid_rip_report_total_size_bits(&pstate);
        info->usage = usage;
        info->usage_page = usage_page;
        break;
      }
      case HID_RI_TYPE_AND_TAG(RI_TYPE_GLOBAL, RI_GLOBAL_REPORT_ID): {
        if (info->report_id > 0 || info->in_len > 0 || info->out_len > 0) {
          info++;
          report_num++;
        }
        info->report_id = tuh_hid_ri_short_udata8(ri);
        break;
      }
      case HID_RI_TYPE_AND_TAG(RI_TYPE_LOCAL, RI_LOCAL_USAGE): {
        // only take into account the "usage" before starting REPORT ID
        if ( pstate.collections_count == 0 ) {
          uint32_t eusage = pstate.usages[pstate.usage_count - 1];
          tuh_hid_ri_split_usage(eusage, &usage, &usage_page);
        }
        break;
      }
      default: break;
    }
  }
  
  for ( uint8_t i = 0; i < report_num + 1; i++ )
  {
    info = report_info_arr+i;
    TU_LOG2("%u: id = %02X, usage_page = %04X, usage = %04X, in_len = %u, out_len = %u\r\n", i, info->report_id, info->usage_page, info->usage, info->in_len, info->out_len);
  }

  return report_num + 1;
}

#endif


