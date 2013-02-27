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
#include "mock_usbh.h"

extern class_hid_keyboard_info_t keyboard_info_pool[TUSB_CFG_HOST_DEVICE_MAX];

tusb_keyboard_report_t sample_key[2] =
{
    {
        .modifier = KEYBOARD_MODIFIER_LEFTCTRL,
        .keycode = {KEYBOARD_KEYCODE_a}
    },
    {
        .modifier = KEYBOARD_MODIFIER_RIGHTALT,
        .keycode = {KEYBOARD_KEYCODE_z}
    }
};

tusb_handle_device_t device_hdl;
tusb_descriptor_interface_t kbd_descriptor;
tusb_keyboard_report_t report;
uint8_t instance_num;

void setUp(void)
{
  device_hdl = 0; // deviceID = 0 ; Configure = 1
  instance_num = 0;
  memset(&report, 0, sizeof(tusb_keyboard_report_t));

  keyboard_info_pool[0].instance_count = 0;
  keyboard_info_pool[0].instance[0].pipe_in = 1;
  keyboard_info_pool[0].instance[0].report_size = sizeof(tusb_keyboard_report_t);

  kbd_descriptor = ((tusb_descriptor_interface_t)
      {
        .bLength            = sizeof(tusb_descriptor_interface_t),
        .bDescriptorType    = TUSB_DESC_INTERFACE,
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = TUSB_CLASS_HID,
        .bInterfaceSubClass = HID_SUBCLASS_BOOT,
        .bInterfaceProtocol = HID_PROTOCOL_KEYBOARD,
        .iInterface         = 0
      });

}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// keyboard_install, keyboard_no_instances, keybaord_init
//--------------------------------------------------------------------+
void test_keyboard_no_instances_invalid_para(void)
{
  tusbh_device_status_get_IgnoreAndReturn(0);
  TEST_ASSERT_EQUAL(0, tusbh_hid_keyboard_no_instances(TUSB_CFG_HOST_DEVICE_MAX));
}

void test_keyboard_install_ok(void)
{
  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  TEST_ASSERT_EQUAL(0, tusbh_hid_keyboard_no_instances(device_hdl));

  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, hidh_keyboard_install(device_hdl, (uint8_t*) &kbd_descriptor));
  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  TEST_ASSERT_EQUAL(1, tusbh_hid_keyboard_no_instances(device_hdl));
}

void test_keyboard_init(void)
{
  class_hid_keyboard_info_t keyboard_info_zero[TUSB_CFG_HOST_DEVICE_MAX];
  memset(keyboard_info_zero, 0, sizeof(class_hid_keyboard_info_t)*TUSB_CFG_HOST_DEVICE_MAX);

  hidh_keyboard_init();

  TEST_ASSERT_EQUAL_MEMORY(keyboard_info_zero, keyboard_info_pool, sizeof(class_hid_keyboard_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

//--------------------------------------------------------------------+
// keyboard_get
//--------------------------------------------------------------------+
pipe_status_t pipe_status_get_stub(pipe_handle_t pipe_hdl, int num_call)
{
  switch (num_call)
  {
    case 0:
      memcpy(keyboard_info_pool[0].instance[0].buffer, &sample_key[0], sizeof(tusb_keyboard_report_t));
      return PIPE_STATUS_COMPLETE;
    break;

    case 1:
      return PIPE_STATUS_AVAILABLE;
    break;

    case 2:
      return PIPE_STATUS_BUSY;
    break;

    case 3:
      memcpy(keyboard_info_pool[0].instance[0].buffer, &sample_key[1], sizeof(tusb_keyboard_report_t));
      return PIPE_STATUS_COMPLETE;
    break;

    default:
      return PIPE_STATUS_AVAILABLE;
  }
}

void test_keyboard_get_invalid_para()
{
  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get(0, 0, NULL));

  tusbh_device_status_get_IgnoreAndReturn(0);
  TEST_ASSERT_EQUAL(TUSB_ERROR_DEVICE_NOT_READY, tusbh_hid_keyboard_get(TUSB_CFG_HOST_DEVICE_MAX, 0, &report));

  tusbh_device_status_get_IgnoreAndReturn(0);
  TEST_ASSERT_EQUAL(TUSB_ERROR_DEVICE_NOT_READY, tusbh_hid_keyboard_get(0, TUSB_CFG_HOST_HID_KEYBOARD_NO_INSTANCES_PER_DEVICE, &report));
}

void test_keyboard_get_class_not_supported()
{
  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  keyboard_info_pool[device_hdl].instance[0].pipe_in = 0;
  TEST_ASSERT_EQUAL(TUSB_ERROR_CLASS_DEVICE_DONT_SUPPORT, tusbh_hid_keyboard_get(device_hdl, instance_num, &report));
}

void test_keyboard_get_report_not_available()
{
  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  usbh_pipe_status_get_IgnoreAndReturn(PIPE_STATUS_BUSY);
  TEST_ASSERT_EQUAL(TUSB_ERROR_CLASS_DATA_NOT_AVAILABLE, tusbh_hid_keyboard_get(device_hdl, instance_num, &report));

  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  usbh_pipe_status_get_IgnoreAndReturn(PIPE_STATUS_AVAILABLE);
  TEST_ASSERT_EQUAL(TUSB_ERROR_CLASS_DATA_NOT_AVAILABLE, tusbh_hid_keyboard_get(device_hdl, instance_num, &report));
}

void test_keyboard_get_ok()
{
  usbh_pipe_status_get_StubWithCallback(pipe_status_get_stub);

  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, tusbh_hid_keyboard_get(device_hdl, instance_num, &report));
  TEST_ASSERT_EQUAL_MEMORY(&sample_key[0], &report, sizeof(tusb_keyboard_report_t));

  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  TEST_ASSERT_EQUAL(TUSB_ERROR_CLASS_DATA_NOT_AVAILABLE, tusbh_hid_keyboard_get(device_hdl, instance_num, &report));
  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  TEST_ASSERT_EQUAL(TUSB_ERROR_CLASS_DATA_NOT_AVAILABLE, tusbh_hid_keyboard_get(device_hdl, instance_num, &report));

  tusbh_device_status_get_IgnoreAndReturn(TUSB_DEVICE_STATUS_READY);
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, tusbh_hid_keyboard_get(device_hdl, instance_num, &report));
  TEST_ASSERT_EQUAL_MEMORY(&sample_key[1], &report, sizeof(tusb_keyboard_report_t));
}

