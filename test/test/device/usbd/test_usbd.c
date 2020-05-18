/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Ha Thach (tinyusb.org)
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

// Mock File
#include "mock_dcd.h"
#include "mock_msc_device.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

enum
{
  EDPT_CTRL_OUT = 0x00,
  EDPT_CTRL_IN  = 0x80
};

uint8_t const rhport = 0;

tusb_desc_device_t const data_desc_device =
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

uint8_t const data_desc_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 0, 0, TUD_CONFIG_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
};

tusb_control_request_t const req_get_desc_device =
{
  .bmRequestType = 0x80,
  .bRequest = TUSB_REQ_GET_DESCRIPTOR,
  .wValue = (TUSB_DESC_DEVICE << 8),
  .wIndex = 0x0000,
  .wLength = 64
};

tusb_control_request_t const req_get_desc_configuration =
{
  .bmRequestType = 0x80,
  .bRequest = TUSB_REQ_GET_DESCRIPTOR,
  .wValue = (TUSB_DESC_CONFIGURATION << 8),
  .wIndex = 0x0000,
  .wLength = 256
};

uint8_t const* desc_device;
uint8_t const* desc_configuration;

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
uint8_t const * tud_descriptor_device_cb(void)
{
  return desc_device;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  return desc_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  return NULL;
}

void setUp(void)
{
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();

  if ( !tusb_inited() )
  {
    mscd_init_Expect();
    dcd_init_Expect(rhport);
    dcd_connect_Expect(rhport);
    tusb_init();
  }
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// Get Descriptor
//--------------------------------------------------------------------+

//------------- Device -------------//
void test_usbd_get_device_descriptor(void)
{
  desc_device = (uint8_t const *) &data_desc_device;
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_device, false);

  // data
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, 0x80, (uint8_t*)&data_desc_device, sizeof(tusb_desc_device_t), sizeof(tusb_desc_device_t), true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, sizeof(tusb_desc_device_t), 0, false);

  // status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_desc_device, 1);

  tud_task();
}

void test_usbd_get_device_descriptor_null(void)
{
  desc_device = NULL;

  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_device, false);

  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_OUT);
  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_IN);

  tud_task();
}

//------------- Configuration -------------//

void test_usbd_get_configuration_descriptor(void)
{
  desc_configuration = data_desc_configuration;
  uint16_t total_len = ((tusb_desc_configuration_t const*) data_desc_configuration)->wTotalLength;

  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_configuration, false);

  // data
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, 0x80, (uint8_t*) data_desc_configuration, total_len, total_len, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, total_len, 0, false);

  // status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_desc_configuration, 1);

  tud_task();
}

void test_usbd_get_configuration_descriptor_null(void)
{
  desc_configuration = NULL;
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_configuration, false);

  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_OUT);
  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_IN);

  tud_task();
}

//--------------------------------------------------------------------+
// Control ZLP
//--------------------------------------------------------------------+

void test_usbd_control_in_zlp(void)
{
  // 128 byte total len, with EP0 size = 64, and request length = 256
  // ZLP must be return
  uint8_t zlp_desc_configuration[CFG_TUD_ENDOINT0_SIZE*2] =
  {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 0, 0, CFG_TUD_ENDOINT0_SIZE*2, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  };

  desc_configuration = zlp_desc_configuration;

  // request, then 1st, 2nd xact + ZLP + status
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_configuration, false);

  // 1st transaction
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN,
                                         zlp_desc_configuration, CFG_TUD_ENDOINT0_SIZE, CFG_TUD_ENDOINT0_SIZE, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, CFG_TUD_ENDOINT0_SIZE, 0, false);

  // 2nd transaction
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN,
                                         zlp_desc_configuration + CFG_TUD_ENDOINT0_SIZE, CFG_TUD_ENDOINT0_SIZE, CFG_TUD_ENDOINT0_SIZE, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, CFG_TUD_ENDOINT0_SIZE, 0, false);

  // Expect Zero length Packet
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);

  // Status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_desc_configuration, 1);

  tud_task();
}
