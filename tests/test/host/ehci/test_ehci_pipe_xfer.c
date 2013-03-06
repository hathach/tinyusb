/*
 * test_ehci_pipe_xfer.c
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
#include "usbh_hcd.h"
#include "ehci.h"
#include "test_ehci.h"

extern ehci_data_t ehci_data;
usbh_device_info_t usbh_device_info_pool[TUSB_CFG_HOST_DEVICE_MAX+1];

LPC_USB0_Type lpc_usb0;
LPC_USB1_Type lpc_usb1;

uint8_t const control_max_packet_size = 64;
uint8_t const hub_addr = 2;
uint8_t const hub_port = 2;
uint8_t dev_addr;
uint8_t hostid;
uint8_t xfer_data [100];

ehci_qhd_t *async_head;

ehci_qtd_t *p_setup;
ehci_qtd_t *p_data;
ehci_qtd_t *p_status;

tusb_descriptor_endpoint_t const desc_ept_bulk_in =
{
    .bLength          = sizeof(tusb_descriptor_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x81,
    .bmAttributes     = { .xfer = TUSB_XFER_BULK },
    .wMaxPacketSize   = 512,
    .bInterval        = 0
};

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  memclr_(&lpc_usb0, sizeof(LPC_USB0_Type));
  memclr_(&lpc_usb1, sizeof(LPC_USB1_Type));

  memclr_(usbh_device_info_pool, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));
  memclr_(&ehci_data, sizeof(ehci_data_t));
  memclr_(xfer_data, sizeof(xfer_data));

  hcd_init();

  dev_addr = 1;

  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;
  for (uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX; i++)
  {
    usbh_device_info_pool[i].core_id  = hostid;
    usbh_device_info_pool[i].hub_addr = hub_addr;
    usbh_device_info_pool[i].hub_port = hub_port;
  }

  async_head =  get_async_head( hostid );

  p_setup  = &ehci_data.device[dev_addr].control.qtd[0];
  p_data   = &ehci_data.device[dev_addr].control.qtd[1];
  p_status = &ehci_data.device[dev_addr].control.qtd[2];
}

void tearDown(void)
{
}
//--------------------------------------------------------------------+
// CONTROL TRANSFER
//--------------------------------------------------------------------+
tusb_std_request_t request_get_dev_desc =
{
    .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
    .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
    .wValue   = (TUSB_DESC_DEVICE << 8),
    .wLength  = 18
};

void verify_qtd(ehci_qtd_t *p_qtd, uint8_t p_data[], uint16_t length)
{
  TEST_ASSERT_TRUE(p_qtd->alternate.terminate); // not used, always invalid

  TEST_ASSERT_FALSE(p_qtd->pingstate_err);
  TEST_ASSERT_FALSE(p_qtd->non_hs_split_state);
  TEST_ASSERT_FALSE(p_qtd->non_hs_period_missed_uframe);
  TEST_ASSERT_FALSE(p_qtd->xact_err);
  TEST_ASSERT_FALSE(p_qtd->babble_err);
  TEST_ASSERT_FALSE(p_qtd->buffer_err);
  TEST_ASSERT_FALSE(p_qtd->halted);
  TEST_ASSERT_TRUE(p_qtd->active);

  TEST_ASSERT_EQUAL(3, p_qtd->cerr);
  TEST_ASSERT_EQUAL(0, p_qtd->current_page);
  TEST_ASSERT_EQUAL(length, p_qtd->total_bytes);

  TEST_ASSERT_EQUAL_HEX(p_data, p_qtd->buffer[0]);
}

void test_control_addr0_xfer_get_check_qhd_qtd_mapping(void)
{
  dev_addr = 0;
  ehci_qhd_t * const p_qhd = async_head;

  hcd_pipe_control_open(dev_addr, control_max_packet_size);

  //------------- Code Under TEST -------------//
  hcd_pipe_control_xfer(dev_addr, &request_get_dev_desc, xfer_data);

  p_setup  = &ehci_data.addr0.qtd[0];
  p_data   = &ehci_data.addr0.qtd[1];
  p_status = &ehci_data.addr0.qtd[2];

  TEST_ASSERT_EQUAL_HEX( p_setup, p_qhd->qtd_overlay.next.address );
  TEST_ASSERT_EQUAL_HEX( p_setup  , p_qhd->p_qtd_list);
  TEST_ASSERT_EQUAL_HEX( p_data   , p_setup->next.address);
  TEST_ASSERT_EQUAL_HEX( p_status , p_data->next.address );
  TEST_ASSERT_TRUE( p_status->next.terminate );

  verify_qtd(p_setup, &ehci_data.addr0.request, 8);
}


void test_control_xfer_get(void)
{
  ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].control.qhd;
  hcd_pipe_control_open(dev_addr, control_max_packet_size);

  //------------- Code Under TEST -------------//
  hcd_pipe_control_xfer(dev_addr, &request_get_dev_desc, xfer_data);

  TEST_ASSERT_EQUAL_HEX( p_setup, p_qhd->qtd_overlay.next.address );
  TEST_ASSERT_EQUAL_HEX( p_setup  , p_qhd->p_qtd_list);
  TEST_ASSERT_EQUAL_HEX( p_data   , p_setup->next.address);
  TEST_ASSERT_EQUAL_HEX( p_status , p_data->next.address );
  TEST_ASSERT_TRUE( p_status->next.terminate );

  //------------- SETUP -------------//
  uint8_t* p_request = (uint8_t *) &ehci_data.device[dev_addr].control.request;
  verify_qtd(p_setup, p_request, 8);

  TEST_ASSERT_EQUAL_MEMORY(&request_get_dev_desc, p_request, sizeof(tusb_std_request_t));

  TEST_ASSERT_FALSE(p_setup->int_on_complete);
  TEST_ASSERT_FALSE(p_setup->data_toggle);
  TEST_ASSERT_EQUAL(EHCI_PID_SETUP, p_setup->pid);

  //------------- DATA -------------//
  verify_qtd(p_data, xfer_data, request_get_dev_desc.wLength);
  TEST_ASSERT_FALSE(p_data->int_on_complete);
  TEST_ASSERT_TRUE(p_data->data_toggle);
  TEST_ASSERT_EQUAL(EHCI_PID_IN, p_data->pid);

  //------------- STATUS -------------//
  verify_qtd(p_status, NULL, 0);
  TEST_ASSERT_TRUE(p_status->int_on_complete);
  TEST_ASSERT_TRUE(p_status->data_toggle);
  TEST_ASSERT_EQUAL(EHCI_PID_OUT, p_status->pid);
}

void test_control_xfer_set(void)
{
  tusb_std_request_t request_set_dev_addr =
  {
      .bmRequestType = { .direction = TUSB_DIR_HOST_TO_DEV, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
      .bRequest = TUSB_REQUEST_SET_ADDRESS,
      .wValue   = 3
  };

  ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].control.qhd;
  hcd_pipe_control_open(dev_addr, control_max_packet_size);

  //------------- Code Under TEST -------------//
  hcd_pipe_control_xfer(dev_addr, &request_set_dev_addr, xfer_data);

  TEST_ASSERT_EQUAL_HEX( p_setup, p_qhd->qtd_overlay.next.address );
  TEST_ASSERT_EQUAL_HEX( p_setup  , p_qhd->p_qtd_list);
  TEST_ASSERT_EQUAL_HEX( p_status , p_setup->next.address );
  TEST_ASSERT_TRUE( p_status->next.terminate );

  //------------- STATUS -------------//
  verify_qtd(p_status, NULL, 0);
  TEST_ASSERT_TRUE(p_status->int_on_complete);
  TEST_ASSERT_TRUE(p_status->data_toggle);
  TEST_ASSERT_EQUAL(EHCI_PID_IN, p_status->pid);
}


