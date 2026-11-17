/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// tu_edpt_validate() wMaxPacketSize rules, the same in every build: zero or above the spec maximum is rejected (endpoint
// buffers are sized to it), other non-compliant sizes are accepted with a warning and kept as reported.

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

static bool validate(uint8_t xfer, uint16_t max_packet_size, tusb_speed_t speed) {
  set_endpoint(0x81, xfer, max_packet_size);
  return tu_edpt_validate(&desc_ep, speed);
}

void test_zero_rejected(void) {
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 0, TUSB_SPEED_FULL));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 0, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_INTERRUPT, 0, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_ISOCHRONOUS, 0, TUSB_SPEED_HIGH));
}

void test_control_rejected(void) {
  TEST_ASSERT_FALSE(validate(TUSB_XFER_CONTROL, 64, TUSB_SPEED_FULL));
}

//--------------------------------------------------------------------+
// Bulk fullspeed
//--------------------------------------------------------------------+

void test_bulk_fullspeed_spec_sizes_accepted(void) {
  const uint16_t sizes[] = {8, 16, 32, 64};
  for (size_t i = 0; i < TU_ARRAY_SIZE(sizes); i++) {
    TEST_ASSERT_TRUE_MESSAGE(validate(TUSB_XFER_BULK, sizes[i], TUSB_SPEED_FULL), "spec fullspeed bulk size rejected");
    TEST_ASSERT_EQUAL_UINT16(sizes[i], tu_edpt_packet_size(&desc_ep));
  }
}

// Non-compliant sizes within the bound (e.g. USB-MIDI bulk 4, one event packet) are accepted and kept as reported
void test_bulk_fullspeed_noncompliant_accepted_unchanged(void) {
  const uint16_t sizes[] = {1, 2, 3, 4, 5, 12, 63};
  for (size_t i = 0; i < TU_ARRAY_SIZE(sizes); i++) {
    TEST_ASSERT_TRUE_MESSAGE(validate(TUSB_XFER_BULK, sizes[i], TUSB_SPEED_FULL), "fullspeed bulk size rejected");
    TEST_ASSERT_EQUAL_UINT16(sizes[i], tu_edpt_packet_size(&desc_ep));
  }

  set_endpoint(0x02, TUSB_XFER_BULK, 4);
  TEST_ASSERT_TRUE(tu_edpt_validate(&desc_ep, TUSB_SPEED_FULL));
  TEST_ASSERT_EQUAL_UINT16(4, tu_edpt_packet_size(&desc_ep));
}

void test_bulk_fullspeed_over_bound_rejected(void) {
  const uint16_t sizes[] = {65, 128, 512};
  for (size_t i = 0; i < TU_ARRAY_SIZE(sizes); i++) {
    TEST_ASSERT_FALSE_MESSAGE(validate(TUSB_XFER_BULK, sizes[i], TUSB_SPEED_FULL), "fullspeed bulk over 64 accepted");
  }
}

//--------------------------------------------------------------------+
// Bulk highspeed
//--------------------------------------------------------------------+

void test_bulk_highspeed_512_accepted(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_BULK, 512, TUSB_SPEED_HIGH));
}

void test_bulk_highspeed_noncompliant_accepted_unchanged(void) {
  const uint16_t sizes[] = {4, 8, 64, 256};
  for (size_t i = 0; i < TU_ARRAY_SIZE(sizes); i++) {
    TEST_ASSERT_TRUE_MESSAGE(validate(TUSB_XFER_BULK, sizes[i], TUSB_SPEED_HIGH), "highspeed bulk size rejected");
    TEST_ASSERT_EQUAL_UINT16(sizes[i], tu_edpt_packet_size(&desc_ep));
  }
}

void test_bulk_highspeed_over_bound_rejected(void) {
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 513, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 1024, TUSB_SPEED_HIGH));
}

//--------------------------------------------------------------------+
// Interrupt and isochronous
//--------------------------------------------------------------------+

void test_interrupt_bounded(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, 64, TUSB_SPEED_FULL));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_INTERRUPT, 65, TUSB_SPEED_FULL));

  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, 1024, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_INTERRUPT, 1025, TUSB_SPEED_HIGH));
}

// lowspeed allows 8, a larger size up to the fullspeed bound is only non-compliant: accepted and kept as reported
void test_interrupt_lowspeed_over_8_accepted_unchanged(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, 8, TUSB_SPEED_LOW));
  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, 9, TUSB_SPEED_LOW));
  TEST_ASSERT_EQUAL_UINT16(9, tu_edpt_packet_size(&desc_ep));
  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, 64, TUSB_SPEED_LOW));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_INTERRUPT, 65, TUSB_SPEED_LOW));
}

void test_isochronous_bounded(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_ISOCHRONOUS, 1023, TUSB_SPEED_FULL));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_ISOCHRONOUS, 1024, TUSB_SPEED_FULL));

  TEST_ASSERT_TRUE(validate(TUSB_XFER_ISOCHRONOUS, 1024, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_ISOCHRONOUS, 1025, TUSB_SPEED_HIGH));
}

// highspeed high-bandwidth: bits 12..11 are additional transactions, the bound applies to bits 10..0
void test_high_bandwidth_bits_not_counted(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, (uint16_t) ((2u << 11) | 1024u), TUSB_SPEED_HIGH));
  TEST_ASSERT_TRUE(validate(TUSB_XFER_ISOCHRONOUS, (uint16_t) ((2u << 11) | 1024u), TUSB_SPEED_HIGH));
}
