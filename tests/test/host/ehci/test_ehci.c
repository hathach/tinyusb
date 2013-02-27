/*
 * test_ehci.c
 *
 *  Created on: Feb 27, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#include "unity.h"
#include "tusb_option.h"
#include "errors.h"
#include "binary.h"
#include "ehci.h"

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
int8_t first_pos_of_high_bit(uint32_t value);
uint8_t number_of_high_bits(uint32_t value);

#define BITFIELD_OFFSET_OF_MEMBER(struct_type, member, bitfield_member) \
  ({\
    uint32_t value=0;\
    struct_type str;\
    memclr_(&str, sizeof(struct_type));\
    str.member.bitfield_member = 1;\
    memcpy(&value, &str.member, sizeof(str.member));\
    first_pos_of_high_bit( value );\
  })

#define BITFIELD_OFFSET_OF_UINT32(struct_type, offset, bitfield_member) \
  ({\
    struct_type str;\
    memclr_(&str, sizeof(struct_type));\
    str.bitfield_member = 1;\
    first_pos_of_high_bit( ((uint32_t*) &str)[offset] );\
  })

void setUp(void)
{
}

void tearDown(void)
{
}


void test_struct_alignment(void)
{
  TEST_ASSERT_EQUAL( 32, __alignof__(ehci_qhd_t) );
//  TEST_ASSERT_EQUAL( 32, __alignof__(ehci_qtd_t) ); ehci_qtd_t is used to declare overlay variable in qhd --> cannot declare with ATTR_ALIGNED(32)

  TEST_ASSERT_EQUAL( 32, __alignof__(ehci_itd_t) );
  TEST_ASSERT_EQUAL( 32, __alignof__(ehci_sitd_t) );

}

void test_struct_size(void)
{
  TEST_ASSERT_EQUAL( 64, sizeof(ehci_qhd_t) );
  TEST_ASSERT_EQUAL( 32, sizeof(ehci_qtd_t) );

  TEST_ASSERT_EQUAL( 64, sizeof(ehci_itd_t) );
  TEST_ASSERT_EQUAL( 32, sizeof(ehci_sitd_t) );

  TEST_ASSERT_EQUAL( 4, sizeof(ehci_link_t) );
}

void test_qtd_structure(void)
{
  TEST_ASSERT_EQUAL( 0, offsetof(ehci_qtd_t, next));
  TEST_ASSERT_EQUAL( 4, offsetof(ehci_qtd_t, alternate));

  //------------- Word 2 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, pingstate_err) );
  TEST_ASSERT_EQUAL( 1, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, split_state) );
  TEST_ASSERT_EQUAL( 2, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, missed_uframe));
  TEST_ASSERT_EQUAL( 3, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, xact_err) );
  TEST_ASSERT_EQUAL( 4, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, babble_err) );
  TEST_ASSERT_EQUAL( 5, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, buffer_err) );
  TEST_ASSERT_EQUAL( 6, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, halted) );
  TEST_ASSERT_EQUAL( 7, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, active) );
  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, pid) );
  TEST_ASSERT_EQUAL( 10, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, cerr) );
  TEST_ASSERT_EQUAL( 12, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, current_page) );
  TEST_ASSERT_EQUAL( 15, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, int_on_complete) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, total_bytes) );
  TEST_ASSERT_EQUAL( 31, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, data_toggle) );

  TEST_ASSERT_EQUAL( 12, offsetof(ehci_qtd_t, buffer));
}

void test_qhd_structure(void)
{
  TEST_ASSERT_EQUAL( 0, offsetof(ehci_qhd_t, next));

  //------------- Word 1 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, device_address) );
  TEST_ASSERT_EQUAL( 7, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, inactive_next_xact) );
  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, endpoint_number) );
  TEST_ASSERT_EQUAL( 12, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, endpoint_speed) );
  TEST_ASSERT_EQUAL( 14, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, data_toggle_control) );
  TEST_ASSERT_EQUAL( 15, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, head_list_flag) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, max_package_size) );
  TEST_ASSERT_EQUAL( 27, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, control_endpoint_flag) );
  TEST_ASSERT_EQUAL( 28, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, nak_count_reload) );

  //------------- Word 2 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, smask) );
  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, cmask) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, hub_address) );
  TEST_ASSERT_EQUAL( 23, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, port_number) );
  TEST_ASSERT_EQUAL( 30, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, mult) );

  TEST_ASSERT_EQUAL( 3*4, offsetof(ehci_qhd_t, qtd_addr));
  TEST_ASSERT_EQUAL( 4*4, offsetof(ehci_qhd_t, qtd_overlay));
}

void test_itd_structure(void)
{
  TEST_ASSERT_EQUAL( 0, offsetof(ehci_itd_t, next));

  // Each Transaction Word
  TEST_ASSERT_EQUAL( 0  , BITFIELD_OFFSET_OF_MEMBER(ehci_itd_t, xact[0], offset) );
  TEST_ASSERT_EQUAL( 12 , BITFIELD_OFFSET_OF_MEMBER(ehci_itd_t, xact[0], page_select) );
  TEST_ASSERT_EQUAL( 15 , BITFIELD_OFFSET_OF_MEMBER(ehci_itd_t, xact[0], int_on_complete) );
  TEST_ASSERT_EQUAL( 16 , BITFIELD_OFFSET_OF_MEMBER(ehci_itd_t, xact[0], length) );
  TEST_ASSERT_EQUAL( 28 , BITFIELD_OFFSET_OF_MEMBER(ehci_itd_t, xact[0], error) );
  TEST_ASSERT_EQUAL( 29 , BITFIELD_OFFSET_OF_MEMBER(ehci_itd_t, xact[0], babble_err) );
  TEST_ASSERT_EQUAL( 30 , BITFIELD_OFFSET_OF_MEMBER(ehci_itd_t, xact[0], buffer_err) );
  TEST_ASSERT_EQUAL( 31 , BITFIELD_OFFSET_OF_MEMBER(ehci_itd_t, xact[0], active) );

  TEST_ASSERT_EQUAL( 9*4, offsetof(ehci_itd_t, BufferPointer));
}

void test_sitd_structure(void)
{
  TEST_ASSERT_EQUAL( 0, offsetof(ehci_sitd_t, next));

  //------------- Word 1 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 1, device_address) );
  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 1, endpoint_number) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 1, hub_address) );
  TEST_ASSERT_EQUAL( 24, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 1, port_number) );
  TEST_ASSERT_EQUAL( 31, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 1, direction) );

  //------------- Word 2 -------------//
  TEST_ASSERT_EQUAL( 4*2, offsetof(ehci_sitd_t, smask));
  TEST_ASSERT_EQUAL( 4*2+1, offsetof(ehci_sitd_t, cmask));

  //------------- Word 3 -------------//
  TEST_ASSERT_EQUAL( 1, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, split_state) );
  TEST_ASSERT_EQUAL( 2, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, missed_uframe));
  TEST_ASSERT_EQUAL( 3, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, xact_err) );
  TEST_ASSERT_EQUAL( 4, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, babble_err) );
  TEST_ASSERT_EQUAL( 5, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, buffer_err) );
  TEST_ASSERT_EQUAL( 6, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, error) );
  TEST_ASSERT_EQUAL( 7, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, active) );

  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, cmask_progress) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, total_bytes) );
  TEST_ASSERT_EQUAL( 30, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, page_select) );
  TEST_ASSERT_EQUAL( 31, BITFIELD_OFFSET_OF_UINT32(ehci_sitd_t, 3, int_on_complete) );

  //------------- Word 4 -------------//
  TEST_ASSERT_EQUAL( 4*4, offsetof(ehci_sitd_t, buffer));

  TEST_ASSERT_EQUAL( 4*6, offsetof(ehci_sitd_t, back));
}

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
int8_t first_pos_of_high_bit(uint32_t value)
{
  for (int8_t i=0; i<32; i++)
  {
    if (value & BIT_(i))
      return i;
  }
  return (-1);
}

uint8_t number_of_high_bits(uint32_t value)
{
  uint8_t result=0;
  for(uint8_t i=0; i<32; i++)
  {
    if (value & BIT_(i))
      result++;
  }
  return result;
}
