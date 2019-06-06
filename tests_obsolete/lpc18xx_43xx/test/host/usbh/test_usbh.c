/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include <stdlib.h>
#include "unity.h"
#include "tusb_errors.h"
#include "type_helper.h"

#include "mock_osal.h"
#include "usbh.h"
#include "mock_hub.h"
#include "usbh_hcd.h"
#include "mock_hcd.h"

#include "mock_tusb_callback.h"
#include "mock_hid_host.h"
#include "mock_cdc_host.h"
#include "mock_msc_host.h"
#include "host_helper.h"

uint8_t dev_addr;
void setUp(void)
{
  dev_addr = RANDOM(CFG_TUSB_HOST_DEVICE_MAX)+1;
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// get_status
//--------------------------------------------------------------------+
void test_usbh_status_get_fail(void)
{
  _usbh_devices[dev_addr].state = 0;

  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATE_INVALID_PARAMETER, tusbh_device_get_state(CFG_TUSB_HOST_DEVICE_MAX+1) );
  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATE_UNPLUG, tusbh_device_get_state(dev_addr) );
}

void test_usbh_status_get_succeed(void)
{
  _usbh_devices[dev_addr].state = TUSB_DEVICE_STATE_CONFIGURED;
  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATE_CONFIGURED, tusbh_device_get_state(dev_addr) );
}

//--------------------------------------------------------------------+
// Init
//--------------------------------------------------------------------+
void test_usbh_init_hcd_failed(void)
{
  hcd_init_IgnoreAndReturn(TUSB_ERROR_HCD_FAILED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_HCD_FAILED, usbh_init());
}

void test_usbh_init_enum_queue_create_failed(void)
{
  hcd_init_ExpectAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn(NULL);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_QUEUE_FAILED, usbh_init());
}

void test_usbh_init_enum_task_create_failed(void)
{
  hcd_init_ExpectAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn((osal_queue_handle_t) 0x1234);
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_OSAL_TASK_FAILED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_TASK_FAILED, usbh_init());
}


void test_usbh_init_semaphore_create_failed(void)
{
  hcd_init_ExpectAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn((osal_queue_handle_t) 0x1234);
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_semaphore_create_IgnoreAndReturn(NULL);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_SEMAPHORE_FAILED, usbh_init());
}

void test_usbh_init_mutex_create_failed(void)
{
  hcd_init_ExpectAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn((osal_queue_handle_t) 0x1234);
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_semaphore_create_IgnoreAndReturn((osal_semaphore_handle_t) 0x1234);
  osal_mutex_create_IgnoreAndReturn(NULL);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_MUTEX_FAILED, usbh_init());
}

void test_usbh_init_ok(void)
{
  hcd_init_ExpectAndReturn(TUSB_ERROR_NONE);

  helper_usbh_init_expect();
  helper_class_init_expect();

  //------------- code under test -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, usbh_init());

  for (uint8_t i=0; i<CFG_TUSB_HOST_DEVICE_MAX+1; i++)
  {
    TEST_ASSERT_NOT_NULL(_usbh_devices[i].control.sem_hdl);
  }
}

#if 0 // TODO TEST enable this
// device is not mounted before, even the control pipe is not open, do nothing
void test_hcd_event_device_remove_device_not_previously_mounted(void)
{
  uint8_t dev_addr = 1;

  _usbh_devices[dev_addr].state    = TUSB_DEVICE_STATE_UNPLUG;
  _usbh_devices[dev_addr].core_id  = 0;
  _usbh_devices[dev_addr].hub_addr = 0;
  _usbh_devices[dev_addr].hub_port = 0;

  hcd_event_device_remove(0);
}

void test_hcd_event_device_remove(void)
{
  uint8_t dev_addr = 1;

  _usbh_devices[dev_addr].state                = TUSB_DEVICE_STATE_CONFIGURED;
  _usbh_devices[dev_addr].core_id              = 0;
  _usbh_devices[dev_addr].hub_addr             = 0;
  _usbh_devices[dev_addr].hub_port             = 0;
  _usbh_devices[dev_addr].flag_supported_class = TU_BIT(TUSB_CLASS_HID);

  hidh_close_Expect(dev_addr);
  hcd_pipe_control_close_ExpectAndReturn(dev_addr, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  hcd_event_device_remove(0);

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_REMOVING, _usbh_devices[dev_addr].state);
}

void test_usbh_device_unplugged_multple_class(void)
{
  uint8_t dev_addr = 1;

  _usbh_devices[dev_addr].state                = TUSB_DEVICE_STATE_CONFIGURED;
  _usbh_devices[dev_addr].core_id              = 0;
  _usbh_devices[dev_addr].hub_addr             = 0;
  _usbh_devices[dev_addr].hub_port             = 0;
  _usbh_devices[dev_addr].flag_supported_class = TU_BIT(TUSB_CLASS_HID) | TU_BIT(TUSB_CLASS_MSC) | TU_BIT(TUSB_CLASS_CDC);

  cdch_close_Expect(dev_addr);
  hidh_close_Expect(dev_addr);
  msch_close_Expect(dev_addr);

  hcd_pipe_control_close_ExpectAndReturn(dev_addr, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  hcd_event_device_remove(0);

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_REMOVING, _usbh_devices[dev_addr].state);

}
#endif

void semaphore_wait_success_stub(osal_mutex_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  (*p_error) = TUSB_ERROR_NONE;
}

static void mutex_wait_failed_stub(osal_mutex_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error, int num_call)
{
  (*p_error) = TUSB_ERROR_OSAL_TIMEOUT;
}

void test_usbh_control_xfer_mutex_failed(void)
{
  tusb_control_request_t a_request =
  {
      .bmRequestType = 1,
      .bRequest = 2,
      .wValue = 3,
      .wIndex = 4,
      .wLength = 0
  };

  osal_mutex_wait_StubWithCallback(mutex_wait_failed_stub);
  osal_mutex_release_ExpectAndReturn(_usbh_devices[dev_addr].control.mutex_hdl, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  usbh_control_xfer(dev_addr, 1, 2, 3, 4, 0, NULL);
}

void test_usbh_control_xfer_ok(void)
{
  tusb_control_request_t a_request =
  {
      .bmRequestType = 1,
      .bRequest = 2,
      .wValue = 3,
      .wIndex = 4,
      .wLength = 0
  };

  osal_mutex_wait_StubWithCallback(semaphore_wait_success_stub);

  hcd_pipe_control_xfer_ExpectAndReturn(dev_addr, &a_request, NULL, TUSB_ERROR_NONE);
  osal_semaphore_wait_StubWithCallback(semaphore_wait_success_stub);

  osal_mutex_release_ExpectAndReturn(_usbh_devices[dev_addr].control.mutex_hdl, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  usbh_control_xfer(dev_addr, 1, 2, 3, 4, 0, NULL);
}

//void test_hcd_event_xfer_complete_non_control_stalled(void) // do nothing for stall on control
//{
//
//}
