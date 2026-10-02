/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "unity.h"
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("hid_device.c")

#include "mock_dcd.h"

enum { RHPORT = 0, EP_OUT = 0x01, EP_IN = 0x81, EP_NEXT_IN = 0x82 };

static const uint8_t desc_fs_hs[] = {
  TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, 8, EP_OUT, EP_IN, 64, 1),
  TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, 8, EP_NEXT_IN, 64, 1)
};

static const uint8_t desc_ss[] = {
  9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_HID, 0, HID_ITF_PROTOCOL_NONE, 0,
  9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(8),
  7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
  TUD_SUPERSPEED_DESC_EP_COMPANION(0, 0, 64),
  7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
  TUD_SUPERSPEED_DESC_EP_COMPANION(0, 0, 64),
  TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, 8, EP_NEXT_IN, 64, 1),
  TUD_SUPERSPEED_DESC_EP_COMPANION(0, 0, 64)
};

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void)index; return NULL; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)index; (void)langid; return NULL;
}
const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) { (void)instance; return NULL; }
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t* buffer, uint16_t reqlen) {
  (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen; return 0;
}
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           const uint8_t* buffer, uint16_t bufsize) {
  (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  if (!tud_inited()) {
    const tusb_rhport_init_t init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    dcd_init_ExpectAndReturn(RHPORT, &init, true);
    TEST_ASSERT_TRUE(tusb_init(RHPORT, &init));
  }
}
void tearDown(void) {}

static void reset_device(tusb_speed_t speed) {
  dcd_event_bus_reset(RHPORT, speed, false);
  tud_task();
}

static void expect_open(const uint8_t* desc, const uint8_t* desc_end) {
  dcd_edpt_open_ExpectAndReturn(RHPORT, (const tusb_desc_endpoint_t*)desc, desc_end, true);
}

static void expect_out_read(bool accepted) {
  dcd_edpt_xfer_ExpectAndReturn(RHPORT, EP_OUT, NULL, CFG_TUD_HID_EP_BUFSIZE, false, accepted);
  dcd_edpt_xfer_IgnoreArg_buffer();
}

static void check_consumed_length(tusb_speed_t speed, const uint8_t* desc, uint16_t total_len, bool ss,
                                   bool accept_out) {
  reset_device(speed);
  const uint8_t* const desc_end = desc + total_len;
  const uint16_t first_len = TUD_HID_INOUT_DESC_LEN + (ss ? 12 : 0);
  const uint16_t next_len = TUD_HID_DESC_LEN + (ss ? 6 : 0);
  expect_open(desc + 18, desc_end);
  expect_open(desc + 25 + (ss ? 6 : 0), desc_end);
  expect_out_read(accept_out);
  const uint16_t consumed = hidd_open(RHPORT, (const tusb_desc_interface_t*)desc, total_len);
  TEST_ASSERT_EQUAL(first_len, consumed);

  // The returned boundary must point to the next interface, not an endpoint companion.
  const tusb_desc_interface_t* next = (const tusb_desc_interface_t*)(desc + consumed);
  TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE, next->bDescriptorType);
  TEST_ASSERT_EQUAL(1, next->bInterfaceNumber);
  expect_open(desc + consumed + 18, desc_end);
  TEST_ASSERT_EQUAL(next_len, hidd_open(RHPORT, next, total_len - consumed));
}

void test_hid_fs_consumes_only_its_interface(void) {
  check_consumed_length(TUSB_SPEED_FULL, desc_fs_hs, sizeof(desc_fs_hs), false, true);
}

void test_hid_hs_consumes_only_its_interface(void) {
  check_consumed_length(TUSB_SPEED_HIGH, desc_fs_hs, sizeof(desc_fs_hs), false, true);
}

void test_hid_ss_consumes_both_companions_before_next_interface(void) {
  check_consumed_length(TUSB_SPEED_SUPER, desc_ss, sizeof(desc_ss), true, true);
}

void test_hid_ss_refused_initial_out_read_keeps_consumed_length(void) {
  check_consumed_length(TUSB_SPEED_SUPER, desc_ss, sizeof(desc_ss), true, false);
}

void test_hid_rejects_hid_descriptor_extending_past_boundary(void) {
  uint8_t malformed[sizeof(desc_fs_hs)];
  memcpy(malformed, desc_fs_hs, sizeof(malformed));
  malformed[9] = sizeof(malformed); // HID descriptor would run beyond max_len.
  reset_device(TUSB_SPEED_FULL);
  TEST_ASSERT_EQUAL(0, hidd_open(RHPORT, (const tusb_desc_interface_t*)malformed, sizeof(malformed)));
}

void test_hid_rejects_truncated_second_endpoint_after_companion(void) {
  reset_device(TUSB_SPEED_SUPER);
  const uint16_t max_len = TUD_HID_INOUT_DESC_LEN;
  expect_open(desc_ss + 18, desc_ss + max_len);
  TEST_ASSERT_EQUAL(0, hidd_open(RHPORT, (const tusb_desc_interface_t*)desc_ss, max_len));
}
