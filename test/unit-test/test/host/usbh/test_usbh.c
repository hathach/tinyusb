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

  tuh_task();
}

void test_usbh_task_runs_due_callback_without_queued_event(void) {
  TEST_ASSERT_TRUE(usbh_defer_func_ms_async(10, deferred_callback, 123));
  TEST_ASSERT_FALSE(tuh_task_event_ready());

  tuh_task();
  TEST_ASSERT_EQUAL(0, callback_count);

  now_ms = 11; // The scheduler adds one tick to guarantee the requested delay.
  TEST_ASSERT_TRUE(tuh_task_event_ready());
  tuh_task();
  TEST_ASSERT_EQUAL(1, callback_count);
  TEST_ASSERT_FALSE(tuh_task_event_ready());
}
