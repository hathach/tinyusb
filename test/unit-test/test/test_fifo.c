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

#include "osal/osal.h"
#include "tusb_fifo.h"

#define FIFO_SIZE   64
uint8_t tu_ff_buf[FIFO_SIZE * sizeof(uint8_t)];
tu_fifo_t tu_ff = TU_FIFO_INIT(tu_ff_buf, FIFO_SIZE, uint8_t, false);

tu_fifo_t* ff = &tu_ff;
tu_fifo_buffer_info_t info;

uint8_t test_data[4096];
uint8_t rd_buf[FIFO_SIZE];

void setUp(void)
{
  tu_fifo_clear(ff);
  memset(&info, 0, sizeof(tu_fifo_buffer_info_t));

  for(int i=0; i<sizeof(test_data); i++) test_data[i] = i;
  memset(rd_buf, 0, sizeof(rd_buf));
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
  uint8_t ff4_buf[FIFO_SIZE * sizeof(uint32_t)];
  tu_fifo_t ff4 = TU_FIFO_INIT(ff4_buf, FIFO_SIZE, uint32_t, false);

  uint32_t data4[2*FIFO_SIZE];
  for(uint32_t i=0; i<sizeof(data4)/4; i++) data4[i] = i;

  // fill up fifo
  tu_fifo_write_n(&ff4, data4, FIFO_SIZE);

  uint32_t rd_buf4[FIFO_SIZE];
  uint16_t rd_count;

  // read 0 -> 4
  rd_count = tu_fifo_read_n(&ff4, rd_buf4, 5);
  TEST_ASSERT_EQUAL( 5, rd_count );
  TEST_ASSERT_EQUAL_UINT32_ARRAY( data4, rd_buf4, rd_count ); // 0 -> 4

  tu_fifo_write_n(&ff4, data4+FIFO_SIZE, 5);

  // read all 5 -> 68
  rd_count = tu_fifo_read_n(&ff4, rd_buf4, FIFO_SIZE);
  TEST_ASSERT_EQUAL( FIFO_SIZE, rd_count );
  TEST_ASSERT_EQUAL_UINT32_ARRAY( data4+5, rd_buf4, rd_count ); // 5 -> 68
}

void test_read_n(void)
{
  uint16_t rd_count;

  // fill up fifo
  for(uint8_t i=0; i < FIFO_SIZE; i++) tu_fifo_write(ff, test_data+i);

  // case 1: Read index + count < depth
  // read 0 -> 4
  rd_count = tu_fifo_read_n(ff, rd_buf, 5);
  TEST_ASSERT_EQUAL( 5, rd_count );
  TEST_ASSERT_EQUAL_MEMORY( test_data, rd_buf, rd_count ); // 0 -> 4

  // case 2: Read index + count > depth
  // write 10, 11, 12
  tu_fifo_write(ff, test_data+FIFO_SIZE);
  tu_fifo_write(ff, test_data+FIFO_SIZE+1);
  tu_fifo_write(ff, test_data+FIFO_SIZE+2);

  rd_count = tu_fifo_read_n(ff, rd_buf, 7);
  TEST_ASSERT_EQUAL( 7, rd_count );

  TEST_ASSERT_EQUAL_MEMORY( test_data+5, rd_buf, rd_count ); // 5 -> 11

  // Should only read until empty
  TEST_ASSERT_EQUAL( FIFO_SIZE-5+3-7, tu_fifo_read_n(ff, rd_buf, 100) );
}

void test_write_n(void)
{
  // case 1: wr + count < depth
  tu_fifo_write_n(ff, test_data, 32); // wr = 32, count = 32

  uint16_t rd_count;

  rd_count = tu_fifo_read_n(ff, rd_buf, 16); // wr = 32, count = 16
  TEST_ASSERT_EQUAL( 16, rd_count );
  TEST_ASSERT_EQUAL_MEMORY( test_data, rd_buf, rd_count );

  // case 2: wr + count > depth
  tu_fifo_write_n(ff, test_data+32, 40); // wr = 72 -> 8, count = 56

  tu_fifo_read_n(ff, rd_buf, 32); // count = 24
  TEST_ASSERT_EQUAL_MEMORY( test_data+16, rd_buf, rd_count);

  TEST_ASSERT_EQUAL(24, tu_fifo_count(ff));
}

void test_write_double_overflowed(void)
{
  tu_fifo_set_overwritable(ff, true);

  uint8_t rd_buf[FIFO_SIZE] = { 0 };
  uint8_t* buf = test_data;

  // full
  buf += tu_fifo_write_n(ff, buf, FIFO_SIZE);
  TEST_ASSERT_EQUAL(FIFO_SIZE, tu_fifo_count(ff));

  // write more, should still full
  buf += tu_fifo_write_n(ff, buf, FIFO_SIZE-8);
  TEST_ASSERT_EQUAL(FIFO_SIZE, tu_fifo_count(ff));

  // double overflowed: in total, write more than > 2*FIFO_SIZE
  buf += tu_fifo_write_n(ff, buf, 16);
  TEST_ASSERT_EQUAL(FIFO_SIZE, tu_fifo_count(ff));

  // reading back should give back data from last FIFO_SIZE write
  tu_fifo_read_n(ff, rd_buf, FIFO_SIZE);

  TEST_ASSERT_EQUAL_MEMORY(buf-16, rd_buf+FIFO_SIZE-16, 16);

  // TODO whole buffer should match, but we deliberately not implement it
  // TEST_ASSERT_EQUAL_MEMORY(buf-FIFO_SIZE, rd_buf, FIFO_SIZE);
}

static uint16_t help_write(uint16_t total, uint16_t n)
{
  tu_fifo_write_n(ff, test_data, n);
  total = tu_min16(FIFO_SIZE, total + n);

  TEST_ASSERT_EQUAL(total, tu_fifo_count(ff));
  TEST_ASSERT_EQUAL(FIFO_SIZE - total, tu_fifo_remaining(ff));

  return total;
}

void test_write_overwritable2(void)
{
  tu_fifo_set_overwritable(ff, true);

  // based on actual crash tests detected by fuzzing
  uint16_t total = 0;

  total = help_write(total, 12);
  total = help_write(total, 55);
  total = help_write(total, 73);
  total = help_write(total, 55);
  total = help_write(total, 75);
  total = help_write(total, 84);
  total = help_write(total, 1);
  total = help_write(total, 10);
  total = help_write(total, 12);
  total = help_write(total, 25);
  total = help_write(total, 192);
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
