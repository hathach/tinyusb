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
#include "usbd_host.h"
#include "mock_osal.h"

extern usbh_device_info_t device_info_pool[TUSB_CFG_HOST_DEVICE_MAX];
tusb_handle_device_t dev_hdl;
void setUp(void)
{
  dev_hdl = 0;
  device_info_pool[dev_hdl].status = TUSB_DEVICE_STATUS_READY;
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// init, get_status
//--------------------------------------------------------------------+
void test_usbh_init_checkmem(void)
{
  usbh_device_info_t device_info_zero[TUSB_CFG_HOST_DEVICE_MAX];
  memset(device_info_zero, 0, sizeof(usbh_device_info_t)*TUSB_CFG_HOST_DEVICE_MAX);

  osal_queue_create_IgnoreAndReturn(TUSB_ERROR_NONE);

  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, usbh_init());
  TEST_ASSERT_EQUAL_MEMORY(device_info_zero, device_info_pool, sizeof(usbh_device_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

void test_usbh_init_queue_create_fail(void)
{
  osal_queue_create_IgnoreAndReturn(TUSB_ERROR_OSAL_QUEUE_FAILED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_QUEUE_FAILED, usbh_init());
}

void test_usbh_status_get_fail(void)
{
  usbh_init();
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
void test_enum_task(void)
{
//  osal_queue_
  TEST_IGNORE();
  usbh_enumerate_task();
}


#if 0
void test_keyboard_open_invalid_para()
{
  tusb_handle_keyboard_t keyboard_handle;

  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_keyboard_open(TUSB_CFG_HOST_DEVICE_MAX, 1, &keyboard_handle) );
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_keyboard_open(0, 0, &keyboard_handle) );
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_keyboard_open(0, TUSB_CFG_CONFIGURATION_MAX+1, &keyboard_handle) );
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_keyboard_open(0, 1, NULL) );
}

void test_keyboard_open_succeed()
{
  tusb_handle_keyboard_t keyboard_handle = 0;

  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, tusbh_keyboard_open(0, 1, &keyboard_handle));
  TEST_ASSERT_TRUE( 0 != keyboard_handle);
}

void test_keyboard_callback__()
{
  TEST_IGNORE();
  tusb_handle_device_t device_handle = __LINE__;
  tusb_handle_configure_t configure_handle = __LINE__;
  tusb_handle_interface_t interface_handle = __LINE__;
  uint32_t configure_flags = BIT_(TUSB_CLASS_HID);
  tusbh_usbd_device_mounted_cb_ExpectWithArray(TUSB_ERROR_NONE, device_handle, &configure_flags, 1, 1);
}
#endif
