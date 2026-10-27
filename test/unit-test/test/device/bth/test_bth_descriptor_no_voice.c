/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// TUD_BTH_DESCRIPTOR with CFG_TUD_BTH_ISO_ALT_COUNT=0 (project.yml): the HCI interface alone, no IAD

#include "unity.h"

#include "tusb_types.h"
#include "device/usbd.h"

enum {
  ITF_NUM_BTH = 0,
  EPNUM_BTH_EVT = 0x81,
  EPNUM_BTH_ACL_IN = 0x82,
  EPNUM_BTH_ACL_OUT = 0x02,
};

static uint8_t const desc_bth[] = {
  TUD_BTH_DESCRIPTOR(ITF_NUM_BTH, 0, EPNUM_BTH_EVT, 16, 1, EPNUM_BTH_ACL_IN, EPNUM_BTH_ACL_OUT, 64)
};

void setUp(void) {}

void tearDown(void) {}

void test_length_matches_desc_len(void) {
  TEST_ASSERT_EQUAL(30, TUD_BTH_DESC_LEN);
  TEST_ASSERT_EQUAL(TUD_BTH_DESC_LEN, sizeof(desc_bth));
}

void test_descriptor_lengths_cover_template(void) {
  uint8_t const* p_desc = desc_bth;
  uint8_t const* desc_end = desc_bth + sizeof(desc_bth);
  uint8_t count = 0;
  while (tu_desc_in_bounds(p_desc, desc_end)) {
    TEST_ASSERT_NOT_EQUAL(0, tu_desc_len(p_desc));
    p_desc = tu_desc_next(p_desc);
    count++;
  }
  TEST_ASSERT_EQUAL_PTR(desc_end, p_desc);
  TEST_ASSERT_EQUAL(4, count); // HCI interface + 3 EPs
}

void test_starts_with_hci_interface(void) {
  tusb_desc_interface_t const* hci = (tusb_desc_interface_t const*) desc_bth;
  TEST_ASSERT_EQUAL(sizeof(tusb_desc_interface_t), hci->bLength);
  TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE, hci->bDescriptorType);
  TEST_ASSERT_EQUAL(ITF_NUM_BTH, hci->bInterfaceNumber);
  TEST_ASSERT_EQUAL(0, hci->bAlternateSetting);
  TEST_ASSERT_EQUAL(3, hci->bNumEndpoints);
  TEST_ASSERT_EQUAL(TUD_BT_APP_CLASS, hci->bInterfaceClass);
}
