/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// tu_edpt_validate() with CFG_TUSB_DEBUG=0 (set in project.yml): the strict compliance checks are debug only,
// but wMaxPacketSize is still bounded by the spec maximum, which endpoint buffers are sized to.

#include "unity.h"

#include "tusb_common.h"
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "common/tusb_private.h"

#if CFG_TUSB_DEBUG
  #error "this test covers the release build of tu_edpt_validate()"
#endif

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
static tusb_desc_endpoint_t desc_ep;

static bool validate(uint8_t xfer, uint16_t max_packet_size, tusb_speed_t speed) {
  desc_ep.bLength           = sizeof(tusb_desc_endpoint_t);
  desc_ep.bDescriptorType   = TUSB_DESC_ENDPOINT;
  desc_ep.bEndpointAddress  = 0x81;
  desc_ep.bmAttributes.xfer = xfer;
  desc_ep.wMaxPacketSize    = tu_htole16(max_packet_size);
  desc_ep.bInterval         = 1;
  return tu_edpt_validate(&desc_ep, speed);
}

void setUp(void) {
  memset(&desc_ep, 0, sizeof(desc_ep));
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+

void test_zero_rejected(void) {
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 0, TUSB_SPEED_FULL));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_INTERRUPT, 0, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_ISOCHRONOUS, 0, TUSB_SPEED_HIGH));
}

void test_bulk_bounded(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_BULK, 64, TUSB_SPEED_FULL));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 65, TUSB_SPEED_FULL));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 512, TUSB_SPEED_FULL));

  TEST_ASSERT_TRUE(validate(TUSB_XFER_BULK, 512, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 513, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_BULK, 1024, TUSB_SPEED_HIGH));
}

// non-compliant sizes below the bound stay accepted, only the debug build checks them
void test_bulk_noncompliant_below_bound_accepted(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_BULK, 12, TUSB_SPEED_FULL));
  TEST_ASSERT_TRUE(validate(TUSB_XFER_BULK, 64, TUSB_SPEED_HIGH));
}

void test_interrupt_bounded(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, 64, TUSB_SPEED_FULL));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_INTERRUPT, 65, TUSB_SPEED_FULL));

  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, 1024, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_INTERRUPT, 1025, TUSB_SPEED_HIGH));
}

void test_isochronous_bounded(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_ISOCHRONOUS, 1023, TUSB_SPEED_FULL));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_ISOCHRONOUS, 1024, TUSB_SPEED_FULL));

  TEST_ASSERT_TRUE(validate(TUSB_XFER_ISOCHRONOUS, 1024, TUSB_SPEED_HIGH));
  TEST_ASSERT_FALSE(validate(TUSB_XFER_ISOCHRONOUS, 1025, TUSB_SPEED_HIGH));
}

// highspeed high-bandwidth: bits 12..11 are additional transactions, the bound applies to bits 10..0
void test_high_bandwidth_bits_not_counted(void) {
  TEST_ASSERT_TRUE(validate(TUSB_XFER_INTERRUPT, (uint16_t)((2u << 11) | 1024u), TUSB_SPEED_HIGH));
  TEST_ASSERT_TRUE(validate(TUSB_XFER_ISOCHRONOUS, (uint16_t)((2u << 11) | 1024u), TUSB_SPEED_HIGH));
}
