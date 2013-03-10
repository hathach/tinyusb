/*
 * test_ehci_init.c
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

#include "hal.h"
#include "mock_osal.h"
#include "hcd.h"
#include "mock_usbh_hcd.h"
#include "ehci.h"

extern ehci_data_t ehci_data;
usbh_device_info_t usbh_device_info_pool[TUSB_CFG_HOST_DEVICE_MAX+1];

LPC_USB0_Type lpc_usb0;
LPC_USB1_Type lpc_usb1;

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  memclr_(&lpc_usb0, sizeof(LPC_USB0_Type));
  memclr_(&lpc_usb1, sizeof(LPC_USB1_Type));
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Initialization
//--------------------------------------------------------------------+
void test_hcd_init_data(void)
{
  uint32_t random_data = 0x1234;
  memcpy(&ehci_data, &random_data, sizeof(random_data));

  hcd_init();

  //------------- check memory data -------------//
  for(uint32_t i=0; i<sizeof(ehci_data.device); i++)
    TEST_ASSERT_EQUAL_HEX8(0, ((uint8_t*) ehci_data.device)[i] );
}

void test_hcd_init_usbint(void)
{
  hcd_init();

  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    ehci_registers_t* const regs = get_operational_register(i+TEST_CONTROLLER_HOST_START_INDEX);

    //------------- USB INT Enable-------------//
    TEST_ASSERT(regs->usb_int_enable_bit.usb_error);
    TEST_ASSERT(regs->usb_int_enable_bit.port_change_detect);
    TEST_ASSERT(regs->usb_int_enable_bit.async_advance);

    TEST_ASSERT_FALSE(regs->usb_int_enable_bit.framelist_rollover);
    TEST_ASSERT_FALSE(regs->usb_int_enable_bit.pci_host_system_error);

    TEST_ASSERT_FALSE(regs->usb_int_enable_bit.usb);
    TEST_ASSERT_TRUE(regs->usb_int_enable_bit.nxp_int_async);
    TEST_ASSERT_TRUE(regs->usb_int_enable_bit.nxp_int_period);

    TEST_IGNORE_MESSAGE("not use nxp int async/period, use usbint instead");
  }
}

void test_hcd_init_async_list(void)
{
  hcd_init();

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

void test_hcd_init_period_list(void)
{
#if EHCI_PERIODIC_LIST
  hcd_init();

  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t           const hostid      = i+TEST_CONTROLLER_HOST_START_INDEX;
    ehci_registers_t* const regs        = get_operational_register(hostid);
    ehci_qhd_t *      const period_head = get_period_head(hostid);
    ehci_link_t *     const framelist   = get_period_frame_list(hostid);

    TEST_ASSERT_EQUAL_HEX( (uint32_t) framelist, regs->periodic_list_base);
    for(uint32_t list_idx=0; list_idx < EHCI_FRAMELIST_SIZE; list_idx++)
    {
      TEST_ASSERT_EQUAL_HEX( (uint32_t) period_head, align32((uint32_t)framelist[list_idx].address) );
      TEST_ASSERT_FALSE(framelist[list_idx].terminate);
      TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, framelist[list_idx].type);
    }

    TEST_ASSERT(period_head->interrupt_smask)
    TEST_ASSERT_TRUE(period_head->next.terminate);
    TEST_ASSERT(period_head->qtd_overlay.halted);
  }
#endif
}

void test_hcd_init_tt_control(void)
{
  hcd_init();

  for(uint32_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t           const hostid      = i+TEST_CONTROLLER_HOST_START_INDEX;
    ehci_registers_t* const regs        = get_operational_register(hostid);

    TEST_ASSERT_EQUAL(0, regs->tt_control);
  }
}

void test_hcd_init_usbcmd(void)
{
  hcd_init();

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
