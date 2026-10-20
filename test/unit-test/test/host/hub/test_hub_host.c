/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "unity.h"
#include "tusb_option.h"
#include "host/hcd.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "host/hub.h"

TEST_SOURCE_FILE("hub.c")

enum {
  HUB_A    = CFG_TUH_DEVICE_MAX + 1,
  HUB_B    = CFG_TUH_DEVICE_MAX + 2,
  EP_IN    = 0x81,
  MAX_CTRL = 8,
  MAX_CB   = 8,

  HUB_POLL_FAIL_THRESHOLD_TEST = 3, // hub.c HUB_POLL_FAIL_THRESHOLD
};

// control requests submitted by hub.c, completed explicitly by the test
typedef struct {
  tuh_xfer_t             xfer;
  tusb_control_request_t setup;
} ctrl_req_t;

static ctrl_req_t ctrl_reqs[MAX_CTRL];
static uint8_t    ctrl_count;
static bool       ctrl_reject;

// completions of the test callback, in order
typedef struct {
  uintptr_t     user_data;
  xfer_result_t result;
} cb_record_t;

static cb_record_t cb_records[MAX_CB];
static uint8_t     cb_count;

// status-change interrupt polls armed, and hub port events raised
static uint8_t     poll_count;
static uint8_t    *poll_buf;
static hcd_event_t events[MAX_CB];
static uint8_t     event_count;

//--------------------------------------------------------------------+
// Stubs
//--------------------------------------------------------------------+
bool tuh_control_xfer(tuh_xfer_t *xfer) {
  if (ctrl_reject) {
    return false;
  }
  TEST_ASSERT_LESS_THAN(MAX_CTRL, ctrl_count);
  ctrl_req_t *req = &ctrl_reqs[ctrl_count++];
  req->setup      = *xfer->setup;
  req->xfer       = *xfer;
  req->xfer.setup = &req->setup;
  return true;
}

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void) daddr;
  (void) desc_ep;
  return true;
}

bool tuh_descriptor_get_device_local(uint8_t daddr, tusb_desc_device_t *desc_device) {
  (void) daddr;
  (void) desc_device;
  return false;
}

bool tuh_interface_set(uint8_t daddr, uint8_t itf_num, uint8_t itf_alt, tuh_xfer_cb_t complete_cb,
                       uintptr_t user_data) {
  (void) daddr;
  (void) itf_num;
  (void) itf_alt;
  (void) complete_cb;
  (void) user_data;
  return true;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void) dev_addr;
  (void) itf_num;
}

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  (void) ep_addr;
  return true;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  (void) ep_addr;
  return true;
}

bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) dev_addr;
  (void) ep_addr;
  (void) total_bytes;
  (void) complete_cb;
  (void) user_data;
  poll_count++;
  poll_buf = buffer;
  return true;
}

uint8_t usbh_get_rhport(uint8_t daddr) {
  (void) daddr;
  return 0;
}

void hcd_event_handler(hcd_event_t const *event, bool in_isr) {
  (void) in_isr;
  TEST_ASSERT_LESS_THAN(MAX_CB, event_count);
  events[event_count++] = *event;
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void cb_a(tuh_xfer_t *xfer) {
  TEST_ASSERT_LESS_THAN(MAX_CB, cb_count);
  cb_records[cb_count++] = (cb_record_t) {.user_data = xfer->user_data, .result = xfer->result};
}

static void complete_ctrl(uint8_t idx, xfer_result_t result) {
  ctrl_req_t *req  = &ctrl_reqs[idx];
  req->xfer.result = result;
  req->xfer.complete_cb(&req->xfer);
}

static void complete_ctrl_data(uint8_t idx, const void *data, uint16_t len) {
  memcpy(ctrl_reqs[idx].xfer.buffer, data, len);
  complete_ctrl(idx, XFER_RESULT_SUCCESS);
}

static void open_hub(uint8_t daddr) {
  const struct TU_ATTR_PACKED {
    tusb_desc_interface_t itf;
    tusb_desc_endpoint_t  ep;
  } desc = {
    .itf = {
      .bLength            = sizeof(tusb_desc_interface_t),
      .bDescriptorType    = TUSB_DESC_INTERFACE,
      .bInterfaceNumber   = 0,
      .bNumEndpoints      = 1,
      .bInterfaceClass    = TUSB_CLASS_HUB,
      .bInterfaceSubClass = 0,
    },
    .ep = {
      .bLength          = sizeof(tusb_desc_endpoint_t),
      .bDescriptorType  = TUSB_DESC_ENDPOINT,
      .bEndpointAddress = EP_IN,
      .bmAttributes     = {.xfer = TUSB_XFER_INTERRUPT},
      .wMaxPacketSize   = 1,
      .bInterval        = 12,
    },
  };
  TEST_ASSERT_EQUAL(sizeof(desc), hub_open(0, daddr, &desc.itf, sizeof(desc)));
}

// open, then run SET_CONFIG: hub descriptor, power every port, arm the status poll
static void configure_hub(uint8_t daddr, uint8_t nports) {
  open_hub(daddr);
  uint8_t first = ctrl_count;
  TEST_ASSERT_TRUE(hub_set_config(daddr, 0));
  const hub_desc_cs_t desc_hub = {.bLength = sizeof(hub_desc_cs_t), .bNbrPorts = nports};
  complete_ctrl_data(first, &desc_hub, sizeof(desc_hub));
  for (uint8_t i = 1; i <= nports; i++) {
    complete_ctrl(first + i, XFER_RESULT_SUCCESS);
  }
  TEST_ASSERT_EQUAL(first + 1 + nports, ctrl_count);
  ctrl_count = 0;
  poll_count = 0;
}

static void fail_polls(uint8_t daddr, xfer_result_t result, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    TEST_ASSERT_TRUE(hub_xfer_cb(daddr, EP_IN, result, 0));
  }
}

static void assert_get_port_status(uint8_t idx, uint8_t daddr, uint8_t port) {
  TEST_ASSERT_EQUAL(daddr, ctrl_reqs[idx].xfer.daddr);
  TEST_ASSERT_EQUAL(HUB_REQUEST_GET_STATUS, ctrl_reqs[idx].setup.bRequest);
  TEST_ASSERT_EQUAL(port, ctrl_reqs[idx].setup.wIndex);
  TEST_ASSERT_EQUAL(port ? TUSB_REQ_RCPT_OTHER : TUSB_REQ_RCPT_DEVICE, ctrl_reqs[idx].setup.bmRequestType_bit.recipient);
}

static void complete_port_status(uint8_t idx, bool connected, bool connection_changed) {
  hub_port_status_response_t resp = {0};
  resp.status.connection = connected;
  resp.change.connection = connection_changed;
  complete_ctrl_data(idx, &resp, sizeof(resp));
}

// fail the status poll until the recovery reads the given port, which is left pending
static void trigger_recovery(uint8_t daddr, uint8_t port) {
  const uint8_t idx = ctrl_count;
  fail_polls(daddr, XFER_RESULT_FAILED, HUB_POLL_FAIL_THRESHOLD_TEST);
  TEST_ASSERT_EQUAL(idx + 1, ctrl_count);
  assert_get_port_status(idx, daddr, port);
}

// the poll reports a connection change on port 1; complete the sequence up to the ATTACH event
static void attach_on_port1(uint8_t daddr) {
  poll_buf[0] = TU_BIT(1);
  TEST_ASSERT_TRUE(hub_xfer_cb(daddr, EP_IN, XFER_RESULT_SUCCESS, 1));
  complete_port_status(0, true, true);
  complete_ctrl(1, XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL(HCD_EVENT_DEVICE_ATTACH, events[event_count - 1].event_id);
}

void setUp(void) {
  ctrl_count  = 0;
  ctrl_reject = false;
  cb_count    = 0;
  poll_count  = 0;
  poll_buf    = NULL;
  event_count = 0;
  hub_init();
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Port GET_STATUS callback and status
//--------------------------------------------------------------------+
void test_port_status_passes_caller_callback(void) {
  TEST_ASSERT_TRUE(hub_port_get_status(HUB_A, 1, NULL, cb_a, 0xA1));

  TEST_ASSERT_EQUAL_PTR(cb_a, ctrl_reqs[0].xfer.complete_cb);
  TEST_ASSERT_EQUAL(0xA1, ctrl_reqs[0].xfer.user_data);
  TEST_ASSERT_NOT_NULL(ctrl_reqs[0].xfer.buffer);
}

void test_port_status_kept_per_hub(void) {
  TEST_ASSERT_TRUE(hub_port_get_status(HUB_A, 1, NULL, cb_a, 0xA1));
  TEST_ASSERT_TRUE(hub_port_get_status(HUB_B, 3, NULL, cb_a, 0xB3));

  complete_port_status(1, true, false);
  complete_port_status(0, false, true);

  hub_port_status_response_t status;
  hub_port_get_status_local(HUB_A, 1, &status);
  TEST_ASSERT_FALSE(status.status.connection);
  TEST_ASSERT_TRUE(status.change.connection);
  hub_port_get_status_local(HUB_B, 3, &status);
  TEST_ASSERT_TRUE(status.status.connection);
  TEST_ASSERT_FALSE(status.change.connection);
}

void test_port_status_survives_clear_feature(void) {
  configure_hub(HUB_A, 4);
  attach_on_port1(HUB_A);

  hub_port_status_response_t status;
  hub_port_get_status_local(HUB_A, 1, &status);
  TEST_ASSERT_TRUE(status.status.connection);
}

// usbh fails a closed hub's requests after hub_close(); e.g. enumeration behind it must still hear about it
void test_port_status_failed_after_close_reaches_caller(void) {
  open_hub(HUB_A);
  TEST_ASSERT_TRUE(hub_port_get_status(HUB_A, 1, NULL, cb_a, 1));

  hub_close(HUB_A);
  complete_ctrl(0, XFER_RESULT_FAILED);

  TEST_ASSERT_EQUAL(1, cb_count);
  TEST_ASSERT_EQUAL(1, cb_records[0].user_data);
  TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, cb_records[0].result);
}

//--------------------------------------------------------------------+
// Status sequence of a closed hub
//--------------------------------------------------------------------+
void test_status_failed_after_close_does_not_rearm(void) {
  configure_hub(HUB_A, 4);
  trigger_recovery(HUB_A, 1);
  const uint8_t polls = poll_count;

  hub_close(HUB_A);
  complete_ctrl(0, XFER_RESULT_FAILED);

  TEST_ASSERT_EQUAL(polls, poll_count);
  TEST_ASSERT_FALSE(hub_edpt_status_xfer(HUB_A));
  TEST_ASSERT_EQUAL(polls, poll_count);
}

//--------------------------------------------------------------------+
// Stuck status-change poll recovery
//--------------------------------------------------------------------+
void test_recovery_reads_port_status_after_threshold(void) {
  configure_hub(HUB_A, 4);
  const xfer_result_t failures[] = {XFER_RESULT_FAILED, XFER_RESULT_STALLED, XFER_RESULT_TIMEOUT};

  for (uint8_t i = 0; i < HUB_POLL_FAIL_THRESHOLD_TEST - 1; i++) {
    fail_polls(HUB_A, failures[i % TU_ARRAY_SIZE(failures)], 1);
  }
  TEST_ASSERT_EQUAL(0, ctrl_count);
  TEST_ASSERT_EQUAL(HUB_POLL_FAIL_THRESHOLD_TEST - 1, poll_count);

  fail_polls(HUB_A, XFER_RESULT_TIMEOUT, 1);
  TEST_ASSERT_EQUAL(1, ctrl_count);
  assert_get_port_status(0, HUB_A, 1);
  TEST_ASSERT_EQUAL(HUB_POLL_FAIL_THRESHOLD_TEST - 1, poll_count); // the status sequence re-arms the poll
}

void test_recovery_streak_broken_by_other_results(void) {
  configure_hub(HUB_A, 4);
  const xfer_result_t breakers[] = {XFER_RESULT_SUCCESS, XFER_RESULT_ABORTED, XFER_RESULT_INVALID};

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(breakers); i++) {
    fail_polls(HUB_A, XFER_RESULT_FAILED, HUB_POLL_FAIL_THRESHOLD_TEST - 1);
    poll_buf[0] = 0; // SUCCESS reports no change
    TEST_ASSERT_TRUE(hub_xfer_cb(HUB_A, EP_IN, breakers[i], 1));
  }
  fail_polls(HUB_A, XFER_RESULT_FAILED, HUB_POLL_FAIL_THRESHOLD_TEST - 1);

  TEST_ASSERT_EQUAL(0, ctrl_count);
}

void test_recovery_rotates_ports_then_hub(void) {
  configure_hub(HUB_A, 2);
  const uint8_t expected_ports[] = {1, 2, 0, 1};

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(expected_ports); i++) {
    fail_polls(HUB_A, XFER_RESULT_FAILED, HUB_POLL_FAIL_THRESHOLD_TEST);
    TEST_ASSERT_EQUAL(i + 1, ctrl_count);
    assert_get_port_status(i, HUB_A, expected_ports[i]);

    uint8_t polls = poll_count;
    complete_port_status(i, false, false); // no change (hub or port): the poll is re-armed
    TEST_ASSERT_EQUAL(polls + 1, poll_count);
  }
  TEST_ASSERT_EQUAL(0, event_count);
}

void test_recovery_counts_per_hub(void) {
  configure_hub(HUB_A, 4);
  configure_hub(HUB_B, 4);

  for (uint8_t i = 0; i < HUB_POLL_FAIL_THRESHOLD_TEST - 1; i++) {
    fail_polls(HUB_A, XFER_RESULT_FAILED, 1);
    fail_polls(HUB_B, XFER_RESULT_FAILED, 1);
  }
  TEST_ASSERT_EQUAL(0, ctrl_count);

  fail_polls(HUB_A, XFER_RESULT_FAILED, 1);
  TEST_ASSERT_EQUAL(1, ctrl_count);
  assert_get_port_status(0, HUB_A, 1);
}

void test_recovery_skips_hub_without_ports(void) {
  open_hub(HUB_A); // not configured: no port count yet

  fail_polls(HUB_A, XFER_RESULT_FAILED, 2 * HUB_POLL_FAIL_THRESHOLD_TEST);

  TEST_ASSERT_EQUAL(0, ctrl_count);
  TEST_ASSERT_EQUAL(2 * HUB_POLL_FAIL_THRESHOLD_TEST, poll_count);
}

void test_recovery_rejected_rearms_and_retries_same_port(void) {
  configure_hub(HUB_A, 4);

  ctrl_reject = true;
  fail_polls(HUB_A, XFER_RESULT_FAILED, HUB_POLL_FAIL_THRESHOLD_TEST);
  ctrl_reject = false;
  TEST_ASSERT_EQUAL(0, ctrl_count);
  TEST_ASSERT_EQUAL(HUB_POLL_FAIL_THRESHOLD_TEST, poll_count);

  fail_polls(HUB_A, XFER_RESULT_FAILED, HUB_POLL_FAIL_THRESHOLD_TEST);
  TEST_ASSERT_EQUAL(1, ctrl_count);
  assert_get_port_status(0, HUB_A, 1);
}

void test_recovery_failed_status_read_rearms(void) {
  configure_hub(HUB_A, 4);
  trigger_recovery(HUB_A, 1);
  uint8_t polls = poll_count;

  complete_ctrl(0, XFER_RESULT_FAILED);

  TEST_ASSERT_EQUAL(polls + 1, poll_count);
  TEST_ASSERT_EQUAL(0, event_count);
}

void test_recovery_reports_disconnect(void) {
  configure_hub(HUB_A, 4);
  trigger_recovery(HUB_A, 1);

  complete_port_status(0, false, true);
  TEST_ASSERT_EQUAL(2, ctrl_count);
  TEST_ASSERT_EQUAL(HUB_REQUEST_CLEAR_FEATURE, ctrl_reqs[1].setup.bRequest);
  TEST_ASSERT_EQUAL(HUB_FEATURE_PORT_CONNECTION_CHANGE, ctrl_reqs[1].setup.wValue);
  TEST_ASSERT_EQUAL(1, ctrl_reqs[1].setup.wIndex);

  uint8_t polls = poll_count;
  complete_ctrl(1, XFER_RESULT_SUCCESS);

  TEST_ASSERT_EQUAL(1, event_count);
  TEST_ASSERT_EQUAL(HCD_EVENT_DEVICE_REMOVE, events[0].event_id);
  TEST_ASSERT_EQUAL(HUB_A, events[0].connection.hub_addr);
  TEST_ASSERT_EQUAL(1, events[0].connection.hub_port);
  TEST_ASSERT_EQUAL(polls + 1, poll_count);
}

void test_recovery_attach_leaves_rearm_to_enumeration(void) {
  configure_hub(HUB_A, 4);
  trigger_recovery(HUB_A, 1);

  complete_port_status(0, true, true);
  uint8_t polls = poll_count;
  complete_ctrl(1, XFER_RESULT_SUCCESS);

  TEST_ASSERT_EQUAL(1, event_count);
  TEST_ASSERT_EQUAL(HCD_EVENT_DEVICE_ATTACH, events[0].event_id);
  TEST_ASSERT_EQUAL(polls, poll_count);
}

void test_recovery_hub_status_clears_change(void) {
  configure_hub(HUB_A, 1);
  trigger_recovery(HUB_A, 1);
  complete_port_status(0, false, false);
  trigger_recovery(HUB_A, 0);

  hub_status_response_t resp = {0};
  resp.change.over_current = 1;
  complete_ctrl_data(1, &resp, sizeof(resp));
  TEST_ASSERT_EQUAL(3, ctrl_count);
  TEST_ASSERT_EQUAL(HUB_REQUEST_CLEAR_FEATURE, ctrl_reqs[2].setup.bRequest);
  TEST_ASSERT_EQUAL(HUB_FEATURE_HUB_OVER_CURRENT_CHANGE, ctrl_reqs[2].setup.wValue);
  TEST_ASSERT_EQUAL(TUSB_REQ_RCPT_DEVICE, ctrl_reqs[2].setup.bmRequestType_bit.recipient);

  uint8_t polls = poll_count;
  complete_ctrl(2, XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL(polls + 1, poll_count);
  TEST_ASSERT_EQUAL(0, event_count);
}
