/*
 * test_host_hid_keyboard.c
 *
 *  Created on: Jan 18, 2013
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
#include "hid_host.h"
#include "mock_osal.h"
#include "mock_usbd_host.h"

tusb_device_info_t usbh_device_pool [2];

tusb_keyboard_report_t sample_key[2] =
{
    {
        .modifier = TUSB_KEYBOARD_MODIFIER_LEFTCTRL,
        .keycode = {TUSB_KEYBOARD_KEYCODE_a}
    },
    {
        .modifier = TUSB_KEYBOARD_MODIFIER_RIGHTALT,
        .keycode = {TUSB_KEYBOARD_KEYCODE_z}
    }
};

tusb_handle_configure_t config_hdl;
tusb_keyboard_report_t report;
tusb_configure_info_t *p_cfg_info;

void setUp(void)
{
  config_hdl = 1; // deviceID = 0 ; Configure = 1
  memset(&report, 0, sizeof(tusb_keyboard_report_t));
  p_cfg_info = NULL;

  usbh_device_pool[0].configuration[0].classes.hid_keyboard.pipe_in = 1;
  usbh_device_pool[0].configuration[0].classes.hid_keyboard.qid = 1;

  usbh_device_pool[0].configuration[1].classes.hid_keyboard.pipe_in = 0;
  usbh_device_pool[0].configuration[1].classes.hid_keyboard.qid = 0;
}

void tearDown(void)
{
}

tusb_error_t queue_get_stub(osal_queue_id_t qid, uint32_t *data, osal_timeout_t msec, int num_call)
{
  memcpy(data, (uint32_t*)(&sample_key[num_call/2]) + (num_call%2), 4);
  return TUSB_ERROR_NONE;
}

tusb_error_t get_configure_class_not_support_stub(tusb_handle_configure_t configure_hdl, tusb_configure_info_t **pp_configure_info, int num_call)
{
  (*pp_configure_info) = &(usbh_device_pool[0].configuration[1]);
  return TUSB_ERROR_NONE;
}

tusb_error_t get_configure_stub(tusb_handle_configure_t configure_hdl, tusb_configure_info_t **pp_configure_info, int num_call)
{
  (*pp_configure_info) = &(usbh_device_pool[0].configuration[0]);
  return TUSB_ERROR_NONE;
}

void test_keyboard_get_invalid_para()
{
  usbh_configure_info_get_IgnoreAndReturn(TUSB_ERROR_INVALID_PARA);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_keyboard_get(config_hdl, NULL));
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_keyboard_get(config_hdl, &report));
}

void test_keyboard_get_class_not_supported()
{
  usbh_configure_info_get_StubWithCallback(get_configure_class_not_support_stub);

  TEST_ASSERT_EQUAL(TUSB_ERROR_CLASS_DEVICE_DONT_SUPPORT, tusbh_keyboard_get(config_hdl, &report));
}

void test_keyboard_get_from_empty_queue()
{
  usbh_configure_info_get_StubWithCallback(get_configure_stub);
  osal_queue_get_IgnoreAndReturn(TUSB_ERROR_OSAL_TIMEOUT);
//  osal_queue_get_ExpectAndReturn( usbh_device_pool[0].configuration[0].classes.hid_keyboard.qid, );

  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_TIMEOUT, tusbh_keyboard_get(config_hdl, &report));
}

void test_keyboard_get_ok()
{
  usbh_configure_info_get_StubWithCallback(get_configure_stub);
  osal_queue_get_StubWithCallback(queue_get_stub);

  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, tusbh_keyboard_get(config_hdl, &report));
  TEST_ASSERT_EQUAL_MEMORY(&sample_key[0], &report, sizeof(tusb_keyboard_report_t));

  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, tusbh_keyboard_get(config_hdl, &report));
  TEST_ASSERT_EQUAL_MEMORY(&sample_key[1], &report, sizeof(tusb_keyboard_report_t));
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


