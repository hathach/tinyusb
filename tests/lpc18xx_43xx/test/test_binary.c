/**************************************************************************/
/*!
    @file     test_binary.c
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

#include <stdio.h>
#include "unity.h"
#include "common/binary.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_binary_8(void)
{
  TEST_ASSERT_EQUAL_HEX8(0x00, TU_BIN8(00000000));
  TEST_ASSERT_EQUAL_HEX8(0x01, TU_BIN8(00000001));
  TEST_ASSERT_EQUAL_HEX8(0x02, TU_BIN8(00000010));
  TEST_ASSERT_EQUAL_HEX8(0x04, TU_BIN8(00000100));
  TEST_ASSERT_EQUAL_HEX8(0x08, TU_BIN8(00001000));
  TEST_ASSERT_EQUAL_HEX8(0x10, TU_BIN8(00010000));
  TEST_ASSERT_EQUAL_HEX8(0x20, TU_BIN8(00100000));
  TEST_ASSERT_EQUAL_HEX8(0x40, TU_BIN8(01000000));
  TEST_ASSERT_EQUAL_HEX8(0x80, TU_BIN8(10000000));

  TEST_ASSERT_EQUAL_HEX8(0x0f, TU_BIN8(00001111));
  TEST_ASSERT_EQUAL_HEX8(0xf0, TU_BIN8(11110000));
  TEST_ASSERT_EQUAL_HEX8(0xff, TU_BIN8(11111111));
}

void test_binary_16(void)
{
  TEST_ASSERT_EQUAL_HEX16(0x0000, TU_BIN16(00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX16(0x000f, TU_BIN16(00000000, 00001111));
  TEST_ASSERT_EQUAL_HEX16(0x00f0, TU_BIN16(00000000, 11110000));
  TEST_ASSERT_EQUAL_HEX16(0x0f00, TU_BIN16(00001111, 00000000));
  TEST_ASSERT_EQUAL_HEX16(0xf000, TU_BIN16(11110000, 00000000));
  TEST_ASSERT_EQUAL_HEX16(0xffff, TU_BIN16(11111111, 11111111));
}

void test_binary_32(void)
{
  TEST_ASSERT_EQUAL_HEX32(0x00000000, TU_BIN32(00000000, 00000000, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x0000000f, TU_BIN32(00000000, 00000000, 00000000, 00001111));
  TEST_ASSERT_EQUAL_HEX32(0x000000f0, TU_BIN32(00000000, 00000000, 00000000, 11110000));
  TEST_ASSERT_EQUAL_HEX32(0x00000f00, TU_BIN32(00000000, 00000000, 00001111, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x0000f000, TU_BIN32(00000000, 00000000, 11110000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x000f0000, TU_BIN32(00000000, 00001111, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x00f00000, TU_BIN32(00000000, 11110000, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x0f000000, TU_BIN32(00001111, 00000000, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0xf0000000, TU_BIN32(11110000, 00000000, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0xffffffff, TU_BIN32(11111111, 11111111, 11111111, 11111111));
}

void test_bit_set(void)
{
  TEST_ASSERT_EQUAL_HEX32( TU_BIN8(00001101), tu_bit_set( TU_BIN8(00001001), 2));
  TEST_ASSERT_EQUAL_HEX32( TU_BIN8(10001101), tu_bit_set( TU_BIN8(00001101), 7));
}

void test_bit_clear(void)
{
  TEST_ASSERT_EQUAL_HEX32( TU_BIN8(00001001), tu_bit_clear( TU_BIN8(00001101), 2));
  TEST_ASSERT_EQUAL_HEX32( TU_BIN8(00001101), tu_bit_clear( TU_BIN8(10001101), 7));
}

void test_bit_mask(void)
{
  TEST_ASSERT_EQUAL_HEX32(0x0000ffff, tu_bit_mask(16));
  TEST_ASSERT_EQUAL_HEX32(0x00ffffff, tu_bit_mask(24));
  TEST_ASSERT_EQUAL_HEX32(0xffffffff, tu_bit_mask(32));
}
