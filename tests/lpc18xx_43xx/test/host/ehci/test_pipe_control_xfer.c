/**************************************************************************/
/*!
    @file     test_ehci_pipe_xfer.c
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
static uint8_t xfer_data [100];

static ehci_qhd_t *async_head;
static ehci_qhd_t *p_control_qhd;

static ehci_qtd_t *p_setup;
static ehci_qtd_t *p_data;
static ehci_qtd_t *p_status;

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  ehci_controller_init();

  memclr_(usbh_devices, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));
  memclr_(xfer_data, sizeof(xfer_data));

  TEST_ASSERT_STATUS( hcd_init() );

  dev_addr = 1;
  hostid   = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;

  helper_usbh_device_emulate(0        , hub_addr, hub_port, hostid, TUSB_SPEED_HIGH);
  helper_usbh_device_emulate(dev_addr , hub_addr, hub_port, hostid, TUSB_SPEED_HIGH);

  async_head =  get_async_head( hostid );

  //------------- pipe open -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );

  p_control_qhd = &ehci_data.device[dev_addr-1].control.qhd;

  p_setup  = &ehci_data.device[dev_addr-1].control.qtd[0];
  p_data   = &ehci_data.device[dev_addr-1].control.qtd[1];
  p_status = &ehci_data.device[dev_addr-1].control.qtd[2];
}

void tearDown(void)
{
}
//--------------------------------------------------------------------+
// CONTROL TRANSFER
//--------------------------------------------------------------------+
tusb_control_request_t request_get_dev_desc =
{
    .bmRequestType_bit = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
    .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
    .wValue   = (TUSB_DESC_TYPE_DEVICE << 8),
    .wLength  = 18
};

tusb_control_request_t request_set_dev_addr =
{
    .bmRequestType_bit = { .direction = TUSB_DIR_HOST_TO_DEV, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
    .bRequest = TUSB_REQUEST_SET_ADDRESS,
    .wValue   = 3
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

//--------------------------------------------------------------------+
// Address 0
//--------------------------------------------------------------------+
void test_control_addr0_xfer_get_check_qhd_qtd_mapping(void)
{
  dev_addr = 0;
  ehci_qhd_t * const p_qhd = async_head;

  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );

  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_xfer(dev_addr, &request_get_dev_desc, xfer_data) );

  p_setup  = &ehci_data.addr0_qtd[0];
  p_data   = &ehci_data.addr0_qtd[1];
  p_status = &ehci_data.addr0_qtd[2];

  TEST_ASSERT_EQUAL(0 , p_qhd->total_xferred_bytes);
  TEST_ASSERT_EQUAL_HEX( p_setup, p_qhd->qtd_overlay.next.address );
  TEST_ASSERT_EQUAL_HEX( p_setup  , p_qhd->p_qtd_list_head);
  TEST_ASSERT_EQUAL_HEX( p_data   , p_setup->next.address);
  TEST_ASSERT_EQUAL_HEX( p_status , p_data->next.address );
  TEST_ASSERT_TRUE( p_status->next.terminate );

  verify_qtd(p_setup, (uint8_t*) &request_get_dev_desc, 8);
}

//--------------------------------------------------------------------+
// Normal Control
//--------------------------------------------------------------------+
void test_control_xfer_get(void)
{
  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_xfer(dev_addr, &request_get_dev_desc, xfer_data) );

  TEST_ASSERT_EQUAL_HEX( p_setup, p_control_qhd->qtd_overlay.next.address );
  TEST_ASSERT_EQUAL_HEX( p_setup  , p_control_qhd->p_qtd_list_head);
  TEST_ASSERT_EQUAL_HEX( p_data   , p_setup->next.address);
  TEST_ASSERT_EQUAL_HEX( p_status , p_data->next.address );
  TEST_ASSERT_TRUE( p_status->next.terminate );

  //------------- SETUP -------------//
  verify_qtd(p_setup, (uint8_t*) &request_get_dev_desc, 8);

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
  TEST_ASSERT_TRUE(p_status->next.terminate);

  TEST_ASSERT_EQUAL_HEX(p_setup, p_control_qhd->p_qtd_list_head);
  TEST_ASSERT_EQUAL_HEX(p_status, p_control_qhd->p_qtd_list_tail);
}

void test_control_xfer_set(void)
{
  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( hcd_pipe_control_xfer(dev_addr, &request_set_dev_addr, xfer_data) );

  TEST_ASSERT_EQUAL_HEX( p_setup, p_control_qhd->qtd_overlay.next.address );
  TEST_ASSERT_EQUAL_HEX( p_setup  , p_control_qhd->p_qtd_list_head);
  TEST_ASSERT_EQUAL_HEX( p_status , p_setup->next.address );
  TEST_ASSERT_TRUE( p_status->next.terminate );

  //------------- STATUS -------------//
  verify_qtd(p_status, NULL, 0);
  TEST_ASSERT_TRUE(p_status->int_on_complete);
  TEST_ASSERT_TRUE(p_status->data_toggle);
  TEST_ASSERT_EQUAL(EHCI_PID_IN, p_status->pid);
  TEST_ASSERT_TRUE(p_status->next.terminate);

  TEST_ASSERT_EQUAL_HEX(p_setup, p_control_qhd->p_qtd_list_head);
  TEST_ASSERT_EQUAL_HEX(p_status, p_control_qhd->p_qtd_list_tail);
}

void test_control_xfer_complete_isr(void)
{
  TEST_ASSERT_STATUS( hcd_pipe_control_xfer(dev_addr, &request_get_dev_desc, xfer_data) );

  usbh_xfer_isr_Expect(((pipe_handle_t){.dev_addr = dev_addr}), 0, TUSB_EVENT_XFER_COMPLETE, 18);

  //------------- Code Under TEST -------------//
  ehci_controller_run(hostid);

  TEST_ASSERT_EQUAL(0, p_control_qhd->total_xferred_bytes);
  TEST_ASSERT_NULL(p_control_qhd->p_qtd_list_head);
  TEST_ASSERT_NULL(p_control_qhd->p_qtd_list_tail);

  TEST_ASSERT_FALSE(p_setup->used);
  TEST_ASSERT_FALSE(p_data->used);
  TEST_ASSERT_FALSE(p_status->used);

}

void test_control_xfer_error_isr(void)
{
  TEST_ASSERT_STATUS( hcd_pipe_control_xfer(dev_addr, &request_get_dev_desc, xfer_data) );

  usbh_xfer_isr_Expect(((pipe_handle_t){.dev_addr = dev_addr}), 0, TUSB_EVENT_XFER_ERROR, 0);

  //------------- Code Under TEST -------------//
  ehci_controller_run_error(hostid);

  TEST_ASSERT_EQUAL(0, p_control_qhd->total_xferred_bytes);

  TEST_ASSERT_NULL( p_control_qhd->p_qtd_list_head );
  TEST_ASSERT_NULL( p_control_qhd->p_qtd_list_tail );

  TEST_ASSERT_TRUE( p_control_qhd->qtd_overlay.next.terminate);
  TEST_ASSERT_TRUE( p_control_qhd->qtd_overlay.alternate.terminate);
  TEST_ASSERT_FALSE( p_control_qhd->qtd_overlay.halted);
}

void test_control_xfer_error_stall(void)
{
  TEST_ASSERT_STATUS( hcd_pipe_control_xfer(dev_addr, &request_get_dev_desc, xfer_data) );

  usbh_xfer_isr_Expect(((pipe_handle_t){.dev_addr = dev_addr}), 0, TUSB_EVENT_XFER_STALLED, 0);

  //------------- Code Under TEST -------------//
  ehci_controller_run_stall(hostid);

  TEST_ASSERT_EQUAL(0, p_control_qhd->total_xferred_bytes);

  TEST_ASSERT_NULL( p_control_qhd->p_qtd_list_head );
  TEST_ASSERT_NULL( p_control_qhd->p_qtd_list_tail );

  TEST_ASSERT_TRUE( p_control_qhd->qtd_overlay.next.terminate);
  TEST_ASSERT_TRUE( p_control_qhd->qtd_overlay.alternate.terminate);
  TEST_ASSERT_FALSE( p_control_qhd->qtd_overlay.halted);

  TEST_ASSERT_FALSE( p_setup->used );
  TEST_ASSERT_FALSE( p_data->used );
  TEST_ASSERT_FALSE( p_status->used );
}

