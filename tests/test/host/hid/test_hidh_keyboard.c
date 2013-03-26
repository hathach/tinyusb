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
#include "type_helper.h"
#include "errors.h"
#include "hid_host.h"
#include "mock_osal.h"
#include "mock_usbh.h"
#include "mock_hcd.h"
#include "descriptor_test.h"

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
hidh_keyboard_info_t *p_hidh_kbd;

tusb_descriptor_interface_t const *p_kbd_interface_desc = &desc_configuration.keyboard_interface;
tusb_descriptor_endpoint_t  const *p_kdb_endpoint_desc  = &desc_configuration.keyboard_endpoint;

void setUp(void)
{
  memclr_(&report, sizeof(tusb_keyboard_report_t));
  dev_addr = RANDOM(TUSB_CFG_HOST_DEVICE_MAX)+1;

  hidh_init();

  p_hidh_kbd = &keyboard_data[dev_addr-1];

  p_hidh_kbd->report_size = sizeof(tusb_keyboard_report_t);
  p_hidh_kbd->pipe_hdl = (pipe_handle_t) {
    .dev_addr  = dev_addr,
    .xfer_type = TUSB_XFER_INTERRUPT,
    .index     = 1
  };

}

void tearDown(void)
{
}

void test_keyboard_init(void)
{
  hidh_init();

  TEST_ASSERT_MEM_ZERO(keyboard_data, sizeof(hidh_keyboard_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

void test_keyboard_is_supported_fail_unplug(void)
{
  hidh_init();
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_UNPLUG);
  TEST_ASSERT_FALSE( tusbh_hid_keyboard_is_supported(dev_addr) );
}

void test_keyboard_is_supported_fail_not_opened(void)
{
  hidh_init();
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_FALSE( tusbh_hid_keyboard_is_supported(dev_addr) );
}

void test_keyboard_open_ok(void)
{
  uint16_t length=0;
  pipe_handle_t pipe_hdl = {.dev_addr = dev_addr, .xfer_type = TUSB_XFER_INTERRUPT, .index = 2};
  memclr_(p_hidh_kbd, sizeof(hidh_keyboard_info_t));

  hcd_pipe_open_ExpectAndReturn(dev_addr, p_kdb_endpoint_desc, TUSB_CLASS_HID, pipe_hdl);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, hidh_open_subtask(dev_addr, p_kbd_interface_desc, &length));

  TEST_ASSERT_PIPE_HANDLE(pipe_hdl, p_hidh_kbd->pipe_hdl);
  TEST_ASSERT_EQUAL(8, p_hidh_kbd->report_size);
  TEST_ASSERT_EQUAL(sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t),
                    length);

  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_TRUE( tusbh_hid_keyboard_is_supported(dev_addr) );
}

void test_keyboard_close(void)
{

}

//--------------------------------------------------------------------+
// keyboard_get
//--------------------------------------------------------------------+
void test_keyboard_get_invalid_address(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get_report(0, 0, NULL)); // invalid address
}

void test_keyboard_get_invalid_buffer(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get_report(dev_addr, 0, NULL)); // invalid buffer
}

void test_keyboard_get_device_not_ready(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_UNPLUG);
  TEST_ASSERT_EQUAL(TUSB_ERROR_DEVICE_NOT_READY, tusbh_hid_keyboard_get_report(dev_addr, 0, &report)); // device not mounted
}

void test_keyboard_get_report_xfer_failed()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  hcd_pipe_xfer_ExpectAndReturn(p_hidh_kbd->pipe_hdl, (uint8_t*) &report, p_hidh_kbd->report_size, true, TUSB_ERROR_INVALID_PARA);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get_report(dev_addr, 0, &report));
}

void test_keyboard_get_ok()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  hcd_pipe_xfer_ExpectAndReturn(p_hidh_kbd->pipe_hdl, (uint8_t*) &report, p_hidh_kbd->report_size, true, TUSB_ERROR_NONE);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, tusbh_hid_keyboard_get_report(dev_addr, 0, &report));
}

