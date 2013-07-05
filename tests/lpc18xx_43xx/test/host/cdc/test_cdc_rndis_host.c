/**************************************************************************/
/*!
    @file     test_cdc_rndis_host.c
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
#include "mock_cdc_callback.h"

#include "descriptor_cdc.h"
#include "cdc_host.h"
#include "cdc_rndis_host.h"

static uint8_t dev_addr;
static uint16_t length;

static tusb_descriptor_interface_t const * p_comm_interface = &rndis_config_descriptor.cdc_comm_interface;
static tusb_descriptor_endpoint_t const * p_endpoint_notification = &rndis_config_descriptor.cdc_endpoint_notification;
static tusb_descriptor_endpoint_t const * p_endpoint_out = &rndis_config_descriptor.cdc_endpoint_out;
static tusb_descriptor_endpoint_t const * p_endpoint_in = &rndis_config_descriptor.cdc_endpoint_in;

static pipe_handle_t pipe_notification = { .dev_addr = 1, .xfer_type = TUSB_XFER_INTERRUPT };
static pipe_handle_t pipe_out          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 0 };
static pipe_handle_t pipe_int          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 1 };

extern cdch_data_t cdch_data[TUSB_CFG_HOST_DEVICE_MAX];
static cdch_data_t * p_cdc = &cdch_data[0];


void stub_mutex_wait(osal_mutex_handle_t mutex_hdl, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  *p_error = TUSB_ERROR_NONE;
}

void setUp(void)
{
  length = 0;
  dev_addr = 1;

  cdch_init();

  osal_mutex_wait_StubWithCallback(stub_mutex_wait);
  osal_mutex_release_IgnoreAndReturn(TUSB_ERROR_NONE);

  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_notification, TUSB_CLASS_CDC, pipe_notification);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_out, TUSB_CLASS_CDC, pipe_out);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_in, TUSB_CLASS_CDC, pipe_int);
}

void tearDown(void)
{

}


rndis_msg_initialize_t msg_init =
{
    .type          = RNDIS_MSG_INITIALIZE,
    .length        = sizeof(rndis_msg_initialize_t),
    .request_id    = 1, // TODO should use some magic number
    .major_version = 1,
    .minor_version = 0,
    .max_xfer_size = 0x4000 // TODO mimic windows
};

void test_rndis_send_initalize_failed(void)
{
  usbh_control_xfer_subtask_ExpectWithArrayAndReturn(
      dev_addr, 0x21, SEND_ENCAPSULATED_COMMAND, 0, p_comm_interface->bInterfaceNumber,
      sizeof(rndis_msg_initialize_t), (uint8_t*)&msg_init, sizeof(rndis_msg_initialize_t), TUSB_ERROR_OSAL_TIMEOUT);

  tusbh_cdc_mounted_cb_Expect(dev_addr);

  //------------- Code Under Test -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );
}

void test_rndis_send_initalize_ok(void)
{
  usbh_control_xfer_subtask_ExpectWithArrayAndReturn(
      dev_addr, 0x21, SEND_ENCAPSULATED_COMMAND, 0, p_comm_interface->bInterfaceNumber,
      sizeof(rndis_msg_initialize_t), (uint8_t*)&msg_init, sizeof(rndis_msg_initialize_t), TUSB_ERROR_NONE);


  tusbh_cdc_rndis_mounted_cb_Expect(dev_addr);

  //------------- Code Under Test -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );
}
