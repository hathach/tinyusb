/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// Route usbh.c's endpoint HCD calls to this suite: open and abort are counted below, close lands on usbh.c's weak
// default, which fails, so a successful tuh_edpt_close() proves it made no HCD call.
#define hcd_edpt_open       counted_hcd_edpt_open
#define hcd_edpt_abort_xfer counted_hcd_edpt_abort_xfer
#define hcd_edpt_close      weak_hcd_edpt_close

// Include the production implementation to build a device tree and drive remove_device_tree() directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("hub.c")
TEST_SOURCE_FILE("usbh_remove_tree_callbacks.c")
TEST_SOURCE_FILE("usbh_test_hcd_stubs.c")

enum {
  RHPORT = 0,
  DEV_A  = 1,
  DEV_B  = 2,
  DEV_D  = 3,
  HUB_1  = CFG_TUH_DEVICE_MAX + 1,
  HUB_2  = CFG_TUH_DEVICE_MAX + 2,
};

TU_VERIFY_STATIC(CFG_TUH_DEVICE_MAX == 3 && CFG_TUH_HUB == 2, "tree layout needs 3 devices and 2 hubs");

static uint8_t umount_count;
static uint8_t first_umount_daddr;
static bool    connected_at_first_umount[TOTAL_DEVICES + 1];

// Order of control callbacks, unmount and class close, each as (kind, value)
enum {
  EV_XFER   = 1, // value: user_data of the completed request
  EV_UMOUNT = 2, // value: daddr
  EV_CLOSE  = 3, // value: daddr
};
typedef struct {
  uint8_t       kind;
  uintptr_t     value;
  xfer_result_t result;
} event_t;
static event_t events[16];
static uint8_t event_count;

static void log_event(uint8_t kind, uintptr_t value, xfer_result_t result) {
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(events), event_count);
  events[event_count++] = (event_t){.kind = kind, .value = value, .result = result};
}

static void assert_event(uint8_t idx, uint8_t kind, uintptr_t value) {
  TEST_ASSERT_LESS_THAN(event_count, idx);
  TEST_ASSERT_EQUAL(kind, events[idx].kind);
  TEST_ASSERT_EQUAL(value, events[idx].value);
  if (kind == EV_XFER) {
    TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, events[idx].result);
  }
}

static const tusb_desc_endpoint_t bulk_in_desc = {
  .bLength          = sizeof(tusb_desc_endpoint_t),
  .bDescriptorType  = TUSB_DESC_ENDPOINT,
  .bEndpointAddress = 0x81,
  .bmAttributes     = {.xfer = TUSB_XFER_BULK},
  .wMaxPacketSize   = 64,
  .bInterval        = 0};

static uint8_t hcd_edpt_open_count;
static uint8_t hcd_edpt_abort_count;

// tuh_umount_cb() is where an application may drive tuh_task(): snapshot what it would see there
static bool umount_xfer_tried;
static bool umount_xfer_ok;
static bool umount_edpt_open_tried;
static bool umount_edpt_open_ok;
static bool umount_edpt_close_tried;
static bool umount_edpt_close_ok;
static uint8_t umount_ep_state_after_close;
static bool umount_edpt_abort_tried;
static bool umount_edpt_abort_ok;
static uint8_t umount_ep_state_after_abort;
static uint8_t peer_close_daddr; // closed from the first tuh_umount_cb(), before its own hcd_device_close()
static bool    peer_close_ok;
static uint8_t peer_ep_state_after_close;
static uint8_t peer_abort_count;

void remove_tree_umount_observer(uint8_t daddr) {
  log_event(EV_UMOUNT, daddr, XFER_RESULT_INVALID);
  if (umount_xfer_tried) {
    static uint8_t buf[8];
    umount_xfer_ok = usbh_edpt_xfer(daddr, 0x81, buf, sizeof(buf));
  }
  if (umount_edpt_open_tried) {
    umount_edpt_open_ok = tuh_edpt_open(daddr, &bulk_in_desc);
  }
  if (umount_edpt_close_tried) {
    umount_edpt_close_ok        = tuh_edpt_close(daddr, 0x81);
    umount_ep_state_after_close = get_device(daddr)->ep_status[1][TUSB_DIR_IN];
  }
  if (umount_edpt_abort_tried) {
    umount_edpt_abort_ok        = tuh_edpt_abort_xfer(daddr, 0x81);
    umount_ep_state_after_abort = get_device(daddr)->ep_status[1][TUSB_DIR_IN];
  }
  if (umount_count == 0) {
    if (peer_close_daddr != 0) {
      peer_close_ok             = tuh_edpt_close(peer_close_daddr, 0x81);
      peer_ep_state_after_close = get_device(peer_close_daddr)->ep_status[1][TUSB_DIR_IN];
      peer_abort_count          = hcd_edpt_abort_count;
    }
    first_umount_daddr = daddr;
    for (uint8_t addr = 1; addr <= TOTAL_DEVICES; addr++) {
      connected_at_first_umount[addr] = tuh_connected(addr);
    }
  }
  umount_count++;
}

//--------------------------------------------------------------------+
// HCD stubs
//--------------------------------------------------------------------+
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) daddr;
  (void) setup_packet;
  return true;
}


bool counted_hcd_edpt_open(uint8_t rhport, uint8_t daddr, const tusb_desc_endpoint_t *ep_desc) {
  (void) rhport;
  (void) daddr;
  (void) ep_desc;
  hcd_edpt_open_count++;
  return true;
}

bool counted_hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  hcd_edpt_abort_count++;
  return true;
}

//--------------------------------------------------------------------+
// Class driver observing its close
//--------------------------------------------------------------------+
static uint8_t close_daddr_filter; // only this device's close is logged; the hub driver closes every daddr too

static void fake_close(uint8_t daddr) {
  if (daddr == close_daddr_filter) {
    log_event(EV_CLOSE, daddr, XFER_RESULT_INVALID);
  }
}

static const usbh_class_driver_t fake_driver = {.name = "fake", .close = fake_close};

//--------------------------------------------------------------------+
// Control requests
//--------------------------------------------------------------------+
static const tusb_control_request_t get_status = {
  .bmRequestType = 0x80, .bRequest = TUSB_REQ_GET_STATUS, .wValue = 0, .wIndex = 0, .wLength = 0};

typedef void (*xfer_hook_t)(const tuh_xfer_t *xfer);
static xfer_hook_t xfer_hook; // run from the next completion callback, then cleared

static void xfer_logger(tuh_xfer_t *xfer) {
  log_event(EV_XFER, xfer->user_data, xfer->result);
  xfer_hook_t hook = xfer_hook;
  xfer_hook        = NULL;
  if (hook != NULL) {
    hook(xfer);
  }
}

static bool submit(uint8_t daddr, uintptr_t tag) {
  tuh_xfer_t xfer = {
    .daddr = daddr, .ep_addr = 0, .setup = &get_status, .buffer = NULL, .complete_cb = xfer_logger, .user_data = tag};
  return tuh_control_xfer(&xfer);
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void attach(uint8_t daddr, uint8_t hub_addr, uint8_t hub_port) {
  usbh_device_t *dev     = get_device(daddr);
  dev->bus_info.rhport   = RHPORT;
  dev->bus_info.hub_addr = hub_addr;
  dev->bus_info.hub_port = hub_port;
  dev->connected         = 1;
  dev->addressed         = 1;
  dev->configured        = 1;
  dev->mounted           = 1;
}

static void assert_removed(uint8_t daddr) {
  const usbh_device_t *dev = get_device(daddr);
  TEST_ASSERT_FALSE(dev->connected);
  TEST_ASSERT_FALSE(dev->removing);
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
void setUp(void) {
  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
    clear_device(&_usbh_devices[i]);
  }
  _app_driver       = &fake_driver;
  _app_driver_count = 1;
  close_daddr_filter = 0;
  tu_fifo_clear(&_usbh_pending_ctrl_q);
  _usbh_controller_id = TUSB_INDEX_INVALID_8;
  event_count = 0;
  xfer_hook   = NULL;
  umount_xfer_tried = false;
  umount_xfer_ok    = false;
  umount_edpt_open_tried  = false;
  umount_edpt_open_ok     = false;
  umount_edpt_close_tried = false;
  umount_edpt_close_ok    = false;
  umount_ep_state_after_close = 0xFF;
  umount_edpt_abort_tried     = false;
  umount_edpt_abort_ok        = false;
  umount_ep_state_after_abort = 0xFF;
  peer_close_daddr          = 0;
  peer_close_ok             = true;
  peer_ep_state_after_close = 0xFF;
  peer_abort_count          = 0;
  hcd_edpt_open_count  = 0;
  hcd_edpt_abort_count = 0;
  _usbh_data.enumerating_daddr = TUSB_INDEX_INVALID_8;

  umount_count       = 0;
  first_umount_daddr = 0;
  tu_memclr(connected_at_first_umount, sizeof(connected_at_first_umount));
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
// unplugging a root hub: B is torn down after A, but must already read unplugged in A's unmount callback
void test_remove_root_hub_marks_siblings_before_umount_cb(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  attach(DEV_B, HUB_1, 2);

  remove_device_tree(RHPORT, 0, 0);

  TEST_ASSERT_EQUAL(2, umount_count);
  TEST_ASSERT_EQUAL(DEV_A, first_umount_daddr);
  TEST_ASSERT_FALSE(connected_at_first_umount[DEV_B]);
  TEST_ASSERT_FALSE(connected_at_first_umount[HUB_1]);

  assert_removed(DEV_A);
  assert_removed(DEV_B);
  assert_removed(HUB_1);
}

// unplugging a downstream hub: the devices behind it are reached on a later pass, so they must be marked up front,
// while the rest of the tree stays connected
void test_remove_nested_hub_marks_subtree_only(void) {
  attach(HUB_1, 0, 0);
  attach(HUB_2, HUB_1, 1);
  attach(DEV_D, HUB_1, 2);
  attach(DEV_A, HUB_2, 1);
  attach(DEV_B, HUB_2, 2);

  remove_device_tree(RHPORT, HUB_1, 1);

  TEST_ASSERT_EQUAL(2, umount_count);
  TEST_ASSERT_EQUAL(DEV_A, first_umount_daddr);
  TEST_ASSERT_FALSE(connected_at_first_umount[DEV_B]);
  TEST_ASSERT_FALSE(connected_at_first_umount[HUB_2]);
  TEST_ASSERT_TRUE(connected_at_first_umount[DEV_D]);
  TEST_ASSERT_TRUE(connected_at_first_umount[HUB_1]);

  assert_removed(DEV_A);
  assert_removed(DEV_B);
  assert_removed(HUB_2);
  TEST_ASSERT_TRUE(tuh_connected(DEV_D));
  TEST_ASSERT_TRUE(tuh_connected(HUB_1));
}

// Every control request of the unplugged device fails, in flight first then queued in submit order, before its
// tuh_umount_cb() and class close; another device's queued requests keep their order and still dispatch.
void test_remove_fails_device_control_xfers_before_umount_and_close(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  attach(DEV_B, HUB_1, 2);
  close_daddr_filter = DEV_A;

  TEST_ASSERT_TRUE(submit(DEV_A, 0xA1)); // in flight
  TEST_ASSERT_TRUE(submit(DEV_A, 0xA2));
  TEST_ASSERT_TRUE(submit(DEV_B, 0xB1));
  TEST_ASSERT_TRUE(submit(DEV_A, 0xA3));
  TEST_ASSERT_TRUE(submit(DEV_B, 0xB2));

  remove_device_tree(RHPORT, HUB_1, 1);

  TEST_ASSERT_EQUAL(5, event_count);
  assert_event(0, EV_XFER, 0xA1);
  assert_event(1, EV_XFER, 0xA2);
  assert_event(2, EV_XFER, 0xA3);
  assert_event(3, EV_UMOUNT, DEV_A);
  assert_event(4, EV_CLOSE, DEV_A);
  assert_removed(DEV_A);

  TEST_ASSERT_EQUAL(2, control_xfer_pending_count());
  control_xfer_dispatch_pending();
  TEST_ASSERT_EQUAL(DEV_B, _usbh_data.ctrl_xfer_info.daddr);
  TEST_ASSERT_EQUAL(0xB1, _usbh_data.ctrl_xfer_info.user_data);
  TEST_ASSERT_EQUAL(1, control_xfer_pending_count());
}

// Another device's transfer in flight is left untouched while the unplugged device's queued requests fail
void test_remove_keeps_other_device_xfer_in_flight(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  attach(DEV_B, HUB_1, 2);

  TEST_ASSERT_TRUE(submit(DEV_B, 0xB1)); // in flight
  TEST_ASSERT_TRUE(submit(DEV_A, 0xA1));
  TEST_ASSERT_TRUE(submit(DEV_A, 0xA2));

  remove_device_tree(RHPORT, HUB_1, 1);

  TEST_ASSERT_EQUAL(3, event_count);
  assert_event(0, EV_XFER, 0xA1);
  assert_event(1, EV_XFER, 0xA2);
  assert_event(2, EV_UMOUNT, DEV_A);
  TEST_ASSERT_EQUAL(CONTROL_STAGE_SETUP, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_EQUAL(DEV_B, _usbh_data.ctrl_xfer_info.daddr);
  TEST_ASSERT_EQUAL(0xB1, _usbh_data.ctrl_xfer_info.user_data);
  TEST_ASSERT_EQUAL(0, control_xfer_pending_count());
}

static bool resubmit_a_ok;
static bool submit_b_ok;
static bool sync_b_ok;

static void resubmit_from_cancel(const tuh_xfer_t *xfer) {
  (void) xfer;
  resubmit_a_ok = submit(DEV_A, 0xA9);
  submit_b_ok   = submit(DEV_B, 0xB9);
  tuh_xfer_t sync_xfer = {.daddr = DEV_B, .ep_addr = 0, .setup = &get_status, .complete_cb = NULL};
  sync_b_ok = tuh_control_xfer(&sync_xfer);
}

// A cancel callback cannot queue for the closed device, nor wait on a blocking request; another device's request
// takes the freed slot
void test_remove_cancel_callback_resubmit(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  attach(DEV_B, HUB_1, 2);

  TEST_ASSERT_TRUE(submit(DEV_A, 0xA1)); // in flight
  TEST_ASSERT_TRUE(submit(DEV_A, 0xA2));
  resubmit_a_ok = true;
  xfer_hook     = resubmit_from_cancel;

  remove_device_tree(RHPORT, HUB_1, 1);

  TEST_ASSERT_FALSE(resubmit_a_ok);
  TEST_ASSERT_TRUE(submit_b_ok);
  TEST_ASSERT_FALSE(sync_b_ok);
  assert_event(0, EV_XFER, 0xA1);
  assert_event(1, EV_XFER, 0xA2);
  TEST_ASSERT_EQUAL(DEV_B, _usbh_data.ctrl_xfer_info.daddr);
  TEST_ASSERT_EQUAL(0xB9, _usbh_data.ctrl_xfer_info.user_data);
  TEST_ASSERT_EQUAL(0, control_xfer_pending_count());
}

static void drive_task_from_cancel(const tuh_xfer_t *xfer) {
  (void) xfer;
  (void) tuh_task_ext(0, false);
}

// A cancel callback driving tuh_task() dispatches nothing: the queued request still fails through the cancel
void test_remove_cancel_callback_drives_task(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  attach(DEV_B, HUB_1, 2);
  _usbh_controller_id = RHPORT; // tuh_inited(), so tuh_task() would run

  TEST_ASSERT_TRUE(submit(DEV_A, 0xA1)); // in flight
  TEST_ASSERT_TRUE(submit(DEV_A, 0xA2));
  TEST_ASSERT_TRUE(submit(DEV_B, 0xB1));
  xfer_hook = drive_task_from_cancel;

  remove_device_tree(RHPORT, HUB_1, 1);
  _usbh_controller_id = TUSB_INDEX_INVALID_8;

  assert_event(0, EV_XFER, 0xA1);
  assert_event(1, EV_XFER, 0xA2);
  TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_EQUAL(1, control_xfer_pending_count());
}

// dev0 unplugged while enumerating: its queued request fails with the one in flight
void test_remove_dev0_fails_queued_xfers(void) {
  _usbh_data.enumerating_daddr = 0;
  _usbh_data.dev0_bus.rhport   = RHPORT;

  TEST_ASSERT_TRUE(submit(0, 0x01)); // in flight
  TEST_ASSERT_TRUE(submit(0, 0x02));

  hcd_event_t event = {.rhport = RHPORT, .event_id = HCD_EVENT_DEVICE_REMOVE};
  process_remove_event(&event);

  TEST_ASSERT_EQUAL(2, event_count);
  assert_event(0, EV_XFER, 0x01);
  assert_event(1, EV_XFER, 0x02);
  TEST_ASSERT_EQUAL(0, control_xfer_pending_count());
  TEST_ASSERT_FALSE(tuh_connected(0));
}

// The HCD closed the device before tuh_umount_cb(): a transfer started from it, e.g. a class advancing a command whose
// completion the callback delivered, is refused and its claim ended
void test_remove_refuses_endpoint_xfer_from_umount_cb(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  umount_xfer_tried = true;
  get_device(DEV_A)->ep_status[1][TUSB_DIR_IN] = TU_EDPT_STATE_CLAIMED;

  remove_device_tree(RHPORT, HUB_1, 1);

  TEST_ASSERT_FALSE(umount_xfer_ok);
  assert_removed(DEV_A);
}

static bool cancel_edpt_open_ok;

static void open_from_cancel(const tuh_xfer_t *xfer) {
  cancel_edpt_open_ok = tuh_edpt_open(xfer->daddr, &bulk_in_desc);
}

// The HCD closed the device before its control callbacks and tuh_umount_cb(): an endpoint opened from either, e.g. a
// class reconfiguring after a failed request, would never be freed, so it is refused before the HCD
void test_remove_refuses_edpt_open_from_cancel_and_umount_cb(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  TEST_ASSERT_TRUE(tuh_edpt_open(DEV_A, &bulk_in_desc));
  TEST_ASSERT_EQUAL(1, hcd_edpt_open_count);

  TEST_ASSERT_TRUE(submit(DEV_A, 0xA1)); // in flight
  cancel_edpt_open_ok    = true;
  xfer_hook              = open_from_cancel;
  umount_edpt_open_tried = true;
  umount_edpt_open_ok    = true;

  remove_device_tree(RHPORT, HUB_1, 1);

  assert_event(0, EV_XFER, 0xA1);
  TEST_ASSERT_FALSE(cancel_edpt_open_ok);
  TEST_ASSERT_FALSE(umount_edpt_open_ok);
  TEST_ASSERT_EQUAL(1, hcd_edpt_open_count);
  assert_removed(DEV_A);
}

// tuh_edpt_close() from tuh_umount_cb() releases usbh's endpoint state and succeeds without reaching the HCD, which
// already closed the endpoint in hcd_device_close()
void test_remove_edpt_close_from_umount_cb_skips_hcd(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  get_device(DEV_A)->ep_status[1][TUSB_DIR_IN] = TU_EDPT_STATE_CLAIMED | TU_EDPT_STATE_BUSY;
  umount_edpt_close_tried = true;

  remove_device_tree(RHPORT, HUB_1, 1);

  TEST_ASSERT_TRUE(umount_edpt_close_ok);
  TEST_ASSERT_EQUAL(0, umount_ep_state_after_close);
  TEST_ASSERT_EQUAL(0, hcd_edpt_abort_count);
  assert_removed(DEV_A);
}

// tuh_edpt_abort_xfer() from tuh_umount_cb() on a busy endpoint releases it without reaching the HCD: hcd_device_close()
// already dropped the endpoint and its transfer, and an HCD such as stm32_fsdev asserts on the failed lookup
void test_remove_edpt_abort_from_umount_cb_skips_hcd(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  get_device(DEV_A)->ep_status[1][TUSB_DIR_IN] = TU_EDPT_STATE_CLAIMED | TU_EDPT_STATE_BUSY;
  umount_edpt_abort_tried = true;

  remove_device_tree(RHPORT, HUB_1, 1);

  TEST_ASSERT_TRUE(umount_edpt_abort_ok);
  TEST_ASSERT_EQUAL(0, umount_ep_state_after_abort);
  TEST_ASSERT_EQUAL(0, hcd_edpt_abort_count);
  assert_removed(DEV_A);
}

// A sibling marked for removal but not yet closed in the HCD: tuh_edpt_close() on it from another device's
// tuh_umount_cb() must abort and close in the HCD, or the controller could still fill a buffer the caller released
void test_remove_edpt_close_of_unclosed_sibling_reaches_hcd(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  attach(DEV_B, HUB_1, 2);
  get_device(DEV_B)->ep_status[1][TUSB_DIR_IN] = TU_EDPT_STATE_CLAIMED | TU_EDPT_STATE_BUSY;
  peer_close_daddr = DEV_B;

  remove_device_tree(RHPORT, 0, 0);

  TEST_ASSERT_EQUAL(DEV_A, first_umount_daddr);
  TEST_ASSERT_EQUAL(1, peer_abort_count);
  TEST_ASSERT_EQUAL(0, peer_ep_state_after_close);
  TEST_ASSERT_FALSE(peer_close_ok); // reached usbh.c's weak hcd_edpt_close(), which fails
  assert_removed(DEV_A);
  assert_removed(DEV_B);
}
