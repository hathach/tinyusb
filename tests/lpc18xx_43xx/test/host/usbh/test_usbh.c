/**************************************************************************/
/*!
    @file     test_usbd_host.c
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
  dev_addr = RANDOM(TUSB_CFG_HOST_DEVICE_MAX)+1;
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// get_status
//--------------------------------------------------------------------+
void test_usbh_status_get_fail(void)
{
  usbh_devices[dev_addr].state = 0;

  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATE_INVALID_PARAMETER, tusbh_device_get_state(TUSB_CFG_HOST_DEVICE_MAX+1) );
  TEST_ASSERT_EQUAL( TUSB_DEVICE_STATE_UNPLUG, tusbh_device_get_state(dev_addr) );
}

void test_usbh_status_get_succeed(void)
{
  usbh_devices[dev_addr].state = TUSB_DEVICE_STATE_CONFIGURED;
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

  for (uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX+1; i++)
  {
    TEST_ASSERT_NOT_NULL(usbh_devices[i].control.sem_hdl);
  }
}

#if 0 // TODO TEST enable this
// device is not mounted before, even the control pipe is not open, do nothing
void test_usbh_hcd_rhport_unplugged_isr_device_not_previously_mounted(void)
{
  uint8_t dev_addr = 1;

  usbh_devices[dev_addr].state    = TUSB_DEVICE_STATE_UNPLUG;
  usbh_devices[dev_addr].core_id  = 0;
  usbh_devices[dev_addr].hub_addr = 0;
  usbh_devices[dev_addr].hub_port = 0;

  usbh_hcd_rhport_unplugged_isr(0);
}

void test_usbh_hcd_rhport_unplugged_isr(void)
{
  uint8_t dev_addr = 1;

  usbh_devices[dev_addr].state                = TUSB_DEVICE_STATE_CONFIGURED;
  usbh_devices[dev_addr].core_id              = 0;
  usbh_devices[dev_addr].hub_addr             = 0;
  usbh_devices[dev_addr].hub_port             = 0;
  usbh_devices[dev_addr].flag_supported_class = BIT_(TUSB_CLASS_HID);

  hidh_close_Expect(dev_addr);
  hcd_pipe_control_close_ExpectAndReturn(dev_addr, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  usbh_hcd_rhport_unplugged_isr(0);

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_REMOVING, usbh_devices[dev_addr].state);
}

void test_usbh_device_unplugged_multple_class(void)
{
  uint8_t dev_addr = 1;

  usbh_devices[dev_addr].state                = TUSB_DEVICE_STATE_CONFIGURED;
  usbh_devices[dev_addr].core_id              = 0;
  usbh_devices[dev_addr].hub_addr             = 0;
  usbh_devices[dev_addr].hub_port             = 0;
  usbh_devices[dev_addr].flag_supported_class = BIT_(TUSB_CLASS_HID) | BIT_(TUSB_CLASS_MSC) | BIT_(TUSB_CLASS_CDC);

  cdch_close_Expect(dev_addr);
  hidh_close_Expect(dev_addr);
  msch_close_Expect(dev_addr);

  hcd_pipe_control_close_ExpectAndReturn(dev_addr, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  usbh_hcd_rhport_unplugged_isr(0);

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_REMOVING, usbh_devices[dev_addr].state);

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
  osal_mutex_release_ExpectAndReturn(usbh_devices[dev_addr].control.mutex_hdl, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  usbh_control_xfer_subtask(dev_addr, 1, 2, 3, 4, 0, NULL);
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

  osal_mutex_release_ExpectAndReturn(usbh_devices[dev_addr].control.mutex_hdl, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  usbh_control_xfer_subtask(dev_addr, 1, 2, 3, 4, 0, NULL);
}

//void test_usbh_xfer_isr_non_control_stalled(void) // do nothing for stall on control
//{
//
//}
