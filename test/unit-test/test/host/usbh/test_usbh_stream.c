/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// usbh.c logs a refused transfer after clearing its BUSY state, before the stream keeps the refused bytes: hook the
// log there (this suite builds with CFG_TUSB_DEBUG=0, which leaves TU_LOG1 to the first definition)
int refusal_log(const char *format, ...);
#define TU_LOG1 refusal_log

// Include the production implementation to set endpoint state directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("usbh_test_hcd_stubs.c")

// Endpoint stream TX through usbh: the endpoint claim serializes submissions (#4449, #4595)

enum {
  RHPORT  = 0,
  DADDR   = 1,
  EP_OUT  = 0x02,
  MPS     = 64,
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

// what hcd_edpt_xfer() was given, copied when it was called
static uint8_t  xfer_count;
static uint8_t  xfer_data[4][MPS];
static uint16_t xfer_len[4];
static bool     xfer_result;
static bool   (*xfer_hook)(uint8_t call);
static void   (*refusal_hook)(void); // run once, at the refused transfer's log

int refusal_log(const char *format, ...) {
  void (*hook)(void) = refusal_hook;
  if (hook != NULL && strcmp(format, "Failed\r\n") == 0) {
    refusal_hook = NULL;
    hook();
  }
  return 0;
}

//--------------------------------------------------------------------+
// HCD stubs
//--------------------------------------------------------------------+
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) daddr;
  (void) setup_packet;
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(xfer_len), xfer_count);
  const uint8_t call = xfer_count++;
  if (buflen > 0) {
    memcpy(xfer_data[call], buffer, buflen);
  }
  xfer_len[call] = buflen;
  return xfer_hook != NULL ? xfer_hook(call) : xfer_result;
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
static volatile uint8_t *ep_state(void) {
  return &get_device(DADDR)->ep_status[tu_edpt_number(EP_OUT)][TUSB_DIR_OUT];
}

static bool ep_claimed(void) {
  return (*ep_state() & TU_EDPT_STATE_CLAIMED) != 0;
}

static void stream_setup(uint8_t *buf) {
  tu_memclr(&stream, sizeof(stream));
  TEST_ASSERT_TRUE(tu_edpt_stream_init(&stream, true, true, false, ff_buf, sizeof(ff_buf), buf));
  tu_edpt_stream_open(&stream, DADDR, &desc_ep, MPS);
}

void setUp(void) {
  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
    clear_device(&_usbh_devices[i]);
  }
  usbh_device_t *dev   = get_device(DADDR);
  dev->bus_info.rhport = RHPORT;
  dev->connected       = 1;
  dev->addressed       = 1;
  dev->configured      = 1;

  xfer_count  = 0;
  xfer_result = true;
  xfer_hook   = NULL;
  refusal_hook = NULL;
  tu_memclr(xfer_len, sizeof(xfer_len));
  stream_setup(ep_buf);
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// usbh_edpt_xfer_keep_claim
//--------------------------------------------------------------------+
void test_xfer_keep_claim_hcd_refusal_keeps_claim(void) {
  TEST_ASSERT_TRUE(usbh_edpt_claim(DADDR, EP_OUT));
  xfer_result = false;
  TEST_ASSERT_FALSE(usbh_edpt_xfer_keep_claim(DADDR, EP_OUT, ep_buf, 8));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_FALSE(usbh_edpt_busy(DADDR, EP_OUT));
  TEST_ASSERT_FALSE(usbh_edpt_claim(DADDR, EP_OUT));
  TEST_ASSERT_TRUE(usbh_edpt_release(DADDR, EP_OUT));
}

void test_xfer_keep_claim_unaligned_keeps_claim(void) {
  TEST_ASSERT_TRUE(usbh_edpt_claim(DADDR, EP_OUT));
  TEST_ASSERT_FALSE(usbh_edpt_xfer_keep_claim(DADDR, EP_OUT, ep_buf + 1, 8));
  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_FALSE(usbh_edpt_busy(DADDR, EP_OUT));
  TEST_ASSERT_FALSE(usbh_edpt_claim(DADDR, EP_OUT));
  TEST_ASSERT_TRUE(usbh_edpt_release(DADDR, EP_OUT));
}

void test_xfer_releases_claim_on_refusal(void) {
  TEST_ASSERT_TRUE(usbh_edpt_claim(DADDR, EP_OUT));
  xfer_result = false;
  TEST_ASSERT_FALSE(usbh_edpt_xfer(DADDR, EP_OUT, ep_buf, 8));
  TEST_ASSERT_FALSE(ep_claimed());

  TEST_ASSERT_TRUE(usbh_edpt_claim(DADDR, EP_OUT));
  TEST_ASSERT_FALSE(usbh_edpt_xfer(DADDR, EP_OUT, ep_buf + 1, 8));
  TEST_ASSERT_FALSE(ep_claimed());
}

//--------------------------------------------------------------------+
// Stream write
//--------------------------------------------------------------------+

// A refused packet is kept as tx_pending and the endpoint released; the next flush resends it first, topped up
void test_stream_refused_xfer_resent_first_and_topped_up(void) {
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  xfer_result = false;
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL_UINT16(3, stream.tx_pending);
  TEST_ASSERT_TRUE(tu_fifo_empty(&stream.ff));
  TEST_ASSERT_FALSE(ep_claimed());

  xfer_result = true;
  TEST_ASSERT_EQUAL_UINT32(2, tu_edpt_stream_write(&stream, "de", 2));
  TEST_ASSERT_EQUAL(2, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(5, xfer_len[1]);
  TEST_ASSERT_EQUAL_MEMORY("abcde", xfer_data[1], 5);
  TEST_ASSERT_EQUAL_UINT16(0, stream.tx_pending);
  TEST_ASSERT_TRUE(usbh_edpt_busy(DADDR, EP_OUT));
}

void test_stream_unaligned_ep_buf_keeps_data(void) {
  stream_setup(ep_buf + 1);
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(3, stream.tx_pending);
  TEST_ASSERT_EQUAL_MEMORY("abc", ep_buf + 1, 3);
  TEST_ASSERT_FALSE(ep_claimed());
}

// another context flushes and sends a ZLP while the stream's own submission is in the HCD, which then refuses it
static bool refuse_after_competitor(uint8_t call) {
  TEST_ASSERT_EQUAL(0, call); // the competitor never reaches the HCD
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS));
  return false;
}

void test_stream_competitor_during_refused_submission(void) {
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  xfer_hook = refuse_after_competitor;
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(3, stream.tx_pending);
  TEST_ASSERT_EQUAL_MEMORY("abc", ep_buf, 3);
  TEST_ASSERT_FALSE(ep_claimed());
}

// another context flushes, with new data queued, once the refusal cleared BUSY and before the refused bytes are kept
static void competitor_after_refusal(void) {
  TEST_ASSERT_FALSE(usbh_edpt_busy(DADDR, EP_OUT));
  TEST_ASSERT_TRUE(ep_claimed());
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "XYZ", 3));
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS));
}

void test_stream_competitor_after_refusal(void) {
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  xfer_result  = false;
  refusal_hook = competitor_after_refusal;
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_NULL(refusal_hook);
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(3, stream.tx_pending);
  TEST_ASSERT_EQUAL_MEMORY("abc", ep_buf, 3);
  TEST_ASSERT_FALSE(ep_claimed());

  xfer_result = true;
  TEST_ASSERT_EQUAL_UINT32(6, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL_MEMORY("abcXYZ", xfer_data[1], 6);
}

// the transfer completes and tuh_task() runs the class callback, which flushes again, before hcd_edpt_xfer() returns
static bool complete_and_flush(uint8_t call) {
  if (call == 0) {
    *ep_state() &= (uint8_t) ~(TU_EDPT_STATE_BUSY | TU_EDPT_STATE_CLAIMED); // as usbh_process_event() does
    TEST_ASSERT_EQUAL_UINT32(MPS, tu_edpt_stream_write_xfer(&stream));
  }
  return true;
}

void test_stream_completion_before_submit_returns(void) {
  uint8_t src[2 * MPS];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = (uint8_t) i;
  }

  xfer_hook = complete_and_flush;
  TEST_ASSERT_EQUAL_UINT32(sizeof(src), tu_edpt_stream_write(&stream, src, sizeof(src)));

  TEST_ASSERT_EQUAL(2, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(MPS, xfer_len[0]);
  TEST_ASSERT_EQUAL_MEMORY(src, xfer_data[0], MPS);
  TEST_ASSERT_EQUAL_UINT16(MPS, xfer_len[1]);
  TEST_ASSERT_EQUAL_MEMORY(src + MPS, xfer_data[1], MPS);
  TEST_ASSERT_EQUAL_UINT16(0, stream.tx_pending);
  TEST_ASSERT_TRUE(tu_fifo_empty(&stream.ff));
  TEST_ASSERT_TRUE(usbh_edpt_busy(DADDR, EP_OUT));
}

//--------------------------------------------------------------------+
// Stream ZLP
//--------------------------------------------------------------------+
void test_stream_zlp_sent_when_idle(void) {
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, 0));
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS - 1));
  TEST_ASSERT_FALSE(ep_claimed());

  TEST_ASSERT_TRUE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(0, xfer_len[0]);
  TEST_ASSERT_TRUE(usbh_edpt_busy(DADDR, EP_OUT));
}

// The data check is made under the claim: data a writer queued meanwhile is sent instead of the ZLP
void test_stream_zlp_sends_data_queued_meanwhile(void) {
  TEST_ASSERT_EQUAL_UINT32(1, tu_edpt_stream_write(&stream, "x", 1));
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(1, xfer_len[0]);
  TEST_ASSERT_EQUAL_UINT8('x', xfer_data[0][0]);
  TEST_ASSERT_TRUE(usbh_edpt_busy(DADDR, EP_OUT));
}

void test_stream_zlp_sends_pending_bytes(void) {
  memcpy(ep_buf, "abc", 3);
  stream.tx_pending = 3;
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(3, xfer_len[0]);
  TEST_ASSERT_EQUAL_MEMORY("abc", xfer_data[0], 3);
  TEST_ASSERT_EQUAL_UINT16(0, stream.tx_pending);
}

void test_stream_zlp_refused_releases(void) {
  xfer_result = false;
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_FALSE(ep_claimed());
  TEST_ASSERT_FALSE(usbh_edpt_busy(DADDR, EP_OUT));
}

//--------------------------------------------------------------------+
// Stream clear
//--------------------------------------------------------------------+

// another context clears the stream and queues new data before the refused bytes are kept: only the new data is sent
static void clear_after_refusal(void) {
  tu_edpt_stream_clear(&stream);
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "new", 3));
}

void test_stream_clear_during_refused_submission(void) {
  uint8_t src[MPS + 3];
  memset(src, 'o', sizeof(src));
  xfer_result = false;
  TEST_ASSERT_EQUAL_UINT32(sizeof(src), tu_edpt_stream_write(&stream, src, sizeof(src)));
  TEST_ASSERT_EQUAL(1, xfer_count); // the write flushed one packet, refused
  TEST_ASSERT_EQUAL_UINT16(MPS, stream.tx_pending);

  refusal_hook = clear_after_refusal;
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_NULL(refusal_hook);
  TEST_ASSERT_EQUAL_UINT16(0, stream.tx_pending);
  TEST_ASSERT_EQUAL_UINT16(3, tu_fifo_count(&stream.ff));
  TEST_ASSERT_FALSE(ep_claimed());

  xfer_result = true;
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL_UINT16(3, xfer_len[2]);
  TEST_ASSERT_EQUAL_MEMORY("new", xfer_data[2], 3);
}

// A clear while a submission reads the FIFO and tx_pending leaves them to it: the submission discards what was queued
// at the clear, and the bytes a refused transfer left, before it reads; data written after the clear is kept
void test_stream_clear_defers_to_active_submission(void) {
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "old", 3));
  memcpy(ep_buf, "pnd", 3);
  stream.tx_pending = 3;

  stream.tx_active = true;
  tu_edpt_stream_clear(&stream);
  TEST_ASSERT_EQUAL_UINT16(3, tu_fifo_count(&stream.ff));
  TEST_ASSERT_EQUAL_UINT16(3, stream.tx_pending);
  stream.tx_active = false;

  TEST_ASSERT_EQUAL_UINT32(2, tu_fifo_write_n(&stream.ff, "ok", 2));
  TEST_ASSERT_EQUAL_UINT32(2, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_MEMORY("ok", xfer_data[0], 2);
  TEST_ASSERT_EQUAL_UINT16(0, stream.tx_pending);
  TEST_ASSERT_TRUE(tu_fifo_empty(&stream.ff));
}

// A clear left to a submission with nothing written after it: the submission discards and sends nothing
void test_stream_deferred_clear_leaves_nothing_to_send(void) {
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "old", 3));
  stream.tx_active = true;
  tu_edpt_stream_clear(&stream);
  stream.tx_active = false;

  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_TRUE(tu_fifo_empty(&stream.ff));
  TEST_ASSERT_FALSE(ep_claimed());
  TEST_ASSERT_EQUAL(stream.tx_clear_req, stream.tx_clear_ack);
}

// A submission that finds a clear running leaves the FIFO and tx_pending to it. The ZLP touches neither, so it is
// still sent: the clear leaves nothing queued to end the transfer instead.
void test_stream_submission_backs_off_during_clear(void) {
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  stream.tx_clearing = true;
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(3, tu_fifo_count(&stream.ff));
  TEST_ASSERT_FALSE(ep_claimed());
  TEST_ASSERT_FALSE(stream.tx_active);

  TEST_ASSERT_TRUE(tu_edpt_stream_write_zlp_if_needed(&stream, MPS));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_UINT16(0, xfer_len[0]);
  TEST_ASSERT_EQUAL_UINT16(3, tu_fifo_count(&stream.ff));
  TEST_ASSERT_FALSE(stream.tx_active);
}

void test_stream_clear_when_idle(void) {
  TEST_ASSERT_EQUAL_UINT32(3, tu_edpt_stream_write(&stream, "abc", 3));
  stream.tx_pending = 2;
  tu_edpt_stream_clear(&stream);
  TEST_ASSERT_TRUE(tu_fifo_empty(&stream.ff));
  TEST_ASSERT_EQUAL_UINT16(0, stream.tx_pending);
  TEST_ASSERT_EQUAL_UINT32(0, tu_edpt_stream_write_xfer(&stream));
  TEST_ASSERT_EQUAL(0, xfer_count);
}
