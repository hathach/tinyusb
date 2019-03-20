/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

#include <stdlib.h>
#include "unity.h"
#include "type_helper.h"
#include "tusb_option.h"
#include "tusb_errors.h"
#include "binary.h"

#include "hal.h"
#include "hcd.h"
#include "ehci.h"

#include "ehci_controller_fake.h"
#include "mock_osal.h"
#include "mock_usbh_hcd.h"

usbh_device_t _usbh_devices[CFG_TUSB_HOST_DEVICE_MAX+1];

static uint8_t hostid;

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  ehci_controller_init();
  TEST_ASSERT_STATUS( hcd_init() );

  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Initialization
//--------------------------------------------------------------------+
void test_hcd_init_data(void)
{
  //------------- check memory data -------------//
  TEST_ASSERT_MEM_ZERO(&ehci_data.device, sizeof(ehci_data.device));
  TEST_ASSERT_MEM_ZERO(ehci_data.addr0_qtd, sizeof(ehci_qtd_t)*3);
}

void test_hcd_init_usbint(void)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  //------------- USB INT Enable-------------//
  TEST_ASSERT(regs->usb_int_enable_bit.usb_error);
  TEST_ASSERT(regs->usb_int_enable_bit.port_change_detect);
  TEST_ASSERT(regs->usb_int_enable_bit.async_advance);

  TEST_ASSERT_FALSE(regs->usb_int_enable_bit.framelist_rollover);
  TEST_ASSERT_FALSE(regs->usb_int_enable_bit.pci_host_system_error);

  TEST_ASSERT_FALSE(regs->usb_int_enable_bit.nxp_int_sof);
  TEST_ASSERT_FALSE(regs->usb_int_enable_bit.usb);
  TEST_ASSERT_TRUE(regs->usb_int_enable_bit.nxp_int_async);
  TEST_ASSERT_TRUE(regs->usb_int_enable_bit.nxp_int_period);

  // TODO to be portable use usbint instead of nxp int async/period
}

void test_hcd_init_async_list(void)
{
  ehci_registers_t * const regs = get_operational_register(hostid);
  ehci_qhd_t * const async_head = get_async_head(hostid);

  TEST_ASSERT_EQUAL_HEX(async_head, regs->async_list_base);

  TEST_ASSERT_EQUAL_HEX(async_head, tu_align32( (uint32_t) async_head) );
  TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, async_head->next.type);
  TEST_ASSERT_FALSE(async_head->next.terminate);

  TEST_ASSERT(async_head->head_list_flag);
  TEST_ASSERT(async_head->qtd_overlay.halted);
}

void check_qhd_endpoint_link(ehci_link_t *p_prev, ehci_qhd_t *p_qhd)
{
  //------------- period list check -------------//
  TEST_ASSERT_EQUAL_HEX((uint32_t) p_qhd, tu_align32(p_prev->address));
  TEST_ASSERT_FALSE(p_prev->terminate);
  TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, p_prev->type);
}

void test_hcd_init_period_list(void)
{
  ehci_registers_t* const regs            = get_operational_register(hostid);
  ehci_qhd_t *      const period_head_arr = get_period_head(hostid, 1);
  ehci_link_t *     const framelist       = get_period_frame_list(hostid);

  TEST_ASSERT_EQUAL_HEX( (uint32_t) framelist, regs->periodic_list_base);

  check_qhd_endpoint_link( framelist+0,  period_head_arr+1);
  check_qhd_endpoint_link( framelist+2,  period_head_arr+1);
  check_qhd_endpoint_link( framelist+4,  period_head_arr+1);
  check_qhd_endpoint_link( framelist+6,  period_head_arr+1);

  check_qhd_endpoint_link( framelist+1,  period_head_arr+2);
  check_qhd_endpoint_link( framelist+5,  period_head_arr+2);

  check_qhd_endpoint_link( framelist+3,  period_head_arr+3);
  check_qhd_endpoint_link( framelist+7,  period_head_arr);
  check_qhd_endpoint_link( (ehci_link_t*) (period_head_arr+1),  period_head_arr);
  check_qhd_endpoint_link( (ehci_link_t*) (period_head_arr+2),  period_head_arr);
  check_qhd_endpoint_link( (ehci_link_t*) (period_head_arr+3),  period_head_arr);

  for(uint32_t i=0; i<4; i++)
  {
    TEST_ASSERT(period_head_arr[i].interrupt_smask);
    TEST_ASSERT(period_head_arr[i].qtd_overlay.halted);
  }

  TEST_ASSERT_TRUE(period_head_arr[0].next.terminate);
}

void test_hcd_init_tt_control(void)
{
  ehci_registers_t* const regs        = get_operational_register(hostid);
}

void test_hcd_init_usbcmd(void)
{
  ehci_registers_t* const regs        = get_operational_register(hostid);

  TEST_ASSERT(regs->usb_cmd_bit.async_enable);
  TEST_ASSERT(regs->usb_cmd_bit.periodic_enable);

  //------------- Framelist size (NXP specific) -------------//
  TEST_ASSERT_BITS(TU_BIN8(11), EHCI_CFG_FRAMELIST_SIZE_BITS, regs->usb_cmd_bit.framelist_size);
  TEST_ASSERT_EQUAL(EHCI_CFG_FRAMELIST_SIZE_BITS >> 2, regs->usb_cmd_bit.nxp_framelist_size_msb);
}

void test_hcd_init_portsc(void)
{
  TEST_IGNORE_MESSAGE("more advance stuff need manipulate this register");
}
