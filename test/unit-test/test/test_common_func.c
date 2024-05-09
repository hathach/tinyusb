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
