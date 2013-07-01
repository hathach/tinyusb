/**************************************************************************/
/*!
    @file     test_cdc_host.c
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "stdlib.h"
#include "unity.h"
#include "tusb_option.h"
#include "errors.h"
#include "binary.h"
#include "type_helper.h"

#include "mock_osal.h"
#include "mock_hcd.h"
#include "mock_usbh.h"

#include "descriptor_cdc.h"
#include "cdc_host.h"

uint8_t dev_addr;
uint16_t length;

tusb_descriptor_interface_t const * p_comm_interface = &cdc_config_descriptor.cdc_comm_interface;
tusb_descriptor_endpoint_t const * p_endpoint_notification = &cdc_config_descriptor.cdc_endpoint_notification;
tusb_descriptor_endpoint_t const * p_endpoint_out = &cdc_config_descriptor.cdc_endpoint_out;
tusb_descriptor_endpoint_t const * p_endpoint_in = &cdc_config_descriptor.cdc_endpoint_in;

extern cdch_data_t cdch_data[TUSB_CFG_HOST_DEVICE_MAX];
cdch_data_t * p_cdc = &cdch_data[0];

void setUp(void)
{
  length = 0;
  dev_addr = 1;

  cdch_init();
}

void tearDown(void)
{

}


void test_cdch_open_failed_to_open_notification_endpoint(void)
{
  pipe_handle_t null_hdl = {0};

  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_notification, TUSB_CLASS_CDC, null_hdl);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_HCD_OPEN_PIPE_FAILED, cdch_open_subtask(dev_addr, p_comm_interface, &length));

}

void test_cdch_open_failed_to_open_data_endpoint_out(void)
{
  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  pipe_handle_t null_hdl = {0};

  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_notification, TUSB_CLASS_CDC, dummy_hld);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_out, TUSB_CLASS_CDC, null_hdl);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_HCD_OPEN_PIPE_FAILED, cdch_open_subtask(dev_addr, p_comm_interface, &length));

}

void test_cdch_open_failed_to_open_data_endpoint_in(void)
{
  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  pipe_handle_t null_hdl = {0};

  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_notification, TUSB_CLASS_CDC, dummy_hld);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_out, TUSB_CLASS_CDC, dummy_hld);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_in, TUSB_CLASS_CDC, null_hdl);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_HCD_OPEN_PIPE_FAILED, cdch_open_subtask(dev_addr, p_comm_interface, &length));

}

void test_cdch_open_length_check(void)
{
  const uint16_t expected_length =
      //------------- Comm Interface -------------//
      sizeof(tusb_descriptor_interface_t) + sizeof(tusb_cdc_func_header_t) +
      sizeof(tusb_cdc_func_abstract_control_management_t) + sizeof(tusb_cdc_func_union_t) +
      sizeof(tusb_descriptor_endpoint_t) +
      //------------- Data Interface -------------//
      sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);

  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  hcd_pipe_open_IgnoreAndReturn(dummy_hld);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  TEST_ASSERT_EQUAL(expected_length, length);
}

void test_cdch_open_interface_number_check(void)
{
  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  hcd_pipe_open_IgnoreAndReturn(dummy_hld);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  TEST_ASSERT_EQUAL(1, p_cdc->interface_number);

}

void test_cdch_open_acm_capacity_check(void)
{
  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  hcd_pipe_open_IgnoreAndReturn(dummy_hld);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  TEST_ASSERT_EQUAL_MEMORY(&cdc_config_descriptor.cdc_acm.bmCapabilities, &p_cdc->acm_capability, 1);
}




