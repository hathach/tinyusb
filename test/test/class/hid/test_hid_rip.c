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


#include "unity.h"

// Files to test
#include "hid_rip.h"
TEST_FILE("hid_ri.c")
TEST_FILE("hid_rip.c")

void setUp(void)
{
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_next_simple(void) 
{
  uint8_t tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(2, pstate.item_length);
  TEST_ASSERT_EQUAL(0x05, *pstate.cursor);
  TEST_ASSERT_EQUAL(&tb[2], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(2, pstate.item_length);
  TEST_ASSERT_EQUAL(0x09, *pstate.cursor);
  TEST_ASSERT_EQUAL(&tb[4], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(2, pstate.item_length);
  TEST_ASSERT_EQUAL(0xA1, *pstate.cursor);
  TEST_ASSERT_EQUAL(NULL, tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0, pstate.item_length);
}

void test_usage_with_page(void) 
{
  uint8_t tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0x01, tuh_hid_ri_short_udata32(pstate.cursor));
  TEST_ASSERT_EQUAL(&tb[2], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(1, pstate.usage_count);
  TEST_ASSERT_EQUAL(0x02, tuh_hid_ri_short_udata32(pstate.cursor));
  TEST_ASSERT_EQUAL(0x00010002, pstate.usages[0]);
}

void test_globals_recorded(void) 
{
  uint8_t tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x15, 0x55,        // Logical Minimum 55
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[2], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0, pstate.stack_index);
  TEST_ASSERT_EQUAL(0x01, tuh_hid_ri_short_udata32(pstate.global_items[pstate.stack_index][0]));
  TEST_ASSERT_EQUAL(0x55, tuh_hid_ri_short_data32(pstate.global_items[pstate.stack_index][1]));
}

void test_globals_overwritten(void) 
{
  uint8_t tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x05, 0x09,        // USAGE_PAGE (Button)
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[2], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0, pstate.stack_index);
  TEST_ASSERT_EQUAL(0x09, tuh_hid_ri_short_udata32(tuh_hid_rip_global(&pstate, 0)));
}

void test_push_pop(void) 
{
  uint8_t tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0xA4,              // Push
    0x05, 0x09,        // USAGE_PAGE (Button)
    0xB4,              // Pop
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[2], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[3], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[5], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0, pstate.stack_index);
  TEST_ASSERT_EQUAL(0x01, tuh_hid_ri_short_udata32(tuh_hid_rip_global(&pstate, 0)));
}

void test_stack_overflow(void) 
{
  uint8_t tb[HID_REPORT_STACK_SIZE];
  memset(tb, 0xA4, sizeof(tb));  // Push
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_INIT);
  for (int i = 0; i < HID_REPORT_STACK_SIZE - 1; ++i) {
    TEST_ASSERT_EQUAL(&tb[i], tuh_hid_rip_next_item(&pstate));
    TEST_ASSERT_EQUAL(pstate.status, HID_RIP_TTEM_OK);
  }
  TEST_ASSERT_EQUAL(NULL, tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_STACK_OVERFLOW);
}

void test_stack_underflow(void) 
{
  uint8_t tb[] = { 
    0xA4,              // Push
    0xB4,              // Pop
    0xB4,              // Pop
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_INIT);
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_TTEM_OK);
  TEST_ASSERT_EQUAL(&tb[1], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_TTEM_OK);
  TEST_ASSERT_EQUAL(0, tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_STACK_UNDERFLOW);
}

void test_usage_overflow(void) 
{
  uint8_t tb[HID_REPORT_MAX_USAGES + 1];
  memset(tb, 0x08, sizeof(tb));  // Usage
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_INIT);
  for (int i = 0; i < HID_REPORT_MAX_USAGES; ++i) {
    TEST_ASSERT_EQUAL(&tb[i], tuh_hid_rip_next_item(&pstate));
    TEST_ASSERT_EQUAL(pstate.status, HID_RIP_TTEM_OK);
  }
  TEST_ASSERT_EQUAL(NULL, tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_USAGES_OVERFLOW);
}

void test_collection_overflow(void) 
{
  uint8_t tb[HID_REPORT_MAX_COLLECTION_DEPTH + 1];
  memset(tb, 0xA0, sizeof(tb));  // Collection start
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_INIT);
  for (int i = 0; i < HID_REPORT_MAX_COLLECTION_DEPTH; ++i) {
    TEST_ASSERT_EQUAL(&tb[i], tuh_hid_rip_next_item(&pstate));
    TEST_ASSERT_EQUAL(pstate.status, HID_RIP_TTEM_OK);
  }
  TEST_ASSERT_EQUAL(NULL, tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_COLLECTIONS_OVERFLOW);
}

void test_collection_underflow(void) 
{
  uint8_t tb[] = { 
    0xA0,              // Collection start
    0xC0,              // Collection end
    0xC0,              // Collection end
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_INIT);
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_TTEM_OK);
  TEST_ASSERT_EQUAL(&tb[1], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_TTEM_OK);
  TEST_ASSERT_EQUAL(0, tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(pstate.status, HID_RIP_COLLECTIONS_UNDERFLOW);
}

void test_main_clears_local(void) 
{
  uint8_t tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0x39, 0x04,        // Designator Index 4
    0xA1, 0x01,        // Collection (Application)
    0x75, 0x05,        // REPORT_SIZE (5)    
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[2], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[4], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[6], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(1, pstate.usage_count);
  TEST_ASSERT_EQUAL(&tb[4], pstate.local_items[3]);
  TEST_ASSERT_EQUAL(4, tuh_hid_ri_short_data32(pstate.local_items[3]));
  TEST_ASSERT_EQUAL(&tb[8], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0, pstate.global_items[pstate.stack_index][3]);
  TEST_ASSERT_EQUAL(0, pstate.usage_count);
}

void test_collections(void) 
{
  uint8_t tb[] = { 
    0xA1, 0x01,        // Collection (Application)
    0xA1, 0x00,        // Collection (Physical)
    0xC0,              // End Collection
    0xC0,              // End Collection
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(1, pstate.collections_count);
  TEST_ASSERT_EQUAL(&tb[2], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(2, pstate.collections_count);
  TEST_ASSERT_EQUAL(&tb[0], pstate.collections[0]);
  TEST_ASSERT_EQUAL(&tb[2], pstate.collections[1]);
  TEST_ASSERT_EQUAL(&tb[4], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(1, pstate.collections_count);
  TEST_ASSERT_EQUAL(&tb[5], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0, pstate.collections_count);
}

void test_total_size_bits(void) 
{
  uint8_t tb[] = { 
    0x75, 0x08,        //     REPORT_SIZE (8)
    0x95, 0x02,        //     REPORT_COUNT (2)
  };
  
  tuh_hid_rip_state_t pstate;
  tuh_hid_rip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(&tb[0], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(&tb[2], tuh_hid_rip_next_item(&pstate));
  TEST_ASSERT_EQUAL(16, tuh_hid_rip_report_total_size_bits(&pstate));
}


void test_hid_parse_report_descriptor_single_mouse_report() {
  const uint8_t const tb[] = { 
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
  };
  
  tuh_hid_report_info_t report_info[3];

  uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, (const uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(1, report_count);
  TEST_ASSERT_EQUAL(1, report_info[0].usage_page);
  TEST_ASSERT_EQUAL(2, report_info[0].usage);
  TEST_ASSERT_EQUAL(0, report_info[0].report_id);
  TEST_ASSERT_EQUAL(3*1 + 1*5 + 8*2, report_info[0].in_len);
  TEST_ASSERT_EQUAL(0, report_info[0].out_len);
}

void test_hid_parse_report_descriptor_single_gamepad_report() {
  const uint8_t const tb[] = { 
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0xa1, 0x00,                    //   COLLECTION (Physical)
                                   // ReportID - 8 bits
    0x85, 0x01,                    //     REPORT_ID (1)
                                   // X & Y - 2x8 = 16 bits
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
                                   // Buttons - 8 bits
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x08,                    //     USAGE_MAXIMUM (Button 8)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0xc0,                          //     END_COLLECTION
    0xc0                           // END_COLLECTION
  };
  
  tuh_hid_report_info_t report_info[3];

  uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, (const uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(1, report_count);
  TEST_ASSERT_EQUAL(1, report_info[0].usage_page);
  TEST_ASSERT_EQUAL(5, report_info[0].usage);
  TEST_ASSERT_EQUAL(1, report_info[0].report_id);
  TEST_ASSERT_EQUAL(8*2 + 8*1, report_info[0].in_len);
  TEST_ASSERT_EQUAL(0, report_info[0].out_len);
}

void test_hid_parse_report_descriptor_dual_report() {
  const uint8_t const tb[] = { 
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)

    0x85, 0x01,        //   Report ID (1)
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x01,        //   Report Count (1)
    0x09, 0xE2,        //   Usage (Mute)
    0x81, 0x62,        //   Input (Data,Var,Abs,No Wrap,Linear,No Preferred State,Null State)

    0x85, 0x02,        //   Report ID (2)
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x02,        //   Report Count (2)
    0x09, 0xE2,        //   Usage (Mute)
    0x09, 0xE2,        //   Usage (Mute)
    0x81, 0x62,        //   Input (Data,Var,Abs,No Wrap,Linear,No Preferred State,Null State)
    0xC0,              // End Collection
  };
  
  tuh_hid_report_info_t report_info[3];

  uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, (const uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(2, report_count);
  TEST_ASSERT_EQUAL(0, report_info[0].usage_page);
  TEST_ASSERT_EQUAL(1, report_info[0].usage);
  TEST_ASSERT_EQUAL(1, report_info[0].report_id);
  TEST_ASSERT_EQUAL(1*1, report_info[0].in_len);
  TEST_ASSERT_EQUAL(0, report_info[0].out_len);
  TEST_ASSERT_EQUAL(0, report_info[1].usage_page);
  TEST_ASSERT_EQUAL(1, report_info[1].usage);
  TEST_ASSERT_EQUAL(2, report_info[1].report_id);
  TEST_ASSERT_EQUAL(1*2, report_info[1].in_len);
  TEST_ASSERT_EQUAL(0, report_info[1].out_len);
}

void test_hid_parse_report_descriptor_joystick_report() {
  const uint8_t const tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x03,  //     Logical Maximum (1023)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0xFF, 0x03,  //     Physical Maximum (1023)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x0A,        //     Report Size (10)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x35,        //     Usage (Rz)
    0x09, 0x32,        //     Usage (Z)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x01,  //     Logical Maximum (511)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0xFF, 0x01,  //     Physical Maximum (511)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x09,        //     Report Size (9)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x39,        //     Usage (Hat switch)
    0x15, 0x01,        //     Logical Minimum (1)
    0x25, 0x08,        //     Logical Maximum (8)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0x3B, 0x01,  //     Physical Maximum (315)
    0x65, 0x14,        //     Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x0C,        //     Usage Maximum (0x0C)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x01,        //     Physical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x0C,        //     Report Count (12)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x04,        //     Report Count (4)
    0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xC0,              // End Collection
  };
  tuh_hid_report_info_t report_info[3];

  uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, (const uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(1, report_count);
  TEST_ASSERT_EQUAL(1, report_info[0].usage_page);
  TEST_ASSERT_EQUAL(4, report_info[0].usage);
  TEST_ASSERT_EQUAL(0, report_info[0].report_id);
  TEST_ASSERT_EQUAL(64, report_info[0].in_len);
  TEST_ASSERT_EQUAL(0, report_info[0].out_len);
}

void test_hid_parse_greenasia_report() {
  const uint8_t const tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)
    0xA1, 0x02,        //   Collection (Logical)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x05,        //     Report Count (5)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0xFF, 0x00,  //     Physical Maximum (255)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x35,        //     Usage (Rz)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x00,        //     Usage (Undefined)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x25, 0x07,        //     Logical Maximum (7)
    0x46, 0x3B, 0x01,  //     Physical Maximum (315)
    0x65, 0x14,        //     Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39,        //     Usage (Hat switch)
    0x81, 0x42,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x0C,        //     Report Count (12)
    0x25, 0x01,        //     Logical Maximum (1)
    0x45, 0x01,        //     Physical Maximum (1)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x0C,        //     Usage Maximum (0x0C)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x08,        //     Report Count (8)
    0x25, 0x01,        //     Logical Maximum (1)
    0x45, 0x01,        //     Physical Maximum (1)
    0x09, 0x01,        //     Usage (0x01)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x04,        //     Report Count (4)
    0x46, 0xFF, 0x00,  //     Physical Maximum (255)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x09, 0x02,        //     Usage (0x02)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xC0,              // End Collection
  };
  tuh_hid_report_info_t report_info[3];

  uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, (const uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(1, report_count);
}

void test_hid_parse_speedlink_report() {
  const uint8_t const tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)
    0xA1, 0x02,        //   Collection (Logical)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x05,        //     Report Count (5)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0xFF, 0x00,  //     Physical Maximum (255)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x35,        //     Usage (Rz)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x25, 0x07,        //     Logical Maximum (7)
    0x46, 0x3B, 0x01,  //     Physical Maximum (315)
    0x65, 0x14,        //     Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39,        //     Usage (Hat switch)
    0x81, 0x42,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x0C,        //     Report Count (12)
    0x25, 0x01,        //     Logical Maximum (1)
    0x45, 0x01,        //     Physical Maximum (1)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x0C,        //     Usage Maximum (0x0C)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x08,        //     Report Count (8)
    0x25, 0x01,        //     Logical Maximum (1)
    0x45, 0x01,        //     Physical Maximum (1)
    0x09, 0x01,        //     Usage (0x01)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x07,        //     Report Count (7)
    0x46, 0xFF, 0x00,  //     Physical Maximum (255)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x09, 0x02,        //     Usage (0x02)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xC0,              // End Collection
  };
  tuh_hid_report_info_t report_info[3];

  uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, (const uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(1, report_count);
}

void test_tuh_hid_report_bits_u32() {
  const uint8_t const tb[] = {
    0x50, 0x05, 0x8f, 0xff
  };
  TEST_ASSERT_EQUAL(0x55, tuh_hid_report_bits_u32(tb, 4, 8));
  TEST_ASSERT_EQUAL(0x8f, tuh_hid_report_bits_u32(tb, 16, 8));
  TEST_ASSERT_EQUAL(0xff8f0550, tuh_hid_report_bits_u32(tb, 0, 32));
}

void test_tuh_hid_report_bits_i32() {
  const uint8_t const tb[] = {
    0x50, 0x05, 0x8f, 0xff
  };
  TEST_ASSERT_EQUAL(0x55, tuh_hid_report_bits_i32(tb, 4, 8));
  TEST_ASSERT_EQUAL(0xffffff8f, tuh_hid_report_bits_i32(tb, 16, 8));
  TEST_ASSERT_EQUAL(0xff8f0550, tuh_hid_report_bits_i32(tb, 0, 32));
}
