/*
 * test_ehci_init.c
 *
 *  Created on: Feb 27, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.org)
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
#include "type_helper.h"
#include "tusb_option.h"
#include "errors.h"
#include "binary.h"

#include "hal.h"
#include "mock_osal.h"
#include "hcd.h"
#include "mock_usbh_hcd.h"
#include "ehci.h"
#include "ehci_controller.h"

usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1];

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  ehci_controller_init();
  hcd_init();
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
  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    ehci_registers_t* const regs = get_operational_register(i+TEST_CONTROLLER_HOST_START_INDEX);

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
}

void test_hcd_init_async_list(void)
{
  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t hostid                = i+TEST_CONTROLLER_HOST_START_INDEX;

    ehci_registers_t * const regs = get_operational_register(hostid);
    ehci_qhd_t * const async_head = get_async_head(hostid);

    TEST_ASSERT_EQUAL_HEX(async_head, regs->async_list_base);

    TEST_ASSERT_EQUAL_HEX(async_head, align32(async_head) );
    TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, async_head->next.type);
    TEST_ASSERT_FALSE(async_head->next.terminate);

    TEST_ASSERT(async_head->head_list_flag);
    TEST_ASSERT(async_head->qtd_overlay.halted);
  }
}

void check_qhd_endpoint_link(ehci_link_t *p_prev, ehci_qhd_t *p_qhd)
{
  //------------- period list check -------------//
  TEST_ASSERT_EQUAL_HEX((uint32_t) p_qhd, align32(p_prev->address));
  TEST_ASSERT_FALSE(p_prev->terminate);
  TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, p_prev->type);
}

void test_hcd_init_period_list(void)
{
#if EHCI_PERIODIC_LIST
  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t           const hostid          = i+TEST_CONTROLLER_HOST_START_INDEX;
    ehci_registers_t* const regs            = get_operational_register(hostid);
    ehci_qhd_t *      const period_head_arr = get_period_head(hostid, 1);
    ehci_link_t *     const framelist       = get_period_frame_list(hostid);

    TEST_ASSERT_EQUAL_HEX( (uint32_t) framelist, regs->periodic_list_base);

    check_qhd_endpoint_link( framelist+1,  period_head_arr+1);
    check_qhd_endpoint_link( framelist+3,  period_head_arr+1);
    check_qhd_endpoint_link( framelist+5,  period_head_arr+1);
    check_qhd_endpoint_link( framelist+7,  period_head_arr+1);

    check_qhd_endpoint_link( framelist+2,  period_head_arr+2);
    check_qhd_endpoint_link( framelist+6,  period_head_arr+2);

    check_qhd_endpoint_link( framelist,  period_head_arr);
    check_qhd_endpoint_link( framelist+4,  period_head_arr);
    check_qhd_endpoint_link( (ehci_link_t*) (period_head_arr+1),  period_head_arr);
    check_qhd_endpoint_link( (ehci_link_t*) (period_head_arr+2),  period_head_arr);

    for(uint32_t i=0; i<3; i++)
    {
      TEST_ASSERT(period_head_arr[i].interrupt_smask);
      TEST_ASSERT(period_head_arr[i].qtd_overlay.halted);
    }

    TEST_ASSERT_TRUE(period_head_arr[0].next.terminate);
  }
#endif
}

void test_hcd_init_tt_control(void)
{
  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t           const hostid      = i+TEST_CONTROLLER_HOST_START_INDEX;
    ehci_registers_t* const regs        = get_operational_register(hostid);

    TEST_ASSERT_EQUAL(0, regs->tt_control);
  }
}

void test_hcd_init_usbcmd(void)
{
  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t           const hostid      = i+TEST_CONTROLLER_HOST_START_INDEX;
    ehci_registers_t* const regs        = get_operational_register(hostid);

    TEST_ASSERT(regs->usb_cmd_bit.async_enable);

#if EHCI_PERIODIC_LIST
    TEST_ASSERT(regs->usb_cmd_bit.periodic_enable);
#else
    TEST_ASSERT_FALSE(regs->usb_cmd_bit.periodic_enable);
#endif

    //------------- Framelist size (NXP specific) -------------//
    TEST_ASSERT_BITS(BIN8(11), EHCI_CFG_FRAMELIST_SIZE_BITS, regs->usb_cmd_bit.framelist_size);
    TEST_ASSERT_EQUAL(EHCI_CFG_FRAMELIST_SIZE_BITS >> 2, regs->usb_cmd_bit.nxp_framelist_size_msb);
  }
}

void test_hcd_init_portsc(void)
{
  TEST_IGNORE_MESSAGE("more advance stuff need manipulate this register");
}
