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

enum { RHPORT = 0, EP_CTRL_OUT = 0x00, EP_CTRL_IN = 0x80, EP_HID_IN = 0x81 };
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

// EP0 transfers the stack queued, with IN data snapshotted at submission
typedef struct {
  uint8_t ep;
  uint16_t len;
  uint8_t data[CFG_TUD_ENDPOINT0_SIZE];
} ep0_xfer_t;
static ep0_xfer_t ep0_log[16];
static unsigned ep0_count;
static unsigned ep0_cursor;
static bool ep0_stalled;

static bool stub_edpt_xfer(uint8_t rhport, uint8_t ep, uint8_t* buf, uint16_t len, bool is_isr, int n) {
  (void) rhport; (void) is_isr; (void) n;
  TEST_ASSERT_EQUAL(0, tu_edpt_number(ep));
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(ep0_log), ep0_count);
  ep0_xfer_t* x = &ep0_log[ep0_count++];
  x->ep = ep;
  x->len = len;
  if (tu_edpt_dir(ep) == TUSB_DIR_IN && len) memcpy(x->data, buf, len);
  return true;
}
static void stub_edpt_stall(uint8_t rhport, uint8_t ep, int n) {
  (void) rhport; (void) ep; (void) n;
  ep0_stalled = true;
}

// the next queued EP0 transfer must be `ep`/`len`; the host completes it
static const ep0_xfer_t* host_complete_ep0(uint8_t ep, uint16_t len) {
  TEST_ASSERT_LESS_THAN_MESSAGE(ep0_count, ep0_cursor, "no EP0 transfer queued");
  const ep0_xfer_t* x = &ep0_log[ep0_cursor++];
  TEST_ASSERT_EQUAL_HEX8(ep, x->ep);
  TEST_ASSERT_EQUAL(len, x->len);
  dcd_event_xfer_complete(RHPORT, ep, len, XFER_RESULT_SUCCESS, false);
  tud_task();
  return x;
}

static void host_setup(const tusb_control_request_t* req) {
  dcd_event_setup_received(RHPORT, (const uint8_t*) req, false);
  tud_task();
}

static void open_device(void) {
  dcd_event_bus_reset(RHPORT, TUSB_SPEED_FULL, false);
  tud_task();
  const tusb_control_request_t req = { .bmRequestType = 0x00, .bRequest = TUSB_REQ_SET_CONFIGURATION, .wValue = 1 };
  host_setup(&req);
  TEST_ASSERT_FALSE(ep0_stalled);
  host_complete_ep0(EP_CTRL_IN, 0);
}

static const tusb_control_request_t req_get_report = {
  .bmRequestType = 0xA1, .bRequest = HID_REQ_CONTROL_GET_REPORT,
  .wValue = (HID_REPORT_TYPE_INPUT << 8) | REPORT_ID, .wIndex = 0, .wLength = 16 };

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt_close_all_Ignore();
  dcd_set_address_Ignore();
  dcd_edpt0_status_complete_Ignore();
  dcd_sof_enable_Ignore();
  dcd_edpt_open_IgnoreAndReturn(true);
  dcd_edpt_xfer_Stub(stub_edpt_xfer);
  dcd_edpt_stall_Stub(stub_edpt_stall);
  ep0_count = ep0_cursor = 0;
  ep0_stalled = false;
  get_report_id = 0;
  get_report_reqlen = 0;

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    dcd_init_ExpectAndReturn(RHPORT, &dev_init, true);
    tusb_init(RHPORT, &dev_init);
  }
  open_device();
}

void tearDown(void) {}

// a zero-length report stalls rather than send the report ID byte alone
void test_get_report_zero_length_stalls(void) {
  get_report_ret = 0;
  const unsigned queued = ep0_count;
  host_setup(&req_get_report);
  TEST_ASSERT_EQUAL(REPORT_ID, get_report_id);
  TEST_ASSERT_TRUE(ep0_stalled);
  TEST_ASSERT_EQUAL(queued, ep0_count); // no data stage
}

void test_get_report_prefixes_report_id(void) {
  get_report_ret = sizeof(report_payload);
  host_setup(&req_get_report);
  TEST_ASSERT_FALSE(ep0_stalled);
  TEST_ASSERT_EQUAL(REPORT_ID, get_report_id);
  TEST_ASSERT_EQUAL(req_get_report.wLength - 1, get_report_reqlen);

  const ep0_xfer_t* x = host_complete_ep0(EP_CTRL_IN, 1 + sizeof(report_payload));
  TEST_ASSERT_EQUAL_HEX8(REPORT_ID, x->data[0]);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(report_payload, x->data + 1, sizeof(report_payload));
  host_complete_ep0(EP_CTRL_OUT, 0);
}
