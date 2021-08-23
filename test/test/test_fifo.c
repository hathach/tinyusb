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
#include "tusb_fifo.h"

#define FIFO_SIZE 10
TU_FIFO_DEF(tu_ff, FIFO_SIZE, uint8_t, false);
tu_fifo_t* ff = &tu_ff;
tu_fifo_buffer_info_t info;

void setUp(void)
{
  tu_fifo_clear(ff);
  memset(&info, 0, sizeof(tu_fifo_buffer_info_t));
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_normal(void)
{
  for(uint8_t i=0; i < FIFO_SIZE; i++) tu_fifo_write(ff, &i);

  for(uint8_t i=0; i < FIFO_SIZE; i++)
  {
    uint8_t c;
    tu_fifo_read(ff, &c);
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

  for(uint8_t i=0; i < FIFO_SIZE; i++) tu_fifo_write(ff, data+i);

  uint8_t rd[10];
  uint16_t rd_count;

  // case 1: Read index + count < depth
  // read 0 -> 4
  rd_count = tu_fifo_read_n(ff, rd, 5);
  TEST_ASSERT_EQUAL( 5, rd_count );
  TEST_ASSERT_EQUAL_MEMORY( data, rd, rd_count ); // 0 -> 4

  // case 2: Read index + count > depth
  // write 10, 11, 12
  tu_fifo_write(ff, data+10);
  tu_fifo_write(ff, data+11);
  tu_fifo_write(ff, data+12);

  rd_count = tu_fifo_read_n(ff, rd, 7);
  TEST_ASSERT_EQUAL( 7, rd_count );

  TEST_ASSERT_EQUAL_MEMORY( data+5, rd, rd_count ); // 5 -> 11

  // Should only read until empty
  TEST_ASSERT_EQUAL( 1, tu_fifo_read_n(ff, rd, 100) );
}

void test_write_n(void)
{
  // prepare data
  uint8_t data[20];
  for(int i=0; i<sizeof(data); i++) data[i] = i;

  // case 1: wr + count < depth
  tu_fifo_write_n(ff, data, 8); // wr = 8, count = 8

  uint8_t rd[10];
  uint16_t rd_count;

  rd_count = tu_fifo_read_n(ff, rd, 5); // wr = 8, count = 3
  TEST_ASSERT_EQUAL( 5, rd_count );
  TEST_ASSERT_EQUAL_MEMORY( data, rd, rd_count ); // 0 -> 4

  // case 2: wr + count > depth
  tu_fifo_write_n(ff, data+8, 6); // wr = 3, count = 9

  for(rd_count=0; rd_count<7; rd_count++) tu_fifo_read(ff, rd+rd_count); // wr = 3, count = 2

  TEST_ASSERT_EQUAL_MEMORY( data+5, rd, rd_count); // 5 -> 11

  TEST_ASSERT_EQUAL(2, tu_fifo_count(ff));
}

void test_peek(void)
{
  uint8_t temp;

  temp = 10; tu_fifo_write(ff, &temp);
  temp = 20; tu_fifo_write(ff, &temp);
  temp = 30; tu_fifo_write(ff, &temp);

  temp = 0;

  tu_fifo_peek(ff, &temp);
  TEST_ASSERT_EQUAL(10, temp);

  tu_fifo_read(ff, &temp);
  tu_fifo_read(ff, &temp);

  tu_fifo_peek(ff, &temp);
  TEST_ASSERT_EQUAL(30, temp);
}

void test_get_read_info_when_no_wrap()
{
  uint8_t ch = 1;

  // write 6 items
  for(uint8_t i=0; i < 6; i++) tu_fifo_write(ff, &ch);

  // read 2 items
  tu_fifo_read(ff, &ch);
  tu_fifo_read(ff, &ch);

  tu_fifo_get_read_info(ff, &info);

  TEST_ASSERT_EQUAL(4, info.len_lin);
  TEST_ASSERT_EQUAL(0, info.len_wrap);

  TEST_ASSERT_EQUAL_PTR(ff->buffer+2, info.ptr_lin);
  TEST_ASSERT_NULL(info.ptr_wrap);
}

void test_get_read_info_when_wrapped()
{
  uint8_t ch = 1;

  // make fifo full
  for(uint8_t i=0; i < FIFO_SIZE; i++) tu_fifo_write(ff, &ch);

  // read 6 items
  for(uint8_t i=0; i < 6; i++) tu_fifo_read(ff, &ch);

  // write 2 items
  tu_fifo_write(ff, &ch);
  tu_fifo_write(ff, &ch);

  tu_fifo_get_read_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE-6, info.len_lin);
  TEST_ASSERT_EQUAL(2, info.len_wrap);

  TEST_ASSERT_EQUAL_PTR(ff->buffer+6, info.ptr_lin);
  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.ptr_wrap);
}

void test_get_write_info_when_no_wrap()
{
  uint8_t ch = 1;

  // write 2 items
  tu_fifo_write(ff, &ch);
  tu_fifo_write(ff, &ch);

  tu_fifo_get_write_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE-2, info.len_lin);
  TEST_ASSERT_EQUAL(0, info.len_wrap);

  TEST_ASSERT_EQUAL_PTR(ff->buffer+2, info .ptr_lin);
  // application should check len instead of ptr.
  // TEST_ASSERT_NULL(info.ptr_wrap);
}

void test_get_write_info_when_wrapped()
{
  uint8_t ch = 1;

  // write 6 items
  for(uint8_t i=0; i < 6; i++) tu_fifo_write(ff, &ch);

  // read 2 items
  tu_fifo_read(ff, &ch);
  tu_fifo_read(ff, &ch);

  tu_fifo_get_write_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE-6, info.len_lin);
  TEST_ASSERT_EQUAL(2, info.len_wrap);

  TEST_ASSERT_EQUAL_PTR(ff->buffer+6, info .ptr_lin);
  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.ptr_wrap);
}

void test_empty(void)
{
  uint8_t temp;
  TEST_ASSERT_TRUE(tu_fifo_empty(ff));

  // read info
  tu_fifo_get_read_info(ff, &info);

  TEST_ASSERT_EQUAL(0, info.len_lin);
  TEST_ASSERT_EQUAL(0, info.len_wrap);

  TEST_ASSERT_NULL(info.ptr_lin);
  TEST_ASSERT_NULL(info.ptr_wrap);

  // write info
  tu_fifo_get_write_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE, info.len_lin);
  TEST_ASSERT_EQUAL(0, info.len_wrap);

  TEST_ASSERT_EQUAL_PTR(ff->buffer, info .ptr_lin);
  // application should check len instead of ptr.
  // TEST_ASSERT_NULL(info.ptr_wrap);

  // write 1 then re-check empty
  tu_fifo_write(ff, &temp);
  TEST_ASSERT_FALSE(tu_fifo_empty(ff));
}

void test_full(void)
{
  TEST_ASSERT_FALSE(tu_fifo_full(ff));

  for(uint8_t i=0; i < FIFO_SIZE; i++) tu_fifo_write(ff, &i);

  TEST_ASSERT_TRUE(tu_fifo_full(ff));

  // read info
  tu_fifo_get_read_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE, info.len_lin);
  TEST_ASSERT_EQUAL(0, info.len_wrap);

  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.ptr_lin);
  // skip this, application must check len instead of buffer
  // TEST_ASSERT_NULL(info.ptr_wrap);

  // write info
}

void test_rd_idx_wrap()
{
  tu_fifo_t ff10;
  uint8_t buf[10];
  uint8_t dst[10];

  tu_fifo_config(&ff10, buf, 10, 1, 1);

  uint16_t n;

  ff10.wr_idx = 6;
  ff10.rd_idx = 15;

  n = tu_fifo_read_n(&ff10, dst, 4);
  TEST_ASSERT_EQUAL(n, 4);
  TEST_ASSERT_EQUAL(ff10.rd_idx, 0);
  n = tu_fifo_read_n(&ff10, dst, 4);
  TEST_ASSERT_EQUAL(n, 4);
  TEST_ASSERT_EQUAL(ff10.rd_idx, 4);
  n = tu_fifo_read_n(&ff10, dst, 4);
  TEST_ASSERT_EQUAL(n, 2);
  TEST_ASSERT_EQUAL(ff10.rd_idx, 6);
}