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

void test_item_size(void)
{
  TU_FIFO_DEF(ff4, FIFO_SIZE, uint32_t, false);
  tu_fifo_clear(&ff4);

  uint32_t data[20];
  for(uint32_t i=0; i<sizeof(data)/4; i++) data[i] = i;

  tu_fifo_write_n(&ff4, data, 10);

  uint32_t rd[10];
  uint16_t rd_count;

  // read 0 -> 4
  rd_count = tu_fifo_read_n(&ff4, rd, 5);
  TEST_ASSERT_EQUAL( 5, rd_count );
  TEST_ASSERT_EQUAL_UINT32_ARRAY( data, rd, rd_count ); // 0 -> 4

  tu_fifo_write_n(&ff4, data+10, 5);

  // read 5 -> 14
  rd_count = tu_fifo_read_n(&ff4, rd, 10);
  TEST_ASSERT_EQUAL( 10, rd_count );
  TEST_ASSERT_EQUAL_UINT32_ARRAY( data+5, rd, rd_count ); // 5 -> 14
}

void test_read_n(void)
{
  // prepare data
  uint8_t data[20];
  for(int i=0; i<sizeof(data); i++) data[i] = i;

  for(uint8_t i=0; i < FIFO_SIZE; i++) tu_fifo_write(&ff, data+i);

  uint8_t rd[10];
  uint16_t rd_count;

  // case 1: Read index + count < depth
  // read 0 -> 4
  rd_count = tu_fifo_read_n(&ff, rd, 5);
  TEST_ASSERT_EQUAL( 5, rd_count );
  TEST_ASSERT_EQUAL_MEMORY( data, rd, rd_count ); // 0 -> 4

  // case 2: Read index + count > depth
  // write 10, 11, 12
  tu_fifo_write(&ff, data+10);
  tu_fifo_write(&ff, data+11);
  tu_fifo_write(&ff, data+12);

  rd_count = tu_fifo_read_n(&ff, rd, 7);
  TEST_ASSERT_EQUAL( 7, rd_count );

  TEST_ASSERT_EQUAL_MEMORY( data+5, rd, rd_count ); // 5 -> 11

  // Should only read until empty
  TEST_ASSERT_EQUAL( 1, tu_fifo_read_n(&ff, rd, 100) );
}

void test_write_n(void)
{
  // prepare data
  uint8_t data[20];
  for(int i=0; i<sizeof(data); i++) data[i] = i;

  // case 1: wr + count < depth
  tu_fifo_write_n(&ff, data, 8); // wr = 8, count = 8

  uint8_t rd[10];
  uint16_t rd_count;

  rd_count = tu_fifo_read_n(&ff, rd, 5); // wr = 8, count = 3
  TEST_ASSERT_EQUAL( 5, rd_count );
  TEST_ASSERT_EQUAL_MEMORY( data, rd, rd_count ); // 0 -> 4

  // case 2: wr + count > depth
  tu_fifo_write_n(&ff, data+8, 6); // wr = 3, count = 9

  for(rd_count=0; rd_count<7; rd_count++) tu_fifo_read(&ff, rd+rd_count); // wr = 3, count = 2

  TEST_ASSERT_EQUAL_MEMORY( data+5, rd, rd_count); // 5 -> 11

  TEST_ASSERT_EQUAL(2, tu_fifo_count(&ff));
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
