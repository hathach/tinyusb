/**************************************************************************/
/*!
    @file     test_ehci_structure.c
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

#include <stdlib.h>
#include "unity.h"
#include "tusb_option.h"
#include "tusb_errors.h"
#include "binary.h"
#include "type_helper.h"

#include "hal.h"
#include "hcd.h"
#include "ehci.h"

#include "ehci_controller_fake.h"
#include "mock_osal.h"
#include "mock_usbh_hcd.h"

usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1];

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{

}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
void test_struct_alignment(void)
{
  TEST_ASSERT_EQUAL( 32, __alignof__(ehci_qhd_t) );
//  TEST_ASSERT_EQUAL( 32, __alignof__(ehci_qtd_t) ); ehci_qtd_t is used to declare overlay variable in qhd --> cannot declare with ATTR_ALIGNED(32)

  TEST_ASSERT_EQUAL( 32, __alignof__(ehci_itd_t) );
  TEST_ASSERT_EQUAL( 32, __alignof__(ehci_sitd_t) );

}

void test_struct_size(void)
{
  if (4 < sizeof(void*)) // running tests in x64 environment
  {
    TEST_ASSERT_EQUAL( 64 - 8, offsetof(ehci_qhd_t, p_qtd_list_head) ); // 64 - 2x 32-bit pointer qtds
  }else
  {
    TEST_ASSERT_EQUAL( 64, sizeof(ehci_qhd_t) );
  }
  TEST_ASSERT_EQUAL( 32, sizeof(ehci_qtd_t) );

  TEST_ASSERT_EQUAL( 64, sizeof(ehci_itd_t) );
  TEST_ASSERT_EQUAL( 32, sizeof(ehci_sitd_t) );

  TEST_ASSERT_EQUAL( 4, sizeof(ehci_link_t) );
}

//--------------------------------------------------------------------+
// EHCI Data Structure
//--------------------------------------------------------------------+
void test_qtd_structure(void)
{
  TEST_ASSERT_EQUAL( 0, offsetof(ehci_qtd_t, next));
  TEST_ASSERT_EQUAL( 4, offsetof(ehci_qtd_t, alternate));
  TEST_ASSERT_EQUAL( 5, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 1, used));

  //------------- Word 2 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, pingstate_err) );
  TEST_ASSERT_EQUAL( 1, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, non_hs_split_state) );
  TEST_ASSERT_EQUAL( 2, BITFIELD_OFFSET_OF_UINT32(ehci_qtd_t, 2, non_hs_period_missed_uframe));
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
  TEST_ASSERT_EQUAL( 7, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, non_hs_period_inactive_next_xact) );
  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, endpoint_number) );
  TEST_ASSERT_EQUAL( 12, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, endpoint_speed) );
  TEST_ASSERT_EQUAL( 14, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, data_toggle_control) );
  TEST_ASSERT_EQUAL( 15, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, head_list_flag) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, max_package_size) );
  TEST_ASSERT_EQUAL( 27, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, non_hs_control_endpoint) );
  TEST_ASSERT_EQUAL( 28, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 1, nak_count_reload) );

  //------------- Word 2 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, interrupt_smask) );
  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, non_hs_interrupt_cmask) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, hub_address) );
  TEST_ASSERT_EQUAL( 23, BITFIELD_OFFSET_OF_UINT32(ehci_qhd_t, 2, hub_port) );
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
  TEST_ASSERT_EQUAL( 4*2, offsetof(ehci_sitd_t, interrupt_smask));
  TEST_ASSERT_EQUAL( 4*2+1, offsetof(ehci_sitd_t, non_hs_interrupt_cmask));

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
// EHCI Register Interface
//--------------------------------------------------------------------+
void test_register_offset(void)
{
  TEST_ASSERT_EQUAL( 0x00, offsetof(ehci_registers_t, usb_cmd));
  TEST_ASSERT_EQUAL( 0x04, offsetof(ehci_registers_t, usb_sts));
  TEST_ASSERT_EQUAL( 0x08, offsetof(ehci_registers_t, usb_int_enable));
  TEST_ASSERT_EQUAL( 0x0C, offsetof(ehci_registers_t, frame_index));
  TEST_ASSERT_EQUAL( 0x10, offsetof(ehci_registers_t, ctrl_ds_seg));
  TEST_ASSERT_EQUAL( 0x14, offsetof(ehci_registers_t, periodic_list_base));
  TEST_ASSERT_EQUAL( 0x18, offsetof(ehci_registers_t, async_list_base));
  TEST_ASSERT_EQUAL( 0x1C, offsetof(ehci_registers_t, tt_control)); // NXP specific
  TEST_ASSERT_EQUAL( 0x40, offsetof(ehci_registers_t, config_flag)); // NXP not used
  TEST_ASSERT_EQUAL( 0x44, offsetof(ehci_registers_t, portsc));
}

void test_register_usbcmd(void)
{
  TEST_ASSERT_EQUAL( 0  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, run_stop) );
  TEST_ASSERT_EQUAL( 1  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, reset) );
  TEST_ASSERT_EQUAL( 2  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, framelist_size) );
  TEST_ASSERT_EQUAL( 4  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, periodic_enable) );
  TEST_ASSERT_EQUAL( 5  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, async_enable) );
  TEST_ASSERT_EQUAL( 6  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, advacne_async) );
  TEST_ASSERT_EQUAL( 7  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, light_reset) );
  TEST_ASSERT_EQUAL( 8  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, async_park) );
  TEST_ASSERT_EQUAL( 11 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, async_park_enable) );
  TEST_ASSERT_EQUAL( 15 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, nxp_framelist_size_msb) );
  TEST_ASSERT_EQUAL( 16 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_cmd_bit, int_threshold) );
}

void test_register_usbsts(void)
{
  TEST_ASSERT_EQUAL( 0  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, usb));
  TEST_ASSERT_EQUAL( 1  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, usb_error));
  TEST_ASSERT_EQUAL( 2  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, port_change_detect));
  TEST_ASSERT_EQUAL( 3  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, framelist_rollover));
  TEST_ASSERT_EQUAL( 4  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, pci_host_system_error));
  TEST_ASSERT_EQUAL( 5  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, async_advance));
  TEST_ASSERT_EQUAL( 7  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, nxp_int_sof));
  TEST_ASSERT_EQUAL( 12 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, hc_halted));
  TEST_ASSERT_EQUAL( 13 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, reclamation));
  TEST_ASSERT_EQUAL( 14 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, period_schedule_status));
  TEST_ASSERT_EQUAL( 15 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, async_schedule_status));
  TEST_ASSERT_EQUAL( 18 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, nxp_int_async));
  TEST_ASSERT_EQUAL( 19 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_sts_bit, nxp_int_period));
}

void test_register_usbint(void)
{
  TEST_ASSERT_EQUAL( 0  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, usb));
  TEST_ASSERT_EQUAL( 1  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, usb_error));
  TEST_ASSERT_EQUAL( 2  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, port_change_detect));
  TEST_ASSERT_EQUAL( 3  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, framelist_rollover));
  TEST_ASSERT_EQUAL( 4  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, pci_host_system_error));
  TEST_ASSERT_EQUAL( 5  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, async_advance));
  TEST_ASSERT_EQUAL( 7  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, nxp_int_sof));
  TEST_ASSERT_EQUAL( 18 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, nxp_int_async));
  TEST_ASSERT_EQUAL( 19 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, usb_int_enable_bit, nxp_int_period));

}

void test_register_portsc(void)
{
  TEST_ASSERT_EQUAL( 0  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, current_connect_status));
  TEST_ASSERT_EQUAL( 1  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, connect_status_change));
  TEST_ASSERT_EQUAL( 2  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, port_enable));
  TEST_ASSERT_EQUAL( 3  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, port_enable_change));
  TEST_ASSERT_EQUAL( 4  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, over_current_active));
  TEST_ASSERT_EQUAL( 5  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, over_current_change));
  TEST_ASSERT_EQUAL( 6  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, force_port_resume));
  TEST_ASSERT_EQUAL( 7  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, suspend));
  TEST_ASSERT_EQUAL( 8  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, port_reset));
  TEST_ASSERT_EQUAL( 9  , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, nxp_highspeed_status));
  TEST_ASSERT_EQUAL( 10 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, line_status));
  TEST_ASSERT_EQUAL( 12 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, port_power));
  TEST_ASSERT_EQUAL( 13 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, port_owner));
  TEST_ASSERT_EQUAL( 14 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, port_indicator_control));
  TEST_ASSERT_EQUAL( 16 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, port_test_control));
  TEST_ASSERT_EQUAL( 20 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, wake_on_connect_enable));
  TEST_ASSERT_EQUAL( 21 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, wake_on_disconnect_enable));
  TEST_ASSERT_EQUAL( 22 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, wake_on_over_current_enable));
  TEST_ASSERT_EQUAL( 23 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, nxp_phy_clock_disable));
  TEST_ASSERT_EQUAL( 24 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, nxp_port_force_fullspeed));
  TEST_ASSERT_EQUAL( 26 , BITFIELD_OFFSET_OF_MEMBER(ehci_registers_t, portsc_bit, nxp_port_speed));
}

//--------------------------------------------------------------------+
// EHCI Data Organization
//--------------------------------------------------------------------+
void test_ehci_data(void)
{
  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t hostid = i+TEST_CONTROLLER_HOST_START_INDEX;
    TEST_ASSERT_BITS_LOW(4096-1, (uint32_t)get_period_frame_list(hostid) );
  }

  // TODO more tests on ehci_data
}
