/*
 * test_ehci_pipe.c
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
#include "ehci_controller.h"

usbh_device_info_t usbh_device_info_pool[TUSB_CFG_HOST_DEVICE_MAX+1];

uint8_t const hub_addr = 2;
uint8_t const hub_port = 2;
uint8_t dev_addr;
uint8_t hostid;

ehci_qhd_t *period_head;
//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  memclr_(usbh_device_info_pool, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  hcd_init();

  dev_addr = 1;

  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;
  for (uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX+1; i++)
  {
    usbh_device_info_pool[i].core_id  = hostid;
    usbh_device_info_pool[i].hub_addr = hub_addr;
    usbh_device_info_pool[i].hub_port = hub_port;
    usbh_device_info_pool[i].speed    = TUSB_SPEED_HIGH;
  }

  period_head = get_period_head( hostid );
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// TODO ISOCRHONOUS PIPE
//--------------------------------------------------------------------+
tusb_descriptor_endpoint_t const desc_ept_iso_in =
{
    .bLength          = sizeof(tusb_descriptor_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x83,
    .bmAttributes     = { .xfer = TUSB_XFER_ISOCHRONOUS },
    .wMaxPacketSize   = 1024,
    .bInterval        = 1
};

void test_open_isochronous(void)
{
  pipe_handle_t pipe_hdl = hcd_pipe_open(dev_addr, &desc_ept_iso_in, TUSB_CLASS_AUDIO);
  TEST_ASSERT_EQUAL(0, pipe_hdl.dev_addr);
}
