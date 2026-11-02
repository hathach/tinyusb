/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

#include "tusb_option.h"
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd.c")
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("hid_device.c")

#include "mock_dcd.h"
#include "usbd_ep0_test.h"

enum { EP_HID_IN = 0x81 };
enum { REPORT_ID = 3 };

static const uint8_t desc_report[] = { TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID)) };

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t desc_config[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100),
  TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_report), EP_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 10)
};

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void) index; return desc_config; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { (void) index; (void) langid; return NULL; }
const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) { (void) instance; return desc_report; }

static const uint8_t report_payload[] = { 0x11, 0x22, 0x33, 0x44 };
static uint16_t get_report_ret;
static uint8_t get_report_id;
static uint16_t get_report_reqlen;

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer,
                               uint16_t reqlen) {
  (void) instance; (void) report_type;
  get_report_id = report_id;
  get_report_reqlen = reqlen;
  memcpy(buffer, report_payload, get_report_ret);
  return get_report_ret;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer,
                           uint16_t bufsize) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

static const tusb_control_request_t req_get_report = {
  .bmRequestType = 0xA1, .bRequest = HID_REQ_CONTROL_GET_REPORT,
  .wValue = (HID_REPORT_TYPE_INPUT << 8) | REPORT_ID, .wIndex = 0, .wLength = 16 };

void setUp(void) {
  ep0_test_init();
  get_report_ret = 0;
  get_report_id = 0;
  get_report_reqlen = 0;
  host_bus_reset();
  host_set_config(1);
}

void tearDown(void) {}

// a zero-length report stalls rather than send the report ID byte alone
void test_get_report_zero_length_stalls(void) {
  get_report_ret = 0;
  const unsigned queued = ep0_count;
  host_setup_stalled(&req_get_report);
  TEST_ASSERT_EQUAL(REPORT_ID, get_report_id);
  TEST_ASSERT_EQUAL(queued, ep0_count); // no data stage
}

void test_get_report_prefixes_report_id(void) {
  get_report_ret = sizeof(report_payload);
  host_setup_ok(&req_get_report);
  TEST_ASSERT_EQUAL(REPORT_ID, get_report_id);
  TEST_ASSERT_EQUAL(req_get_report.wLength - 1, get_report_reqlen);

  const ep0_xfer_t* x = host_complete_ep0(EP_CTRL_IN, 1 + sizeof(report_payload));
  TEST_ASSERT_EQUAL_HEX8(REPORT_ID, x->data[0]);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(report_payload, x->data + 1, sizeof(report_payload));
  host_complete_ep0(EP_CTRL_OUT, 0);
}
