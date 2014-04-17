/**************************************************************************/
/*!
    @file     test_host_hid.c
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

extern hidh_interface_info_t keyboardh_data[TUSB_CFG_HOST_DEVICE_MAX];

tusb_descriptor_interface_t const *p_kbd_interface_desc = &desc_configuration.keyboard_interface;
tusb_hid_descriptor_hid_t   const *p_kbh_hid_desc       = &desc_configuration.keyboard_hid;
tusb_descriptor_endpoint_t  const *p_kdb_endpoint_desc  = &desc_configuration.keyboard_endpoint;

void setUp(void)
{
  dev_addr = RANDOM(TUSB_CFG_HOST_DEVICE_MAX)+1;
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
//  hcd_pipe_open_IgnoreAndReturn( pipe_hdl );
//
//  //------------- Code Under TEST -------------//
//  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, hidh_open_subtask(dev_addr, p_kbd_interface_desc, &length) );
//
//  TEST_ASSERT_EQUAL(sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t),
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
