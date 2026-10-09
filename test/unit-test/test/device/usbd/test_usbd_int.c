/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// USB IRQ masking without an OS (#4163): usbd_int_set() nests, and the stack-owned state decides whether the
// outermost unmask re-enables the IRQ. CMock fails any dcd_int_enable/disable call a test does not expect.

#include "unity.h"

#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd.c")

#include "mock_dcd.h"
#include "mock_msc_device.h"

uint32_t tusb_time_millis_api(void) {
  return 0;
}

// CFG_TUSB_DEBUG_BREAKPOINT (project.yml): counts TU_ASSERT failures
static unsigned assert_count;
void test_assert_hit(void) {
  assert_count++;
}

// Never reached: these tests enumerate nothing
uint8_t const *tud_descriptor_device_cb(void) {
  return NULL;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return NULL;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) index;
  (void) langid;
  return NULL;
}

static const tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};

// A lock/unlock inside dcd_init(), e.g. from dcd_connect(), must leave the IRQ off
static bool dcd_init_locks(uint8_t rhport, const tusb_rhport_init_t *rh_init, int num_calls) {
  (void) rhport;
  (void) rh_init;
  (void) num_calls;
  usbd_critical_enter(false);
  usbd_critical_exit(false);
  return true;
}

static void device_init(bool expect_enable) {
  mscd_init_Expect();
  dcd_init_Stub(dcd_init_locks);
  if (expect_enable) {
    dcd_int_enable_Expect(0);
  }
  TEST_ASSERT_TRUE(tusb_init(0, &dev_init));
  TEST_ASSERT_TRUE(tud_inited());
}

// A lock/unlock inside dcd_deinit() must not re-enable the IRQ
static bool dcd_deinit_locks(uint8_t rhport, int num_calls) {
  (void) rhport;
  (void) num_calls;
  usbd_critical_enter(false);
  usbd_critical_exit(false);
  return true;
}

static void device_deinit(bool expect_disable) {
  if (expect_disable) {
    dcd_int_disable_Expect(0);
  }
  dcd_disconnect_Expect(0);
  dcd_deinit_Stub(dcd_deinit_locks);
  mscd_deinit_IgnoreAndReturn(true);
  TEST_ASSERT_TRUE(tud_deinit(0));
  TEST_ASSERT_FALSE(tud_inited());
}

void setUp(void) {
  assert_count = 0;
}

void tearDown(void) {
}

void test_usbd_int_lock_before_init_touches_no_irq(void) {
  usbd_critical_enter(false);
  usbd_critical_exit(false);
}

void test_usbd_int_init_enables_after_dcd_init(void) {
  device_init(true);
  device_deinit(true);
}

void test_usbd_int_nested_mask_unmasks_once(void) {
  device_init(true);

  dcd_int_disable_Expect(0);
  usbd_critical_enter(false);

  // queue access inside the spinlock, as osal_queue_send/receive do without an OS
  usbd_int_set(false);
  usbd_int_set(true);

  dcd_int_enable_Expect(0);
  usbd_critical_exit(false);

  device_deinit(true);
}

void test_usbd_int_isr_lock_touches_no_irq(void) {
  device_init(true);
  usbd_critical_enter(true);
  usbd_critical_exit(true);
  device_deinit(true);
}

void test_usbd_int_deinit_while_masked_stays_off(void) {
  device_init(true);

  dcd_int_disable_Expect(0);
  usbd_int_set(false);

  device_deinit(false);

  // outermost unmask after deinit leaves the IRQ off
  usbd_int_set(true);

  usbd_critical_enter(false);
  usbd_critical_exit(false);
}

void test_usbd_int_unmatched_unmask_ignored(void) {
  device_init(true);

  usbd_int_set(true);
  TEST_ASSERT_EQUAL(1, assert_count);
  usbd_critical_exit(false); // osal_none: unlock without lock
  TEST_ASSERT_EQUAL(2, assert_count);

  dcd_int_disable_Expect(0);
  usbd_critical_enter(false);
  dcd_int_enable_Expect(0);
  usbd_critical_exit(false);

  device_deinit(true);
}

void test_usbd_int_init_while_masked_enables_on_unmask(void) {
  usbd_int_set(false);
  device_init(false);

  dcd_int_enable_Expect(0);
  usbd_int_set(true);

  device_deinit(true);
  TEST_ASSERT_EQUAL(0, assert_count);
}

void test_usbd_int_reinit_while_masked_enables_on_unmask(void) {
  device_init(true);

  dcd_int_disable_Expect(0);
  usbd_int_set(false);

  device_deinit(false);
  device_init(false);

  dcd_int_enable_Expect(0);
  usbd_int_set(true);

  device_deinit(true);
  TEST_ASSERT_EQUAL(0, assert_count);
}

void test_usbd_int_mask_holds_across_critical_section(void) {
  device_init(true);

  dcd_int_disable_Expect(0);
  usbd_int_mask_enter(false);

  // a critical section and a queue access inside the mask leave the IRQ off
  usbd_critical_enter(false);
  usbd_critical_exit(false);
  usbd_int_set(false);
  usbd_int_set(true);

  dcd_int_enable_Expect(0);
  usbd_int_mask_exit(false);

  device_deinit(true);
  TEST_ASSERT_EQUAL(0, assert_count);
}

void test_usbd_int_mask_before_init_touches_no_irq(void) {
  usbd_int_mask_enter(false);
  usbd_int_mask_exit(false);
  TEST_ASSERT_EQUAL(0, assert_count);
}

void test_usbd_int_mask_isr_leaves_task_mask(void) {
  device_init(true);

  dcd_int_disable_Expect(0);
  usbd_int_mask_enter(false);

  usbd_int_mask_enter(true);
  usbd_int_mask_exit(true);

  dcd_int_enable_Expect(0);
  usbd_int_mask_exit(false);

  // from the ISR without a task mask: touches no IRQ state
  usbd_int_mask_enter(true);
  usbd_int_mask_exit(true);

  device_deinit(true);
  TEST_ASSERT_EQUAL(0, assert_count);
}
