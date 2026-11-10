/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * SPDX-FileCopyrightText: Copyright (c) 2021 Ha Thach (tinyusb.org) for Double Buffered
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && (CFG_TUSB_MCU == OPT_MCU_RP2040) && !CFG_TUH_RPI_PIO_USB && !CFG_TUH_MAX3421

  #include "pico.h"

  #if defined(PICO_RP2350) && PICO_RP2350 == 1
    #define HAS_STOP_EPX_ON_NAK
  #endif
  // Interrupt endpoints are polled on EPX in software by bInterval (EPSTATE_WAITING), not by the controller's interrupt
  // endpoint poller, which is left unused. On both chips that poller corrupts the EPX transfer in progress, writing a
  // zero-length FULL|LAST status into EPX's armed buffer (or upsetting its DATA PID), the packet the device sent ACKed
  // and lost: when one of its polls times out (its device unplugged behind a hub, until the hub reports it; on RP2040
  // seen with a low-speed device) and, on RP2040, when it sends an interrupt OUT. On EPX such a timeout is an ordinary
  // timeout of the poll itself. A poll ends at the device's NAK: by stop-on-NAK on RP2350, as an interrupt-type transfer
  // on RP2040 (epx_poll_nak()).

// port 0 is native USB port, other is counted as software PIO
  #define RHPORT_NATIVE 0

  //--------------------------------------------------------------------+
  // INCLUDE
  //--------------------------------------------------------------------+
  #include "rp2040_usb.h"
  #include "osal/osal.h"

  #include "host/hcd.h"
  #include "host/usbh.h"

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Host mode uses one shared endpoint register for non-interrupt endpoint
static hw_endpoint_t  ep_pool[USB_MAX_ENDPOINTS];
static hw_endpoint_t *epx = &ep_pool[0]; // current active endpoint

// Between hcd_init() and hcd_deinit(). tuh_deinit() calls hcd_deinit() before it removes the devices, so
// hcd_device_close() can come after rp2usb_deinit() has freed the lock it would take.
static bool hcd_running = false;

  #ifndef HAS_STOP_EPX_ON_NAK
// Set by the SOF round-robin for the transfer active on EPX at that SOF; any EPX completion or start clears it
static volatile bool epx_switch_request = false;
// The round-robin stopped EPX with a transaction still on the wire: it saves and switches at the next SOF
static bool epx_stop_deferred = false;
// Length armed for the interrupt IN poll on EPX, given back when the poll gets no data (epx_poll_nak())
static uint16_t epx_poll_len = 0;
// The interrupt IN poll on EPX timed out: the RX timeout handler clears RX_TIMEOUT before epx_poll_nak() sees the poll's
// zero-length completion
static bool epx_poll_timeout = false;
// SIE_STATUS bits that end an interrupt-type EPX poll without data (with RX_TIMEOUT, see epx_poll_timeout)
enum {
  EPX_POLL_NO_DATA_BITS = USB_SIE_STATUS_NAK_REC_BITS | USB_SIE_STATUS_CRC_ERROR_BITS |
                          USB_SIE_STATUS_BIT_STUFF_ERROR_BITS | USB_SIE_STATUS_RX_OVERFLOW_BITS
};
  #endif

// After an RX timeout fails the EPX transfer, the SIE can still make one more attempt of that transaction, whose
// timeout (20-50 us later at full speed) would fail whatever EPX started meanwhile (#4170). So EPX starts nothing for
// EPX_RX_TIMEOUT_HOLD_US: transfers queue as pending, and the next EPX request or SOF after the hold starts them.
enum {
  EPX_RX_TIMEOUT_HOLD_US = 1000
};
static bool     epx_hold = false;
static uint32_t epx_hold_start;

// Whether the hold after an RX timeout still applies
static bool __tusb_irq_path_func(epx_held)(void) {
  if (epx_hold && (time_us_32() - epx_hold_start) >= EPX_RX_TIMEOUT_HOLD_US) {
    epx_hold = false;
  }
  return epx_hold;
}

// The SIE retries a transaction that timed out by itself. Like host controllers' error counters, the transfer fails only
// on the EPX_RX_TIMEOUT_MAX-th timeout in a row: a low-speed device behind a hub misses one now and then (#4414). The SIE
// retries at once (the next timeout follows within ~50 us at full speed), so only timeouts EPX_RX_TIMEOUT_GAP_US or less
// apart count as in a row. (An interrupt poll on RP2040 is not retried: see epx_poll_nak().)
enum {
  EPX_RX_TIMEOUT_MAX    = 3,
  EPX_RX_TIMEOUT_GAP_US = 500
};
static uint8_t  epx_rx_timeouts = 0;
static uint32_t epx_rx_timeout_last;

// The endpoint on EPX that hcd_edpt_close() closed during its abort: epx_abort_finish() releases it once the
// transaction under way has ended. Until then it stays allocated (max_packet_size), and edpt_find() skips it.
static hw_endpoint_t *epx_closing = NULL;

// sof_service() made an interrupt poll pending: a bulk transfer streaming on EPX yields to it (epx_poll_waiting())
static bool epx_poll_due = false;
// The transfer on EPX left a completed buffer unarmed to yield (epx_yield()): its next buffer completion stops it
static bool epx_yielding = false;
// The transfer that yielded to due polls (epx_yield()): once they have run it goes on before the others' turn, so a
// poll only comes in between its packets (epx_next_pending())
static hw_endpoint_t *epx_yielded = NULL;

// A transfer starts on EPX: on RP2040 it gets its own two-SOF window. A request left by a transfer that ended without
// a buffer completion (RX timeout, STALL) would otherwise stop it at its first SOF, even right after it completed.
TU_ATTR_ALWAYS_INLINE static inline void epx_xfer_started(void) {
  #ifndef HAS_STOP_EPX_ON_NAK
  epx_switch_request = false;
  epx_stop_deferred  = false;
  #endif
  epx_rx_timeouts = 0;
  epx_yielding    = false;
}

enum {
  SIE_CTRL_SPEED_DISCONNECT = 0,
  SIE_CTRL_SPEED_LOW        = 1,
  SIE_CTRL_SPEED_FULL       = 2,
};

enum {
  EPX_CTRL_DEFAULT = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER | offsetof(usb_host_dpram_t, epx_data)
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static hw_endpoint_t *edpt_alloc(void) {
  for (uint i = 1; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if (ep->max_packet_size == 0) {
      return ep;
    }
  }
  return NULL;
}

static hw_endpoint_t *edpt_find(uint8_t daddr, uint8_t ep_addr) {
  for (uint32_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if ((ep->dev_addr == daddr) && (ep->max_packet_size > 0) && (ep != epx_closing) &&
        (ep->ep_addr == ep_addr || (tu_edpt_number(ep_addr) == 0 && tu_edpt_number(ep->ep_addr) == 0))) {
      return ep;
    }
  }

  return NULL;
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint8_t dev_speed(void) {
  return (usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS) >> USB_SIE_STATUS_SPEED_LSB;
}

TU_ATTR_ALWAYS_INLINE static inline bool need_pre(uint8_t dev_addr) {
  // If this device is different to the speed of the root device
  // (i.e. is a low speed device on a full speed hub) then need pre
  return hcd_port_speed_get(0) != tuh_speed_get(dev_addr);
}

//--------------------------------------------------------------------+
// EPX
//--------------------------------------------------------------------+
enum {
  EPX_DATA_SEQ_RETRY_MAX = 3 // resends of an EPX IN transfer after DATA_SEQ errors with no packet received between
};

TU_ATTR_ALWAYS_INLINE static inline void sie_stop_xfer(void) {
  uint32_t sie_ctrl = (usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK) | USB_SIE_CTRL_STOP_TRANS_BITS;
  usb_hw->sie_ctrl  = sie_ctrl;
  while (usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS) {}
}

static void __tusb_irq_path_func(sie_start_xfer)(bool send_setup, bool is_rx, bool need_pre) {
  uint32_t sie_ctrl = usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK; // preserve base bits
  if (send_setup) {
    sie_ctrl |= USB_SIE_CTRL_SEND_SETUP_BITS;
  } else {
    sie_ctrl |= (is_rx ? USB_SIE_CTRL_RECEIVE_DATA_BITS : USB_SIE_CTRL_SEND_DATA_BITS);
  }
  if (need_pre) {
    sie_ctrl |= USB_SIE_CTRL_PREAMBLE_EN_BITS;
  }

  // START_TRANS bit on SIE_CTRL has the same behavior as the AVAILABLE bit
  // described in RP2040 Datasheet, release 2.1, section "4.1.2.5.1. Concurrent access".!
  // We write everything except the START_TRANS bit first, then wait some cycles.
  usb_hw->sie_ctrl = sie_ctrl;
  busy_wait_at_least_cycles(12);
  usb_hw->sie_ctrl = sie_ctrl | USB_SIE_CTRL_START_TRANS_BITS;
}

// prepare epx_ctrl register for new endpoint
TU_ATTR_ALWAYS_INLINE static inline void epx_ctrl_prepare(const hw_endpoint_t *ep) {
  // An interrupt-type EPX ends a poll the device NAKs with a zero-length buffer completion. RP2350 programs an interrupt
  // endpoint as bulk, so that a NAK stops EPX instead (stop-on-NAK); RP2040 has no stop-on-NAK and keeps an IN as
  // interrupt type for that ending (epx_poll_nak()), and programs an OUT as bulk, its NAK retried until the round-robin
  // switches it out
  uint8_t transfer_type = ep->transfer_type;
  if (transfer_type == TUSB_XFER_INTERRUPT) {
  #ifndef HAS_STOP_EPX_ON_NAK
    if (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT)
  #endif
    {
      transfer_type = TUSB_XFER_BULK;
    }
  }
  usbh_dpram->epx_ctrl = EPX_CTRL_DEFAULT | ((uint32_t)transfer_type << EP_CTRL_BUFFER_TYPE_LSB);
}

// Undo PID toggle and buffer accounting for EPX buffers NOT completed on the wire (called after STOP_TRANS).
// A buffer completed on wire means: controller reached STATUS phase (ACK received).
//   OUT completed: FULL cleared to 0 in STATUS phase (was 1 when armed)
//   IN  completed: FULL set to 1 in STATUS phase (was 0 when armed)
// So undo when: AVAIL=1 (never started), or (OUT: FULL=1) or (IN: FULL=0)
// pid_only: give back the toggles alone, for an aborted transfer whose user_buf/remaining_len may already hold the next
static void __tusb_irq_path_func(epx_undo_unsent_ex)(hw_endpoint_t *ep, bool pid_only) {
  uint32_t   buf_ctrl = usbh_dpram->epx_buf_ctrl;
  const bool is_out   = (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT);

  do {
    const uint16_t bc16 = (uint16_t)buf_ctrl;
    if (bc16) {
      const bool avail = (bc16 & USB_BUF_CTRL_AVAIL);
      const bool full  = (bc16 & USB_BUF_CTRL_FULL);
      if (avail || (is_out ? full : !full)) {
        const uint16_t buf_len = bc16 & USB_BUF_CTRL_LEN_MASK;
        ep->next_pid ^= 1u;
        if (!pid_only) {
          ep->remaining_len += buf_len;
          if (is_out) {
            ep->user_buf -= buf_len;
          }
        }
      }
    }

    if (usbh_dpram->epx_ctrl & EP_CTRL_DOUBLE_BUFFERED_BITS) {
      buf_ctrl >>= 16;
    } else {
      buf_ctrl = 0;
    }
  } while (buf_ctrl > 0);
}

TU_ATTR_ALWAYS_INLINE static inline void epx_undo_unsent(hw_endpoint_t *ep) {
  epx_undo_unsent_ex(ep, false);
}

// Save buffer context for EPX preemption (called after STOP_TRANS)
static void __tusb_irq_path_func(epx_save_context)(hw_endpoint_t *ep) {
  epx_undo_unsent(ep);
  usbh_dpram->epx_buf_ctrl = 0;

  ep->state = EPSTATE_PENDING;
}

// switch epx to new endpoint and start the transfer
static void __tusb_irq_path_func(epx_switch_ep)(hw_endpoint_t *ep) {
  const bool is_setup = (ep->state == EPSTATE_PENDING_SETUP);
  if (ep == epx_yielded) {
    epx_yielded = NULL;
  }

  epx       = ep; // switch pointer
  ep->state = EPSTATE_ACTIVE;
  epx_xfer_started();
  if (ep->transfer_type == TUSB_XFER_INTERRUPT) {
  #ifdef HAS_STOP_EPX_ON_NAK
    usb_hw_set->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS; // one poll: a NAK ends it until the next interval
  #else
    usb_hw_clear->sie_status = EPX_POLL_NO_DATA_BITS; // epx_poll_nak() tells by them how this poll ended
    epx_poll_timeout         = false;
  #endif
  }

  if (is_setup) {
    // panic("new setup \n");
    usb_hw->dev_addr_ctrl = ep->dev_addr;
    sie_start_xfer(true, false, ep->need_pre);
  } else {
    const bool is_rx   = (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN);
    io_rw_32  *ep_reg  = &usbh_dpram->epx_ctrl;
    io_rw_32 *buf_reg = &usbh_dpram->epx_buf_ctrl;

    epx_ctrl_prepare(ep);
    rp2usb_buffer_start(ep, ep_reg, buf_reg, is_rx);
  #ifndef HAS_STOP_EPX_ON_NAK
    epx_poll_len = (uint16_t)(usbh_dpram->epx_buf_ctrl & USB_BUF_CTRL_LEN_MASK); // single-buffered: buffer 0
  #endif

    usb_hw->dev_addr_ctrl = (uint32_t)(ep->dev_addr | (tu_edpt_number(ep->ep_addr) << USB_ADDR_ENDP_ENDPOINT_LSB));
    sie_start_xfer(is_setup, is_rx, ep->need_pre);
  }
}

TU_ATTR_ALWAYS_INLINE static inline bool epx_is_pending(const hw_endpoint_t *ep) {
  return ep->state == EPSTATE_PENDING || ep->state == EPSTATE_PENDING_SETUP;
}

// First interrupt endpoint whose poll is pending, or NULL
TU_ATTR_ALWAYS_INLINE static inline hw_endpoint_t *epx_pending_poll(void) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if (ep->state == EPSTATE_PENDING && ep->transfer_type == TUSB_XFER_INTERRUPT) {
      return ep;
    }
  }
  return NULL;
}

// Round-robin find next pending ep after current epx
static hw_endpoint_t *__tusb_irq_path_func(epx_next_pending)(hw_endpoint_t *cur_ep) {
  if (epx_yielded != NULL && cur_ep->transfer_type == TUSB_XFER_INTERRUPT) {
    // after a poll: the next due poll, else the transfer that yielded to them, unless it has ended or was closed. In
    // turn, a transfer that NAKs (a CDC IN with no data) would hold EPX until the round-robin switches it out (RP2040:
    // two SOFs), so a stream yielding every frame would lose most of the bus to it
    hw_endpoint_t *poll_ep = epx_pending_poll();
    if (poll_ep != NULL) {
      return poll_ep;
    }
    if (epx_is_pending(epx_yielded)) {
      return epx_yielded;
    }
    epx_yielded = NULL;
  }
  const uint cur_idx = (uint)(cur_ep - &ep_pool[0]);
  for (uint i = cur_idx + 1; i < TU_ARRAY_SIZE(ep_pool); i++) {
    if (epx_is_pending(&ep_pool[i])) {
      return &ep_pool[i];
    }
  }
  for (uint i = 0; i < cur_idx; i++) {
    if (epx_is_pending(&ep_pool[i])) {
      return &ep_pool[i];
    }
  }
  // last the current one itself: with EPX idle it can be pending again (an interrupt poll due after its NAK)
  return epx_is_pending(cur_ep) ? cur_ep : NULL;
}

// Release an unfinished EPX transfer's buffers: a STALL or RX timeout leaves one AVAILABLE, and a buffer completed
// before it leaves BUFF_STATUS pending, which the ISR would otherwise apply to the next transfer
static void __tusb_irq_path_func(epx_drop_buffers)(void) {
  usbh_dpram->epx_buf_ctrl = 0;
  for (uint i = 0; i < 2 && (usb_hw->buf_status & 1u); i++) { // re-sets once if both buffers completed
    usb_hw_clear->buf_status = 1u;
  }
}

TU_ATTR_ALWAYS_INLINE static inline bool epx_is_aborting(const hw_endpoint_t *ep) {
  return ep->state == EPSTATE_EPX_ABORTING || ep->state == EPSTATE_EPX_ABORTING_PENDING;
}

// EPX has a transfer on it: active, or aborted and waiting for the transaction under way to end
TU_ATTR_ALWAYS_INLINE static inline bool epx_busy(void) {
  return epx->state == EPSTATE_ACTIVE || epx_is_aborting(epx);
}

// Cancel ep's transfer without completing it; if it owns EPX, stop the SIE and drop status latched for it so the
// ISR cannot apply it to the next transfer. Caller holds the critical section and starts the next pending ep.
static void epx_retire(hw_endpoint_t *ep) {
  ep->data_seq_retries = 0;
  if (ep == epx && epx_busy()) {
    const bool is_setup = (usb_hw->sie_ctrl & USB_SIE_CTRL_SEND_SETUP_BITS) != 0;
    sie_stop_xfer();
    if (!is_setup) {
      epx_save_context(ep); // undo PID toggle of buffers not completed on the wire; a SETUP has none
    }
    epx_drop_buffers();

    usb_hw_clear->sie_status =
      USB_SIE_STATUS_RX_TIMEOUT_BITS | USB_SIE_STATUS_TRANS_COMPLETE_BITS | USB_SIE_STATUS_STALL_REC_BITS;
  #ifdef HAS_STOP_EPX_ON_NAK
    usb_hw_clear->nak_poll = USB_NAK_POLL_EPX_STOPPED_ON_NAK_BITS;
  #else
    epx_switch_request = false; // the next transfer gets its own two-SOF window
  #endif
  }

  rp2usb_reset_transfer(ep);
}

static void __tusb_irq_path_func(epx_start_next_pending)(void) {
  if (!epx_busy() && !epx_held()) {
    hw_endpoint_t *next_ep = epx_next_pending(epx);
    if (next_ep != NULL) {
      epx_switch_ep(next_ep);
    }
  }
}

// A new transfer must queue when EPX is busy, held after an RX timeout, or idle with transfers the hold left pending:
// those go first, in turn with it, so a device that keeps timing out cannot hold them off by retrying
static bool epx_must_queue(void) {
  return epx_busy() || epx_held() || epx_next_pending(epx) != NULL;
}

// A transfer was queued as pending: with EPX idle, start the next one now, or have sof_service() do it after a hold
static void epx_queued(void) {
  if (!epx_busy()) {
    usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
    epx_start_next_pending();
  }
}

// Abort the transfer on EPX without stopping the SIE under a transaction: STOP_TRANS there loses the packet the device
// is sending or has just taken, and the toggle given back no longer matches the device's (IN: DATA_SEQ on the next
// packet; OUT: the device drops the next packet as a repeat). EPX keeps the transfer, its completions unreported, until
// the transaction under way has ended: a buffer completion with no other buffer armed, a NAK stop (RP2350), an RX
// timeout, a STALL, or a SOF where the bus is idle (RP2040, as the round-robin stops).
static void epx_abort_start(void) {
  epx->state            = EPSTATE_EPX_ABORTING;
  epx->data_seq_retries = 0;
  #ifdef HAS_STOP_EPX_ON_NAK
  usb_hw_set->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS;
  #else
  epx_switch_request = false;
  usb_hw_set->inte   = USB_INTE_HOST_SOF_BITS;
  #endif
}

// The aborted transfer's transaction has ended: stop EPX, give back the toggles of the buffers that never went out (a
// packet that completed keeps its toggle, as the device saw it), and start the transfer queued meanwhile, if any
static void __tusb_irq_path_func(epx_abort_finish)(void) {
  hw_endpoint_t *ep = epx;
  sie_stop_xfer();
  epx_undo_unsent_ex(ep, true); // user_buf/remaining_len may be the queued transfer's already
  epx_drop_buffers();
  usb_hw_clear->sie_status =
    USB_SIE_STATUS_RX_TIMEOUT_BITS | USB_SIE_STATUS_TRANS_COMPLETE_BITS | USB_SIE_STATUS_STALL_REC_BITS;
  #ifdef HAS_STOP_EPX_ON_NAK
  usb_hw_clear->nak_poll = USB_NAK_POLL_EPX_STOPPED_ON_NAK_BITS;
  #else
  epx_switch_request = false;
  epx_stop_deferred  = false;
  #endif
  ep->data_seq_retries = 0;
  if (ep == epx_closing) {
    epx_closing = NULL;
    rp2usb_reset_transfer(ep);
    ep->max_packet_size = 0; // released: hcd_edpt_close() left it to this
  } else if (ep->state == EPSTATE_EPX_ABORTING_PENDING) {
    ep->state = EPSTATE_PENDING;
  } else {
    rp2usb_reset_transfer(ep);
  }
  epx_start_next_pending();
}

  #ifndef HAS_STOP_EPX_ON_NAK
// RP2040: an interrupt-type EPX IN is not retried. A poll the device NAKs, or that times out or fails (CRC, bit stuff,
// overflow), ends with a zero-length buffer completion, its NAK_REC or error bit set (cleared when the poll started) or,
// for a timeout, epx_poll_timeout; a zero-length packet the device sends sets none of them. For a poll without data give
// back the PID and length it armed and send the endpoint back to wait for its next interval (or finish its abort).
// Returns whether the completion was one.
static bool __tusb_irq_path_func(epx_poll_nak)(uint8_t buf_id) {
  if (epx->transfer_type != TUSB_XFER_INTERRUPT || tu_edpt_dir(epx->ep_addr) != TUSB_DIR_IN ||
      !(epx_poll_timeout || (usb_hw->sie_status & EPX_POLL_NO_DATA_BITS))) {
    return false;
  }
  const uint32_t buf_ctrl = usbh_dpram->epx_buf_ctrl;
  if (((buf_id ? (buf_ctrl >> 16) : buf_ctrl) & USB_BUF_CTRL_LEN_MASK) != 0) {
    return false;
  }

  usb_hw_clear->sie_status = EPX_POLL_NO_DATA_BITS;
  epx_poll_timeout         = false;
  usbh_dpram->epx_buf_ctrl = 0;
  epx->next_pid ^= 1u;
  if (epx->state != EPSTATE_EPX_ABORTING_PENDING) {
    epx->remaining_len += epx_poll_len; // (the transfer queued behind an abort holds its own length already)
  }
  epx_switch_request = false;
  if (epx_is_aborting(epx)) {
    epx_abort_finish();
  } else {
    epx->state       = EPSTATE_WAITING;
    usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
    epx_start_next_pending();
  }
  return true;
}
  #endif

// A due interrupt poll would wait while a bulk transfer keeps EPX streaming data: on RP2040 each packet clears the
// round-robin's switch request, and on RP2350 the transfer gives way only at a NAK. So that transfer yields to the
// poll at a packet boundary (epx_yield()). Not EP0, whose stages are short, and not a poll itself.
static bool __tusb_irq_path_func(epx_poll_waiting)(void) {
  if (!epx_poll_due || epx->transfer_type == TUSB_XFER_INTERRUPT || tu_edpt_number(epx->ep_addr) == 0) {
    return false;
  }
  if (epx_pending_poll() != NULL) {
    return true;
  }
  epx_poll_due = false; // every due poll has started
  return false;
}

  #ifndef HAS_STOP_EPX_ON_NAK
enum {
  EPX_POST_SOF_SLOT_US = 10, // covers a retry the SIE sends right after SOF (~3 us) and its NAK
  LINE_IDLE_US         = 3,  // longer than the bus turnaround between a token and the device's reply
  EPX_STOP_SETTLE_US   = 2   // after STOP_TRANS: a token sent meanwhile is on the wire by then
};

// Whether the bus stays idle (J) for at least us microseconds
static bool __tusb_irq_path_func(bus_idle_for)(uint32_t us) {
  // J is D+ high at full speed (1), D- high at low speed (2); a low-speed device behind a hub runs at full speed here
  const uint32_t line_j = (dev_speed() == SIE_CTRL_SPEED_LOW) ? 2u : 1u;
  const uint32_t t0     = time_us_32();
  do {
    if (((usb_hw->sie_status & USB_SIE_STATUS_LINE_STATE_BITS) >> USB_SIE_STATUS_LINE_STATE_LSB) != line_j) {
      return false;
    }
  } while (time_us_32() - t0 <= us);
  return true;
}

// RP2040: the buffer completion that leads to a yield can be taken while the SIE still sends the packet's ACK, and
// STOP_TRANS on that handshake makes the device miss it and repeat the packet (a DATA_SEQ error the host recovers
// from, see #3964). So the yield stops EPX only once the bus has been idle for a turnaround, waiting at most one
// post-SOF slot; nothing is armed, so nothing new starts meanwhile.
static bool __tusb_irq_path_func(epx_yield_bus_settled)(void) {
  const uint32_t t0 = time_us_32();
  do {
    if (bus_idle_for(LINE_IDLE_US)) {
      return true;
    }
  } while (time_us_32() - t0 <= EPX_POST_SOF_SLOT_US);
  return false;
}
  #endif

// The bulk transfer on EPX completed a buffer and left it unarmed for a waiting poll. Once its other buffer is not
// armed either, the SIE waits for a buffer with nothing on the wire, so STOP_TRANS cannot cut a packet: the transfer is
// saved as pending, resumed in turn where it stopped, and the poll starts.
// Double-buffered, the other buffer stays the controller's until its own completion is taken, unless an earlier
// completion left it unarmed here (epx_yielding): both buffers can complete before the ISR takes the first, and a
// completed buffer reads not AVAILABLE, as one left unarmed does. Switching before its BUFF_STATUS is taken would apply
// it to the poll and lose its packet.
static void __tusb_irq_path_func(epx_yield)(uint8_t buf_id) {
  const uint32_t buf_ctrl = usbh_dpram->epx_buf_ctrl;
  const uint16_t other    = (uint16_t)(buf_id ? buf_ctrl : (buf_ctrl >> 16));
  if ((other & USB_BUF_CTRL_AVAIL) ||
      (!epx_yielding && (usbh_dpram->epx_ctrl & EP_CTRL_DOUBLE_BUFFERED_BITS))) {
    // its packet may be under way, or its completion not yet taken: yield when that completes (taken against this
    // transfer; a short or last packet ends it), even if the poll has gone meanwhile, as the SIE would wait for this
    // unarmed buffer
    epx_yielding = true;
    return;
  }
  #ifndef HAS_STOP_EPX_ON_NAK
  if (!epx_yield_bus_settled()) {
    return; // nothing is armed: the SOF round-robin switches the poll in, with its own bus check
  }
  #endif
  sie_stop_xfer();
  epx_save_context(epx); // no buffer is armed: nothing to give back
  if (!epx_held()) {
    hw_endpoint_t *poll_ep = epx_pending_poll();
    if (poll_ep != NULL) {
      epx_yielded = epx;
      epx_switch_ep(poll_ep);
      return;
    }
  }
  epx_start_next_pending(); // the poll has gone: the next pending transfer, this one last
}

// An interrupt transfer on EPX takes one packet per poll: a full packet with more to come leaves the endpoint waiting
// for its next interval (USB 2.0: one transaction per service interval). Single-buffered, its packet is never re-armed
// in buffer 1, which the controller would not read (RP2040-E4).
static void __tusb_irq_path_func(epx_poll_next_interval)(void) {
  sie_stop_xfer(); // nothing is armed: the SIE waits for a buffer
  usbh_dpram->epx_buf_ctrl = 0;
  epx->state               = EPSTATE_WAITING;
  usb_hw_set->inte         = USB_INTE_HOST_SOF_BITS;
  epx_start_next_pending();
}

//--------------------------------------------------------------------+
// Interrupt handlers
//--------------------------------------------------------------------+
static void __tusb_irq_path_func(xfer_complete_isr)(hw_endpoint_t *ep, xfer_result_t xfer_result, bool is_more) {
  if (ep == epx && xfer_result != XFER_RESULT_SUCCESS) {
    // bufctrl_prepare16() toggled next_pid for every buffer it armed; give back the toggles of packets that never
    // completed on the wire, or the next transfer on this endpoint goes out with the wrong DATA PID (IN: DATA_SEQ
    // error, OUT: the device drops it as a repeat). Not for EP0: hcd_edpt_xfer() sets its PID for every stage, and a
    // SETUP has no data buffers (an RX timeout has already cleared SEND_SETUP, so that bit cannot tell).
    // The RX timeout path does not wait for STOP_TRANS (nor would that settle it, see there), and the ISR must not
    // wait: the buffer control is read and then cleared at once by epx_drop_buffers(), so a retry finds no AVAILABLE
    // buffer; only a packet already on the wire at that moment can still complete, the race that path already accepts.
    if (tu_edpt_number(ep->ep_addr) != 0) {
      epx_undo_unsent(ep);
    }
    epx_drop_buffers();
  }

  // Mark transfer as done before we tell the tinyusb stack
  uint32_t xferred_len = ep->xferred_len;
  rp2usb_reset_transfer(ep);
  ep->data_seq_retries = 0;
  hcd_event_xfer_complete(ep->dev_addr, ep->ep_addr, xferred_len, xfer_result, true);

  // Carry more transfer on epx
  if (is_more) {
    epx_start_next_pending();
  }
}

static void __tusb_irq_path_func(handle_buf_status_isr)(void) {
  pico_trace("buf_status 0x%08lx\n", buf_status);
  enum {
    BUF_STATUS_EPX = 1u
  };

  // Check EPX first (bit 0).
  // Double-buffered: if both buffers completed at once, buf_status re-sets
  // immediately after clearing (datasheet Table 406). Process the second buffer too.
  while (usb_hw->buf_status & BUF_STATUS_EPX) {
    const uint8_t buf_id     = (usb_hw->buf_cpu_should_handle & BUF_STATUS_EPX) ? 1 : 0;
    usb_hw_clear->buf_status = 1u; // clear
    epx->data_seq_retries    = 0;  // a packet got through
    epx_rx_timeouts          = 0;

  #ifndef HAS_STOP_EPX_ON_NAK
    if (epx_poll_nak(buf_id)) {
      continue;
    }
  #endif

    if (epx_is_aborting(epx)) {
      // the aborted transfer's packet went through and keeps its toggle; finish once no other buffer is armed
      const uint32_t bc    = usbh_dpram->epx_buf_ctrl;
      const uint16_t other = (uint16_t)(buf_id ? bc : (bc >> 16));
      if (!(other & USB_BUF_CTRL_AVAIL)) {
        epx_abort_finish();
      }
      continue;
    }

    io_rw_32 *ep_reg  = &usbh_dpram->epx_ctrl;
    io_rw_32 *buf_reg = &usbh_dpram->epx_buf_ctrl;
  #ifndef HAS_STOP_EPX_ON_NAK
    // Any packet completion (mid-transfer or final) means data is flowing.
    // Clear switch request so the 2-SOF fallback only fires for NAK-retrying endpoints.
    epx_switch_request = false;
  #endif
    const bool is_poll = (epx->transfer_type == TUSB_XFER_INTERRUPT);
    const bool yield   = !is_poll && (epx_yielding || epx_poll_waiting());
    if (rp2usb_xfer_continue_ex(epx, ep_reg, buf_reg, buf_id, tu_edpt_dir(epx->ep_addr) == TUSB_DIR_IN,
                                !(is_poll || yield))) {
      xfer_complete_isr(epx, XFER_RESULT_SUCCESS, true);
    } else if (epx->state == EPSTATE_ACTIVE) { // more to come, not a status left behind by a transfer that has ended
      if (is_poll) {
        epx_poll_next_interval();
      } else if (yield) {
        epx_yield(buf_id);
      }
    }
  }

  // Only EPX is used: drop any other bit, which would keep the BUFF_STATUS interrupt raised
  const uint32_t others = usb_hw->buf_status & ~(uint32_t)BUF_STATUS_EPX;
  if (others != 0) {
    usb_hw_clear->buf_status = others;
  }
}

  #ifndef HAS_STOP_EPX_ON_NAK
// An EPX completion the ISR has not taken yet: switching EPX now would apply it to the transfer switched in
// ("buf_ctrl already available", or an early completion of that transfer with this one's length)
TU_ATTR_ALWAYS_INLINE static inline bool epx_completion_latched(void) {
  return (usb_hw->buf_status & 1u) || (usb_hw->sie_status & USB_SIE_STATUS_TRANS_COMPLETE_BITS);
}

// Whether the round-robin can stop EPX now. The SIE sends a retry that came due in the end-of-frame guard window right
// after SOF, where this ISR runs; STOP_TRANS on it while the device returns data makes the host ACK the packet without
// writing its buffer status (#3964). So wait out that slot, then sample the bus: idle (J) throughout means no
// transaction is under way. The retry can also complete within the wait (a zero-length packet takes a few us); the ISR
// takes its status next. Busy-waits only on frames the round-robin switches.
static bool __tusb_irq_path_func(epx_can_stop_after_sof)(void) {
  busy_wait_us_32(EPX_POST_SOF_SLOT_US);
  return bus_idle_for(LINE_IDLE_US) && !epx_completion_latched();
}

// RP2040: on SOF, switch EPX if another endpoint is pending.
// First SOF sets epx_switch_request. If a transfer completes before next SOF, the flag is
// cleared (data is flowing, no need to force-switch). Second SOF with flag still set means
// no data exchanged (endpoint NAK-retrying): STOP_TRANS is safe and we switch.
// This avoids stopping mid-data-transfer which corrupts double-buffered PID tracking.
// Return true while an endpoint is pending.
TU_ATTR_ALWAYS_INLINE static inline bool epx_sof_round_robin(void) {
  hw_endpoint_t *next_ep = epx_next_pending(epx);

  if (epx_is_aborting(epx)) {
    // an aborted transfer still on EPX (NAK-retrying): stop it the way a switch stops, then finish the abort
    if (epx_stop_deferred) {
      if (!epx_completion_latched()) {
        epx_abort_finish(); // the transaction seen at last SOF's stop has ended
      }
      return true;
    }
    if (!epx_can_stop_after_sof()) {
      return true;
    }
    sie_stop_xfer();
    if (!bus_idle_for(EPX_STOP_SETTLE_US)) {
      epx_stop_deferred = true;
      return true;
    }
    epx_abort_finish();
    return true;
  }

  if (epx_stop_deferred && epx->state == EPSTATE_ACTIVE) {
    // stopped last SOF with a transaction on the wire, which has ended since (its status, if any, taken by the ISR):
    // switch now, or restart the transfer if nothing is pending any more
    if (epx_completion_latched()) {
      return true;
    }
    epx_save_context(epx);
    epx_switch_ep(next_ep != NULL ? next_ep : epx);
    return true;
  }
  epx_stop_deferred = false; // that transfer ended meanwhile

  if (next_ep == NULL) {
    usb_hw->nak_poll   = USB_NAK_POLL_RESET;
    epx_switch_request = false;
    return false;
  }

  if (epx->state == EPSTATE_ACTIVE) {
    if (epx_switch_request) {
      // Second SOF with no transfer completion: endpoint is NAK-retrying, switch it out unless a packet is under way or
      // has just completed (its completion drops the request; otherwise the next SOF tries again)
      if (!epx_can_stop_after_sof()) {
        return true;
      }
      epx_switch_request = false;
      sie_stop_xfer();
      // A retry can start between the bus check and STOP_TRANS: the SIE then finishes it and writes its status into
      // the buffer control, which must not be the next transfer's by then. Leave the switch to the next SOF.
      // An IRQ preempting this one between the check above and STOP_TRANS gives a retry time to complete. With the SIE
      // stopped and settled the check is conclusive: keep this transfer, the ISR takes its status, the next SOF switches.
      if (!bus_idle_for(EPX_STOP_SETTLE_US) || epx_completion_latched()) {
        epx_stop_deferred = true;
        return true;
      }
      epx_save_context(epx);
      epx_switch_ep(next_ep);
    } else {
      epx_switch_request = true;
    }
  }
  return true;
}
  #endif

// HOST_SOF is enabled while an EPX endpoint is pending (RP2040), EPX is held after an RX timeout with transfers pending,
// or an interrupt endpoint has a transfer queued.
static void __tusb_irq_path_func(sof_service)(void) {
  const uint32_t frame      = usb_hw->sof_rd; // also acknowledges HOST_SOF
  bool           sof_needed = false;
  #ifndef HAS_STOP_EPX_ON_NAK
  sof_needed = epx_sof_round_robin(); // first, as close to SOF as possible: its STOP_TRANS relies on it
  #endif

  // interrupt endpoints polled on EPX: a queued transfer becomes pending when its interval is due
  for (size_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if (ep->state == EPSTATE_WAITING) {
      if (ep->last_poll == UINT16_MAX || ((frame - ep->last_poll) & 0x7ffu) >= ep->interval) {
        ep->last_poll = (uint16_t)(frame & 0x7ffu);
        ep->state     = EPSTATE_PENDING;
        epx_poll_due  = true;
  #ifdef HAS_STOP_EPX_ON_NAK
        usb_hw_set->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS; // a transfer on EPX gives way at its next NAK
  #endif // RP2040: the SOF round-robin switches a transfer NAK-retrying on EPX out for it
      }
      sof_needed = true; // also until a poll made pending here has run
    }
  }

  // EPX idle with transfers pending (left by a hold after an RX timeout, or a due interrupt poll): start them
  if (!epx_busy() && epx_next_pending(epx) != NULL) {
    if (epx_held()) {
      sof_needed = true;
    } else {
      epx_start_next_pending();
    }
  }

  if (!sof_needed) {
    usb_hw_clear->inte = USB_INTE_HOST_SOF_BITS;
  }
}

// the SDK registers this as the ISR, so it records its own span; under tuh_int_handler() the depth counter keeps one
static void __tusb_irq_path_func(hcd_rp2040_irq)(void) {
  if (TUH_SYSVIEW_ISR_TRACED) { TU_SYSVIEW_ISR_ENTER(); }
  uint32_t status = usb_hw->ints;
  // EPX when the status was read: a STALL or SETUP completion below can start the next pending transfer on it
  hw_endpoint_t *const status_epx = epx;

  if (status & USB_INTS_HOST_CONN_DIS_BITS) {
    uint8_t speed = dev_speed();
    if (speed == SIE_CTRL_SPEED_DISCONNECT) {
      hcd_event_device_remove(RHPORT_NATIVE, true);
    } else {
      if (speed == SIE_CTRL_SPEED_LOW) {
        usb_hw->sie_ctrl = SIE_CTRL_BASE | USB_SIE_CTRL_KEEP_ALIVE_EN_BITS;
      } else {
        usb_hw->sie_ctrl = SIE_CTRL_BASE | USB_SIE_CTRL_SOF_EN_BITS;
      }
      hcd_event_device_attach(RHPORT_NATIVE, true);
    }
    usb_hw_clear->sie_status = USB_SIE_STATUS_SPEED_BITS;
  }

  if (status & USB_INTS_STALL_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_STALL_REC_BITS;
    if (epx_is_aborting(epx)) {
      epx_abort_finish();
    } else {
      xfer_complete_isr(epx, XFER_RESULT_STALLED, true);
    }
  }

  if (status & USB_INTS_ERROR_RX_TIMEOUT_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_RX_TIMEOUT_BITS;

  #ifndef HAS_STOP_EPX_ON_NAK
    if (epx->transfer_type == TUSB_XFER_INTERRUPT && tu_edpt_dir(epx->ep_addr) == TUSB_DIR_IN && epx_busy()) {
      // an interrupt poll is not retried: it ends with a zero-length completion, a poll without data to epx_poll_nak(),
      // and is sent again at its next interval (a device that is gone until the hub reports it)
      epx_poll_timeout = true;
    } else
  #endif
    {
      // a timeout short of EPX_RX_TIMEOUT_MAX in a row: the SIE retries the transaction
      const uint32_t now = time_us_32();
      if (now - epx_rx_timeout_last > EPX_RX_TIMEOUT_GAP_US) {
        epx_rx_timeouts = 0; // not in a row with the previous one (see EPX_RX_TIMEOUT_GAP_US)
      }
      epx_rx_timeout_last = now;
      if (epx->state != EPSTATE_ACTIVE || ++epx_rx_timeouts >= EPX_RX_TIMEOUT_MAX) {
        const uint32_t sie_ctrl = (usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK) | USB_SIE_CTRL_STOP_TRANS_BITS;
        usb_hw->sie_ctrl        = sie_ctrl;
        // while (usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS) {}

        // Even if STOP_TRANS bit is clear, controller maybe in middle of retrying and may re-raise timeout once extra
        // time. Only handle if epx is active, don't carry more epx transfer since STOP_TRANS is raced and not safe.
        if (epx_busy()) {
          epx_hold         = true; // see epx_held()
          epx_hold_start   = time_us_32();
          usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
          if (epx_is_aborting(epx)) {
            epx_abort_finish(); // its queued transfer, if any, starts after the hold
          } else {
            xfer_complete_isr(epx, XFER_RESULT_FAILED, false);
          }
        }
      }
    }
  }

  if (status & USB_INTS_TRANS_COMPLETE_BITS) {
    // only applies for epx, interrupt endpoint does not seem to raise this
    usb_hw_clear->sie_status = USB_SIE_STATUS_TRANS_COMPLETE_BITS;
    if (usb_hw->sie_ctrl & USB_SIE_CTRL_SEND_SETUP_BITS) {
      uint32_t sie_ctrl = usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK;
      usb_hw->sie_ctrl  = sie_ctrl; // clear setup bit
      epx->xferred_len  = 8;
      xfer_complete_isr(epx, XFER_RESULT_SUCCESS, true);
    }
  }

  // A packet with the wrong DATA PID: the device repeated one whose ACK it missed, or the toggles are out of step
  // (USB 2.0 §8.6.4: a host must survive either). The controller discards it. Handled before BUFF_STATUS, which would
  // otherwise deliver that buffer as a success. Only EPX receives (the controller's interrupt endpoint poller is unused),
  // so the packet is EPX's.
  // §8.6.4's answer is to discard the packet and ask again with the same PID, which the class driver never sees: give
  // back the toggles of the buffers not received and restart the transfer. Only while no EPX buffer completion is
  // pending (dropping it would lose a good packet) and up to EPX_DATA_SEQ_RETRY_MAX times in a row (a device that
  // keeps repeating, or is gone); otherwise fail the transfer, as for an RX timeout, instead of panicking. A transfer
  // started by the branches above since the status was read did not receive it.
  if (status & USB_INTS_ERROR_DATA_SEQ_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_DATA_SEQ_ERROR_BITS;
    pico_trace("data seq error: epx %02x buf_ctrl 0x%08lx\n", epx->ep_addr, (unsigned long)usbh_dpram->epx_buf_ctrl);
    if (epx == status_epx && epx->state == EPSTATE_ACTIVE && tu_edpt_dir(epx->ep_addr) == TUSB_DIR_IN &&
        !(usb_hw->sie_ctrl & USB_SIE_CTRL_SEND_SETUP_BITS)) {
      sie_stop_xfer();
      // Switching state latched for this attempt must not reach the transfer started next on EPX (this one again, or
      // the next pending one, a queued SETUP included), which would be stopped and restarted as a data transfer
  #ifdef HAS_STOP_EPX_ON_NAK
      // a NAK stop latched with the error: the NAK branch below would act on it
      usb_hw_clear->nak_poll = USB_NAK_POLL_EPX_STOPPED_ON_NAK_BITS;
      status &= ~(uint32_t)USB_INTS_EPX_STOPPED_ON_NAK_BITS;
  #else
      // a switch request from the last SOF: the next SOF would stop the new transfer at once
      epx_switch_request = false;
  #endif
      if (epx->data_seq_retries < EPX_DATA_SEQ_RETRY_MAX && !(usb_hw->buf_status & 1u)) {
        epx->data_seq_retries++;
        epx_save_context(epx); // PIDs of the buffers not received given back, EPX pending again
        usb_hw_clear->sie_status = USB_SIE_STATUS_RX_TIMEOUT_BITS | USB_SIE_STATUS_TRANS_COMPLETE_BITS;
        epx_switch_ep(epx); // the same transfer again at once, as after a NAK stop
      } else {
        xfer_complete_isr(epx, XFER_RESULT_FAILED, true);
      }
    } else if (epx == status_epx && epx_is_aborting(epx)) {
  #ifdef HAS_STOP_EPX_ON_NAK
      // epx_abort_finish() clears a NAK stop latched with the error, and can restart this endpoint with the transfer
      // queued behind the abort: the NAK branch below must not save that one
      status &= ~(uint32_t)USB_INTS_EPX_STOPPED_ON_NAK_BITS;
  #endif
      epx_abort_finish(); // the transaction under way has ended, its packet discarded
    }
  }

  if (status & USB_INTS_BUFF_STATUS_BITS) {
    handle_buf_status_isr();
  }

  // EPX switching below runs after BUFF_STATUS, so a completion is credited to the EPX endpoint it belongs to
  #ifdef HAS_STOP_EPX_ON_NAK
  if (status & USB_INTS_EPX_STOPPED_ON_NAK_BITS) {
    usb_hw_clear->nak_poll = USB_NAK_POLL_EPX_STOPPED_ON_NAK_BITS;
    // a buffer that completed after the status was read is taken first: the context save below drops the buffers
    if (usb_hw->buf_status & 1u) {
      handle_buf_status_isr();
    }
    // Only for the transfer that stopped: one retired above (RX timeout) is never restarted or re-queued, and one
    // started above since (after a completion) is running.
    if (epx == status_epx && epx_is_aborting(epx)) {
      epx_abort_finish(); // stopped on a NAK: nothing is on the wire
    } else if (epx == status_epx && epx->state == EPSTATE_ACTIVE) {
      hw_endpoint_t *next_ep = epx_next_pending(epx);
      if (epx->transfer_type == TUSB_XFER_INTERRUPT) {
        // the device NAKed the poll: it waits for its next interval, EPX goes on to the next pending transfer
        epx_save_context(epx);
        epx->state       = EPSTATE_WAITING;
        usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
        if (next_ep != NULL) {
          epx_switch_ep(next_ep);
        }
      } else {
        if (next_ep == NULL) {
          usb_hw_clear->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS;
          next_ep                = epx;
        }
        // Resuming takes a context save and a fresh start, as a switch does: START_TRANS resumes on buffer 0, not on
        // the buffer the stop left next, so a double-buffered transfer would expect the other DATA PID (IN: DATA_SEQ,
        // the device's packet lost) or send its buffers out of order (OUT)
        epx_save_context(epx);
        epx_switch_ep(next_ep);
      }
    }
  }
  #endif

  if (status & USB_INTS_HOST_SOF_BITS) {
    sof_service();
  }

  if (TUH_SYSVIEW_ISR_TRACED) { TU_SYSVIEW_ISR_EXIT(); }
}

void __tusb_irq_path_func(hcd_int_handler)(uint8_t rhport, bool in_isr) {
  (void)rhport;
  (void)in_isr;
  hcd_rp2040_irq();
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;
  (void)rh_init;
  pico_trace("hcd_init %d\n", rhport);
  assert(rhport == 0);

  // Reset any previous state
  rp2usb_init();

  // Force VBUS detect to always present, for now we assume vbus is always provided (without using VBUS En)
  usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;

  // Remove shared irq if it was previously added so as not to fill up shared irq slots
  irq_remove_handler(USBCTRL_IRQ, hcd_rp2040_irq);
  irq_add_shared_handler(USBCTRL_IRQ, hcd_rp2040_irq, PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);

  // clear epx and interrupt eps
  memset(&ep_pool, 0, sizeof(ep_pool));
  epx         = &ep_pool[0];
  epx_closing = NULL;
  hcd_running = true;

  // Enable in host mode with SOF / Keep alive on
  usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS | USB_MAIN_CTRL_HOST_NDEVICE_BITS;
  usb_hw->sie_ctrl  = SIE_CTRL_BASE;
  // HOST_RESUME stays off: no host suspend support, and nothing would clear SIE_STATUS.RESUME
  usb_hw->inte      = USB_INTE_BUFF_STATUS_BITS | USB_INTE_HOST_CONN_DIS_BITS | USB_INTE_STALL_BITS |
                 USB_INTE_TRANS_COMPLETE_BITS | USB_INTE_ERROR_RX_TIMEOUT_BITS | USB_INTE_ERROR_DATA_SEQ_BITS;

  #ifdef HAS_STOP_EPX_ON_NAK
  usb_hw_set->inte = USB_INTE_EPX_STOPPED_ON_NAK_BITS;
  #endif

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void)rhport;
  irq_remove_handler(USBCTRL_IRQ, hcd_rp2040_irq);
  reset_block(RESETS_RESET_USBCTRL_BITS);
  unreset_block_wait(RESETS_RESET_USBCTRL_BITS);

  // The controller is reset, so no transfer is under way: forget every endpoint while the lock still exists, and
  // let the hcd_device_close() calls tuh_deinit() makes next return at once
  rp2usb_critical_enter();
  hcd_running = false;
  memset(&ep_pool, 0, sizeof(ep_pool));
  epx         = &ep_pool[0];
  epx_closing = NULL;
  rp2usb_critical_exit();

  // Release allocated resources
  rp2usb_deinit();

  return true;
}

void hcd_port_reset(uint8_t rhport) {
  (void)rhport;
  // The controller's line-state machine resets the bus itself on attach, so first enumeration needs nothing here.
  // Measured on RP2040 and RP2350 via SIE_STATUS.LINE_STATE: 100 ms after attach (or VBUS_DETECT) it drives SE0 for
  // 50 ms, then sets SPEED and raises HOST_CONN_DIS. SIE_CTRL.RESET_BUS self-clears within ~1 us, no usable reset.
  // TODO a reset of a device already attached (enumeration retry, tuh_rhport_reset_bus()) is not done: clearing then
  // setting USB_PWR.VBUS_DETECT re-runs the sequence (pico-feedback #387), but drops the connection for ~150 ms,
  // longer than usbh waits after hcd_port_reset_end().
}

void hcd_port_reset_end(uint8_t rhport) {
  (void)rhport;
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void)rhport;
  return usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void)rhport;
  switch (dev_speed()) {
    case SIE_CTRL_SPEED_LOW:
      return TUSB_SPEED_LOW;
    case SIE_CTRL_SPEED_FULL:
      return TUSB_SPEED_FULL;
    default:
      return TUSB_SPEED_INVALID;
  }
}

// Release an endpoint; caller holds the critical section and starts the next pending EPX transfer
static void edpt_close(hw_endpoint_t *ep) {
  if (ep == epx_closing) {
    epx_closing = NULL; // closed with its device: released at once
  }
  epx_retire(ep);
  ep->max_packet_size = 0; // mark as unused
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void)rhport;

  if (!hcd_running) {
    return; // after hcd_deinit(): nothing open, and rp2usb_lock is gone
  }

  rp2usb_critical_enter();

  for (size_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if (ep->dev_addr == dev_addr && ep->max_packet_size > 0) {
      edpt_close(ep);
    }
  }
  epx_start_next_pending();

  rp2usb_critical_exit();
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void)rhport;
  return usb_hw->sof_rd;
}

void hcd_int_enable(uint8_t rhport) {
  (void)rhport;
  irq_set_enabled(USBCTRL_IRQ, true);
}

void hcd_int_disable(uint8_t rhport) {
  (void)rhport;
  // todo we should check this is disabling from the correct core; note currently this is never called
  irq_set_enabled(USBCTRL_IRQ, false);
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t *ep_desc) {
  (void)rhport;
  // not after hcd_deinit() (e.g. from tuh_umount_cb()): an endpoint opened now would lead a later transfer to the
  // freed rp2usb_lock
  TU_VERIFY(hcd_running);
  pico_trace("hcd_edpt_open dev_addr %d, ep_addr %d\n", dev_addr, ep_desc->bEndpointAddress);
  hw_endpoint_t *ep;
  if (dev_addr == 0) {
    ep = &ep_pool[0];
  } else {
    ep = edpt_alloc();
  }
  TU_ASSERT(ep);

  const uint8_t  ep_addr         = ep_desc->bEndpointAddress;
  const uint16_t max_packet_size = tu_edpt_packet_size(ep_desc);

  ep->max_packet_size = max_packet_size;
  ep->ep_addr         = ep_addr;
  ep->dev_addr        = dev_addr;
  ep->transfer_type   = ep_desc->bmAttributes.xfer;
  ep->need_pre        = need_pre(dev_addr);
  ep->next_pid        = 0u;
  ep->dpram_buf       = usbh_dpram->epx_data;          // every endpoint uses EPX
  ep->interval        = tu_max8(ep_desc->bInterval, 1); // interrupt endpoint: full/low speed, frames
  ep->last_poll       = UINT16_MAX;                     // first poll due at once

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void)rhport;
  TU_VERIFY(tu_edpt_number(ep_addr) != 0); // EP0 is closed with the device
  hw_endpoint_t *ep = edpt_find(daddr, ep_addr);
  TU_VERIFY(ep != NULL);

  rp2usb_critical_enter();
  if (ep == epx && ep->state == EPSTATE_ACTIVE) {
    epx_abort_start(); // tuh_edpt_close() aborts first, so normally under way already
  }
  if (ep == epx && epx_is_aborting(ep)) {
    // A transaction may be under way: STOP_TRANS would lose its packet, and the status it writes would land in the
    // next transfer's buffers. epx_abort_finish() releases the endpoint once it has ended; a transfer queued behind
    // the abort is dropped with it.
    ep->state   = EPSTATE_EPX_ABORTING;
    epx_closing = ep;
  } else {
    edpt_close(ep);
    epx_start_next_pending();
  }
  rp2usb_critical_exit();

  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  hw_endpoint_t *ep = edpt_find(dev_addr, ep_addr);
  TU_VERIFY(ep != NULL);

  bool ret = true;

  rp2usb_critical_enter();
  if (ep == epx && ep->state == EPSTATE_ACTIVE && tu_edpt_number(ep_addr) != 0) {
    epx_abort_start(); // finished from the ISR once the transaction under way has ended (EP0 sets its PIDs per stage)
  } else if (ep->state == EPSTATE_EPX_ABORTING_PENDING) {
    ep->state = EPSTATE_EPX_ABORTING; // the transfer queued behind the abort never started
  } else if (ep->state == EPSTATE_IDLE || ep->state == EPSTATE_EPX_ABORTING) {
    ret = false; // nothing to abort: already completed (its event is queued) or already aborted
  } else {
    epx_retire(ep);
    epx_start_next_pending();
  }
  rp2usb_critical_exit();

  return ret;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void)rhport;

  hw_endpoint_t *ep = edpt_find(dev_addr, ep_addr);
  TU_ASSERT(ep);

  if (ep->transfer_type == TUSB_XFER_INTERRUPT) {
    // polled on EPX: sof_service() makes it pending when its interval is due
    rp2usb_critical_enter();
    ep->user_buf      = buffer;
    ep->remaining_len = buflen;
    ep->xferred_len   = 0;
    if (epx_is_aborting(ep)) {
      ep->state = EPSTATE_EPX_ABORTING_PENDING; // pending once the abort has finished
    } else {
      ep->state        = EPSTATE_WAITING;
      usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
    }
    rp2usb_critical_exit();
    return true;
  } else {
    // Control transfer data and status stages always start with DATA1, regardless of
    // whether the direction changed since the previous stage. SET_REPORT (and any other
    // host-to-device class request with an OUT data stage) keeps the same direction
    // across SETUP -> DATA, so we cannot key off "direction changed" -- we must reset
    // next_pid every time hcd_edpt_xfer is invoked on ep 0. Without this, the data stage
    // of SET_REPORT goes out as DATA0 because ep->next_pid is still 0 from hcd_edpt_open(),
    // which strict devices treat as a protocol violation and disconnect.
    if (tu_edpt_number(ep_addr) == 0) {
      ep->ep_addr  = ep_addr;
      ep->next_pid = 1;
    }

    // If EPX is busy with another transfer, held, or idle with transfers pending, mark as pending
    rp2usb_critical_enter();
    if (epx_is_aborting(ep)) {
      // this endpoint's aborted transfer is still on EPX: epx_abort_finish() makes this one pending
      ep->user_buf      = buffer;
      ep->remaining_len = buflen;
      ep->xferred_len   = 0;
      ep->state         = EPSTATE_EPX_ABORTING_PENDING;
    } else if (epx_must_queue()) {
      ep->user_buf      = buffer;
      ep->remaining_len = buflen;
      ep->state         = EPSTATE_PENDING;

  #ifdef HAS_STOP_EPX_ON_NAK
      usb_hw_set->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS;
  #else
      // Only enable SOF round-robin for non-control endpoints
      usb_hw->nak_poll = (300 << USB_NAK_POLL_DELAY_FS_LSB) | (300 << USB_NAK_POLL_DELAY_LS_LSB);
      usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
  #endif
      epx_queued();
    } else {
      io_rw_32 *ep_reg  = &usbh_dpram->epx_ctrl;
      io_rw_32 *buf_reg = &usbh_dpram->epx_buf_ctrl;

      epx = ep;
      epx_xfer_started();

      epx_ctrl_prepare(ep);
      rp2usb_xfer_start(ep, ep_reg, buf_reg, buffer, NULL, buflen); // prepare bufctrl
      usb_hw->dev_addr_ctrl = (uint32_t)(ep->dev_addr | (tu_edpt_number(ep->ep_addr) << USB_ADDR_ENDP_ENDPOINT_LSB));
      sie_start_xfer(false, tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN, ep->need_pre);
    }
    rp2usb_critical_exit();
  }

  return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
  (void)rhport;

  hw_endpoint_t *ep = edpt_find(dev_addr, 0x00);
  TU_ASSERT(ep);

  rp2usb_critical_enter();

  // Copy data into setup packet buffer (usbh only schedules one setup at a time)
  for (uint8_t i = 0; i < 8; i++) {
    usbh_dpram->setup_packet[i] = setup_packet[i];
  }

  ep->ep_addr       = 0; // setup is OUT
  ep->remaining_len = 8;
  ep->xferred_len   = 0;

  // If EPX is busy, held, or idle with transfers pending, mark as pending setup (DPRAM already has the packet)
  if (epx_must_queue()) {
    ep->state = EPSTATE_PENDING_SETUP;
  #ifdef HAS_STOP_EPX_ON_NAK
    usb_hw_set->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS;
  #else
    usb_hw->nak_poll = (300 << USB_NAK_POLL_DELAY_FS_LSB) | (300 << USB_NAK_POLL_DELAY_LS_LSB);
    usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
  #endif
    epx_queued();
  } else {
    epx       = ep;
    ep->state = EPSTATE_ACTIVE;
    epx_xfer_started();

    usb_hw->dev_addr_ctrl = ep->dev_addr;
    sie_start_xfer(true, tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN, ep->need_pre);
  }

  rp2usb_critical_exit();
  return true;
}

// After the device's CLEAR_FEATURE(ENDPOINT_HALT), whose control transfer the class sends: the endpoint's data toggle
// restarts at DATA0 (USB 2.0 §9.4.5)
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  hw_endpoint_t *ep = edpt_find(dev_addr, ep_addr);
  TU_VERIFY(ep != NULL);
  ep->next_pid = 0;
  return true;
}

#endif
