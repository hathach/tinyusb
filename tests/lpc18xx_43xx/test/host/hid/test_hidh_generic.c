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
#include "mock_cdc_host.h"
#include "mock_msc_host.h"
#include "mock_hub.h"

#include "mock_hcd.h"
#include "usbh.h"
#include "mock_hidh_callback.h"
#include "descriptor_test.h"
#include "host_helper.h"


extern hidh_interface_info_t mouseh_data[CFG_TUSB_HOST_DEVICE_MAX];
hidh_interface_info_t *p_hidh_mouse;
hid_mouse_report_t report;

tusb_desc_interface_t const *p_mouse_interface_desc = &desc_configuration.mouse_interface;
tusb_desc_endpoint_t  const *p_mouse_endpoint_desc  = &desc_configuration.mouse_endpoint;

uint8_t dev_addr;

void setUp(void)
{
  helper_usbh_init_expect();
  usbh_init();

  dev_addr = RANDOM(CFG_TUSB_HOST_DEVICE_MAX)+1;

//  uint16_t length;
//  TEST_ASSERT_STATUS( hidh_open_subtask(dev_addr, p_mouse_interface_desc, &length) );
//
//  p_hidh_mouse = &mouse_data[dev_addr-1];
//
//  p_hidh_mouse->report_size = sizeof(hid_mouse_report_t);
//  p_hidh_mouse->pipe_hdl = (pipe_handle_t) {
//    .dev_addr  = dev_addr,
//    .xfer_type = TUSB_XFER_INTERRUPT,
//    .index     = 1
//  };
}

void tearDown(void)
{

}

//void test_generic_init(void)
//{
//  hidh_init();
//
//  TEST_ASSERT_MEM_ZERO(mouse_data, sizeof(hidh_interface_info_t)*CFG_TUSB_HOST_DEVICE_MAX);
//}
