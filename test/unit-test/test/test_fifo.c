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

#define FIFO_SIZE 64
uint8_t   tu_ff_buf[FIFO_SIZE * sizeof(uint8_t)];
tu_fifo_t tu_ff = TU_FIFO_INIT(tu_ff_buf, FIFO_SIZE, uint8_t, false);

tu_fifo_t            *ff = &tu_ff;
tu_fifo_buffer_info_t info;

uint8_t test_data[4096];
uint8_t rd_buf[FIFO_SIZE];

void setUp(void) {
  tu_fifo_clear(ff);
  memset(&info, 0, sizeof(tu_fifo_buffer_info_t));

  for (size_t i = 0; i < sizeof(test_data); i++) {
    test_data[i] = i;
  }
  memset(rd_buf, 0, sizeof(rd_buf));
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_normal(void) {
  for (uint8_t i = 0; i < FIFO_SIZE; i++) {
    tu_fifo_write(ff, &i);
  }

  for (uint8_t i = 0; i < FIFO_SIZE; i++) {
    uint8_t c;
    tu_fifo_read(ff, &c);
    TEST_ASSERT_EQUAL(i, c);
  }
}

void test_item_size(void) {
  uint8_t   ff4_buf[FIFO_SIZE * sizeof(uint32_t)];
  tu_fifo_t ff4 = TU_FIFO_INIT(ff4_buf, FIFO_SIZE, uint32_t, false);

  uint32_t data4[2 * FIFO_SIZE];
  for (uint32_t i = 0; i < sizeof(data4) / 4; i++) {
    data4[i] = i;
  }

  // fill up fifo
  tu_fifo_write_n(&ff4, data4, FIFO_SIZE);

  uint32_t rd_buf4[FIFO_SIZE];
  uint16_t rd_count;

  // read 0 -> 4
  rd_count = tu_fifo_read_n(&ff4, rd_buf4, 5);
  TEST_ASSERT_EQUAL(5, rd_count);
  TEST_ASSERT_EQUAL_UINT32_ARRAY(data4, rd_buf4, rd_count); // 0 -> 4

  tu_fifo_write_n(&ff4, data4 + FIFO_SIZE, 5);

  // read all 5 -> 68
  rd_count = tu_fifo_read_n(&ff4, rd_buf4, FIFO_SIZE);
  TEST_ASSERT_EQUAL(FIFO_SIZE, rd_count);
  TEST_ASSERT_EQUAL_UINT32_ARRAY(data4 + 5, rd_buf4, rd_count); // 5 -> 68
}

void test_read_n(void) {
  uint16_t rd_count;

  // fill up fifo
  for (uint8_t i = 0; i < FIFO_SIZE; i++) {
    tu_fifo_write(ff, test_data + i);
  }

  // case 1: Read index + count < depth
  // read 0 -> 4
  rd_count = tu_fifo_read_n(ff, rd_buf, 5);
  TEST_ASSERT_EQUAL(5, rd_count);
  TEST_ASSERT_EQUAL_MEMORY(test_data, rd_buf, rd_count); // 0 -> 4

  // case 2: Read index + count > depth
  // write 10, 11, 12
  tu_fifo_write(ff, test_data + FIFO_SIZE);
  tu_fifo_write(ff, test_data + FIFO_SIZE + 1);
  tu_fifo_write(ff, test_data + FIFO_SIZE + 2);

  rd_count = tu_fifo_read_n(ff, rd_buf, 7);
  TEST_ASSERT_EQUAL(7, rd_count);

  TEST_ASSERT_EQUAL_MEMORY(test_data + 5, rd_buf, rd_count); // 5 -> 11

  // Should only read until empty
  TEST_ASSERT_EQUAL(FIFO_SIZE - 5 + 3 - 7, tu_fifo_read_n(ff, rd_buf, 100));
}

void test_write_n(void) {
  // case 1: wr + count < depth
  tu_fifo_write_n(ff, test_data, 32); // wr = 32, count = 32

  uint16_t rd_count;

  rd_count = tu_fifo_read_n(ff, rd_buf, 16); // wr = 32, count = 16
  TEST_ASSERT_EQUAL(16, rd_count);
  TEST_ASSERT_EQUAL_MEMORY(test_data, rd_buf, rd_count);

  // case 2: wr + count > depth
  tu_fifo_write_n(ff, test_data + 32, 40); // wr = 72 -> 8, count = 56

  tu_fifo_read_n(ff, rd_buf, 32);          // count = 24
  TEST_ASSERT_EQUAL_MEMORY(test_data + 16, rd_buf, rd_count);

  TEST_ASSERT_EQUAL(24, tu_fifo_count(ff));
}

void test_write_double_overflowed(void) {
  tu_fifo_set_overwritable(ff, true);

  uint8_t  rd_buf[FIFO_SIZE] = {0};
  uint8_t *buf               = test_data;

  // full
  buf += tu_fifo_write_n(ff, buf, FIFO_SIZE);
  TEST_ASSERT_EQUAL(FIFO_SIZE, tu_fifo_count(ff));

  // write more, should still full
  buf += tu_fifo_write_n(ff, buf, FIFO_SIZE - 8);
  TEST_ASSERT_EQUAL(FIFO_SIZE, tu_fifo_count(ff));

  // double overflowed: in total, write more than > 2*FIFO_SIZE
  buf += tu_fifo_write_n(ff, buf, 16);
  TEST_ASSERT_EQUAL(FIFO_SIZE, tu_fifo_count(ff));

  // reading back should give back data from last FIFO_SIZE write
  tu_fifo_read_n(ff, rd_buf, FIFO_SIZE);

  TEST_ASSERT_EQUAL_MEMORY(buf - 16, rd_buf + FIFO_SIZE - 16, 16);

  // TODO whole buffer should match, but we deliberately not implement it
  // TEST_ASSERT_EQUAL_MEMORY(buf-FIFO_SIZE, rd_buf, FIFO_SIZE);
}

static uint16_t help_write(uint16_t total, uint16_t n) {
  tu_fifo_write_n(ff, test_data, n);
  total = tu_min16(FIFO_SIZE, total + n);

  TEST_ASSERT_EQUAL(total, tu_fifo_count(ff));
  TEST_ASSERT_EQUAL(FIFO_SIZE - total, tu_fifo_remaining(ff));

  return total;
}

void test_write_overwritable2(void) {
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

void test_peek(void) {
  uint8_t temp;

  temp = 10;
  tu_fifo_write(ff, &temp);
  temp = 20;
  tu_fifo_write(ff, &temp);
  temp = 30;
  tu_fifo_write(ff, &temp);

  temp = 0;

  tu_fifo_peek(ff, &temp);
  TEST_ASSERT_EQUAL(10, temp);

  tu_fifo_read(ff, &temp);
  tu_fifo_read(ff, &temp);

  tu_fifo_peek(ff, &temp);
  TEST_ASSERT_EQUAL(30, temp);
}

void test_get_read_info_when_no_wrap() {
  uint8_t ch = 1;

  // write 6 items
  for (uint8_t i = 0; i < 6; i++) {
    tu_fifo_write(ff, &ch);
  }

  // read 2 items
  tu_fifo_read(ff, &ch);
  tu_fifo_read(ff, &ch);

  tu_fifo_get_read_info(ff, &info);

  TEST_ASSERT_EQUAL(4, info.linear.len);
  TEST_ASSERT_EQUAL(0, info.wrapped.len);

  TEST_ASSERT_EQUAL_PTR(ff->buffer + 2, info.linear.ptr);
  TEST_ASSERT_NULL(info.wrapped.ptr);
}

void test_get_read_info_when_wrapped() {
  uint8_t ch = 1;

  // make fifo full
  for (uint8_t i = 0; i < FIFO_SIZE; i++) {
    tu_fifo_write(ff, &ch);
  }

  // read 6 items
  for (uint8_t i = 0; i < 6; i++) {
    tu_fifo_read(ff, &ch);
  }

  // write 2 items
  tu_fifo_write(ff, &ch);
  tu_fifo_write(ff, &ch);

  tu_fifo_get_read_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE - 6, info.linear.len);
  TEST_ASSERT_EQUAL(2, info.wrapped.len);

  TEST_ASSERT_EQUAL_PTR(ff->buffer + 6, info.linear.ptr);
  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.wrapped.ptr);
}

void test_get_write_info_when_no_wrap() {
  uint8_t ch = 1;

  // write 2 items
  tu_fifo_write(ff, &ch);
  tu_fifo_write(ff, &ch);

  tu_fifo_get_write_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE - 2, info.linear.len);
  TEST_ASSERT_EQUAL(0, info.wrapped.len);

  TEST_ASSERT_EQUAL_PTR(ff->buffer + 2, info.linear.ptr);
  // application should check len instead of ptr.
  // TEST_ASSERT_NULL(info.wrapped.ptr);
}

void test_get_write_info_when_wrapped() {
  uint8_t ch = 1;

  // write 6 items
  for (uint8_t i = 0; i < 6; i++) {
    tu_fifo_write(ff, &ch);
  }

  // read 2 items
  tu_fifo_read(ff, &ch);
  tu_fifo_read(ff, &ch);

  tu_fifo_get_write_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE - 6, info.linear.len);
  TEST_ASSERT_EQUAL(2, info.wrapped.len);

  TEST_ASSERT_EQUAL_PTR(ff->buffer + 6, info.linear.ptr);
  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.wrapped.ptr);
}

void test_empty(void) {
  uint8_t temp;
  TEST_ASSERT_TRUE(tu_fifo_empty(ff));

  // read info
  tu_fifo_get_read_info(ff, &info);

  TEST_ASSERT_EQUAL(0, info.linear.len);
  TEST_ASSERT_EQUAL(0, info.wrapped.len);

  TEST_ASSERT_NULL(info.linear.ptr);
  TEST_ASSERT_NULL(info.wrapped.ptr);

  // write info
  tu_fifo_get_write_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE, info.linear.len);
  TEST_ASSERT_EQUAL(0, info.wrapped.len);

  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.linear.ptr);
  // application should check len instead of ptr.
  // TEST_ASSERT_NULL(info.wrapped.ptr);

  // write 1 then re-check empty
  tu_fifo_write(ff, &temp);
  TEST_ASSERT_FALSE(tu_fifo_empty(ff));
}

void test_full(void) {
  TEST_ASSERT_FALSE(tu_fifo_full(ff));

  for (uint8_t i = 0; i < FIFO_SIZE; i++) {
    tu_fifo_write(ff, &i);
  }

  TEST_ASSERT_TRUE(tu_fifo_full(ff));

  // read info
  tu_fifo_get_read_info(ff, &info);

  TEST_ASSERT_EQUAL(FIFO_SIZE, info.linear.len);
  TEST_ASSERT_EQUAL(0, info.wrapped.len);

  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.linear.ptr);
  // skip this, application must check len instead of buffer
  // TEST_ASSERT_NULL(info.wrapped.ptr);

  // write info
}

void test_rd_idx_wrap(void) {
  tu_fifo_t ff10;
  uint8_t   buf[10];
  uint8_t   dst[10];

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

void test_advance_write_pointer_cases(void) {
  tu_fifo_clear(ff);

  tu_fifo_advance_write_pointer(ff, 3);
  TEST_ASSERT_EQUAL(3, ff->wr_idx);
  TEST_ASSERT_EQUAL(3, tu_fifo_count(ff));

  // advance to cross depth but stay within 0..2*depth window
  ff->wr_idx = FIFO_SIZE - 2;            // 62
  ff->rd_idx = 0;
  tu_fifo_advance_write_pointer(ff, 10); // 62 + 10 = 72 within window
  TEST_ASSERT_EQUAL(72, ff->wr_idx);
  TEST_ASSERT_EQUAL(FIFO_SIZE, tu_fifo_count(ff));

  // advance past the unused index space (beyond 2*depth)
  ff->wr_idx = (uint16_t)(2 * FIFO_SIZE - 3); // 125
  ff->rd_idx = 0;
  tu_fifo_advance_write_pointer(ff, 6);       // forces wrap across unused space
  TEST_ASSERT_EQUAL(3, ff->wr_idx);
  TEST_ASSERT_EQUAL(3, tu_fifo_count(ff));
}

void test_advance_read_pointer_cases(void) {
  tu_fifo_clear(ff);

  ff->wr_idx = 6;
  tu_fifo_advance_read_pointer(ff, 3);
  TEST_ASSERT_EQUAL(3, ff->rd_idx);
  TEST_ASSERT_EQUAL(3, tu_fifo_count(ff));

  ff->wr_idx = FIFO_SIZE + 10;          // 74
  ff->rd_idx = FIFO_SIZE - 10;          // 54
  tu_fifo_advance_read_pointer(ff, 20); // move to match write index within window
  TEST_ASSERT_EQUAL(74, ff->rd_idx);
  TEST_ASSERT_EQUAL(0, tu_fifo_count(ff));

  ff->wr_idx = 9;
  ff->rd_idx = (uint16_t)(2 * FIFO_SIZE - 1); // 127
  tu_fifo_advance_read_pointer(ff, 6);        // crosses unused index space
  TEST_ASSERT_EQUAL(5, ff->rd_idx);
  TEST_ASSERT_EQUAL(4, tu_fifo_count(ff));
}

void test_write_n_fixed_addr_rw32_nowrap(void) {
  tu_fifo_clear(ff);

  volatile uint32_t reg         = 0x11223344;
  uint8_t           expected[8] = {0x44, 0x33, 0x22, 0x11, 0x44, 0x33, 0x22, 0x11};

  for (uint8_t n = 1; n <= 8; n++) {
    tu_fifo_clear(ff);
    uint16_t written = tu_fifo_write_n_access_mode(ff, (const void *)&reg, n, TU_FIFO_FIXED_ADDR_RW32);
    TEST_ASSERT_EQUAL(n, written);
    TEST_ASSERT_EQUAL(n, tu_fifo_count(ff));

    uint8_t out[8] = {0};
    tu_fifo_read_n(ff, out, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, n);
  }
}

void test_write_n_fixed_addr_rw32_wrapped(void) {
  tu_fifo_clear(ff);

  volatile uint32_t reg         = 0xA1B2C3D4;
  uint8_t           expected[8] = {0xD4, 0xC3, 0xB2, 0xA1, 0xD4, 0xC3, 0xB2, 0xA1};

  for (uint8_t n = 1; n <= 8; n++) {
    tu_fifo_clear(ff);
    // Position the fifo near the end so writes wrap
    ff->wr_idx = FIFO_SIZE - 3;
    ff->rd_idx = FIFO_SIZE - 3;

    uint16_t written = tu_fifo_write_n_access_mode(ff, (const void *)&reg, n, TU_FIFO_FIXED_ADDR_RW32);
    TEST_ASSERT_EQUAL(n, written);
    TEST_ASSERT_EQUAL(n, tu_fifo_count(ff));

    uint8_t out[8] = {0};
    tu_fifo_read_n(ff, out, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, n);
  }
}

void test_read_n_fixed_addr_rw32_nowrap(void) {
  uint8_t pattern[8] = {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87};
  uint32_t reg_expected[8] = {
      0x00000010, 0x00002110, 0x00322110, 0x43322110, 0x00000054, 0x00006554, 0x00766554, 0x87766554};

  for (uint8_t n = 1; n <= 8; n++) {
    tu_fifo_clear(ff);
    tu_fifo_write_n(ff, pattern, 8);

    uint32_t reg      = 0;
    uint16_t read_cnt = tu_fifo_read_n_access_mode(ff, &reg, n, TU_FIFO_FIXED_ADDR_RW32);
    TEST_ASSERT_EQUAL(n, read_cnt);
    TEST_ASSERT_EQUAL(8 - n, tu_fifo_count(ff));

    TEST_ASSERT_EQUAL_HEX32(reg_expected[n - 1], reg);
  }
}

void test_read_n_fixed_addr_rw32_wrapped(void) {
  uint8_t pattern[8] = {0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87};
  uint32_t reg_expected[8] = {
      0x000000F0, 0x0000E1F0, 0x00D2E1F0, 0xC3D2E1F0, 0x000000B4, 0x0000A5B4, 0x0096A5B4, 0x8796A5B4};

  for (uint8_t n = 1; n <= 8; n++) {
    tu_fifo_clear(ff);
    ff->rd_idx = FIFO_SIZE - 2;
    ff->wr_idx = (uint16_t)(ff->rd_idx + n);

    for (uint8_t i = 0; i < n; i++) {
      uint8_t idx = (uint8_t)((ff->rd_idx + i) % FIFO_SIZE);
      ff->buffer[idx] = pattern[i];
    }

    uint32_t reg      = 0;
    uint16_t read_cnt = tu_fifo_read_n_access_mode(ff, &reg, n, TU_FIFO_FIXED_ADDR_RW32);
    TEST_ASSERT_EQUAL(n, read_cnt);
    TEST_ASSERT_EQUAL(0, tu_fifo_count(ff));

    TEST_ASSERT_EQUAL_HEX32(reg_expected[n - 1], reg);
  }
}

void test_get_read_info_advanced_cases(void) {
  tu_fifo_clear(ff);

  ff->wr_idx = 20;
  ff->rd_idx = 2;
  tu_fifo_get_read_info(ff, &info);
  TEST_ASSERT_EQUAL(18, info.linear.len);
  TEST_ASSERT_EQUAL(0, info.wrapped.len);
  TEST_ASSERT_EQUAL_PTR(ff->buffer + 2, info.linear.ptr);
  TEST_ASSERT_NULL(info.wrapped.ptr);

  ff->wr_idx = 68; // ptr = 4
  ff->rd_idx = 56; // ptr = 56
  tu_fifo_get_read_info(ff, &info);
  TEST_ASSERT_EQUAL(8, info.linear.len);
  TEST_ASSERT_EQUAL(4, info.wrapped.len);
  TEST_ASSERT_EQUAL_PTR(ff->buffer + 56, info.linear.ptr);
  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.wrapped.ptr);
}

void test_get_write_info_advanced_cases(void) {
  tu_fifo_clear(ff);

  ff->wr_idx = 10;
  ff->rd_idx = 104; // ptr = 40
  tu_fifo_get_write_info(ff, &info);
  TEST_ASSERT_EQUAL(30, info.linear.len);
  TEST_ASSERT_EQUAL(0, info.wrapped.len);
  TEST_ASSERT_EQUAL_PTR(ff->buffer + 10, info.linear.ptr);
  TEST_ASSERT_NULL(info.wrapped.ptr);

  ff->wr_idx = 60;
  ff->rd_idx = 20;
  tu_fifo_get_write_info(ff, &info);
  TEST_ASSERT_EQUAL(4, info.linear.len);
  TEST_ASSERT_EQUAL(20, info.wrapped.len);
  TEST_ASSERT_EQUAL_PTR(ff->buffer + 60, info.linear.ptr);
  TEST_ASSERT_EQUAL_PTR(ff->buffer, info.wrapped.ptr);
}

void test_correct_read_pointer_cases(void) {
  tu_fifo_clear(ff);

  // wr beyond depth: rd should be wr - depth
  ff->wr_idx = FIFO_SIZE + 6; // 70
  tu_fifo_correct_read_pointer(ff);
  TEST_ASSERT_EQUAL(6, ff->rd_idx);

  // wr exactly at depth: rd should wrap to zero
  ff->wr_idx = FIFO_SIZE;
  tu_fifo_correct_read_pointer(ff);
  TEST_ASSERT_EQUAL(0, ff->rd_idx);

  // wr below depth: rd should be wr + depth
  ff->wr_idx = 10;
  tu_fifo_correct_read_pointer(ff);
  TEST_ASSERT_EQUAL(FIFO_SIZE + 10, ff->rd_idx);
}
