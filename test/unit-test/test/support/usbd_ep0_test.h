/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// Shared by device class tests that play the host on EP0 through usbd and a mocked DCD: records the EP0 transfers the
// stack queues (IN data snapshotted at submission) and whether it stalled. Each test file defines the descriptor
// callbacks.

#ifndef USBD_EP0_TEST_H_
#define USBD_EP0_TEST_H_

#include "unity.h"
#include "tusb.h"
#include "mock_dcd.h"

enum { RHPORT = 0, EP_CTRL_OUT = 0x00, EP_CTRL_IN = 0x80 };

typedef struct {
  uint8_t ep;
  uint16_t len;
  uint8_t data[CFG_TUD_ENDPOINT0_SIZE];
} ep0_xfer_t;
static ep0_xfer_t ep0_log[32];
static unsigned ep0_count;
static unsigned ep0_cursor;
static bool ep0_stalled;

static inline bool ep0_test_edpt_xfer(uint8_t rhport, uint8_t ep, uint8_t* buf, uint16_t len, bool is_isr, int n) {
  (void) rhport; (void) is_isr; (void) n;
  TEST_ASSERT_EQUAL(0, tu_edpt_number(ep));
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(ep0_log), ep0_count);
  ep0_xfer_t* x = &ep0_log[ep0_count++];
  x->ep = ep;
  x->len = len;
  if (tu_edpt_dir(ep) == TUSB_DIR_IN && len) memcpy(x->data, buf, len);
  return true;
}

static inline void ep0_test_edpt_stall(uint8_t rhport, uint8_t ep, int n) {
  (void) rhport; (void) ep; (void) n;
  ep0_stalled = true;
}

// mocks the DCD, starts the stack once and clears the EP0 record; call from setUp()
static inline void ep0_test_init(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt_open_IgnoreAndReturn(true);
  dcd_edpt_close_all_Ignore();
  dcd_set_address_Ignore();
  dcd_edpt0_status_complete_Ignore();
  dcd_edpt0_setup_begin_IgnoreAndReturn(true);
  dcd_sof_enable_Ignore();
  dcd_edpt_xfer_Stub(ep0_test_edpt_xfer);
  dcd_edpt_stall_Stub(ep0_test_edpt_stall);
  ep0_count = ep0_cursor = 0;
  ep0_stalled = false;

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    dcd_init_ExpectAndReturn(RHPORT, &dev_init, true);
    tusb_init(RHPORT, &dev_init);
  }
}

// the next queued EP0 transfer must be `ep`/`len`; the host completes it
static inline const ep0_xfer_t* host_complete_ep0(uint8_t ep, uint16_t len) {
  TEST_ASSERT_LESS_THAN_MESSAGE(ep0_count, ep0_cursor, "no EP0 transfer queued");
  const ep0_xfer_t* x = &ep0_log[ep0_cursor++];
  TEST_ASSERT_EQUAL_HEX8(ep, x->ep);
  TEST_ASSERT_EQUAL(len, x->len);
  dcd_event_xfer_complete(RHPORT, ep, len, XFER_RESULT_SUCCESS, false);
  tud_task();
  return x;
}

// the caller checks ep0_stalled
static inline void host_setup(const tusb_control_request_t* req) {
  dcd_event_setup_received(RHPORT, (const uint8_t*) req, false);
  tud_task();
}

// no-data request the device must accept: SETUP, then the IN status stage
static inline void host_no_data(const tusb_control_request_t* req) {
  host_setup(req);
  TEST_ASSERT_FALSE_MESSAGE(ep0_stalled, "request stalled");
  host_complete_ep0(EP_CTRL_IN, 0);
}

static inline void host_bus_reset(void) {
  dcd_event_bus_reset(RHPORT, TUSB_SPEED_FULL, false);
  tud_task();
}

static inline void host_set_config(uint8_t cfg) {
  const tusb_control_request_t req = { .bmRequestType = 0x00, .bRequest = TUSB_REQ_SET_CONFIGURATION, .wValue = cfg };
  host_no_data(&req);
}

#endif
