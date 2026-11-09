/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// Include the production implementation to set endpoint state and process events directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")

enum {
  RHPORT = 0,
  DADDR  = 1,
  EP_IN  = 0x81,
};

static bool     hcd_abort_result;
static uint8_t  hcd_abort_count;
static uint8_t  hcd_close_count;
static uint8_t  xfer_cb_count;
static uint32_t xfer_cb_len;

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
  (void) setup_packet;
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
  hcd_close_count++;
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

// true: the transfer was still pending and is aborted; false: the HCD has none (completed, its event queued)
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  hcd_abort_count++;
  return hcd_abort_result;
}

//--------------------------------------------------------------------+
// Fake class driver owning EP_IN
//--------------------------------------------------------------------+
static bool fake_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) dev_addr;
  (void) ep_addr;
  (void) result;
  xfer_cb_count++;
  xfer_cb_len = xferred_bytes;
  return true;
}

static const usbh_class_driver_t fake_driver = {
  .name    = "FAKE",
  .xfer_cb = fake_xfer_cb,
};

static volatile uint8_t *ep_in_status(void) {
  return &get_device(DADDR)->ep_status[tu_edpt_number(EP_IN)][tu_edpt_dir(EP_IN)];
}

// the completion event of EP_IN's transfer, as the HCD queued it before the abort
static void process_ep_in_complete(uint32_t len) {
  hcd_event_t event = {
    .rhport        = RHPORT,
    .event_id      = HCD_EVENT_XFER_COMPLETE,
    .dev_addr      = DADDR,
    .xfer_complete = {.ep_addr = EP_IN, .result = XFER_RESULT_SUCCESS, .len = len},
  };
  usbh_process_event(&event, false);
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

  hcd_abort_result = true;
  hcd_abort_count  = 0;
  hcd_close_count  = 0;
  xfer_cb_count    = 0;
  xfer_cb_len      = 0;

  usbh_device_t *dev   = get_device(DADDR);
  dev->bus_info.rhport = RHPORT;
  dev->connected       = 1;
  dev->addressed       = 1;
  dev->configured      = 1;
  dev->mounted         = 1;
  dev->ep2drv[tu_edpt_number(EP_IN)][tu_edpt_dir(EP_IN)] = 0;

  // a transfer on EP_IN, as usbh_edpt_claim() + usbh_edpt_xfer() leave it
  *ep_in_status() = TU_EDPT_STATE_CLAIMED | TU_EDPT_STATE_BUSY;
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_abort_pending_transfer_releases_endpoint(void) {
  TEST_ASSERT_TRUE(tuh_edpt_abort_xfer(DADDR, EP_IN));

  TEST_ASSERT_EQUAL(1, hcd_abort_count);
  TEST_ASSERT_EQUAL_HEX8(0, *ep_in_status());
  TEST_ASSERT_TRUE(usbh_edpt_claim(DADDR, EP_IN));
}

// The transfer completed before the abort: its event is still queued. The endpoint must stay busy until that event
// is processed, or a new transfer started into the same buffer would be completed by it (and released early).
void test_abort_completed_transfer_leaves_endpoint_to_its_event(void) {
  hcd_abort_result = false;

  TEST_ASSERT_FALSE(tuh_edpt_abort_xfer(DADDR, EP_IN));
  TEST_ASSERT_EQUAL_HEX8(TU_EDPT_STATE_CLAIMED | TU_EDPT_STATE_BUSY, *ep_in_status());
  TEST_ASSERT_FALSE(usbh_edpt_claim(DADDR, EP_IN));

  process_ep_in_complete(64);

  TEST_ASSERT_EQUAL(1, xfer_cb_count);
  TEST_ASSERT_EQUAL(64, xfer_cb_len);
  TEST_ASSERT_EQUAL_HEX8(0, *ep_in_status());
  TEST_ASSERT_TRUE(usbh_edpt_claim(DADDR, EP_IN));
}

void test_abort_idle_endpoint_returns_false(void) {
  *ep_in_status() = 0;

  TEST_ASSERT_FALSE(tuh_edpt_abort_xfer(DADDR, EP_IN));
  TEST_ASSERT_EQUAL(0, hcd_abort_count);
}

// Closing releases the endpoint even when the HCD had no transfer to abort.
void test_close_releases_endpoint_without_transfer_to_abort(void) {
  hcd_abort_result = false;

  TEST_ASSERT_TRUE(tuh_edpt_close(DADDR, EP_IN));

  TEST_ASSERT_EQUAL(1, hcd_abort_count);
  TEST_ASSERT_EQUAL(1, hcd_close_count);
  TEST_ASSERT_EQUAL_HEX8(0, *ep_in_status());
}
