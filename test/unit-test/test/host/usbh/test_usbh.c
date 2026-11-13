/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "host/usbh_pvt.h"

TEST_SOURCE_FILE("usbh.c")

#include "mock_hcd.h"

static uint32_t now_ms;
static unsigned callback_count;

uint32_t tusb_time_millis_api(void) {
  return now_ms;
}

static void deferred_callback(uintptr_t param) {
  TEST_ASSERT_EQUAL(123, param);
  callback_count++;
}

static bool setup_send_stalls(uint8_t rhport, uint8_t daddr, uint8_t const setup_packet[8], int num_calls) {
  (void) rhport;
  (void) setup_packet;
  (void) num_calls;
  hcd_event_xfer_complete(daddr, 0x00, 0, XFER_RESULT_STALLED, false);
  return true;
}

void setUp(void) {
  now_ms = 0;
  callback_count = 0;
  hcd_int_disable_Ignore();
  hcd_int_enable_Ignore();

  if (!tuh_inited()) {
    tusb_rhport_init_t host_init = {
      .role = TUSB_ROLE_HOST,
      .speed = TUSB_SPEED_AUTO
    };
    hcd_init_ExpectAndReturn(0, &host_init, true);
    TEST_ASSERT_TRUE(tusb_init(0, &host_init));
  }
}

void tearDown(void) {
}

void test_usbh_task_idle_does_not_mask_interrupts(void) {
  TEST_ASSERT_FALSE(tuh_task_event_ready());
  hcd_int_disable_StopIgnore();
  hcd_int_enable_StopIgnore();

  TEST_ASSERT_FALSE(tuh_task());
}

void test_usbh_task_runs_due_callback_without_queued_event(void) {
  TEST_ASSERT_TRUE(usbh_defer_func_ms_async(10, deferred_callback, 123));
  TEST_ASSERT_FALSE(tuh_task_event_ready());

  tuh_task();
  TEST_ASSERT_EQUAL(0, callback_count);

  now_ms = 10; // The scheduler adds one tick to guarantee the requested delay.
  TEST_ASSERT_FALSE(tuh_task_event_ready());
  tuh_task();
  TEST_ASSERT_EQUAL(0, callback_count);

  now_ms = 11;
  TEST_ASSERT_TRUE(tuh_task_event_ready());
  tuh_task();
  TEST_ASSERT_EQUAL(1, callback_count);
  TEST_ASSERT_FALSE(tuh_task_event_ready());
}

void test_usbh_task_returns_stop_left_by_sync_control_xfer(void) {
  hcd_event_device_attach(0, false);
  TEST_ASSERT_FALSE(tuh_task()); // dev0 starts debouncing
  TEST_ASSERT_TRUE(tuh_connected(0));

  // The sync transfer's own non-stoppable poll consumes this stop event and must leave it for tuh_task()
  TEST_ASSERT_TRUE(tuh_task_stop());
  hcd_setup_send_Stub(setup_send_stalls);
  const tusb_control_request_t request = {
    .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_DEVICE, .type = TUSB_REQ_TYPE_STANDARD, .direction = TUSB_DIR_IN },
    .bRequest = TUSB_REQ_GET_STATUS,
    .wValue = 0,
    .wIndex = 0,
    .wLength = 0
  };
  tuh_xfer_t xfer = { .daddr = 0, .ep_addr = 0, .setup = &request };
  TEST_ASSERT_TRUE(tuh_control_xfer(&xfer));
  TEST_ASSERT_EQUAL(XFER_RESULT_STALLED, xfer.result);

  TEST_ASSERT_FALSE(tuh_task_event_ready());
  TEST_ASSERT_TRUE(tuh_task());
  TEST_ASSERT_FALSE(tuh_task());

  hcd_deinit_ExpectAndReturn(0, true);
  TEST_ASSERT_TRUE(tuh_deinit(0)); // drop the debouncing dev0; setUp() reinits the stack
}
