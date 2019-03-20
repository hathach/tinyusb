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
#include "tusb_option.h"
#include "tusb_errors.h"
#include "binary.h"
#include "type_helper.h"
#include "common/common.h"

#include "descriptor_test.h"
#include "mock_osal.h"
#include "mock_hcd.h"
#include "mock_usbh.h"
#include "hid_host.h"
#include "mock_hidh_callback.h"

uint8_t dev_addr;
pipe_handle_t pipe_hdl;

extern hidh_interface_info_t keyboardh_data[CFG_TUSB_HOST_DEVICE_MAX];

tusb_desc_interface_t const *p_kbd_interface_desc = &desc_configuration.keyboard_interface;
tusb_hid_descriptor_hid_t   const *p_kbh_hid_desc       = &desc_configuration.keyboard_hid;
tusb_desc_endpoint_t  const *p_kdb_endpoint_desc  = &desc_configuration.keyboard_endpoint;

void setUp(void)
{
  dev_addr = RANDOM(CFG_TUSB_HOST_DEVICE_MAX)+1;
  pipe_hdl = (pipe_handle_t) {.dev_addr = dev_addr, .xfer_type = TUSB_XFER_INTERRUPT, .index = 2};
}

void tearDown(void)
{
}

//void test_hidh_open_ok(void)
//{
//  uint16_t length=0;
//
//  // TODO expect get HID report descriptor
//  hcd_edpt_open_IgnoreAndReturn( pipe_hdl );
//
//  //------------- Code Under TEST -------------//
//  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, hidh_open_subtask(dev_addr, p_kbd_interface_desc, &length) );
//
//  TEST_ASSERT_EQUAL(sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_desc_endpoint_t),
//                    length);
//}

void test_hidh_close(void)
{
  keyboardh_data[dev_addr-1].pipe_hdl = pipe_hdl;
  keyboardh_data[dev_addr-1].report_size = 8;

  hcd_pipe_close_ExpectAndReturn(pipe_hdl, TUSB_ERROR_NONE);
  tusbh_hid_keyboard_unmounted_cb_Expect(dev_addr);

  //------------- Code Under TEST -------------//
  hidh_close(dev_addr);

  TEST_ASSERT_MEM_ZERO(&keyboardh_data[dev_addr-1], sizeof(hidh_interface_info_t));
}
