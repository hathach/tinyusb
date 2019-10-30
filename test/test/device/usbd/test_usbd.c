/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 hathach for Adafruit Industries
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
 */

#include "unity.h"

// Files to test
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
TEST_FILE("usbd_control.c")
//TEST_FILE("usb_descriptors.c")

// Mock File
#include "mock_dcd.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

uint8_t const rhport = 0;

tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = 0xCafe,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

tusb_control_request_t const req_get_desc_device =
{
  .bmRequestType = 0x80,
  .bRequest = TUSB_REQ_GET_DESCRIPTOR,
  .wValue = (TUSB_DESC_DEVICE << 8),
  .wIndex = 0x0000,
  .wLength = 64
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
uint8_t const * ptr_desc_device;

uint8_t const * tud_descriptor_device_cb(void)
{
  return ptr_desc_device;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  TEST_FAIL();
  return NULL;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index)
{
  return NULL;
}

void setUp(void)
{
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();

  if ( !tusb_inited() )
  {
    dcd_init_Expect(rhport);
    tusb_init();
  }

  ptr_desc_device = (uint8_t const *) &desc_device;
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
void test_usbd_get_device_descriptor(void)
{
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_device, false);

  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, 0x80, (uint8_t*)&desc_device, sizeof(tusb_desc_device_t), sizeof(tusb_desc_device_t), true);

  tud_task();
}

void test_usbd_get_device_descriptor_null(void)
{
  ptr_desc_device = NULL;

  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_device, false);

  dcd_edpt_stall_Expect(rhport, 0);
  dcd_edpt_stall_Expect(rhport, 0x80);

  tud_task();
}
