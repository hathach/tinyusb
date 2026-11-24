/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// count TU_ASSERT() hits in usbh.c and hub.c, both built into this unit
#define CFG_TUSB_DEBUG_BREAKPOINT enum_hub_breakpoint_hook

// Include the production implementation to drive process_enumeration() directly.
#include "host/usbh.c"
#include "host/hub.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("usbh_test_hcd_stubs.c")

enum {
  RHPORT    = 0,
  HUB_ADDR  = CFG_TUH_DEVICE_MAX + 1,
  HUB_PORT  = 1,
  HUB_EP_IN = 0x81,
};

static uint8_t                setup_count;
static uint8_t                last_setup_daddr;
static tusb_control_request_t last_setup;
static bool                   setup_send_fails;
static uint8_t                breakpoint_count;

void enum_hub_breakpoint_hook(void) {
  breakpoint_count++;
}

//--------------------------------------------------------------------+
// HCD stubs
//--------------------------------------------------------------------+
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  last_setup_daddr = daddr;
  memcpy(&last_setup, setup_packet, sizeof(last_setup));
  setup_count++;
  return !setup_send_fails;
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void noop_cb(tuh_xfer_t *xfer) {
  (void) xfer;
}

static bool hub_status_polled(void) {
  return usbh_edpt_busy(HUB_ADDR, HUB_EP_IN);
}

// Leave a port status in the hub's ctrl_buf as an earlier GET_STATUS would, then free the control slot
static void stage_port_status_speed(bool connected, bool reset_changed, bool high_speed) {
  TEST_ASSERT_TRUE(hub_port_get_status(HUB_ADDR, HUB_PORT, NULL, noop_cb, 0));
  hub_port_status_response_t *status = (hub_port_status_response_t *) _usbh_data.ctrl_xfer_info.buffer;
  TEST_ASSERT_NOT_NULL(status);
  tu_memclr(status, sizeof(*status));
  status->status.connection = connected ? 1 : 0;
  status->status.high_speed = high_speed ? 1 : 0;
  status->change.reset      = reset_changed ? 1 : 0;

  tu_memclr(&_usbh_data.ctrl_xfer_info, sizeof(_usbh_data.ctrl_xfer_info));
  setup_count = 0;
}

static void stage_port_status(bool connected, bool reset_changed) {
  stage_port_status_speed(connected, reset_changed, false);
}

// a failed child left in the Default state would answer address 0 with the next child: its port is disabled, the
// only request sent (no PORT_RESET on a failure)
static void assert_port_disabled(void) {
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL(1, setup_count);
  TEST_ASSERT_EQUAL(HUB_ADDR, last_setup_daddr);
  TEST_ASSERT_EQUAL(HUB_REQUEST_CLEAR_FEATURE, last_setup.bRequest);
  TEST_ASSERT_EQUAL(HUB_FEATURE_PORT_ENABLE, tu_le16toh(last_setup.wValue));
  TEST_ASSERT_EQUAL(HUB_PORT, tu_le16toh(last_setup.wIndex));
  TEST_ASSERT_FALSE(hub_status_polled()); // re-armed once the port is disabled
}

// the hub's GET_STATUS reply for the port being enumerated completes with actual_len bytes
static void complete_port_status(uintptr_t state, uint32_t actual_len) {
  tuh_xfer_t xfer = {
    .daddr      = 0,
    .ep_addr    = 0,
    .result     = XFER_RESULT_SUCCESS,
    .actual_len = actual_len,
    .user_data  = state,
  };
  process_enumeration(&xfer);
}

enum {
  CHILD_ADDR = 1, // the only device address (CFG_TUH_DEVICE_MAX = 1)
};

// the child on HUB_PORT took CHILD_ADDR, then failed a request with every retry spent
static void fail_addressed_child(void) {
  usbh_device_t *child = get_device(CHILD_ADDR);
  child->bus_info      = _usbh_data.dev0_bus;
  child->connected     = 1;
  child->addressed     = 1;

  _usbh_data.enumerating_daddr = CHILD_ADDR;
  _usbh_data.enum_attempt      = CFG_TUH_ENUM_ATTEMPT_MAX - 1;
  _usbh_data.enum_xfer_retry   = ENUM_XFER_ATTEMPT_MAX - 1;

  tuh_xfer_t xfer = {.daddr = CHILD_ADDR, .result = XFER_RESULT_FAILED, .user_data = ENUM_GET_STRING_LANGUAGE_ID_LEN};
  process_enumeration(&xfer);
}

static bool child_addr_free(void) {
  return enum_get_new_address(false) == CHILD_ADDR;
}

// the hub reports a child connected on hub_port, as hub.c posts it
static void child_attached(uint8_t hub_port) {
  hcd_event_t attach = {
    .rhport = RHPORT, .event_id = HCD_EVENT_DEVICE_ATTACH, .connection = {.hub_addr = HUB_ADDR, .hub_port = hub_port}
  };
  TEST_ASSERT_TRUE(usbh_process_event(&attach, false));
}

// the child on HUB_PORT, highspeed on a fullspeed hub, fails at address 0, with its port disable refused at submit or not
static void child_fails_at_dev0(bool disable_send_fails) {
  get_device(HUB_ADDR)->bus_info.speed = TUSB_SPEED_FULL;
  stage_port_status_speed(true, false, true);
  setup_send_fails = disable_send_fails;
  complete_port_status(ENUM_HUB_CLEAR_RESET_COMPLETE, sizeof(hub_port_status_response_t));
}

// the child on HUB_PORT failed at address 0: its port disable is in flight
static void fail_child_at_dev0(void) {
  child_fails_at_dev0(false);
  assert_port_disabled();
}

// the port disable was sent count times in all, the last still awaited or waiting to be resent: the hub is not polled
static void assert_port_disable_sent(uint8_t count) {
  TEST_ASSERT_EQUAL(count, setup_count);
  TEST_ASSERT_EQUAL(HUB_ADDR, last_setup_daddr);
  TEST_ASSERT_EQUAL(HUB_REQUEST_CLEAR_FEATURE, last_setup.bRequest);
  TEST_ASSERT_EQUAL(HUB_FEATURE_PORT_ENABLE, tu_le16toh(last_setup.wValue));
  TEST_ASSERT_EQUAL(HUB_PORT, tu_le16toh(last_setup.wIndex));
  TEST_ASSERT_FALSE(hub_status_polled());
}

// the child on HUB_PORT failed at address 0 and its port disable was refused at submit
static void fail_child_at_dev0_unsent(void) {
  child_fails_at_dev0(true);
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);
  assert_port_disable_sent(1);
}

// the child on HUB_PORT failed at address 0 and the hub answered the port disable with result
static void fail_child_port_disable(xfer_result_t result) {
  fail_child_at_dev0();
  control_xfer_complete(HUB_ADDR, result);
}

// while the failed child's port disable is in flight the next child, attached on HUB_PORT + 1, is held. No hub is polled
// for it: that would report more attaches than the deferred queue holds, and race the held child's port reset
static void assert_next_child_held(void) {
  child_attached(HUB_PORT + 1);
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
  TEST_ASSERT_FALSE(osal_queue_empty(_usbh_daq));
  TEST_ASSERT_FALSE(tuh_task_event_ready());
  TEST_ASSERT_FALSE(hub_status_polled());
}

// the next child attached on HUB_PORT + 1 enumerates right away
static void assert_next_child_enumerates(void) {
  child_attached(HUB_PORT + 1);
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL(HUB_PORT + 1, _usbh_data.dev0_bus.hub_port);
  TEST_ASSERT_TRUE(osal_queue_empty(_usbh_daq));
}

// the held attach runs as tuh_task() would run it
static void run_deferred_attach(void) {
  TEST_ASSERT_TRUE(tuh_task_event_ready());
  hcd_event_t event;
  TEST_ASSERT_TRUE(osal_queue_receive(_usbh_daq, &event, 0));
  TEST_ASSERT_TRUE(usbh_process_event(&event, false));
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL(HUB_PORT + 1, _usbh_data.dev0_bus.hub_port);
}

// a request to the failed child still in flight when its enumeration ends: its completion, run by the teardown,
// drives the usbh task, which starts the next child's enumeration
static void in_flight_cb_attaches_next_child(tuh_xfer_t *xfer) {
  (void) xfer;
  child_attached(HUB_PORT + 1);
}

// a second child on another port of the hub reaches SET_ADDRESS
static void second_child_set_address(uint8_t hub_port) {
  static const uint8_t desc_dev8[8] = {18, TUSB_DESC_DEVICE, U16_TO_U8S_LE(0x0200), 0, 0, 0, 64};
  _usbh_data.enumerating_daddr = 0;
  _usbh_data.dev0_bus.hub_port = hub_port;
  memcpy(_usbh_epbuf.ctrl, desc_dev8, sizeof(desc_dev8));
  setup_count = 0;

  tuh_xfer_t xfer = {
    .daddr = 0, .result = XFER_RESULT_SUCCESS, .actual_len = sizeof(desc_dev8), .user_data = ENUM_SET_ADDR
  };
  process_enumeration(&xfer);
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
void setUp(void) {
  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
    clear_device(&_usbh_devices[i]);
  }
  _app_driver       = NULL;
  _app_driver_count = 0;

  _usbh_q   = osal_queue_create(&_usbh_qdef);
  _usbh_daq = osal_queue_create(&_usbh_daqdef);
  _usbh_controller_id = RHPORT; // tuh_inited(), for tuh_task_event_ready()

  usbh_device_t *hub   = get_device(HUB_ADDR);
  hub->bus_info.rhport = RHPORT;
  hub->connected       = 1;
  hub->addressed       = 1;
  hub->configured      = 1;
  hub->mounted         = 1;

  // the hub's status endpoint is open and idle, as while it waits for this child's enumeration
  tu_memclr(hub_itfs, sizeof(hub_itfs));
  get_hub_itf(HUB_ADDR)->ep_in     = HUB_EP_IN;
  get_hub_itf(HUB_ADDR)->ep_in_mps = 1;

  // a device on the hub's port is being enumerated at address 0
  _usbh_data.enumerating_daddr = 0;
  _usbh_data.dev0_bus.rhport   = RHPORT;
  _usbh_data.dev0_bus.hub_addr = HUB_ADDR;
  _usbh_data.dev0_bus.hub_port = HUB_PORT;
  _usbh_data.dev0_bus.speed    = TUSB_SPEED_FULL;

  setup_count      = 0;
  last_setup_daddr = 0;
  tu_memclr(&last_setup, sizeof(last_setup));
  setup_send_fails = false;
  breakpoint_count = 0;
}

void tearDown(void) {
  _usbh_controller_id = TUSB_INDEX_INVALID_8;
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
// a short reply leaves a stale status in ctrl_buf: enumeration must fail instead of resetting the port on it
void test_hub_reset_short_status_fails(void) {
  for (uint32_t len = 0; len < sizeof(hub_port_status_response_t); len++) {
    setUp();
    stage_port_status(true, false);
    complete_port_status(ENUM_HUB_RERSET, len);

    assert_port_disabled();
  }
}

void test_hub_reset_full_status_resets_port(void) {
  stage_port_status(true, false);
  complete_port_status(ENUM_HUB_RERSET, sizeof(hub_port_status_response_t));

  TEST_ASSERT_EQUAL(1, setup_count);
  TEST_ASSERT_EQUAL(HUB_REQUEST_SET_FEATURE, last_setup.bRequest);
  TEST_ASSERT_EQUAL(HUB_FEATURE_PORT_RESET, tu_le16toh(last_setup.wValue));
  TEST_ASSERT_EQUAL(HUB_PORT, tu_le16toh(last_setup.wIndex));
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
}

void test_hub_clear_reset_short_status_fails(void) {
  const uintptr_t states[] = {ENUM_HUB_CLEAR_RESET, ENUM_HUB_CLEAR_RESET_RETRY};
  for (size_t i = 0; i < TU_ARRAY_SIZE(states); i++) {
    for (uint32_t len = 0; len < sizeof(hub_port_status_response_t); len++) {
      setUp();
      stage_port_status(true, true);
      complete_port_status(states[i], len);

      assert_port_disabled();
    }
  }
}

void test_hub_clear_reset_full_status_clears_change(void) {
  const uintptr_t states[] = {ENUM_HUB_CLEAR_RESET, ENUM_HUB_CLEAR_RESET_RETRY};
  for (size_t i = 0; i < TU_ARRAY_SIZE(states); i++) {
    setUp();
    stage_port_status(true, true);
    complete_port_status(states[i], sizeof(hub_port_status_response_t));

    TEST_ASSERT_EQUAL(1, setup_count);
    TEST_ASSERT_EQUAL(HUB_REQUEST_CLEAR_FEATURE, last_setup.bRequest);
    TEST_ASSERT_EQUAL(HUB_FEATURE_PORT_RESET_CHANGE, tu_le16toh(last_setup.wValue));
    TEST_ASSERT_EQUAL(HUB_PORT, tu_le16toh(last_setup.wIndex));
    TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
  }
}

// a fullspeed hub (always the case on a fullspeed host) cannot run a highspeed child: its endpoint sizes would exceed
// the buffers, which are sized to the host speed
void test_hub_highspeed_child_on_fullspeed_hub_fails(void) {
  get_device(HUB_ADDR)->bus_info.speed = TUSB_SPEED_FULL;
  stage_port_status_speed(true, false, true);
  complete_port_status(ENUM_HUB_CLEAR_RESET_COMPLETE, sizeof(hub_port_status_response_t));

  assert_port_disabled();
}

void test_hub_highspeed_child_on_highspeed_hub_enumerates(void) {
  get_device(HUB_ADDR)->bus_info.speed = TUSB_SPEED_HIGH;
  stage_port_status_speed(true, false, true);
  complete_port_status(ENUM_HUB_CLEAR_RESET_COMPLETE, sizeof(hub_port_status_response_t));

  TEST_ASSERT_EQUAL(TUSB_SPEED_HIGH, _usbh_data.dev0_bus.speed);
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
}

// every address taken: the child stays at address 0, so its port is disabled before the next child is reset
void test_hub_child_no_free_address_disables_port(void) {
  static const uint8_t desc_dev8[8] = {18, TUSB_DESC_DEVICE, U16_TO_U8S_LE(0x0200), 0, 0, 0, 64};
  for (uint8_t daddr = 1; daddr <= CFG_TUH_DEVICE_MAX; daddr++) {
    get_device(daddr)->connected = 1;
  }
  memcpy(_usbh_epbuf.ctrl, desc_dev8, sizeof(desc_dev8));

  tuh_xfer_t xfer = {
    .daddr = 0, .result = XFER_RESULT_SUCCESS, .actual_len = sizeof(desc_dev8), .user_data = ENUM_SET_ADDR
  };
  process_enumeration(&xfer);

  assert_port_disabled();
}

// the hub status poll resumes once the port is disabled
void test_hub_port_disabled_rearms_status_poll(void) {
  fail_child_port_disable(XFER_RESULT_SUCCESS);
  TEST_ASSERT_TRUE(hub_status_polled());
}

// the hub did not disable the port, so the failed child still answers address 0: the disable is sent again, the hub
// left unpolled and the next child held, until the hub disables the port
void test_hub_port_not_disabled_resends_disable(void) {
  const xfer_result_t results[] = {XFER_RESULT_STALLED, XFER_RESULT_FAILED, XFER_RESULT_TIMEOUT};
  for (size_t i = 0; i < TU_ARRAY_SIZE(results); i++) {
    setUp();
    fail_child_port_disable(results[i]);
    assert_port_disable_sent(2);
    assert_next_child_held();

    control_xfer_complete(HUB_ADDR, XFER_RESULT_SUCCESS);
    TEST_ASSERT_TRUE(hub_status_polled());
    run_deferred_attach();
    TEST_ASSERT_EQUAL(0, breakpoint_count);
  }
}

// the port disable is refused at every submit: once its attempts are spent the hub is polled again
void test_hub_port_disable_never_sent_polls_hub(void) {
  fail_child_at_dev0_unsent();
  for (uint8_t attempt = 2; attempt <= USBH_ENUM_PORT_DISABLE_ATTEMPT_MAX; attempt++) {
    TEST_ASSERT_TRUE(tuh_task_event_ready());
    (void) tuh_task_ext(0, false);
    assert_port_disable_sent(attempt);
    TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);
  }

  TEST_ASSERT_TRUE(tuh_task_event_ready());
  (void) tuh_task_ext(0, false);
  TEST_ASSERT_EQUAL(USBH_ENUM_PORT_DISABLE_ATTEMPT_MAX, setup_count);
  TEST_ASSERT_TRUE(hub_status_polled());
  TEST_ASSERT_FALSE(tuh_task_event_ready());
}

// the hub was unplugged (and closed) while its child debounced: the child fails with no request to the gone hub and no
// assert
void test_hub_removed_child_fails_without_request(void) {
  usbh_device_t *hub = get_device(HUB_ADDR);
  hub->connected     = 0;
  hub->configured    = 0;
  hub->mounted       = 0;
  hub_close(HUB_ADDR);

  enum_delay_async(ENUM_AFTER_DEBOUNCING_DELAY);

  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL(0, setup_count);
  TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_EQUAL(0, breakpoint_count);
}

// a failed child left enabled at its address answers alongside the next child given that address: its port is disabled,
// and the address held until it is
void test_hub_addressed_child_failure_disables_port(void) {
  fail_addressed_child();

  assert_port_disabled();
  TEST_ASSERT_FALSE(child_addr_free());
  TEST_ASSERT_FALSE(tuh_connected(CHILD_ADDR));

  control_xfer_complete(HUB_ADDR, XFER_RESULT_SUCCESS);
  TEST_ASSERT_TRUE(child_addr_free());
  TEST_ASSERT_TRUE(hub_status_polled());
}

// the hub did not disable the failed child's port: the next child on another port is not given its address, which is
// freed once the hub reports the failed child unplugged
void test_hub_addressed_child_port_not_disabled_keeps_address(void) {
  const xfer_result_t results[] = {XFER_RESULT_STALLED, XFER_RESULT_FAILED};
  for (size_t i = 0; i < TU_ARRAY_SIZE(results); i++) {
    setUp();
    fail_addressed_child();
    assert_port_disabled();

    control_xfer_complete(HUB_ADDR, results[i]);
    TEST_ASSERT_TRUE(hub_status_polled());
    TEST_ASSERT_FALSE(child_addr_free());

    second_child_set_address(HUB_PORT + 1);
    TEST_ASSERT_EQUAL(1, setup_count);
    TEST_ASSERT_EQUAL(HUB_ADDR, last_setup_daddr); // no SET_ADDRESS, the second child's port is disabled
    TEST_ASSERT_EQUAL(HUB_REQUEST_CLEAR_FEATURE, last_setup.bRequest);
    TEST_ASSERT_EQUAL(HUB_PORT + 1, tu_le16toh(last_setup.wIndex));
    tu_memclr(&_usbh_data.ctrl_xfer_info, sizeof(_usbh_data.ctrl_xfer_info));

    remove_device_tree(RHPORT, HUB_ADDR, HUB_PORT);
    TEST_ASSERT_TRUE(child_addr_free());
    TEST_ASSERT_EQUAL(0, breakpoint_count);
  }
}

// the port disable cannot be sent: the hub is polled right away, and the failed child keeps its address
void test_hub_addressed_child_port_disable_not_sent_keeps_address(void) {
  setup_send_fails = true;
  fail_addressed_child();

  TEST_ASSERT_EQUAL(1, setup_count);
  TEST_ASSERT_EQUAL(HUB_FEATURE_PORT_ENABLE, tu_le16toh(last_setup.wValue));
  TEST_ASSERT_TRUE(hub_status_polled());
  TEST_ASSERT_FALSE(child_addr_free());
}

// the hub was removed: its failed child is gone with it, so nothing is sent and the address is free
void test_hub_removed_addressed_child_frees_address(void) {
  get_device(HUB_ADDR)->connected = 0;
  fail_addressed_child();

  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL(0, setup_count);
  TEST_ASSERT_TRUE(child_addr_free());
}

// a child that failed at address 0 answers there until its port is disabled: the next child is not reset into address 0
// until the hub disabled the port
void test_hub_child_port_disable_holds_next_attach(void) {
  fail_child_at_dev0();
  assert_next_child_held();

  control_xfer_complete(HUB_ADDR, XFER_RESULT_SUCCESS);
  TEST_ASSERT_TRUE(hub_status_polled());
  run_deferred_attach();
  TEST_ASSERT_EQUAL(0, breakpoint_count);
}

// the hub never disables the port: after USBH_ENUM_PORT_DISABLE_ATTEMPT_MAX attempts it is polled again and the next
// child released, rather than every later attach held and the hub left unserviced until the failed child is unplugged
void test_hub_child_port_not_disabled_releases_next_attach(void) {
  const xfer_result_t results[] = {XFER_RESULT_STALLED, XFER_RESULT_FAILED, XFER_RESULT_TIMEOUT};
  for (size_t i = 0; i < TU_ARRAY_SIZE(results); i++) {
    setUp();
    fail_child_at_dev0();
    for (uint8_t attempt = 2; attempt <= USBH_ENUM_PORT_DISABLE_ATTEMPT_MAX; attempt++) {
      control_xfer_complete(HUB_ADDR, results[i]);
      assert_port_disable_sent(attempt);
    }
    assert_next_child_held();

    control_xfer_complete(HUB_ADDR, results[i]);
    TEST_ASSERT_EQUAL(USBH_ENUM_PORT_DISABLE_ATTEMPT_MAX, setup_count);
    TEST_ASSERT_TRUE(hub_status_polled());
    run_deferred_attach();
    TEST_ASSERT_EQUAL(0, breakpoint_count);
  }
}

// address 0 is per bus: the port disable on RHPORT does not hold a device attached to another root port
void test_hub_child_port_disable_holds_only_its_rhport(void) {
  fail_child_at_dev0();

  hcd_event_t attach = {
    .rhport = RHPORT + 1, .event_id = HCD_EVENT_DEVICE_ATTACH, .connection = {.hub_addr = 0, .hub_port = 0}
  };
  TEST_ASSERT_TRUE(usbh_process_event(&attach, false));
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL(RHPORT + 1, _usbh_data.dev0_bus.rhport);
  TEST_ASSERT_TRUE(osal_queue_empty(_usbh_daq));
}

// the port disable cannot be sent: the next child stays held while the usbh task sends it again
void test_hub_child_port_disable_not_sent_holds_next_attach(void) {
  fail_child_at_dev0_unsent();
  setup_send_fails = false;
  child_attached(HUB_PORT + 1);
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
  TEST_ASSERT_FALSE(osal_queue_empty(_usbh_daq));

  TEST_ASSERT_TRUE(tuh_task_event_ready());
  (void) tuh_task_ext(0, false);
  assert_port_disable_sent(2);
  TEST_ASSERT_FALSE(tuh_task_event_ready());

  control_xfer_complete(HUB_ADDR, XFER_RESULT_SUCCESS);
  TEST_ASSERT_TRUE(hub_status_polled());
  run_deferred_attach();
}

// the hub is removed while the port disable waits to be resent: nothing is sent to it, and the hold ends
void test_hub_removed_with_port_disable_unsent_releases_hold(void) {
  fail_child_at_dev0_unsent();
  setup_send_fails = false;

  remove_device_tree(RHPORT, 0, 0);
  TEST_ASSERT_TRUE(tuh_task_event_ready());
  (void) tuh_task_ext(0, false);
  TEST_ASSERT_EQUAL(1, setup_count);
  TEST_ASSERT_EQUAL(0, _usbh_data.failed_dev0_bus.hub_addr);
  TEST_ASSERT_FALSE(tuh_task_event_ready());
}

// the hub is removed while the port disable is in flight: the request fails with it, which releases the next child
void test_hub_removed_with_port_disable_in_flight_releases_next_attach(void) {
  fail_child_at_dev0();
  assert_next_child_held();

  remove_device_tree(RHPORT, 0, 0);
  TEST_ASSERT_FALSE(tuh_connected(HUB_ADDR));
  TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_TRUE(tuh_task_event_ready());
  TEST_ASSERT_EQUAL(0, breakpoint_count);
}

// the port is disabled: the next child enumerates right away
void test_hub_child_port_disabled_next_attach_enumerates(void) {
  fail_child_port_disable(XFER_RESULT_SUCCESS);
  TEST_ASSERT_TRUE(hub_status_polled());

  assert_next_child_enumerates();
}

// the failed child's teardown starts the next child's enumeration on another port: the port disabled and the address
// held are still the failed child's
void test_hub_addressed_child_teardown_attach_keeps_its_port(void) {
  usbh_ctrl_xfer_info_t *ctrl_info = &_usbh_data.ctrl_xfer_info;
  ctrl_info->daddr       = CHILD_ADDR;
  ctrl_info->stage       = CONTROL_STAGE_DATA;
  ctrl_info->complete_cb = in_flight_cb_attaches_next_child;
  fail_addressed_child();

  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL(HUB_PORT + 1, _usbh_data.dev0_bus.hub_port);
  TEST_ASSERT_EQUAL(HUB_ADDR, last_setup_daddr);
  TEST_ASSERT_EQUAL(HUB_FEATURE_PORT_ENABLE, tu_le16toh(last_setup.wValue));
  TEST_ASSERT_EQUAL(HUB_PORT, tu_le16toh(last_setup.wIndex));
  TEST_ASSERT_EQUAL(HUB_PORT, get_device(CHILD_ADDR)->bus_info.hub_port);
  TEST_ASSERT_FALSE(child_addr_free());
}
