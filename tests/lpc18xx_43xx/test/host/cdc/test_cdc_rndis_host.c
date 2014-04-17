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

void setUp(void)
{
}
void tearDown(void)
{
}

#if 0 // TODO enable
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
#include "cdc_rndis_host.h"

static uint8_t dev_addr;
static uint16_t length;

static tusb_descriptor_interface_t const * p_comm_interface = &rndis_config_descriptor.cdc_comm_interface;
static tusb_descriptor_endpoint_t const * p_endpoint_notification = &rndis_config_descriptor.cdc_endpoint_notification;
static tusb_descriptor_endpoint_t const * p_endpoint_out = &rndis_config_descriptor.cdc_endpoint_out;
static tusb_descriptor_endpoint_t const * p_endpoint_in = &rndis_config_descriptor.cdc_endpoint_in;

static pipe_handle_t pipe_notification = { .dev_addr = 1, .xfer_type = TUSB_XFER_INTERRUPT };
static pipe_handle_t pipe_out          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 0 };
static pipe_handle_t pipe_in          = { .dev_addr  = 1, .xfer_type = TUSB_XFER_BULK, .index = 1 };

extern cdch_data_t cdch_data[TUSB_CFG_HOST_DEVICE_MAX];
extern rndish_data_t rndish_data[TUSB_CFG_HOST_DEVICE_MAX];

static cdch_data_t * p_cdc = &cdch_data[0];
static rndish_data_t * p_rndis = &rndish_data[0];

enum {
  bmrequest_send = 0x21,
  bmrequest_get  = 0xA1
};

void stub_mutex_wait(osal_mutex_handle_t mutex_hdl, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  *p_error = TUSB_ERROR_NONE;
}

void setUp(void)
{
  length = 0;
  dev_addr = 1;

  for (uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX; i++)
  {
    osal_semaphore_create_ExpectAndReturn( &rndish_data[i].semaphore_notification, &rndish_data[i].semaphore_notification);
  }

  cdch_init();

  osal_mutex_wait_StubWithCallback(stub_mutex_wait);
  osal_mutex_release_IgnoreAndReturn(TUSB_ERROR_NONE);

  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_notification, TUSB_CLASS_CDC, pipe_notification);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_out, TUSB_CLASS_CDC, pipe_out);
  hcd_pipe_open_ExpectAndReturn(dev_addr, p_endpoint_in, TUSB_CLASS_CDC, pipe_in);
}

void tearDown(void)
{

}


static rndis_msg_initialize_t msg_init =
{
    .type          = RNDIS_MSG_INITIALIZE,
    .length        = sizeof(rndis_msg_initialize_t),
    .request_id    = 1, // TODO should use some magic number
    .major_version = 1,
    .minor_version = 0,
    .max_xfer_size = 0x4000 // TODO mimic windows
};

static rndis_msg_initialize_cmplt_t msg_init_cmplt =
{
    .type                    = RNDIS_MSG_INITIALIZE_CMPLT,
    .length                  = sizeof(rndis_msg_initialize_cmplt_t),
    .request_id              = 1,
    .status                  = RNDIS_STATUS_SUCCESS,
    .major_version           = 1,
    .minor_version           = 0,
    .device_flags            = 0x10,
    .medium                  = 0, // TODO cannot find info on this
    .max_packet_per_xfer     = 1,
    .max_xfer_size           = 0x100, // TODO change later
    .packet_alignment_factor = 5 // aligment of each RNDIS message (payload) = 2^factor
};

static rndis_msg_query_t msg_query_permanent_addr =
{
    .type          = RNDIS_MSG_QUERY,
    .length        = sizeof(rndis_msg_query_t)+6,
    .request_id    = 1,
    .oid           = RNDIS_OID_802_3_PERMANENT_ADDRESS,
    .buffer_length = 6,
    .buffer_offset = 20,
    .oid_buffer    = {0, 0, 0, 0, 0, 0}
};

static rndis_msg_query_cmplt_t msg_query_permanent_addr_cmplt =
{
    .type          = RNDIS_MSG_QUERY_CMPLT,
    .length        = sizeof(rndis_msg_query_cmplt_t)+6,
    .request_id    = 1,
    .status        = RNDIS_STATUS_SUCCESS,
    .buffer_length = 6,
    .buffer_offset = 16,
    .oid_buffer    = "CAFEBB"
};

static rndis_msg_set_t msg_set_packet_filter =
{
    .type          = RNDIS_MSG_SET,
    .length        = sizeof(rndis_msg_set_t)+4,
    .request_id    = 1,
    .oid           = RNDIS_OID_GEN_CURRENT_PACKET_FILTER,
    .buffer_length = 4,
    .buffer_offset = 20,
    .oid_buffer    = {RNDIS_PACKET_TYPE_DIRECTED | RNDIS_PACKET_TYPE_MULTICAST | RNDIS_PACKET_TYPE_BROADCAST, 0, 0, 0}
};

static rndis_msg_set_cmplt_t msg_set_packet_filter_cmplt =
{
    .type          = RNDIS_MSG_SET_CMPLT,
    .length        = sizeof(rndis_msg_set_cmplt_t),
    .request_id    = 1,
    .status        = RNDIS_STATUS_SUCCESS
};

static tusb_error_t  stub_pipe_notification_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete, int num_call)
{
  TEST_ASSERT( pipehandle_is_equal(pipe_notification, pipe_hdl) );
  TEST_ASSERT_EQUAL( 8, total_bytes );
  TEST_ASSERT( int_on_complete );

  buffer[0] = 1; // response available

  cdch_isr(pipe_hdl, TUSB_EVENT_XFER_COMPLETE, 8);

  return TUSB_ERROR_NONE;
}

void stub_sem_wait_success(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  TEST_ASSERT_EQUAL_HEX(p_rndis->sem_notification_hdl, sem_hdl);
  (*p_error) = TUSB_ERROR_NONE;
}

void stub_sem_wait_timeout(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  (*p_error) = TUSB_ERROR_OSAL_TIMEOUT;
}

//------------- Test Code -------------//
void test_rndis_send_initalize_failed(void)
{
  usbh_control_xfer_subtask_ExpectWithArrayAndReturn(
      dev_addr, bmrequest_send, SEND_ENCAPSULATED_COMMAND, 0, p_comm_interface->bInterfaceNumber,
      sizeof(rndis_msg_initialize_t), (uint8_t*)&msg_init, sizeof(rndis_msg_initialize_t), TUSB_ERROR_OSAL_TIMEOUT);

  tusbh_cdc_mounted_cb_Expect(dev_addr);

  //------------- Code Under Test -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, cdch_open_subtask(dev_addr, p_comm_interface, &length) );
}

void test_rndis_initialization_notification_timeout(void)
{
  usbh_control_xfer_subtask_ExpectWithArrayAndReturn(
      dev_addr, bmrequest_send, SEND_ENCAPSULATED_COMMAND, 0, p_comm_interface->bInterfaceNumber,
      sizeof(rndis_msg_initialize_t), (uint8_t*)&msg_init, sizeof(rndis_msg_initialize_t), TUSB_ERROR_NONE);

  hcd_pipe_xfer_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_semaphore_wait_StubWithCallback(stub_sem_wait_timeout);

  tusbh_cdc_mounted_cb_Expect(dev_addr);

  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( cdch_open_subtask(dev_addr, p_comm_interface, &length) );
  TEST_ASSERT_FALSE(p_cdc->is_rndis);
}

tusb_error_t stub_control_xfer(uint8_t addr, uint8_t bmRequestType, uint8_t bRequest,
                               uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t* data, int num_call )
{
  TEST_ASSERT_EQUAL(p_comm_interface->bInterfaceNumber, wIndex);
  TEST_ASSERT_EQUAL(0, wValue);
  TEST_ASSERT_EQUAL(dev_addr, addr);

  switch(num_call)
  {
    //------------- Initialize -------------//
    case 0*2+0: //  initialize
      TEST_ASSERT_EQUAL(bmrequest_send, bmRequestType);
      TEST_ASSERT_EQUAL(SEND_ENCAPSULATED_COMMAND, bRequest);
      TEST_ASSERT_EQUAL(sizeof(rndis_msg_initialize_t), wLength);
      TEST_ASSERT_EQUAL_HEX8_ARRAY(&msg_init, data, wLength);
    break;

    case 0*2+1: // initialize complete
      TEST_ASSERT_EQUAL(bmrequest_get, bmRequestType);
      TEST_ASSERT_EQUAL(GET_ENCAPSULATED_RESPONSE, bRequest);
      TEST_ASSERT( wLength >= 0x0400 ); // Microsoft Specs
      memcpy(data, &msg_init_cmplt, sizeof(rndis_msg_initialize_cmplt_t));
    break;

    // query for RNDIS_OID_802_3_PERMANENT_ADDRESS
    case 1*2+0:
      TEST_ASSERT_EQUAL(bmrequest_send, bmRequestType);
      TEST_ASSERT_EQUAL(SEND_ENCAPSULATED_COMMAND, bRequest);
      TEST_ASSERT_EQUAL(sizeof(rndis_msg_query_t) + 6, wLength); // 6 bytes for MAC address
      TEST_ASSERT_EQUAL_HEX8_ARRAY(&msg_query_permanent_addr, data, wLength);
    break;

    case 1*2+1: // query complete for RNDIS_OID_802_3_PERMANENT_ADDRESS
      TEST_ASSERT_EQUAL(bmrequest_get, bmRequestType);
      TEST_ASSERT_EQUAL(GET_ENCAPSULATED_RESPONSE, bRequest);
      TEST_ASSERT( wLength >= 0x0400 ); // Microsoft Specs
      memcpy(data, &msg_query_permanent_addr_cmplt, sizeof(rndis_msg_query_cmplt_t) + 6);
    break;

    // set RNDIS_OID_GEN_CURRENT_PACKET_FILTER to DIRECTED | MULTICAST | BROADCAST
    case 2*2+0:
      TEST_ASSERT_EQUAL(bmrequest_send, bmRequestType);
      TEST_ASSERT_EQUAL(SEND_ENCAPSULATED_COMMAND, bRequest);
      TEST_ASSERT_EQUAL(sizeof(rndis_msg_set_t)+4, wLength);
      TEST_ASSERT_EQUAL_HEX8_ARRAY(&msg_set_packet_filter, data, wLength);
    break;

    case 2*2+1: // query complete for RNDIS_OID_802_3_PERMANENT_ADDRESS
      TEST_ASSERT_EQUAL(bmrequest_get, bmRequestType);
      TEST_ASSERT_EQUAL(GET_ENCAPSULATED_RESPONSE, bRequest);
      TEST_ASSERT( wLength >= 0x0400 ); // Microsoft Specs
      memcpy(data, &msg_set_packet_filter_cmplt, sizeof(rndis_msg_set_cmplt_t) );
    break;

    default:
    return TUSB_ERROR_OSAL_TIMEOUT;
  }

  return TUSB_ERROR_NONE;
}

void test_rndis_initialization_sequence_ok(void)
{
  usbh_control_xfer_subtask_StubWithCallback(stub_control_xfer);
  hcd_pipe_xfer_StubWithCallback(stub_pipe_notification_xfer);
  osal_semaphore_wait_StubWithCallback(stub_sem_wait_success);

  osal_semaphore_post_ExpectAndReturn(p_rndis->sem_notification_hdl, TUSB_ERROR_NONE);
  osal_semaphore_post_ExpectAndReturn(p_rndis->sem_notification_hdl, TUSB_ERROR_NONE);
  osal_semaphore_post_ExpectAndReturn(p_rndis->sem_notification_hdl, TUSB_ERROR_NONE);

  tusbh_cdc_rndis_mounted_cb_Expect(dev_addr);

  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( cdch_open_subtask(dev_addr, p_comm_interface, &length) );

  TEST_ASSERT(p_cdc->is_rndis);
  TEST_ASSERT_EQUAL(msg_init_cmplt.max_xfer_size, p_rndis->max_xfer_size);
  TEST_ASSERT_EQUAL_HEX8_ARRAY("CAFEBB", p_rndis->mac_address, 6);
}
#endif
