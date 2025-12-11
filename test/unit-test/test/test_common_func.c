/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023, Ha Thach (tinyusb.org)
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

#include "tusb_common.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+


//------------- IMPLEMENTATION -------------//

void setUp(void)
{
}

void tearDown(void)
{
}

void test_TU_ARGS_NUM(void)
{
  TEST_ASSERT_EQUAL( 0, TU_ARGS_NUM());
  TEST_ASSERT_EQUAL( 1, TU_ARGS_NUM(a1));
  TEST_ASSERT_EQUAL( 2, TU_ARGS_NUM(a1, a2));
  TEST_ASSERT_EQUAL( 3, TU_ARGS_NUM(a1, a2, a3));
  TEST_ASSERT_EQUAL( 4, TU_ARGS_NUM(a1, a2, a3, a4));
  TEST_ASSERT_EQUAL( 5, TU_ARGS_NUM(a1, a2, a3, a4, a5));
  TEST_ASSERT_EQUAL( 6, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6));
  TEST_ASSERT_EQUAL( 7, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7));
  TEST_ASSERT_EQUAL( 8, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8));
  TEST_ASSERT_EQUAL( 9, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9));
  TEST_ASSERT_EQUAL(10, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10));
  TEST_ASSERT_EQUAL(11, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11));
  TEST_ASSERT_EQUAL(12, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12));
  TEST_ASSERT_EQUAL(13, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13));
  TEST_ASSERT_EQUAL(14, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14));
  TEST_ASSERT_EQUAL(15, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15));
  TEST_ASSERT_EQUAL(16, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16));
  TEST_ASSERT_EQUAL(17, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17));
  TEST_ASSERT_EQUAL(18, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18));
  TEST_ASSERT_EQUAL(19, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19));
  TEST_ASSERT_EQUAL(20, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20));
  TEST_ASSERT_EQUAL(21, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21));
  TEST_ASSERT_EQUAL(22, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22));
  TEST_ASSERT_EQUAL(23, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23));
  TEST_ASSERT_EQUAL(24, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24));
  TEST_ASSERT_EQUAL(25, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25));
  TEST_ASSERT_EQUAL(26, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26));
  TEST_ASSERT_EQUAL(27, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27));
  TEST_ASSERT_EQUAL(28, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28));
  TEST_ASSERT_EQUAL(29, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29));
  TEST_ASSERT_EQUAL(30, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30));
  TEST_ASSERT_EQUAL(31, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31));
  TEST_ASSERT_EQUAL(32, TU_ARGS_NUM(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, a32));
}

void test_tu_scatter_read32(void) {
  // Test data: 0x04030201
  uint8_t buf1[] = {0x01, 0x02, 0x03, 0x04};
  uint8_t buf2[] = {0x05, 0x06, 0x07, 0x08};

  // len1=1, len2=0: read 1 byte from buf1
  TEST_ASSERT_EQUAL_HEX32(0x01, tu_scatter_read32(buf1, 1, buf2, 0));

  // len1=1, len2=1: read 1 byte from buf1, 1 byte from buf2
  TEST_ASSERT_EQUAL_HEX32(0x0501, tu_scatter_read32(buf1, 1, buf2, 1));

  // len1=1, len2=2: read 1 byte from buf1, 2 bytes from buf2
  TEST_ASSERT_EQUAL_HEX32(0x060501, tu_scatter_read32(buf1, 1, buf2, 2));

  // len1=1, len2=3: read 1 byte from buf1, 3 bytes from buf2
  TEST_ASSERT_EQUAL_HEX32(0x07060501, tu_scatter_read32(buf1, 1, buf2, 3));

  // len1=2, len2=0: read 2 bytes from buf1
  TEST_ASSERT_EQUAL_HEX32(0x0201, tu_scatter_read32(buf1, 2, buf2, 0));

  // len1=2, len2=1: read 2 bytes from buf1, 1 byte from buf2
  TEST_ASSERT_EQUAL_HEX32(0x050201, tu_scatter_read32(buf1, 2, buf2, 1));

  // len1=2, len2=2: read 2 bytes from buf1, 2 bytes from buf2
  TEST_ASSERT_EQUAL_HEX32(0x06050201, tu_scatter_read32(buf1, 2, buf2, 2));

  // len1=3, len2=0: read 3 bytes from buf1
  TEST_ASSERT_EQUAL_HEX32(0x030201, tu_scatter_read32(buf1, 3, buf2, 0));

  // len1=3, len2=1: read 3 bytes from buf1, 1 byte from buf2
  TEST_ASSERT_EQUAL_HEX32(0x05030201, tu_scatter_read32(buf1, 3, buf2, 1));
}

void test_tu_scatter_write32(void) {
  uint8_t buf1[4];
  uint8_t buf2[4];

  // len1=1, len2=0: write 1 byte to buf1
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x01, buf1, 1, buf2, 0);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, buf2[0]);

  // len1=1, len2=1: write 1 byte to buf1, 1 byte to buf2
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x0201, buf1, 1, buf2, 1);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf2[0]);

  // len1=1, len2=2: write 1 byte to buf1, 2 bytes to buf2
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x030201, buf1, 1, buf2, 2);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf2[0]);
  TEST_ASSERT_EQUAL_HEX8(0x03, buf2[1]);

  // len1=1, len2=3: write 1 byte to buf1, 3 bytes to buf2
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x04030201, buf1, 1, buf2, 3);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf2[0]);
  TEST_ASSERT_EQUAL_HEX8(0x03, buf2[1]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf2[2]);

  // len1=2, len2=0: write 2 bytes to buf1
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x0201, buf1, 2, buf2, 0);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf1[1]);

  // len1=2, len2=1: write 2 bytes to buf1, 1 byte to buf2
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x030201, buf1, 2, buf2, 1);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf1[1]);
  TEST_ASSERT_EQUAL_HEX8(0x03, buf2[0]);

  // len1=2, len2=2: write 2 bytes to buf1, 2 bytes to buf2
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x04030201, buf1, 2, buf2, 2);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf1[1]);
  TEST_ASSERT_EQUAL_HEX8(0x03, buf2[0]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf2[1]);

  // len1=3, len2=0: write 3 bytes to buf1
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x030201, buf1, 3, buf2, 0);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf1[1]);
  TEST_ASSERT_EQUAL_HEX8(0x03, buf1[2]);

  // len1=3, len2=1: write 3 bytes to buf1, 1 byte to buf2
  memset(buf1, 0, sizeof(buf1));
  memset(buf2, 0, sizeof(buf2));
  tu_scatter_write32(0x04030201, buf1, 3, buf2, 1);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf1[0]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf1[1]);
  TEST_ASSERT_EQUAL_HEX8(0x03, buf1[2]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf2[0]);
}
