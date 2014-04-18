/**************************************************************************/
/*!
    @file     test_ehci_isr.c
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
#include "ehci.h"

#include "ehci_controller_fake.h"
#include "mock_osal.h"
#include "mock_usbh_hcd.h"

usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1];

static uint8_t hostid;
static ehci_registers_t * regs;

void setUp(void)
{
  ehci_controller_init();
  TEST_ASSERT_STATUS( hcd_init());

  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;
  regs = get_operational_register(hostid);
}

void tearDown(void)
{
}

void test_isr_device_connect_highspeed(void)
{
  usbh_hcd_rhport_plugged_isr_Expect(hostid);

  //------------- Code Under Test -------------//
  ehci_controller_device_plug(hostid, TUSB_SPEED_HIGH);
}

void test_isr_device_connect_fullspeed(void)
{
  usbh_hcd_rhport_plugged_isr_Expect(hostid);

  //------------- Code Under Test -------------//
  ehci_controller_device_plug(hostid, TUSB_SPEED_FULL);
}

void test_isr_device_connect_slowspeed(void)
{
  usbh_hcd_rhport_plugged_isr_Expect(hostid);

  //------------- Code Under Test -------------//
  ehci_controller_device_plug(hostid, TUSB_SPEED_LOW);
}

void test_isr_device_disconnect(void)
{
  usbh_hcd_rhport_unplugged_isr_Expect(hostid);

  //------------- Code Under Test -------------//
  ehci_controller_device_unplug(hostid);


//  TEST_ASSERT(regs->usb_cmd_bit.advacne_async);
}
