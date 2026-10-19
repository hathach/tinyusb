/* SPDX-License-Identifier: MIT */
#include "unity.h"
#include "mock_dwc2_test.h"

// Compile the driver itself so these tests exercise its admission and retry paths.
#include "../../../../../src/portable/synopsys/dwc2/hcd_dwc2.c"

static uint8_t payload[512];
static unsigned fifo_writes;

void tu_hwfifo_write(volatile void* fifo, const uint8_t* src, uint16_t len,
                     const tu_hwfifo_access_t* access_mode) {
  (void) fifo;
  (void) src;
  (void) access_mode;
  if (len > 0) {
    fifo_writes++;
  }
}

void setUp(void) {
  memset(&test_regs, 0, sizeof(test_regs));
  memset(&_hcd_data, 0, sizeof(_hcd_data));
  _hcd_csplit_behind = _hcd_csplit_pass = _hcd_csplit_late_held = _hcd_split_kicked = false;
  fifo_writes = 0;
  dwc2_ghwcfg2_t hw = {.value = 0};
  hw.num_host_ch = 15;
  test_regs.ghwcfg2 = hw.value;
  dwc2_hptxsts_t tx = {.value = 0};
  tx.req_queue_available = 8;
  tx.fifo_available = 236;
  test_regs.hptxsts = tx.value;
  test_regs.hfnum = (3500u << 16) | 6u;
}

void tearDown(void) {
}

static void endpoint(uint8_t ch, bool in, uint16_t bytes, bool iso) {
  hcd_endpoint_t* ep = &_hcd_data.edpt[ch];
  ep->hcchar_bm.ep_type = iso ? HCCHAR_EPTYPE_ISOCHRONOUS : HCCHAR_EPTYPE_INTERRUPT;
  ep->hcchar_bm.ep_dir = in;
  ep->hcchar_bm.ep_size = iso ? 188 : 64;
  ep->hcchar_bm.dev_addr = 1;
  ep->hcchar_bm.ep_num = 1 + ch % 15;
  ep->hcsplt_bm.split_en = 1;
  ep->hcsplt_bm.hub_addr = 5;
  ep->hcsplt_bm.hub_port = 1;
  ep->uframe_interval = 8;
  ep->split_slot = 0;
  ep->buffer = payload;
  ep->buflen = bytes;
  _hcd_data.xfer[ch].allocated = true;
  _hcd_data.xfer[ch].ep_id = ch;
  test_regs.channel[ch].hcchar = ep->hcchar;
  test_regs.channel[ch].hcsplt = ep->hcsplt;
  dwc2_channel_tsize_t ts = {.value = 0};
  ts.packet_count = 1;
  ts.xfer_size = bytes;
  test_regs.channel[ch].hctsiz = ts.value;
}

static void enable(uint8_t ch) {
  const uint16_t target = (test_regs.hfnum + 1u) & HCD_FRAME_NUMBER_MASK;
  TEST_ASSERT_EQUAL(target, channel_enable(&test_regs, &test_regs.channel[ch], true, target & 7u));
}

// After both SS complete, the second one's CS must wait for the first one's CS.
// Observe CHENA, rather than asserting the implementation's sequence numbers.
static void complete_pair(uint8_t first, uint8_t second) {
  for (uint8_t i = 0; i < 2; i++) {
    const uint8_t ch = i ? second : first;
    test_regs.channel[ch].hcchar &= ~HCCHAR_CHENA;
    test_regs.channel[ch].hcsplt |= HCSPLT_COMPLSPLT;
    test_regs.channel[ch].hcint = 0;
    _hcd_data.xfer[ch].csplit_next = 2;
    _hcd_data.xfer[ch].csplit_wait = 1;
  }
  test_regs.hfnum = (_hcd_data.xfer[first].split_uframe + 1u) & HCD_FRAME_NUMBER_MASK;
  TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, second));
  TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[second].hcchar);
  TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, first));
  TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[first].hcchar);
  TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, second));
  TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[second].hcchar);

  // NYET retries must retain the original SS order.
  test_regs.hfnum = (_hcd_data.xfer[first].split_uframe + 2u) & HCD_FRAME_NUMBER_MASK;
  test_regs.channel[first].hcchar &= ~HCCHAR_CHENA;
  test_regs.channel[second].hcchar &= ~HCCHAR_CHENA;
  TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, second));
  TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[second].hcchar);
  TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, first));
  TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[first].hcchar);
}

void test_deferred_out_complete_split_follows_the_in_that_entered_first(void) {
  // Both channel orders, FIFO shortage and request-queue shortage, frame rollover.
  for (unsigned variant = 0; variant < 8; variant++) {
    setUp();
    const uint8_t out = variant & 1u;
    const uint8_t in = 1u - out;
    endpoint(out, false, 64, false);
    endpoint(in, true, 64, false);
    if (variant & 4u) {
      test_regs.hfnum = (3500u << 16) | (HCD_FRAME_NUMBER_MASK - 1u);
    }
    dwc2_hptxsts_t tx = {.value = test_regs.hptxsts};
    tx.fifo_available = (variant & 2u) ? 236 : 12;
    tx.req_queue_available = (variant & 2u) ? 0 : 3;
    test_regs.hptxsts = tx.value;
    enable(out);
    TEST_ASSERT_TRUE(channel_txfifo_write(&test_regs, out, true));
    TEST_ASSERT_EQUAL(0, fifo_writes);
    tx.req_queue_available = 3;
    test_regs.hptxsts = tx.value;
    enable(in);
    tx.fifo_available = 236;
    test_regs.hptxsts = tx.value;
    TEST_ASSERT_FALSE(channel_txfifo_write(&test_regs, out, true));
    TEST_ASSERT_EQUAL(1, fifo_writes);
    complete_pair(in, out);
  }
}

void test_immediate_out_zlp_and_dma_keep_their_order(void) {
  const uint16_t lengths[] = {64, 7, 0, 64};
  for (unsigned i = 0; i < TU_ARRAY_SIZE(lengths); i++) {
    setUp();
    const bool dma = i == 3;
    if (dma) {
      dwc2_ghwcfg2_t hw = {.value = test_regs.ghwcfg2};
      hw.arch = GHWCFG2_ARCH_INTERNAL_DMA;
      test_regs.ghwcfg2 = hw.value;
    }
    endpoint(0, false, lengths[i], false);
    endpoint(1, true, 64, false);
    enable(0);
    if (lengths[i] && !dma) {
      TEST_ASSERT_FALSE(channel_txfifo_write(&test_regs, 0, true));
    }
    enable(1);
    if (lengths[i] && !dma) {
      TEST_ASSERT_FALSE(channel_txfifo_write(&test_regs, 0, true));
      TEST_ASSERT_EQUAL(1, fifo_writes);
    }
    complete_pair(0, 1);
  }
}

void test_unissued_channels_at_frame_zero_do_not_take_order_numbers(void) {
  test_regs.hfnum = (3500u << 16) | HCD_FRAME_NUMBER_MASK;
  for (uint8_t ch = 0; ch < 16; ch++) {
    endpoint(ch, true, 8, false);
    _hcd_data.edpt[ch].split_slot = 1;
  }
  for (uint8_t ch = 0; ch < 16; ch++) {
    enable(ch);
    TEST_ASSERT_EQUAL(ch, _hcd_data.xfer[ch].split_seq);
  }
}

void test_iso_out_reserves_tt_while_its_first_fifo_write_waits(void) {
  endpoint(0, false, 400, true);
  endpoint(1, true, 64, false);
  dwc2_hptxsts_t tx = {.value = test_regs.hptxsts};
  tx.fifo_available = 0;
  test_regs.hptxsts = tx.value;
  enable(0);
  TEST_ASSERT_TRUE(channel_txfifo_write(&test_regs, 0, true));
  TEST_ASSERT_EQUAL(HCD_PERIODIC_FRAME_NOT_STARTED,
                    channel_enable(&test_regs, &test_regs.channel[1], true, 7));
  TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[1].hcchar);
  TEST_ASSERT_EQUAL(0, fifo_writes);
  tx.fifo_available = 236;
  test_regs.hptxsts = tx.value;
  TEST_ASSERT_FALSE(channel_txfifo_write(&test_regs, 0, true));
  TEST_ASSERT_TRUE(_hcd_data.xfer[0].iso_open);
  channel_dealloc(&test_regs, 0);
  TEST_ASSERT_EQUAL(0, channel_alloc(&test_regs));
  TEST_ASSERT_FALSE(_hcd_data.xfer[0].iso_open);
}

static void pending_csplit(uint8_t ch, bool in, uint16_t ss_frame, uint8_t next) {
  endpoint(ch, in, 64, false);
  test_regs.channel[ch].hcsplt |= HCSPLT_COMPLSPLT;
  _hcd_data.xfer[ch].split_uframe = ss_frame & HCD_FRAME_NUMBER_MASK;
  _hcd_data.xfer[ch].csplit_next = next;
  _hcd_data.xfer[ch].csplit_wait = 1;
  _hcd_data.xfer[ch].result = XFER_RESULT_INVALID;
}

void test_pending_receive_or_channel_work_precedes_future_complete_split(void) {
  const uint32_t causes[] = {GINTSTS_RXFLVL, GINTSTS_HCINT, GINTSTS_RXFLVL | GINTSTS_HCINT};
  for (unsigned variant = 0; variant < 12; variant++) {
    setUp();
    const uint8_t a = variant & 1u;
    const uint8_t b = 1u - a;
    const uint16_t now = (variant & 2u) ? 0 : 10;
    pending_csplit(a, false, (uint16_t) (now - 1u), 2);
    pending_csplit(b, true, (uint16_t) (now - 3u), 3);
    _hcd_data.edpt[b].hcsplt_bm.hub_port = 4; // Different TT, shared periodic queue.
    _hcd_data.edpt[a].tt_multi = _hcd_data.edpt[b].tt_multi = 1;
    test_regs.channel[b].hcsplt = _hcd_data.edpt[b].hcsplt | HCSPLT_COMPLSPLT;
    test_regs.hfnum = (3500u << 16) | now;
    test_regs.gintmsk = GINTSTS_SOF | GINTSTS_RXFLVL | GINTSTS_HCINT;
    test_regs.gintsts = causes[variant / 4];

    // A is for the next microframe; B's genuine NYET halt still needs service.
    _hcd_data.xfer[b].csplit_wait = 0;
    _hcd_data.xfer[b].halted_nyet = 1;
    test_regs.channel[b].hcintmsk = HCINT_HALTED;
    TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, a));
    TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[a].hcchar);

    // Model RX having exposed ChHltd; exercise the real slave halt/retry handler.
    test_regs.gintsts = GINTSTS_HCINT;
    TEST_ASSERT_FALSE(handle_channel_in_slave(&test_regs, b, HCINT_HALTED));
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[b].hcchar);
    TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[a].hcchar);
    TEST_ASSERT_FALSE(_hcd_data.xfer[b].halted_nyet);

    // With pending work drained, the real IRQ tail admits the deferred A.
    test_regs.gintsts = 0;
    hcd_int_handler(0, true);
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[a].hcchar);
  }
}

void test_rx_without_channel_interrupt_releases_future_split_in_irq_tail(void) {
  pending_csplit(0, false, 9, 2);
  pending_csplit(1, true, 7, 3);
  _hcd_data.xfer[1].csplit_wait = 0;
  _hcd_data.edpt[1].hcsplt_bm.hub_port = 4;
  _hcd_data.edpt[0].tt_multi = _hcd_data.edpt[1].tt_multi = 1;
  test_regs.channel[1].hcsplt = _hcd_data.edpt[1].hcsplt | HCSPLT_COMPLSPLT;
  test_regs.hfnum = (3500u << 16) | 10;
  test_regs.gintmsk = GINTSTS_SOF | GINTSTS_RXFLVL;
  test_regs.gintsts = GINTSTS_RXFLVL;
  TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, 0));
  TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);

  // Model receive status drained without generating a channel interrupt.
  test_regs.gintsts = 0;
  hcd_int_handler(0, true);
  TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
  TEST_ASSERT_FALSE(_hcd_data.xfer[0].csplit_wait);
}

void test_unrelated_pending_work_does_not_delay_iso_in_continuation(void) {
  const uint32_t causes[] = {GINTSTS_RXFLVL, GINTSTS_HCINT, GINTSTS_RXFLVL | GINTSTS_HCINT};
  for (unsigned cause = 0; cause < TU_ARRAY_SIZE(causes); cause++) {
    for (unsigned other = 0; other < 9; other++) {
      setUp();
      pending_csplit(0, true, 8, 3);
      _hcd_data.edpt[0].hcchar_bm.ep_type = HCCHAR_EPTYPE_ISOCHRONOUS;
      test_regs.channel[0].hcchar = _hcd_data.edpt[0].hcchar;
      pending_csplit(1, true, 8, 2);
      _hcd_data.xfer[1].csplit_wait = 0;
      switch (other) {
        case 0: _hcd_data.edpt[1].hcchar_bm.ep_type = HCCHAR_EPTYPE_CONTROL; break;
        case 1: _hcd_data.edpt[1].hcchar_bm.ep_type = HCCHAR_EPTYPE_BULK; break;
        case 2: _hcd_data.edpt[1].hcsplt_bm.split_en = 0; break;
        case 3:
          _hcd_data.edpt[1].hcchar_bm.ep_type = HCCHAR_EPTYPE_ISOCHRONOUS;
          _hcd_data.edpt[1].hcchar_bm.ep_dir = TUSB_DIR_OUT;
          break;
        case 4: _hcd_data.xfer[1].allocated = false; break;
        case 5: _hcd_data.xfer[1].closing = 1; break;
        case 6: _hcd_data.xfer[1].aborting = 1; break;
        case 7:
          _hcd_data.xfer[1].split_uframe = 10;
          test_regs.channel[1].hcsplt &= ~HCSPLT_COMPLSPLT; // First CS still in the future.
          break;
        case 8: _hcd_data.xfer[1].csplit_next = 4; break; // Continuation still in the future.
      }
      test_regs.channel[1].hcchar = _hcd_data.edpt[1].hcchar;
      test_regs.hfnum = (3500u << 16) | 10u;
      test_regs.gintmsk = GINTSTS_SOF | GINTSTS_RXFLVL | GINTSTS_HCINT;
      test_regs.gintsts = causes[cause];
      TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, 0));
      TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
      TEST_ASSERT_FALSE(_hcd_data.xfer[0].csplit_wait);
    }
  }
}

void test_pending_start_ack_or_iso_payload_preserves_current_split_priority(void) {
  for (unsigned variant = 0; variant < 8; variant++) {
    setUp();
    const uint16_t now = (variant & 1u) ? 0 : 10;
    const bool start_ack = (variant & 2u) != 0;
    pending_csplit(0, false, (uint16_t) (now - 1u), 2);
    pending_csplit(1, true, (uint16_t) (now - (start_ack ? 2u : 3u)), 3);
    _hcd_data.edpt[1].hcchar_bm.ep_type = HCCHAR_EPTYPE_ISOCHRONOUS;
    _hcd_data.edpt[1].hcsplt_bm.hub_port = 4;
    _hcd_data.edpt[0].tt_multi = _hcd_data.edpt[1].tt_multi = 1;
    test_regs.channel[1].hcchar = _hcd_data.edpt[1].hcchar;
    test_regs.channel[1].hcsplt = _hcd_data.edpt[1].hcsplt | (start_ack ? 0 : HCSPLT_COMPLSPLT);
    _hcd_data.xfer[1].csplit_wait = 0;
    test_regs.hfnum = (3500u << 16) | now;
    test_regs.gintmsk = GINTSTS_SOF | GINTSTS_RXFLVL | GINTSTS_HCINT;
    test_regs.gintsts = (variant & 4u) ? GINTSTS_HCINT : GINTSTS_RXFLVL;
    TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, 0));
    TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);

    // The pending ACK/payload handler makes B's current CS eligible.
    test_regs.channel[1].hcsplt |= HCSPLT_COMPLSPLT;
    _hcd_data.xfer[1].csplit_next = start_ack ? 2 : 3;
    _hcd_data.xfer[1].csplit_wait = 1;
    TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, 1));
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[1].hcchar);
    TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);
    test_regs.gintsts = 0;
    hcd_int_handler(0, true);
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
  }
}

void test_pending_work_keeps_current_iso_and_masked_status_behavior(void) {
  for (unsigned variant = 0; variant < 8; variant++) {
    setUp();
    if (variant & 1u) {
      dwc2_ghwcfg2_t hw = {.value = test_regs.ghwcfg2};
      hw.arch = GHWCFG2_ARCH_INTERNAL_DMA;
      test_regs.ghwcfg2 = hw.value;
    }
    pending_csplit(0, true, 9, 2);
    const bool iso = (variant & 2u) != 0;
    const bool current = (variant & 4u) != 0;
    if (iso) {
      _hcd_data.edpt[0].hcchar_bm.ep_type = HCCHAR_EPTYPE_ISOCHRONOUS;
      test_regs.channel[0].hcchar = _hcd_data.edpt[0].hcchar;
    }
    test_regs.hfnum = (3500u << 16) | (current ? 11u : 10u);
    test_regs.gintsts = GINTSTS_RXFLVL | GINTSTS_HCINT;
    test_regs.gintmsk = GINTSTS_SOF | (current ? test_regs.gintsts : 0);
    TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, 0));
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
    if (iso) {
      // The new guard does not allow an ISO IN continuation to skip its window.
      test_regs.channel[0].hcchar &= ~HCCHAR_CHENA;
      test_regs.hfnum = (3500u << 16) | 13u;
      TEST_ASSERT_FALSE(channel_csplit_arm(&test_regs, 0));
      TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);
    }
  }
}
