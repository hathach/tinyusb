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
#include "tusb_errors.h"
#include "binary.h"
#include "type_helper.h"

#include "mock_osal.h"
#include "mock_hcd.h"
#include "mock_usbh.h"
#include "mock_cdc_callback.h"

#include "descriptor_cdc.h"
#include "cdc_host.h"

#if TUSB_CFG_HOST_CDC_RNDIS // TODO enable
#include "cdc_rndis_host.h"
#endif

static uint8_t dev_addr;
static uint16_t length;

static tusb_descriptor_interface_t const * p_comm_interface = &cdc_config_descriptor.cdc_comm_interface;
static tusb_descriptor_endpoint_t const * p_endpoint_notification = &cdc_config_descriptor.cdc_endpoint_notification;
static tusb_descriptor_endpoint_t const * p_endpoint_out = &cdc_config_descriptor.cdc_endpoint_out;
static tusb_descriptor_endpoint_t const * p_endpoint_in = &cdc_config_descriptor.cdc_endpoint_in;

extern cdch_data_t cdch_data[TUSB_CFG_HOST_DEVICE_MAX];
static cdch_data_t * p_cdc = &cdch_data[0];

void setUp(void)
{
  length = 0;
  dev_addr = 1;

  memclr_(cdch_data, sizeof(cdch_data_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

void tearDown(void)
{

}

//--------------------------------------------------------------------+
// OPEN
//--------------------------------------------------------------------+
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
      sizeof(tusb_descriptor_interface_t) + sizeof(cdc_desc_func_header_t) +
      sizeof(cdc_desc_func_abstract_control_management_t) + sizeof(cdc_desc_func_union_t) +
      sizeof(tusb_descriptor_endpoint_t) +
      //------------- Data Interface -------------//
      sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);

  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  hcd_pipe_open_IgnoreAndReturn(dummy_hld);
  tusbh_cdc_mounted_cb_Expect(dev_addr);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  TEST_ASSERT_EQUAL(expected_length, length);
}

void test_cdch_open_interface_number_check(void)
{
  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  hcd_pipe_open_IgnoreAndReturn(dummy_hld);
  tusbh_cdc_mounted_cb_Expect(dev_addr);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  TEST_ASSERT_EQUAL(1, p_cdc->interface_number);

}

void test_cdch_open_protocol_check(void)
{
  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  hcd_pipe_open_IgnoreAndReturn(dummy_hld);
  tusbh_cdc_mounted_cb_Expect(dev_addr);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  TEST_ASSERT_EQUAL(p_comm_interface->bInterfaceProtocol, p_cdc->interface_protocol);

}

void test_cdch_open_acm_capacity_check(void)
{
  pipe_handle_t dummy_hld = { .dev_addr = 1 };
  hcd_pipe_open_IgnoreAndReturn(dummy_hld);
  tusbh_cdc_mounted_cb_Expect(dev_addr);

  //------------- CUT -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  TEST_ASSERT_EQUAL_MEMORY(&cdc_config_descriptor.cdc_acm.bmCapabilities, &p_cdc->acm_capability, 1);
}

//--------------------------------------------------------------------+
// CLOSE
//--------------------------------------------------------------------+
void test_cdch_close_device(void)
{
  pipe_handle_t pipe_notification = { .dev_addr = 1, .xfer_type = TUSB_XFER_INTERRUPT };
  pipe_handle_t pipe_out          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 0 };
  pipe_handle_t pipe_int          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 1 };

  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_notification, TUSB_CLASS_CDC, pipe_notification);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_out, TUSB_CLASS_CDC, pipe_out);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_in, TUSB_CLASS_CDC, pipe_int);
  tusbh_cdc_mounted_cb_Expect(dev_addr);

  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  hcd_pipe_close_ExpectAndReturn(pipe_notification , TUSB_ERROR_NONE);
  hcd_pipe_close_ExpectAndReturn(pipe_int          , TUSB_ERROR_NONE);
  hcd_pipe_close_ExpectAndReturn(pipe_out          , TUSB_ERROR_NONE);

  tusbh_cdc_unmounted_cb_Expect(dev_addr);

  //------------- CUT -------------//
  cdch_close(dev_addr);
}

//--------------------------------------------------------------------+
// CHECKING API
//--------------------------------------------------------------------+
void test_cdc_serial_is_mounted_not_configured(void)
{
  tusbh_device_get_mounted_class_flag_ExpectAndReturn(dev_addr, 0);

  TEST_ASSERT_FALSE( tusbh_cdc_serial_is_mounted(dev_addr) );
}

void test_cdc_serial_is_mounted_protocol_zero(void)
{
  tusbh_device_get_mounted_class_flag_ExpectAndReturn(dev_addr, BIT_(TUSB_CLASS_CDC) );
  cdch_data[0].interface_protocol = 0;

  TEST_ASSERT_FALSE( tusbh_cdc_serial_is_mounted(dev_addr) );
}

void test_cdc_serial_is_mounted_protocol_is_vendor(void)
{
  tusbh_device_get_mounted_class_flag_ExpectAndReturn(dev_addr, BIT_(TUSB_CLASS_CDC) );
  cdch_data[0].interface_protocol = 0xff;

  TEST_ASSERT_FALSE( tusbh_cdc_serial_is_mounted(dev_addr) );
}

void test_cdc_serial_is_mounted_protocol_is_at_command(void)
{
  tusbh_device_get_mounted_class_flag_ExpectAndReturn(dev_addr, BIT_(TUSB_CLASS_CDC) );
  cdch_data[0].interface_protocol = CDC_COMM_PROTOCOL_ATCOMMAND;

  TEST_ASSERT( tusbh_cdc_serial_is_mounted(dev_addr) );
}

//--------------------------------------------------------------------+
// TRANSFER API
//--------------------------------------------------------------------+
void test_cdc_xfer_notification_pipe(void)
{
  pipe_handle_t pipe_notification = { .dev_addr = 1, .xfer_type = TUSB_XFER_INTERRUPT };
  pipe_handle_t pipe_out          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 0 };
  pipe_handle_t pipe_in           = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 1 };

  cdch_data[dev_addr-1].pipe_notification = pipe_notification;
  cdch_data[dev_addr-1].pipe_out          = pipe_out;
  cdch_data[dev_addr-1].pipe_in           = pipe_in;

  tusbh_cdc_xfer_isr_Expect(dev_addr, TUSB_EVENT_XFER_COMPLETE, CDC_PIPE_NOTIFICATION, 10);

  //------------- CUT -------------//
  cdch_isr(pipe_notification, TUSB_EVENT_XFER_COMPLETE, 10);
}

void test_cdc_xfer_pipe_out(void)
{
  pipe_handle_t pipe_notification = { .dev_addr = 1, .xfer_type = TUSB_XFER_INTERRUPT };
  pipe_handle_t pipe_out          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 0 };
  pipe_handle_t pipe_in           = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 1 };

  cdch_data[dev_addr-1].pipe_notification = pipe_notification;
  cdch_data[dev_addr-1].pipe_out          = pipe_out;
  cdch_data[dev_addr-1].pipe_in           = pipe_in;

  tusbh_cdc_xfer_isr_Expect(dev_addr, TUSB_EVENT_XFER_ERROR, CDC_PIPE_DATA_OUT, 20);

  //------------- CUT -------------//
  cdch_isr(pipe_out, TUSB_EVENT_XFER_ERROR, 20);
}

void test_cdc_xfer_pipe_in(void)
{
  pipe_handle_t pipe_notification = { .dev_addr = 1, .xfer_type = TUSB_XFER_INTERRUPT };
  pipe_handle_t pipe_out          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 0 };
  pipe_handle_t pipe_in           = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 1 };

  cdch_data[dev_addr-1].pipe_notification = pipe_notification;
  cdch_data[dev_addr-1].pipe_out          = pipe_out;
  cdch_data[dev_addr-1].pipe_in           = pipe_in;

  tusbh_cdc_xfer_isr_Expect(dev_addr, TUSB_EVENT_XFER_STALLED, CDC_PIPE_DATA_IN, 0);

  //------------- CUT -------------//
  cdch_isr(pipe_in, TUSB_EVENT_XFER_STALLED, 0);
}
