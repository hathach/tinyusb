/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// Include the production implementation to drive process_enumeration() directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("hub.c")
TEST_SOURCE_FILE("usbh_test_hcd_stubs.c")

enum {
  RHPORT   = 0,
  HUB_ADDR = CFG_TUH_DEVICE_MAX + 1,
  HUB_PORT = 1,
};

static uint8_t                setup_count;
static tusb_control_request_t last_setup;

//--------------------------------------------------------------------+
// HCD stubs
//--------------------------------------------------------------------+
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) daddr;
  memcpy(&last_setup, setup_packet, sizeof(last_setup));
  setup_count++;
  return true;
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void noop_cb(tuh_xfer_t *xfer) {
  (void) xfer;
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

  usbh_device_t *hub   = get_device(HUB_ADDR);
  hub->bus_info.rhport = RHPORT;
  hub->connected       = 1;
  hub->addressed       = 1;
  hub->configured      = 1;
  hub->mounted         = 1;

  // a device on the hub's port is being enumerated at address 0
  _usbh_data.enumerating_daddr = 0;
  _usbh_data.dev0_bus.rhport   = RHPORT;
  _usbh_data.dev0_bus.hub_addr = HUB_ADDR;
  _usbh_data.dev0_bus.hub_port = HUB_PORT;
  _usbh_data.dev0_bus.speed    = TUSB_SPEED_FULL;

  setup_count = 0;
  tu_memclr(&last_setup, sizeof(last_setup));
}

void tearDown(void) {
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

    TEST_ASSERT_EQUAL(0, setup_count);
    TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
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

      TEST_ASSERT_EQUAL(0, setup_count);
      TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
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

  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
}

void test_hub_highspeed_child_on_highspeed_hub_enumerates(void) {
  get_device(HUB_ADDR)->bus_info.speed = TUSB_SPEED_HIGH;
  stage_port_status_speed(true, false, true);
  complete_port_status(ENUM_HUB_CLEAR_RESET_COMPLETE, sizeof(hub_port_status_response_t));

  TEST_ASSERT_EQUAL(TUSB_SPEED_HIGH, _usbh_data.dev0_bus.speed);
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
}
