/**************************************************************************/
/*!
    @file     test_ehci_pipe_bulk_xfer.c
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

static uint8_t hub_addr = 2;
static uint8_t hub_port = 2;
static uint8_t dev_addr;
static uint8_t hostid;
static uint8_t xfer_data [18000]; // 18K to test buffer pointer list
static uint8_t data2[100];

static ehci_qhd_t *async_head;
static ehci_qhd_t *p_qhd_bulk;
static pipe_handle_t pipe_hdl_bulk;

tusb_descriptor_endpoint_t const desc_ept_bulk_in =
{
    .bLength          = sizeof(tusb_descriptor_endpoint_t),
    .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
    .bEndpointAddress = 0x81,
    .bmAttributes     = { .xfer = TUSB_XFER_BULK },
    .wMaxPacketSize   = 512,
    .bInterval        = 0
};

tusb_descriptor_endpoint_t const desc_ept_bulk_out =
{
    .bLength          = sizeof(tusb_descriptor_endpoint_t),
    .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
    .bEndpointAddress = 0x01,
    .bmAttributes     = { .xfer = TUSB_XFER_BULK },
    .wMaxPacketSize   = 512,
    .bInterval        = 0
};

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  ehci_controller_init();
  memclr_(xfer_data, sizeof(xfer_data));
  memclr_(usbh_devices, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  TEST_ASSERT_STATUS( hcd_init() );

  dev_addr = 1;
  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;
  helper_usbh_device_emulate(dev_addr, hub_addr, hub_port, hostid, TUSB_SPEED_HIGH);

  async_head =  get_async_head( hostid );

  //------------- pipe open -------------//
  pipe_hdl_bulk = hcd_pipe_open(dev_addr, &desc_ept_bulk_in, TUSB_CLASS_MSC);

  TEST_ASSERT_EQUAL(dev_addr, pipe_hdl_bulk.dev_addr);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, pipe_hdl_bulk.xfer_type);

  p_qhd_bulk = &ehci_data.device[ dev_addr -1].qhd[ pipe_hdl_bulk.index ];
}

void tearDown(void)
{
}
//--------------------------------------------------------------------+
// BULK TRANSFER
//--------------------------------------------------------------------+
void verify_qtd(ehci_qtd_t *p_qtd, uint8_t p_data[], uint16_t length)
{
  TEST_ASSERT_TRUE(p_qtd->alternate.terminate); // not used, always invalid

  //------------- status -------------//
  TEST_ASSERT_FALSE(p_qtd->pingstate_err);
  TEST_ASSERT_FALSE(p_qtd->non_hs_split_state);
  TEST_ASSERT_FALSE(p_qtd->non_hs_period_missed_uframe);
  TEST_ASSERT_FALSE(p_qtd->xact_err);
  TEST_ASSERT_FALSE(p_qtd->babble_err);
  TEST_ASSERT_FALSE(p_qtd->buffer_err);
  TEST_ASSERT_FALSE(p_qtd->halted);
  TEST_ASSERT_TRUE(p_qtd->active);

  TEST_ASSERT_FALSE(p_qtd->data_toggle);
  TEST_ASSERT_EQUAL(3, p_qtd->cerr);
  TEST_ASSERT_EQUAL(0, p_qtd->current_page);
  TEST_ASSERT_EQUAL(length, p_qtd->total_bytes);
  TEST_ASSERT_EQUAL(length, p_qtd->expected_bytes);
  TEST_ASSERT_TRUE(p_qtd->used);

  TEST_ASSERT_EQUAL_HEX( p_data, p_qtd->buffer[0] );
  for(uint8_t i=1; i<5; i++)
  {
    TEST_ASSERT_EQUAL_HEX( align4k((uint32_t) (p_data+4096*i)), align4k(p_qtd->buffer[i]) );
  }
}

void test_bulk_xfer_hs_ping_out(void)
{
  usbh_devices[dev_addr].speed    = TUSB_SPEED_HIGH;

  pipe_handle_t pipe_hdl = hcd_pipe_open(dev_addr, &desc_ept_bulk_out, TUSB_CLASS_MSC);
  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle(pipe_hdl);

  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( hcd_pipe_xfer(pipe_hdl, xfer_data, sizeof(xfer_data), true) );

  ehci_qtd_t* p_qtd = p_qhd->p_qtd_list_head;
}

void test_bulk_xfer(void)
{
  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( hcd_pipe_xfer(pipe_hdl_bulk, xfer_data, sizeof(xfer_data), true) );

  ehci_qtd_t* p_qtd = p_qhd_bulk->p_qtd_list_head;
  TEST_ASSERT_NOT_NULL(p_qtd);

  verify_qtd( p_qtd, xfer_data, sizeof(xfer_data));
  TEST_ASSERT_EQUAL_HEX(p_qhd_bulk->qtd_overlay.next.address, p_qtd);
  TEST_ASSERT_TRUE(p_qtd->next.terminate);
  TEST_ASSERT_EQUAL(EHCI_PID_IN, p_qtd->pid);
  TEST_ASSERT_TRUE(p_qtd->int_on_complete);
}

void test_bulk_xfer_double(void)
{
  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( hcd_pipe_xfer(pipe_hdl_bulk, xfer_data, sizeof(xfer_data), false) );

  TEST_ASSERT_STATUS( hcd_pipe_xfer(pipe_hdl_bulk, data2, sizeof(data2), true) );

  ehci_qtd_t* p_head = p_qhd_bulk->p_qtd_list_head;
  ehci_qtd_t* p_tail = p_qhd_bulk->p_qtd_list_tail;

  //------------- list head -------------//
  TEST_ASSERT_NOT_NULL(p_head);
  verify_qtd(p_head, xfer_data, sizeof(xfer_data));
  TEST_ASSERT_EQUAL_HEX(p_qhd_bulk->qtd_overlay.next.address, p_head);
  TEST_ASSERT_EQUAL(EHCI_PID_IN, p_head->pid);
  TEST_ASSERT_FALSE(p_head->next.terminate);
  TEST_ASSERT_FALSE(p_head->int_on_complete);

  //------------- list tail -------------//
  TEST_ASSERT_NOT_NULL(p_tail);
  verify_qtd(p_tail, data2, sizeof(data2));
  TEST_ASSERT_EQUAL_HEX( align32(p_head->next.address), p_tail);
  TEST_ASSERT_EQUAL(EHCI_PID_IN, p_tail->pid);
  TEST_ASSERT_TRUE(p_tail->next.terminate);
  TEST_ASSERT_TRUE(p_tail->int_on_complete);
}

void test_bulk_xfer_complete_isr(void)
{
  TEST_ASSERT_STATUS( hcd_pipe_xfer(pipe_hdl_bulk, xfer_data, sizeof(xfer_data), false) );

  TEST_ASSERT_STATUS( hcd_pipe_xfer(pipe_hdl_bulk, data2, sizeof(data2), true) );

  ehci_qtd_t* p_head = p_qhd_bulk->p_qtd_list_head;
  ehci_qtd_t* p_tail = p_qhd_bulk->p_qtd_list_tail;

  usbh_xfer_isr_Expect(pipe_hdl_bulk, TUSB_CLASS_MSC, TUSB_EVENT_XFER_COMPLETE, sizeof(data2)+sizeof(xfer_data));

  //------------- Code Under Test -------------//
  ehci_controller_run(hostid);

  TEST_ASSERT_EQUAL(0, p_qhd_bulk->total_xferred_bytes);
  TEST_ASSERT_TRUE(p_qhd_bulk->qtd_overlay.next.terminate);
  TEST_ASSERT_FALSE(p_head->used);
  TEST_ASSERT_FALSE(p_tail->used);
  TEST_ASSERT_NULL(p_qhd_bulk->p_qtd_list_head);
  TEST_ASSERT_NULL(p_qhd_bulk->p_qtd_list_tail);
}
