/*
 * test_usbd_host.c
 *
 *  Created on: Jan 21, 2013
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
#include "errors.h"
#include "usbh.h"
#include "mock_osal.h"
#include "mock_hcd.h"
#include "mock_usbh_hcd.h"

extern usbh_device_info_t device_info_pool[TUSB_CFG_HOST_DEVICE_MAX];
tusb_handle_device_t dev_hdl;
void setUp(void)
{
  dev_hdl = 0;
  memset(device_info_pool, 0, TUSB_CFG_HOST_DEVICE_MAX*sizeof(usbh_device_info_t));
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// init, get_status
//--------------------------------------------------------------------+
void test_usbh_init_hcd_failed(void)
{
  hcd_init_IgnoreAndReturn(TUSB_ERROR_HCD_FAILED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_HCD_FAILED, usbh_init());
}

void test_usbh_init_task_create_failed(void)
{
  for(uint32_t i=0; i<TUSB_CFG_HOST_CONTROLLER_NUM; i++)
    hcd_init_ExpectAndReturn(i, TUSB_ERROR_NONE);

  osal_task_create_IgnoreAndReturn(TUSB_ERROR_OSAL_TASK_FAILED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_TASK_FAILED, usbh_init());
}

void test_usbh_init_queue_create_failed(void)
{
  for(uint32_t i=0; i<TUSB_CFG_HOST_CONTROLLER_NUM; i++)
    hcd_init_ExpectAndReturn(i, TUSB_ERROR_NONE);

  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn(NULL);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_QUEUE_FAILED, usbh_init());
}


void test_usbh_init_ok(void)
{
  uint32_t dummy;

  usbh_device_info_t device_info_zero[TUSB_CFG_HOST_DEVICE_MAX];
  memset(device_info_zero, 0, sizeof(usbh_device_info_t)*TUSB_CFG_HOST_DEVICE_MAX);

  for(uint32_t i=0; i<TUSB_CFG_HOST_CONTROLLER_NUM; i++)
    hcd_init_ExpectAndReturn(i, TUSB_ERROR_NONE);

  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn((osal_queue_handle_t)(&dummy));

  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, usbh_init());

  TEST_ASSERT_EQUAL_MEMORY(device_info_zero, device_info_pool, sizeof(usbh_device_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

void test_usbh_status_get_fail(void)
{
  device_info_pool[dev_hdl].status = 0;

  TEST_ASSERT_EQUAL( 0, tusbh_device_status_get(TUSB_CFG_HOST_DEVICE_MAX) );
  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATUS_UNPLUG, tusbh_device_status_get(dev_hdl) );
}

void test_usbh_status_get_succeed(void)
{
  device_info_pool[dev_hdl].status = TUSB_DEVICE_STATUS_READY;
  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATUS_READY, tusbh_device_status_get(dev_hdl) );
}

//--------------------------------------------------------------------+
// enum task
//--------------------------------------------------------------------+
extern osal_queue_handle_t enum_queue_hdl;
usbh_enumerate_t enum_connect =
{
    .core_id = 0,
    .hub_addr = 0,
    .hub_port = 0,
    .connect_status = 1
};

void queue_recv_stub (osal_queue_handle_t const queue_hdl, uint32_t *p_data, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  TEST_ASSERT_EQUAL_PTR(enum_queue_hdl, queue_hdl);
  (*p_data) = ( *((uint32_t*) &enum_connect) );
  (*p_error) = TUSB_ERROR_NONE;
}

tusb_error_t pipe_control_stub(pipe_handle_t pipe_hdl, const tusb_std_request_t * const p_request, uint8_t data[], int num_call)
{
  tusb_descriptor_device_t dev_desc =
  {
      .bLength            = sizeof(tusb_descriptor_device_t),
      .bDescriptorType    = TUSB_DESC_DEVICE,
      .bcdUSB             = 0x0200,
      .bDeviceClass       = 0x00,
      .bDeviceSubClass    = 0x00,
      .bDeviceProtocol    = 0x00,

      .bMaxPacketSize0    = 64,

      .idVendor           = 0x1FC9,
      .idProduct          = 0x4000,
      .bcdDevice          = 0x0100,

      .iManufacturer      = 0x01,
      .iProduct           = 0x02,
      .iSerialNumber      = 0x03,

      .bNumConfigurations = 0x02
  };

  if (p_request->bRequest == TUSB_REQUEST_GET_DESCRIPTOR)
  {
    switch (p_request->wValue >> 8)
    {
      case TUSB_DESC_DEVICE:
        memcpy(data, &dev_desc, p_request->wLength);
      break;

      default:
        TEST_FAIL();
      break;
    }
  }

  return TUSB_ERROR_NONE;
}

void test_enum_task_connect(void)
{
  pipe_handle_t pipe_addr0 = 12;

  osal_queue_receive_StubWithCallback(queue_recv_stub);
  hcd_port_connect_status_ExpectAndReturn(enum_connect.core_id, true);
  hcd_port_speed_ExpectAndReturn(enum_connect.core_id, TUSB_SPEED_FULL);
  hcd_addr0_open_IgnoreAndReturn(pipe_addr0);

  hcd_pipe_control_xfer_StubWithCallback(pipe_control_stub);
//  hcd_pipe_control_open_ExpectAnd(1, );

  usbh_enumeration_task();
}

void test_enum_task_disconnect(void)
{
  TEST_IGNORE();
}

void test_enum_task_connect_via_hub(void)
{
  TEST_IGNORE();
}

void test_enum_task_disconnect_via_hub(void)
{
  TEST_IGNORE();
}
