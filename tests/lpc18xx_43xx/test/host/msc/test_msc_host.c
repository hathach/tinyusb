/**************************************************************************/
/*!
    @file     test_msc_host.c
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

#include "descriptor_test.h"
#include "mock_osal.h"
#include "mock_hcd.h"
#include "mock_usbh.h"

#include "mock_msch_callback.h"
#include "msc_host.h"

extern msch_interface_t msch_data[TUSB_CFG_HOST_DEVICE_MAX];

static tusb_descriptor_interface_t const *p_msc_interface_desc = &desc_configuration.msc_interface;
static tusb_descriptor_endpoint_t const *p_edp_in  = &desc_configuration.msc_endpoint_in;
static tusb_descriptor_endpoint_t const *p_edp_out = &desc_configuration.msc_endpoint_out;

static msch_interface_t* p_msc;

static uint8_t dev_addr;
static pipe_handle_t pipe_in, pipe_out;
static pipe_handle_t const pipe_null = { 0 };
static uint16_t length;

void setUp(void)
{
  dev_addr = RANDOM(TUSB_CFG_HOST_DEVICE_MAX)+1;

  osal_semaphore_create_IgnoreAndReturn( (osal_semaphore_handle_t) 0x1234);
  msch_init();
  TEST_ASSERT_MEM_ZERO(msch_data, sizeof(msch_interface_t)*TUSB_CFG_HOST_DEVICE_MAX);

  p_msc = &msch_data[dev_addr-1];
  pipe_in  = (pipe_handle_t) {.dev_addr = dev_addr, .xfer_type = TUSB_XFER_BULK, .index = 1};
  pipe_out = (pipe_handle_t) {.dev_addr = dev_addr, .xfer_type = TUSB_XFER_BULK, .index = 2};
}

void tearDown(void)
{
  length = 0;
}

void test_open_pipe_in_failed(void)
{
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_edp_in, TUSB_CLASS_MSC, pipe_null);

  TEST_ASSERT(TUSB_ERROR_NONE != msch_open_subtask(dev_addr, p_msc_interface_desc, &length));
}

void test_open_pipe_out_failed(void)
{
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_edp_in, TUSB_CLASS_MSC, (pipe_handle_t) {1} );
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_edp_out, TUSB_CLASS_MSC, pipe_null);

  TEST_ASSERT(TUSB_ERROR_NONE != msch_open_subtask(dev_addr, p_msc_interface_desc, &length));
}

tusb_error_t stub_control_xfer(uint8_t dev_addr, uint8_t bmRequestType, uint8_t bRequest,
                               uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t* data, int num_call)
{
  switch ( num_call )
  {
    case 0: // get max lun
      TEST_ASSERT_EQUAL(bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_CLASS, TUSB_REQUEST_RECIPIENT_INTERFACE),
                        bmRequestType);
      TEST_ASSERT_EQUAL(MSC_REQUEST_GET_MAX_LUN, bRequest);
      TEST_ASSERT_EQUAL(p_msc_interface_desc->bInterfaceNumber, wIndex);
      TEST_ASSERT_EQUAL(1, wLength);
      *data = 1; // TODO multiple LUN support
    break;

    default:
      TEST_FAIL();
      return TUSB_ERROR_FAILED;
  }

  return TUSB_ERROR_NONE;
}
#if 0 // TODO TEST enable this
void test_open_desc_length(void)
{
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_edp_in, TUSB_CLASS_MSC, pipe_in);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_edp_out, TUSB_CLASS_MSC, pipe_out);

  usbh_control_xfer_subtask_IgnoreAndReturn(TUSB_ERROR_NONE);
  hcd_pipe_xfer_IgnoreAndReturn(TUSB_ERROR_NONE);
  hcd_pipe_queue_xfer_IgnoreAndReturn(TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( msch_open_subtask(dev_addr, p_msc_interface_desc, &length) );

  TEST_ASSERT_EQUAL(sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t),
                    length);
}

void test_open_ok(void)
{
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_edp_in, TUSB_CLASS_MSC, pipe_in);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_edp_out, TUSB_CLASS_MSC, pipe_out);

  //------------- get max lun -------------//
  usbh_control_xfer_subtask_StubWithCallback(stub_control_xfer);

  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( msch_open_subtask(dev_addr, p_msc_interface_desc, &length) );

  TEST_ASSERT_EQUAL(p_msc_interface_desc->bInterfaceNumber, p_msc->interface_number);
}
#endif
