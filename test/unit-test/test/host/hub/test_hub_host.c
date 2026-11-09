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
  (void) desc_ep;
  return true;
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

// a multi-TT hub: alternate setting 0 single-TT, 1 multi-TT, each with the status endpoint
static void open_mtt_hub(uint8_t daddr) {
  const tusb_desc_endpoint_t ep = {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = EP_IN,
    .bmAttributes     = {.xfer = TUSB_XFER_INTERRUPT},
    .wMaxPacketSize   = 1,
    .bInterval        = 12,
  };
  const struct TU_ATTR_PACKED {
    tusb_desc_interface_t itf0;
    tusb_desc_endpoint_t  ep0;
    tusb_desc_interface_t itf1;
    tusb_desc_endpoint_t  ep1;
  } desc = {
    .itf0 = {
      .bLength            = sizeof(tusb_desc_interface_t),
      .bDescriptorType    = TUSB_DESC_INTERFACE,
      .bNumEndpoints      = 1,
      .bInterfaceClass    = TUSB_CLASS_HUB,
      .bInterfaceProtocol = HUB_PROTOCOL_HIGH_SPEED_STT,
    },
    .ep0  = ep,
    .itf1 = {
      .bLength            = sizeof(tusb_desc_interface_t),
      .bDescriptorType    = TUSB_DESC_INTERFACE,
      .bAlternateSetting  = 1,
      .bNumEndpoints      = 1,
      .bInterfaceClass    = TUSB_CLASS_HUB,
      .bInterfaceProtocol = HUB_PROTOCOL_HIGH_SPEED_MTT,
    },
    .ep1  = ep,
  };
  dev_mtt = true;
  TEST_ASSERT_EQUAL(sizeof(desc), hub_open(0, daddr, &desc.itf0, sizeof(desc)));
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
  ctrl_count  = 0;
  ctrl_reject = false;
  cb_count    = 0;
  poll_count  = 0;
  poll_buf    = NULL;
  event_count = 0;
  dev_mtt     = false;
  config_complete_count = 0;
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

//--------------------------------------------------------------------+
// Split ISO slot allocation
//--------------------------------------------------------------------+
enum {
  NO_SLOT = 0xff
};

static uint8_t iso_slot(uint8_t used, uint16_t packet_size, uint8_t dir) {
  uint8_t slot = NO_SLOT;
  const bool ok = hub_iso_split_slot(used, packet_size, dir, &slot);
  TEST_ASSERT_EQUAL(ok, slot != NO_SLOT);
  return slot;
}

void test_iso_split_slot_out_grows_from_slot0(void) {
  TEST_ASSERT_EQUAL(0, iso_slot(0x00, 1, TUSB_DIR_OUT));
  TEST_ASSERT_EQUAL(1, iso_slot(0x01, 188, TUSB_DIR_OUT));
  TEST_ASSERT_EQUAL(5, iso_slot(0x1f, 188, TUSB_DIR_OUT));
  TEST_ASSERT_EQUAL(0, iso_slot(0x00, 189, TUSB_DIR_OUT)); // 2 slots
  TEST_ASSERT_EQUAL(2, iso_slot(0x02, 376, TUSB_DIR_OUT)); // first free 2-slot window
  TEST_ASSERT_EQUAL(3, iso_slot(0x04, 564, TUSB_DIR_OUT)); // 3 slots
  TEST_ASSERT_EQUAL(0, iso_slot(0x00, 1023, TUSB_DIR_OUT)); // 6 slots: 0..5
  TEST_ASSERT_EQUAL(0, iso_slot(0xc0, 1023, TUSB_DIR_OUT)); // slots 6, 7 are not used
}

void test_iso_split_slot_in_grows_from_slot3(void) {
  TEST_ASSERT_EQUAL(3, iso_slot(0x00, 1, TUSB_DIR_IN));
  TEST_ASSERT_EQUAL(2, iso_slot(0x08, 188, TUSB_DIR_IN));
  TEST_ASSERT_EQUAL(2, iso_slot(0x00, 189, TUSB_DIR_IN)); // 2 slots: 2..3
  TEST_ASSERT_EQUAL(1, iso_slot(0x08, 376, TUSB_DIR_IN));
  TEST_ASSERT_EQUAL(1, iso_slot(0x00, 564, TUSB_DIR_IN)); // 3 slots: 1..3
  TEST_ASSERT_EQUAL(0, iso_slot(0x08, 564, TUSB_DIR_IN));
  TEST_ASSERT_EQUAL(1, iso_slot(0x30, 564, TUSB_DIR_IN)); // slots 4, 5 are not used
}

void test_iso_split_slot_full_window_fails(void) {
  TEST_ASSERT_EQUAL(NO_SLOT, iso_slot(0x3f, 1, TUSB_DIR_OUT));
  TEST_ASSERT_EQUAL(NO_SLOT, iso_slot(0x20, 1023, TUSB_DIR_OUT));
  TEST_ASSERT_EQUAL(NO_SLOT, iso_slot(0x15, 189, TUSB_DIR_OUT)); // no two adjacent free slots in 0..5
  TEST_ASSERT_EQUAL(NO_SLOT, iso_slot(0x0f, 1, TUSB_DIR_IN));
  TEST_ASSERT_EQUAL(NO_SLOT, iso_slot(0x05, 376, TUSB_DIR_IN));
  TEST_ASSERT_EQUAL(NO_SLOT, iso_slot(0x02, 564, TUSB_DIR_IN));
}
