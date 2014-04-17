/**************************************************************************/
/*!
    @file     test_assertion.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#define _TEST_ASSERT_

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "unity.h"
#include "CException.h"
#include "binary.h"
#include "common/assertion.h"
#include "tusb_errors.h"

CEXCEPTION_T e;

void setUp(void)
{
  e = 0;
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Error Status
//--------------------------------------------------------------------+
void test_assert_status(void)
{
  Try
  {
    ASSERT_STATUS(TUSB_ERROR_NONE);
    ASSERT_STATUS(TUSB_ERROR_INVALID_PARA);
  }
  Catch (e)
  {
  }
}

//--------------------------------------------------------------------+
// Logical
//--------------------------------------------------------------------+
void test_assert_logical_true(void)
{
  Try
  {
    ASSERT       (true , __LINE__);
    ASSERT_TRUE  (true , __LINE__);
    ASSERT_TRUE  (false, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

void test_assert_logical_false(void)
{
  Try
  {
    ASSERT_FALSE (false, __LINE__);
    ASSERT_FALSE (true , 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}
//--------------------------------------------------------------------+
// Pointer
//--------------------------------------------------------------------+
void test_assert_pointer_not_null(void)
{
  uint32_t n;

  Try
  {
    ASSERT_PTR_NOT_NULL(&n, __LINE__);
    ASSERT_PTR(&n, __LINE__);
    ASSERT_PTR(NULL, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

void test_assert_pointer_null(void)
{
  uint32_t n;

  Try
  {
    ASSERT_PTR_NULL(NULL, __LINE__);
    ASSERT_PTR_NULL(&n, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

//--------------------------------------------------------------------+
// Integer
//--------------------------------------------------------------------+
void test_assert_int_eqal(void)
{
  Try
  {
    ASSERT_INT       (1, 1, __LINE__);
    ASSERT_INT_EQUAL (1, 1, __LINE__);

    // test side effect
    uint32_t x = 0;
    uint32_t y = 0;
    ASSERT_INT (x++, y++, __LINE__);
    TEST_ASSERT_EQUAL(1, x);
    TEST_ASSERT_EQUAL(1, y);

    ASSERT_INT (1, 0, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

void test_assert_int_within(void)
{
  Try
  {
    ASSERT_INT_WITHIN (1, 5, 3, __LINE__);
    ASSERT_INT_WITHIN (1, 5, 1, __LINE__);
    ASSERT_INT_WITHIN (1, 5, 5, __LINE__);

    ASSERT_INT_WITHIN (1, 5, 0, 0);
    ASSERT_INT_WITHIN (1, 5, 10, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

void test_assert_int_within_greater(void)
{
  Try
  {
    ASSERT_INT_WITHIN (1, 5, 10, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }

}

void test_assert_int_within_less(void)
{
  Try
  {
    ASSERT_INT_WITHIN (1, 5, 0, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }

}

//--------------------------------------------------------------------+
// HEX
//--------------------------------------------------------------------+
void test_assert_hex_equal(void)
{
  Try
  {
    ASSERT_HEX       (0xffee, 0xffee, __LINE__);
    ASSERT_HEX_EQUAL (0xffee, 0xffee, __LINE__);

    // test side effect
    uint32_t x = 0xf0f0;
    uint32_t y = 0xf0f0;
    ASSERT_HEX (x++, y++, __LINE__);
    TEST_ASSERT_EQUAL(0xf0f1, x);
    TEST_ASSERT_EQUAL(0xf0f1, y);

    ASSERT_HEX(0x1234, 0x4321, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

void test_assert_hex_within(void)
{
  Try
  {
    ASSERT_HEX_WITHIN (0xff00, 0xffff, 0xff11, __LINE__);
    ASSERT_HEX_WITHIN (0xff00, 0xffff, 0xff00, __LINE__);
    ASSERT_HEX_WITHIN (0xff00, 0xffff, 0xffff, __LINE__);
  }
  Catch (e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

void test_assert_hex_within_less(void)
{
  Try
  {
    ASSERT_HEX_WITHIN (0xff00, 0xffff, 0xeeee, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

void test_assert_hex_within_greater(void)
{
  Try
  {
    ASSERT_HEX_WITHIN (0xff00, 0xffff, 0x1eeee, 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}

//--------------------------------------------------------------------+
// BIN
//--------------------------------------------------------------------+
void test_assert_bin_equal(void)
{
  Try
  {
    ASSERT_BIN8       (BIN8(11110000), BIN8(11110000), __LINE__);
    ASSERT_BIN8_EQUAL (BIN8(00001111), BIN8(00001111), __LINE__);

    // test side effect
    uint32_t x = BIN8(11001100);
    uint32_t y = BIN8(11001100);
    ASSERT_BIN8 (x++, y++, __LINE__);
    TEST_ASSERT_EQUAL(BIN8(11001101), x);
    TEST_ASSERT_EQUAL(BIN8(11001101), y);

    ASSERT_BIN8(BIN8(11001111), BIN8(11111100), 0);
  }
  Catch(e)
  {
    TEST_ASSERT_EQUAL(0, e);
  }
}





