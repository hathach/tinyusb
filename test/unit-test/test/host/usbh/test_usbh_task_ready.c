/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// Stage deferred work independently of the main event queue.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("hub.c")

#include "mock_hcd.h"

static uint32_t now_ms;
static unsigned complete_count[2];
static xfer_result_t results[2];

static const tusb_control_request_t requests[2] = {
  { .bmRequestType = 0x40, .bRequest = 1, .wValue = 1 },
  { .bmRequestType = 0x40, .bRequest = 1, .wValue = 2 }
};

uint32_t tusb_time_millis_api(void) {
  return now_ms;
}

static void control_complete(tuh_xfer_t *xfer) {
  TEST_ASSERT_LESS_THAN(2, xfer->user_data);
  complete_count[xfer->user_data]++;
  results[xfer->user_data] = xfer->result;
}

static void submit_control(unsigned index) {
  tuh_xfer_t xfer = {
    .daddr = 1, .ep_addr = 0, .setup = &requests[index],
    .complete_cb = control_complete, .user_data = index
  };
  TEST_ASSERT_TRUE(tuh_control_xfer(&xfer));
}

void setUp(void) {
  now_ms = 0;
  tu_memclr(complete_count, sizeof(complete_count));
  results[0] = results[1] = XFER_RESULT_INVALID;
  hcd_int_disable_Ignore();
  hcd_int_enable_Ignore();

  const tusb_rhport_init_t host_init = { .role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO };
  hcd_init_ExpectAndReturn(0, &host_init, true);
  TEST_ASSERT_TRUE(tusb_init(0, &host_init));

  usbh_device_t *dev = get_device(1);
  dev->bus_info.rhport = 0;
  dev->connected = 1;
}

void tearDown(void) {
  hcd_deinit_ExpectAndReturn(0, true);
  hcd_device_close_Ignore();
  TEST_ASSERT_TRUE(tuh_deinit(0));
}

void test_usbh_task_runs_watchdog_without_queued_event(void) {
  hcd_setup_send_ExpectWithArrayAndReturn(0, 1, (const uint8_t *)&requests[0], 8, true);
  submit_control(0);
  TEST_ASSERT_TRUE(osal_queue_empty(_usbh_q));
  TEST_ASSERT_NULL(_usbh_data.call_after.func);

  now_ms = CFG_TUH_CONTROL_TIMEOUT_MS - 1;
  TEST_ASSERT_FALSE(tuh_task_event_ready());
  TEST_ASSERT_FALSE(tuh_task());
  TEST_ASSERT_EQUAL(0, complete_count[0]);

  now_ms++;
  TEST_ASSERT_TRUE(tuh_task_event_ready());
  hcd_edpt_abort_xfer_ExpectAndReturn(0, 1, 0, true);
  TEST_ASSERT_FALSE(tuh_task());
  TEST_ASSERT_EQUAL(1, complete_count[0]);
  TEST_ASSERT_EQUAL(XFER_RESULT_TIMEOUT, results[0]);
  TEST_ASSERT_FALSE(tuh_task_event_ready());
  TEST_ASSERT_FALSE(tuh_task());
  TEST_ASSERT_EQUAL(1, complete_count[0]);
}

void test_usbh_task_dispatches_pending_control_without_queued_event(void) {
  hcd_setup_send_ExpectWithArrayAndReturn(0, 1, (const uint8_t *)&requests[0], 8, true);
  submit_control(0);
  submit_control(1);
  TEST_ASSERT_FALSE(tu_fifo_empty(&_usbh_pending_ctrl_q));
  TEST_ASSERT_FALSE(tuh_task_event_ready());
  TEST_ASSERT_FALSE(tuh_task()); // The busy slot must keep the second request pending.
  TEST_ASSERT_EQUAL(0, complete_count[1]);

  hcd_edpt_abort_xfer_ExpectAndReturn(0, 1, 0, true);
  TEST_ASSERT_TRUE(tuh_edpt_abort_xfer(1, 0));
  TEST_ASSERT_EQUAL(1, complete_count[0]);
  TEST_ASSERT_EQUAL(XFER_RESULT_ABORTED, results[0]);
  TEST_ASSERT_TRUE(osal_queue_empty(_usbh_q));
  TEST_ASSERT_NULL(_usbh_data.call_after.func);
  TEST_ASSERT_TRUE(tuh_task_event_ready());

  hcd_setup_send_ExpectWithArrayAndReturn(0, 1, (const uint8_t *)&requests[1], 8, true);
  TEST_ASSERT_FALSE(tuh_task());
  TEST_ASSERT_TRUE(tu_fifo_empty(&_usbh_pending_ctrl_q));
  TEST_ASSERT_EQUAL(0, complete_count[1]);
  TEST_ASSERT_FALSE(tuh_task_event_ready());

  hcd_event_xfer_complete(1, 0, 0, XFER_RESULT_STALLED, false);
  TEST_ASSERT_FALSE(tuh_task());
  TEST_ASSERT_EQUAL(1, complete_count[1]);
  TEST_ASSERT_EQUAL(XFER_RESULT_STALLED, results[1]);
}

void test_usbh_task_runs_eligible_deferred_attach_without_queued_event(void) {
  const hcd_event_t attach = {
    .rhport = 0, .event_id = HCD_EVENT_DEVICE_ATTACH,
    .connection = { .hub_addr = CFG_TUH_DEVICE_MAX + 1, .hub_port = 1 }
  };
  TEST_ASSERT_TRUE(osal_queue_send(_usbh_daq, &attach, false));
  _usbh_data.enumerating_daddr = 1;
  TEST_ASSERT_FALSE(tuh_task_event_ready());
  TEST_ASSERT_FALSE(tuh_task());
  TEST_ASSERT_FALSE(osal_queue_empty(_usbh_daq));

  _usbh_data.enumerating_daddr = TUSB_INDEX_INVALID_8;
  TEST_ASSERT_TRUE(osal_queue_empty(_usbh_q));
  TEST_ASSERT_NULL(_usbh_data.call_after.func);
  TEST_ASSERT_TRUE(tuh_task_event_ready());
  TEST_ASSERT_FALSE(tuh_task());
  TEST_ASSERT_TRUE(osal_queue_empty(_usbh_daq));
  TEST_ASSERT_TRUE(tuh_connected(0));
  TEST_ASSERT_EQUAL(attach.connection.hub_addr, _usbh_data.dev0_bus.hub_addr);
  TEST_ASSERT_EQUAL(attach.connection.hub_port, _usbh_data.dev0_bus.hub_port);
  TEST_ASSERT_NOT_NULL(_usbh_data.call_after.func);
}
