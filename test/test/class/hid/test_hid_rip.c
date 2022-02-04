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
  hidrip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(2, hidrip_next_item(&pstate));
  TEST_ASSERT_EQUAL(2, pstate.item_length);
  TEST_ASSERT_EQUAL(0x05, *pstate.cursor);
  TEST_ASSERT_EQUAL(2, hidrip_next_item(&pstate));
  TEST_ASSERT_EQUAL(2, pstate.item_length);
  TEST_ASSERT_EQUAL(0x09, *pstate.cursor);
  TEST_ASSERT_EQUAL(2, hidrip_next_item(&pstate));
  TEST_ASSERT_EQUAL(2, pstate.item_length);
  TEST_ASSERT_EQUAL(0xA1, *pstate.cursor);
  TEST_ASSERT_EQUAL(0, hidrip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0, pstate.item_length);
}

void test_usage_with_page(void) 
{
  uint8_t tb[] = { 
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
  };
  
  tuh_hid_rip_state_t pstate;
  hidrip_init_state(&pstate, (uint8_t*)&tb, sizeof(tb));
  TEST_ASSERT_EQUAL(2, hidrip_next_item(&pstate));
  TEST_ASSERT_EQUAL(0x01, hidri_short_udata32(pstate.cursor));
  TEST_ASSERT_EQUAL(2, hidrip_next_item(&pstate));
  TEST_ASSERT_EQUAL(1, pstate.usage_count);
  TEST_ASSERT_EQUAL(0x02, hidri_short_udata32(pstate.cursor));
  TEST_ASSERT_EQUAL(0x00010002, pstate.usages[0]);
}

