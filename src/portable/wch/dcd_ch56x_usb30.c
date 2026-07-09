/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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
 *
 * This file is part of the TinyUSB stack.
 */

// CH565/CH569 USB3.0 SuperSpeed (Gen1, 5 Gbps) device controller (USBSS @0x40008000).
//
// WCH ships USB3 support for this chip only as an undocumented binary library; this driver is a
// fresh implementation written against the register model reverse engineered by the hydrausb3
// project (wch-ch56x-lib usb30.c, Apache-2.0) with the TeenyUSB CH56x port (MIT) as a secondary
// reference, and the LINK-layer semantics confirmed by the CH32H417 reference manual (same IP).
//
// Notes:
// - The LTSSM is driven by software from the LINK interrupt: link training, LMP port
//   capability/configuration exchange and warm/hot reset are all serviced here.
// - The "bus reset" event for the stack is the link reaching U0 (LINK_IF_RDY after TXEQ).
// - Bulk endpoints burst up to CFG_TUD_WCH_USB30_MAX_BURST packets per arm (default 4,
//   hardware-validated with data-integrity tests; the TeenyUSB-reported burst corruption
//   quirk did not reproduce with per-burst DMA re-arm). Burst 8 fails to configure - the
//   link layer has 4 header-packet buffers (NUM_HP_BUF).
// - Isochronous endpoints are not supported (the open register model has no proven ISO path).
// - USB DMA reaches only RAMX (16-byte aligned); transfers from other memory are bounced
//   through per-endpoint slots allocated from a small RAMX pool.
// - Silicon erratum: EP0 OUT data stages whose wLength % 4 == 1 are intermittently dropped
//   (host sees -EPROTO; rate varies ~0.1-25% per transfer, worst near 245-257 and 509 bytes;
//   other lengths and bulk OUT are unaffected). Not fixable in software: WCH's own binary USB3
//   stack (SimulateCDC demo) fails identically on the same board, independent of host LPM
//   (failures persist with U1/U2 disabled) and of sysclk (80 vs 120 MHz). Standard classes are
//   unaffected in practice (control-OUT payloads are fixed-size structs); usbtest's ctrl_out
//   cases hit it, so the usbtest example advertises a skip quirk in bcdDevice.

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USB30) && defined(CFG_TUD_WCH_USBIP_USB30) && \
    (CFG_TUD_WCH_USBIP_USB30 == 1)

#include "device/dcd.h"
#include "dcd_ch56x.h"
#include "ch56x_usb30_reg.h"

#define EP_MAX        8
#define EP0_MPS       512

// NUMP per arm; default set in tusb_mcu.h (4, hardware-validated: 50x MSC write/read-back and
// 256KB CDC echo integrity clean, ~2x throughput over single packets; 8 fails to configure,
// matching the link layer's 4 header-packet buffers)
#ifndef CFG_TUD_WCH_USB30_MAX_BURST
  #define CFG_TUD_WCH_USB30_MAX_BURST 4
#endif

// RAMX pool for per-endpoint bounce slots (one max packet per opened EP direction). Bounced
// transfers are limited to single-packet bursts; zero-copy transfers use the full burst.
#define BOUNCE_SLOT_SIZE 1024
#ifndef CFG_TUD_WCH_USB30_RAMX_POOL_SIZE
  #define CFG_TUD_WCH_USB30_RAMX_POOL_SIZE (6 * BOUNCE_SLOT_SIZE)
#endif

// The hardware EP0 max packet size is fixed at 512 (SuperSpeed). Applications should set
// CFG_TUD_ENDPOINT0_SIZE to 512 (and provide SuperSpeed descriptors); ones that keep 64 still
// compile but will not enumerate on a SuperSpeed host.

typedef struct {
  uint8_t *buffer;    // transfer buffer (may be outside RAMX)
  uint8_t *slot;      // RAMX bounce slot for this endpoint direction (NULL until allocated)
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t armed_len; // bytes armed in the current burst
  uint16_t mps;
  bool     bounce;    // current transfer goes through the bounce slot
  volatile bool active;
  volatile bool stalled; // endpoint halted: keep answering STALL until CLEAR_FEATURE
} xfer_ctl_t;

static xfer_ctl_t xfer_status[EP_MAX][2];

/* EP0 buffer, shared by SETUP/IN/OUT (UEP0_DMA) */
CFG_TUD_WCH_DMA_SECTION TU_ATTR_ALIGNED(16) static uint8_t _ep0_buf[EP0_MPS];
CFG_TUD_WCH_DMA_SECTION TU_ATTR_ALIGNED(16) static uint8_t _ramx_pool[CFG_TUD_WCH_USB30_RAMX_POOL_SIZE];
static uint16_t _pool_brk;

static uint8_t _pending_addr;             // 0x80 | address while SET_ADDRESS status is pending
static uint8_t _ep0_seq;                  // EP0 IN packet sequence within a data stage
static tusb_dir_t _ep0_status_dir;        // direction usbd queued for the status stage
static volatile bool _ep0_status_pending;  // status stage armed, completion not yet delivered
static volatile uint16_t _ep0_early_len;   // OUT data stage received before usbd armed it (in _ep0_buf)
static volatile bool _lmp_pending;        // send LMP PORT_CAPABILITY at next link-ready

#if CFG_TUD_WCH_USB30_FALLBACK
// Runtime USB2 high-speed fallback (WCH reference pattern): a ~0.5 s timer (TMR0) counts down
// while SuperSpeed link training runs. First expiry shuts USB3 down (host sees disconnect),
// second expiry brings the USB2 controller up on the same rhport. SuperSpeed success kills the
// timer. One way: returning to SuperSpeed after fallback requires a new dcd_init (or power cycle).
enum { FB_USB3_TRAINING = 0, FB_USB3_OFF, FB_USB3_UP, FB_USB2_ACTIVE };
static volatile uint8_t _fb_state;
static uint8_t _fb_train_ticks;         // training timer expiries; a fresh detect cycle every 2nd tick
static volatile bool _fb_saw_terms;     // far-end RX terminations seen: an SS-capable partner exists

static void fallback_timer_stop(void) {
  R8_TMR0_INTER_EN = 0;
  PFIC_DisableIRQ(TMR0_IRQn);
  R8_TMR0_CTRL_MOD = RB_TMR_ALL_CLEAR;
}

static void fallback_timer_start(void) {
  _fb_train_ticks = 0;
  _fb_saw_terms = false;
  R8_TMR0_CTRL_MOD = RB_TMR_ALL_CLEAR;
  // TMR0 counts 26 bits: CNT_END must stay below 2^26 (67108864). 0.55 s per expiry at
  // 120 MHz; training gets several expiries (with re-attempts) before USB2 comes up
  R32_TMR0_CNT_END = 66000000;
  R8_TMR0_INT_FLAG = RB_TMR_IF_CYC_END;
  R8_TMR0_INTER_EN = RB_TMR_IE_CYC_END;
  R8_TMR0_CTRL_MOD = RB_TMR_COUNT_EN;
}
#endif

//--------------------------------------------------------------------+
// Link layer (software LTSSM assist)
//--------------------------------------------------------------------+

static void link_set_power_mode(uint32_t pm) {
  // bounded: BUSY can stick when the link is dead (e.g. during fallback teardown)
  uint32_t timeout = 100000;
  while ((USBSS->LINK_STATUS & USBSS_LINK_STATUS_BUSY) && --timeout) {}
  USBSS->LINK_CTRL = (USBSS->LINK_CTRL & ~USBSS_LINK_CTRL_PM_MASK) | pm;
}

// Bounded busy wait, only used in rare link (re)configuration paths
static void link_delay_us(uint32_t us) {
  // 120 MHz core, roughly 4 cycles per loop iteration
  volatile uint32_t n = us * (120 / 4);
  while (n--) {}
}

static bool usb30_hw_init(void) {
  USBSS->LINK_CFG = USBSS_LINK_CFG_INIT;
  USBSS->LINK_CTRL = 0x12; // power mode U2, per reference init
  // bounded, best effort: BUSY can stick after a teardown; carry on and let the link
  // interrupts (or the USB2 fallback timer) recover
  uint32_t timeout = 0x4c4b41;
  while ((USBSS->LINK_STATUS & USBSS_LINK_STATUS_BUSY) && --timeout) {}
  for (uint8_t ep = 0; ep < EP_MAX; ep++) {
    USBSS_TX_CTRL(ep) = 0;
    USBSS_RX_CTRL(ep) = 0;
  }
  USBSS->USB_STATUS = USBSS_USB_STATUS_CLEAR_INIT;
  USBSS->USB_CONTROL = USBSS_USB_CTRL_INIT;
  USBSS->UEP_CFG = USBSS_EP_R_EN(0) | USBSS_EP_T_EN(0);
  USBSS->UEP0_DMA = (uint32_t)(uintptr_t)_ep0_buf;
  USBSS->LINK_CFG |= USBSS_LINK_CFG_RX_TERM_EN;
  USBSS->LINK_INT_CTRL = USBSS_LINK_INT_EN_INIT;
  USBSS->LINK_CTRL = 2; // U2, wait for terminations (TERM_PRESENT irq starts polling)
  return true;
}

static void usb30_hw_deinit(void) {
  link_set_power_mode(3);
  USBSS->LINK_CFG = USBSS_LINK_CFG_PIPE_RESET | USBSS_LINK_CFG_LFPS_RX_PD;
  USBSS->LINK_CTRL = USBSS_LINK_CTRL_GO_DISABLED | 3;
  USBSS->LINK_INT_CTRL = 0;
  USBSS->USB_CONTROL = USBSS_USB_CTRL_FORCE_RST | USBSS_USB_CTRL_ALL_CLR;
}

static void usb30_bus_reset(void) {
  usb30_hw_deinit();
  link_delay_us(30000);
  usb30_hw_init();
}

// Reset endpoint/transfer bookkeeping when the link (re)trains
static void ep_state_reset(void) {
  for (uint8_t ep = 0; ep < EP_MAX; ep++) {
    xfer_status[ep][0].active = false;
    xfer_status[ep][1].active = false;
    xfer_status[ep][0].stalled = false;
    xfer_status[ep][1].stalled = false;
  }
  _pending_addr = 0;
  _ep0_seq = 0;
}

//--------------------------------------------------------------------+
// Endpoint arming
//--------------------------------------------------------------------+

static void xfer_in_arm(uint8_t ep) {
  xfer_ctl_t *xfer = &xfer_status[ep][TUSB_DIR_IN];
  const uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t burst_max = (uint16_t)(xfer->mps * CFG_TUD_WCH_USB30_MAX_BURST);
  if (xfer->bounce) {
    burst_max = TU_MIN(burst_max, (uint16_t)BOUNCE_SLOT_SIZE);
  }
  const uint16_t chunk = TU_MIN(remaining, burst_max);
  const uint8_t nump = (chunk == 0) ? 1 : (uint8_t)TU_DIV_CEIL(chunk, xfer->mps);
  // length field holds the length of the last packet of the burst
  const uint16_t last_pkt = (chunk == 0) ? 0 : (uint16_t)(chunk - (nump - 1) * xfer->mps);

  const uint8_t *src = xfer->buffer + xfer->queued_len;
  if (xfer->bounce && chunk) {
    memcpy(xfer->slot, src, chunk);
    src = xfer->slot;
  }
  *usbss_tx_dma(ep) = (uint32_t)(uintptr_t)src; // DMA address auto-increments: rewrite every burst
  xfer->armed_len = chunk;
  USBSS_TX_CTRL(ep) = (USBSS_TX_CTRL(ep) & USBSS_EP_SEQ_MASK) | ((uint32_t)nump << USBSS_EP_NUMP_SHIFT) |
                      USBSS_EP_RES_ACK | USBSS_EP_LPF | last_pkt;
  usbss_send_erdy(ep, true, nump);
}

static void xfer_out_arm(uint8_t ep) {
  xfer_ctl_t *xfer = &xfer_status[ep][TUSB_DIR_OUT];
  const uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint8_t nump = (uint8_t)TU_MAX(1u, TU_MIN((uint32_t)TU_DIV_CEIL(remaining, xfer->mps),
                                            (uint32_t)CFG_TUD_WCH_USB30_MAX_BURST));
  // The receiver has no byte limit: an armed packet may write up to mps bytes. Bounce whenever
  // the remaining user buffer cannot hold the armed burst; bounced bursts fit one slot.
  xfer->bounce = ((uint32_t)remaining < (uint32_t)nump * xfer->mps) ||
                 !ch56x_is_dma_capable(xfer->buffer + xfer->queued_len);
  if (xfer->bounce) {
    nump = (uint8_t)TU_MAX(1u, TU_MIN((uint32_t)nump, (uint32_t)(BOUNCE_SLOT_SIZE / xfer->mps)));
  }
  uint8_t *dst = xfer->bounce ? xfer->slot : (xfer->buffer + xfer->queued_len);
  *usbss_rx_dma(ep) = (uint32_t)(uintptr_t)dst;
  xfer->armed_len = (uint16_t)(nump * xfer->mps);
  USBSS_RX_CTRL(ep) = (USBSS_RX_CTRL(ep) & USBSS_EP_SEQ_MASK) | ((uint32_t)nump << USBSS_EP_NUMP_SHIFT) |
                      USBSS_EP_RES_ACK;
  usbss_send_erdy(ep, false, nump);
}

//--------------------------------------------------------------------+
// EP0 control endpoint
//--------------------------------------------------------------------+

static void ep0_arm_status(void) {
  // SuperSpeed status stage is a STATUS transaction packet from the host, reported as an EP0
  // RX event with the status-stage flag. Both directions must be armed: the RX engine
  // acknowledges the STATUS TP and TX supplies the zero-length response (verified on hardware:
  // RX unarmed makes SET_ADDRESS time out)
  _ep0_status_pending = true;
  // Both sides must be armed: the RX engine acknowledges the host's STATUS TP and the TX
  // side supplies the zero-length response (verified on hardware: RX unarmed -> the STATUS
  // TP is ignored and SET_ADDRESS times out)
  USBSS->UEP0_RX_CTRL = USBSS_UEP0_RX_ARM;
  USBSS->UEP0_TX_CTRL = USBSS_UEP0_TX_ARM;
  usbss_send_erdy(0, false, 1);
}

// Deliver the status-stage completion: with direction-aware arming it surfaces as a TX event
// for an IN status (zero-length IN answered) and as an RX status-flag event for an OUT status
static void ep0_status_complete(uint8_t rhport) {
  _ep0_status_pending = false;
  if (_pending_addr) {
    USBSS->USB_CONTROL = (USBSS->USB_CONTROL & 0x00FFFFFFu) |
                         ((uint32_t)(_pending_addr & 0x7Fu) << USBSS_DEV_ADDR_SHIFT);
    _pending_addr = 0;
  }
  USBSS->UEP0_RX_CTRL = USBSS_UEP0_RX_ARM;
  USBSS->UEP0_TX_CTRL = 0;
  dcd_event_xfer_complete(rhport, tu_edpt_addr(0, _ep0_status_dir), 0, XFER_RESULT_SUCCESS, true);
}

static void ep0_in_arm(void) {
  xfer_ctl_t *xfer = &xfer_status[0][TUSB_DIR_IN];
  const uint16_t chunk = TU_MIN((uint16_t)(xfer->total_len - xfer->queued_len), (uint16_t)EP0_MPS);
  memcpy(_ep0_buf, xfer->buffer + xfer->queued_len, chunk);
  xfer->armed_len = chunk;
  USBSS->UEP0_TX_CTRL = ((uint32_t)_ep0_seq << 21) | USBSS_UEP0_TX_ARM | chunk;
  usbss_send_erdy(0, true, 1);
}

static void handle_ep0_in(uint8_t rhport) {
  xfer_ctl_t *xfer = &xfer_status[0][TUSB_DIR_IN];
  if (!xfer->active) {
    USBSS->UEP0_TX_CTRL = 0;
    return;
  }
  _ep0_seq = (_ep0_seq + 1) & 0x1F;
  xfer->queued_len += xfer->armed_len;

  if (xfer->queued_len < xfer->total_len) {
    ep0_in_arm();
  } else {
    xfer->active = false;
    USBSS->UEP0_TX_CTRL = 0;
    dcd_event_xfer_complete(rhport, 0x80, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }
}

static void handle_ep0_rx(uint8_t rhport) {
  const uint32_t rx_ctrl = USBSS->UEP0_RX_CTRL;

  // Handle a latched status-stage completion BEFORE a simultaneously latched SETUP: with
  // back-to-back control transfers both flags can be set in one ISR pass, and taking the
  // SETUP first would drop the previous transfer's completion inside usbd
  if (rx_ctrl & USBSS_EP_RX_STATUS_STAGE) {
    if (_ep0_status_pending) {
      ep0_status_complete(rhport);
    } else {
      USBSS->UEP0_RX_CTRL = 0;
    }
    if (0 == (rx_ctrl & USBSS_EP_RX_SETUP)) {
      return;
    }
  }

  if (rx_ctrl & USBSS_EP_RX_SETUP) {
    // SETUP packet in _ep0_buf
    _ep0_early_len = 0;
    USBSS->UEP0_RX_CTRL = USBSS_UEP0_RX_ARM;
    USBSS->UEP0_TX_CTRL = 0;
    _ep0_seq = 0;
    // Host-to-device data stage: arm and signal ERDY here in the ISR, like the reference flow.
    // Sending the ERDY only when usbd arms the stage from task context (~100 us later) made
    // the host's data DP vanish intermittently (usbtest ctrl_out); the packet lands in
    // _ep0_buf and is held (_ep0_early_len) until usbd asks for it
    if (((_ep0_buf[0] & 0x80u) == 0) && (_ep0_buf[6] || _ep0_buf[7])) {
      usbss_send_erdy(0, false, 1);
    }
    dcd_event_setup_received(rhport, _ep0_buf, true);
    return;
  }

  // OUT data stage
  xfer_ctl_t *xfer = &xfer_status[0][TUSB_DIR_OUT];
  if (!xfer->active) {
    // Data stage arrived before usbd armed it (see the pre-arm in the SETUP branch): hold it
    const uint16_t early = (uint16_t)(rx_ctrl & USBSS_EP_RX_LEN_MASK);
    if (early) {
      _ep0_early_len = early;
    }
    USBSS->UEP0_RX_CTRL = USBSS_UEP0_RX_ARM;
    return;
  }
  uint16_t len = (uint16_t)(rx_ctrl & USBSS_EP_RX_LEN_MASK);
  len = TU_MIN(len, (uint16_t)(xfer->total_len - xfer->queued_len));
  memcpy(xfer->buffer + xfer->queued_len, _ep0_buf, len);
  xfer->queued_len += len;

  if ((xfer->queued_len == xfer->total_len) || (len < EP0_MPS)) {
    xfer->active = false;
    USBSS->UEP0_RX_CTRL = USBSS_UEP0_RX_NRDY;
    dcd_event_xfer_complete(rhport, 0x00, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    USBSS->UEP0_RX_CTRL |= USBSS_UEP0_RX_ARM; // OR: preserve the live packet-sequence bits
    usbss_send_erdy(0, false, 1);
  }
}

//--------------------------------------------------------------------+
// Data endpoints
//--------------------------------------------------------------------+

static void handle_ep_in(uint8_t rhport, uint8_t ep) {
  xfer_ctl_t *xfer = &xfer_status[ep][TUSB_DIR_IN];
  const uint32_t ctrl = USBSS_TX_CTRL(ep);
  USBSS_TX_CTRL(ep) &= USBSS_EP_SEQ_MASK; // clear event, NRDY, keep packet sequence
  if (xfer->stalled) {
    // halted while a burst was in flight: keep answering STALL, do not advance or re-arm
    USBSS_TX_CTRL(ep) = (USBSS_TX_CTRL(ep) & USBSS_EP_SEQ_MASK) |
                        (1u << USBSS_EP_NUMP_SHIFT) | USBSS_EP_RES_STALL;
    return;
  }
  if (!xfer->active) {
    return;
  }

  // The host may drain only part of the burst; account for what was actually sent
  const uint8_t nump_left = (uint8_t)((ctrl & USBSS_EP_NUMP_MASK) >> USBSS_EP_NUMP_SHIFT);
  uint16_t sent = xfer->armed_len;
  if (nump_left) {
    const uint8_t nump_armed = (uint8_t)TU_DIV_CEIL(xfer->armed_len, xfer->mps);
    sent = (uint16_t)((nump_armed - nump_left) * xfer->mps);
  }
  xfer->queued_len += TU_MIN(sent, (uint16_t)(xfer->total_len - xfer->queued_len));

  if (xfer->queued_len >= xfer->total_len) {
    xfer->active = false;
    dcd_event_xfer_complete(rhport, ep | TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    xfer_in_arm(ep);
  }
}

static void handle_ep_out(uint8_t rhport, uint8_t ep) {
  xfer_ctl_t *xfer = &xfer_status[ep][TUSB_DIR_OUT];
  const uint32_t ctrl = USBSS_RX_CTRL(ep);
  USBSS_RX_CTRL(ep) &= USBSS_EP_SEQ_MASK; // clear event, NRDY while processing
  if (xfer->stalled) {
    // halted while a receive was armed: keep answering STALL, do not advance or re-arm
    USBSS_RX_CTRL(ep) = (USBSS_RX_CTRL(ep) & USBSS_EP_SEQ_MASK) |
                        (1u << USBSS_EP_NUMP_SHIFT) | USBSS_EP_RES_STALL;
    return;
  }
  if (!xfer->active) {
    return;
  }

  const uint16_t last_len = (uint16_t)(ctrl & USBSS_EP_RX_LEN_MASK);
  const uint8_t nump_left = (uint8_t)((ctrl & USBSS_EP_NUMP_MASK) >> USBSS_EP_NUMP_SHIFT);
  const uint8_t nump_armed = (uint8_t)TU_DIV_CEIL(xfer->armed_len, xfer->mps);
  const uint8_t got = (uint8_t)(nump_armed - nump_left);
  uint16_t rx_bytes = (got == 0) ? 0 : (uint16_t)((got - 1) * xfer->mps + last_len);
  rx_bytes = TU_MIN(rx_bytes, (uint16_t)(xfer->total_len - xfer->queued_len));

  if (xfer->bounce && rx_bytes) {
    memcpy(xfer->buffer + xfer->queued_len, xfer->slot, rx_bytes);
  }
  xfer->queued_len += rx_bytes;

  const bool short_packet = (last_len < xfer->mps);
  if (short_packet || (xfer->queued_len >= xfer->total_len)) {
    xfer->active = false;
    dcd_event_xfer_complete(rhport, ep, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    xfer_out_arm(ep);
  }
}

//--------------------------------------------------------------------+
// Interrupt handlers
//--------------------------------------------------------------------+

static void handle_link_irq(uint8_t rhport) {
  const uint32_t flag = USBSS->LINK_INT_FLAG;

  if (flag & USBSS_LINK_IF_UX_EXIT) {
    USBSS->LINK_CFG = USBSS_LINK_CFG_RX_EQ_EN | USBSS_LINK_CFG_TX_DEEMPH | USBSS_LINK_CFG_RX_TERM_EN;
    link_set_power_mode(0);
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_UX_EXIT;
    dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_RESUME};
    dcd_event_handler(&event, true);
  }
  if (flag & USBSS_LINK_IF_RDY) {
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_RDY;
    if (_lmp_pending) {
      // link trained to U0: exchange port capability, then the host enumerates
      USBSS->LMP_TX_DATA0 = USBSS_LMP_TX_PORT_CAP_DATA0;
      USBSS->LMP_TX_DATA1 = USBSS_LMP_TX_PORT_CAP_DATA1;
      USBSS->LMP_TX_DATA2 = 0;
      _lmp_pending = false;
#if CFG_TUD_WCH_USB30_FALLBACK
      _fb_state = FB_USB3_UP;
      fallback_timer_stop();
#endif
      ep_state_reset();
      dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
    }
  }
  if (flag & USBSS_LINK_IF_INACT) {
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_INACT;
    link_set_power_mode(2);
  }
  if (flag & USBSS_LINK_IF_DISABLE) {
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_DISABLE;
#if CFG_TUD_WCH_USB30_FALLBACK
    // SuperSpeed rejected by the link partner: switch to USB2 right away
    PFIC_DisableIRQ(USBSS_IRQn);
    PFIC_DisableIRQ(LINK_IRQn);
    usb30_hw_deinit();
    fallback_timer_stop();
    _fb_state = FB_USB2_ACTIVE;
    USBSS->LINK_INT_FLAG = 0xFFFFFFFFu;
    PFIC_ClearPendingIRQ(USBSS_IRQn);
    PFIC_ClearPendingIRQ(LINK_IRQn);
    PFIC_ClearPendingIRQ(TMR0_IRQn);
    ch56x_usb2_init(rhport);
    ch56x_usb2_int_enable();
#else
    // No SuperSpeed host (or link lost): retry SS training
    dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_UNPLUGGED};
    dcd_event_handler(&event, true);
    usb30_bus_reset();
#endif
  }
  if (flag & USBSS_LINK_IF_RX_DET) {
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_RX_DET;
    link_set_power_mode(2);
  }
  if (flag & USBSS_LINK_IF_TERM_PRESENT) {
#if CFG_TUD_WCH_USB30_FALLBACK
    _fb_saw_terms = true;
#endif
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_TERM_PRESENT;
    if (USBSS->LINK_STATUS & USBSS_LINK_STATUS_PRESENT) {
      link_set_power_mode(2);
      USBSS->LINK_CTRL |= USBSS_LINK_CTRL_POLLING_EN;
    } else {
      // partner disappeared
      USBSS->LINK_INT_CTRL = 0;
      link_delay_us(2000);
      usb30_bus_reset();
      dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_UNPLUGGED};
      dcd_event_handler(&event, true);
    }
  }
  if (flag & USBSS_LINK_IF_TXEQ) {
#if CFG_TUD_WCH_USB30_FALLBACK
    _fb_saw_terms = true;
#endif
    _lmp_pending = true;
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_TXEQ;
    link_set_power_mode(0);
  }
  if (flag & USBSS_LINK_IF_WARM_RESET) {
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_WARM_RESET;
    link_set_power_mode(2);
    usb30_bus_reset();
    USBSS->USB_CONTROL &= 0x00FFFFFFu; // address 0
    USBSS->LINK_CTRL |= USBSS_LINK_CTRL_TX_WARM_RST;
    uint32_t timeout = 1000000;
    while ((USBSS->LINK_STATUS & USBSS_LINK_STATUS_RX_WARM) && --timeout) {}
    USBSS->LINK_CTRL &= ~USBSS_LINK_CTRL_TX_WARM_RST;
    link_delay_us(2);
  }
  if (flag & USBSS_LINK_IF_HOT_RESET) {
    // hot reset: link stays up, reset protocol state and re-enumerate
    USBSS->USB_CONTROL |= USBSS_USB_CTRL_HOT_RST_ACK;
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_HOT_RESET;
    USBSS->UEP0_TX_CTRL = 0;
    for (uint8_t ep = 1; ep < EP_MAX; ep++) {
      USBSS_TX_CTRL(ep) = 0;
      USBSS_RX_CTRL(ep) = 0;
    }
    USBSS->USB_CONTROL &= 0x00FFFFFFu; // address 0
    USBSS->LINK_CTRL &= ~USBSS_LINK_CTRL_TX_HOT_RST;
    ep_state_reset();
    dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
  }
  if (flag & USBSS_LINK_IF_GO_U1) {
    link_set_power_mode(1);
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_GO_U1;
  }
  if (flag & USBSS_LINK_IF_GO_U2) {
    link_set_power_mode(2);
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_GO_U2;
  }
  if (flag & USBSS_LINK_IF_GO_U3) {
    link_set_power_mode(2);
    USBSS->LINK_INT_FLAG = USBSS_LINK_IF_GO_U3;
    dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_SUSPEND};
    dcd_event_handler(&event, true);
  }
}

static void handle_usb_irq(uint8_t rhport) {
  const uint32_t status = USBSS->USB_STATUS;

  if ((status & USBSS_USB_STATUS_EP_EVT) == 0) {
    if (status & USBSS_USB_STATUS_LMP_EVT) {
      // answer LMP port configuration in software (timing critical)
      if ((USBSS->LMP_RX_DATA0 & USBSS_LMP_SUBTYPE_MASK) == USBSS_LMP_PORT_CFG) {
        USBSS->LMP_TX_DATA0 = USBSS_LMP_PORT_CFG_RES;
        USBSS->LMP_TX_DATA1 = 0;
        USBSS->LMP_TX_DATA2 = 0;
      }
      USBSS->USB_STATUS = USBSS_USB_STATUS_LMP_EVT;
      return;
    }
    if (status & USBSS_USB_STATUS_ITP_EVT) {
      USBSS->USB_STATUS = USBSS_USB_STATUS_ITP_EVT;
    }
    return;
  }

  const uint8_t ep = (uint8_t)usbss_status_ep_num(status);
  const bool is_in = usbss_status_ep_is_in(status);

  if (ep == 0) {
    if (is_in) {
      handle_ep0_in(rhport);
    } else {
      handle_ep0_rx(rhport);
    }
  } else {
    if (is_in) {
      handle_ep_in(rhport, ep);
    } else {
      handle_ep_out(rhport, ep);
    }
  }
}

void dcd_int_handler(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    ch56x_usb2_int_handler(rhport);
    return;
  }
  // Fallback timer tick: SuperSpeed training has not succeeded yet
  if (R8_TMR0_INT_FLAG & RB_TMR_IF_CYC_END) {
    R8_TMR0_INT_FLAG = RB_TMR_IF_CYC_END;
    if (_fb_state == FB_USB3_TRAINING) {
      // Retry SuperSpeed training with a fresh detect cycle before giving up: some hubs
      // (e.g. Renesas uPD720201's) only complete training on a re-attempt. When far-end
      // terminations were seen the partner is SS-capable: retry longer before settling
      // for USB2; with no terminations (USB2-only host) fall back quickly
      _fb_train_ticks++;
      if (_fb_train_ticks < (_fb_saw_terms ? 8 : 4)) {
        if ((_fb_train_ticks & 1u) == 0) {
          usb30_hw_deinit();
          usb30_hw_init();
        }
        return;
      }
      // training window exhausted: shut USB3 down so the host sees a disconnect
      PFIC_DisableIRQ(USBSS_IRQn);
      PFIC_DisableIRQ(LINK_IRQn);
      usb30_hw_deinit();
      _fb_state = FB_USB3_OFF;
    } else if (_fb_state == FB_USB3_OFF) {
      // second expiry: switch to the USB2 high-speed controller on the same rhport
      _fb_state = FB_USB2_ACTIVE;
      fallback_timer_stop();
      USBSS->LINK_INT_FLAG = 0xFFFFFFFFu;
      PFIC_ClearPendingIRQ(USBSS_IRQn);
      PFIC_ClearPendingIRQ(LINK_IRQn);
      PFIC_ClearPendingIRQ(TMR0_IRQn);
      ch56x_usb2_init(rhport);
      ch56x_usb2_int_enable();
    }
    return;
  }
#endif

  // Both the USBSS and LINK vectors forward here; dispatch on the flag registers
  if (USBSS->LINK_INT_FLAG & USBSS->LINK_INT_CTRL) {
    handle_link_irq(rhport);
  }
  if (USBSS->USB_STATUS & (USBSS_USB_STATUS_EP_EVT | USBSS_USB_STATUS_LMP_EVT | USBSS_USB_STATUS_ITP_EVT)) {
    handle_usb_irq(rhport);
  }
}

//--------------------------------------------------------------------+
// Device API
//--------------------------------------------------------------------+

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;
  (void)rh_init;

  memset(xfer_status, 0, sizeof(xfer_status));
  _pool_brk = 0;
  _pending_addr = 0;
  _ep0_seq = 0;
  _lmp_pending = false;
  xfer_status[0][TUSB_DIR_OUT].mps = EP0_MPS;
  xfer_status[0][TUSB_DIR_IN].mps = EP0_MPS;

#if CFG_TUD_WCH_USB30_FALLBACK
  _fb_state = FB_USB3_TRAINING;
  ch56x_usb2_deinit(); // keep the USB2 controller quiescent while SS trains
  fallback_timer_start();
#endif

  return usb30_hw_init();
}

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    ch56x_usb2_int_enable();
    return;
  }
  PFIC_EnableIRQ(TMR0_IRQn);
#endif
  PFIC_EnableIRQ(USBSS_IRQn);
  PFIC_EnableIRQ(LINK_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;
  PFIC_DisableIRQ(USBSS_IRQn);
  PFIC_DisableIRQ(LINK_IRQn);
#if CFG_TUD_WCH_USB30_FALLBACK
  PFIC_DisableIRQ(TMR0_IRQn);
  ch56x_usb2_int_disable();
#endif
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    // address is applied in ch56x_usb2_edpt0_status_complete(); reply with a status ZLP
    (void)dev_addr;
    ch56x_usb2_edpt_xfer(rhport, 0x80, NULL, 0);
    return;
  }
#endif
  (void)rhport;
  // Applied at the status stage (handle_ep0_rx); respond with the status transaction
  _pending_addr = 0x80u | dev_addr;
  _ep0_status_dir = TUSB_DIR_IN;
  ep0_arm_status();
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    ch56x_usb2_edpt0_status_complete(rhport, request);
    return;
  }
#endif
  (void)rhport;
  (void)request; // address is applied in the status-stage event
}

void dcd_remote_wakeup(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    return;
  }
#endif
  (void)rhport;
  USBSS->LINK_CTRL |= USBSS_LINK_CTRL_TX_UX_EXIT; // best effort U-state exit
}

void dcd_sof_enable(uint8_t rhport, bool en) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    ch56x_usb2_sof_enable(rhport, en);
    return;
  }
#endif
  (void)rhport;
  (void)en; // no frame interrupt at SuperSpeed (ITP is ignored)
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    return ch56x_usb2_edpt_open(rhport, desc_edpt);
  }
#endif
  (void)rhport;

  const uint8_t ep_num = tu_edpt_number(desc_edpt->bEndpointAddress);
  const tusb_dir_t dir = tu_edpt_dir(desc_edpt->bEndpointAddress);

  TU_ASSERT(ep_num < EP_MAX);
  // Isochronous: supported via the per-endpoint iso mode bits in UEP_CFG (register model
  // recovered from WCH's USB30 device blob, USB30_ISO_Setendp)

  if (ep_num == 0) {
    return true;
  }

  xfer_ctl_t *xfer = &xfer_status[ep_num][dir];
  xfer->mps = tu_edpt_packet_size(desc_edpt);
  xfer->active = false;
  xfer->stalled = false; // (re)configuring the endpoint clears any prior halt (USB 2.0 §9.4.5)

  // Allocate a RAMX bounce slot once per endpoint direction
  if (xfer->slot == NULL) {
    TU_ASSERT(_pool_brk + BOUNCE_SLOT_SIZE <= CFG_TUD_WCH_USB30_RAMX_POOL_SIZE);
    xfer->slot = &_ramx_pool[_pool_brk];
    _pool_brk += BOUNCE_SLOT_SIZE;
  }

  const bool is_iso = (desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  if (dir == TUSB_DIR_IN) {
    USBSS_TX_CTRL(ep_num) = 0;
    USBSS->UEP_CFG = (USBSS->UEP_CFG & ~USBSS_EP_ISO_TX(ep_num)) | USBSS_EP_T_EN(ep_num) |
                     (is_iso ? USBSS_EP_ISO_TX(ep_num) : 0u);
  } else {
    USBSS_RX_CTRL(ep_num) = 0;
    USBSS->UEP_CFG = (USBSS->UEP_CFG & ~USBSS_EP_ISO_RX(ep_num)) | USBSS_EP_R_EN(ep_num) |
                     (is_iso ? USBSS_EP_ISO_RX(ep_num) : 0u);
  }
  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    ch56x_usb2_edpt_close(rhport, ep_addr);
    return;
  }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  xfer_status[ep_num][dir].active = false;
  xfer_status[ep_num][dir].stalled = false;
  if (dir == TUSB_DIR_IN) {
    USBSS_TX_CTRL(ep_num) = 0;
    USBSS->UEP_CFG &= ~(USBSS_EP_T_EN(ep_num) | USBSS_EP_ISO_TX(ep_num));
  } else {
    USBSS_RX_CTRL(ep_num) = 0;
    USBSS->UEP_CFG &= ~(USBSS_EP_R_EN(ep_num) | USBSS_EP_ISO_RX(ep_num));
  }
}

void dcd_edpt_close_all(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    ch56x_usb2_edpt_close_all(rhport);
    return;
  }
#endif
  (void)rhport;
  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    USBSS_TX_CTRL(ep) = 0;
    USBSS_RX_CTRL(ep) = 0;
    xfer_status[ep][0].active = false;
    xfer_status[ep][1].active = false;
    xfer_status[ep][0].stalled = false;
    xfer_status[ep][1].stalled = false;
    xfer_status[ep][0].slot = NULL;
    xfer_status[ep][1].slot = NULL;
  }
  USBSS->UEP_CFG = USBSS_EP_R_EN(0) | USBSS_EP_T_EN(0);
  _pool_brk = 0;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    return ch56x_usb2_edpt_xfer(rhport, ep_addr, buffer, total_bytes);
  }
#endif
  (void)rhport;
  (void)is_isr;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  xfer_ctl_t *xfer = &xfer_status[ep_num][dir];
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  xfer->armed_len = 0;
  xfer->active = true;

  if (ep_num == 0) {
    if (total_bytes == 0) {
      // status stage
      xfer->active = false;
      _ep0_status_dir = dir;
      ep0_arm_status();
    } else if (dir == TUSB_DIR_IN) {
      ep0_in_arm();
    } else {
      if (_ep0_early_len) {
        // the host already delivered the (single-packet) data stage into _ep0_buf
        uint16_t len = TU_MIN(_ep0_early_len, total_bytes);
        _ep0_early_len = 0;
        memcpy(buffer, _ep0_buf, len);
        xfer->active = false;
        dcd_event_xfer_complete(rhport, 0x00, len, XFER_RESULT_SUCCESS, true);
      } else {
        USBSS->UEP0_RX_CTRL |= USBSS_UEP0_RX_ARM; // OR: preserve the live packet-sequence bits
        usbss_send_erdy(0, false, 1);
      }
    }
    return true;
  }

  if (dir == TUSB_DIR_IN) {
    xfer->bounce = (total_bytes != 0) && !ch56x_is_dma_capable(buffer);
    xfer_in_arm(ep_num);
  } else {
    xfer_out_arm(ep_num);
  }
  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    ch56x_usb2_edpt_stall(rhport, ep_addr);
    return;
  }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  if (ep_num == 0) {
    // cleared automatically at the next SETUP
    USBSS->UEP0_TX_CTRL = USBSS_UEP0_STALL;
    USBSS->UEP0_RX_CTRL = USBSS_UEP0_STALL;
    usbss_send_erdy(0, dir == TUSB_DIR_IN, 1);
    return;
  }

  xfer_status[ep_num][dir].stalled = true;
  // The host must see the STALL: keep NUMP=1 armed and signal ERDY.
  // Silicon limitation (usbtest case 13): a halted data endpoint answers exactly ONE probe
  // with a STALL TP; further probes before CLEAR_FEATURE get no valid response (EPROTO on
  // the host). Verified exhaustively on hardware: re-arming the response (any NUMP, with
  // ERDY, at 30 us tick rate, with an endpoint enable bounce) never revives it, no endpoint
  // or ITP event fires for a transmitted STALL TP, and WCH's own USB30 blob only writes
  // one-shot responses. The CH32H417's reworked endpoint engine adds a persistent
  // RB_EP_TX_HALT mode, evidently to fix exactly this.
  if (dir == TUSB_DIR_IN) {
    USBSS_TX_CTRL(ep_num) = (USBSS_TX_CTRL(ep_num) & USBSS_EP_SEQ_MASK) |
                            (1u << USBSS_EP_NUMP_SHIFT) | USBSS_EP_RES_STALL;
  } else {
    USBSS_RX_CTRL(ep_num) = (USBSS_RX_CTRL(ep_num) & USBSS_EP_SEQ_MASK) |
                            (1u << USBSS_EP_NUMP_SHIFT) | USBSS_EP_RES_STALL;
  }
  usbss_send_erdy(ep_num, dir == TUSB_DIR_IN, 1);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (_fb_state == FB_USB2_ACTIVE) {
    ch56x_usb2_edpt_clear_stall(rhport, ep_addr);
    return;
  }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  // CLEAR_FEATURE(ENDPOINT_HALT) resets the packet sequence: clear everything including seq.
  // A transfer may still be armed (the class driver considers it submitted and won't resubmit):
  // re-arm it with the fresh sequence instead of dropping it, or the endpoint never answers
  // again after clear-halt (usbtest halt/toggle tests 13 and 29)
  xfer_ctl_t *xfer = &xfer_status[ep_num][dir];
  xfer->stalled = false;
  if (dir == TUSB_DIR_IN) {
    USBSS_TX_CTRL(ep_num) = 0;
    if (xfer->active) {
      xfer_in_arm(ep_num);
    }
  } else {
    USBSS_RX_CTRL(ep_num) = 0;
    if (xfer->active) {
      xfer_out_arm(ep_num);
    }
  }
}

#endif
