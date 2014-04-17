/**************************************************************************/
/*!
    @file     test_ehci_pipe.c
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
#include "mock_osal.h"
#include "hcd.h"
#include "mock_usbh_hcd.h"
#include "ehci.h"
#include "ehci_controller_fake.h"
#include "host_helper.h"

usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1];

static uint8_t const control_max_packet_size = 64;
static uint8_t const hub_addr = 2;
static uint8_t const hub_port = 2;
static uint8_t dev_addr;
static uint8_t hostid;

static ehci_qhd_t *async_head;
static ehci_qhd_t *p_control_qhd;

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  memclr_(usbh_devices, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  TEST_ASSERT_STATUS( hcd_init() );

  dev_addr = 1;
  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;

  helper_usbh_device_emulate(0, hub_addr, hub_port, hostid, TUSB_SPEED_HIGH);
  helper_usbh_device_emulate(dev_addr, hub_addr, hub_port, hostid, TUSB_SPEED_HIGH);

  async_head =  get_async_head( hostid );
  p_control_qhd = &ehci_data.device[dev_addr-1].control.qhd;
}

void tearDown(void)
{
}

void verify_open_qhd(ehci_qhd_t *p_qhd, uint8_t endpoint_addr, uint16_t max_packet_size)
{
  TEST_ASSERT_EQUAL(dev_addr, p_qhd->device_address);
  TEST_ASSERT_FALSE(p_qhd->non_hs_period_inactive_next_xact);
  TEST_ASSERT_EQUAL(endpoint_addr & 0x0F, p_qhd->endpoint_number);
  TEST_ASSERT_EQUAL(usbh_devices[dev_addr].speed, p_qhd->endpoint_speed);
  TEST_ASSERT_EQUAL(max_packet_size, p_qhd->max_package_size);
  TEST_ASSERT_EQUAL(0, p_qhd->nak_count_reload); // TDD NAK Reload disable

  TEST_ASSERT_EQUAL(hub_addr, p_qhd->hub_address);
  TEST_ASSERT_EQUAL(hub_port, p_qhd->hub_port);
  TEST_ASSERT_EQUAL(1, p_qhd->mult); // TDD operation model for mult

  TEST_ASSERT_FALSE(p_qhd->qtd_overlay.halted);
  TEST_ASSERT(p_qhd->qtd_overlay.next.terminate);
  TEST_ASSERT(p_qhd->qtd_overlay.alternate.terminate);

  //------------- HCD -------------//
  TEST_ASSERT(p_qhd->used);
  TEST_ASSERT_FALSE(p_qhd->is_removing);
  TEST_ASSERT_NULL(p_qhd->p_qtd_list_head);
  TEST_ASSERT_NULL(p_qhd->p_qtd_list_tail);
}

void verify_control_open_qhd(ehci_qhd_t *p_qhd)
{
  verify_open_qhd(p_qhd, 0, control_max_packet_size);

  TEST_ASSERT_EQUAL(0, p_qhd->class_code);
  TEST_ASSERT_EQUAL(1, p_qhd->data_toggle_control);
  TEST_ASSERT_EQUAL(0, p_qhd->interrupt_smask);
  TEST_ASSERT_EQUAL(0, p_qhd->non_hs_interrupt_cmask);
}

//--------------------------------------------------------------------+
// PIPE OPEN
//--------------------------------------------------------------------+
void test_control_open_addr0_qhd_data(void)
{
  dev_addr = 0;

  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );

  verify_control_open_qhd(async_head);
  TEST_ASSERT(async_head->head_list_flag);
}

void test_control_open_qhd_data(void)
{
  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size));

  verify_control_open_qhd(p_control_qhd);
  TEST_ASSERT_FALSE(p_control_qhd->head_list_flag);

  //------------- async list check -------------//
  TEST_ASSERT_EQUAL_HEX((uint32_t) p_control_qhd, align32(async_head->next.address));
  TEST_ASSERT_FALSE(async_head->next.terminate);
  TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, async_head->next.type);
}

void test_control_open_highspeed(void)
{
  usbh_devices[dev_addr].speed   = TUSB_SPEED_HIGH;

  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );
  TEST_ASSERT_FALSE(p_control_qhd->non_hs_control_endpoint);
}

void test_control_open_non_highspeed(void)
{
  usbh_devices[dev_addr].speed   = TUSB_SPEED_FULL;

  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );

  TEST_ASSERT_TRUE(p_control_qhd->non_hs_control_endpoint);
}

//--------------------------------------------------------------------+
// PIPE CLOSE
//--------------------------------------------------------------------+
void test_control_addr0_close(void)
{
  dev_addr = 0;
  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );

  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_close(dev_addr) );

  TEST_ASSERT(async_head->head_list_flag);
  TEST_ASSERT(async_head->is_removing);
}

void test_control_close(void)
{
  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );

  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_close(dev_addr) );

  TEST_ASSERT(p_control_qhd->is_removing);
  TEST_ASSERT(p_control_qhd->used);

  TEST_ASSERT( align32(get_async_head(hostid)->next.address) != (uint32_t) p_control_qhd );
  TEST_ASSERT_EQUAL( get_async_head(hostid), align32(p_control_qhd->next.address));
}
