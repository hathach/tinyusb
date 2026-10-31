/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "mock_dwc2_test.h"

// Compile the driver itself so these tests exercise its admission and retry paths.
#include "../../../../../src/portable/synopsys/dwc2/hcd_dwc2.c"

static uint8_t payload[512];
static void (*on_int_enable)(void);
static unsigned fifo_writes;
static uint32_t gahbcfg_at_fifo_write;
static bool     fifo_write_unmasks; // models another core ending its own masked region meanwhile

void tu_hwfifo_write(volatile void* fifo, const uint8_t* src, uint16_t len,
                     const tu_hwfifo_access_t* access_mode) {
  (void) fifo;
  (void) src;
  (void) access_mode;
  if (len > 0) {
    fifo_writes++;
  }
  gahbcfg_at_fifo_write = test_regs.gahbcfg;
  if (fifo_write_unmasks) {
    test_regs.gahbcfg |= GAHBCFG_GINT;
  }
}

void setUp(void) {
  memset(&test_regs, 0, sizeof(test_regs));
  memset(&_hcd_data, 0, sizeof(_hcd_data));
  _hcd_csplit_behind = _hcd_csplit_pass = _hcd_csplit_late_held = _hcd_split_kicked = false;
  _hcd_nonperiodic_uframe = 0;
  on_int_enable = NULL;
  _tuh_cfg.max_nak = HCD_XFER_NAK_DEFAULT;
  fifo_writes = 0;
  fifo_write_unmasks = false;
  dwc2_ghwcfg2_t hw = {.value = 0};
  hw.num_host_ch = 15;
  test_regs.ghwcfg2 = hw.value;
  dwc2_hptxsts_t tx = {.value = 0};
  tx.req_queue_available = 8;
  tx.fifo_available = 236;
  test_regs.hptxsts = tx.value;
  test_regs.hfir = 7499; // high-speed microframe: the tail is FrRem below 937
  test_regs.hfnum = (3500u << 16) | 6u;
}

void tearDown(void) {
}

static void set_ptx_queue(uint8_t available) {
  dwc2_hptxsts_t tx = {.value = test_regs.hptxsts};
  tx.req_queue_available = available;
  test_regs.hptxsts = tx.value;
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
    test_regs.hptxsts = tx.value;
    set_ptx_queue((variant & 2u) ? 0 : 3);
    enable(out);
    TEST_ASSERT_TRUE(channel_txfifo_write(&test_regs, out, true));
    TEST_ASSERT_EQUAL(0, fifo_writes);
    set_ptx_queue(3);
    enable(in);
    tx.value = test_regs.hptxsts;
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
}

void test_iso_out_end_starts_the_interrupt_split_held_behind_it(void) {
  endpoint(0, false, 400, true);
  endpoint(1, true, 64, false);
  _hcd_data.xfer[1].allocated = false; // its transfer has no channel yet
  enable(0);
  TEST_ASSERT_FALSE(channel_txfifo_write(&test_regs, 0, true));
  TEST_ASSERT_TRUE(_hcd_data.xfer[0].iso_open);

  // Due in the microframe of the BEGIN, after it: held for the END
  TEST_ASSERT_TRUE(edpt_xfer_kickoff(&test_regs, 1));
  TEST_ASSERT_TRUE(_hcd_data.edpt[1].split_held);
  TEST_ASSERT_TRUE(_hcd_data.edpt[1].xfer_pending);
  TEST_ASSERT_FALSE(_hcd_data.xfer[1].allocated);

  // Queueing the END closes the packet and starts the held transfer at once
  split_iso_out_close(&test_regs, 0);
  TEST_ASSERT_FALSE(_hcd_data.xfer[0].iso_open);
  TEST_ASSERT_TRUE(_hcd_split_kicked);
  TEST_ASSERT_FALSE(_hcd_data.edpt[1].xfer_pending);
  TEST_ASSERT_FALSE(_hcd_data.edpt[1].split_held);
  TEST_ASSERT_TRUE(_hcd_data.xfer[1].allocated);
  TEST_ASSERT_EQUAL(1, _hcd_data.xfer[1].ep_id);
  TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[1].hcchar);
}

static void pending_csplit(uint8_t ch, bool in, uint16_t ss_frame, uint8_t next) {
  endpoint(ch, in, 64, false);
  test_regs.channel[ch].hcsplt |= HCSPLT_COMPLSPLT;
  _hcd_data.xfer[ch].split_uframe = ss_frame & HCD_FRAME_NUMBER_MASK;
  _hcd_data.xfer[ch].csplit_next = next;
  _hcd_data.xfer[ch].csplit_wait = 1;
  _hcd_data.xfer[ch].split_issued = 1;
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

    if (causes[variant / 4] & GINTSTS_HCINT) {
      // B's halt goes through the channel interrupt pass, which admits the deferred A at its end.
      test_regs.channel[b].hcint = HCINT_HALTED;
      test_regs.haint = 1u << b;
      test_regs.gintsts = GINTSTS_HCINT;
      hcd_int_handler(0, true);
      TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[b].hcchar);
      TEST_ASSERT_FALSE(_hcd_data.xfer[b].halted_nyet);
      TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[a].hcchar);
      continue;
    }

    // RX exposed ChHltd: exercise the real slave halt/retry handler.
    TEST_ASSERT_FALSE(handle_channel_in_slave(&test_regs, b, HCINT_HALTED));
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[b].hcchar);
    TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[a].hcchar);
    TEST_ASSERT_FALSE(_hcd_data.xfer[b].halted_nyet);

    // With the receive status drained, the IRQ tail admits the deferred A.
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
    for (unsigned other = 0; other < 10; other++) {
      setUp();
      pending_csplit(0, true, 8, 3);
      _hcd_data.edpt[0].hcchar_bm.ep_type = HCCHAR_EPTYPE_ISOCHRONOUS;
      test_regs.channel[0].hcchar = _hcd_data.edpt[0].hcchar;
      pending_csplit(1, true, 8, 2);
      _hcd_data.xfer[1].csplit_wait = 0;
      switch (other) {
        case 0: _hcd_data.edpt[1].hcchar_bm.ep_type = HCCHAR_EPTYPE_CONTROL; break;
        case 1: _hcd_data.edpt[1].hcchar_bm.ep_type = HCCHAR_EPTYPE_BULK; break;
        case 2:
          _hcd_data.edpt[1].hcsplt_bm.split_en = 0;
          test_regs.channel[1].hcsplt = 0;
          break;
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
        case 9: // Allocated, its start-split not yet enabled.
          _hcd_data.xfer[1].split_issued = 0;
          _hcd_data.xfer[1].split_uframe = 0;
          test_regs.channel[1].hcsplt &= ~HCSPLT_COMPLSPLT;
          break;
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

    if (variant & 4u) {
      // B's ACK or payload goes through the channel interrupt pass, which admits the deferred A at its end.
      dwc2_channel_tsize_t ts = {.value = test_regs.channel[1].hctsiz};
      ts.pid = HCTSIZ_PID_MDATA;
      test_regs.channel[1].hctsiz = ts.value;
      _hcd_data.xfer[1].csplit_next = 3;
      test_regs.channel[1].hcint = start_ack ? HCINT_ACK : HCINT_XFER_COMPLETE;
      test_regs.haint = 2u;
      test_regs.gintsts = GINTSTS_HCINT;
      hcd_int_handler(0, true);
      TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[1].hcchar);
      TEST_ASSERT_BITS_HIGH(HCSPLT_COMPLSPLT, test_regs.channel[1].hcsplt);
      TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
      continue;
    }

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
    const bool iso = (variant & 2u) != 0;
    const bool current = (variant & 4u) != 0;
    const uint16_t now = current ? 11u : 10u;
    pending_csplit(0, true, 9, 2);
    // Another periodic split, on another TT, has a complete-split due in the current microframe.
    pending_csplit(1, true, (uint16_t) (now - 3u), 3);
    _hcd_data.xfer[1].csplit_wait = 0;
    _hcd_data.edpt[1].hcsplt_bm.hub_port = 4;
    _hcd_data.edpt[0].tt_multi = _hcd_data.edpt[1].tt_multi = 1;
    test_regs.channel[1].hcsplt = _hcd_data.edpt[1].hcsplt | HCSPLT_COMPLSPLT;
    if (iso) {
      _hcd_data.edpt[0].hcchar_bm.ep_type = HCCHAR_EPTYPE_ISOCHRONOUS;
      test_regs.channel[0].hcchar = _hcd_data.edpt[0].hcchar;
    }
    test_regs.hfnum = (3500u << 16) | now;
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

void test_late_complete_split_in_the_microframe_tail_goes_in_the_next(void) {
  for (unsigned tail = 0; tail < 2; tail++) {
    setUp();
    pending_csplit(0, true, 8, 2);
    test_regs.hfnum = ((tail ? 900u : 1000u) << 16) | 10u; // its first complete-split is due now
    TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, 0));
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
    TEST_ASSERT_EQUAL(tail ? 4 : 3, _hcd_data.xfer[0].csplit_next);
    TEST_ASSERT_EQUAL(tail ? HCCHAR_ODDFRM : 0, test_regs.channel[0].hcchar & HCCHAR_ODDFRM);
  }
}

void test_slave_complete_split_never_spins_on_a_full_periodic_queue(void) {
  // An IN at the head of a full queue waits for RxFIFO space, which the RxFIFO service after SOF in the ISR frees
  pending_csplit(0, true, 8, 2);
  _hcd_data.xfer[0].csplit_wait = 0;
  test_regs.hfnum = (3500u << 16) | 10u; // its first complete-split is due now
  set_ptx_queue(0);
  TEST_ASSERT_TRUE(channel_csplit_arm(&test_regs, 0));
  TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);
  TEST_ASSERT_TRUE(_hcd_data.xfer[0].csplit_wait);
  TEST_ASSERT_BITS_HIGH(GINTMSK_SOFM, test_regs.gintmsk);

  // the ISR tail retries it once the RxFIFO service has freed an entry
  set_ptx_queue(1);
  test_regs.gintsts = 0;
  hcd_int_handler(0, true);
  TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
  TEST_ASSERT_FALSE(_hcd_data.xfer[0].csplit_wait);
  TEST_ASSERT_EQUAL(3, _hcd_data.xfer[0].csplit_next);
}

void test_iso_out_piece_for_a_microframe_the_core_no_longer_serves_fails_the_packet(void) {
  // the MID of a packet started in microframe 9 is for 10
  const struct {
    uint16_t frrem;
    uint16_t uframe;
    bool     bulk;
    bool     late;
  } cases[] = {
    {3500, 10, false, false}, // in time
    {900,  10, false, true},  // in the tail of its microframe
    {3500, 10, true,  true},  // behind a bulk transfer
    {3500, 9,  true,  false}, // a microframe ahead
    {3500, 11, false, true},  // its microframe passed
  };
  for (unsigned i = 0; i < TU_ARRAY_SIZE(cases); i++) {
    setUp();
    endpoint(0, false, 400, true);
    _hcd_data.xfer[0].split_uframe = 9;
    _hcd_data.xfer[0].iso_open = 1;
    if (cases[i].bulk) {
      endpoint(1, true, 64, false);
      _hcd_data.edpt[1].hcchar_bm.ep_type = HCCHAR_EPTYPE_BULK;
    }
    test_regs.hfnum = ((uint32_t) cases[i].frrem << 16) | cases[i].uframe;
    TEST_ASSERT_EQUAL(!cases[i].late, channel_iso_out_next(&test_regs, 0));
    if (cases[i].late) {
      TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, _hcd_data.xfer[0].result);
      TEST_ASSERT_FALSE(_hcd_data.xfer[0].iso_open);
      TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);
    } else {
      const dwc2_channel_split_t hcsplt = {.value = test_regs.channel[0].hcsplt};
      TEST_ASSERT_EQUAL(HCD_SPLIT_XACTPOS_MID, hcsplt.xact_pos);
      TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
    }
  }
}

// hcd_int_enable() lets the ISR run: on_int_enable models it, the first time only.
static unsigned isr_runs;
static bool     int_enabled;

static void int_set_stub(uint8_t rhport, tusb_role_t role, bool enabled, int cmock_num_calls) {
  (void) rhport;
  (void) role;
  (void) cmock_num_calls;
  int_enabled = enabled;
  if (enabled && on_int_enable) {
    void (*isr)(void) = on_int_enable;
    on_int_enable = NULL;
    isr_runs++;
    isr();
  }
}

static void isr_frees_request_queue(void) {
  set_ptx_queue(1);
}

// channel 0 halts and is retired, then taken by another endpoint's transfer
static void isr_retires_and_reallocates_channel(void) {
  isr_frees_request_queue();
  channel_dealloc(&test_regs, 0);
  TEST_ASSERT_EQUAL(0, channel_alloc(&test_regs));
  _hcd_data.xfer[0].ep_id = 2;
  test_regs.channel[0].hcchar = _hcd_data.edpt[2].hcchar;
}

static void abort_setup(void (*isr)(void)) {
  endpoint(0, true, 64, false);
  _hcd_data.edpt[0].hcchar_bm.enable = 1; // opened, its channel enabled
  test_regs.channel[0].hcchar = _hcd_data.edpt[0].hcchar;
  endpoint(2, true, 64, false);
  _hcd_data.xfer[2].allocated = false;
  set_ptx_queue(0); // a disable must wait for an entry
  isr_runs = 0;
  on_int_enable = isr;
  dwc2_int_set_Stub(int_set_stub);
}

void test_abort_waits_for_request_queue_space_with_the_interrupt_enabled(void) {
  abort_setup(isr_frees_request_queue);
  TEST_ASSERT_TRUE(hcd_edpt_abort_xfer(0, 1, 0x81));
  TEST_ASSERT_EQUAL(1, isr_runs);
  TEST_ASSERT_TRUE(int_enabled);
  TEST_ASSERT_BITS_HIGH(HCCHAR_CHDIS | HCCHAR_CHENA, test_regs.channel[0].hcchar);
  // its halt retires the channel; until then the endpoint takes no replacement transfer
  TEST_ASSERT_TRUE(_hcd_data.xfer[0].aborting);
  TEST_ASSERT_TRUE(_hcd_data.edpt[0].aborting);
  TEST_ASSERT_FALSE(hcd_edpt_xfer(0, 1, 0x81, payload, 64));
}

void test_abort_does_not_halt_a_channel_reallocated_while_it_waited(void) {
  abort_setup(isr_retires_and_reallocates_channel);
  TEST_ASSERT_TRUE(hcd_edpt_abort_xfer(0, 1, 0x81));
  TEST_ASSERT_EQUAL(1, isr_runs);
  TEST_ASSERT_TRUE(int_enabled);
  TEST_ASSERT_BITS_LOW(HCCHAR_CHDIS, test_regs.channel[0].hcchar);
  TEST_ASSERT_FALSE(_hcd_data.xfer[0].aborting);
  TEST_ASSERT_EQUAL(2, _hcd_data.xfer[0].ep_id);
  TEST_ASSERT_FALSE(_hcd_data.edpt[0].aborting);
}

// A bulk transfer on channel 0 whose NAKs the CPU retries: a split one, or with split false a native one in slave mode
static void bulk_nak_endpoint(bool in, bool split) {
  endpoint(0, in, 64, false);
  hcd_endpoint_t* ep        = &_hcd_data.edpt[0];
  ep->hcchar_bm.ep_type     = HCCHAR_EPTYPE_BULK;
  ep->hcchar_bm.enable      = 1;
  ep->hcsplt_bm.split_en    = split ? 1u : 0u;
  test_regs.channel[0].hcchar = ep->hcchar | HCCHAR_CHENA;
  test_regs.channel[0].hcsplt = ep->hcsplt;
  _hcd_data.xfer[0].result  = XFER_RESULT_INVALID;
  dwc2_hnptxsts_t nptx      = {.value = 0};
  nptx.req_queue_available  = 8;
  test_regs.hnptxsts        = nptx.value;
}

// One NAKed transaction of the bulk channel 0: the NAK, then the halt its disable brings (slave), or the halt carrying
// the NAK (DMA). Return whether the retry was armed at once.
static bool bulk_nak(bool dma, bool in) {
  if (dma) {
    test_regs.channel[0].hcchar &= ~HCCHAR_CHENA;
    TEST_ASSERT_FALSE(in ? handle_channel_in_dma(&test_regs, 0, HCINT_HALTED | HCINT_NAK) :
                           handle_channel_out_dma(&test_regs, 0, HCINT_HALTED | HCINT_NAK));
    if (in) { // the NAK disabled the channel again, re-arming on that halt
      test_regs.channel[0].hcchar &= ~(HCCHAR_CHENA | HCCHAR_CHDIS);
      TEST_ASSERT_FALSE(handle_channel_in_dma(&test_regs, 0, HCINT_HALTED));
    }
  } else {
    TEST_ASSERT_FALSE(in ? handle_channel_in_slave(&test_regs, 0, HCINT_NAK) :
                           handle_channel_out_slave(&test_regs, 0, HCINT_NAK));
    test_regs.channel[0].hcchar &= ~(HCCHAR_CHENA | HCCHAR_CHDIS);
    TEST_ASSERT_FALSE(in ? handle_channel_in_slave(&test_regs, 0, HCINT_HALTED) :
                           handle_channel_out_slave(&test_regs, 0, HCINT_HALTED));
  }
  return (test_regs.channel[0].hcchar & HCCHAR_CHENA) != 0;
}

void test_max_nak_holds_cpu_retries_until_the_next_frame(void) {
  // slave split IN, slave native IN, slave split OUT, slave native OUT, DMA split IN, DMA split OUT
  const struct {
    bool dma, in, split;
  } cases[] = {{false, true, true}, {false, true, false}, {false, false, true},
               {false, false, false}, {true, true, true}, {true, false, true}};
  for (unsigned i = 0; i < TU_ARRAY_SIZE(cases); i++) {
    setUp();
    dwc2_int_set_Ignore();
    hcd_dcache_clean_IgnoreAndReturn(true);
    if (cases[i].dma) {
      dwc2_ghwcfg2_t hw = {.value = test_regs.ghwcfg2};
      hw.arch = GHWCFG2_ARCH_INTERNAL_DMA;
      test_regs.ghwcfg2 = hw.value;
    }
    bulk_nak_endpoint(cases[i].in, cases[i].split);
    test_regs.hfnum = (3500u << 16) | (8u * 20u + 1u); // microframe 1 of frame 20
    TEST_ASSERT_EQUAL(HCD_XFER_NAK_DEFAULT, _tuh_cfg.max_nak);

    // max_nak NAKs in a frame: the first ones retry at once, the last one parks for SOF
    for (unsigned n = 1; n < HCD_XFER_NAK_DEFAULT; n++) {
      TEST_ASSERT_TRUE(bulk_nak(cases[i].dma, cases[i].in));
      TEST_ASSERT_EQUAL(PARK_NONE, _hcd_data.xfer[0].park);
    }
    TEST_ASSERT_FALSE(bulk_nak(cases[i].dma, cases[i].in));
    TEST_ASSERT_EQUAL(cases[i].in ? PARK_IN_TOKEN : PARK_XFER_START, _hcd_data.xfer[0].park);
    TEST_ASSERT_BITS_HIGH(GINTMSK_SOFM, test_regs.gintmsk);

    // SOF of a later microframe in the same frame: still parked
    test_regs.hfnum = (3500u << 16) | (8u * 20u + 7u);
    TEST_ASSERT_TRUE(handle_sof_irq(0, true));
    TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);

    // SOF of the next frame: retried, and the count starts over
    test_regs.hfnum = (3500u << 16) | (8u * 21u);
    (void) handle_sof_irq(0, true);
    TEST_ASSERT_EQUAL(PARK_NONE, _hcd_data.xfer[0].park);
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
    TEST_ASSERT_TRUE(bulk_nak(cases[i].dma, cases[i].in));
  }
}

void test_max_nak_zero_never_holds_and_abort_retires_a_held_channel(void) {
  dwc2_int_set_Ignore();
  bulk_nak_endpoint(true, true);
  test_regs.hfnum = (3500u << 16) | (8u * 20u + 1u);

  _tuh_cfg.max_nak = 0;
  for (unsigned n = 0; n < 2u * HCD_XFER_NAK_MAX; n++) {
    TEST_ASSERT_TRUE(bulk_nak(false, true));
  }

  // Held: the abort takes the channel back at once, it is not enabled and would never halt
  _tuh_cfg.max_nak = 1;
  test_regs.hfnum = (3500u << 16) | (8u * 22u);
  TEST_ASSERT_FALSE(bulk_nak(false, true));
  TEST_ASSERT_TRUE(_hcd_data.xfer[0].allocated);
  TEST_ASSERT_TRUE(hcd_edpt_abort_xfer(0, 1, 0x81));
  TEST_ASSERT_FALSE(_hcd_data.xfer[0].allocated);
  TEST_ASSERT_EQUAL(PARK_NONE, _hcd_data.xfer[0].park);
}

// The SETUP stage of a split control transfer on EP0 of the device at address 1, channel 0 enabled
static void setup_split_endpoint(bool complete_split) {
  bulk_nak_endpoint(false, true);
  hcd_endpoint_t* ep        = &_hcd_data.edpt[0];
  ep->hcchar_bm.ep_type     = HCCHAR_EPTYPE_CONTROL;
  ep->hcchar_bm.ep_num      = 0;
  ep->buflen                = 8;
  test_regs.channel[0].hcchar = ep->hcchar | HCCHAR_CHENA;
  dwc2_channel_tsize_t ts   = {.value = 0};
  ts.pid                    = HCTSIZ_PID_SETUP;
  ts.packet_count           = 1;
  ts.xfer_size              = complete_split ? 0 : 8;
  test_regs.channel[0].hctsiz = ts.value;
  if (complete_split) {
    test_regs.channel[0].hcsplt |= HCSPLT_COMPLSPLT;
  }
}

// One XactErr of the slave OUT channel 0 and the halt its disable brings; return whether the transfer ended
static bool slave_out_xact_err(void) {
  TEST_ASSERT_FALSE(handle_channel_out_slave(&test_regs, 0, HCINT_XACT_ERR));
  test_regs.channel[0].hcchar &= ~(HCCHAR_CHENA | HCCHAR_CHDIS);
  return handle_channel_out_slave(&test_regs, 0, HCINT_HALTED);
}

void test_slave_setup_start_split_xact_error_is_the_tt_nak(void) {
  // the core reports a TT's NAK of a SETUP start-split as XactErr alone: retried as a NAK, HCD_SPLIT_SETUP_BUSY_MAX
  // times per transfer, then counted. XactErr with NAK on a SETUP start-split, an XactErr of its complete-split, or of
  // a bulk start-split, counts at once.
  for (unsigned variant = 0; variant < 4; variant++) {
    setUp();
    dwc2_int_set_Ignore();
    _tuh_cfg.max_nak = 0; // every retry re-arms at once, none parked
    if (variant == 0 || variant == 3) {
      setup_split_endpoint(false);
    } else if (variant == 1) {
      setup_split_endpoint(true);
    } else {
      bulk_nak_endpoint(false, true);
    }
    const unsigned busy = (variant == 0) ? HCD_SPLIT_SETUP_BUSY_MAX : 0u;
    for (unsigned n = 0; n < busy + HCD_XFER_ERROR_MAX; n++) {
      if (variant == 1) {
        test_regs.channel[0].hcsplt |= HCSPLT_COMPLSPLT; // each retry here stays a complete-split
      }
      bool done;
      if (variant == 3) {
        TEST_ASSERT_FALSE(handle_channel_out_slave(&test_regs, 0, HCINT_XACT_ERR | HCINT_NAK));
        test_regs.channel[0].hcchar &= ~(HCCHAR_CHENA | HCCHAR_CHDIS);
        done = handle_channel_out_slave(&test_regs, 0, HCINT_HALTED);
      } else {
        done = slave_out_xact_err();
      }
      if (n < busy) {
        TEST_ASSERT_FALSE(done);
        TEST_ASSERT_EQUAL(0, _hcd_data.xfer[0].err_count);
        TEST_ASSERT_EQUAL(PARK_NONE, _hcd_data.xfer[0].park);
        TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar); // the start-split retried at once
      } else {
        TEST_ASSERT_EQUAL(n + 1u - busy, _hcd_data.xfer[0].err_count);
        TEST_ASSERT_EQUAL(n + 1u - busy == HCD_XFER_ERROR_MAX, done);
      }
    }
  }
}

void test_sof_teardown_of_an_enabled_parked_channel_never_waits_for_the_queue(void) {
  // slave mode: a closing channel parked while still enabled is disabled from SOF, but only with a queue entry free
  dwc2_int_set_Ignore();
  bulk_nak_endpoint(true, true);
  test_regs.hfnum = (3500u << 16) | (8u * 20u + 1u);
  channel_park(&test_regs, &test_regs.channel[0], PARK_IN_TOKEN);
  test_regs.channel[0].hcchar |= HCCHAR_CHENA;
  _hcd_data.edpt[0].closing = 1;

  dwc2_hnptxsts_t nptx = {.value = test_regs.hnptxsts};
  nptx.req_queue_available = 0;
  test_regs.hnptxsts = nptx.value;
  TEST_ASSERT_TRUE(handle_sof_irq(0, true));
  TEST_ASSERT_BITS_LOW(HCCHAR_CHDIS, test_regs.channel[0].hcchar);
  TEST_ASSERT_EQUAL(PARK_IN_TOKEN, _hcd_data.xfer[0].park);

  nptx.req_queue_available = 8;
  test_regs.hnptxsts = nptx.value;
  (void) handle_sof_irq(0, true);
  TEST_ASSERT_BITS_HIGH(HCCHAR_CHDIS | HCCHAR_CHENA, test_regs.channel[0].hcchar);
  TEST_ASSERT_TRUE(_hcd_data.xfer[0].closing);
  TEST_ASSERT_EQUAL(PARK_NONE, _hcd_data.xfer[0].park); // channel_disable() drops the park, its halt finishes it
}

void test_sof_never_waits_for_the_periodic_queue_to_start_a_slave_in(void) {
  // a due interrupt IN, split and native: SOF leaves it pending on a full queue, which the RxFIFO service after it in
  // the ISR may be needed to drain
  for (unsigned split = 0; split < 2; split++) {
    setUp();
    dwc2_int_set_Ignore();
    endpoint(0, true, 64, false);
    _hcd_data.edpt[0].hcsplt_bm.split_en = split ? 1u : 0u;
    _hcd_data.edpt[0].hcchar_bm.enable = 1;
    _hcd_data.edpt[0].xfer_pending = 1;
    _hcd_data.xfer[0].allocated = false; // its transfer has no channel yet

    set_ptx_queue(0);
    TEST_ASSERT_TRUE(handle_sof_irq(0, true));
    TEST_ASSERT_TRUE(_hcd_data.edpt[0].xfer_pending);
    TEST_ASSERT_FALSE(_hcd_data.xfer[0].allocated);
    TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);

    set_ptx_queue(1);
    (void) handle_sof_irq(0, true);
    TEST_ASSERT_FALSE(_hcd_data.edpt[0].xfer_pending);
    TEST_ASSERT_TRUE(_hcd_data.xfer[0].allocated);
    TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
  }
}

static unsigned fifo_reads;
static uint8_t* fifo_read_dest;
static uint16_t fifo_read_len;

static void fifo_read_stub(const volatile void* fifo, uint8_t* dest, uint16_t len,
                           const tu_hwfifo_access_t* access_mode, int cmock_num_calls) {
  (void) fifo;
  (void) access_mode;
  (void) cmock_num_calls;
  fifo_reads++;
  fifo_read_dest = dest;
  fifo_read_len = len;
}

void test_isr_drains_the_rxfifo_after_an_sof_that_leaves_a_slave_in_pending(void) {
  for (unsigned split = 0; split < 2; split++) {
    setUp();
    dwc2_int_set_Ignore();
    fifo_reads = 0;
    fifo_read_dest = NULL;
    fifo_read_len = 0;
    tu_hwfifo_read_Stub(fifo_read_stub);
    endpoint(0, true, 64, false);
    _hcd_data.edpt[0].hcsplt_bm.split_en = split ? 1u : 0u;
    _hcd_data.edpt[0].hcchar_bm.enable = 1;
    _hcd_data.edpt[0].xfer_pending = 1;
    _hcd_data.xfer[0].allocated = false;
    set_ptx_queue(0);

    // channel 1 has an IN packet waiting in the RxFIFO
    endpoint(1, true, 64, false);
    dwc2_grxstsp_t rx = {.value = 0};
    rx.ep_ch_num = 1;
    rx.byte_count = 32;
    rx.packet_status = GRXSTS_PKTSTS_RX_DATA;
    test_regs.grxstsp = rx.value;

    // the RAM gintsts keeps RXFLVL until handle_sof_irq() writes its SOF clear
    test_regs.gintmsk = GINTSTS_SOF | GINTSTS_RXFLVL;
    test_regs.gintsts = GINTSTS_SOF | GINTSTS_RXFLVL;
    hcd_int_handler(0, true);

    TEST_ASSERT_EQUAL(1, fifo_reads);
    TEST_ASSERT_EQUAL_PTR(payload, fifo_read_dest); // channel 1's buffer, nothing transferred yet
    TEST_ASSERT_EQUAL(32, fifo_read_len);
    TEST_ASSERT_EQUAL(32, _hcd_data.xfer[1].xferred_bytes);
    TEST_ASSERT_EQUAL(32, _hcd_data.xfer[1].fifo_bytes);
    TEST_ASSERT_BITS_HIGH(GINTSTS_RXFLVL, test_regs.gintmsk);
    TEST_ASSERT_TRUE(_hcd_data.edpt[0].xfer_pending);
    TEST_ASSERT_FALSE(_hcd_data.xfer[0].allocated);
    TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);
  }
}

// Hand the start-split ACK or a complete-split NYET of the bulk split channel 0 to its handler, the channel halted by
// the core (or, slave IN NYET, by the disable that follows it)
static void bulk_split_event(bool dma, bool in, uint32_t hcint) {
  test_regs.channel[0].hcchar &= ~(HCCHAR_CHENA | HCCHAR_CHDIS);
  if (dma) {
    TEST_ASSERT_FALSE(in ? handle_channel_in_dma(&test_regs, 0, HCINT_HALTED | hcint) :
                           handle_channel_out_dma(&test_regs, 0, HCINT_HALTED | hcint));
  } else if (in) {
    TEST_ASSERT_FALSE(handle_channel_in_slave(&test_regs, 0, hcint));
    if (hcint & HCINT_NYET) {
      test_regs.channel[0].hcchar &= ~(HCCHAR_CHENA | HCCHAR_CHDIS);
      TEST_ASSERT_FALSE(handle_channel_in_slave(&test_regs, 0, HCINT_HALTED));
    }
  } else {
    TEST_ASSERT_FALSE(handle_channel_out_slave(&test_regs, 0, hcint));
  }
}

void test_bulk_complete_split_waits_for_the_next_microframe(void) {
  // slave IN, slave OUT, DMA IN, DMA OUT
  for (unsigned i = 0; i < 4; i++) {
    setUp();
    dwc2_int_set_Ignore();
    const bool dma = i >= 2;
    const bool in  = (i & 1u) == 0;
    if (dma) {
      dwc2_ghwcfg2_t hw = {.value = test_regs.ghwcfg2};
      hw.arch = GHWCFG2_ARCH_INTERNAL_DMA;
      test_regs.ghwcfg2 = hw.value;
    }
    bulk_nak_endpoint(in, true);
    const channel_park_t kind = in ? PARK_CSPLIT_IN : PARK_CSPLIT_OUT;

    // the start-split ACK, and later a NYET, park the complete-split for the next microframe
    for (unsigned step = 0; step < 2; step++) {
      const uint16_t uframe = (uint16_t) (8u * 20u + 3u + 2u * step);
      test_regs.hfnum = (3500u << 16) | uframe;
      bulk_split_event(dma, in, step == 0 ? HCINT_ACK : HCINT_NYET);
      TEST_ASSERT_BITS_HIGH(HCSPLT_COMPLSPLT, test_regs.channel[0].hcsplt);
      TEST_ASSERT_EQUAL(kind, _hcd_data.xfer[0].park);
      TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);
      TEST_ASSERT_BITS_HIGH(GINTMSK_SOFM, test_regs.gintmsk);

      // an SOF of the same microframe, still pending, leaves it parked
      TEST_ASSERT_TRUE(handle_sof_irq(0, true));
      TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);

      // the next microframe sends it, once the request queue has room: SOF never waits for it
      test_regs.hfnum = (3500u << 16) | (uframe + 1u);
      dwc2_hnptxsts_t nptx = {.value = test_regs.hnptxsts};
      nptx.req_queue_available = 0;
      test_regs.hnptxsts = nptx.value;
      TEST_ASSERT_TRUE(handle_sof_irq(0, true));
      TEST_ASSERT_EQUAL(kind, _hcd_data.xfer[0].park);
      TEST_ASSERT_BITS_LOW(HCCHAR_CHENA, test_regs.channel[0].hcchar);
      nptx.req_queue_available = 8;
      test_regs.hnptxsts = nptx.value;
      (void) handle_sof_irq(0, true);
      TEST_ASSERT_EQUAL(PARK_NONE, _hcd_data.xfer[0].park);
      TEST_ASSERT_BITS_HIGH(HCCHAR_CHENA, test_regs.channel[0].hcchar);
      TEST_ASSERT_BITS_HIGH(HCSPLT_COMPLSPLT, test_regs.channel[0].hcsplt);
    }
  }
}

void test_slave_out_start_restores_only_an_interrupt_mask_it_set(void) {
  // The FIFO write of a slave OUT start runs with the interrupt masked. Found masked already, the mask is another
  // region's, on a multi-core MCU possibly one on the other core that unmasks first: restoring the 0 read here would
  // leave the interrupt masked for good.
  for (unsigned found_masked = 0; found_masked < 2; found_masked++) {
    setUp();
    dwc2_int_set_Ignore();
    bulk_nak_endpoint(false, false);
    _hcd_data.xfer[0].allocated = false;
    test_regs.channel[0].hcchar &= ~HCCHAR_CHENA;
    dwc2_hnptxsts_t nptx = {.value = test_regs.hnptxsts};
    nptx.fifo_available = 64;
    test_regs.hnptxsts = nptx.value;
    test_regs.gahbcfg = found_masked ? 0 : GAHBCFG_GINT;
    fifo_write_unmasks = found_masked;

    TEST_ASSERT_TRUE(hcd_edpt_xfer(0, 1, 0x01, payload, 64));
    TEST_ASSERT_EQUAL(1, fifo_writes);
    TEST_ASSERT_BITS_LOW(GAHBCFG_GINT, gahbcfg_at_fifo_write);
    TEST_ASSERT_BITS_HIGH(GAHBCFG_GINT, test_regs.gahbcfg);
  }
}

void test_parked_bulk_channel_closes_only_the_microframe_it_parked_in(void) {
  // an isochronous OUT MID due in the microframe the bulk channel parked in by max_nak, in a later one, and in a later
  // one with the channel still enabled, as a slave IN parked between packets is
  for (unsigned i = 0; i < 3; i++) {
    setUp();
    dwc2_int_set_Ignore();
    bulk_nak_endpoint(true, true);
    _tuh_cfg.max_nak = 1;
    const uint16_t parked_in = 8u * 20u + 1u;
    test_regs.hfnum = (3500u << 16) | parked_in;
    TEST_ASSERT_FALSE(bulk_nak(false, true));
    TEST_ASSERT_EQUAL(PARK_IN_TOKEN, _hcd_data.xfer[0].park);
    if (i == 2) {
      test_regs.channel[0].hcchar |= HCCHAR_CHENA;
    }

    const uint16_t uframe = (uint16_t) (parked_in + (i == 0 ? 0u : 1u));
    endpoint(1, false, 400, true);
    _hcd_data.xfer[1].split_uframe = (uint16_t) (uframe - 1u);
    _hcd_data.xfer[1].iso_open = 1;
    test_regs.hfnum = (3500u << 16) | uframe;
    TEST_ASSERT_EQUAL(i == 1, channel_iso_out_next(&test_regs, 1));
  }
}
