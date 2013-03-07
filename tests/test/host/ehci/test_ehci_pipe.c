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

ehci_qhd_t *async_head;
ehci_qhd_t *period_head;

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  memclr_(&lpc_usb0, sizeof(LPC_USB0_Type));
  memclr_(&lpc_usb1, sizeof(LPC_USB1_Type));

  memclr_(usbh_device_info_pool, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));
  memclr_(&ehci_data, sizeof(ehci_data_t));

  hcd_init();

  dev_addr = 1;

  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;
  for (uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX; i++)
  {
    usbh_device_info_pool[i].core_id  = hostid;
    usbh_device_info_pool[i].hub_addr = hub_addr;
    usbh_device_info_pool[i].hub_port = hub_port;
    usbh_device_info_pool[i].speed    = TUSB_SPEED_HIGH;
  }

  async_head =  get_async_head( hostid );
  period_head = get_period_head( hostid );
}

void tearDown(void)
{
}

void verify_open_qhd(ehci_qhd_t *p_qhd, uint8_t endpoint_addr, uint16_t max_packet_size)
{
  TEST_ASSERT_EQUAL(dev_addr, p_qhd->device_address);
  TEST_ASSERT_FALSE(p_qhd->inactive_next_xact);
  TEST_ASSERT_EQUAL(endpoint_addr & 0x0F, p_qhd->endpoint_number);
  TEST_ASSERT_EQUAL(usbh_device_info_pool[dev_addr].speed, p_qhd->endpoint_speed);
  TEST_ASSERT_EQUAL(max_packet_size, p_qhd->max_package_size);
  TEST_ASSERT_EQUAL(0, p_qhd->nak_count_reload); // TDD NAK Reload disable

  TEST_ASSERT_EQUAL(hub_addr, p_qhd->hub_address);
  TEST_ASSERT_EQUAL(hub_port, p_qhd->hub_port);
  TEST_ASSERT_EQUAL(1, p_qhd->mult); // TDD operation model for mult

  TEST_ASSERT_FALSE(p_qhd->qtd_overlay.halted);
  TEST_ASSERT(p_qhd->qtd_overlay.next.terminate);
  TEST_ASSERT(p_qhd->qtd_overlay.alternate.terminate);

  //------------- HCD -------------//
  TEST_ASSERT(p_qhd->used);
  TEST_ASSERT_NULL(p_qhd->p_qtd_list_head);
}

//--------------------------------------------------------------------+
// CONTROL PIPE
//--------------------------------------------------------------------+
void verify_control_open_qhd(ehci_qhd_t *p_qhd)
{
  verify_open_qhd(p_qhd, 0, control_max_packet_size);

  TEST_ASSERT_EQUAL(1, p_qhd->data_toggle_control);
  TEST_ASSERT_EQUAL(0, p_qhd->interrupt_smask);
  TEST_ASSERT_EQUAL(0, p_qhd->non_hs_interrupt_cmask);
}

void test_control_open_addr0_qhd_data(void)
{
  dev_addr = 0;

  ehci_qhd_t * const p_qhd = async_head;

  //------------- Code Under Test -------------//
  hcd_pipe_control_open(dev_addr, control_max_packet_size);

  verify_control_open_qhd(p_qhd);
  TEST_ASSERT(p_qhd->head_list_flag);
}

void test_control_open_qhd_data(void)
{
  ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].control.qhd;

  //------------- Code Under TEST -------------//
  hcd_pipe_control_open(dev_addr, control_max_packet_size);

  verify_control_open_qhd(p_qhd);
  TEST_ASSERT_FALSE(p_qhd->head_list_flag);

  //------------- async list check -------------//
  TEST_ASSERT_EQUAL_HEX((uint32_t) p_qhd, align32(async_head->next.address));
  TEST_ASSERT_FALSE(async_head->next.terminate);
  TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, async_head->next.type);
}

void test_control_open_highspeed(void)
{
  ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].control.qhd;

  usbh_device_info_pool[dev_addr].speed   = TUSB_SPEED_HIGH;

  //------------- Code Under TEST -------------//
  hcd_pipe_control_open(dev_addr, control_max_packet_size);

  TEST_ASSERT_FALSE(p_qhd->non_hs_control_endpoint);
}

void test_control_open_non_highspeed(void)
{
  ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].control.qhd;

  usbh_device_info_pool[dev_addr].speed   = TUSB_SPEED_FULL;

  //------------- Code Under TEST -------------//
  hcd_pipe_control_open(dev_addr, control_max_packet_size);

  TEST_ASSERT_TRUE(p_qhd->non_hs_control_endpoint);
}

//--------------------------------------------------------------------+
// BULK PIPE
//--------------------------------------------------------------------+
tusb_descriptor_endpoint_t const desc_ept_bulk_in =
{
    .bLength          = sizeof(tusb_descriptor_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x81,
    .bmAttributes     = { .xfer = TUSB_XFER_BULK },
    .wMaxPacketSize   = 512,
    .bInterval        = 0
};

void verify_bulk_open_qhd(ehci_qhd_t *p_qhd, tusb_descriptor_endpoint_t const * desc_endpoint)
{
  verify_open_qhd(p_qhd, desc_endpoint->bEndpointAddress, desc_endpoint->wMaxPacketSize);

  TEST_ASSERT_FALSE(p_qhd->head_list_flag);
  TEST_ASSERT_EQUAL(0, p_qhd->data_toggle_control);
  TEST_ASSERT_EQUAL(0, p_qhd->interrupt_smask);
  TEST_ASSERT_EQUAL(0, p_qhd->non_hs_interrupt_cmask);
  TEST_ASSERT_FALSE(p_qhd->non_hs_control_endpoint);

  //  TEST_ASSERT_EQUAL(desc_endpoint->bInterval); TDD highspeed bulk/control OUT

  TEST_ASSERT_EQUAL(desc_endpoint->bEndpointAddress & 0x80 ? EHCI_PID_IN : EHCI_PID_OUT, p_qhd->pid_non_control);

  //------------- async list check -------------//
  TEST_ASSERT_EQUAL_HEX((uint32_t) p_qhd, align32(async_head->next.address));
  TEST_ASSERT_FALSE(async_head->next.terminate);
  TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, async_head->next.type);
}

void test_open_bulk_qhd_data(void)
{
  ehci_qhd_t *p_qhd;
  pipe_handle_t pipe_hdl;
  tusb_descriptor_endpoint_t const * desc_endpoint = &desc_ept_bulk_in;

  //------------- Code Under TEST -------------//
  pipe_hdl = hcd_pipe_open(dev_addr, desc_endpoint);

  TEST_ASSERT_EQUAL(dev_addr, pipe_hdl.dev_addr);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, pipe_hdl.xfer_type);

  p_qhd = &ehci_data.device[ pipe_hdl.dev_addr ].qhd[ pipe_hdl.index ];
  verify_bulk_open_qhd(p_qhd, desc_endpoint);

  //------------- async list check -------------//
  TEST_ASSERT_EQUAL_HEX((uint32_t) p_qhd, align32(async_head->next.address));
  TEST_ASSERT_FALSE(async_head->next.terminate);
  TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, async_head->next.type);
}

//--------------------------------------------------------------------+
// INTERRUPT PIPE
//--------------------------------------------------------------------+
tusb_descriptor_endpoint_t const desc_ept_interrupt_out =
{
    .bLength          = sizeof(tusb_descriptor_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x02,
    .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
    .wMaxPacketSize   = 16,
    .bInterval        = 1
};
void verify_int_qhd(ehci_qhd_t *p_qhd, tusb_descriptor_endpoint_t const * desc_endpoint)
{
  verify_open_qhd(p_qhd, desc_endpoint->bEndpointAddress, desc_endpoint->wMaxPacketSize);

  TEST_ASSERT_FALSE(p_qhd->head_list_flag);
  TEST_ASSERT_EQUAL(0, p_qhd->data_toggle_control);
  TEST_ASSERT_FALSE(p_qhd->non_hs_control_endpoint);

  //  TEST_ASSERT_EQUAL(desc_endpoint->bInterval); TDD highspeed bulk/control OUT

  TEST_ASSERT_EQUAL(desc_endpoint->bEndpointAddress & 0x80 ? EHCI_PID_IN : EHCI_PID_OUT, p_qhd->pid_non_control);

  //------------- period list check -------------//
  TEST_ASSERT_EQUAL_HEX((uint32_t) p_qhd, align32(period_head->next.address));
  TEST_ASSERT_FALSE(period_head->next.terminate);
  TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, period_head->next.type);
}

void test_open_interrupt_qhd_hs(void)
{
  ehci_qhd_t *p_qhd;
  pipe_handle_t pipe_hdl;

  //------------- Code Under TEST -------------//
  pipe_hdl = hcd_pipe_open(dev_addr, &desc_ept_interrupt_out);

  TEST_ASSERT_EQUAL(dev_addr, pipe_hdl.dev_addr);
  TEST_ASSERT_EQUAL(TUSB_XFER_INTERRUPT, pipe_hdl.xfer_type);

  p_qhd = &ehci_data.device[ pipe_hdl.dev_addr ].qhd[ pipe_hdl.index ];

  verify_int_qhd(p_qhd, &desc_ept_interrupt_out);

  TEST_ASSERT_EQUAL(0xFF, p_qhd->interrupt_smask);
  //TEST_ASSERT_EQUAL(0, p_qhd->non_hs_interrupt_cmask); cmask in high speed is ignored
}

void test_open_interrupt_qhd_non_hs(void)
{
  ehci_qhd_t *p_qhd;
  pipe_handle_t pipe_hdl;

  usbh_device_info_pool[dev_addr].speed = TUSB_SPEED_FULL;

  //------------- Code Under TEST -------------//
  pipe_hdl = hcd_pipe_open(dev_addr, &desc_ept_interrupt_out);

  TEST_ASSERT_EQUAL(dev_addr, pipe_hdl.dev_addr);
  TEST_ASSERT_EQUAL(TUSB_XFER_INTERRUPT, pipe_hdl.xfer_type);

  p_qhd = &ehci_data.device[ pipe_hdl.dev_addr ].qhd[ pipe_hdl.index ];

  verify_int_qhd(p_qhd, &desc_ept_interrupt_out);

  TEST_ASSERT_EQUAL(1, p_qhd->interrupt_smask);
  TEST_ASSERT_EQUAL(0x1c, p_qhd->non_hs_interrupt_cmask);

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
  pipe_handle_t pipe_hdl = hcd_pipe_open(dev_addr, &desc_ept_iso_in);
  TEST_ASSERT_EQUAL(0, pipe_hdl.dev_addr);
}
