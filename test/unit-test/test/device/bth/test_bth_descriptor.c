/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// TUD_BTH_DESCRIPTOR with CFG_TUD_BTH_ISO_ALT_COUNT=2 (project.yml): IAD, HCI interface, two voice alternates

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
  TUD_BTH_DESCRIPTOR(ITF_NUM_BTH, 0, EPNUM_BTH_EVT, 16, 1, EPNUM_BTH_ACL_IN, EPNUM_BTH_ACL_OUT, 64, 0, 9)
};

void setUp(void) {}

void tearDown(void) {}

void test_length_matches_desc_len(void) {
  TEST_ASSERT_EQUAL(8 + 30 + 2 * 23, TUD_BTH_DESC_LEN);
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
  TEST_ASSERT_EQUAL(1 + 4 + 2 * 3, count); // IAD, HCI interface + 3 EPs, 2 alternates of interface + 2 EPs
}

void test_iad_groups_hci_and_voice_interfaces(void) {
  tusb_desc_interface_assoc_t const* iad = (tusb_desc_interface_assoc_t const*) desc_bth;
  TEST_ASSERT_EQUAL(sizeof(tusb_desc_interface_assoc_t), iad->bLength);
  TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE_ASSOCIATION, iad->bDescriptorType);
  TEST_ASSERT_EQUAL(ITF_NUM_BTH, iad->bFirstInterface);
  TEST_ASSERT_EQUAL(2, iad->bInterfaceCount);
  TEST_ASSERT_EQUAL(TUD_BT_APP_CLASS, iad->bFunctionClass);
}

void test_voice_alternates_follow_hci_interface(void) {
  tusb_desc_interface_t const* hci = (tusb_desc_interface_t const*) (desc_bth + 8);
  TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE, hci->bDescriptorType);
  TEST_ASSERT_EQUAL(ITF_NUM_BTH, hci->bInterfaceNumber);
  TEST_ASSERT_EQUAL(3, hci->bNumEndpoints);

  for (uint8_t alt = 0; alt < 2; alt++) {
    tusb_desc_interface_t const* voice = (tusb_desc_interface_t const*) (desc_bth + 8 + 30 + alt * 23);
    TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE, voice->bDescriptorType);
    TEST_ASSERT_EQUAL(ITF_NUM_BTH + 1, voice->bInterfaceNumber);
    TEST_ASSERT_EQUAL(alt, voice->bAlternateSetting);
    TEST_ASSERT_EQUAL(2, voice->bNumEndpoints);
  }
}
