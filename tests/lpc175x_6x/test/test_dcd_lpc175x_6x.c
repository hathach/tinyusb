/**************************************************************************/
/*!
    @file     test_dcd.c
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include <stdlib.h>
#include "unity.h"
#include "errors.h"
#include "type_helper.h"

#include "dcd_lpc175x_6x.h"

extern dcd_dma_descriptor_t* dcd_udca[32];

void setUp(void)
{

}

void tearDown(void)
{
}

void test_dd_udca_align(void)
{
  TEST_ASSERT_BITS_LOW(128-1, (uint32_t) dcd_udca);
}

void test_dd_structure(void)
{
  //------------- word 0 -------------//
  TEST_ASSERT_EQUAL( 0, offsetof(dcd_dma_descriptor_t, next));

  //------------- word 1 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, mode) );
  TEST_ASSERT_EQUAL( 2, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, is_next_valid) );
  TEST_ASSERT_EQUAL( 4, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, is_isochronous) );
  TEST_ASSERT_EQUAL( 5, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, max_packet_size) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, buffer_length) );

  //------------- word 2 -------------//
  TEST_ASSERT_EQUAL( 8, offsetof(dcd_dma_descriptor_t, buffer_start_addr) );

  //------------- word 3 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, is_retired) );
  TEST_ASSERT_EQUAL( 1, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, status) );
  TEST_ASSERT_EQUAL( 5, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, iso_last_packet_valid) );
  TEST_ASSERT_EQUAL( 6, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, atle_is_lsb_extracted) );
  TEST_ASSERT_EQUAL( 7, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, atle_is_msb_extracted) );
  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, atle_message_length_position) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, present_count) );

  //------------- word 4 -------------//
}

