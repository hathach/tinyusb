/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

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

// Boot keyboard: interface, HID descriptor and one interrupt IN endpoint
static const uint8_t hid_itf_desc[] = {TEST_HID_ITF_DESC, TEST_HID_DESC, TEST_HID_EP_IN_DESC};

static uint8_t *edpt_xfer_buffer; // buffer the driver queued on the IN endpoint

static uint8_t        report_cb_count;
static uint8_t        report_cb_daddr;
static uint8_t        report_cb_idx;
static const uint8_t *report_cb_report;
static uint16_t       report_cb_len;

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
  (void)total_bytes;
  (void)complete_cb;
  (void)user_data;
  if (ep_addr == HID_EP_IN) {
    edpt_xfer_buffer = buffer;
  }
  return true;
}

//--------------------------------------------------------------------+
// Application callback
//--------------------------------------------------------------------+
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) {
  report_cb_count++;
  report_cb_daddr  = dev_addr;
  report_cb_idx    = idx;
  report_cb_report = report;
  report_cb_len    = len;
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void setUp(void) {
  edpt_xfer_buffer = NULL;
  report_cb_count  = 0;
  report_cb_daddr  = 0;
  report_cb_idx    = TUSB_INDEX_INVALID_8;
  report_cb_report = NULL;
  report_cb_len    = 0xFFFF;

  TEST_ASSERT_TRUE(hidh_init());
  TEST_ASSERT_EQUAL(sizeof(hid_itf_desc),
                    hidh_open(0, HID_DEV_ADDR, (const tusb_desc_interface_t *)hid_itf_desc, sizeof(hid_itf_desc)));

  // queue the IN transfer the way an application does
  TEST_ASSERT_TRUE(tuh_hid_receive_report(HID_DEV_ADDR, 0));
  TEST_ASSERT_NOT_NULL(edpt_xfer_buffer);
}

void tearDown(void) {
  hidh_close(HID_DEV_ADDR);
}

void test_hidh_report_received(void) {
  const uint8_t report[HID_EP_SIZE] = {0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
  memcpy(edpt_xfer_buffer, report, sizeof(report)); // what the HCD writes

  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_SUCCESS, sizeof(report)));

  TEST_ASSERT_EQUAL(1, report_cb_count);
  TEST_ASSERT_EQUAL(HID_DEV_ADDR, report_cb_daddr);
  TEST_ASSERT_EQUAL(0, report_cb_idx);
  TEST_ASSERT_EQUAL_PTR(edpt_xfer_buffer, report_cb_report);
  TEST_ASSERT_EQUAL(sizeof(report), report_cb_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(report, report_cb_report, sizeof(report));
}

// Some devices send zero-length reports: a successful transfer of 0 bytes still passes the buffer
void test_hidh_report_received_zero_length(void) {
  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_SUCCESS, 0));

  TEST_ASSERT_EQUAL(1, report_cb_count);
  TEST_ASSERT_EQUAL_PTR(edpt_xfer_buffer, report_cb_report);
  TEST_ASSERT_EQUAL(0, report_cb_len);
}

// A failed transfer passes report = NULL, so the application can tell it from a zero-length report
void test_hidh_report_failed(void) {
  const xfer_result_t results[] = {XFER_RESULT_FAILED, XFER_RESULT_STALLED, XFER_RESULT_TIMEOUT, XFER_RESULT_ABORTED};

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(results); i++) {
    report_cb_report = edpt_xfer_buffer; // not NULL, so the check below sees what the callback got

    TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, results[i], 0));

    TEST_ASSERT_EQUAL(i + 1, report_cb_count);
    TEST_ASSERT_EQUAL(HID_DEV_ADDR, report_cb_daddr);
    TEST_ASSERT_EQUAL(0, report_cb_idx);
    TEST_ASSERT_NULL(report_cb_report);
    TEST_ASSERT_EQUAL(0, report_cb_len);
  }
}
