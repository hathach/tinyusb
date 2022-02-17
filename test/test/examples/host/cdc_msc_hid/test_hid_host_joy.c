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
#include "hid_host_joy.h"
TEST_FILE("hid_ri.c")
TEST_FILE("hid_rip.c")
TEST_FILE("hid_host_utils.c")
TEST_FILE("hid_host_joy.c")

static const uint8_t const tb_greenasia[] = { 
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

static const uint8_t const tb_speedlink[] = { 
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
  
static const uint8_t const tb_apple[] = { 
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


void setUp(void)
{
  tuh_hid_free_simple_joysticks();
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+

void test_simple_joystick_allocator() {
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(5, 1, 2));
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(5, 1, 3));
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(4, 2, 3));
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(4, 5, 2));
  TEST_ASSERT_NOT_NULL(tuh_hid_allocate_simple_joystick(5, 1, 2));
  TEST_ASSERT_NOT_NULL(tuh_hid_allocate_simple_joystick(5, 1, 3));
  TEST_ASSERT_NOT_NULL(tuh_hid_allocate_simple_joystick(4, 2, 3));
  TEST_ASSERT_NOT_NULL(tuh_hid_allocate_simple_joystick(4, 5, 2));
  TEST_ASSERT_NOT_NULL(tuh_hid_get_simple_joystick(5, 1, 2));
  TEST_ASSERT_NOT_NULL(tuh_hid_get_simple_joystick(5, 1, 3));
  TEST_ASSERT_NOT_NULL(tuh_hid_get_simple_joystick(4, 2, 3));
  TEST_ASSERT_NOT_NULL(tuh_hid_get_simple_joystick(4, 5, 2));
  tuh_hid_free_simple_joysticks_for_instance(5, 1);
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(5, 1, 2));
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(5, 1, 3));  
  TEST_ASSERT_NOT_NULL(tuh_hid_get_simple_joystick(4, 2, 3));
  TEST_ASSERT_NOT_NULL(tuh_hid_get_simple_joystick(4, 5, 2));
  tuh_hid_free_simple_joysticks_for_instance(4, 5);
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(4, 5, 2));
  TEST_ASSERT_NOT_NULL(tuh_hid_get_simple_joystick(4, 2, 3)); 
}

void test_simple_joystick_allocate_too_many() {
  for (int i =0; i < HID_MAX_JOYSTICKS; ++i) {
    TEST_ASSERT_NOT_NULL(tuh_hid_allocate_simple_joystick(1, i + 1, 4));
  }
  TEST_ASSERT_NULL(tuh_hid_allocate_simple_joystick(2, 1, 0));
}

void test_simple_joystick_free_all() {
  TEST_ASSERT_NOT_NULL(tuh_hid_allocate_simple_joystick(7, 1, 3));
  TEST_ASSERT_NOT_NULL(tuh_hid_allocate_simple_joystick(0, 2, 3));
  tuh_hid_free_simple_joysticks();
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(7, 1, 3));
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(0, 2, 3));  
}

void test_simple_joystick_obtain() {
  tusb_hid_simple_joysick_t* j1 = tuh_hid_allocate_simple_joystick(7, 1, 3);
  TEST_ASSERT_NOT_NULL(j1);
  TEST_ASSERT_EQUAL(j1, tuh_hid_obtain_simple_joystick(7, 1, 3));
  TEST_ASSERT_NULL(tuh_hid_get_simple_joystick(7, 2, 3));  
  tusb_hid_simple_joysick_t* j2 = tuh_hid_allocate_simple_joystick(7, 2, 3);
  TEST_ASSERT_NOT_NULL(j2);
  TEST_ASSERT_NOT_EQUAL(j1,j2);
}

void test_tuh_hid_joystick_get_data() {
  tuh_hid_joystick_data_t joystick_data;
  tuh_hid_rip_state_t pstate;
  tusb_hid_simple_joysick_t* simple_joystick;
  tuh_hid_rip_init_state(&pstate, tb_speedlink, sizeof(tb_speedlink));
  const uint8_t *ri;

  while((ri = tuh_hid_rip_next_item(&pstate)) != NULL ) if (ri >= &tb_speedlink[32]) break;
  TEST_ASSERT_EQUAL(&tb_speedlink[32], ri); // Move to the first input in the speedlink description
  TEST_ASSERT_EQUAL(true, tuh_hid_joystick_get_data(&pstate, ri, &joystick_data));

  TEST_ASSERT_EQUAL(8, joystick_data.report_size);
  TEST_ASSERT_EQUAL(5, joystick_data.report_count);
  TEST_ASSERT_EQUAL(0, joystick_data.report_id);
  TEST_ASSERT_EQUAL(0, joystick_data.logical_min);
  TEST_ASSERT_EQUAL(255, joystick_data.logical_max);
  TEST_ASSERT_EQUAL(false, joystick_data.usage_is_range);

  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.data_const);
  TEST_ASSERT_EQUAL(true, joystick_data.input_flags.array_variable);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.absolute_relative);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.nowrap_wrap);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.linear_nonlinear);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.prefered_noprefered);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.nonull_null);

  tuh_hid_joystick_process_usages(&pstate, &joystick_data, 0, 5, 9);
  simple_joystick = tuh_hid_get_simple_joystick(5, 9, 0);
  TEST_ASSERT_NOT_NULL(simple_joystick);
  // x1
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_x1.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_x1.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_x1.flags.is_signed);
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_x1.logical_min);
  TEST_ASSERT_EQUAL(255, simple_joystick->axis_x1.logical_max);

  // y1
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y1.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y1.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_y1.flags.is_signed);
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_y1.logical_min);
  TEST_ASSERT_EQUAL(255, simple_joystick->axis_y1.logical_max);
  // x2
  TEST_ASSERT_EQUAL(24, simple_joystick->axis_x2.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_x2.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_x2.flags.is_signed);
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_x2.logical_min);
  TEST_ASSERT_EQUAL(255, simple_joystick->axis_x2.logical_max);
  // y2
  TEST_ASSERT_EQUAL(32, simple_joystick->axis_y2.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y2.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_y2.flags.is_signed);
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_y2.logical_min);
  TEST_ASSERT_EQUAL(255, simple_joystick->axis_y2.logical_max);

  while((ri = tuh_hid_rip_next_item(&pstate)) != NULL ) if (ri >= &tb_speedlink[47]) break;
  TEST_ASSERT_EQUAL(&tb_speedlink[47], ri); // Move to the second input in the speedlink description
  TEST_ASSERT_EQUAL(true, tuh_hid_joystick_get_data(&pstate, ri, &joystick_data));

  TEST_ASSERT_EQUAL(4, joystick_data.report_size);
  TEST_ASSERT_EQUAL(1, joystick_data.report_count);
  TEST_ASSERT_EQUAL(0, joystick_data.report_id);
  TEST_ASSERT_EQUAL(0, joystick_data.logical_min);
  TEST_ASSERT_EQUAL(7, joystick_data.logical_max);
  TEST_ASSERT_EQUAL(false, joystick_data.usage_is_range);

  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.data_const);
  TEST_ASSERT_EQUAL(true, joystick_data.input_flags.array_variable);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.absolute_relative);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.nowrap_wrap);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.linear_nonlinear);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.prefered_noprefered);
  TEST_ASSERT_EQUAL(true, joystick_data.input_flags.nonull_null);

  tuh_hid_joystick_process_usages(&pstate, &joystick_data, 40, 5, 9);
  simple_joystick = tuh_hid_get_simple_joystick(5, 9, 0);
  TEST_ASSERT_NOT_NULL(simple_joystick);
  TEST_ASSERT_EQUAL(40, simple_joystick->hat.start);
  TEST_ASSERT_EQUAL(4, simple_joystick->hat.length);

  while((ri = tuh_hid_rip_next_item(&pstate)) != NULL ) if (ri >= &tb_speedlink[65]) break;
  TEST_ASSERT_EQUAL(&tb_speedlink[65], ri); // Move to the second input in the speedlink description
  TEST_ASSERT_EQUAL(true, tuh_hid_joystick_get_data(&pstate, ri, &joystick_data));

  TEST_ASSERT_EQUAL(1, joystick_data.report_size);
  TEST_ASSERT_EQUAL(12, joystick_data.report_count);
  TEST_ASSERT_EQUAL(0, joystick_data.report_id);
  TEST_ASSERT_EQUAL(0, joystick_data.logical_min);
  TEST_ASSERT_EQUAL(1, joystick_data.logical_max);
  TEST_ASSERT_EQUAL(true, joystick_data.usage_is_range);
  TEST_ASSERT_EQUAL(1, joystick_data.usage_min);
  TEST_ASSERT_EQUAL(12, joystick_data.usage_max);

  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.data_const);
  TEST_ASSERT_EQUAL(true, joystick_data.input_flags.array_variable);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.absolute_relative);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.nowrap_wrap);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.linear_nonlinear);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.prefered_noprefered);
  TEST_ASSERT_EQUAL(false, joystick_data.input_flags.nonull_null);

  tuh_hid_joystick_process_usages(&pstate, &joystick_data, 44, 5, 9); // 56
  simple_joystick = tuh_hid_get_simple_joystick(5, 9, 0);
  TEST_ASSERT_NOT_NULL(simple_joystick);
  TEST_ASSERT_EQUAL(44, simple_joystick->buttons.start);
  TEST_ASSERT_EQUAL(12, simple_joystick->buttons.length);
  
  TEST_ASSERT_EQUAL(7, simple_joystick->report_length);
}

void test_hid_parse_greenasia_report() {
  tuh_hid_joystick_parse_report_descriptor(tb_speedlink, sizeof(tb_speedlink), 5, 9);
  tusb_hid_simple_joysick_t* simple_joystick = tuh_hid_get_simple_joystick(5, 9, 0);
  TEST_ASSERT_NOT_NULL(simple_joystick);
  // x1
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_x1.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_x1.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_x1.flags.is_signed);
  // y1
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y1.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y1.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_y1.flags.is_signed);
  // x2
  TEST_ASSERT_EQUAL(24, simple_joystick->axis_x2.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_x2.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_x2.flags.is_signed);
  // y2
  TEST_ASSERT_EQUAL(32, simple_joystick->axis_y2.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y2.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_y2.flags.is_signed);
  TEST_ASSERT_EQUAL(40, simple_joystick->hat.start);
  TEST_ASSERT_EQUAL(4, simple_joystick->hat.length);
  TEST_ASSERT_EQUAL(44, simple_joystick->buttons.start);
  TEST_ASSERT_EQUAL(12, simple_joystick->buttons.length);

  TEST_ASSERT_EQUAL(8, simple_joystick->report_length);
}

void test_hid_parse_speedlink_report() {
  tuh_hid_joystick_parse_report_descriptor(tb_greenasia, sizeof(tb_greenasia), 5, 9);
  tusb_hid_simple_joysick_t* simple_joystick = tuh_hid_get_simple_joystick(5, 9, 0);
  TEST_ASSERT_NOT_NULL(simple_joystick);
  // x1
  TEST_ASSERT_EQUAL(16, simple_joystick->axis_x1.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_x1.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_x1.flags.is_signed);
  // y1
  TEST_ASSERT_EQUAL(24, simple_joystick->axis_y1.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y1.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_y1.flags.is_signed);
  // x2
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_x2.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_x2.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_x2.flags.is_signed);
  // y2
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y2.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->axis_y2.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_y2.flags.is_signed);
  
  TEST_ASSERT_EQUAL(40, simple_joystick->hat.start);
  TEST_ASSERT_EQUAL(4, simple_joystick->hat.length);
  TEST_ASSERT_EQUAL(44, simple_joystick->buttons.start);
  TEST_ASSERT_EQUAL(12, simple_joystick->buttons.length);

  TEST_ASSERT_EQUAL(8, simple_joystick->report_length);
}

void test_apple_joystick() {
  // Thanks to https://jenswilly.dk/2012/10/parsing-usb-joystick-hid-data/
  
  //  10 bits of X-axis data (for values from 0 to 1023)
  //  10 bits of Y-axis data (for values from 0 to 1023)
  //   9 bits of Rz-axis (yaw or rotation) data (for values from 0-511)
  //   9 bits of Z-axis (throttle) data (for values from 0-511)
  //   2 "constant" bits – i.e. unused padding bits
  //   8 bits of hat switch data (though values are only from 1-8)
  //  12 bits of button states (12 buttons of 0 or 1 values)
  //   4 bits of padding  
  tuh_hid_joystick_parse_report_descriptor(tb_apple, sizeof(tb_apple), 5, 9);
  tusb_hid_simple_joysick_t* simple_joystick = tuh_hid_get_simple_joystick(5, 9, 0);
  TEST_ASSERT_NOT_NULL(simple_joystick);
  // x1
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_x1.start);
  TEST_ASSERT_EQUAL(10, simple_joystick->axis_x1.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_x1.flags.is_signed);
  TEST_ASSERT_EQUAL(0, simple_joystick->axis_x1.logical_min);
  TEST_ASSERT_EQUAL(1023, simple_joystick->axis_x1.logical_max);
  // y1
  TEST_ASSERT_EQUAL(10, simple_joystick->axis_y1.start);
  TEST_ASSERT_EQUAL(10, simple_joystick->axis_y1.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_y1.flags.is_signed);
  // x2
  TEST_ASSERT_EQUAL(29, simple_joystick->axis_x2.start);
  TEST_ASSERT_EQUAL(9, simple_joystick->axis_x2.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_x2.flags.is_signed);
  // y2
  TEST_ASSERT_EQUAL(20, simple_joystick->axis_y2.start);
  TEST_ASSERT_EQUAL(9, simple_joystick->axis_y2.length);
  TEST_ASSERT_EQUAL(false, simple_joystick->axis_y2.flags.is_signed);
  
  TEST_ASSERT_EQUAL(40, simple_joystick->hat.start);
  TEST_ASSERT_EQUAL(8, simple_joystick->hat.length);
  
  TEST_ASSERT_EQUAL(48, simple_joystick->buttons.start);
  TEST_ASSERT_EQUAL(12, simple_joystick->buttons.length); 
  
  // Raw hex: e9 31 e8 ef 3f 00 00 00
  // 
  // Binary  11101001 00110001 11101000 11101111 00111111 00000000 00000000 00000000
  // field   XXXXXXXX YYYYYYXX RRRRYYYY ZZZRRRRR --ZZZZZZ HHHHHHHH BBBBBBBB ----BBBB
  // bit no. 76543210 54321098 32109876 21087654 --876543 76543210 76543210 ----BA98
  // 
  // X = X-axis:  b0111101001 = 489
  // Y = Y-axis:  b1000001100 = 524
  // R = Rz-axis: b011111110 = 254
  // Z = Z-axis:  b111111111 = 511
  // H = hat:     b00000000 = 0 (centered)
  // B = buttons  b000000000000 = 0 (no buttons pressed)
  // - = padding
  
  uint8_t report[] = {0xe9, 0x31, 0xe8, 0xef, 0x3f, 0x00, 0x00, 0x00};
  tusb_hid_simple_joysick_process_report(simple_joystick, report, sizeof(report));
  TEST_ASSERT_EQUAL(true, simple_joystick->has_values);
  TEST_ASSERT_EQUAL(489, simple_joystick->values.x1);
  TEST_ASSERT_EQUAL(524, simple_joystick->values.y1);
  TEST_ASSERT_EQUAL(511, simple_joystick->values.x2);
  TEST_ASSERT_EQUAL(254, simple_joystick->values.y2);
  TEST_ASSERT_EQUAL(0, simple_joystick->values.hat);
  TEST_ASSERT_EQUAL(0, simple_joystick->values.buttons);

  TEST_ASSERT_EQUAL(8, simple_joystick->report_length);
}

void test_get_simple_joysticks() {
  static tusb_hid_simple_joysick_t* hid_simple_joysicks[4];
  uint8_t jcount;
  jcount = tuh_hid_get_simple_joysticks(hid_simple_joysicks, 4);
  TEST_ASSERT_EQUAL(0, jcount);
  tuh_hid_joystick_parse_report_descriptor(tb_speedlink, sizeof(tb_speedlink), 5, 9);
  jcount = tuh_hid_get_simple_joysticks(hid_simple_joysicks, 4);
  TEST_ASSERT_EQUAL(1, jcount);
  TEST_ASSERT_EQUAL(tuh_hid_get_simple_joystick(5, 9, 0), hid_simple_joysicks[0]);
  tuh_hid_joystick_parse_report_descriptor(tb_apple, sizeof(tb_apple), 1, 3);
  jcount = tuh_hid_get_simple_joysticks(hid_simple_joysicks, 1);
  TEST_ASSERT_EQUAL(1, jcount);
  jcount = tuh_hid_get_simple_joysticks(hid_simple_joysicks, 4);
  TEST_ASSERT_EQUAL(2, jcount);
  TEST_ASSERT_EQUAL(tuh_hid_get_simple_joystick(5, 9, 0), hid_simple_joysicks[0]);
  TEST_ASSERT_EQUAL(tuh_hid_get_simple_joystick(1, 3, 0), hid_simple_joysicks[1]);
  tuh_hid_free_simple_joysticks_for_instance(5, 9);
  jcount = tuh_hid_get_simple_joysticks(hid_simple_joysicks, 4);
  TEST_ASSERT_EQUAL(1, jcount);
  TEST_ASSERT_EQUAL(tuh_hid_get_simple_joystick(1, 3, 0), hid_simple_joysicks[0]);  
}

// TODO Test multiple report IDs



