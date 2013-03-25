/*
 * test_host_hid_keyboard.c
 *
 *  Created on: Jan 18, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.org)
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

#include "stdlib.h"
#include "unity.h"
#include "errors.h"
#include "hid_host.h"
#include "hid_host_keyboard.h"
#include "mock_osal.h"
#include "mock_usbh.h"
#include "mock_hcd.h"

extern hidh_keyboard_info_t keyboard_data[TUSB_CFG_HOST_DEVICE_MAX];

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

uint8_t dev_addr;
tusb_keyboard_report_t report;
uint8_t instance_num;
keyboard_interface_t* p_hidh_kbd_interface;

tusb_descriptor_interface_t const kbd_descriptor =
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
};


void setUp(void)
{
  instance_num = 0;
  dev_addr = RANDOM(TUSB_CFG_HOST_DEVICE_MAX)+1;

  memclr_(&report, sizeof(tusb_keyboard_report_t));

  hidh_keyboard_init();

  keyboard_data[dev_addr-1].instance_count = 0;
  p_hidh_kbd_interface = &keyboard_data[dev_addr-1].instance[instance_num];

  p_hidh_kbd_interface->report_size = sizeof(tusb_keyboard_report_t);
  p_hidh_kbd_interface->pipe_in = (pipe_handle_t) {
    .dev_addr = dev_addr,
    .xfer_type = TUSB_XFER_INTERRUPT,
    .index = 1
  };

}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// keyboard_install, keyboard_no_instances, keybaord_init
//--------------------------------------------------------------------+
void test_keyboard_no_instances_invalid_para(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_UNPLUG);
  TEST_ASSERT_EQUAL(0, tusbh_hid_keyboard_no_instances(dev_addr));
}

void test_keyboard_install_ok(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(0, tusbh_hid_keyboard_no_instances(dev_addr));

  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, hidh_keyboard_install(dev_addr, (uint8_t*) &kbd_descriptor));
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(1, tusbh_hid_keyboard_no_instances(dev_addr));
}

void test_keyboard_init(void)
{
  hidh_keyboard_init();

  for(uint32_t i=0; i < sizeof(hidh_keyboard_info_t)*TUSB_CFG_HOST_DEVICE_MAX; i++)
    TEST_ASSERT_EQUAL(0, ((uint8_t*)keyboard_data) [i]);
}

//--------------------------------------------------------------------+
// keyboard_get
//--------------------------------------------------------------------+
void test_keyboard_get_invalid_address(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get(0, 0, NULL)); // invalid address
}

void test_keyboard_get_invalid_instance(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get(dev_addr, TUSB_CFG_HOST_HID_KEYBOARD_NO_INSTANCES_PER_DEVICE, &report)); // invalid instance
}

void test_keyboard_get_invalid_buffer(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get(dev_addr, 0, NULL)); // invalid buffer
}

void test_keyboard_get_device_not_ready(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_UNPLUG);
  TEST_ASSERT_EQUAL(TUSB_ERROR_DEVICE_NOT_READY, tusbh_hid_keyboard_get(dev_addr, 0, &report)); // device not mounted
}

void test_keyboard_get_class_not_supported()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  p_hidh_kbd_interface->pipe_in = (pipe_handle_t) { 0 };
  TEST_ASSERT_EQUAL(TUSB_ERROR_CLASS_DEVICE_DONT_SUPPORT, tusbh_hid_keyboard_get(dev_addr, instance_num, &report));
}

void test_keyboard_get_report_xfer_failed()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  hcd_pipe_xfer_ExpectAndReturn(p_hidh_kbd_interface->pipe_in, &report, p_hidh_kbd_interface->report_size, true, TUSB_ERROR_INVALID_PARA);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get(dev_addr, instance_num, &report));
}

void test_keyboard_get_ok()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  hcd_pipe_xfer_ExpectAndReturn(p_hidh_kbd_interface->pipe_in, &report, p_hidh_kbd_interface->report_size, true, TUSB_ERROR_NONE);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, tusbh_hid_keyboard_get(dev_addr, instance_num, &report));
}

