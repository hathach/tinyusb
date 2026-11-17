/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 HiFiPhile (Zixun LI)
 * SPDX-License-Identifier: MIT
 *
 * Asynchronous host driver for the CH32V305/307 USBHS v1 controller.
 * The single transaction engine is shared round-robin between software pipes.
 * Every packet yields, including packets of a long control or bulk transfer.
 *
 * References: CH32FV2x/V3x RM v2.5, 22.2.1 and 22.2.3; WCH USBHS HOST_KM.
 * Low-speed SPLIT and PRE transactions are unsupported.
 * SPLIT ISO, high-bandwidth endpoints and MDATA continuation are unsupported.
 * Periodic SPLITs use SS microframes 0..3, with per-TT bandwidth admission.
 * TODO: USBH must implement TT recovery after abandoned nonperiodic SPLITs.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_WCH_USBHS_V1) && CFG_TUH_WCH_USBIP_USBHS

#include "wch_usbhs_v1_reg.h"
#include "host/hcd.h"
#include "host/usbh.h"

#define HCD_SPLIT (CFG_TUH_HUB && TUH_OPT_HIGH_SPEED)
#if HCD_SPLIT
  #include "host/hub.h"
#endif

#ifndef CFG_TUH_WCH_USBHS_ENDPOINT_MAX
  #define CFG_TUH_WCH_USBHS_ENDPOINT_MAX (1 + 4 * (CFG_TUH_DEVICE_MAX + CFG_TUH_HUB))
#endif
TU_VERIFY_STATIC(CFG_TUH_WCH_USBHS_ENDPOINT_MAX > 0 && CFG_TUH_WCH_USBHS_ENDPOINT_MAX <= 127,
                 "USBHS pipe index must fit int8_t");

// Four-bit PID values written to HOST_EP_PID and returned in INT_ST.
enum {
  USBHS_PID_OUT = 0x1, USBHS_PID_ACK = 0x2, USBHS_PID_NYET = 0x6,
  USBHS_PID_IN = 0x9, USBHS_PID_NAK = 0xa, USBHS_PID_PING = 0x4,
  USBHS_PID_SETUP = 0xd, USBHS_PID_STALL = 0xe,
#if HCD_SPLIT
  USBHS_PID_ERR = 0xc, USBHS_PID_MDATA = 0xf
#endif
};

enum { NAK_PER_FRAME = 3, ERROR_MAX = 3 };
#if HCD_SPLIT
enum { SPLIT_IDLE, SPLIT_START, SPLIT_COMPLETE };
enum { SPLIT_PERIODIC_SLOTS = 4 };
#endif

typedef struct {
  TU_ATTR_ALIGNED(4) uint8_t setup[8];
  uint8_t *buffer;
  uint32_t due;      // monotonic microframes, also for full/low speed (8 per frame)
  uint32_t interval;
  uint16_t mps;      // zero marks an unused pipe
  uint16_t total;
  uint16_t done;
  uint8_t daddr;
  uint8_t ep_addr;
  uint8_t type;
  uint8_t errors;
  uint8_t naks;
#if HCD_SPLIT
  uint8_t tt_hub;
  uint8_t tt_port;
  uint8_t split_phase;
  uint32_t split_start;
  uint32_t split_order;
  uint8_t split_slot;
  bool tt_multi;
#endif
  bool pending;
  bool setup_pending;
  bool toggle;
  bool ping;
#if HCD_SPLIT
  bool toggle_unknown;
#endif
} hcd_ep_t;

#if HCD_SPLIT
  #define EP_SPLIT(ep) ((ep)->tt_hub != 0)
  #define EP_SPLIT_BUSY(ep) ((ep)->split_phase != SPLIT_IDLE)
  #define EP_TOGGLE_UNKNOWN(ep) ((ep)->toggle_unknown)
#else
  #define EP_SPLIT(ep) false
  #define EP_SPLIT_BUSY(ep) false
  #define EP_TOGGLE_UNKNOWN(ep) false
#endif

static struct {
  hcd_ep_t ep[CFG_TUH_WCH_USBHS_ENDPOINT_MAX];
  uint32_t now;
  uint32_t active_started;
  uint16_t frame_raw;
  int8_t active;
  int8_t last;
#if HCD_SPLIT
  int8_t split_owner;
  uint32_t split_order;
#endif
  bool attached;
  bool resetting;
  bool sending_ping;
  tusb_speed_t root_speed;
} _hcd;

#ifndef USBHS_DMA_ADDRESS
  #define USBHS_DMA_ADDRESS(buffer) ((uintptr_t)(buffer))
#endif

static void engine_kick(void);
static void complete(hcd_ep_t *ep, xfer_result_t result, bool in_isr);

#if HCD_SPLIT
static bool split_complete_older(hcd_ep_t const *ep, uint32_t now);

#ifndef USBHS_IRQ_SAVE
static inline uint32_t usbhs_irq_save(void) {
  uint32_t state;
  // QingKe gintenr, as used by the WCH SDK and this BSP's SystemView lock.
  // RM v2.5 p104 requires fence.i after masking interrupts through a CSR.
  __asm volatile("csrr %0, 0x800\n csrc 0x800, %1\n fence.i"
                 : "=&r"(state) : "r"(0x88) : "memory");
  return state;
}
static inline void usbhs_irq_restore(uint32_t state) {
  __asm volatile("csrw 0x800, %0" :: "r"(state) : "memory");
}
  #define USBHS_IRQ_SAVE() usbhs_irq_save()
  #define USBHS_IRQ_RESTORE(state) usbhs_irq_restore(state)
#endif

#ifndef USBHS_SPLIT_DONE
  #define USBHS_SPLIT_DONE() (USBHSH->INT_FG & USBHS_TRANSFER_FLAG)
#endif

// No IRQ/callback or SIE_FREE wait may separate the prefix from its child.
// The 128-read guard bounds a malfunctioning SIE; it is not a response timeout.
// Return 1 when launched, 0 when the microframe boundary requires deferral,
// -1 when the bounded prefix poll timed out.
static int split_pair(hcd_ep_t *ep, uint8_t token, uint8_t tx_ctrl, uint8_t rx_ctrl) {
  uint8_t const child = (uint8_t)((token << 4) | tu_edpt_number(ep->ep_addr));
  uint16_t const split = (uint16_t)((ep->tt_port << 1) | (ep->type << 10) |
                                  (ep->split_phase == SPLIT_COMPLETE));
  uint32_t const state = USBHS_IRQ_SAVE();
  uint16_t const frame = USBHSH->FRAME_NO;
  uint8_t const micro = (uint8_t)(frame >> USBHS_FRAME_NO_MICROFRAME_SHIFT);
  bool const periodic = ep->type == TUSB_XFER_INTERRUPT;
  if ((USBHSH->MIS_ST & USBHS_SOF_PRES) ||
      (ep->split_phase == SPLIT_START && (micro == 6 || (periodic && micro != ep->split_slot)))) {
    USBHS_IRQ_RESTORE(state);
    return 0;
  }
  uint16_t const raw = (uint16_t)(((frame & USBHS_FRAME_NO_NUM_MASK) << 3) | micro);
  _hcd.active_started = _hcd.now + ((uint16_t)(raw - _hcd.frame_raw) & 0x3fff);
  if (periodic && ep->split_phase == SPLIT_COMPLETE &&
      ((uint32_t)(_hcd.active_started - ep->split_start - 2) > 2 ||
       split_complete_older(ep, _hcd.active_started))) {
    USBHS_IRQ_RESTORE(state);
    return 0;
  }
  // The ordering scan itself may straddle SOF. Keep its timestamp valid up to
  // the bounded prefix/child sequence below.
  if (USBHSH->FRAME_NO != frame || (USBHSH->MIS_ST & USBHS_SOF_PRES)) {
    USBHS_IRQ_RESTORE(state);
    return 0;
  }
  if (ep->split_phase == SPLIT_START) {
    ep->split_start = _hcd.active_started;
    ep->split_order = _hcd.split_order++;
  }
  USBHSH->DEV_AD = ep->tt_hub;
  USBHSH->HOST_SPLIT_DATA = split;
  USBHSH->HOST_RX_CTRL = rx_ctrl;
  USBHSH->HOST_TX_CTRL = USBHS_UH_T_DATA_NO;
  USBHSH->INT_FG = USBHS_TRANSFER_FLAG;
  USBHSH->HOST_EP_PID = 0x80; // SPLIT PID
  uint32_t guard = 128;
  while (!USBHS_SPLIT_DONE() && --guard) {}
  if (!guard) {
    USBHSH->HOST_EP_PID = 0;
    USBHSH->INT_FG = USBHS_TRANSFER_FLAG;
    USBHS_IRQ_RESTORE(state);
    return -1;
  }
  // Preserve this empirically validated immediate MMIO order.
  USBHSH->HOST_EP_PID = 0;
  USBHSH->DEV_AD = ep->daddr;
  USBHSH->HOST_TX_CTRL = tx_ctrl;
  USBHSH->INT_FG = USBHS_TRANSFER_FLAG;
  USBHSH->HOST_EP_PID = child;
  USBHS_IRQ_RESTORE(state);
  return 1;
}

#endif

static hcd_ep_t *ep_find(uint8_t daddr, uint8_t ep_addr) {
  for (unsigned i = 0; i < TU_ARRAY_SIZE(_hcd.ep); i++) {
    hcd_ep_t *ep = &_hcd.ep[i];
    if (ep->mps && ep->daddr == daddr &&
        (ep->ep_addr == ep_addr || (tu_edpt_number(ep->ep_addr) == 0 && tu_edpt_number(ep_addr) == 0))) {
      return ep;
    }
  }
  return NULL;
}

static uint16_t frame_read(void) {
  uint16_t const raw = USBHSH->FRAME_NO;
  return (uint16_t)(((raw & USBHS_FRAME_NO_NUM_MASK) << 3) |
                    ((raw & USBHS_FRAME_NO_MICROFRAME_MASK) >> USBHS_FRAME_NO_MICROFRAME_SHIFT));
}

static void time_update(void) {
  uint16_t const raw = frame_read();
  uint32_t const old = _hcd.now;
  _hcd.now += (uint16_t)(raw - _hcd.frame_raw) & 0x3fff;
  _hcd.frame_raw = raw;
  if ((old >> 3) != (_hcd.now >> 3)) {
    for (unsigned i = 0; i < TU_ARRAY_SIZE(_hcd.ep); i++) {
      _hcd.ep[i].naks = 0;
    }
  }
}

static bool packet_accepted(hcd_ep_t const *ep, uint8_t status) {
  uint8_t const response = status & USBHS_HOST_RES;
  if (ep->setup_pending || tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT) {
    return response == USBHS_PID_ACK || (!EP_SPLIT(ep) && !ep->setup_pending && _hcd.root_speed == TUSB_SPEED_HIGH &&
           ep->type != TUSB_XFER_INTERRUPT && response == USBHS_PID_NYET);
  }
  return (response == 3 || response == 0xb) && (status & USBHS_TOGGLE_OK) != 0;
}

static bool engine_stop(bool preserve_toggle) {
  // INT_BUSY holds the completed transaction until TRANSFER_FLAG is cleared.
  // Remove the token first, including when cancelling a transaction in flight.
  USBHSH->HOST_EP_PID = 0;
  uint32_t timeout = 2000000;
  while (!(USBHSH->MIS_ST & USBHS_SIE_FREE) && --timeout) {}
  bool accepted = false;
  if (preserve_toggle && _hcd.active >= 0 && !_hcd.sending_ping &&
      (USBHSH->INT_FG & USBHS_TRANSFER_FLAG)) {
    hcd_ep_t *ep = &_hcd.ep[_hcd.active];
    bool can_accept = !ep->setup_pending && ep->type != TUSB_XFER_ISOCHRONOUS;
#if HCD_SPLIT
    can_accept = can_accept && (!EP_SPLIT(ep) || ep->split_phase == SPLIT_COMPLETE);
#endif
    if (can_accept && packet_accepted(ep, USBHSH->INT_ST)) {
      ep->toggle = !ep->toggle;
      accepted = true;
      if (!EP_SPLIT(ep) && tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT && _hcd.root_speed == TUSB_SPEED_HIGH) {
        ep->ping = (USBHSH->INT_ST & USBHS_HOST_RES) == USBHS_PID_NYET;
      }
    }
  }
  USBHSH->INT_FG = USBHS_TRANSFER_FLAG;
  _hcd.active = -1;
  _hcd.sending_ping = false;
  return accepted;
}

#if HCD_SPLIT
static void split_release(hcd_ep_t *ep) {
  ep->split_phase = SPLIT_IDLE;
  if (ep->type != TUSB_XFER_INTERRUPT) {
    _hcd.split_owner = -1;
  }
}

static void split_abandon(hcd_ep_t *ep, bool in_isr) {
  if (!EP_SPLIT(ep) || ep->split_phase == SPLIT_IDLE) {
    return;
  }
  if (tu_edpt_number(ep->ep_addr) && tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT) {
    // The TT may have delivered the OUT packet before cancellation. IN can
    // resynchronize by discarding duplicate DATA; OUT needs CLEAR_FEATURE.
    ep->toggle_unknown = true;
  }
  if (ep->type == TUSB_XFER_INTERRUPT) {
    // Let this transaction expire before reusing its TT result entry. Other
    // endpoints may continue; a later matching CS discards abandoned results.
    ep->due = ep->split_start + ep->interval;
  }
  split_release(ep);
  (void)in_isr;
}

static bool split_tt_same(hcd_ep_t const *a, hcd_ep_t const *b) {
  return a->tt_hub == b->tt_hub && (!a->tt_multi || a->tt_port == b->tt_port);
}

static bool split_complete_older(hcd_ep_t const *ep, uint32_t now) {
  // USB2 11.18.8: a later matching CS can discard earlier retained results.
  // NYET defers to the next microframe; transport errors retry before peers.
  for (unsigned i = 0; i < TU_ARRAY_SIZE(_hcd.ep); i++) {
    hcd_ep_t const *other = &_hcd.ep[i];
    if (other->split_phase == SPLIT_COMPLETE && other->type == TUSB_XFER_INTERRUPT &&
        split_tt_same(ep, other) && (int32_t)(other->split_order - ep->split_order) < 0 &&
        (int32_t)(now - other->due) >= 0) {
      return true;
    }
  }
  return false;
}

static uint16_t split_bus_time_us(uint16_t mps) {
  // USB2 5.11.3: full-speed interrupt, worst-case stuffing and 32-bit TT think time.
  uint32_t const bits = 4u + (56u * mps) / 6u;
  return (uint16_t)((9107u + 84u * bits + 2667u + 999u) / 1000u);
}

static bool split_slot_pick(hcd_ep_t *ep) {
  uint16_t load[SPLIT_PERIODIC_SLOTS] = {0};
  for (unsigned i = 0; i < TU_ARRAY_SIZE(_hcd.ep); i++) {
    hcd_ep_t const *other = &_hcd.ep[i];
    if (other != ep && other->mps && other->tt_hub && other->type == TUSB_XFER_INTERRUPT &&
        split_tt_same(ep, other)) {
      load[other->split_slot] += split_bus_time_us(other->mps);
    }
  }
  // Reserve each endpoint every frame, independent of its polling interval.
  // SS0..3 budget FS work in microframes1..4 and keep all CS attempts in-frame.
  uint8_t slot = 0;
  for (uint8_t i = 1; i < SPLIT_PERIODIC_SLOTS; i++) {
    if (load[i] < load[slot]) {
      slot = i;
    }
  }
  ep->split_slot = slot;
  return load[slot] + split_bus_time_us(ep->mps) <= 125;
}

#endif

static void engine_kick(void) {
  if (_hcd.active >= 0 || !_hcd.attached || _hcd.resetting) {
    return;
  }
  // Let SOF go first, then reconsider all pipes using the new frame number.
  if (USBHSH->MIS_ST & USBHS_SOF_PRES) {
    return;
  }
  time_update();
  int8_t const last = _hcd.last;
#if HCD_SPLIT
  bool rescanned = false;
#endif
  // Periodic SPLIT windows take precedence; other pipes retain round-robin service.
  for (unsigned k = 1; k <= TU_ARRAY_SIZE(_hcd.ep) * (HCD_SPLIT ? 2 : 1); k++) {
    int8_t const idx = (int8_t)(((unsigned)(last + k)) % TU_ARRAY_SIZE(_hcd.ep));
    hcd_ep_t *ep = &_hcd.ep[idx];
    if (!ep->pending || (int32_t)(_hcd.now - ep->due) < 0) {
      continue;
    }
#if HCD_SPLIT
    bool const periodic = ep->tt_hub && ep->type == TUSB_XFER_INTERRUPT;
    if ((k <= TU_ARRAY_SIZE(_hcd.ep)) != periodic) {
      continue;
    }
    int8_t const owner = periodic ? -1 : _hcd.split_owner;
    if (ep->tt_hub && (ep->errors >= ERROR_MAX ||
        (owner >= 0 && owner != idx))) {
      continue;
    }
    bool const start_split = ep->tt_hub && ep->split_phase == SPLIT_IDLE;
    if (ep->tt_hub) {
      uint8_t const micro = _hcd.frame_raw & 7;
      if ((start_split && (micro == 6 || (periodic && micro != ep->split_slot))) ||
          (ep->split_phase == SPLIT_COMPLETE && periodic &&
           ((int32_t)(_hcd.now - (ep->split_start + 4)) > 0 || split_complete_older(ep, _hcd.now)))) {
        continue;
      }
      if (start_split) {
        ep->split_phase = SPLIT_START;
        if (!periodic) {
          _hcd.split_owner = idx;
        }
      }
    }
#endif
    _hcd.active = _hcd.last = idx;
    _hcd.sending_ping = !EP_SPLIT(ep) && ep->ping && !ep->setup_pending &&
                        tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT;
    USBHSH->DEV_AD = ep->daddr;
    USBHSH->HOST_EP_TYPE = 0; // restore native asynchronous receive framing
    uint16_t const remaining = ep->total - ep->done;
    USBHSH->HOST_RX_MAX_LEN = tu_min16(ep->mps, remaining);
    uint8_t *dma = ep->setup_pending || !remaining ? ep->setup : ep->buffer + ep->done;
    USBHSH->HOST_TX_DMA = USBHSH->HOST_RX_DMA = (uint32_t)USBHS_DMA_ADDRESS(dma);
    USBHSH->HOST_TX_CTRL = USBHSH->HOST_RX_CTRL = ep->toggle ? USBHS_UH_T_TOG_1 : 0;

    uint8_t pid;
    uint16_t len = 0;
#if HCD_SPLIT
    if (ep->tt_hub && ep->split_phase == SPLIT_COMPLETE) {
      pid = ep->setup_pending ? USBHS_PID_SETUP :
            (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT ? USBHS_PID_OUT : USBHS_PID_IN);
    } else
#endif
    if (ep->setup_pending) {
      pid = USBHS_PID_SETUP;
      len = 8;
      USBHSH->HOST_TX_CTRL = 0;
    } else if (_hcd.sending_ping) {
      pid = USBHS_PID_PING;
      USBHSH->HOST_TX_CTRL |= USBHS_UH_T_DATA_NO;
    } else if (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT) {
      pid = USBHS_PID_OUT;
      len = tu_min16(ep->mps, ep->total - ep->done);
    } else {
      pid = USBHS_PID_IN;
    }
    USBHSH->HOST_TX_LEN = len;
#if HCD_SPLIT
    if (ep->tt_hub) {
      bool const complete_split = ep->split_phase == SPLIT_COMPLETE;
      bool const in = !ep->setup_pending && tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN;
      uint8_t tx_ctrl = ep->setup_pending ? 0 : (ep->toggle ? USBHS_UH_T_TOG_1 : 0);
      uint8_t rx_ctrl = USBHS_UH_R_DATA_NO;
      if (complete_split || in) {
        tx_ctrl = USBHS_UH_T_DATA_NO;
      } else if (ep->type == TUSB_XFER_INTERRUPT) {
        tx_ctrl |= USBHS_UH_T_RES_NO; // interrupt SS OUT has no handshake
      }
      if (complete_split && in) {
        // RM22.2.3.3 synchronous RX suppresses the CS-IN host ACK (bench-verified).
        // This changes receiver framing, not the SPLIT wire endpoint type.
        USBHSH->HOST_EP_TYPE = USBHS_ENDP_RX_ISO;
        // USB2 8.4.2: complete-split DATA has no subsequent host ACK.
        rx_ctrl = (uint8_t)((ep->toggle ? USBHS_UH_R_TOG_1 : 0) | USBHS_UH_R_RES_NO);
      }
      int const launched = split_pair(ep, pid, tx_ctrl, rx_ctrl);
      if (launched <= 0) {
        _hcd.active = -1;
        if (launched == 0) {
          if (start_split) {
            split_release(ep);
          }
          if (USBHSH->MIS_ST & USBHS_SOF_PRES) {
            return;
          }
          uint32_t const before = _hcd.now;
          time_update();
          if (!rescanned && _hcd.now != before) {
            // A boundary may have made an earlier CS eligible after it was
            // skipped. Revisit once, including when this is its final window.
            rescanned = true;
            k = 0;
          }
          continue;
        } else {
          // No child was launched. Revisit through the ISR error path without
          // spinning or invoking a task callback from an unknown context.
          ep->due = _hcd.now + 1;
          ep->errors = ERROR_MAX;
        }
      }
      return;
    }
#endif
    if (ep->type == TUSB_XFER_ISOCHRONOUS) {
      bool const in = tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN;
      USBHSH->HOST_EP_TYPE = in ? USBHS_ENDP_RX_ISO : USBHS_ENDP_TX_ISO;
      USBHSH->HOST_RX_CTRL = USBHS_UH_R_RES_NO;
      USBHSH->HOST_TX_CTRL = USBHS_UH_T_RES_NO;
    }
    _hcd.active_started = _hcd.now + ((uint16_t)(frame_read() - _hcd.frame_raw) & 0x3fff);
    // A periodic packet takes its service interval at launch: aborting it, even after it went
    // out on the wire, must not let a re-arm send a second packet in the same interval.
    if (ep->type == TUSB_XFER_ISOCHRONOUS || ep->type == TUSB_XFER_INTERRUPT) {
      ep->due = _hcd.active_started + ep->interval;
    }
    USBHSH->INT_FG = USBHS_TRANSFER_FLAG;
    USBHSH->HOST_EP_PID = (uint8_t)((pid << 4) | tu_edpt_number(ep->ep_addr));
    return;
  }
}

static void complete(hcd_ep_t *ep, xfer_result_t result, bool in_isr) {
  uint8_t const daddr = ep->daddr;
  uint8_t const ep_addr = ep->ep_addr;
  uint16_t const done = ep->done;
  ep->pending = ep->setup_pending = false;
  engine_kick();
  hcd_event_xfer_complete(daddr, ep_addr, done, result, in_isr);
}

#if HCD_SPLIT
static void split_retry_start(hcd_ep_t *ep, bool error, bool in_isr) {
  bool const periodic = ep->type == TUSB_XFER_INTERRUPT;
  if (error && ++ep->errors >= ERROR_MAX) {
    split_abandon(ep, in_isr);
    complete(ep, XFER_RESULT_FAILED, in_isr);
    return;
  }
  if (!error) {
    ep->errors = 0;
  }
  if (periodic) {
    ep->due = ep->split_start + ep->interval;
  } else {
    ep->due = _hcd.now;
    if (++ep->naks >= NAK_PER_FRAME) {
      ep->due = (_hcd.now & ~7u) + 8;
    }
  }
  split_release(ep);
}

// Return false only for a valid complete-split packet: the common path then
// advances length and toggle exactly once, never on a start-split.
static bool split_done(hcd_ep_t *ep, uint8_t status, uint16_t received, bool in_isr) {
  uint8_t const response = status & USBHS_HOST_RES;
  bool const periodic = ep->type == TUSB_XFER_INTERRUPT;
  if (ep->split_phase == SPLIT_START) {
    if (periodic || response == USBHS_PID_ACK) {
      // Interrupt SS has no handshake, including IN. Its normal timeout-like
      // completion is not a downstream USB error (USB2 figures11-68/70).
      ep->split_phase = SPLIT_COMPLETE;
      ep->due = ep->split_start + 2;
      if (!periodic) {
        ep->errors = 0;
      }
    } else if (response == USBHS_PID_NAK) {
      split_retry_start(ep, false, in_isr);
      return true;
    } else if (++ep->errors >= ERROR_MAX) {
      split_abandon(ep, in_isr);
      complete(ep, XFER_RESULT_FAILED, in_isr);
      return true;
    } else {
      // Repeat the same SS: TT deduplicates a pending SS whose ACK was lost.
      ep->due = _hcd.now + 1;
    }
    return true;
  }
  if (periodic && response == USBHS_PID_MDATA) {
    // The controller's retention of an unexpected MDATA PID is unverified.
    // Do not accept unverified RX data. Drain periodic TT state.
    split_abandon(ep, in_isr);
    complete(ep, XFER_RESULT_FAILED, in_isr);
    return true;
  }
  if (response == USBHS_PID_NYET) {
    if (!periodic) {
      ep->errors = 0;
    }
    if (periodic && (int32_t)(_hcd.now - (ep->split_start + 4)) >= 0) {
      split_retry_start(ep, true, in_isr);
    } else {
      ep->due = _hcd.now + 1;
    }
    return true;
  }
  if (response == USBHS_PID_NAK) {
    split_retry_start(ep, false, in_isr);
    return true;
  }
  if (response == USBHS_PID_STALL) {
    split_abandon(ep, in_isr);
    complete(ep, XFER_RESULT_STALLED, in_isr);
    return true;
  }
  if (periodic && response == USBHS_PID_ERR) {
    split_retry_start(ep, true, in_isr);
    return true;
  }
  bool const in = !ep->setup_pending && tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN;
  uint8_t const expected_data = ep->toggle ? 0xb : 3;
  if (in && (response == 3 || response == 0xb) && response != expected_data &&
      !(status & USBHS_TOGGLE_OK)) {
    // Duplicate downstream DATA: discard, keep expected toggle, restart SS.
    split_retry_start(ep, false, in_isr);
    return true;
  }
  if (packet_accepted(ep, status)) {
    if (in && received > ep->mps) {
      split_abandon(ep, in_isr);
      complete(ep, XFER_RESULT_FAILED, in_isr);
      return true;
    }
    split_release(ep);
    return false;
  }
  // Expected DATA without TOG_OK (e.g. a damaged packet), timeout or unexpected
  // response: retry CS immediately, before another
  // nonperiodic SS can overwrite its TT result. Native/periodic work may run.
  if (++ep->errors >= ERROR_MAX) {
    split_abandon(ep, in_isr);
    complete(ep, XFER_RESULT_FAILED, in_isr);
  } else {
    ep->due = _hcd.now;
  }
  return true;
}

static void split_maintenance(bool in_isr) {
  if (_hcd.active >= 0) {
    hcd_ep_t *ep = &_hcd.ep[_hcd.active];
    if (ep->tt_hub && ep->split_phase == SPLIT_COMPLETE && !ep->setup_pending &&
        tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN && (int32_t)(_hcd.now - _hcd.active_started) >= 2) {
      // RM22.2.1.8: synchronous RX CRC errors need not raise TRANSFER. Give a
      // near-SOF launch one full microframe, then discard DMA/stale status.
      // The IRQ path consumes a real completion before reaching this watchdog.
      engine_stop(false);
      if (ep->type == TUSB_XFER_INTERRUPT && (int32_t)(_hcd.now - (ep->split_start + 4)) > 0) {
        split_retry_start(ep, true, in_isr); // window expired: count this error once
      } else {
        (void)split_done(ep, 0, 0, in_isr); // retain CS ownership until completion or failure
      }
    }
  }
  for (unsigned i = 0; i < TU_ARRAY_SIZE(_hcd.ep); i++) {
    hcd_ep_t *ep = &_hcd.ep[i];
    if (!ep->pending || !EP_SPLIT(ep) || ep->split_phase == SPLIT_IDLE || _hcd.active == (int8_t)i) {
      continue;
    }
    if (ep->errors >= ERROR_MAX || (ep->type != TUSB_XFER_INTERRUPT &&
        (uint32_t)(_hcd.now - ep->split_start) >= 256)) {
      // 32ms is a driver watchdog, not an invented USB NYET retry limit.
      split_abandon(ep, in_isr);
      complete(ep, XFER_RESULT_FAILED, in_isr);
    } else if (ep->type == TUSB_XFER_INTERRUPT &&
               (int32_t)(_hcd.now - (ep->split_start + 4)) > 0) {
      split_retry_start(ep, true, in_isr);
    }
  }
}

#endif

static void transaction_done(bool in_isr) {
  uint8_t const status = USBHSH->INT_ST;
  uint8_t const response = status & USBHS_HOST_RES;
  uint16_t const received = USBHSH->RX_LEN;
  int8_t const idx = _hcd.active;
  bool const ping = _hcd.sending_ping;
  USBHSH->HOST_EP_PID = 0;
  USBHSH->INT_FG = USBHS_TRANSFER_FLAG;
  _hcd.active = -1;
  _hcd.sending_ping = false;
  if (idx < 0) {
    return;
  }
  hcd_ep_t *ep = &_hcd.ep[idx];
#if HCD_SPLIT
  if (ep->tt_hub && split_done(ep, status, received, in_isr)) {
    return;
  }
#endif
  if (ep->type == TUSB_XFER_ISOCHRONOUS) {
    bool const in = tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN;
    bool const valid = !in || (response == 3 && (status & USBHS_TOGGLE_OK) &&
                              received <= ep->mps && received <= ep->total);
    if (valid) {
      ep->done = in ? received : ep->total;
    }
    complete(ep, valid ? XFER_RESULT_SUCCESS : XFER_RESULT_FAILED, in_isr);
    return;
  }
  bool const periodic = ep->type == TUSB_XFER_INTERRUPT;
  if (periodic) {
#if HCD_SPLIT
    ep->due = (ep->tt_hub ? ep->split_start : _hcd.now) + ep->interval;
#else
    ep->due = _hcd.now + ep->interval;
#endif
  }
  if (ping && response == USBHS_PID_ACK) {
    ep->ping = false;
    ep->errors = 0;
  } else if (!ping && packet_accepted(ep, status)) {
    ep->errors = ep->naks = 0;
    ep->toggle = !ep->toggle;
    if (ep->setup_pending) {
      ep->done = 8;
      complete(ep, XFER_RESULT_SUCCESS, in_isr);
      return;
    }
    bool short_packet = false;
    if (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT) {
      ep->done += tu_min16(ep->mps, ep->total - ep->done);
      ep->ping = !EP_SPLIT(ep) && _hcd.root_speed == TUSB_SPEED_HIGH && response == USBHS_PID_NYET && !periodic;
    } else {
      if (received > ep->mps || received > ep->total - ep->done) {
        complete(ep, XFER_RESULT_FAILED, in_isr);
        return;
      }
      uint16_t const len = tu_min16(received, ep->total - ep->done);
      ep->done += len;
      short_packet = received < ep->mps;
    }
    if (ep->done == ep->total || short_packet) {
      complete(ep, XFER_RESULT_SUCCESS, in_isr);
      return;
    }
  } else if (response == USBHS_PID_STALL) {
    complete(ep, XFER_RESULT_STALLED, in_isr);
    return;
  } else if (response == USBHS_PID_NAK) {
    ep->errors = 0;
    if (!ep->setup_pending && !periodic && _hcd.root_speed == TUSB_SPEED_HIGH &&
        tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT) {
      ep->ping = true;
    }
    if (!periodic && ++ep->naks >= NAK_PER_FRAME) {
      ep->due = (_hcd.now & ~7u) + 8;
    }
  } else {
    if (++ep->errors >= ERROR_MAX) {
      complete(ep, XFER_RESULT_FAILED, in_isr);
      return;
    }
    if (!periodic) {
      ep->due = (_hcd.now & ~7u) + 8;
    }
  }
}

static void port_check(uint8_t rhport, bool in_isr) {
  if (_hcd.resetting) {
    return;
  }
  bool const attached = (USBHSH->MIS_ST & USBHS_ATTACH) != 0;
  if (attached != _hcd.attached) {
    _hcd.attached = attached;
    if (attached) {
      hcd_event_device_attach(rhport, in_isr);
    } else {
      engine_stop(false);
#if HCD_SPLIT
      _hcd.split_owner = -1;
#endif
      for (unsigned i = 0; i < TU_ARRAY_SIZE(_hcd.ep); i++) {
        _hcd.ep[i].pending = false;
#if HCD_SPLIT
        _hcd.ep[i].split_phase = SPLIT_IDLE;
#endif
      }
      hcd_event_device_remove(rhport, in_isr);
    }
  }
}

static uint8_t host_speed_bits(tusb_speed_t speed) {
  return TUH_OPT_HIGH_SPEED && speed != TUSB_SPEED_FULL ? USBHS_HIGH_SPEED : USBHS_FULL_SPEED;
}

bool hcd_init(uint8_t rhport, tusb_rhport_init_t const *rh_init) {
  hcd_int_disable(rhport);
  tu_memclr(&_hcd, sizeof(_hcd));
  _hcd.active = _hcd.last = -1;
#if HCD_SPLIT
  _hcd.split_owner = -1;
#endif
  _hcd.root_speed = TUSB_SPEED_FULL;
  USBHSH->CONTROL = USBHS_FORCE_RST | USBHS_ALL_CLR;
  tusb_time_delay_ms_api(1);
  uint8_t const speed = host_speed_bits(rh_init->speed);
  USBHSH->CONTROL = USBHS_HOST_MODE | speed | USBHS_INT_BUSY_EN | USBHS_DMA_EN;
  USBHSH->DEV_AD = 0;
  USBHSH->HOST_EP_PID = 0;
  // SDK HOST_KM enables both bits. The English RM 22.2.3.2 reverses their
  // descriptions; the Chinese table identifies them as RX/TX enable bits.
  USBHSH->HOST_EP_CONFIG = USBHS_HOST_TX_EN | USBHS_HOST_RX_EN;
  USBHSH->HOST_EP_TYPE = 0;
  USBHSH->HOST_RX_DMA = USBHSH->HOST_TX_DMA = (uint32_t)USBHS_DMA_ADDRESS(_hcd.ep[0].setup);
  USBHSH->HOST_RX_MAX_LEN = 0;
  USBHSH->HOST_RX_CTRL = USBHSH->HOST_TX_CTRL = 0;
  USBHSH->HOST_CTRL = USBHS_PHY_SUSPENDM;
  _hcd.frame_raw = frame_read();
  USBHSH->INT_FG = 0xff;
  USBHSH->INT_EN = USBHS_DETECT_EN | USBHS_TRANSFER_EN | USBHS_SOF_ACT_EN;
  // An already attached device need not produce another DETECT edge.
  port_check(rhport, false);
  hcd_int_enable(rhport);
  return true;
}

bool hcd_deinit(uint8_t rhport) {
  hcd_int_disable(rhport);
  engine_stop(false);
  USBHSH->INT_EN = 0;
  USBHSH->CONTROL = USBHS_FORCE_RST | USBHS_ALL_CLR;
  return true;
}

bool hcd_configure(uint8_t rhport, uint32_t cfg_id, void const *cfg_param) {
  (void)rhport; (void)cfg_id; (void)cfg_param;
  return false;
}

void hcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USBHS_IRQn);
}

void hcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USBHS_IRQn);
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void)rhport;
  return _hcd.now >> 3;
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void)rhport;
  return (USBHSH->MIS_ST & USBHS_ATTACH) != 0;
}

void hcd_port_reset(uint8_t rhport) {
  hcd_int_disable(rhport);
  _hcd.resetting = true;
  engine_stop(false);
  USBHSH->DEV_AD = 0;
  USBHSH->HOST_CTRL = USBHS_PHY_SUSPENDM | USBHS_SEND_BUS_RESET;
  hcd_int_enable(rhport);
}

void hcd_port_reset_end(uint8_t rhport) {
  USBHSH->HOST_CTRL &= (uint8_t)~USBHS_SEND_BUS_RESET;
  tusb_time_delay_ms_api(1);
  hcd_int_disable(rhport);
  uint8_t const speed = USBHSH->SPEED_TYPE & USBHS_SPEED_TYPE_MASK;
  _hcd.root_speed = speed == USBHS_SPEED_TYPE_HIGH ? TUSB_SPEED_HIGH :
                    (speed == USBHS_SPEED_TYPE_LOW ? TUSB_SPEED_LOW : TUSB_SPEED_FULL);
  _hcd.frame_raw = frame_read();
  if (hcd_port_connect_status(rhport)) {
    USBHSH->HOST_CTRL |= USBHS_SEND_SOF_EN;
  }
  USBHSH->INT_FG = USBHS_DETECT_FLAG | USBHS_HST_SOF_FLAG;
  _hcd.resetting = false;
  port_check(rhport, false);
  hcd_int_enable(rhport);
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void)rhport;
  return _hcd.root_speed;
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  uint8_t const flags = USBHSH->INT_FG;
  if (flags & USBHS_DETECT_FLAG) {
    USBHSH->INT_FG = USBHS_DETECT_FLAG;
    port_check(rhport, in_isr);
  }
  if (flags & USBHS_HST_SOF_FLAG) {
    USBHSH->INT_FG = USBHS_HST_SOF_FLAG;
    if (!_hcd.resetting) {
      time_update();
    }
  }
  if ((flags & USBHS_TRANSFER_FLAG) && (USBHSH->INT_FG & USBHS_TRANSFER_FLAG)) {
    transaction_done(in_isr);
  }
  if (!_hcd.resetting && _hcd.attached) {
    if (_hcd.active >= 0) {
      hcd_ep_t *ep = &_hcd.ep[_hcd.active];
      uint32_t const timeout = _hcd.root_speed == TUSB_SPEED_HIGH ? 2 : 16;
      if (ep->type == TUSB_XFER_ISOCHRONOUS && (int32_t)(_hcd.now - _hcd.active_started) >= (int32_t)timeout) {
        engine_stop(false);
        complete(ep, XFER_RESULT_FAILED, in_isr);
      }
    }
#if HCD_SPLIT
    split_maintenance(in_isr);
#endif
  }
  // Schedule retries and remaining work after transaction and timeout handling.
  engine_kick();
}

bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, tusb_desc_endpoint_t const *desc) {
  tuh_bus_info_t bus;
  TU_VERIFY(tuh_bus_info_get(daddr, &bus));
#if HCD_SPLIT
  bool const split = _hcd.root_speed == TUSB_SPEED_HIGH && bus.speed == TUSB_SPEED_FULL;
  TU_VERIFY(split || bus.speed == _hcd.root_speed);
  uint8_t tt_hub = 0, tt_port = 0;
  if (split) {
    tt_hub = bus.hub_addr;
    tt_port = bus.hub_port;
    bool found = false;
    for (unsigned depth = CFG_TUH_HUB; tt_hub && depth; depth--) {
      tuh_bus_info_t parent;
      TU_VERIFY(tuh_bus_info_get(tt_hub, &parent));
      if (parent.speed == TUSB_SPEED_HIGH) {
        found = true;
        break;
      }
      tt_hub = parent.hub_addr;
      tt_port = parent.hub_port;
    }
    TU_VERIFY(found);
  }
#else
  TU_VERIFY(bus.speed == _hcd.root_speed);
#endif
  TU_VERIFY(bus.speed != TUSB_SPEED_LOW || bus.hub_addr == 0);
  uint8_t const type = desc->bmAttributes.xfer;
  uint16_t const mps = tu_edpt_packet_size(desc);
  TU_VERIFY((tu_le16toh(desc->wMaxPacketSize) & 0x1800) == 0); // no high-bandwidth transactions
#if HCD_SPLIT
  TU_VERIFY(!split || type != TUSB_XFER_ISOCHRONOUS);
#endif
  uint32_t interval = 8;
  if (type == TUSB_XFER_INTERRUPT || type == TUSB_XFER_ISOCHRONOUS) {
    uint8_t const binterval = tu_max8(desc->bInterval, 1);
    if (bus.speed == TUSB_SPEED_HIGH || type == TUSB_XFER_ISOCHRONOUS) {
      interval = (bus.speed == TUSB_SPEED_HIGH ? 1u : 8u) << (tu_min8(binterval, 16) - 1);
    } else {
      interval = 8u * binterval;
    }
  }
  hcd_int_disable(rhport);
  hcd_ep_t *ep = ep_find(daddr, desc->bEndpointAddress);
  if (ep && (ep->pending || EP_SPLIT_BUSY(ep))) {
    hcd_int_enable(rhport);
    return false;
  }
  if (!ep) {
    for (unsigned i = 0; i < TU_ARRAY_SIZE(_hcd.ep); i++) {
      if (!_hcd.ep[i].mps) {
        ep = &_hcd.ep[i];
        break;
      }
    }
  }
  if (ep) {
    tu_memclr(ep, sizeof(*ep));
    ep->daddr = daddr;
    ep->ep_addr = desc->bEndpointAddress;
    ep->mps = mps;
    ep->type = type;
    ep->interval = interval;
    ep->due = _hcd.now;
#if HCD_SPLIT
    ep->tt_hub = tt_hub;
    ep->tt_port = tt_port;
    ep->tt_multi = tt_hub && hub_tt_multi(tt_hub);
    if (split && type == TUSB_XFER_INTERRUPT) {
      // A close/reopen must not reuse a canceled TT entry or its consumed budget
      // in this frame. Starting next frame also makes admission independent of it.
      time_update();
      ep->due = _hcd.now + 8 - (_hcd.frame_raw & 7u);
      if (!split_slot_pick(ep)) {
        tu_memclr(ep, sizeof(*ep));
        ep = NULL;
      }
    }
#endif
  }
  hcd_int_enable(rhport);
  return ep != NULL;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, uint8_t const setup[8]) {
  hcd_ep_t *ep = ep_find(daddr, 0);
  TU_VERIFY(ep != NULL);
  hcd_int_disable(rhport);
  if (ep->pending || EP_SPLIT_BUSY(ep)) {
    hcd_int_enable(rhport);
    return false;
  }
  memcpy(ep->setup, setup, 8);
  ep->ep_addr = 0;
  ep->toggle = ep->ping = false;
#if HCD_SPLIT
  ep->toggle_unknown = false;
#endif
  ep->setup_pending = ep->pending = true;
  ep->errors = ep->naks = 0;
  ep->total = 8;
  ep->done = 0;
  ep->due = _hcd.now;
  engine_kick();
  hcd_int_enable(rhport);
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t *buffer, uint16_t len) {
  hcd_ep_t *ep = ep_find(daddr, ep_addr);
  TU_VERIFY(ep != NULL);
  TU_VERIFY(ep->type != TUSB_XFER_ISOCHRONOUS || len <= ep->mps);
  hcd_int_disable(rhport);
  if (ep->pending || EP_SPLIT_BUSY(ep) || EP_TOGGLE_UNKNOWN(ep)) {
    hcd_int_enable(rhport);
    return false;
  }
  if (tu_edpt_number(ep_addr) == 0) {
    ep->ep_addr = ep_addr;
    ep->toggle = true;
  }
  ep->buffer = buffer;
  ep->total = len;
  ep->done = 0;
  ep->errors = ep->naks = 0;
  ep->pending = true;
  ep->setup_pending = false;
  if ((ep->type != TUSB_XFER_INTERRUPT && ep->type != TUSB_XFER_ISOCHRONOUS) || (int32_t)(_hcd.now - ep->due) >= 0) {
    ep->due = _hcd.now;
  }
  engine_kick();
  hcd_int_enable(rhport);
  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  hcd_int_disable(rhport);
  hcd_ep_t *ep = ep_find(daddr, ep_addr);
  if (ep) {
    bool accepted = false;
    if (_hcd.active >= 0 && ep == &_hcd.ep[_hcd.active]) {
      accepted = engine_stop(true);
    }
#if HCD_SPLIT
    if (accepted && ep->tt_hub) {
      if (ep->type == TUSB_XFER_INTERRUPT) {
        ep->due = ep->split_start + ep->interval;
      }
      split_release(ep);
    } else {
      split_abandon(ep, false);
    }
#else
    (void)accepted;
#endif
    ep->pending = ep->setup_pending = false;
    engine_kick();
  }
  hcd_int_enable(rhport);
  return ep != NULL;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  hcd_int_disable(rhport);
  hcd_ep_t *ep = ep_find(daddr, ep_addr);
  if (ep) {
    if (_hcd.active >= 0 && ep == &_hcd.ep[_hcd.active]) {
      engine_stop(false);
    }
#if HCD_SPLIT
    split_abandon(ep, false);
#endif
    tu_memclr(ep, sizeof(*ep));
    engine_kick();
  }
  hcd_int_enable(rhport);
  return true;
}

void hcd_device_close(uint8_t rhport, uint8_t daddr) {
  hcd_int_disable(rhport);
  if (_hcd.active >= 0 && _hcd.ep[_hcd.active].daddr == daddr) {
    engine_stop(false);
  }
  for (unsigned i = 0; i < TU_ARRAY_SIZE(_hcd.ep); i++) {
    hcd_ep_t *ep = &_hcd.ep[i];
    bool matches = ep->daddr == daddr;
#if HCD_SPLIT
    matches = matches || (daddr && ep->tt_hub == daddr);
#endif
    if (ep->mps && matches) {
      if (_hcd.active == (int8_t)i) {
        engine_stop(false);
      }
#if HCD_SPLIT
      if (daddr && ep->tt_hub == daddr) {
        if (ep->split_phase != SPLIT_IDLE) {
          split_release(ep); // its TT is no longer reachable
        }
      } else {
        split_abandon(ep, false);
      }
#endif
      tu_memclr(ep, sizeof(*ep));
    }
  }
  engine_kick();
  hcd_int_enable(rhport);
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  hcd_int_disable(rhport);
  hcd_ep_t *ep = ep_find(daddr, ep_addr);
  bool const ok = ep != NULL && !ep->pending;
  if (ok) {
    ep->toggle = ep->ping = false;
#if HCD_SPLIT
    ep->toggle_unknown = false;
#endif
  }
  hcd_int_enable(rhport);
  return ok;
}

#endif
