/**************************************************************************/
/*!
    @file     test_host_hid_keyboard.c
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

#include "stdlib.h"
#include "unity.h"
#include "type_helper.h"
#include "tusb_errors.h"
#include "common/common.h"
#include "hid_host.h"
#include "mock_osal.h"
#include "mock_usbh.h"
#include "mock_hcd.h"
#include "mock_hidh_callback.h"
#include "descriptor_test.h"

extern hidh_interface_info_t keyboardh_data[TUSB_CFG_HOST_DEVICE_MAX];

hid_keyboard_report_t sample_key[2] =
{
    {
        .modifier = KEYBOARD_MODIFIER_LEFTCTRL,
        .keycode = {4}//{KEYBOARD_KEYCODE_a} TODO ascii to key code table
    },
    {
        .modifier = KEYBOARD_MODIFIER_RIGHTALT,
        .keycode = {5}//{KEYBOARD_KEYCODE_z}
    }
};

uint8_t dev_addr;
hidh_interface_info_t *p_hidh_kbd;

hid_keyboard_report_t report;

tusb_descriptor_interface_t const *p_kbd_interface_desc = &desc_configuration.keyboard_interface;
tusb_descriptor_endpoint_t  const *p_kdb_endpoint_desc  = &desc_configuration.keyboard_endpoint;

void setUp(void)
{
  hidh_init();
  memclr_(&report, sizeof(hid_keyboard_report_t));
  dev_addr = RANDOM(TUSB_CFG_HOST_DEVICE_MAX)+1;

  p_hidh_kbd = &keyboardh_data[dev_addr-1];

  p_hidh_kbd->report_size = sizeof(hid_keyboard_report_t);
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

  TEST_ASSERT_MEM_ZERO(keyboardh_data, sizeof(hidh_interface_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

//------------- is supported -------------//
void test_keyboard_is_supported_fail_unplug(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_UNPLUG);
  TEST_ASSERT_FALSE( tusbh_hid_keyboard_is_mounted(dev_addr) );
}

void test_keyboard_is_supported_fail_not_opened(void)
{
  hidh_init();
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_FALSE( tusbh_hid_keyboard_is_mounted(dev_addr) );
}

void test_keyboard_is_supported_ok(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_TRUE( tusbh_hid_keyboard_is_mounted(dev_addr) );
}

tusb_error_t stub_set_idle_request(uint8_t address, tusb_control_request_t const* p_request, uint8_t* data, int num_call)
{
  TEST_ASSERT_EQUAL( dev_addr, address);

  //------------- expecting Set Idle with value = 0 -------------//
  TEST_ASSERT_NOT_NULL( p_request );
  TEST_ASSERT_EQUAL(TUSB_DIR_HOST_TO_DEV                   , p_request->bmRequestType_bit.direction);
  TEST_ASSERT_EQUAL(TUSB_REQUEST_TYPE_CLASS                , p_request->bmRequestType_bit.type);
  TEST_ASSERT_EQUAL(TUSB_REQUEST_RECIPIENT_INTERFACE       , p_request->bmRequestType_bit.recipient);
  TEST_ASSERT_EQUAL(HID_REQUEST_CONTROL_SET_IDLE           , p_request->bRequest);
  TEST_ASSERT_EQUAL(0                                      , p_request->wValue);
  TEST_ASSERT_EQUAL(p_kbd_interface_desc->bInterfaceNumber , p_request->wIndex);

  TEST_ASSERT_NULL(data);

  return TUSB_ERROR_NONE;
}

void test_keyboard_open_ok(void)
{
  uint16_t length=0;
  pipe_handle_t pipe_hdl = {.dev_addr = dev_addr, .xfer_type = TUSB_XFER_INTERRUPT, .index = 2};

  hidh_init();

  usbh_control_xfer_subtask_ExpectAndReturn(dev_addr, bm_request_type(TUSB_DIR_HOST_TO_DEV, TUSB_REQUEST_TYPE_CLASS, TUSB_REQUEST_RECIPIENT_INTERFACE),
                                            HID_REQUEST_CONTROL_SET_IDLE, 0, p_kbd_interface_desc->bInterfaceNumber, 0, NULL,
                                            TUSB_ERROR_NONE);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_kdb_endpoint_desc, TUSB_CLASS_HID, pipe_hdl);
  tusbh_hid_keyboard_mounted_cb_Expect(dev_addr);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, hidh_open_subtask(dev_addr, p_kbd_interface_desc, &length));

  TEST_ASSERT_PIPE_HANDLE(pipe_hdl, p_hidh_kbd->pipe_hdl);
  TEST_ASSERT_EQUAL(8, p_hidh_kbd->report_size);
  TEST_ASSERT_EQUAL(sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t),
                    length);
  TEST_ASSERT_EQUAL(p_kbd_interface_desc->bInterfaceNumber, p_hidh_kbd->interface_number);

  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_TRUE( tusbh_hid_keyboard_is_mounted(dev_addr) );
//  TEST_ASSERT_FALSE( tusbh_hid_keyboard_is_busy(dev_addr) );
}

//--------------------------------------------------------------------+
// keyboard_get
//--------------------------------------------------------------------+
void test_keyboard_get_invalid_address(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get_report(0, NULL)); // invalid address
}

void test_keyboard_get_invalid_buffer(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get_report(dev_addr, NULL)); // invalid buffer
}

void test_keyboard_get_device_not_ready(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_UNPLUG);
  TEST_ASSERT_EQUAL(TUSB_ERROR_DEVICE_NOT_READY, tusbh_hid_keyboard_get_report(dev_addr, &report)); // device not mounted
}

void test_keyboard_get_report_xfer_failed()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  hcd_pipe_is_busy_ExpectAndReturn(p_hidh_kbd->pipe_hdl, false);
  hcd_pipe_xfer_ExpectAndReturn(p_hidh_kbd->pipe_hdl, (uint8_t*) &report, p_hidh_kbd->report_size, true, TUSB_ERROR_INVALID_PARA);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_keyboard_get_report(dev_addr, &report));
}

void test_keyboard_get_report_xfer_failed_busy()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  hcd_pipe_is_busy_ExpectAndReturn(p_hidh_kbd->pipe_hdl, true);

  TEST_ASSERT_EQUAL(TUSB_ERROR_INTERFACE_IS_BUSY, tusbh_hid_keyboard_get_report(dev_addr, &report));
}

void test_keyboard_get_ok()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
//  TEST_ASSERT_EQUAL(TUSB_INTERFACE_STATUS_READY, tusbh_hid_keyboard_status(dev_addr));
  hcd_pipe_is_busy_ExpectAndReturn(p_hidh_kbd->pipe_hdl, false);
  hcd_pipe_xfer_ExpectAndReturn(p_hidh_kbd->pipe_hdl, (uint8_t*) &report, p_hidh_kbd->report_size, true, TUSB_ERROR_NONE);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, tusbh_hid_keyboard_get_report(dev_addr, &report));
//  TEST_ASSERT_EQUAL(TUSB_INTERFACE_STATUS_BUSY, tusbh_hid_keyboard_status(dev_addr));
}

void test_keyboard_isr_event_complete(void)
{
  tusbh_hid_keyboard_isr_Expect(dev_addr, TUSB_EVENT_XFER_COMPLETE);

  //------------- Code Under TEST -------------//
  hidh_isr(p_hidh_kbd->pipe_hdl, TUSB_EVENT_XFER_COMPLETE, 8);

//  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
//  TEST_ASSERT_EQUAL(TUSB_INTERFACE_STATUS_COMPLETE, tusbh_hid_keyboard_status(dev_addr));
}


