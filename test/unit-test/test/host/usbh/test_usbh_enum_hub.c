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
uint32_t tusb_time_millis_api(void) {
  return 0;
}

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void) rhport;
  (void) rh_init;
  return true;
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void) rhport;
  (void) in_isr;
}

void hcd_int_enable(uint8_t rhport) {
  (void) rhport;
}

void hcd_int_disable(uint8_t rhport) {
  (void) rhport;
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return true;
}

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
}

void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  return TUSB_SPEED_FULL;
}

void hcd_device_close(uint8_t rhport, uint8_t daddr) {
  (void) rhport;
  (void) daddr;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) daddr;
  memcpy(&last_setup, setup_packet, sizeof(last_setup));
  setup_count++;
  return true;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, const tusb_desc_endpoint_t *ep_desc) {
  (void) rhport;
  (void) daddr;
  (void) ep_desc;
  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  (void) buffer;
  (void) buflen;
  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void noop_cb(tuh_xfer_t *xfer) {
  (void) xfer;
}

// Leave a port status in the hub's ctrl_buf as an earlier GET_STATUS would, then free the control slot
static void stage_port_status(bool connected, bool reset_changed) {
  TEST_ASSERT_TRUE(hub_port_get_status(HUB_ADDR, HUB_PORT, NULL, noop_cb, 0));
  hub_port_status_response_t *status = (hub_port_status_response_t *) _usbh_data.ctrl_xfer_info.buffer;
  TEST_ASSERT_NOT_NULL(status);
  tu_memclr(status, sizeof(*status));
  status->status.connection = connected ? 1 : 0;
  status->change.reset      = reset_changed ? 1 : 0;

  tu_memclr(&_usbh_data.ctrl_xfer_info, sizeof(_usbh_data.ctrl_xfer_info));
  setup_count = 0;
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
