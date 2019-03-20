/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

tusb_error_t stub_hidd_init(uint8_t coreid, tusb_desc_interface_t const* p_interface_desc, uint16_t* p_length, int num_call)
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
#if CFG_TUD_HID
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
  uint32_t const manufacturer_len = sizeof(CFG_TUD_STRING_MANUFACTURER) - 1;
  TEST_ASSERT_EQUAL(manufacturer_len*2 + 2, app_tusb_desc_strings.manufacturer.bLength);
  for(uint32_t i=0; i<manufacturer_len; i++)
  {
    TEST_ASSERT_EQUAL(CFG_TUD_STRING_MANUFACTURER[i], app_tusb_desc_strings.manufacturer.unicode_string[i]);
  }

  //------------- product string descriptor -------------//
  uint32_t const product_len = sizeof(CFG_TUD_STRING_PRODUCT) - 1;
  TEST_ASSERT_EQUAL(product_len*2 + 2, app_tusb_desc_strings.product.bLength);
  for(uint32_t i=0; i < product_len; i++)
  {
    TEST_ASSERT_EQUAL(CFG_TUD_STRING_PRODUCT[i], app_tusb_desc_strings.product.unicode_string[i]);
  }

  //------------- serial string descriptor -------------//
  uint32_t const serial_len = sizeof(CFG_TUD_STRING_SERIAL) - 1;
  TEST_ASSERT_EQUAL(serial_len*2 + 2, app_tusb_desc_strings.serial.bLength);
  for(uint32_t i=0; i<serial_len; i++)
  {
    TEST_ASSERT_EQUAL(CFG_TUD_STRING_SERIAL[i], app_tusb_desc_strings.serial.unicode_string[i]);
  }
}
