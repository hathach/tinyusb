/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// SuperSpeed endpoint companion walk (usbd_pvt.h): the walk must never read a descriptor header
// past desc_end. Each buffer carries one guard byte after desc_end holding a companion type, so a
// header read past the end changes the result instead of passing silently.

#include "unity.h"

#include "common/tusb_common.h"
#include "device/usbd_pvt.h"

#if !TUD_OPT_SUPER_SPEED
#error "test_usbd_ss_companion needs a SuperSpeed build (no TUD_TEST_NO_SUPER_SPEED)"
#endif

#define EP_DESC(addr, attr)  7, TUSB_DESC_ENDPOINT, addr, attr, 0x00, 0x04, 0
#define COMP_DESC            6, TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION, 0, 0, 0, 0
#define ISO_COMP_DESC        8, TUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION, 0, 0, 0, 0, 0, 0
#define GUARD                TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION

void setUp(void) {}
void tearDown(void) {}

//--------------------------------------------------------------------+
// Valid descriptors
//--------------------------------------------------------------------+
void test_companion_len_bulk_pair(void) {
  const uint8_t desc[] = { EP_DESC(0x01, 2), COMP_DESC, EP_DESC(0x81, 2), COMP_DESC };
  TEST_ASSERT_EQUAL_UINT16(12, usbd_ss_ep_companion_len(desc, desc + sizeof(desc), 2));
}

void test_companion_len_iso(void) {
  const uint8_t desc[] = { EP_DESC(0x81, 1), ISO_COMP_DESC };
  TEST_ASSERT_EQUAL_UINT16(8, usbd_ss_ep_companion_len(desc, desc + sizeof(desc), 1));
}

void test_companion_len_stops_at_ep_count(void) {
  const uint8_t desc[] = { EP_DESC(0x01, 2), COMP_DESC, EP_DESC(0x81, 2), COMP_DESC };
  TEST_ASSERT_EQUAL_UINT16(6, usbd_ss_ep_companion_len(desc, desc + sizeof(desc), 1));
}

void test_skip_companion(void) {
  const uint8_t desc[] = { COMP_DESC, EP_DESC(0x81, 2) };
  TEST_ASSERT_EQUAL_PTR(desc + 6, usbd_skip_ss_ep_companion(desc, desc + sizeof(desc)));
}

//--------------------------------------------------------------------+
// Truncated: a final 1-byte descriptor has no bDescriptorType inside desc_end
//--------------------------------------------------------------------+
void test_skip_stops_at_one_byte_descriptor(void) {
  const uint8_t buf[] = { 1, GUARD };
  const uint8_t* end = buf + 1;
  TEST_ASSERT_EQUAL_PTR(buf, usbd_skip_ss_ep_companion(buf, end));
}

void test_companion_len_ignores_one_byte_trailer(void) {
  const uint8_t buf[] = { EP_DESC(0x81, 3), 1, GUARD };
  const uint8_t* end = buf + sizeof(buf) - 1;
  TEST_ASSERT_EQUAL_UINT16(0, usbd_ss_ep_companion_len(buf, end, 1));
}

void test_companion_len_rejects_one_byte_endpoint(void) {
  const uint8_t buf[] = { 1, TUSB_DESC_ENDPOINT, GUARD, 0, 0, 0 };
  TEST_ASSERT_EQUAL_UINT16(0, usbd_ss_ep_companion_len(buf, buf + sizeof(buf), 1));
}

void test_skip_stops_at_zero_length_descriptor(void) {
  const uint8_t buf[] = { 0, TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION, 0, 0, 0, 0 };
  TEST_ASSERT_EQUAL_PTR(buf, usbd_skip_ss_ep_companion(buf, buf + sizeof(buf)));
}
