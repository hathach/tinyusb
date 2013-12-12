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
  TEST_ASSERT_EQUAL_HEX8(0x00, BIN8(00000000));
  TEST_ASSERT_EQUAL_HEX8(0x01, BIN8(00000001));
  TEST_ASSERT_EQUAL_HEX8(0x02, BIN8(00000010));
  TEST_ASSERT_EQUAL_HEX8(0x04, BIN8(00000100));
  TEST_ASSERT_EQUAL_HEX8(0x08, BIN8(00001000));
  TEST_ASSERT_EQUAL_HEX8(0x10, BIN8(00010000));
  TEST_ASSERT_EQUAL_HEX8(0x20, BIN8(00100000));
  TEST_ASSERT_EQUAL_HEX8(0x40, BIN8(01000000));
  TEST_ASSERT_EQUAL_HEX8(0x80, BIN8(10000000));

  TEST_ASSERT_EQUAL_HEX8(0x0f, BIN8(00001111));
  TEST_ASSERT_EQUAL_HEX8(0xf0, BIN8(11110000));
  TEST_ASSERT_EQUAL_HEX8(0xff, BIN8(11111111));
}

void test_binary_16(void)
{
  TEST_ASSERT_EQUAL_HEX16(0x0000, BIN16(00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX16(0x000f, BIN16(00000000, 00001111));
  TEST_ASSERT_EQUAL_HEX16(0x00f0, BIN16(00000000, 11110000));
  TEST_ASSERT_EQUAL_HEX16(0x0f00, BIN16(00001111, 00000000));
  TEST_ASSERT_EQUAL_HEX16(0xf000, BIN16(11110000, 00000000));
  TEST_ASSERT_EQUAL_HEX16(0xffff, BIN16(11111111, 11111111));
}

void test_binary_32(void)
{
  TEST_ASSERT_EQUAL_HEX32(0x00000000, BIN32(00000000, 00000000, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x0000000f, BIN32(00000000, 00000000, 00000000, 00001111));
  TEST_ASSERT_EQUAL_HEX32(0x000000f0, BIN32(00000000, 00000000, 00000000, 11110000));
  TEST_ASSERT_EQUAL_HEX32(0x00000f00, BIN32(00000000, 00000000, 00001111, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x0000f000, BIN32(00000000, 00000000, 11110000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x000f0000, BIN32(00000000, 00001111, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x00f00000, BIN32(00000000, 11110000, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0x0f000000, BIN32(00001111, 00000000, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0xf0000000, BIN32(11110000, 00000000, 00000000, 00000000));
  TEST_ASSERT_EQUAL_HEX32(0xffffffff, BIN32(11111111, 11111111, 11111111, 11111111));
}

void test_bit_set(void)
{
  TEST_ASSERT_EQUAL_HEX32( BIN8(00001101), bit_set( BIN8(00001001), 2));
  TEST_ASSERT_EQUAL_HEX32( BIN8(10001101), bit_set( BIN8(00001101), 7));
}

void test_bit_clear(void)
{
  TEST_ASSERT_EQUAL_HEX32( BIN8(00001001), bit_clear( BIN8(00001101), 2));
  TEST_ASSERT_EQUAL_HEX32( BIN8(00001101), bit_clear( BIN8(10001101), 7));
}

void test_bit_mask(void)
{
  TEST_ASSERT_EQUAL_HEX32(0x0000ffff, bit_mask(16));
  TEST_ASSERT_EQUAL_HEX32(0x00ffffff, bit_mask(24));
  TEST_ASSERT_EQUAL_HEX32(0xffffffff, bit_mask(32));
}

void test_bit_range(void)
{
  TEST_ASSERT_EQUAL_HEX32(BIN8(00001111), bit_mask_range(0, 3));
  TEST_ASSERT_EQUAL_HEX32(BIN8(01100000), bit_mask_range(5, 6));

  TEST_ASSERT_EQUAL_HEX32(BIN16(00001111, 00000000), bit_mask_range(8, 11));
  TEST_ASSERT_EQUAL_HEX32(0xf0000000, bit_mask_range(28, 31));
}

void test_bit_set_range(void)
{
  TEST_ASSERT_EQUAL_HEX32(BIN8(01011001), bit_set_range(BIN8(00001001), 4, 6, BIN8(101)));

  TEST_ASSERT_EQUAL_HEX32(BIN32(11001011, 10100000, 00000000, 00001001),
                          bit_set_range(BIN8(00001001), 21, 31, BIN16(110, 01011101)));
}
