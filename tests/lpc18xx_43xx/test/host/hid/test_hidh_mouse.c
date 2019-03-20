/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

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

extern hidh_interface_info_t mouseh_data[CFG_TUSB_HOST_DEVICE_MAX];
hidh_interface_info_t *p_hidh_mouse;
hid_mouse_report_t report;

tusb_desc_interface_t const *p_mouse_interface_desc = &desc_configuration.mouse_interface;
tusb_desc_endpoint_t  const *p_mouse_endpoint_desc  = &desc_configuration.mouse_endpoint;

uint8_t dev_addr;

void setUp(void)
{
  hidh_init();

  tu_memclr(&report, sizeof(hid_mouse_report_t));
  dev_addr = RANDOM(CFG_TUSB_HOST_DEVICE_MAX)+1;

  p_hidh_mouse = &mouseh_data[dev_addr-1];

  p_hidh_mouse->report_size = sizeof(hid_mouse_report_t);
  p_hidh_mouse->pipe_hdl = (pipe_handle_t) {
    .dev_addr  = dev_addr,
    .xfer_type = TUSB_XFER_INTERRUPT,
    .index     = 1
  };
}

void tearDown(void)
{

}

void test_mouse_init(void)
{
  hidh_init();

  TEST_ASSERT_MEM_ZERO(mouseh_data, sizeof(hidh_interface_info_t)*CFG_TUSB_HOST_DEVICE_MAX);
}

//------------- is supported -------------//
void test_mouse_is_supported_fail_unplug(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_UNPLUG);
  TEST_ASSERT_FALSE( tusbh_hid_mouse_is_mounted(dev_addr) );
}

void test_mouse_is_supported_fail_not_opened(void)
{
  hidh_init();
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_FALSE( tusbh_hid_mouse_is_mounted(dev_addr) );
}

void test_mouse_is_supported_ok(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_TRUE( tusbh_hid_mouse_is_mounted(dev_addr) );
}

void test_mouse_open_ok(void)
{
  uint16_t length=0;
  pipe_handle_t pipe_hdl = {.dev_addr = dev_addr, .xfer_type = TUSB_XFER_INTERRUPT, .index = 2};

  hidh_init();

  usbh_control_xfer_subtask_ExpectAndReturn(dev_addr, bm_request_type(TUSB_DIR_HOST_TO_DEV, TUSB_REQ_TYPE_CLASS, TUSB_REQ_RECIPIENT_INTERFACE),
                                            HID_REQ_CONTROL_SET_IDLE, 0, p_mouse_interface_desc->bInterfaceNumber, 0, NULL,
                                            TUSB_ERROR_NONE);
  hcd_edpt_open_ExpectAndReturn(dev_addr, p_mouse_endpoint_desc, TUSB_CLASS_HID, pipe_hdl);
  tusbh_hid_mouse_mounted_cb_Expect(dev_addr);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( hidh_open_subtask(dev_addr, p_mouse_interface_desc, &length));

  TEST_ASSERT_PIPE_HANDLE(pipe_hdl, p_hidh_mouse->pipe_hdl);
  TEST_ASSERT_EQUAL(8, p_hidh_mouse->report_size);
  TEST_ASSERT_EQUAL(sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_desc_endpoint_t),
                    length);
  TEST_ASSERT_EQUAL(p_mouse_interface_desc->bInterfaceNumber, p_hidh_mouse->interface_number);

  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_TRUE( tusbh_hid_mouse_is_mounted(dev_addr) );
//  TEST_ASSERT_FALSE( tusbh_hid_mouse_is_busy(dev_addr) );

}

//--------------------------------------------------------------------+
// mouse_get
//--------------------------------------------------------------------+
void test_mouse_get_invalid_address(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_mouse_get_report(0, NULL)); // invalid address
}

void test_mouse_get_invalid_buffer(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_mouse_get_report(dev_addr, NULL)); // invalid buffer
}

void test_mouse_get_device_not_ready(void)
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_UNPLUG);
  TEST_ASSERT_EQUAL(TUSB_ERROR_DEVICE_NOT_READY, tusbh_hid_mouse_get_report(dev_addr, &report)); // device not mounted
}

void test_mouse_get_report_xfer_failed()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  hcd_edpt_busy_ExpectAndReturn(p_hidh_mouse->pipe_hdl, false);
  hcd_pipe_xfer_ExpectAndReturn(p_hidh_mouse->pipe_hdl, (uint8_t*) &report, p_hidh_mouse->report_size, true, TUSB_ERROR_INVALID_PARA);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_INVALID_PARA, tusbh_hid_mouse_get_report(dev_addr, &report));
}

void test_mouse_get_report_xfer_failed_busy()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
  hcd_edpt_busy_ExpectAndReturn(p_hidh_mouse->pipe_hdl, true);

  TEST_ASSERT_EQUAL(TUSB_ERROR_INTERFACE_IS_BUSY, tusbh_hid_mouse_get_report(dev_addr, &report));
}

void test_mouse_get_ok()
{
  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
//  TEST_ASSERT_EQUAL(TUSB_INTERFACE_STATUS_READY, tusbh_hid_mouse_status(dev_addr));
  hcd_edpt_busy_ExpectAndReturn(p_hidh_mouse->pipe_hdl, false);
  hcd_pipe_xfer_ExpectAndReturn(p_hidh_mouse->pipe_hdl, (uint8_t*) &report, p_hidh_mouse->report_size, true, TUSB_ERROR_NONE);

  //------------- Code Under TEST -------------//
  TEST_ASSERT_STATUS( tusbh_hid_mouse_get_report(dev_addr, &report));

//  TEST_ASSERT_EQUAL(TUSB_INTERFACE_STATUS_BUSY, tusbh_hid_mouse_status(dev_addr));
}

void test_mouse_isr_event_xfer_complete(void)
{
  tusbh_hid_mouse_isr_Expect(dev_addr, XFER_RESULT_SUCCESS);

  //------------- Code Under TEST -------------//
  hidh_isr(p_hidh_mouse->pipe_hdl, XFER_RESULT_SUCCESS, 8);

  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
//  TEST_ASSERT_EQUAL(TUSB_INTERFACE_STATUS_COMPLETE, tusbh_hid_mouse_status(dev_addr));
}

void test_mouse_isr_event_xfer_error(void)
{
  tusbh_hid_mouse_isr_Expect(dev_addr, XFER_RESULT_FAILED);

  //------------- Code Under TEST -------------//
  hidh_isr(p_hidh_mouse->pipe_hdl, XFER_RESULT_FAILED, 0);

  tusbh_device_get_state_IgnoreAndReturn(TUSB_DEVICE_STATE_CONFIGURED);
//  TEST_ASSERT_EQUAL(TUSB_INTERFACE_STATUS_ERROR, tusbh_hid_mouse_status(dev_addr));
}



