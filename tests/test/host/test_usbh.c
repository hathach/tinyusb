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
#include "mock_tusb_callback.h"
#include "mock_hid_host.h"

extern usbh_device_info_t usbh_device_info_pool[TUSB_CFG_HOST_DEVICE_MAX+1];
tusb_handle_device_t dev_hdl;
void setUp(void)
{
  dev_hdl = 0;
  memset(usbh_device_info_pool, 0, (TUSB_CFG_HOST_DEVICE_MAX+1)*sizeof(usbh_device_info_t));
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// get_status
//--------------------------------------------------------------------+
void test_usbh_status_get_fail(void)
{
  usbh_device_info_pool[dev_hdl].status = 0;

  TEST_ASSERT_EQUAL( 0, tusbh_device_status_get(TUSB_CFG_HOST_DEVICE_MAX+1) );
  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATUS_UNPLUG, tusbh_device_status_get(dev_hdl) );
}

void test_usbh_status_get_succeed(void)
{
  usbh_device_info_pool[dev_hdl].status = TUSB_DEVICE_STATUS_READY;
  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATUS_READY, tusbh_device_status_get(dev_hdl) );
}

//--------------------------------------------------------------------+
// Init
//--------------------------------------------------------------------+
void hcd_init_expect(void)
{
  for(uint32_t i=0; i<TUSB_CFG_HOST_CONTROLLER_NUM; i++)
    hcd_init_ExpectAndReturn(TUSB_CFG_HOST_CONTROLLER_START_INDEX+i, TUSB_ERROR_NONE);
}

void test_usbh_init_hcd_failed(void)
{
  hcd_init_IgnoreAndReturn(TUSB_ERROR_HCD_FAILED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_HCD_FAILED, usbh_init());
}

void test_usbh_init_enum_task_create_failed(void)
{
  hcd_init_expect();
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_OSAL_TASK_FAILED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_TASK_FAILED, usbh_init());
}

void test_usbh_init_enum_queue_create_failed(void)
{
  hcd_init_expect();
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn(NULL);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_QUEUE_FAILED, usbh_init());
}

void test_usbh_init_reporter_taks_create_failed(void)
{
  TEST_IGNORE();
}

void test_usbh_init_reporter_queue_create_failed(void)
{
  TEST_IGNORE();
}

void class_init_expect(void)
{
  hidh_init_Expect();

  //TODO update more classes
}

void test_usbh_init_ok(void)
{
  osal_queue_handle_t dummy;

  usbh_device_info_t device_info_zero[TUSB_CFG_HOST_DEVICE_MAX+1];
  memclr_(device_info_zero, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  hcd_init_expect();
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn(dummy);

  class_init_expect();

  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, usbh_init());

  TEST_ASSERT_EQUAL_MEMORY(device_info_zero, usbh_device_info_pool, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

}
