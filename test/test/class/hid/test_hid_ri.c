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
#include "hid_ri.h"
TEST_FILE("hid_ri.c")

void setUp(void)
{
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_short_item_length(void) 
{
  uint8_t tb[] = { 0x00, 0x01, 0x02, 0x03 };
  
  TEST_ASSERT_EQUAL(0, hidri_short_data_length(&tb[0]));
  TEST_ASSERT_EQUAL(1, hidri_short_data_length(&tb[1]));
  TEST_ASSERT_EQUAL(2, hidri_short_data_length(&tb[2]));
  TEST_ASSERT_EQUAL(4, hidri_short_data_length(&tb[3]));
}

void test_short_item_size_check(void)
{
  uint8_t tb[] = { 0x46, 0x3B, 0x01 };        /*     Physical Maximum (315)  */
  
  TEST_ASSERT_EQUAL( 3, hidri_size(tb, 3));
  TEST_ASSERT_EQUAL(-1, hidri_size(tb, 2));
  TEST_ASSERT_EQUAL(-1, hidri_size(tb, 1));
  TEST_ASSERT_EQUAL( 0, hidri_size(tb, 0));
}

void test_long_item(void) 
{
  uint8_t tb[] = { 0xfe, 0xff, 0x81, 0x01 };
  
  TEST_ASSERT_EQUAL(true, hidri_is_long(tb));
  TEST_ASSERT_EQUAL(2, hidri_short_data_length(tb));
  TEST_ASSERT_EQUAL(255, hidri_long_data_length(tb));
  TEST_ASSERT_EQUAL(0x81, hidri_long_tag(tb));
  TEST_ASSERT_EQUAL(1, hidri_long_item_data(tb)[0]);
  TEST_ASSERT_EQUAL(3 + 255, hidri_size(tb, 3 + 255));
}

void test_long_item_size_check(void) 
{
  uint8_t tb[] = { 0xfe, 0xff, 0x81, 0x01 };
  
  TEST_ASSERT_EQUAL(3 + 255, hidri_size(tb, 3 + 255));
  TEST_ASSERT_EQUAL(-2, hidri_size(tb, 3 + 254));
  TEST_ASSERT_EQUAL(-2, hidri_size(tb, 3));
  TEST_ASSERT_EQUAL(-1, hidri_size(tb, 2));
  TEST_ASSERT_EQUAL(-1, hidri_size(tb, 1));
  TEST_ASSERT_EQUAL( 0, hidri_size(tb, 0));
}

void test_physical_max_315(void)
{
  uint8_t tb[] = { 0x46, 0x3B, 0x01 };        /*     Physical Maximum (315)  */
  
  TEST_ASSERT_EQUAL(2, hidri_short_data_length(tb));
  TEST_ASSERT_EQUAL(1, hidri_short_type(tb));
  TEST_ASSERT_EQUAL(4, hidri_short_tag(tb));
  TEST_ASSERT_EQUAL(false, hidri_is_long(tb));
  TEST_ASSERT_EQUAL(315, hidri_short_udata32(tb));
  TEST_ASSERT_EQUAL(315, hidri_short_data32(tb));
  TEST_ASSERT_EQUAL(3, hidri_size(tb, 3));
}

void test_physical_max_123(void) {
  uint8_t tb[] = { 0x46, 0x7B, 0x00 };        /*     Physical Maximum (123)  */
  
  TEST_ASSERT_EQUAL(2, hidri_short_data_length(tb));
  TEST_ASSERT_EQUAL(1, hidri_short_type(tb));
  TEST_ASSERT_EQUAL(4, hidri_short_tag(tb));
  TEST_ASSERT_EQUAL(false, hidri_is_long(tb));
  TEST_ASSERT_EQUAL(123, hidri_short_udata32(tb));
  TEST_ASSERT_EQUAL(123, hidri_short_udata8(tb));
  TEST_ASSERT_EQUAL(123, hidri_short_data32(tb));
  TEST_ASSERT_EQUAL(3, hidri_size(tb, 3));
}

void test_logical_min_neg_127(void)
{
  uint8_t tb[] = { 0x15, 0x81 };        /*     LOGICAL_MINIMUM (-127)  */
  
  TEST_ASSERT_EQUAL(1, hidri_short_data_length(tb));
  TEST_ASSERT_EQUAL(1, hidri_short_type(tb));
  TEST_ASSERT_EQUAL(1, hidri_short_tag(tb));
  TEST_ASSERT_EQUAL(false, hidri_is_long(tb));
  TEST_ASSERT_EQUAL(0x81, hidri_short_udata32(tb));
  TEST_ASSERT_EQUAL(-127, hidri_short_data32(tb));
  TEST_ASSERT_EQUAL(2, hidri_size(tb, 2));
}

void test_logical_min_neg_32768(void)
{
  uint8_t tb[] = { 0x16, 0x00, 0x80 };  //     Logical Minimum (-32768)
  
  TEST_ASSERT_EQUAL(2, hidri_short_data_length(tb));
  TEST_ASSERT_EQUAL(1, hidri_short_type(tb));
  TEST_ASSERT_EQUAL(1, hidri_short_tag(tb));
  TEST_ASSERT_EQUAL(false, hidri_is_long(tb));
  TEST_ASSERT_EQUAL(-32768, hidri_short_data32(tb));
  TEST_ASSERT_EQUAL(3, hidri_size(tb, 3));
}

// https://eleccelerator.com/usbdescreqparser/ says this should be -2147483648,
// which I am pretty sure is wrong.. but worth noting in case problems arise later.
void test_logical_min_neg_2147483647(void)
{
  uint8_t tb[] = { 0x17, 0x01, 0x00, 0x00, 0x80 };  //     Logical Minimum (-2147483647)
  
  TEST_ASSERT_EQUAL(4, hidri_short_data_length(tb));
  TEST_ASSERT_EQUAL(1, hidri_short_type(tb));
  TEST_ASSERT_EQUAL(1, hidri_short_tag(tb));
  TEST_ASSERT_EQUAL(false, hidri_is_long(tb));
  TEST_ASSERT_EQUAL(-2147483647, hidri_short_data32(tb));
  TEST_ASSERT_EQUAL(5, hidri_size(tb, 5));
}




