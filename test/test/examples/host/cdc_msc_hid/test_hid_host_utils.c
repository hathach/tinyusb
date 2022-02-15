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
#include "hid_host_utils.h"
TEST_FILE("hid_host_utils.c")

void setUp(void)
{
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+

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

void test_tuh_hid_report_bytes_u32() {
  const uint8_t const tb[] = {
    0x50, 0x05, 0x8f, 0xff
  };
  TEST_ASSERT_EQUAL(0x0550, tuh_hid_report_bytes_u32(tb, 0, 2));
  TEST_ASSERT_EQUAL(0x8f05, tuh_hid_report_bytes_u32(tb, 1, 2));
  TEST_ASSERT_EQUAL(0xff8f05, tuh_hid_report_bytes_u32(tb, 1, 3));
  TEST_ASSERT_EQUAL(0xff8f0550, tuh_hid_report_bytes_u32(tb, 0, 4));
  TEST_ASSERT_EQUAL(0x8f, tuh_hid_report_bytes_u32(tb, 2, 1));
}

void test_tuh_hid_report_bytes_i32() {
  const uint8_t const tb[] = {
    0x50, 0x05, 0x8f, 0xff
  };
  TEST_ASSERT_EQUAL(0x0550, tuh_hid_report_bytes_i32(tb, 0, 2));
  TEST_ASSERT_EQUAL(0xffff8f05, tuh_hid_report_bytes_i32(tb, 1, 2));
  TEST_ASSERT_EQUAL(0xffff8f05, tuh_hid_report_bytes_i32(tb, 1, 3));
  TEST_ASSERT_EQUAL(0xff8f0550, tuh_hid_report_bytes_i32(tb, 0, 4));
  TEST_ASSERT_EQUAL(0xffffff8f, tuh_hid_report_bytes_i32(tb, 2, 1));
}

void test_tuh_hid_report_i32() {
  const uint8_t const tb[] = {
    0x50, 0x05, 0x8f, 0xff
  };
  TEST_ASSERT_EQUAL(0x55, tuh_hid_report_i32(tb, 4, 8, false));
  TEST_ASSERT_EQUAL(0x8f, tuh_hid_report_i32(tb, 16, 8, false));
  TEST_ASSERT_EQUAL(0xff8f0550, tuh_hid_report_i32(tb, 0, 32, false));

  TEST_ASSERT_EQUAL(0x55, tuh_hid_report_i32(tb, 4, 8, true));
  TEST_ASSERT_EQUAL(0xffffff8f, tuh_hid_report_i32(tb, 16, 8, true));
  TEST_ASSERT_EQUAL(0xff8f0550, tuh_hid_report_i32(tb, 0, 32, true));
}


// TODO Test multiple report IDs


