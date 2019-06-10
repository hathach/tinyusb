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

#include "unity.h"
#include "tusb_fifo.h"

#define FIFO_SIZE 10
TU_FIFO_DEF(ff, FIFO_SIZE, uint8_t, false);

void setUp(void)
{
  tu_fifo_clear(&ff);
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_normal(void)
{
  for(uint8_t i=0; i < FIFO_SIZE; i++) tu_fifo_write(&ff, &i);

  for(uint8_t i=0; i < FIFO_SIZE; i++)
  {
    uint8_t c;
    tu_fifo_read(&ff, &c);
    TEST_ASSERT_EQUAL(i, c);
  }
}

void test_peek(void)
{
  uint8_t temp;

  temp = 10; tu_fifo_write(&ff, &temp);
  temp = 20; tu_fifo_write(&ff, &temp);
  temp = 30; tu_fifo_write(&ff, &temp);

  temp = 0;

  tu_fifo_peek(&ff, &temp);
  TEST_ASSERT_EQUAL(10, temp);

  tu_fifo_peek_at(&ff, 1, &temp);
  TEST_ASSERT_EQUAL(20, temp);
}

void test_empty(void)
{
  uint8_t temp;
  TEST_ASSERT_TRUE(tu_fifo_empty(&ff));
  tu_fifo_write(&ff, &temp);
  TEST_ASSERT_FALSE(tu_fifo_empty(&ff));
}

void test_full(void)
{
  TEST_ASSERT_FALSE(tu_fifo_full(&ff));

  for(uint8_t i=0; i < FIFO_SIZE; i++) tu_fifo_write(&ff, &i);

  TEST_ASSERT_TRUE(tu_fifo_full(&ff));
}
