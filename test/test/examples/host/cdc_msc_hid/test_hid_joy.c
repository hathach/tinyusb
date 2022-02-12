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
#include "hid_joy.h"
TEST_FILE("hid_ri.c")
TEST_FILE("hid_rip.c")
TEST_FILE("hid_joy.c")

void setUp(void)
{
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_nothing() {
  const uint8_t const tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
  };
  TEST_ASSERT_EQUAL(0, tuh_hid_joystick_parse_report_descriptor(tb, sizeof(tb)));
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
  tuh_hid_joystick_parse_report_descriptor(tb, sizeof(tb));

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

}


