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

// the status endpoint open fails in hub_open()
static bool    edpt_open_fail;
static uint8_t edpt_open_count;
static uint8_t edpt_open_interval; // bInterval of the last endpoint opened

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

// the device descriptor reports a multi-TT hub; SET_CONFIG completions reported to usbh
static bool    dev_mtt;
static uint8_t config_complete_count;

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
  edpt_open_count++;
  edpt_open_interval = desc_ep->bInterval;
  return !edpt_open_fail;
}

bool tuh_descriptor_get_device_local(uint8_t daddr, tusb_desc_device_t *desc_device) {
  (void) daddr;
  desc_device->bDeviceProtocol = HUB_PROTOCOL_HIGH_SPEED_MTT;
  return dev_mtt;
}

// queued like a control request, completed by the test
bool tuh_interface_set(uint8_t daddr, uint8_t itf_num, uint8_t itf_alt, tuh_xfer_cb_t complete_cb,
                       uintptr_t user_data) {
  const tusb_control_request_t setup = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE},
    .bRequest          = TUSB_REQ_SET_INTERFACE,
    .wValue            = itf_alt,
    .wIndex            = itf_num,
  };
  tuh_xfer_t xfer = {.daddr = daddr, .setup = &setup, .complete_cb = complete_cb, .user_data = user_data};
  return tuh_control_xfer(&xfer);
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void) dev_addr;
  (void) itf_num;
  config_complete_count++;
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
  ctrl_reqs[idx].xfer.actual_len = len;
  complete_ctrl(idx, XFER_RESULT_SUCCESS);
}

// hub interface and its status-change endpoint
static const struct TU_ATTR_PACKED {
  tusb_desc_interface_t itf;
  tusb_desc_endpoint_t  ep;
} desc_hub_itf = {
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

static void open_hub(uint8_t daddr) {
  TEST_ASSERT_EQUAL(sizeof(desc_hub_itf), hub_open(0, daddr, &desc_hub_itf.itf, sizeof(desc_hub_itf)));
}

// a multi-TT hub: alternate setting 0 single-TT, 1 multi-TT, each with the status endpoint
static const struct TU_ATTR_PACKED {
  tusb_desc_interface_t itf0;
  tusb_desc_endpoint_t  ep0;
  tusb_desc_interface_t itf1;
  tusb_desc_endpoint_t  ep1;
} desc_mtt_hub = {
  .itf0 = {
    .bLength            = sizeof(tusb_desc_interface_t),
    .bDescriptorType    = TUSB_DESC_INTERFACE,
    .bNumEndpoints      = 1,
    .bInterfaceClass    = TUSB_CLASS_HUB,
    .bInterfaceProtocol = HUB_PROTOCOL_HIGH_SPEED_STT,
  },
  .ep0 = {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = EP_IN,
    .bmAttributes     = {.xfer = TUSB_XFER_INTERRUPT},
    .wMaxPacketSize   = 1,
    .bInterval        = 12,
  },
  .itf1 = {
    .bLength            = sizeof(tusb_desc_interface_t),
    .bDescriptorType    = TUSB_DESC_INTERFACE,
    .bAlternateSetting  = 1,
    .bNumEndpoints      = 1,
    .bInterfaceClass    = TUSB_CLASS_HUB,
    .bInterfaceProtocol = HUB_PROTOCOL_HIGH_SPEED_MTT,
  },
  .ep1 = {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = EP_IN,
    .bmAttributes     = {.xfer = TUSB_XFER_INTERRUPT},
    .wMaxPacketSize   = 1,
    .bInterval        = 12,
  },
};

static void open_mtt_hub(uint8_t daddr) {
  dev_mtt = true;
  TEST_ASSERT_EQUAL(sizeof(desc_mtt_hub), hub_open(0, daddr, &desc_mtt_hub.itf0, sizeof(desc_mtt_hub)));
  dev_mtt = false;
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
  ctrl_count     = 0;
  ctrl_reject    = false;
  edpt_open_fail = false;
  edpt_open_count    = 0;
  edpt_open_interval = 0;
  cb_count       = 0;
  poll_count     = 0;
  poll_buf       = NULL;
  event_count    = 0;
  dev_mtt        = false;
  config_complete_count = 0;
  hub_init();
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Open
//--------------------------------------------------------------------+
// usbh gives a hub address only to a device whose bDeviceClass is hub, but offers each interface to every driver
void test_open_rejects_non_hub_address(void) {
  for (uint8_t daddr = 1; daddr <= CFG_TUH_DEVICE_MAX; daddr++) {
    TEST_ASSERT_EQUAL(0, hub_open(0, daddr, &desc_hub_itf.itf, sizeof(desc_hub_itf)));
  }
  open_hub(HUB_A);
}

// a failed open leaves the entry unopened, which hub_close() does not clear
void test_open_failed_does_not_leave_mtt(void) {
  dev_mtt        = true;
  edpt_open_fail = true;
  TEST_ASSERT_EQUAL(0, hub_open(0, HUB_A, &desc_mtt_hub.itf0, sizeof(desc_mtt_hub)));
  dev_mtt        = false;
  edpt_open_fail = false;
  hub_close(HUB_A);
  TEST_ASSERT_FALSE(hub_tt_multi(HUB_A));

  // a single-TT hub at the same address reads its hub descriptor without SET_INTERFACE
  open_hub(HUB_A);
  TEST_ASSERT_FALSE(hub_tt_multi(HUB_A));
  TEST_ASSERT_TRUE(hub_set_config(HUB_A, 0));
  TEST_ASSERT_EQUAL(1, ctrl_count);
  TEST_ASSERT_EQUAL(HUB_REQUEST_GET_DESCRIPTOR, ctrl_reqs[0].setup.bRequest);
}

// interface and status endpoint each longer than defined, followed by another interface
static const uint8_t desc_hub_long[] = {
  12, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_HUB, 0, 0, 0, 0xAA, 0xAA, 0xAA,
  9, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_INTERRUPT, 1, 0, 12, 0xAA, 0xAA,
  9, TUSB_DESC_INTERFACE, 1, 0, 0, 0xFF, 0, 0, 0,
};
enum { DESC_HUB_LONG_LEN = 12 + 9 };

// extra bytes are skipped (USB 2.0 9.5): the walk follows bLength and returns what it consumed
void test_open_walks_long_descriptors_by_length(void) {
  const tusb_desc_interface_t *itf = (const tusb_desc_interface_t *) desc_hub_long;
  TEST_ASSERT_EQUAL(DESC_HUB_LONG_LEN, hub_open(0, HUB_A, itf, sizeof(desc_hub_long)));
  TEST_ASSERT_EQUAL(1, edpt_open_count);
  TEST_ASSERT_EQUAL(12, edpt_open_interval);
}

// a span ending inside or right after a descriptor: nothing past max_len is read or opened
void test_open_bounded_by_max_len(void) {
  const tusb_desc_interface_t *itf = (const tusb_desc_interface_t *) desc_hub_long;
  const uint16_t lens[] = {12, DESC_HUB_LONG_LEN - 1};
  for (uint8_t i = 0; i < TU_ARRAY_SIZE(lens); i++) {
    TEST_ASSERT_EQUAL(0, hub_open(0, HUB_A, itf, lens[i]));
  }
  TEST_ASSERT_EQUAL(0, edpt_open_count);
}

void test_open_rejects_short_descriptors(void) {
  // interface shorter than defined
  uint8_t desc[sizeof(desc_hub_itf)];
  memcpy(desc, &desc_hub_itf, sizeof(desc));
  desc[0] = sizeof(tusb_desc_interface_t) - 1;
  TEST_ASSERT_EQUAL(0, hub_open(0, HUB_A, (const tusb_desc_interface_t *) desc, sizeof(desc)));

  // status endpoint shorter than defined
  memcpy(desc, &desc_hub_itf, sizeof(desc));
  desc[sizeof(tusb_desc_interface_t)] = sizeof(tusb_desc_endpoint_t) - 1;
  TEST_ASSERT_EQUAL(0, hub_open(0, HUB_A, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  TEST_ASSERT_EQUAL(0, edpt_open_count);
}

// multi-TT hub with every descriptor longer than defined: alt 1's endpoint is opened
static const uint8_t desc_mtt_hub_long[] = {
  10, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_HUB, 0, HUB_PROTOCOL_HIGH_SPEED_STT, 0, 0xAA,
  8, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_INTERRUPT, 1, 0, 12, 0xAA,
  10, TUSB_DESC_INTERFACE, 0, 1, 1, TUSB_CLASS_HUB, 0, HUB_PROTOCOL_HIGH_SPEED_MTT, 0, 0xAA,
  8, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_INTERRUPT, 1, 0, 4, 0xAA,
};

void test_open_mtt_walks_long_descriptors_by_length(void) {
  const tusb_desc_interface_t *itf = (const tusb_desc_interface_t *) desc_mtt_hub_long;
  dev_mtt = true;
  TEST_ASSERT_EQUAL(0, hub_open(0, HUB_A, itf, sizeof(desc_mtt_hub_long) - 1));
  TEST_ASSERT_EQUAL(0, hub_open(0, HUB_A, itf, 10 + 8)); // alt 1 outside the span
  TEST_ASSERT_EQUAL(0, edpt_open_count);

  TEST_ASSERT_EQUAL(sizeof(desc_mtt_hub_long), hub_open(0, HUB_A, itf, sizeof(desc_mtt_hub_long)));
  dev_mtt = false;
  TEST_ASSERT_EQUAL(1, edpt_open_count);
  TEST_ASSERT_EQUAL(4, edpt_open_interval);
  TEST_ASSERT_TRUE(hub_tt_multi(HUB_A));
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

// a short reply leaves the previous status in ctrl_buf: it fails like any failed read
void test_short_port_status_rearms(void) {
  configure_hub(HUB_A, 4);
  attach_on_port1(HUB_A); // ctrl_buf: port 1 connected, connection changed

  poll_buf[0] = TU_BIT(2);
  TEST_ASSERT_TRUE(hub_xfer_cb(HUB_A, EP_IN, XFER_RESULT_SUCCESS, 1));
  assert_get_port_status(2, HUB_A, 2);
  const uint8_t  polls       = poll_count;
  const uint16_t status_only = 0; // wPortStatus without wPortChange
  complete_ctrl_data(2, &status_only, sizeof(status_only));

  TEST_ASSERT_EQUAL(3, ctrl_count); // no CLEAR_FEATURE for port 1's change
  TEST_ASSERT_EQUAL(polls + 1, poll_count);
  TEST_ASSERT_EQUAL(1, event_count);
}

void test_short_hub_status_rearms(void) {
  configure_hub(HUB_A, 1);
  hub_status_response_t resp = {0};
  resp.change.over_current   = 1;

  for (uint8_t len = 0; len < sizeof(resp); len++) {
    poll_buf[0] = TU_BIT(0);
    TEST_ASSERT_TRUE(hub_xfer_cb(HUB_A, EP_IN, XFER_RESULT_SUCCESS, 1));
    TEST_ASSERT_EQUAL(len + 1, ctrl_count);
    assert_get_port_status(len, HUB_A, 0);
    memcpy(ctrl_reqs[len].xfer.buffer, &resp, sizeof(resp)); // what an earlier reply left in ctrl_buf
    const uint8_t polls = poll_count;
    complete_ctrl_data(len, &resp, len);

    TEST_ASSERT_EQUAL(len + 1, ctrl_count); // no CLEAR_FEATURE
    TEST_ASSERT_EQUAL(polls + 1, poll_count);
  }
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

//--------------------------------------------------------------------+
// Multi-TT hub configuration
//--------------------------------------------------------------------+
// SET_INTERFACE(1) ends with result; the rest of SET_CONFIG succeeds
static void configure_mtt_hub(xfer_result_t result) {
  open_mtt_hub(HUB_A);
  TEST_ASSERT_TRUE(hub_tt_multi(HUB_A));
  TEST_ASSERT_TRUE(hub_set_config(HUB_A, 0));
  TEST_ASSERT_EQUAL(1, ctrl_count);
  TEST_ASSERT_EQUAL(TUSB_REQ_SET_INTERFACE, ctrl_reqs[0].setup.bRequest);
  TEST_ASSERT_EQUAL(1, ctrl_reqs[0].setup.wValue);
  complete_ctrl(0, result);

  TEST_ASSERT_EQUAL(2, ctrl_count);
  TEST_ASSERT_EQUAL(HUB_REQUEST_GET_DESCRIPTOR, ctrl_reqs[1].setup.bRequest);
  const hub_desc_cs_t desc_hub = {.bLength = sizeof(hub_desc_cs_t), .bNbrPorts = 2};
  complete_ctrl_data(1, &desc_hub, sizeof(desc_hub));
  complete_ctrl(2, XFER_RESULT_SUCCESS);
  complete_ctrl(3, XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL(1, config_complete_count);
  TEST_ASSERT_EQUAL(1, poll_count);
}

void test_mtt_hub_runs_multi_tt_after_set_interface(void) {
  configure_mtt_hub(XFER_RESULT_SUCCESS);
  TEST_ASSERT_TRUE(hub_tt_multi(HUB_A));
}

// usbh holds every other enumeration until the hub completes SET_CONFIG: a failed SET_INTERFACE must not stop it
void test_mtt_hub_failed_set_interface_completes_config_as_single_tt(void) {
  const xfer_result_t failures[] = {XFER_RESULT_STALLED, XFER_RESULT_TIMEOUT, XFER_RESULT_FAILED};
  for (uint8_t i = 0; i < TU_ARRAY_SIZE(failures); i++) {
    setUp();
    configure_mtt_hub(failures[i]);
    TEST_ASSERT_FALSE(hub_tt_multi(HUB_A));
  }
}
