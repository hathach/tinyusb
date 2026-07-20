/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
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

// Second build of usbd.c in the default full/high-speed shape (EP0 = 64) - what every shipping
// port compiles. test_usbd.c builds the SuperSpeed shape (EP0 = 512) instead, which selects the
// other side of every `#if TUD_OPT_SUPER_SPEED` / `#if CFG_TUD_ENDPOINT0_SIZE > 64`, so without
// this suite the FS/HS branches are never compiled at all. project.yml defines
// TUD_TEST_NO_SUPER_SPEED for this test executable only (:defines: per-test matcher).

#include "unity.h"

// Files to test
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd.c")

// Mock File
#include "mock_dcd.h"
#include "mock_msc_device.h"

#if TUD_OPT_SUPER_SPEED
#error "test_usbd_fshs must build the non-SuperSpeed shape: TUD_TEST_NO_SUPER_SPEED is missing (project.yml :defines:)"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

uint32_t tusb_time_millis_api(void) {
  return 0;
}

enum
{
  EDPT_CTRL_OUT = 0x00,
  EDPT_CTRL_IN  = 0x80
};

uint8_t const rhport = 0;

tusb_control_request_t const req_get_desc_configuration =
{
  .bmRequestType = 0x80,
  .bRequest = TUSB_REQ_GET_DESCRIPTOR,
  .wValue = (TUSB_DESC_CONFIGURATION << 8),
  .wIndex = 0x0000,
  .wLength = 256
};

uint8_t const* desc_configuration;

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
uint8_t const * tud_descriptor_device_cb(void) {
  return NULL;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) index; (void) langid;
  return NULL;
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();

  if ( !tud_inited() ) {
    tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
    };

    mscd_init_Expect();
    dcd_init_ExpectAndReturn(0, &dev_init, true);

    tusb_init(0, &dev_init);
  }
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Control ZLP
//--------------------------------------------------------------------+

// EP0 data stage is chunked at CFG_TUD_ENDPOINT0_SIZE (ep0_xact_limit's non-SuperSpeed branch):
// 128 byte total with wLength 256 is exactly two full transactions, then a ZLP.
void test_usbd_control_in_zlp(void)
{
  uint8_t zlp_desc_configuration[CFG_TUD_ENDPOINT0_SIZE*2] =
  {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 0, 0, CFG_TUD_ENDPOINT0_SIZE*2, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  };
  desc_configuration = zlp_desc_configuration;

  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_configuration, false);

  // 1st transaction
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN,
                                         zlp_desc_configuration, CFG_TUD_ENDPOINT0_SIZE, CFG_TUD_ENDPOINT0_SIZE, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, CFG_TUD_ENDPOINT0_SIZE, 0, false);

  // 2nd transaction
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN,
                                         zlp_desc_configuration + CFG_TUD_ENDPOINT0_SIZE, CFG_TUD_ENDPOINT0_SIZE, CFG_TUD_ENDPOINT0_SIZE, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, CFG_TUD_ENDPOINT0_SIZE, 0, false);

  // Expect Zero length Packet
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);

  // Status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_desc_configuration, 1);

  tud_task();
}

//--------------------------------------------------------------------+
// Endpoint pair
//--------------------------------------------------------------------+

// Without SuperSpeed there are no endpoint companion descriptors: usbd_skip_ss_ep_companion()
// must return the descriptor it was handed, so consecutive endpoint descriptors open as a pair.
void test_usbd_open_edpt_pair_no_companion(void)
{
  uint8_t const desc_ep_pair[] = {
    // EP Out (bulk 64), EP In (bulk 64)
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
    7, TUSB_DESC_ENDPOINT, 0x82, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
  };

  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) &desc_ep_pair[0], true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) &desc_ep_pair[7], true);

  uint8_t ep_out = 0, ep_in = 0;
  TEST_ASSERT_TRUE(usbd_open_edpt_pair(rhport, desc_ep_pair, desc_ep_pair + sizeof(desc_ep_pair), 2, TUSB_XFER_BULK, &ep_out, &ep_in));
  TEST_ASSERT_EQUAL_HEX8(0x02, ep_out);
  TEST_ASSERT_EQUAL_HEX8(0x82, ep_in);
}
