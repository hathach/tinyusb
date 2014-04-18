/**************************************************************************/
/*!
    @file     test_usbh_hcd_integration.c
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
#include "mock_hid_host.h"
#include "mock_msc_host.h"
#include "mock_cdc_host.h"

#include "hcd.h"
#include "usbh_hcd.h"
#include "hub.h"
#include "usbh.h"
#include "ehci.h"
#include "ehci_controller_fake.h"
#include "host_helper.h"

static uint8_t const control_max_packet_size = 64;
static uint8_t hub_addr;
static uint8_t hub_port;
static uint8_t dev_addr;
static uint8_t hostid;

static ehci_registers_t * regs;
static ehci_qhd_t *async_head;
static ehci_qhd_t *period_head_arr;

extern osal_queue_handle_t enum_queue_hdl;

void setUp(void)
{
  hub_addr = hub_port = 0;
  dev_addr = 1;
  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;

  ehci_controller_init();

  helper_usbh_init_expect();
  helper_class_init_expect();
  usbh_init();

  helper_usbh_device_emulate(dev_addr, hub_addr, hub_port, hostid, TUSB_SPEED_HIGH);

  regs            = get_operational_register(hostid);
  async_head      = get_async_head( hostid );
  period_head_arr = (ehci_qhd_t*) get_period_head( hostid, 1 );
  regs->usb_sts   = 0; // hcd_init clear usb_sts by writing 1s
}

void tearDown(void)
{
}

void test_addr0_control_close(void)
{
  dev_addr = 0;
  helper_usbh_device_emulate(0, hub_addr, hub_port, hostid, TUSB_SPEED_HIGH);

  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );

  TEST_ASSERT_STATUS( hcd_pipe_control_xfer(dev_addr,
                        &(tusb_control_request_t) {
                              .bmRequestType_bit = { .direction = TUSB_DIR_HOST_TO_DEV, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
                              .bRequest = TUSB_REQUEST_SET_ADDRESS,
                              .wValue   = 3 },
                        NULL) ) ;

  ehci_qhd_t *p_qhd = async_head;
  TEST_ASSERT_STATUS( hcd_pipe_control_close(dev_addr) );

  //------------- Code Under Test -------------//
  regs->usb_sts_bit.port_change_detect = 0; // clear port change detect
  regs->usb_sts_bit.async_advance      = 1;
  hcd_isr(hostid); // async advance

  TEST_ASSERT( p_qhd->qtd_overlay.halted );
  TEST_ASSERT_FALSE( p_qhd->is_removing );
  TEST_ASSERT_NULL( p_qhd->p_qtd_list_head );
  TEST_ASSERT_NULL( p_qhd->p_qtd_list_tail );
}

#if 0 // TODO TEST enable this
void test_isr_disconnect_then_async_advance_control_pipe(void)
{
  TEST_ASSERT_STATUS( hcd_pipe_control_open(dev_addr, control_max_packet_size) );

  TEST_ASSERT_STATUS( hcd_pipe_control_xfer(dev_addr,
                        &(tusb_control_request_t) {
                              .bmRequestType_bit = { .direction = TUSB_DIR_HOST_TO_DEV, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
                              .bRequest = TUSB_REQUEST_SET_ADDRESS,
                              .wValue   = 3 },
                        NULL) );

  ehci_qhd_t *p_qhd = get_control_qhd(dev_addr);
  ehci_qtd_t *p_qtd_head = p_qhd->p_qtd_list_head;
  ehci_qtd_t *p_qtd_tail = p_qhd->p_qtd_list_tail;

  usbh_enumerate_t root_enum_entry = { .core_id  = hostid, .hub_addr = 0, .hub_port = 0 };
  osal_queue_send_ExpectWithArrayAndReturn(enum_queue_hdl, (uint8_t*)&root_enum_entry, sizeof(usbh_enumerate_t), TUSB_ERROR_NONE);

  ehci_controller_device_unplug(hostid);

  //------------- Code Under Test -------------//
  usbh_enumeration_task(NULL); // carry out unplug task
  regs->usb_sts_bit.port_change_detect = 0; // clear port change detect
  regs->usb_sts_bit.async_advance = 1;
  hcd_isr(hostid); // async advance

  TEST_ASSERT_FALSE(p_qhd->used);
  TEST_ASSERT_FALSE(p_qhd->is_removing);
//  TEST_ASSERT_NULL(p_qhd->p_qtd_list_head);
//  TEST_ASSERT_NULL(p_qhd->p_qtd_list_tail);
}
#endif

void test_bulk_pipe_close(void)
{
  tusb_descriptor_endpoint_t const desc_ept_bulk_in =
  {
      .bLength          = sizeof(tusb_descriptor_endpoint_t),
      .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
      .bEndpointAddress = 0x81,
      .bmAttributes     = { .xfer = TUSB_XFER_BULK },
      .wMaxPacketSize   = 512,
      .bInterval        = 0
  };

  uint8_t xfer_data[100];
  pipe_handle_t pipe_hdl = hcd_pipe_open(dev_addr, &desc_ept_bulk_in, TUSB_CLASS_MSC);

  TEST_ASSERT_STATUS( hcd_pipe_xfer(pipe_hdl, xfer_data, sizeof(xfer_data), 100) );
  TEST_ASSERT_STATUS( hcd_pipe_xfer(pipe_hdl, xfer_data, sizeof(xfer_data), 50) );

  ehci_qhd_t *p_qhd = &ehci_data.device[dev_addr-1].qhd[pipe_hdl.index];
  ehci_qtd_t *p_qtd_head = p_qhd->p_qtd_list_head;
  ehci_qtd_t *p_qtd_tail = p_qhd->p_qtd_list_tail;

  TEST_ASSERT_STATUS( hcd_pipe_close(pipe_hdl) );

  //------------- Code Under Test -------------//
  regs->usb_sts_bit.async_advance = 1;
  get_control_qhd(dev_addr)->is_removing = 1; // mimic unmount
  hcd_isr(hostid); // async advance

  TEST_ASSERT_FALSE(p_qhd->used);
  TEST_ASSERT_FALSE(p_qhd->is_removing);
//  TEST_ASSERT_NULL(p_qhd->p_qtd_list_head);
//  TEST_ASSERT_NULL(p_qhd->p_qtd_list_tail);
  TEST_ASSERT_FALSE(p_qtd_head->used);
  TEST_ASSERT_FALSE(p_qtd_tail->used);
}

#if 0 // TODO TEST enable this
void test_device_unplugged_status(void)
{
  usbh_enumerate_t root_enum_entry = { .core_id  = hostid, .hub_addr = 0, .hub_port = 0 };
  osal_queue_send_ExpectWithArrayAndReturn(enum_queue_hdl, (uint8_t*)&root_enum_entry, sizeof(usbh_enumerate_t), TUSB_ERROR_NONE);

  ehci_controller_device_unplug(hostid);
//  hcd_isr(hostid);
  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_REMOVING, usbh_devices[dev_addr].state);

  regs->usb_sts_bit.async_advance = 1;
  hcd_isr(hostid); // async advance

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_UNPLUG, usbh_devices[dev_addr].state);
}
#endif
