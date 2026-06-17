/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji KITAYAMA
 * Copyright (c) 2024, Brent Kowal (Analog Devices, Inc)
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

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_MUSB)

#define MUSB_DEBUG 2
#define MUSB_REGS(rhport)   ((musb_regs_t*) MUSB_BASES[rhport])

#include "musb_type.h"
#include "device/dcd.h"

// Following symbols must be defined by port header
// - musb_dcd_int_enable/disable/clear/get_enable
// - musb_dcd_int_handler_enter/exit
#if defined(TUP_USBIP_MUSB_TI)
  #include "musb_ti.h"
#elif defined(TUP_USBIP_MUSB_ADI)
  #include "musb_max32.h"
#else
  #error "Unsupported MCU"
#endif

/*------------------------------------------------------------------
 * MACRO TYPEDEF CONSTANT ENUM DECLARATION
 *------------------------------------------------------------------*/

typedef union {
  volatile uint8_t   u8;
  volatile uint16_t  u16;
  volatile uint32_t  u32;
} hw_fifo_t;

typedef struct {
  union {
    uint8_t  *buf;      /* the start address of a transfer data buffer */
    tu_fifo_t *fifo;
  };
  uint16_t  length;    /* the number of bytes in the buffer */
  uint16_t  remaining; /* the number of bytes remaining in the buffer */
  bool      armed;     /* true while a transfer is posted */
  bool      use_fifo;  /* true: buf is tu_fifo_t*; false: buf is plain byte pointer. */
} pipe_state_t;

// Pipe array layout (N = TUP_DCD_ENDPOINT_MAX). EP0 has its own scalars in
// dcd_data_t and does not occupy a pipe slot.
// One-direction-only IPs (CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY=1):
//   [0..N-2] : EP1..N-1 (single slot per endpoint)
// Bidirectional-capable IPs:
//   [0..N-2     ] : EP1..N-1 OUT
//   [N-1..2*N-3 ] : EP1..N-1 IN
#if CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY
  #define MUSB_PIPE_COUNT (TUP_DCD_ENDPOINT_MAX - 1u)
#else
  #define MUSB_PIPE_COUNT (2u * (TUP_DCD_ENDPOINT_MAX - 1u))
#endif

enum {
  PIPE0_STATE_IDLE = 0,         // no active control transfer
  PIPE0_STATE_DATA_IN,          // DATA IN stage
  PIPE0_STATE_DATA_OUT,         // DATA OUT stage
  PIPE0_STATE_STATUS_IN,        // STATUS IN — device sends IN-ZLP; awaits send-ACK IRQ
  PIPE0_STATE_STATUS_OUT,       // post-DATAEND, neither edpt0_xfer(STATUS OUT) nor confirmation IRQ has happened yet
  PIPE0_STATE_STATUS_OUT_PENDING_XFER, // edpt0_xfer(STATUS OUT) called first; the confirmation IRQ fires xfer_complete
  PIPE0_STATE_STATUS_OUT_PENDING_IRQ,  // confirmation IRQ seen (or synthesized) first; edpt0_xfer(STATUS OUT) fires xfer_complete
};

// EP0 control-transfer state (own scalars, not a pipe[] slot).
typedef struct {
  uint8_t  *buf;            // DATA OUT drain target (only valid while EP0 is in DATA OUT stage)
  uint16_t xact_len;        // DATA IN chunk length armed via edpt0_xfer; reported in its xfer_complete (OUT reports count0)
  uint16_t remain_wlength;  // bytes remaining in the control transfer's DATA stage
  uint8_t  state;
  uint8_t  pending_addr;    // new USB address latched by dcd_set_address; applied when STATUS IN completes
  bool     rxrdy_consumed;  // RxPktRdy left set in hw for an already-consumed packet (NAK flow control);
                            // RXRDY events are stale while set. Cleared when RXRDYC is written.
  bool     deferred_setup_valid;
  uint32_t deferred_setup[2];  // raw SETUP words, replayed via pipe0_start_setup
} pipe0_state_t;

typedef struct {
  pipe0_state_t pipe0;
  pipe_state_t pipe[MUSB_PIPE_COUNT];
} dcd_data_t;

static dcd_data_t _dcd;

// Read the 8-byte SETUP packet (2 words) from the EP0 FIFO into setup[]. Does not ack RxPktRdy.
static bool pipe0_read_setup(musb_regs_t* musb_regs, musb_ep_csr_t* ep_csr, uint32_t setup[2]) {
  TU_ASSERT(sizeof(tusb_control_request_t) == ep_csr->count0);
  setup[0] = musb_regs->fifo[0];
  setup[1] = musb_regs->fifo[0];
  return true;
}

static void pipe0_start_setup(uint8_t rhport, musb_ep_csr_t* ep_csr,
                              const uint32_t setup[2], bool is_isr) {
  tusb_control_request_t const* req = (tusb_control_request_t const*) setup;
  pipe0_state_t* pipe0 = &_dcd.pipe0;
  pipe0->remain_wlength = req->wLength;

  if (req->wLength == 0) {
    // Leave RXRDY set; edpt0_xfer(STATUS IN) acks it together with DATAEND.
    pipe0->state = PIPE0_STATE_STATUS_IN;
    pipe0->rxrdy_consumed = true;
  } else {
    if (req->bmRequestType & TUSB_DIR_IN_MASK) {
      pipe0->state = PIPE0_STATE_DATA_IN;
      // On a deferred replay the packet's RXRDY stays parked until the edpt0_xfer(DATA IN) arm
      // acks it — a stale latched EP0 IRQ in between is gated by rxrdy_consumed.
      if (!pipe0->rxrdy_consumed) {
        ep_csr->csr0l = MUSB_CSRL0_RXRDYC;
      }
    } else {
      // If OUT (rx) direction, let edpt0_xfer() clear RXRDY when it's ready to receive data.
      // Deliberate deviation from the databook's canonical flow (ack right after unload),
      // used as NAK flow control until usbd arms the drain buffer.
      pipe0->state = PIPE0_STATE_DATA_OUT;
      pipe0->rxrdy_consumed = true;
    }
  }

  dcd_event_setup_received(rhport, (const uint8_t *) setup, is_isr);
}

// Replay a previously deferred SETUP, if any.
static void pipe0_try_deferred_setup(uint8_t rhport, musb_ep_csr_t* ep_csr, bool is_isr) {
  pipe0_state_t* pipe0 = &_dcd.pipe0;
  if (!pipe0->deferred_setup_valid) {
    return;
  }

  pipe0->deferred_setup_valid = false;
  pipe0_start_setup(rhport, ep_csr, pipe0->deferred_setup, is_isr);
}

// Last DATA packet: wLength satisfied, or a short packet (incl. ZLP) ends the stage.
TU_ATTR_ALWAYS_INLINE static inline bool pipe0_data_stage_done(uint16_t xfer_len) {
  return _dcd.pipe0.remain_wlength == 0 || xfer_len < CFG_TUD_ENDPOINT0_SIZE;
}

// EP0 must not call this — it has its own scalars in dcd_data_t.
TU_ATTR_ALWAYS_INLINE static inline pipe_state_t* pipe_get(uint8_t epnum, tusb_dir_t epdir) {
  size_t idx = epnum - 1u;
#if CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY
  (void) epdir;
#else
  if (epdir == TUSB_DIR_IN) {
    idx += TUP_DCD_ENDPOINT_MAX - 1u;
  }
#endif
  return &_dcd.pipe[idx];
}

//--------------------------------------------------------------------
// HW FIFO Helper
// Note: Index register is already set by caller
//--------------------------------------------------------------------

#if MUSB_CFG_DYNAMIC_FIFO

// musb is configured to use dynamic FIFO sizing.
// FF Size is encodded: 1 << (fifo_size[3:0] + 3) = 8 << fifo_size[3:0]
// FF Address is 8*ff_addr[12:0]
// First 64 bytes are reserved for EP0
static uint32_t alloced_fifo_bytes;

// ffsize is log2(mps) - 3 (round up)
TU_ATTR_ALWAYS_INLINE static inline uint8_t hwfifo_byte2size(uint16_t nbytes) {
  uint8_t ffsize = 28 - tu_min8(28, __builtin_clz(nbytes));
  if ((8u << ffsize) < nbytes) {
    ++ffsize;
  }
  return ffsize;
}

TU_ATTR_ALWAYS_INLINE static inline void hwfifo_reset(musb_regs_t* musb, unsigned epnum, unsigned is_rx) {
  (void) epnum;
  musb->fifo_size[is_rx] = 0;
  musb->fifo_addr[is_rx] = 0;
}

TU_ATTR_ALWAYS_INLINE static inline bool hwfifo_config(musb_regs_t* musb, unsigned epnum, unsigned is_rx, unsigned mps,
                                                       bool double_packet) {
  uint8_t ffsize = hwfifo_byte2size(mps);
  mps = 8 << ffsize; // round up to the next power of 2

  if (double_packet) {
    ffsize |= MUSB_FIFOSZ_DOUBLE_PACKET;
    mps <<= 1;
  }

  TU_ASSERT(alloced_fifo_bytes + mps <= MUSB_CFG_DYNAMIC_FIFO_SIZE);
  musb->fifo_addr[is_rx] = alloced_fifo_bytes / 8;
  musb->fifo_size[is_rx] = ffsize;

  volatile uint16_t* dp_disable = is_rx ? &musb->rx_doulbe_packet_disable : &musb->tx_double_packet_disable;
  if (double_packet) {
    *dp_disable &= ~(1u << epnum);
  } else {
    *dp_disable |= (1u << epnum);
  }

  alloced_fifo_bytes += mps;
  return true;
}

#else

TU_ATTR_ALWAYS_INLINE static inline void hwfifo_reset(musb_regs_t* musb, unsigned epnum, unsigned is_rx) {
  (void) musb; (void) epnum; (void) is_rx;
  // nothing to do for static FIFO
}

TU_ATTR_ALWAYS_INLINE static inline bool hwfifo_config(musb_regs_t* musb, unsigned epnum, unsigned is_rx, unsigned mps,
                                                       bool double_packet) {
  (void) mps;

  #if defined(TUP_USBIP_MUSB_ADI)
  // AnalogDevice FIFO sizes: EP1..7 = 512 B, EP8..9 = 2048 B, EP10..11 = 4096 B.
  // DPB requires FIFO >= 2 * MPS. For HS bulk (MPS=512) only EP >= 8 qualifies.
  // Force single-buffered on EP < 8 even if the caller requested DPB.
  if (epnum < 8 && (musb->power & MUSB_POWER_HSMODE)) {
    double_packet = false;
  }
  volatile uint8_t* csrh = &musb->indexed_csr.maxp_csr[is_rx].csrh;
  if (double_packet) {
    *csrh &= ~MUSB_CSRH_DISABLE_DOUBLE_PACKET;
  } else {
    *csrh |= MUSB_CSRH_DISABLE_DOUBLE_PACKET;
  }
  #else
  volatile uint16_t* dp_disable = is_rx ? &musb->rx_doulbe_packet_disable : &musb->tx_double_packet_disable;
  if (double_packet) {
    *dp_disable &= ~(1u << epnum);
  } else {
    *dp_disable |= (1u << epnum);
  }
  #endif

  return true;
}

#endif

// Flush FIFO and clear data toggle
TU_ATTR_ALWAYS_INLINE static inline void hwfifo_flush(musb_regs_t* musb, unsigned epnum, unsigned is_rx, bool clear_dtog) {
  (void) epnum;
  const uint8_t csrl_dtog = clear_dtog ? MUSB_CSRL_CLEAR_DATA_TOGGLE(is_rx) : 0;
  musb_ep_maxp_csr_t* maxp_csr = &musb->indexed_csr.maxp_csr[is_rx];
  // may need to flush twice for double packet
  for (unsigned i=0; i<2; i++) {
    if (maxp_csr->csrl & MUSB_CSRL_PACKET_READY(is_rx)) {
      maxp_csr->csrl = MUSB_CSRL_FLUSH_FIFO(is_rx) | csrl_dtog;
    }
  }
}

// write to txfifo using pipe_state_t info
static void pipe_write(musb_regs_t* musb_regs, pipe_state_t* pipe, uint8_t epnum) {
  musb_ep_csr_t* ep_csr = &musb_regs->indexed_csr;
  const uint16_t mps = ep_csr->tx_maxp & MUSB_TXMAXP_PACKET_SIZE_M;
  const uint16_t xact_len = tu_min16(mps, pipe->remaining);
  volatile void *hwfifo = &musb_regs->fifo[epnum];
  if (xact_len) {
    if (pipe->use_fifo) {
      tu_hwfifo_write_from_fifo(hwfifo, pipe->fifo, xact_len, NULL);
    } else {
      tu_hwfifo_write(hwfifo, pipe->buf, xact_len, NULL);
      pipe->buf += xact_len;
    }
    pipe->remaining -= xact_len;
  }
  ep_csr->tx_csrl = MUSB_TXCSRL1_TXRDY;
}

// Called from the TX interrupt. If the last queued packet finished the transfer,
// signal completion; otherwise queue the next packet.
static void process_epin_isr(uint8_t rhport, musb_regs_t *musb_regs, uint8_t epnum) {
  musb_ep_csr_t* ep_csr = get_ep_csr(musb_regs, epnum);
  const uint_fast8_t csrl = ep_csr->tx_csrl;
  if (csrl & MUSB_TXCSRL1_STALLED) {
    ep_csr->tx_csrl &= ~(MUSB_TXCSRL1_STALLED | MUSB_TXCSRL1_UNDRN);
    return; // sent STALL, do nothing
  }

  pipe_state_t* pipe = pipe_get(epnum, TUSB_DIR_IN);
  if (pipe->remaining > 0) {
    pipe_write(musb_regs, pipe, epnum);
  } else {
    // All bytes have been loaded into the FIFO. With double-packet buffering a
    // second packet may still be waiting in the FIFO when this IRQ fires (the
    // hardware signals TXRDY clear as soon as a slot frees, not when the wire
    // transfer finishes). Defer completion until FIFONE == 0 so we don't emit
    // a duplicate xfer_complete before the final packet has been sent.
    if (csrl & MUSB_TXCSRL1_FIFONE) {
      return;
    }
    const uint16_t xferred_len = pipe->length;
    pipe->buf = NULL;
    pipe->armed = false;
    dcd_event_xfer_complete(rhport, tu_edpt_addr(epnum, TUSB_DIR_IN), xferred_len, XFER_RESULT_SUCCESS, true);
  }
}

// Drain one packet from the Rx FIFO into pipe->buf/fifo, update pipe state, and
// release the FIFO slot by clearing RXRDY. return true if short packet
static bool pipe_read(musb_regs_t* musb_regs, pipe_state_t* pipe, uint8_t epnum) {
  musb_ep_csr_t* ep_csr = &musb_regs->indexed_csr; // index already set in process_epout_isr()
  const uint16_t mps = ep_csr->rx_maxp & MUSB_RXMAXP_PACKET_SIZE_M;
  const uint16_t rx_count = ep_csr->rx_count;
  const uint16_t xact_len = tu_min16(tu_min16(pipe->remaining, mps), rx_count);
  volatile void *hwfifo = &musb_regs->fifo[epnum];
  if (xact_len) {
    if (pipe->use_fifo) {
      tu_hwfifo_read_to_fifo(hwfifo, pipe->fifo, xact_len, NULL);
    } else {
      tu_hwfifo_read(hwfifo, pipe->buf, xact_len, NULL);
      pipe->buf += xact_len;
    }
    pipe->remaining -= xact_len;
  }
  ep_csr->rx_csrl = 0; /* Clear RXRDY - release this FIFO slot */

  return (xact_len < mps);
}

static void process_epout_isr(uint8_t rhport, musb_regs_t *musb_regs, uint8_t epnum, bool is_isr) {
  musb_ep_csr_t* ep_csr = get_ep_csr(musb_regs, epnum);
  if (ep_csr->rx_csrl & MUSB_RXCSRL1_STALLED) {
    ep_csr->rx_csrl &= ~(MUSB_RXCSRL1_STALLED | MUSB_RXCSRL1_OVER);
    return; // sent STALL, do nothing
  }

  // Fail gracefully. Spurious interrupt.
  if (!(ep_csr->rx_csrl & MUSB_RXCSRL1_RXRDY)) {
    return;
  }

  pipe_state_t *pipe = pipe_get(epnum, TUSB_DIR_OUT);
  if (!pipe->armed) {
    // Packet is already ACK'd by hardware and sitting in the Rx FIFO, but no transfer is
    // posted. Do NOT flush (per MUSB spec §3.3.11 FlushFIFO) - that would silently drop
    // acknowledged data. Mask this endpoint's Rx interrupt so the ISR stops re-firing;
    // the FIFO stays occupied so hardware NAKs further OUT tokens (natural backpressure).
    // The next dcd_edpt_xfer() on this endpoint will drain the staged packet.
    musb_regs->intr_rxen &= (uint16_t) ~TU_BIT(epnum);
    return;
  }

  const bool is_short = pipe_read(musb_regs, pipe, epnum);

  // Transfer completes on a short packet or when the rx buffer is filled.
  if (is_short || pipe->remaining == 0) {
    const uint16_t xferred_len = pipe->length - pipe->remaining;
    pipe->buf = NULL;
    pipe->armed = false;
    dcd_event_xfer_complete(rhport, epnum, xferred_len, XFER_RESULT_SUCCESS, is_isr);
  }
}

static bool edpt_n_xfer(uint8_t rhport, uint8_t ep_addr, void *buffer, uint16_t total_bytes, bool use_fifo, bool is_isr) {
  const uint8_t epnum  = tu_edpt_number(ep_addr);
  const tusb_dir_t dir_in = tu_edpt_dir(ep_addr);

  pipe_state_t *pipe = pipe_get(epnum, dir_in);
  if (use_fifo) {
    pipe->fifo = (tu_fifo_t *)buffer;
  } else {
    pipe->buf = (uint8_t *)buffer;
  }
  pipe->length    = total_bytes;
  pipe->remaining = total_bytes;
  pipe->use_fifo  = use_fifo;
  pipe->armed     = true;

  musb_regs_t   *musb_regs = MUSB_REGS(rhport);
  musb_ep_csr_t *ep_csr    = get_ep_csr(musb_regs, epnum);

  if (dir_in) {
    pipe_write(musb_regs, pipe, epnum);
  } else {
    // Re-enable Rx interrupt (may have been masked by the no-buffer path in process_epout_isr)
    musb_regs->intr_rxen |= (uint16_t)TU_BIT(epnum);

    // Drain any packet staged in the Rx FIFO from a prior no-buffer interrupt.
    // process_epout_isr() fires dcd_event_xfer_complete() itself if the drain completes.
    if (ep_csr->rx_csrl & MUSB_RXCSRL1_RXRDY) {
      process_epout_isr(rhport, musb_regs, epnum, is_isr);
    }
  }
  return true;
}

static bool edpt0_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  TU_ASSERT(total_bytes <= CFG_TUD_ENDPOINT0_SIZE); /* EP0 only supports 1 packet per dcd_edpt_xfer()*/
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  musb_ep_csr_t* ep_csr = get_ep_csr(musb_regs, 0);
  pipe0_state_t* pipe0 = &_dcd.pipe0;
  const unsigned dir_in = tu_edpt_dir(ep_addr);

  switch (pipe0->state) {
    // DATA stage exits on its last packet, so state matches the call direction here.
    case PIPE0_STATE_DATA_IN:
      TU_ASSERT(dir_in);
      pipe0->xact_len = total_bytes;
      if (pipe0->rxrdy_consumed) { // replayed SETUP: ack its parked RXRDY before loading the FIFO
        ep_csr->csr0l = MUSB_CSRL0_RXRDYC;
        pipe0->rxrdy_consumed = false;
      }
      tu_hwfifo_write(&musb_regs->fifo[0], buffer, total_bytes, NULL);
      pipe0->remain_wlength -= total_bytes;
      // Add DATAEND on the last packet to end the data stage.
      if (pipe0_data_stage_done(total_bytes)) {
        ep_csr->csr0l = MUSB_CSRL0_TXRDY | MUSB_CSRL0_DATAEND;
      } else {
        ep_csr->csr0l = MUSB_CSRL0_TXRDY;
      }
      break;

    case PIPE0_STATE_DATA_OUT:
      TU_ASSERT(!dir_in);
      pipe0->xact_len = total_bytes;
      pipe0->buf = buffer; // arm drain target, ack RXRDY so host can send DATA OUT
      ep_csr->csr0l = MUSB_CSRL0_RXRDYC;
      pipe0->rxrdy_consumed = false;
      break;

    case PIPE0_STATE_STATUS_IN:
      TU_ASSERT(dir_in && total_bytes == 0); // only STATUS IN allowed
      ep_csr->csr0l = MUSB_CSRL0_RXRDYC | MUSB_CSRL0_DATAEND;
      pipe0->rxrdy_consumed = false;
      break;

    case PIPE0_STATE_STATUS_OUT:
      TU_ASSERT(!dir_in && total_bytes == 0); // only STATUS OUT allowed
      // First event of the STATUS OUT pair — wait for the IRQ to fire complete.
      pipe0->state = PIPE0_STATE_STATUS_OUT_PENDING_XFER;
      break;

    case PIPE0_STATE_STATUS_OUT_PENDING_IRQ:
      // Second event — IRQ already arrived, fire complete now. The old transfer is retired here,
      // so a deferred SETUP can be replayed safely.
      pipe0->state = PIPE0_STATE_IDLE;
      dcd_event_xfer_complete(rhport, ep_addr, 0, XFER_RESULT_SUCCESS, is_isr);
      pipe0_try_deferred_setup(rhport, ep_csr, is_isr);
      break;

    default: break;
  }

  return true;
}

// Advance EP0's status-stage state machine on a tail event: the csrl==0 confirmation IRQ, or such a
// confirmation combined with a new SETUP (caller sets deferred_setup_valid first). ISR context only.
static void pipe0_process_xfer_state_isr(uint8_t rhport, musb_regs_t* musb_regs, musb_ep_csr_t* ep_csr) {
  pipe0_state_t* pipe0 = &_dcd.pipe0;
  switch (pipe0->state) {
    case PIPE0_STATE_DATA_IN:
      if (pipe0_data_stage_done(pipe0->xact_len)) {
        if (pipe0->deferred_setup_valid) {
          pipe0->state = PIPE0_STATE_STATUS_OUT_PENDING_IRQ; // status confirm coalesced with deferred SETUP
        } else {
          pipe0->state = PIPE0_STATE_STATUS_OUT; // await host's STATUS-OUT ZLP IRQ
        }
      }
      dcd_event_xfer_complete(rhport, TU_EP0_IN, pipe0->xact_len, XFER_RESULT_SUCCESS, true);
      break;

    case PIPE0_STATE_STATUS_OUT:
      // Confirmation seen — await edpt0_xfer(STATUS OUT) to fire complete.
      pipe0->state = PIPE0_STATE_STATUS_OUT_PENDING_IRQ;
      break;

    case PIPE0_STATE_STATUS_OUT_PENDING_XFER:
      // edpt0_xfer(STATUS OUT) already called — fire complete and replay now.
      pipe0->state = PIPE0_STATE_IDLE;
      dcd_event_xfer_complete(rhport, TU_EP0_OUT, 0, XFER_RESULT_SUCCESS, true);
      pipe0_try_deferred_setup(rhport, ep_csr, true);
      break;

    case PIPE0_STATE_STATUS_OUT_PENDING_IRQ:
      // Confirmation already accounted for — the pairing edpt0_xfer(STATUS OUT) fires complete.
      break;

    case PIPE0_STATE_STATUS_IN:
      if (pipe0->pending_addr) {
        musb_regs->faddr = pipe0->pending_addr;
        pipe0->pending_addr = 0;
      }
      pipe0->state = PIPE0_STATE_IDLE;
      dcd_event_xfer_complete(rhport, TU_EP0_IN, 0, XFER_RESULT_SUCCESS, true);
      pipe0_try_deferred_setup(rhport, ep_csr, true);
      break;

    default: break;
  }
}

// 21.1.5: endpoint 0 service routine as peripheral
static void process_ep0_isr(uint8_t rhport) {
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  musb_ep_csr_t* ep_csr = get_ep_csr(musb_regs, 0);
  pipe0_state_t* pipe0 = &_dcd.pipe0;
  uint_fast8_t csrl = ep_csr->csr0l;

  // 21.1.5: SentStall and SetupEnd must be checked before anything else.
  if (csrl & MUSB_CSRL0_STALLED) {
    ep_csr->csr0l = 0;
    pipe0->state = PIPE0_STATE_IDLE;
    pipe0->deferred_setup_valid = false;
    pipe0->rxrdy_consumed = false;
    return;
  }

  if (csrl & MUSB_CSRL0_SETEND) {
    // Host aborted the current control transfer (new SETUP or premature STATUS).
    // do nothing, it is probably another setup packet, usbd will reset its state.
    ep_csr->csr0l = MUSB_CSRL0_SETENDC;
    pipe0->state = PIPE0_STATE_IDLE;
    pipe0->deferred_setup_valid = false;
    pipe0->rxrdy_consumed = false;
    if (!(csrl & MUSB_CSRL0_RXRDY)) {
      return; /* no SETUP waiting behind it */
    }
  }

  // Receive Data (Setup or OUT)
  if (csrl & MUSB_CSRL0_RXRDY) {
    if (pipe0->rxrdy_consumed) {
      return; // stale latched IRQ: this RXRDY's packet was already drained
    }
    switch (pipe0->state) {
      case PIPE0_STATE_IDLE: {
        uint32_t setup[2];
        TU_VERIFY(pipe0_read_setup(musb_regs, ep_csr, setup), );
        pipe0_start_setup(rhport, ep_csr, setup, true);
        break;
      }

      case PIPE0_STATE_DATA_OUT: {
        // EP0 OUT is single-packet (TU_ASSERT total_bytes <= EP0_SIZE in edpt0_xfer)
        // so the whole packet drains in one shot.
        const uint16_t count0 = ep_csr->count0;
        if (count0) {
          TU_ASSERT(pipe0->buf, );
          tu_hwfifo_read(&musb_regs->fifo[0], pipe0->buf, count0, NULL);
          pipe0->remain_wlength -= tu_min16(count0, pipe0->remain_wlength); // clamp: host may overrun
        }
        // RXRDY stays set until the next edpt0_xfer arm acks it (NAK flow control):
        // edpt0_xfer(DATA OUT) for a mid-stream packet, edpt0_xfer(STATUS IN) for the last.
        pipe0->rxrdy_consumed = true;
        if (pipe0_data_stage_done(count0)) {
          pipe0->state = PIPE0_STATE_STATUS_IN;
        }
        dcd_event_xfer_complete(rhport, TU_EP0_OUT, count0, XFER_RESULT_SUCCESS, true);
        break;
      }

      // New SETUP arrived while the old control transfer's tail events are still in flight (IRQs
      // combined under high CPU load): the old transfer's status confirm and this SETUP land together.
      case PIPE0_STATE_DATA_IN:
      case PIPE0_STATE_STATUS_OUT:
      case PIPE0_STATE_STATUS_OUT_PENDING_XFER:
      case PIPE0_STATE_STATUS_OUT_PENDING_IRQ:
      case PIPE0_STATE_STATUS_IN:
        // Save it, then finish the old transfer's tail event; deferred_setup_valid makes
        // pipe0_process_xfer_state_isr() synthesize the coalesced status confirm and replay the SETUP
        // once the old transfer is retired. Its RXRDY stays parked so a stale IRQ can't re-process it.
        TU_VERIFY(pipe0_read_setup(musb_regs, ep_csr, pipe0->deferred_setup), );
        pipe0->deferred_setup_valid = true;
        pipe0->rxrdy_consumed = true;
        pipe0_process_xfer_state_isr(rhport, musb_regs, ep_csr);
        break;

      default: break;
    }

    return;
  }

  if (csrl & MUSB_CSRL0_DATAEND) {
    // Last DATA IN chunk / STATUS IN arm wrote TXRDY|DATAEND and the status stage has not completed
    // yet — nothing to service. DataEnd is CPU-set-only per the CSR access table; whether it ever
    // reads back 1 is vendor-dependent (on cores where it reads 0 this guard is dead code).
    return;
  }

  /* When CSRL0 is zero, it means that either
 * - completion of sending any length packet TxPktRdy clear
 * - or status stage is complete (ZLP) after DataEnd is set */
  pipe0_process_xfer_state_isr(rhport, musb_regs, ep_csr);
}

// Upon BUS RESET is detected, hardware havs already done:
// faddr = 0, index = 0, flushes all ep fifos, clears all ep csr, enabled all ep interrupts
static void process_bus_reset_isr(uint8_t rhport) {
  musb_regs_t* musb = MUSB_REGS(rhport);

#if MUSB_CFG_DYNAMIC_FIFO
  alloced_fifo_bytes = CFG_TUD_ENDPOINT0_SIZE;
#endif

  pipe0_state_t* pipe0 = &_dcd.pipe0;
  pipe0->state = PIPE0_STATE_IDLE;
  pipe0->buf = NULL;
  pipe0->xact_len = 0;
  pipe0->remain_wlength = 0;
  pipe0->deferred_setup_valid = false;
  pipe0->rxrdy_consumed = false;

  musb->intr_txen = 1; /* Enable only EP0 */
  musb->intr_rxen = 0;

  /* Clear FIFO settings */
  for (unsigned i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    musb->index = i;
    hwfifo_reset(musb, i, 0);
    hwfifo_reset(musb, i, 1);
  }
  dcd_event_bus_reset(rhport, (musb->power & MUSB_POWER_HSMODE) ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL, true);
}

/*------------------------------------------------------------------
 * Device API
 *------------------------------------------------------------------*/

#if CFG_TUSB_DEBUG >= MUSB_DEBUG
static void print_musb_info(musb_regs_t* musb_regs) {
  // print version, epinfo, raminfo, config_data0, fifo_size
  TU_LOG1("musb version = %u.%u\r\n", musb_regs->hwvers_bit.major, musb_regs->hwvers_bit.minor);
  TU_LOG1("Number of endpoints: %u TX, %u RX\r\n", musb_regs->epinfo_bit.tx_ep_num, musb_regs->epinfo_bit.rx_ep_num);
  TU_LOG1("RAM Info: %u DMA Channel, %u RAM address width\r\n", musb_regs->raminfo_bit.dma_channel, musb_regs->raminfo_bit.ram_bits);

  musb_regs->index = 0;
  TU_LOG1("config_data0 = 0x%x\r\n", musb_regs->indexed_csr.config_data0);

#if MUSB_CFG_DYNAMIC_FIFO
  TU_LOG1("Dynamic FIFO configuration\r\n");
#else
  for (uint8_t i=1; i <= musb_regs->epinfo_bit.tx_ep_num; i++) {
    musb_regs->index = i;
    TU_LOG1("FIFO %u Size: TX %u RX %u\r\n", i, musb_regs->indexed_csr.fifo_size_bit.tx, musb_regs->indexed_csr.fifo_size_bit.rx);
  }
#endif
}
#endif

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;
  musb_regs_t* musb_regs = MUSB_REGS(rhport);

#if CFG_TUSB_DEBUG >= MUSB_DEBUG
  print_musb_info(musb_regs);
#endif

  musb_regs->intr_usben |= MUSB_IE_SUSPND;
  musb_dcd_int_clear(rhport);
  musb_dcd_phy_init(rhport);
  dcd_connect(rhport);
  return true;
}

void dcd_int_enable(uint8_t rhport) {
  musb_dcd_int_enable(rhport);
}

void dcd_int_disable(uint8_t rhport) {
  musb_dcd_int_disable(rhport);
}

// Receive Set Address request. Stash the new address here; hardware faddr is
// latched from pending_addr in process_ep0_isr once the STATUS IN completes (per
// USB spec, address must only take effect after the status stage).
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  musb_ep_csr_t* ep_csr = get_ep_csr(musb_regs, 0);

  pipe0_state_t* pipe0 = &_dcd.pipe0;
  pipe0->pending_addr = dev_addr;
  pipe0->buf = NULL;
  pipe0->xact_len = 0;
  pipe0->state = PIPE0_STATE_STATUS_IN;
  /* Send STATUS IN ZLP with DATAEND; host ACK fires the confirmation IRQ. */
  ep_csr->csr0l = MUSB_CSRL0_RXRDYC | MUSB_CSRL0_DATAEND;
  pipe0->rxrdy_consumed = false;
}

// Wake up host
void dcd_remote_wakeup(uint8_t rhport) {
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  musb_regs->power |= MUSB_POWER_RESUME;

  unsigned cnt = SystemCoreClock / 1000;
  while (cnt--) __NOP();

  musb_regs->power &= ~MUSB_POWER_RESUME;
}

// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  musb_regs->power |= TUD_OPT_HIGH_SPEED ? MUSB_POWER_HSENAB : 0;
  musb_regs->power |= MUSB_POWER_SOFTCONN;
}

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  musb_regs->power &= ~MUSB_POWER_SOFTCONN;
}

void dcd_sof_enable(uint8_t rhport, bool en)
{
  (void) rhport;
  (void) en;

  // TODO implement later
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * ep_desc) {
  const unsigned ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn     = tu_edpt_number(ep_addr);
  const tusb_dir_t epdir  = tu_edpt_dir(ep_addr);
  const unsigned mps     = tu_edpt_packet_size(ep_desc);

  pipe_state_t *pipe = pipe_get(epn, epdir);
  pipe->buf       = NULL;
  pipe->length    = 0;
  pipe->remaining = 0;
  pipe->armed     = false;

  musb_regs_t* musb = MUSB_REGS(rhport);
  musb_ep_csr_t* ep_csr = get_ep_csr(musb, epn);
  const uint8_t is_rx = (1 - epdir);
  musb_ep_maxp_csr_t* maxp_csr = &ep_csr->maxp_csr[is_rx];

  maxp_csr->maxp = mps;
  maxp_csr->csrh = 0;
#if MUSB_CFG_SHARED_FIFO
  if (epdir) {
    maxp_csr->csrh |= MUSB_CSRH_TX_MODE;
  }
#endif

  hwfifo_flush(musb, epn, is_rx, true);

  TU_ASSERT(hwfifo_config(musb, epn, is_rx, mps, ep_desc->bmAttributes.xfer == TUSB_XFER_BULK));
  musb->intren_ep[is_rx] |= TU_BIT(epn);

  return true;
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  const unsigned epn    = tu_edpt_number(ep_addr);
  const unsigned dir_in = tu_edpt_dir(ep_addr);
  musb_regs_t* musb = MUSB_REGS(rhport);
  musb_ep_csr_t* ep_csr = get_ep_csr(musb, epn);
  const uint8_t is_rx = 1 - dir_in;
  ep_csr->maxp_csr[is_rx].csrh = 0;
  return hwfifo_config(musb, epn, is_rx, largest_packet_size, true);
}

bool dcd_edpt_iso_activate(uint8_t rhport, tusb_desc_endpoint_t const *ep_desc ) {
  const unsigned ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn     = tu_edpt_number(ep_addr);
  const tusb_dir_t dir_in  = tu_edpt_dir(ep_addr);
  const unsigned mps     = tu_edpt_packet_size(ep_desc);

  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);

  pipe_state_t *pipe = pipe_get(epn, dir_in);
  pipe->buf       = NULL;
  pipe->length    = 0;
  pipe->remaining = 0;
  pipe->armed     = false;

  musb_regs_t* musb = MUSB_REGS(rhport);
  musb_ep_csr_t* ep_csr = get_ep_csr(musb, epn);
  const uint8_t is_rx = 1 - dir_in;
  musb_ep_maxp_csr_t* maxp_csr = &ep_csr->maxp_csr[is_rx];

  maxp_csr->maxp = mps;
  maxp_csr->csrh |= MUSB_CSRH_ISO;
#if MUSB_CFG_SHARED_FIFO
  if (dir_in) {
    maxp_csr->csrh |= MUSB_CSRH_TX_MODE;
  }
#endif

  hwfifo_flush(musb, epn, is_rx, true);

#if MUSB_CFG_DYNAMIC_FIFO
  // fifo space is already allocated, keep the address and just change packet size
  musb->fifo_size[is_rx] = hwfifo_byte2size(mps) | MUSB_FIFOSZ_DOUBLE_PACKET;
#endif

  musb->intren_ep[is_rx] |= TU_BIT(epn);

  if (ie) musb_dcd_int_enable(rhport);

  return true;
}

void dcd_edpt_close_all(uint8_t rhport)
{
  musb_regs_t* musb = MUSB_REGS(rhport);
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);

  musb->intr_txen = 1; /* Enable only EP0 */
  musb->intr_rxen = 0;
  for (unsigned i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    musb_ep_csr_t* ep_csr = get_ep_csr(musb, i);
    for (unsigned d = 0; d < 2; d++) {
      musb_ep_maxp_csr_t* maxp_csr = &ep_csr->maxp_csr[d];
      hwfifo_flush(musb, i, d, true);
      hwfifo_reset(musb, i, d);
      maxp_csr->maxp = 0;
      maxp_csr->csrh = 0;
    }
  }

#if MUSB_CFG_DYNAMIC_FIFO
  alloced_fifo_bytes = CFG_TUD_ENDPOINT0_SIZE;
#endif

  if (ie) musb_dcd_int_enable(rhport);
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes, bool is_isr)
{
  (void)rhport;
  bool ret;
  unsigned const epnum = tu_edpt_number(ep_addr);
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);

  if (epnum) {
    ret = edpt_n_xfer(rhport, ep_addr, buffer, total_bytes, false, is_isr);
  } else {
    (void) is_isr;
    ret = edpt0_xfer(rhport, ep_addr, buffer, total_bytes, is_isr);
  }

  if (ie) {
    musb_dcd_int_enable(rhport);
  }
  return ret;
}

// Submit a transfer where is managed by FIFO, When complete dcd_event_xfer_complete() is invoked to notify the stack
// - optional, however, must be listed in usbd.c
bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes, bool is_isr)
{
  (void)rhport;
  bool ret;
  unsigned const epnum = tu_edpt_number(ep_addr);
  TU_ASSERT(epnum);
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);
  ret = edpt_n_xfer(rhport, ep_addr, ff, total_bytes, true, is_isr);
  if (ie) musb_dcd_int_enable(rhport);
  return ret;
}

// Stall endpoint
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);

  unsigned const epn = tu_edpt_number(ep_addr);
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  musb_ep_csr_t* ep_csr = get_ep_csr(musb_regs, epn);

  if (0 == epn) {
    if (ep_addr == TU_EP0_OUT) { /* Ignore EP0 IN */
      pipe0_state_t* pipe0 = &_dcd.pipe0;
      pipe0->state = PIPE0_STATE_IDLE;
      pipe0->buf = NULL;
      if (pipe0->deferred_setup_valid) {
        // A deferred SETUP means the stalled transfer already ended on the wire and the host's next
        // request was ACKed — SendStall would hit that innocent request. Replay it instead of stalling.
        pipe0_try_deferred_setup(rhport, ep_csr, false);
      } else {
        // Forcing EP0 to IDLE: any RXRDY parked by the aborted transfer's flow control is stale,
        // clear it so the next SETUP IRQ is not gated off.
        pipe0->rxrdy_consumed = false;
        ep_csr->csr0l = MUSB_CSRL0_STALL;
      }
    }
  } else {
    const tusb_dir_t ep_dir = tu_edpt_dir(ep_addr);
    const uint8_t is_rx = (ep_dir == TUSB_DIR_OUT ? 1u : 0u);
    ep_csr->maxp_csr[is_rx].csrl = MUSB_CSRL_SEND_STALL(is_rx);
    pipe_state_t* pipe = pipe_get(epn, ep_dir);
    pipe->armed = false;
  }

  if (ie) musb_dcd_int_enable(rhport);
}

// clear stall, data toggle is also reset to DATA0
void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);

  unsigned const epn = tu_edpt_number(ep_addr);
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  musb_ep_csr_t* ep_csr = get_ep_csr(musb_regs, epn);
  const uint8_t is_rx = 1 - tu_edpt_dir(ep_addr);

  ep_csr->maxp_csr[is_rx].csrl = MUSB_CSRL_CLEAR_DATA_TOGGLE(is_rx);

  if (ie) musb_dcd_int_enable(rhport);
}

/*-------------------------------------------------------------------
 * ISR
 *-------------------------------------------------------------------*/
void dcd_int_handler(uint8_t rhport) {
  musb_regs_t* musb_regs = MUSB_REGS(rhport);
  const uint8_t saved_index = musb_regs->index; // save endpoint index

  //Part specific ISR setup/entry
  musb_dcd_int_handler_enter(rhport);

  uint_fast8_t intr_usb = musb_regs->intr_usb; // a read will clear this interrupt status
  uint_fast8_t intr_tx = musb_regs->intr_tx; // a read will clear this interrupt status
  uint_fast8_t intr_rx = musb_regs->intr_rx; // a read will clear this interrupt status
  // TU_LOG1("D%2x T%2x R%2x\r\n", is, txis, rxis);

  intr_usb &= musb_regs->intr_usben; /* Clear disabled interrupts */
  if (intr_usb & MUSB_IS_DISCON) {
  }
  if (intr_usb & MUSB_IS_SOF) {
    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
  }
  if (intr_usb & MUSB_IS_RESET) {
    process_bus_reset_isr(rhport);
  }
  if (intr_usb & MUSB_IS_RESUME) {
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
  }
  if (intr_usb & MUSB_IS_SUSPEND) {
    dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
  }

  intr_tx &= musb_regs->intr_txen; /* Clear disabled interrupts */

  while (intr_tx) {
    const unsigned epnum = __builtin_ctz(intr_tx);
    if (epnum == 0) {
      process_ep0_isr(rhport);  // EP0 has its own state machine (control transfers)
    } else {
      process_epin_isr(rhport, musb_regs, epnum);
    }
    intr_tx &= ~TU_BIT(epnum);

    // Double packet endpoint: TxPktRdy is clear, and interrupt is generated immediately when 1st packet is written.
    // Also catches EP0 SETUP arriving during bulk processing.
    uint_fast8_t new_intr_tx = musb_regs->intr_tx;
    new_intr_tx &= musb_regs->intr_txen;

    intr_tx |= new_intr_tx;
  }

  intr_rx &= musb_regs->intr_rxen; /* Clear disabled interrupts */
  while (intr_rx) {
    unsigned const epnum = __builtin_ctz(intr_rx);
    process_epout_isr(rhport, musb_regs, epnum, true);
    intr_rx &= ~TU_BIT(epnum);

    // Double packet endpoint: RxPktRdy is set and interrupt is generated immediately if 2nd packet is received
    uint_fast8_t new_intr_rx = musb_regs->intr_rx;
    new_intr_rx &= musb_regs->intr_rxen;

    intr_rx |= new_intr_rx;
  }

  musb_regs->index = saved_index; // restore endpoint index
}

#endif
