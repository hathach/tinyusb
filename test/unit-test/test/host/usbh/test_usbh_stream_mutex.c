/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "test_watchdog.h"

// Include the production implementation to set endpoint state directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("usbh_test_hcd_stubs.c")

// Built with a real osal_none mutex (TUP_MCU_MULTIPLE_CORE). Stream submissions also run from tuh_task(), which may
// preempt a writer holding the TX FIFO's write mutex: with that mutex held, they must not wait for it (#4595).

enum {
  DADDR  = 1,
  EP_OUT = 0x02,
  MPS    = 64,
};

static const tusb_desc_endpoint_t desc_ep = {
  .bLength          = sizeof(tusb_desc_endpoint_t),
  .bDescriptorType  = TUSB_DESC_ENDPOINT,
  .bEndpointAddress = EP_OUT,
  .bmAttributes     = { .xfer = TUSB_XFER_BULK },
  .wMaxPacketSize   = MPS,
  .bInterval        = 0
};

static uint8_t ff_buf[256];
TU_ATTR_ALIGNED(TUP_HCD_MEM_ALIGN) static uint8_t ep_buf[MPS + TUP_HCD_MEM_ALIGN];
static tu_edpt_stream_t stream;

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) daddr;
  (void) setup_packet;
  return true;
}

// osal_none's mutex spins forever when it is taken twice
static void mutex_watchdog(int sig) {
  (void) sig;
  TEST_FAIL_MESSAGE("stream submission waited for the TX FIFO's write mutex");
}

static volatile uint8_t *ep_state(void) {
  return &get_device(DADDR)->ep_status[tu_edpt_number(EP_OUT)][TUSB_DIR_OUT];
}

static void stream_setup_mode(uint8_t *buf, bool overwritable) {
  tu_memclr(&stream, sizeof(stream));
  TEST_ASSERT_TRUE(tu_edpt_stream_init(&stream, true, true, overwritable, ff_buf, sizeof(ff_buf), buf));
  tu_edpt_stream_open(&stream, DADDR, &desc_ep, MPS);
  TEST_ASSERT_NOT_NULL(stream.ff.mutex_wr);
  TEST_ASSERT_EQUAL(1, stream.ff_mutexdef.count);
}

static void stream_setup(uint8_t *buf) {
  stream_setup_mode(buf, false);
}

// the writer that the task preempted holds the FIFO's write mutex
static void writer_lock(void) {
  TEST_ASSERT_TRUE(osal_mutex_lock(stream.ff.mutex_wr, OSAL_TIMEOUT_WAIT_FOREVER));
}

static void writer_unlock(void) {
  TEST_ASSERT_EQUAL(0, stream.ff_mutexdef.count); // untouched by the submissions
  TEST_ASSERT_TRUE(osal_mutex_unlock(stream.ff.mutex_wr));
}

void setUp(void) {
  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
    clear_device(&_usbh_devices[i]);
  }
  _usbh_mutex = osal_mutex_create(&_usbh_mutexdef);
  usbh_device_t *dev = get_device(DADDR);
  dev->connected     = 1;
  dev->addressed     = 1;
  dev->configured    = 1;
  test_watchdog_arm(mutex_watchdog, 5);
}

void tearDown(void) {
  test_watchdog_disarm();
}

void test_stream_idle_flush_skips_write_mutex(void) {
  stream_setup(ep_buf);
  writer_lock();
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  writer_unlock();
}

void test_stream_data_submit_skips_write_mutex(void) {
  stream_setup(ep_buf);
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  writer_lock();
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write_xfer(&stream));
  writer_unlock();
}

// an overwritable FIFO (CDC before DTR, printer) is drained without the mutex too
void test_stream_overwritable_submit_skips_write_mutex(void) {
  stream_setup_mode(ep_buf, true);
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  writer_lock();
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write_xfer(&stream));
  writer_unlock();
}

// A completion's submission sends a short write even while a writer holds the mutex: that writer's own flush skips a
// short write, so data left queued would wait for an unrelated write
void test_stream_overwritable_short_write_not_stranded(void) {
  stream_setup_mode(ep_buf, true);
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  writer_lock();
  (void) tu_edpt_stream_write_xfer(&stream);
  writer_unlock();
  TEST_ASSERT_EQUAL_UINT32(1, tu_edpt_stream_write(&stream, "d", 1));
  TEST_ASSERT_NOT_EQUAL(0, *ep_state() & TU_EDPT_STATE_BUSY);
}

// a refused submission (here the alignment check) recovers without the mutex too
void test_stream_refused_submit_skips_write_mutex(void) {
  stream_setup(ep_buf + 1);
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  writer_lock();
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL_UINT16(3, stream.tx_pending);
  TEST_ASSERT_EQUAL(0, *ep_state() & TU_EDPT_STATE_CLAIMED);
  writer_unlock();
}

void test_stream_zlp_skips_write_mutex(void) {
  stream_setup(ep_buf);
  writer_lock();
  TEST_ASSERT_TRUE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS));
  writer_unlock();
}

// A clear takes the write mutex, as tu_fifo_clear() does; a clear it left to a submission is applied without it
void test_stream_deferred_clear_skips_write_mutex(void) {
  stream_setup(ep_buf);
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "old", 3));
  stream.tx_active = true;
  tu_edpt_stream_clear(&stream);
  stream.tx_active = false;
  TEST_ASSERT_EQUAL_UINT16(3, tu_fifo_count(&stream.ff));
  TEST_ASSERT_EQUAL_UINT16(2, tu_fifo_write_n(&stream.ff, "ok", 2));

  writer_lock();
  TEST_ASSERT_EQUAL_UINT32(2, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_TRUE(tu_fifo_empty(&stream.ff));
  writer_unlock();
}
