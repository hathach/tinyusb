/**************************************************************************/
/*!
    @file     test_usbd.c
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

#include <stdlib.h>
#include "unity.h"
#include "tusb_errors.h"
#include "type_helper.h"

#include "tusb_descriptors.h"

#include "mock_hid_device.h"
#include "mock_dcd.h"

#include "usbd.h"

void setUp(void)
{

}

void tearDown(void)
{
}

void test_dcd_init_failed(void)
{
  dcd_init_ExpectAndReturn(TUSB_ERROR_FAILED);

  //------------- Code Under Test -------------//
  TEST_ASSERT_EQUAL(TUSB_ERROR_FAILED, usbd_init() );
}

tusb_error_t stub_hidd_init(uint8_t coreid, tusb_descriptor_interface_t const* p_interface_desc, uint16_t* p_length, int num_call)
{
  switch(num_call)
  {
    case 0:
      TEST_ASSERT_EQUAL_HEX(&app_tusb_desc_configuration.keyboard_interface, p_interface_desc);
    break;

    case 1:
      TEST_ASSERT_EQUAL_HEX32(&app_tusb_desc_configuration.mouse_interface, p_interface_desc);
    break;

    default:
      TEST_FAIL();
      return TUSB_ERROR_FAILED;
  }

  return TUSB_ERROR_NONE;
}

void class_init_epxect(void)
{
#if DEVICE_CLASS_HID
  hidd_init_StubWithCallback(stub_hidd_init);
#endif
}

void test_usbd_init(void)
{
  dcd_init_ExpectAndReturn(TUSB_ERROR_NONE);

  class_init_epxect();
  dcd_controller_connect_Expect(0);


  //------------- Code Under Test -------------//
  TEST_ASSERT_STATUS( usbd_init() );
}


void test_usbd_init_ok(void)
{
  TEST_IGNORE_MESSAGE("pause device stack");
  dcd_init_ExpectAndReturn(TUSB_ERROR_NONE);

  hidd_init_StubWithCallback(stub_hidd_init);

  dcd_controller_connect_Expect(0);

  //------------- Code Under Test -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, usbd_init() );

}

void test_usbd_string_descriptor(void)
{
  dcd_init_IgnoreAndReturn(TUSB_ERROR_FAILED);

  //------------- Code Under Test -------------//
  TEST_ASSERT_EQUAL( TUSB_ERROR_FAILED, usbd_init() );


  //------------- manufacturer string descriptor -------------//
  uint32_t const manufacturer_len = sizeof(TUSB_CFG_DEVICE_STRING_MANUFACTURER) - 1;
  TEST_ASSERT_EQUAL(manufacturer_len*2 + 2, app_tusb_desc_strings.manufacturer.bLength);
  for(uint32_t i=0; i<manufacturer_len; i++)
  {
    TEST_ASSERT_EQUAL(TUSB_CFG_DEVICE_STRING_MANUFACTURER[i], app_tusb_desc_strings.manufacturer.unicode_string[i]);
  }

  //------------- product string descriptor -------------//
  uint32_t const product_len = sizeof(TUSB_CFG_DEVICE_STRING_PRODUCT) - 1;
  TEST_ASSERT_EQUAL(product_len*2 + 2, app_tusb_desc_strings.product.bLength);
  for(uint32_t i=0; i < product_len; i++)
  {
    TEST_ASSERT_EQUAL(TUSB_CFG_DEVICE_STRING_PRODUCT[i], app_tusb_desc_strings.product.unicode_string[i]);
  }

  //------------- serial string descriptor -------------//
  uint32_t const serial_len = sizeof(TUSB_CFG_DEVICE_STRING_SERIAL) - 1;
  TEST_ASSERT_EQUAL(serial_len*2 + 2, app_tusb_desc_strings.serial.bLength);
  for(uint32_t i=0; i<serial_len; i++)
  {
    TEST_ASSERT_EQUAL(TUSB_CFG_DEVICE_STRING_SERIAL[i], app_tusb_desc_strings.serial.unicode_string[i]);
  }
}
