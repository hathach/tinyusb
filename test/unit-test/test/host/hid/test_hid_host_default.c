/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// Exercises the weak default tuh_hid_report_failed_cb(): this file does not override it

#include "unity.h"
#include "hid_host_test.h"

TEST_SOURCE_FILE("hid_host.c")

enum {
  HID_DEV_ADDR = 1,
  HID_ITF_NUM  = 0,
  HID_EP_IN    = 0x81,
  HID_EP_SIZE  = 8,
};

#define TEST_HID_ITF_DESC                                                                                    \
  9, TUSB_DESC_INTERFACE, HID_ITF_NUM, 0, 1, TUSB_CLASS_HID, HID_SUBCLASS_BOOT, HID_ITF_PROTOCOL_KEYBOARD, 0

#define TEST_HID_DESC       9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(63)

#define TEST_HID_EP_IN_DESC 7, TUSB_DESC_ENDPOINT, HID_EP_IN, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(HID_EP_SIZE), 10

static const uint8_t hid_itf_desc[] = {TEST_HID_ITF_DESC, TEST_HID_DESC, TEST_HID_EP_IN_DESC};

static uint8_t xfer_count;    // IN transfers queued
static uint8_t release_count; // endpoint released after a failed queue
static bool    xfer_ok;       // what the HCD answers when a transfer is queued
static uint8_t report_cb_count;

static uint8_t breakpoint_count; // TU_BREAKPOINT() hits, see CFG_TUSB_DEBUG_BREAKPOINT in project.yml

void hid_breakpoint_hook(void) {
  breakpoint_count++;
}

//--------------------------------------------------------------------+
// Host stack functions called by hid_host.c
//--------------------------------------------------------------------+
bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void)daddr;
  (void)desc_ep;
  return true;
}

bool tuh_edpt_abort_xfer(uint8_t daddr, uint8_t ep_addr) {
  (void)daddr;
  (void)ep_addr;
  return true;
}

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  (void)xfer;
  return false;
}

bool tuh_descriptor_get_hid_report(uint8_t daddr, uint8_t itf_num, uint8_t desc_type, uint8_t index, void *buffer,
                                   uint16_t len, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void)daddr;
  (void)itf_num;
  (void)desc_type;
  (void)index;
  (void)buffer;
  (void)len;
  (void)complete_cb;
  (void)user_data;
  return false;
}

uint8_t *usbh_get_enum_buf(void) {
  return NULL;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void)dev_addr;
  (void)itf_num;
}

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  return true;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  release_count++;
  return true;
}

bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  return false;
}

bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void)dev_addr;
  (void)buffer;
  (void)total_bytes;
  (void)complete_cb;
  (void)user_data;
  if (ep_addr == HID_EP_IN) {
    xfer_count++;
  }
  return xfer_ok;
}

//--------------------------------------------------------------------+
// Application callback, re-arms like the examples
//--------------------------------------------------------------------+
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) {
  (void)report;
  (void)len;
  report_cb_count++;
  TEST_ASSERT_TRUE(tuh_hid_receive_report(dev_addr, idx));
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void setUp(void) {
  xfer_count       = 0;
  release_count    = 0;
  xfer_ok          = true;
  report_cb_count  = 0;
  breakpoint_count = 0;

  TEST_ASSERT_TRUE(hidh_init());
  TEST_ASSERT_EQUAL(sizeof(hid_itf_desc),
                    hidh_open(0, HID_DEV_ADDR, (const tusb_desc_interface_t *)hid_itf_desc, sizeof(hid_itf_desc)));
  TEST_ASSERT_TRUE(tuh_hid_receive_report(HID_DEV_ADDR, 0));
  TEST_ASSERT_EQUAL(1, xfer_count);
}

void tearDown(void) {
  hidh_close(HID_DEV_ADDR);
}

// The default queues exactly one new transfer per failure, the report callback is not involved
void test_default_rearms_after_failure(void) {
  const xfer_result_t results[] = {XFER_RESULT_FAILED, XFER_RESULT_TIMEOUT};

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(results); i++) {
    TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, results[i], 0));
    TEST_ASSERT_EQUAL(i + 2, xfer_count);
  }
  TEST_ASSERT_EQUAL(0, report_cb_count);
}

// A stalled endpoint stays halted until CLEAR_FEATURE(ENDPOINT_HALT), an aborted transfer was cancelled: no re-arm
void test_default_stops_after_stall_or_abort(void) {
  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_STALLED, 0));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_ABORTED, 0));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL(0, report_cb_count);
}

// The application re-arms on success, the default on failure: never two transfers for one completion
void test_success_and_failure_rearm_once_each(void) {
  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_SUCCESS, HID_EP_SIZE));
  TEST_ASSERT_EQUAL(2, xfer_count);
  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_FAILED, 0));
  TEST_ASSERT_EQUAL(3, xfer_count);
  TEST_ASSERT_EQUAL(1, report_cb_count);
}

// The host controller can refuse the re-arm, e.g. when the device is gone: the endpoint is released and polling
// ends without a callback, but the CPU must not halt
void test_default_rearm_failure(void) {
  xfer_ok = false;
  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_FAILED, 0));
  TEST_ASSERT_EQUAL(2, xfer_count);
  TEST_ASSERT_EQUAL(1, release_count);
  TEST_ASSERT_EQUAL(0, report_cb_count);
  TEST_ASSERT_EQUAL(0, breakpoint_count);
}
