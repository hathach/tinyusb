/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include <string.h>
#include "unity.h"
#include "tusb_allocator.h"

#define RAM_BASE 0x10000
#define RAM_SIZE 4096
// It's implementation detail of allocator
#define INVALID_ADDR 0xffffffff

void setUp(void)
{
  tu_allocator_init(RAM_BASE, RAM_SIZE);
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_normal(void)
{
    uint32_t address = tu_malloc(5);
    TEST_ASSERT_EQUAL(true, tu_free(address));
}

void test_double_free(void)
{
  uint32_t address = tu_malloc(10);
  TEST_ASSERT_EQUAL(true, tu_free(address));
  // Double free
  TEST_ASSERT_EQUAL(false, tu_free(address));
}

void test_out_of_memory(void)
{
    uint32_t address0 = tu_malloc(RAM_SIZE / 2);
    uint32_t address1 = tu_malloc(RAM_SIZE / 2);
    uint32_t address2 = tu_malloc(1);
    TEST_ASSERT_NOT_EQUAL(INVALID_ADDR, address0);
    TEST_ASSERT_NOT_EQUAL(INVALID_ADDR, address1);
    TEST_ASSERT_EQUAL(INVALID_ADDR, address2);
}
