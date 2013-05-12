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
#include "errors.h"
#include "type_helper.h"

#include "mock_osal.h"
#include "usbh.h"
#include "usbh_hcd.h"
#include "mock_hcd.h"
#include "usbh_hcd.h"

#include "mock_tusb_callback.h"
#include "mock_hid_host.h"

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

void test_usbh_init_enum_task_create_failed(void)
{
  hcd_init_ExpectAndReturn(TUSB_ERROR_NONE);
  osal_semaphore_create_IgnoreAndReturn( (osal_semaphore_handle_t) 0x1234);
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_OSAL_TASK_FAILED);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_TASK_FAILED, usbh_init());
}

void test_usbh_init_enum_queue_create_failed(void)
{
  hcd_init_ExpectAndReturn(TUSB_ERROR_NONE);
  osal_semaphore_create_IgnoreAndReturn( (osal_semaphore_handle_t) 0x1234);
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn(NULL);
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_QUEUE_FAILED, usbh_init());
}

void class_init_expect(void)
{
  hidh_init_Expect();

  //TODO update more classes
}

void test_usbh_init_ok(void)
{
  hcd_init_ExpectAndReturn(TUSB_ERROR_NONE);

  osal_semaphore_create_IgnoreAndReturn( (osal_semaphore_handle_t) 0x1234);
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);
  osal_queue_create_IgnoreAndReturn( (osal_queue_handle_t) 0x4566 );

  class_init_expect();

  //------------- code under test -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_NONE, usbh_init());

  for (uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX+1; i++)
  {
    TEST_ASSERT_NOT_NULL(usbh_devices[i].control.sem_hdl);
  }
}

void class_close_expect(void)
{
  hidh_close_Expect(1);
}

// device is not mounted before, even the control pipe is not open, do nothing
void test_usbh_device_unplugged_isr_device_not_previously_mounted(void)
{
  uint8_t dev_addr = 1;

  usbh_devices[dev_addr].state    = TUSB_DEVICE_STATE_UNPLUG;
  usbh_devices[dev_addr].core_id  = 0;
  usbh_devices[dev_addr].hub_addr = 0;
  usbh_devices[dev_addr].hub_port = 0;

  usbh_device_unplugged_isr(0);
}

void test_usbh_device_unplugged_isr(void)
{
  uint8_t dev_addr = 1;

  usbh_devices[dev_addr].state                = TUSB_DEVICE_STATE_CONFIGURED;
  usbh_devices[dev_addr].core_id              = 0;
  usbh_devices[dev_addr].hub_addr             = 0;
  usbh_devices[dev_addr].hub_port             = 0;
  usbh_devices[dev_addr].flag_supported_class = TUSB_CLASS_FLAG_HID;

  class_close_expect();
  hcd_pipe_control_close_ExpectAndReturn(dev_addr, TUSB_ERROR_NONE);

  //------------- Code Under Test -------------//
  usbh_device_unplugged_isr(0);

  TEST_ASSERT_EQUAL(TUSB_DEVICE_STATE_REMOVING, usbh_devices[dev_addr].state);
}
