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


// USB IRQ masking without an OS (#4163): usbh_int_set() nests, and the stack-owned state decides whether the
// outermost unmask re-enables the IRQ. CMock fails any hcd_int_enable/disable call a test does not expect.

#include "unity.h"

#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"
TEST_SOURCE_FILE("usbh.c")

#include "mock_hcd.h"

uint32_t tusb_time_millis_api(void) {
  return 0;
}

// CFG_TUSB_DEBUG_BREAKPOINT (project.yml): counts TU_ASSERT failures
static unsigned assert_count;
void test_assert_hit(void) {
  assert_count++;
}

static const tusb_rhport_init_t host_init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO};

// A lock/unlock inside hcd_init() must leave the IRQ off
static bool hcd_init_locks(uint8_t rhport, const tusb_rhport_init_t *rh_init, int num_calls) {
  (void) rhport;
  (void) rh_init;
  (void) num_calls;
  usbh_spin_lock(false);
  usbh_spin_unlock(false);
  return true;
}

static void host_init_port(bool expect_enable) {
  hcd_init_Stub(hcd_init_locks);
  if (expect_enable) {
    hcd_int_enable_Expect(1);
  }
  TEST_ASSERT_TRUE(tuh_rhport_init(1, &host_init));
  TEST_ASSERT_TRUE(tuh_inited());
}

// A lock/unlock inside hcd_deinit() must not re-enable the IRQ
static bool hcd_deinit_locks(uint8_t rhport, int num_calls) {
  (void) rhport;
  (void) num_calls;
  usbh_spin_lock(false);
  usbh_spin_unlock(false);
  return true;
}

static void host_deinit_port(bool expect_disable) {
  if (expect_disable) {
    hcd_int_disable_Expect(1);
  }
  hcd_deinit_Stub(hcd_deinit_locks);
  hcd_device_close_Ignore();
  TEST_ASSERT_TRUE(tuh_deinit(1));
  TEST_ASSERT_FALSE(tuh_inited());
}

void setUp(void) {
  assert_count = 0;
}

void tearDown(void) {
}

void test_usbh_int_lock_before_init_touches_no_irq(void) {
  usbh_spin_lock(false);
  usbh_spin_unlock(false);
}

void test_usbh_int_init_enables_after_hcd_init(void) {
  host_init_port(true);
  host_deinit_port(true);
}

void test_usbh_int_nested_mask_unmasks_once(void) {
  host_init_port(true);

  hcd_int_disable_Expect(1);
  usbh_spin_lock(false);

  // queue access inside the spinlock, as osal_queue_send/receive do without an OS
  usbh_int_set(false);
  usbh_int_set(true);

  hcd_int_enable_Expect(1);
  usbh_spin_unlock(false);

  host_deinit_port(true);
}

void test_usbh_int_deinit_while_masked_stays_off(void) {
  host_init_port(true);

  hcd_int_disable_Expect(1);
  usbh_int_set(false);

  host_deinit_port(false);

  // outermost unmask after deinit leaves the IRQ off and does not use the invalid controller id
  usbh_int_set(true);

  usbh_spin_lock(false);
  usbh_spin_unlock(false);
}

void test_usbh_int_unmatched_unmask_ignored(void) {
  host_init_port(true);

  usbh_int_set(true);
  TEST_ASSERT_EQUAL(1, assert_count);
  usbh_spin_unlock(false); // osal_none: unlock without lock
  TEST_ASSERT_EQUAL(2, assert_count);

  hcd_int_disable_Expect(1);
  usbh_spin_lock(false);
  hcd_int_enable_Expect(1);
  usbh_spin_unlock(false);

  host_deinit_port(true);
}

void test_usbh_int_init_while_masked_enables_on_unmask(void) {
  usbh_int_set(false);
  host_init_port(false);

  hcd_int_enable_Expect(1);
  usbh_int_set(true);

  host_deinit_port(true);
  TEST_ASSERT_EQUAL(0, assert_count);
}

void test_usbh_int_reinit_while_masked_enables_on_unmask(void) {
  host_init_port(true);

  hcd_int_disable_Expect(1);
  usbh_int_set(false);

  host_deinit_port(false);
  host_init_port(false);

  hcd_int_enable_Expect(1);
  usbh_int_set(true);

  host_deinit_port(true);
  TEST_ASSERT_EQUAL(0, assert_count);
}
