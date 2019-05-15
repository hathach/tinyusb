/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
#include "tusb_option.h"
#include "tusb_errors.h"
#include "binary.h"
#include "type_helper.h"

#include "hal.h"
#include "ehci.h"

#include "ehci_controller_fake.h"
#include "mock_osal.h"
#include "mock_usbh_hcd.h"

usbh_device_t _usbh_devices[CFG_TUSB_HOST_DEVICE_MAX+1];

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
  hcd_event_device_attach_Expect(hostid);

  //------------- Code Under Test -------------//
  ehci_controller_device_plug(hostid, TUSB_SPEED_HIGH);
}

void test_isr_device_connect_fullspeed(void)
{
  hcd_event_device_attach_Expect(hostid);

  //------------- Code Under Test -------------//
  ehci_controller_device_plug(hostid, TUSB_SPEED_FULL);
}

void test_isr_device_connect_slowspeed(void)
{
  hcd_event_device_attach_Expect(hostid);

  //------------- Code Under Test -------------//
  ehci_controller_device_plug(hostid, TUSB_SPEED_LOW);
}

void test_isr_device_disconnect(void)
{
  hcd_event_device_remove_Expect(hostid);

  //------------- Code Under Test -------------//
  ehci_controller_device_unplug(hostid);


//  TEST_ASSERT(regs->usb_cmd_bit.advacne_async);
}
