/**************************************************************************/
/*!
    @file     test_enum_task.c
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

#include "unity.h"
#include "tusb_errors.h"
#include "descriptor_test.h"

#include "mock_osal.h"
#include "usbh.h"
#include "mock_hcd.h"
#include "mock_hub.h"
#include "usbh_hcd.h"

#include "mock_tusb_callback.h"
#include "mock_hid_host.h"
#include "mock_cdc_host.h"
#include "mock_msc_host.h"

extern usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1];
extern uint8_t enum_data_buffer[TUSB_CFG_HOST_ENUM_BUFFER_SIZE];

usbh_enumerate_t const enum_connect = {
    .core_id  = 0,
    .hub_addr = 0,
    .hub_port = 0,
};

tusb_speed_t device_speed = TUSB_SPEED_FULL;

void queue_recv_stub (osal_queue_handle_t const queue_hdl, uint32_t *p_data, uint32_t msec, tusb_error_t *p_error, int num_call);
void semaphore_wait_success_stub(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error, int num_call);
tusb_error_t control_xfer_stub(uint8_t dev_addr, const tusb_control_request_t * const p_request, uint8_t data[], int num_call);

enum {
  POWER_STABLE_DELAY = 500,
  RESET_DELAY = 200 // USB specs say only 50ms though many devices requires a longer time
};

void setUp(void)
{
  memclr_(usbh_devices, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  osal_queue_receive_StubWithCallback(queue_recv_stub);
  osal_semaphore_wait_StubWithCallback(semaphore_wait_success_stub);
  osal_semaphore_post_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_mutex_wait_StubWithCallback(semaphore_wait_success_stub);
  osal_mutex_release_IgnoreAndReturn(TUSB_ERROR_NONE);
  hcd_pipe_control_xfer_StubWithCallback(control_xfer_stub);

  hcd_port_connect_status_ExpectAndReturn(enum_connect.core_id, true);
  osal_task_delay_Expect(POWER_STABLE_DELAY);
  hcd_port_connect_status_ExpectAndReturn(enum_connect.core_id, true);
  hcd_port_reset_Expect(enum_connect.core_id);
  osal_task_delay_Expect(RESET_DELAY);
  hcd_port_speed_get_ExpectAndReturn(enum_connect.core_id, device_speed);

  osal_semaphore_reset_Expect( usbh_devices[0].control.sem_hdl );
  osal_mutex_reset_Expect( usbh_devices[0].control.mutex_hdl );
  hcd_pipe_control_open_ExpectAndReturn(0, 8, TUSB_ERROR_NONE);
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// STUB & HELPER
//--------------------------------------------------------------------+
void queue_recv_stub (osal_queue_handle_t const queue_hdl, uint32_t *p_data, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  (*p_data) = ( *((uint32_t*) &enum_connect) );
  (*p_error) = TUSB_ERROR_NONE;
}

void semaphore_wait_success_stub(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  (*p_error) = TUSB_ERROR_NONE;
}

#define semaphore_wait_timeout_stub(n) semaphore_wait_timeout_##n
#define semaphore_wait_timeout(n) \
  void semaphore_wait_timeout_##n(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error, int num_call) {\
    if (num_call >= n)\
      (*p_error) = TUSB_ERROR_OSAL_TIMEOUT;\
    else  \
      (*p_error) = TUSB_ERROR_NONE;\
  }

semaphore_wait_timeout(0)
semaphore_wait_timeout(1)
semaphore_wait_timeout(2)
semaphore_wait_timeout(3)
semaphore_wait_timeout(4)
semaphore_wait_timeout(5)
semaphore_wait_timeout(6)
semaphore_wait_timeout(7)
semaphore_wait_timeout(8)

tusb_error_t control_xfer_stub(uint8_t dev_addr, const tusb_control_request_t * const p_request, uint8_t data[], int num_call)
{
  switch (num_call)
  {
    case 0: // get 8 bytes of device descriptor
      TEST_ASSERT_EQUAL(TUSB_REQUEST_GET_DESCRIPTOR, p_request->bRequest);
      TEST_ASSERT_EQUAL(TUSB_DESC_TYPE_DEVICE, p_request->wValue >> 8);
      TEST_ASSERT_EQUAL(8, p_request->wLength);
      memcpy(data, &desc_device, p_request->wLength);
    break;

    case 1: // set device address
      TEST_ASSERT_EQUAL(TUSB_REQUEST_SET_ADDRESS, p_request->bRequest);
      TEST_ASSERT_EQUAL(p_request->wValue, 1);
    break;

    case 2: // get full device decriptor for new address
      TEST_ASSERT_EQUAL(TUSB_REQUEST_GET_DESCRIPTOR, p_request->bRequest);
      TEST_ASSERT_EQUAL(TUSB_DESC_TYPE_DEVICE, p_request->wValue >> 8);
      TEST_ASSERT_EQUAL(18, p_request->wLength);
      memcpy(data, &desc_device, p_request->wLength);
    break;

    case 3: // get 9 bytes of configuration descriptor
      TEST_ASSERT_EQUAL(TUSB_REQUEST_GET_DESCRIPTOR, p_request->bRequest);
      TEST_ASSERT_EQUAL(TUSB_DESC_TYPE_CONFIGURATION, p_request->wValue >> 8);
      TEST_ASSERT_EQUAL(9, p_request->wLength);
      memcpy(data, &desc_configuration, p_request->wLength);
    break;

    case 4: // get full-length configuration descriptor
      TEST_ASSERT_EQUAL(TUSB_REQUEST_GET_DESCRIPTOR, p_request->bRequest);
      TEST_ASSERT_EQUAL(TUSB_DESC_TYPE_CONFIGURATION, p_request->wValue >> 8);
      TEST_ASSERT_EQUAL(TUSB_CFG_HOST_ENUM_BUFFER_SIZE, p_request->wLength);
      memcpy(data, &desc_configuration, p_request->wLength);
    break;

    case 5: // set configure
      TEST_ASSERT_EQUAL(TUSB_REQUEST_SET_CONFIGURATION, p_request->bRequest);
      TEST_ASSERT_EQUAL(1, p_request->wValue);
      TEST_ASSERT_EQUAL(0, p_request->wIndex);
      TEST_ASSERT_EQUAL(0, p_request->wLength);
    break;

    default:
      return TUSB_ERROR_OSAL_TIMEOUT;
  }

  usbh_xfer_isr(
      (pipe_handle_t) { .dev_addr = (num_call > 1 ? 1 : 0), .xfer_type = TUSB_XFER_CONTROL },
      0, TUSB_EVENT_XFER_COMPLETE, 0);

  return TUSB_ERROR_NONE;
}

tusb_error_t stub_hidh_open(uint8_t dev_addr, tusb_descriptor_interface_t const *descriptor, uint16_t *p_length, int num_call)
{
  *p_length = sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t);
  return TUSB_ERROR_NONE;
}

tusb_error_t stub_msch_open(uint8_t dev_addr, tusb_descriptor_interface_t const *descriptor, uint16_t *p_length, int num_call)
{
  *p_length = sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);
  return TUSB_ERROR_NONE;
}

tusb_error_t stub_cdch_open(uint8_t dev_addr, tusb_descriptor_interface_t const *descriptor, uint16_t *p_length, int num_call)
{
  *p_length =
      //------------- Comm Interface -------------//
      sizeof(tusb_descriptor_interface_t) + sizeof(cdc_desc_func_header_t) +
      sizeof(cdc_desc_func_abstract_control_management_t) + sizeof(cdc_desc_func_union_t) +
      sizeof(tusb_descriptor_endpoint_t) +
      //------------- Data Interface -------------//
      sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// enumeration
//--------------------------------------------------------------------+
void test_addr0_failed_dev_desc(void)
{
  osal_semaphore_wait_StubWithCallback(semaphore_wait_timeout_stub(0));

//  tusbh_device_mount_failed_cb_Expect(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL);

  usbh_enumeration_task(NULL);

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_ADDRESSED, usbh_devices[0].state);

}

void test_addr0_failed_set_address(void)
{
  osal_semaphore_wait_StubWithCallback(semaphore_wait_timeout_stub(1));
  hcd_port_reset_Expect( usbh_devices[0].core_id );
  osal_task_delay_Expect(RESET_DELAY);
//  tusbh_device_mount_failed_cb_Expect(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL);

  usbh_enumeration_task(NULL);

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_ADDRESSED, usbh_devices[0].state);
  TEST_ASSERT_EQUAL_MEMORY(&desc_device, enum_data_buffer, 8);
}

void test_enum_failed_get_full_dev_desc(void)
{
  osal_semaphore_wait_StubWithCallback(semaphore_wait_timeout_stub(2));
  hcd_port_reset_Expect( usbh_devices[0].core_id );
  osal_task_delay_Expect(RESET_DELAY);
  hcd_pipe_control_close_ExpectAndReturn(0, TUSB_ERROR_NONE);

  osal_semaphore_reset_Expect( usbh_devices[0].control.sem_hdl );
  osal_mutex_reset_Expect( usbh_devices[0].control.mutex_hdl );
  hcd_pipe_control_open_ExpectAndReturn(1, desc_device.bMaxPacketSize0, TUSB_ERROR_NONE);
//  tusbh_device_mount_failed_cb_Expect(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL);

  usbh_enumeration_task(NULL);

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_UNPLUG, usbh_devices[0].state);
  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_ADDRESSED, usbh_devices[1].state);
  TEST_ASSERT_EQUAL(TUSB_SPEED_FULL, usbh_devices[1].speed);
  TEST_ASSERT_EQUAL(enum_connect.core_id, usbh_devices[1].core_id);
  TEST_ASSERT_EQUAL(enum_connect.hub_addr, usbh_devices[1].hub_addr);
  TEST_ASSERT_EQUAL(enum_connect.hub_port, usbh_devices[1].hub_port);
}

void test_enum_failed_get_9byte_config_desc(void)
{
  osal_semaphore_wait_StubWithCallback(semaphore_wait_timeout_stub(3));
  hcd_port_reset_Expect( usbh_devices[0].core_id );
  osal_task_delay_Expect(RESET_DELAY);
  hcd_pipe_control_close_ExpectAndReturn(0, TUSB_ERROR_NONE);
  osal_semaphore_reset_Expect( usbh_devices[0].control.sem_hdl );
  osal_mutex_reset_Expect( usbh_devices[0].control.mutex_hdl );

  hcd_pipe_control_open_ExpectAndReturn(1, desc_device.bMaxPacketSize0, TUSB_ERROR_NONE);
  tusbh_device_attached_cb_ExpectAndReturn((tusb_descriptor_device_t*) enum_data_buffer, 1);
//  tusbh_device_mount_failed_cb_Expect(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL);

  usbh_enumeration_task(NULL);

  TEST_ASSERT_EQUAL(desc_device.idVendor, usbh_devices[1].vendor_id);
  TEST_ASSERT_EQUAL(desc_device.idProduct, usbh_devices[1].product_id);
  TEST_ASSERT_EQUAL(desc_device.bNumConfigurations, usbh_devices[1].configure_count);
}

void test_enum_failed_get_full_config_desc(void)
{
  osal_semaphore_wait_StubWithCallback(semaphore_wait_timeout_stub(4));
  hcd_port_reset_Expect( usbh_devices[0].core_id );
  osal_task_delay_Expect(RESET_DELAY);
  hcd_pipe_control_close_ExpectAndReturn(0, TUSB_ERROR_NONE);
  osal_semaphore_reset_Expect( usbh_devices[0].control.sem_hdl );
  osal_mutex_reset_Expect( usbh_devices[0].control.mutex_hdl );
  hcd_pipe_control_open_ExpectAndReturn(1, desc_device.bMaxPacketSize0, TUSB_ERROR_NONE);
  tusbh_device_attached_cb_ExpectAndReturn((tusb_descriptor_device_t*) enum_data_buffer, 1);
//  tusbh_device_mount_failed_cb_Expect(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL);

  usbh_enumeration_task(NULL);
}

void test_enum_parse_config_desc(void)
{
  osal_semaphore_wait_StubWithCallback(semaphore_wait_timeout_stub(5));
  hcd_port_reset_Expect( usbh_devices[0].core_id );
  osal_task_delay_Expect(RESET_DELAY);
  hcd_pipe_control_close_ExpectAndReturn(0, TUSB_ERROR_NONE);
  osal_semaphore_reset_Expect( usbh_devices[0].control.sem_hdl );
  osal_mutex_reset_Expect( usbh_devices[0].control.mutex_hdl );
  hcd_pipe_control_open_ExpectAndReturn(1, desc_device.bMaxPacketSize0, TUSB_ERROR_NONE);
  tusbh_device_attached_cb_ExpectAndReturn((tusb_descriptor_device_t*) enum_data_buffer, 1);

//  tusbh_device_mount_failed_cb_Expect(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL); // fail to set configure

  usbh_enumeration_task(NULL);

  TEST_ASSERT_EQUAL(desc_configuration.configuration.bNumInterfaces, usbh_devices[1].interface_count);
}

void test_enum_set_configure(void)
{
  osal_semaphore_wait_StubWithCallback(semaphore_wait_timeout_stub(6));
  hcd_port_reset_Expect( usbh_devices[0].core_id );
  osal_task_delay_Expect(RESET_DELAY);
  hcd_pipe_control_close_ExpectAndReturn(0, TUSB_ERROR_NONE);
  osal_semaphore_reset_Expect( usbh_devices[0].control.sem_hdl );
  osal_mutex_reset_Expect( usbh_devices[0].control.mutex_hdl );
  hcd_pipe_control_open_ExpectAndReturn(1, desc_device.bMaxPacketSize0, TUSB_ERROR_NONE);
  tusbh_device_attached_cb_ExpectAndReturn((tusb_descriptor_device_t*) enum_data_buffer, 1);

  // class open TODO  more class expect
  hidh_open_subtask_StubWithCallback(stub_hidh_open);
  msch_open_subtask_StubWithCallback(stub_msch_open);
  cdch_open_subtask_StubWithCallback(stub_cdch_open);

  tusbh_device_mount_succeed_cb_Expect(1);

  usbh_enumeration_task(NULL);

  TEST_ASSERT_EQUAL( BIT_(TUSB_CLASS_HID) | BIT_(TUSB_CLASS_MSC) | BIT_(TUSB_CLASS_CDC),
                    usbh_devices[1].flag_supported_class); // TODO change later
}
