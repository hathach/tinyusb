/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// tu_edpt_validate() bulk wMaxPacketSize rules. The check only compiles with CFG_TUSB_DEBUG on, which
// test/support/tusb_config.h defaults to 1: this test must not override it in project.yml.

#include "unity.h"

// Files to test
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "common/tusb_private.h"
#include "usbd.h"
TEST_SOURCE_FILE("tusb.c")

// tusb.c links the device stack (and its default MSC class) against these
#include "mock_dcd.h"
#include "mock_msc_device.h"

#if !CFG_TUSB_DEBUG
  #error "tu_edpt_validate() only checks wMaxPacketSize with CFG_TUSB_DEBUG enabled"
#endif

uint32_t tusb_time_millis_api(void) {
  return 0;
}

uint8_t const* tud_descriptor_device_cb(void) {
  return NULL;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return NULL;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) index;
  (void) langid;
  return NULL;
}

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
static tusb_desc_endpoint_t desc_ep;

static void set_endpoint(uint8_t ep_addr, uint8_t xfer, uint16_t max_packet_size) {
  desc_ep.bLength                = sizeof(tusb_desc_endpoint_t);
  desc_ep.bDescriptorType        = TUSB_DESC_ENDPOINT;
  desc_ep.bEndpointAddress       = ep_addr;
  desc_ep.bmAttributes.xfer      = xfer;
  desc_ep.wMaxPacketSize         = tu_htole16(max_packet_size);
  desc_ep.bInterval              = 0;
}

void setUp(void) {
  memset(&desc_ep, 0, sizeof(desc_ep));
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Bulk fullspeed
//--------------------------------------------------------------------+

// Non-compliant USB-MIDI devices report 4 (one event packet): accepted, and the reported size is kept
void test_bulk_fullspeed_size4_accepted_unchanged(void) {
  set_endpoint(0x81, TUSB_XFER_BULK, 4);
  TEST_ASSERT_TRUE(tu_edpt_validate(&desc_ep, TUSB_SPEED_FULL));
  TEST_ASSERT_EQUAL_UINT16(4, tu_edpt_packet_size(&desc_ep));

  set_endpoint(0x02, TUSB_XFER_BULK, 4);
  TEST_ASSERT_TRUE(tu_edpt_validate(&desc_ep, TUSB_SPEED_FULL));
  TEST_ASSERT_EQUAL_UINT16(4, tu_edpt_packet_size(&desc_ep));
}

void test_bulk_fullspeed_spec_sizes_accepted(void) {
  const uint16_t sizes[] = {8, 16, 32, 64};
  for (size_t i = 0; i < TU_ARRAY_SIZE(sizes); i++) {
    set_endpoint(0x81, TUSB_XFER_BULK, sizes[i]);
    TEST_ASSERT_TRUE_MESSAGE(tu_edpt_validate(&desc_ep, TUSB_SPEED_FULL), "spec fullspeed bulk size rejected");
    TEST_ASSERT_EQUAL_UINT16(sizes[i], tu_edpt_packet_size(&desc_ep));
  }
}

void test_bulk_fullspeed_other_sizes_rejected(void) {
  const uint16_t sizes[] = {0, 1, 2, 3, 5, 12, 63, 128, 512};
  for (size_t i = 0; i < TU_ARRAY_SIZE(sizes); i++) {
    set_endpoint(0x81, TUSB_XFER_BULK, sizes[i]);
    TEST_ASSERT_FALSE_MESSAGE(tu_edpt_validate(&desc_ep, TUSB_SPEED_FULL), "non-spec fullspeed bulk size accepted");
  }
}

//--------------------------------------------------------------------+
// Bulk highspeed: exactly 512, the size 4 exception is fullspeed only
//--------------------------------------------------------------------+

void test_bulk_highspeed_512_accepted(void) {
  set_endpoint(0x81, TUSB_XFER_BULK, 512);
  TEST_ASSERT_TRUE(tu_edpt_validate(&desc_ep, TUSB_SPEED_HIGH));
}

void test_bulk_highspeed_size4_rejected(void) {
  set_endpoint(0x81, TUSB_XFER_BULK, 4);
  TEST_ASSERT_FALSE(tu_edpt_validate(&desc_ep, TUSB_SPEED_HIGH));
}

void test_bulk_highspeed_other_sizes_rejected(void) {
  const uint16_t sizes[] = {8, 64, 256, 1024};
  for (size_t i = 0; i < TU_ARRAY_SIZE(sizes); i++) {
    set_endpoint(0x81, TUSB_XFER_BULK, sizes[i]);
    TEST_ASSERT_FALSE_MESSAGE(tu_edpt_validate(&desc_ep, TUSB_SPEED_HIGH), "non-512 highspeed bulk size accepted");
  }
}
