/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_DWC2) && !CFG_TUH_MAX3421

#if !(CFG_TUH_DWC2_SLAVE_ENABLE || CFG_TUH_DWC2_DMA_ENABLE)
#error DWC2 require either CFG_TUH_DWC2_SLAVE_ENABLE or CFG_TUH_DWC2_DMA_ENABLE to be enabled
#endif

#include "host/hcd.h"
#include "host/usbh.h"
#if CFG_TUH_HUB
#include "host/hub.h"
#endif
#include "dwc2_common.h"

  // Debug level for DWC2
  #define DWC2_DEBUG 2

  // Max number of endpoints application can open, can be larger than DWC2_CHANNEL_COUNT_MAX
  #ifndef CFG_TUH_DWC2_ENDPOINT_MAX
    #define CFG_TUH_DWC2_ENDPOINT_MAX 16u
  #endif

  #define DWC2_CHANNEL_COUNT_MAX 16u // absolute max channel count

  // Conservative time budget for enabling a slave-mode periodic OUT channel and writing its first packet before the
  // current (micro)frame ends. HFNUM.FrRem is measured in PHY clocks; 1024 clocks are 17.1 us at 60 MHz, 21.3 us at
  // 48 MHz, or 34.1 us at 30 MHz. Defer to SOF when less time remains.
  #define DWC2_PERIODIC_OUT_MIN_FRREM 1024u

TU_VERIFY_STATIC(CFG_TUH_DWC2_ENDPOINT_MAX <= 255, "currently only use 8-bit for index");

#if TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3, OPT_MCU_ESP32P4)
// On the full-speed core of ESP32-S2/S3 and ESP32-P4 (OTG 1.1) a low-speed device behind a full-speed hub can hang the
// host's transmitter: it sends the preamble packet but never the low-speed packet after it, and drops the port on the
// timeout. ESP-IDF delays each low-speed control stage by 1 ms (espressif/esp-idf#15683). On ESP32-S3 the hang only
// hit the preamble of a transaction the driver started while the last packet on the bus was a low-speed one, about one
// in fifty of those whatever the gap. A preamble after an SOF or another full-speed packet, within a transaction, or of
// the core's own NAK retries and packet sequence never hung. So one preamble channel owns the bus at a time, and the
// next may start only in a frame after the one its predecessor halted in, which puts an SOF in between. The
// port-disable recovery remains the backstop. Directly attached low-speed devices use no preamble and are not
// affected, and neither is the high-speed core of ESP32-P4 (see ls_needs_preamble()).
#define DWC2_LS_ONE_XACT_PER_FRAME 1
#else
#define DWC2_LS_ONE_XACT_PER_FRAME 0
#endif

enum {
  HPRT_W1_MASK = HPRT_CONN_DETECT | HPRT_ENABLE | HPRT_ENABLE_CHANGE | HPRT_OVER_CURRENT_CHANGE | HPRT_SUSPEND
};

enum {
  HCD_XFER_ERROR_MAX = 3,
  HCD_XFER_NAK_MAX = 15,
  HCD_XFER_NAK_DEFAULT = 3,
};

enum {
  HCD_FRAME_NUMBER_MASK = 0x3fff,
  HCD_FRAME_COUNT = HCD_FRAME_NUMBER_MASK + 1,
  HCD_NONPERIODIC_UFRAME_VALID = 0x8000
};

/* Periodic split transactions (full/low-speed interrupt or isochronous endpoint behind a high-speed hub, high-speed
 * root port) follow USB 2.0 section 11.18.4, scheduled per transaction translator (TT). They need a high-speed host
 * with hubs (HCD_SPLIT): there are no splits otherwise, and none of this is built. Interrupt:
 * - Each endpoint gets a fixed slot when opened, the least loaded of 0..3 among the endpoints sharing its TT. Slot s
 *   is the microframe Y(s) its full/low-speed transaction is budgeted in, so its start-split goes out one microframe
 *   earlier, as in an EHCI H-frame: Y(-1) is microframe 7 of the previous frame. No start-split goes out in Y6 (rule
 *   1), and the complete-splits end in the frame, unless the start-split was held behind an isochronous OUT packet.
 * - The first complete-split goes out in the second microframe after its start-split (rule 3b, Programming Guide
 *   "Normal Interrupt IN Split Operation" step 5), retried each following microframe through the third one. The TT
 *   keeps a result for one microframe only (section 11.18.7), so retries must not skip a microframe.
 * - Complete-splits on a TT go out in the order of their start-splits (section 11.18.8): one waits while an older one
 *   on its TT may still send a complete-split in the same microframe or before that is not yet in the request queue.
 * - The periodic request queue does not pass a request of the other frame parity, so a late complete-split queued
 *   behind one for the next microframe goes out two microframes late. Complete-splits are enabled a microframe ahead,
 *   those for the next microframe only after the late ones of channel interrupt handling. The core starts periodic
 *   transactions in a microframe only ahead of its non-periodic ones and not in its last eighth, so a late one is
 *   enabled for the current microframe only while no control or bulk transfer is in progress, else for the next.
 *   Late service still leaves one case: a late complete-split behind an isochronous OUT piece or a high-speed retry
 *   enabled for the next microframe earlier in the same pass.
 * - A transaction that misses its complete-split window is retried in the next service interval. A NYET on its last
 *   complete-split is an error by section 11.18.8 case 4, but is not counted here, as in Linux dwc2: late interrupt
 *   service or a crowded TT give the same NYET.
 * The TT is resolved to the nearest high-speed hub; a multi-TT hub has one TT per downstream port. */
// a plain 0/1, so that clang takes the code it disables in conditions as configured out (-Wunreachable-code)
#if TUH_OPT_HIGH_SPEED && CFG_TUH_HUB
  #define HCD_SPLIT 1
#else
  #define HCD_SPLIT 0
#endif

// A channel can be parked for the SOF interrupt to start: one whose low-speed frame is busy, or a control/bulk transfer
// whose NAKs the CPU retries (slave mode, or a split) that has had max_nak of them in this frame (tuh_configure_dwc2_t),
// see channel_park() and channel_nak_hold()
#if DWC2_LS_ONE_XACT_PER_FRAME || HCD_SPLIT || CFG_TUH_DWC2_SLAVE_ENABLE
  #define HCD_CHANNEL_PARK 1
#else
  #define HCD_CHANNEL_PARK 0
#endif

enum {
  HCD_SPLIT_SLOT_COUNT   = 4, // slots 0..3 for interrupt endpoints
  HCD_SPLIT_CSPLIT_FIRST = 2, // first complete-split, in microframes after the start-split
  HCD_SPLIT_CSPLIT_LAST  = 4, // last interrupt complete-split
  HCD_SPLIT_TAIL_SHIFT   = 3, // microframe tail, 1/8 of it, where a complete-split is not enabled for the current one
  HCD_SPLIT_SLOT_LATE    = 0xfe, // start-split held behind an isochronous OUT packet: any start-split microframe
  HCD_SPLIT_SLOT_NONE    = 0xff
};

/* An isochronous split endpoint reserves the slots of its maximum packet on its TT when opened, one per 188 bytes
 * (hub_iso_split_slot(), shared with other host drivers): OUT from slot 0 upwards, start-splits in consecutive
 * microframes (all, or begin/mid/end, rule 2a); IN early enough for one complete-split per 188 bytes plus two (rule
 * 3c) within the frame, MDATA continuing a packet and DATA0 ending it. A packet that misses its microframes fails, as
 * isochronous data is not retried (section 5.6). In buffer DMA mode an IN packet is received through a bounce buffer:
 * a complete-split returns any number of bytes, and HCDMA must stay DWORD aligned.
 * No other start-split on the TT comes between the start-splits of an OUT packet, from BEGIN to END: a TT may break
 * the packet then (a Genesys hub does), and EHCI never interleaves them. Interrupt start-splits for the microframe of
 * a BEGIN are enabled ahead of it; one due while a packet is open waits for its END and goes right after it, its
 * interval still counted from its slot. Complete-splits in between are harmless. */
#define HCD_SPLIT_ISO_BOUNCE (HCD_SPLIT && CFG_TUH_DWC2_DMA_ENABLE)
enum {
  HCD_SPLIT_ISO_PAYLOAD      = 188, // full-speed bytes per microframe (USB 2.0 11.18.1)
  HCD_SPLIT_ISO_IN_MAX       = 564,
  HCD_SPLIT_ISO_OUT_MAX      = 1023,
  HCD_SPLIT_ISO_BOUNCE_SIZE  = 192, // one complete-split, DWORD multiple
  HCD_SPLIT_ISO_BOUNCE_COUNT = 2    // isochronous split IN endpoints open at once in DMA mode
};
enum {
  HCD_SPLIT_XACTPOS_MID   = 0,
  HCD_SPLIT_XACTPOS_END   = 1,
  HCD_SPLIT_XACTPOS_BEGIN = 2,
  HCD_SPLIT_XACTPOS_ALL   = 3
};

//--------------------------------------------------------------------
//
//--------------------------------------------------------------------

// Host driver struct for each opened endpoint
typedef struct {
  union {
    uint32_t hcchar;
    dwc2_channel_char_t hcchar_bm;
  };
  union {
    uint32_t hcsplt;
    dwc2_channel_split_t hcsplt_bm;
  };

  struct TU_ATTR_PACKED {
    uint32_t uframe_interval : 19; // micro-frame interval
    uint32_t speed           : 2;
    uint32_t next_pid        : 2; // PID for next transfer
    uint32_t next_do_ping    : 1; // Do PING for next transfer if possible (highspeed OUT)
    uint32_t closing         : 1; // endpoint is closing
    uint32_t aborting        : 1; // periodic transfer abort is waiting for its channel's halt
    uint32_t periodic_phase  : 1; // periodic transfer phase is established
    uint32_t xfer_pending    : 1; // periodic transfer waiting for its service interval
    uint32_t split_slot      : 3; // periodic split: microframe its transaction is budgeted in
    uint32_t tt_multi        : 1; // split: the TT serves only this hub port
  };

  struct TU_ATTR_PACKED {
    uint32_t uframe_countdown : 19; // micro-frame count down to transfer for periodic
    uint32_t periodic_err     : 2;  // errors of a periodic transfer waiting for its next service interval
    uint32_t split_bounce     : 2;  // isochronous split IN in DMA mode: its bounce buffer + 1
    uint32_t split_held       : 1;  // interrupt split: start-split held behind an isochronous OUT packet on its TT
    uint32_t                  : 8;
  };

  uint8_t* buffer;
  uint16_t buflen;
  uint16_t periodic_frame; // frame/microframe number of the last scheduled periodic transaction
#if HCD_SPLIT
  uint16_t periodic_xferred; // bytes a periodic split OUT transfer sent before its retry in a later service interval
#endif
} hcd_endpoint_t;

// Additional info for each channel when it is active
typedef struct {
  volatile bool allocated;
  uint8_t ep_id;
  struct TU_ATTR_PACKED {
    uint8_t err_count : 3;
    uint8_t halted_nyet : 1;
    uint8_t closing : 1; // closing channel
    uint8_t csplit_wait : 1; // periodic complete-split waits for the SOF before csplit_next
    uint8_t iso_open : 1; // isochronous split OUT packet: BEGIN enabled, END not yet queued
    uint8_t split_issued : 1; // periodic start-split has its request-order number
  };
  uint8_t result;

  uint16_t xferred_bytes;  // bytes that accumulate transferred though USB bus for the whole hcd_edpt_xfer(), which can
                           // be composed of multiple channel_xfer_start() (retry with NAK/NYET)
  uint16_t fifo_bytes;     // bytes written/read from/to FIFO (may not be transferred on USB bus).
  struct TU_ATTR_PACKED {
    uint8_t retry_disabled : 1; // disabled to throttle a split retry (NAK in / XactErr out); re-arm on its halt
    uint8_t csplit_next    : 3; // periodic split: earliest microframe of the next complete-split, after split_uframe
    uint8_t split_seq      : 4; // periodic split: request order of its start-split in the same microframe
  };
  volatile bool aborting;  // periodic abort waiting for the channel's halt
#if HCD_SPLIT
  uint16_t split_uframe;   // periodic split: microframe number of the start-split
#endif
#if HCD_CHANNEL_PARK
  uint8_t  park;           // channel is programmed but waiting for the SOF interrupt to start it, see channel_park_t
  uint8_t  nak_frame;      // control/bulk: frame of the NAKs counted in nak_count, see channel_nak_hold()
  uint8_t  park_uframe;    // split control/bulk: microframe a complete-split was parked in, see split_csplit_park()
  struct TU_ATTR_PACKED {
    uint8_t nak_count   : 4; // control/bulk: NAKs in nak_frame, up to HCD_XFER_NAK_MAX
    uint8_t nak_hold    : 1; // control/bulk: max_nak reached, the retry waits for the frame after nak_frame
    uint8_t uframe_hold : 1; // split control/bulk: the complete-split waits for the microframe after park_uframe
  };
#endif
} hcd_xfer_t;

#if HCD_CHANNEL_PARK
// The call to replay from SOF. Replaying the enable on its own is not enough: an IN token has to re-check
// request-queue space, and an OUT has to write its FIFO.
typedef enum {
  PARK_NONE = 0,
  PARK_IN_TOKEN,           // channel_send_in_token(.., next_periodic_frame = false)
  PARK_IN_TOKEN_PERIODIC,  // channel_send_in_token(.., next_periodic_frame = true)
  PARK_XFER_START,          // channel_xfer_start(.., defer_periodic_out = false)
  PARK_XFER_START_PERIODIC, // channel_xfer_start(.., defer_periodic_out = true)
  PARK_CSPLIT_OUT,          // enable a split control/bulk OUT complete-split, which carries no data
  PARK_CSPLIT_IN            // send a split control/bulk IN complete-split
} channel_park_t;
#endif

typedef struct {
  hcd_xfer_t xfer[DWC2_CHANNEL_COUNT_MAX];
  hcd_endpoint_t edpt[CFG_TUH_DWC2_ENDPOINT_MAX];
} hcd_data_t;

static hcd_data_t _hcd_data;

#if HCD_SPLIT
// A complete-split waits behind an older one on its TT, or for the end of channel interrupt handling: enable it then
static bool _hcd_csplit_behind;

// Channel interrupts are being handled: a complete-split for the next microframe is enabled only once all are, after
// the late ones for this microframe, which the periodic request queue would otherwise hold behind it
static bool _hcd_csplit_pass;

// A late complete-split waits behind an older one on its TT: enable it as soon as that one is handled
static bool _hcd_csplit_late_held;

// Microframe in which a control or bulk transfer last completed, as nonperiodic_uframe_tag() gives it, else 0
static uint16_t _hcd_nonperiodic_uframe;

// The microframe of hfnum, tagged so that it never equals 0, which stands for none
TU_ATTR_ALWAYS_INLINE static inline uint16_t nonperiodic_uframe_tag(uint32_t hfnum) {
  return (uint16_t) (HCD_NONPERIODIC_UFRAME_VALID | (hfnum & HCD_FRAME_NUMBER_MASK));
}

// A held start-split was started during the FIFO-empty interrupt, which then stays enabled for its FIFO write
static bool _hcd_split_kicked;
#endif

#if HCD_SPLIT_ISO_BOUNCE
typedef struct {
  TUH_EPBUF_DEF(buf, HCD_SPLIT_ISO_BOUNCE_SIZE);
} hcd_split_bounce_t;

CFG_TUH_MEM_SECTION static hcd_split_bounce_t _hcd_split_bounce[HCD_SPLIT_ISO_BOUNCE_COUNT];
#endif

static tuh_configure_dwc2_t _tuh_cfg = {.use_hs_phy = TUH_OPT_HIGH_SPEED, .max_nak = HCD_XFER_NAK_DEFAULT};

//--------------------------------------------------------------------
//
//--------------------------------------------------------------------
static uint8_t dwc2_channel_count(const dwc2_regs_t* dwc2) {
  const dwc2_ghwcfg2_t ghwcfg2 = {.value = dwc2->ghwcfg2};
  return tu_min8(ghwcfg2.num_host_ch + 1, DWC2_CHANNEL_COUNT_MAX);
}

static tusb_speed_t hprt_speed_get(dwc2_regs_t* dwc2) {
  tusb_speed_t speed;
  const dwc2_hprt_t hprt = {.value = dwc2->hprt};
  switch(hprt.speed) {
    case HPRT_SPEED_HIGH: speed = TUSB_SPEED_HIGH; break;
    case HPRT_SPEED_FULL: speed = TUSB_SPEED_FULL; break;
    case HPRT_SPEED_LOW : speed = TUSB_SPEED_LOW ; break;
    default:
      speed = TUSB_SPEED_INVALID;
      TU_BREAKPOINT();
    break;
  }
  return speed;
}

TU_ATTR_ALWAYS_INLINE static inline bool dma_host_enabled(const dwc2_regs_t* dwc2) {
  (void) dwc2;
  // Internal DMA only
  const dwc2_ghwcfg2_t ghwcfg2 = {.value = dwc2->ghwcfg2};
  return CFG_TUH_DWC2_DMA_ENABLE && ghwcfg2.arch == GHWCFG2_ARCH_INTERNAL_DMA;
}

#if CFG_TUH_MEM_DCACHE_ENABLE
bool hcd_dcache_clean(const void* addr, uint32_t data_size) {
  TU_VERIFY(addr && data_size);
  return dwc2_dcache_clean(addr, data_size);
}

bool hcd_dcache_invalidate(const void* addr, uint32_t data_size) {
  TU_VERIFY(addr && data_size);
  return dwc2_dcache_invalidate(addr, data_size);
}

bool hcd_dcache_clean_invalidate(const void* addr, uint32_t data_size) {
  TU_VERIFY(addr && data_size);
  return dwc2_dcache_clean_invalidate(addr, data_size);
}
#endif

// Allocate a channel for new transfer
TU_ATTR_ALWAYS_INLINE static inline uint8_t channel_alloc(dwc2_regs_t* dwc2) {
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
    if (!xfer->allocated) {
      dwc2->channel[ch_id].hcsplt = 0; // no complete-split seen on it until channel_xfer_start() sets it up
      tu_memclr(xfer, sizeof(hcd_xfer_t));
      xfer->allocated = true;
      return ch_id;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

// Check if is periodic (interrupt/isochronous)
TU_ATTR_ALWAYS_INLINE static inline bool channel_is_periodic(uint32_t hcchar) {
  const dwc2_channel_char_t hcchar_bm = {.value = hcchar};
  return hcchar_bm.ep_type == HCCHAR_EPTYPE_INTERRUPT || hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS;
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t req_queue_avail(const dwc2_regs_t* dwc2, bool is_period) {
  if (is_period) {
    const dwc2_hptxsts_t hptxsts = {.value = dwc2->hptxsts};
    return hptxsts.req_queue_available;
  } else {
    const dwc2_hnptxsts_t hnptxsts = {.value = dwc2->hnptxsts};
    return hnptxsts.req_queue_available;
  }
}

#if DWC2_LS_ONE_XACT_PER_FRAME
#define DWC2_LS_FRAME_MASK 0x3FFFu // FrNum counts 0..0x3FFF
#define DWC2_LS_FRAME_CLAIM_MAX 1  // furthest a claim can reach: the next frame, for a periodic channel

static uint32_t _ls_frame_end = 0; // last frame used by a low-speed transaction
static uint8_t _ls_busy_ch = TUSB_INDEX_INVALID_8; // preamble channel in flight: claimed, not yet halted

TU_ATTR_ALWAYS_INLINE static inline uint32_t ls_frame_now(const dwc2_regs_t* dwc2) {
  return dwc2->hfnum & DWC2_LS_FRAME_MASK;
}

TU_ATTR_ALWAYS_INLINE static inline int32_t ls_frame_diff(uint32_t a, uint32_t b) {
  const uint32_t d = (a - b) & DWC2_LS_FRAME_MASK;
  return (int32_t)(d << 18) >> 18; // sign-extend 14-bit so the FrNum wrap compares correctly
}

// A transaction is free to run once the frame used by the previous one has passed. offset is where this channel
// would start: periodic channels are armed for the next frame (ODDFRM), non-periodic ones go out in the current frame.
TU_ATTR_ALWAYS_INLINE static inline bool ls_frame_free(const dwc2_regs_t* dwc2, uint32_t offset) {
  const int32_t diff = ls_frame_diff(ls_frame_now(dwc2) + offset, _ls_frame_end);
  // A claim reaches at most DWC2_LS_FRAME_CLAIM_MAX frames ahead, so a reservation that appears to sit
  // further out than that is a stale one the frame counter has since wrapped past. Without this, a bus
  // left idle for half a counter cycle (8.192 s) would read as permanently busy.
  return diff > 0 || diff < -((int32_t) DWC2_LS_FRAME_CLAIM_MAX);
}

TU_ATTR_ALWAYS_INLINE static inline bool ls_needs_preamble(const dwc2_regs_t* dwc2, uint32_t hcchar, uint32_t hcsplt) {
  const dwc2_channel_char_t char_bm = {.value = hcchar};
  const dwc2_channel_split_t splt_bm = {.value = hcsplt};
  const dwc2_hprt_t hprt = {.value = dwc2->hprt};
  // high/full speed device, split transaction or low-speed root port: no preamble, nothing to space out
  if (!(char_bm.low_speed_dev && !splt_bm.split_en && hprt.speed == HPRT_SPEED_FULL)) {
    return false;
  }
  // only the full-speed core hangs: a high-speed one (ESP32-P4 OTG 2.0) is left alone even when running at full speed
  const dwc2_ghwcfg2_t ghwcfg2 = {.value = dwc2->ghwcfg2};
  return ghwcfg2.hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED;
}

// Take the bus for this channel until it halts. Returns false while another preamble channel is in flight or the
// frame its predecessor ended in has not passed, in which case the caller must leave the channel alone. Never
// waits, because the callers are reached from interrupt context on every NAK retry and control stage.
static bool ls_frame_claim(const dwc2_regs_t* dwc2, dwc2_channel_t* channel) {
  if (!ls_needs_preamble(dwc2, channel->hcchar, channel->hcsplt)) {
    return true;
  }

  // Held until the halt: where the transaction actually ran is not known up front. It can slip into the next frame
  // when armed late, and in DMA mode the core repeats NAKed tokens and runs every packet without software. A channel
  // claiming again has halted since, so a stale hold of its own, or of a channel since freed, does not count.
  const uint8_t ch_id = (uint8_t) (channel - dwc2->channel);
  if (_ls_busy_ch != TUSB_INDEX_INVALID_8 && _ls_busy_ch != ch_id && _hcd_data.xfer[_ls_busy_ch].allocated) {
    return false;
  }

  const uint32_t offset = channel_is_periodic(channel->hcchar) ? 1u : 0u;
  if (!ls_frame_free(dwc2, offset)) {
    return false;
  }
  _ls_busy_ch = ch_id;
  _ls_frame_end = (ls_frame_now(dwc2) + offset) & DWC2_LS_FRAME_MASK;
  return true;
}

// The channel halted: free the bus, but not the frame its last transaction ran in.
TU_ATTR_ALWAYS_INLINE static inline void ls_bus_release(const dwc2_regs_t* dwc2, uint8_t ch_id) {
  if (_ls_busy_ch == ch_id) {
    _ls_busy_ch = TUSB_INDEX_INVALID_8;
    const uint32_t now = ls_frame_now(dwc2);
    if (ls_frame_diff(now, _ls_frame_end) > 0) {
      _ls_frame_end = now;
    }
  }
}

#endif

#if HCD_CHANNEL_PARK
#define DWC2_LS_PARKED 0xFFFFu // channel_send_in_token() parked the channel, never a frame number

// Park a channel for the SOF interrupt to replay the call kind names, and make sure SOF is running to pick it up.
static void channel_park(dwc2_regs_t* dwc2, dwc2_channel_t* channel, channel_park_t kind) {
  _hcd_data.xfer[channel - dwc2->channel].park = (uint8_t) kind;
  if (0 == (dwc2->gintmsk & GINTMSK_SOFM)) {
    dwc2->gintsts = GINTSTS_SOF;
    dwc2->gintmsk |= GINTMSK_SOFM;
  }
}
#endif

static void channel_dealloc(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  xfer->allocated = false;
#if HCD_CHANNEL_PARK
  xfer->park = PARK_NONE;
#endif
#if DWC2_LS_ONE_XACT_PER_FRAME
  ls_bus_release(dwc2, ch_id); // a completed periodic IN can retire without a halt interrupt of its own
#endif
  dwc2->haintmsk &= ~TU_BIT(ch_id);
}

#if HCD_CHANNEL_PARK
// Take back a parked channel before it was enabled again: nothing of it is on the bus and
// no halt will come to retire it, since in DMA mode channel_disable() leaves a periodic channel to an automatic halt
// that only an enabled channel produces. Returns false otherwise, including for a slave IN parked between the packets
// of its transfer, which is still enabled and goes through channel_disable() and its halt instead. The caller masks
// this controller's interrupt so SOF cannot replay the channel meanwhile.
static bool channel_park_retire(dwc2_regs_t* dwc2, uint8_t ch_id) {
  const hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  const dwc2_channel_t* channel = &dwc2->channel[ch_id];
  if (!xfer->allocated || xfer->park == PARK_NONE || (channel->hcchar & HCCHAR_CHENA)) {
    return false;
  }
  // channel_xfer_start() predicts the PID after the whole transfer, give back the one it was armed with
  const dwc2_channel_char_t hcchar = {.value = channel->hcchar};
  if (hcchar.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
    const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
    _hcd_data.edpt[xfer->ep_id].next_pid = hctsiz.pid;
  }
  channel_dealloc(dwc2, ch_id);
  return true;
}
#endif

#if HCD_CHANNEL_PARK
// Low byte of the current (1 ms) frame number: a high-speed port counts microframes
static uint8_t channel_nak_frame(dwc2_regs_t* dwc2) {
  const uint32_t frnum = dwc2->hfnum & HCD_FRAME_NUMBER_MASK;
  return (uint8_t) (hprt_speed_get(dwc2) == TUSB_SPEED_HIGH ? (frnum >> 3) : frnum);
}
#endif

// Only slave mode and splits retry a NAK in software; a DMA build without them parks for low-speed pacing only
#if HCD_CHANNEL_PARK && (CFG_TUH_DWC2_SLAVE_ENABLE || HCD_SPLIT)
// A NAK ended a transaction of a control or bulk transfer whose retry is up to the CPU: in slave mode, or a split one
// (the core repeats a non-split NAKed token itself only in DMA mode). Count it in the current frame and return true
// when the endpoint has had max_nak of them there (tuh_configure_dwc2_t): its retry then waits for the next frame.
// Each retry is an interrupt or more (a split one: a start-split and complete-splits answered NYET until the hub's
// transaction ends, an interrupt each), so an endpoint the device keeps NAKing, such as an idle bulk IN, would
// otherwise retry as fast as the CPU can service them and leave it no time for anything else.
static bool channel_nak_hold(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t*   xfer  = &_hcd_data.xfer[ch_id];
  const uint8_t frame = channel_nak_frame(dwc2);
  if (frame != xfer->nak_frame) {
    xfer->nak_frame = frame;
    xfer->nak_count = 0;
  }
  if (xfer->nak_count < HCD_XFER_NAK_MAX) {
    xfer->nak_count++;
  }
  xfer->nak_hold = (_tuh_cfg.max_nak != 0 && xfer->nak_count >= _tuh_cfg.max_nak) ? 1u : 0u;
  return xfer->nak_hold == 1u;
}
#endif

#if HCD_SPLIT
// Send the complete-split of a split control or bulk transaction in the next microframe, from the SOF interrupt: after
// its start-split ACK, and after a NYET. The hub forwards the full-speed transaction meanwhile, about 50 us for a
// 64-byte packet, and a complete-split sent sooner gets NYET, an interrupt each, as fast as the CPU can re-arm them.
// Waiting leaves the CPU to the application, for one transaction per pipe per microframe.
static void split_csplit_park(dwc2_regs_t* dwc2, dwc2_channel_t* channel) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[channel - dwc2->channel];
  xfer->park_uframe = (uint8_t) (dwc2->hfnum & HCD_FRAME_NUMBER_MASK);
  xfer->uframe_hold = 1;
  const dwc2_channel_char_t hcchar = {.value = channel->hcchar};
  channel_park(dwc2, channel, hcchar.ep_dir == TUSB_DIR_IN ? PARK_CSPLIT_IN : PARK_CSPLIT_OUT);
}
#endif

static bool channel_disable(const dwc2_regs_t* dwc2, dwc2_channel_t* channel) {
#if HCD_CHANNEL_PARK
  // A parked channel is never in the request queue, so CHDIS|CHENA below halts it without putting anything on the
  // bus. Drop the park so SOF does not arm it afterwards.
  _hcd_data.xfer[channel - dwc2->channel].park = PARK_NONE;
#endif
  const bool is_period = channel_is_periodic(channel->hcchar);
  if (dma_host_enabled(dwc2)) {
    // In buffer DMA or external DMA mode:
    // - Channel disable must not be programmed for non-split periodic channels. At the end of the next uframe/frame (in
    //   the worst case), the controller generates a channel halted and disables the channel automatically.
    // - For split enabled channels (both non-periodic and periodic), channel disable must not be programmed randomly.
    //   However, channel disable can be programmed for specific scenarios such as NAK and FrmOvrn.
    if (is_period) {
      return true;
    }
  } else {
    while (0 == req_queue_avail(dwc2, is_period)) {
      // blocking wait for request queue available
    }
  }
  channel->hcintmsk |= HCINT_HALTED;
  channel->hcchar |= HCCHAR_CHDIS | HCCHAR_CHENA; // must set both CHDIS and CHENA
  return true;
}

// A channel_disable() would not wait. In slave mode it takes a request queue entry, which a full queue may free only
// once the ISR pops the RxFIFO, and only the application adds entries (Programming Guide 3.5, Table 1-2). Checked with
// the interrupt masked, so that the disable never waits with the ISR held off.
TU_ATTR_ALWAYS_INLINE static inline bool channel_disable_ready(const dwc2_regs_t* dwc2, const dwc2_channel_t* channel) {
  return !CFG_TUH_DWC2_SLAVE_ENABLE || dma_host_enabled(dwc2) ||
         0 != req_queue_avail(dwc2, channel_is_periodic(channel->hcchar));
}

// Wait, with the interrupt enabled, until channel_disable_ready()
TU_ATTR_ALWAYS_INLINE static inline void channel_disable_wait(const dwc2_regs_t* dwc2, const dwc2_channel_t* channel) {
  while (!channel_disable_ready(dwc2, channel)) {
    // the ISR drains the request queue
  }
}

// Retire all active host channels on root-port disconnect without waiting for
// Channel Halted interrupts.
// stop new channel/FIFO interrupts, flush queued slave requests, request a
// halt for enabled channels, then clear their interrupt and software state.
static void channel_cleanup_on_disconnect(dwc2_regs_t *dwc2) {
  const uint32_t xfer_ints = GINTSTS_NPTX_FIFO_EMPTY | GINTSTS_PTX_FIFO_EMPTY | GINTSTS_HCINT;
  dwc2->gintmsk &= ~xfer_ints;
  dwc2->gintsts  = xfer_ints;
  dwc2->haintmsk = 0;

  const uint8_t max_channel = dwc2_channel_count(dwc2);

#if HCD_CHANNEL_PARK
  // SOF stops while the port is down, so anything parked here would never be replayed. Drop the parks
  // and release the frame, the channels are halted below anyway.
  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    _hcd_data.xfer[ch_id].park = PARK_NONE;
  }
#endif
#if DWC2_LS_ONE_XACT_PER_FRAME
  _ls_busy_ch = TUSB_INDEX_INVALID_8;
  _ls_frame_end = (ls_frame_now(dwc2) - 1u) & DWC2_LS_FRAME_MASK;
#endif

  #if CFG_TUH_DWC2_SLAVE_ENABLE
  if (!dma_host_enabled(dwc2)) {
    // With CHENA clear, CHDIS flushes a posted request without consuming
    // request-queue space. Clear EPDIR as required for this flush operation.
    for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
      if (_hcd_data.xfer[ch_id].allocated) {
        dwc2_channel_t *channel = &dwc2->channel[ch_id];
        const uint32_t  hcchar  = channel->hcchar;
        if (hcchar & HCCHAR_CHENA) {
          channel->hcchar = (hcchar & ~(HCCHAR_CHENA | HCCHAR_EPDIR)) | HCCHAR_CHDIS;
        }
      }
    }
  }
  #endif

  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    if (_hcd_data.xfer[ch_id].allocated) {
      dwc2_channel_t *channel = &dwc2->channel[ch_id];
      const uint32_t  hcchar  = channel->hcchar;
      if (hcchar & HCCHAR_CHENA) {
        channel->hcchar = hcchar | HCCHAR_CHDIS;
      }
      channel->hcintmsk = 0;
      channel->hcint    = 0xFFFFFFFFU;
    }
  }

  tu_memclr(_hcd_data.xfer, sizeof(_hcd_data.xfer));
  for (uint8_t ep_id = 0; ep_id < CFG_TUH_DWC2_ENDPOINT_MAX; ep_id++) {
    hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];
    if (edpt->hcchar_bm.enable) {
      edpt->closing      = 1;
      edpt->xfer_pending = 0;
    }
  }
}

// periodic_frame value of a channel that channel_enable() did not enable, never a frame number nor DWC2_LS_PARKED
#define HCD_PERIODIC_FRAME_NOT_STARTED 0xFFFEu

// Microframe of a periodic split endpoint's start-split, the one before its slot
TU_ATTR_ALWAYS_INLINE static inline uint8_t split_ss_uframe(const hcd_endpoint_t* edpt) {
  return (uint8_t) ((edpt->split_slot + 7u) & 7u);
}

#if HCD_SPLIT
// Number an admitted periodic start-split after those already issued for its microframe.
// In slave mode a nonempty OUT enters the request queue at its packet's final FIFO DWORD,
// not at CHENA (DWC_otg Programming Guide 4.20a, Table 1-2).
static void split_ss_issued(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  uint8_t seq = 0;
  for (uint8_t i = 0; i < max_channel; i++) {
    const hcd_xfer_t* other = &_hcd_data.xfer[i];
    if (i != ch_id && other->allocated && other->split_issued &&
        other->split_uframe == xfer->split_uframe && other->split_seq >= seq) {
      seq = (uint8_t) (other->split_seq + 1u);
    }
  }
  xfer->split_seq = seq;
  xfer->split_issued = 1;
}

// Reserve start timing and an isochronous OUT packet before CHENA, even when its FIFO write
// must wait. Keep IN, DMA and zero-length OUT numbering at their existing enable point.
static void split_ss_record(dwc2_regs_t* dwc2, uint8_t ch_id, uint16_t uframe) {
  hcd_xfer_t*     xfer = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  xfer->split_uframe   = uframe;
  xfer->split_issued   = 0;
  xfer->iso_open       = edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS &&
                   edpt->hcchar_bm.ep_dir == TUSB_DIR_OUT && edpt->buflen > HCD_SPLIT_ISO_PAYLOAD;
  edpt->split_held     = 0;
  edpt->periodic_frame = (uint16_t) ((uframe - ((uframe - split_ss_uframe(edpt)) & 7u)) & HCD_FRAME_NUMBER_MASK);
  if (dma_host_enabled(dwc2) || edpt->hcchar_bm.ep_dir == TUSB_DIR_IN || edpt->buflen == 0) {
    split_ss_issued(dwc2, ch_id);
  }
}

static bool split_ss_start_ok(dwc2_regs_t* dwc2, uint8_t ch_id, uint16_t uframe, uint8_t split_ss);
#else
TU_ATTR_ALWAYS_INLINE static inline void split_ss_record(dwc2_regs_t* dwc2, uint8_t ch_id, uint16_t uframe) {
  (void) dwc2;
  (void) ch_id;
  (void) uframe;
}

TU_ATTR_ALWAYS_INLINE static inline bool split_ss_start_ok(dwc2_regs_t* dwc2, uint8_t ch_id, uint16_t uframe,
                                                          uint8_t split_ss) {
  (void) dwc2;
  (void) ch_id;
  (void) uframe;
  (void) split_ss;
  return true;
}
#endif

// Enable a channel, selecting the following frame for a new periodic transfer.
// Return that frame from the same HFNUM sample used for ODDFRM selection.
// A periodic start-split (split_ss, its microframe, other than HCD_SPLIT_SLOT_NONE) is enabled only when
// split_ss_start_ok() takes that frame, otherwise HCD_PERIODIC_FRAME_NOT_STARTED is returned. It is recorded before
// CHENA, ahead of its ACK.
// Clear CHDIS explicitly: a halted channel may retain it in HCCHAR.
static uint16_t channel_enable(dwc2_regs_t* dwc2, dwc2_channel_t* channel,
                               bool next_periodic_frame, uint8_t split_ss) {
  uint32_t hcchar = channel->hcchar & ~HCCHAR_CHDIS;
  uint16_t periodic_frame = 0;
  if (next_periodic_frame) {
    // Prevent the USB interrupt from consuming the selected frame before
    // HCCHAR.CHENA is written. Queue-space waits happen before this helper.
    const uint32_t gahbcfg = dwc2->gahbcfg;
    dwc2->gahbcfg          = gahbcfg & ~GAHBCFG_GINT;
    const uint32_t hfnum   = dwc2->hfnum;
    periodic_frame         = (uint16_t) ((hfnum + 1u) & HCD_FRAME_NUMBER_MASK);
    if (split_ss != HCD_SPLIT_SLOT_NONE) {
      const uint8_t ch_id = (uint8_t) (channel - dwc2->channel);
      if (!split_ss_start_ok(dwc2, ch_id, periodic_frame, split_ss)) {
        dwc2->gahbcfg = gahbcfg;
        return HCD_PERIODIC_FRAME_NOT_STARTED;
      }
      split_ss_record(dwc2, ch_id, periodic_frame);
    }
    hcchar          = (hcchar & ~HCCHAR_ODDFRM) | (((hfnum & 1u) ^ 1u) << HCCHAR_ODDFRM_Pos);
    channel->hcchar = hcchar | HCCHAR_CHENA;
    dwc2->gahbcfg   = gahbcfg;
  } else {
    channel->hcchar = hcchar | HCCHAR_CHENA;
  }
  return periodic_frame;
}

// Attempt to send an IN token to receive data. For a new periodic transfer,
// select its frame only after request-queue space is available.
// With DWC2_LS_ONE_XACT_PER_FRAME, returns DWC2_LS_PARKED when the channel was parked instead.
static uint16_t channel_send_in_token(dwc2_regs_t* dwc2, dwc2_channel_t* channel,
                                      bool next_periodic_frame, uint8_t split_ss) {
#if DWC2_LS_ONE_XACT_PER_FRAME
  // ls_frame_claim() books a periodic channel for the next frame, so pick ODDFRM at the enable. A retry that set it
  // beforehand may be a frame late by then, and so would the SOF replay of a parked call.
  if (channel_is_periodic(channel->hcchar) && ls_needs_preamble(dwc2, channel->hcchar, channel->hcsplt)) {
    next_periodic_frame = true;
  }
  // Ahead of the queue wait, so a channel that cannot have the frame leaves without entering it: SOF
  // replays this whole function, and that wait must not run on the deferral path. Taking the frame is a
  // read-modify-write on state shared with the ISR, and hcd_edpt_xfer() reaches here from task context,
  // so mask this controller's interrupt across it.
  const uint32_t gahbcfg = dwc2->gahbcfg;
  dwc2->gahbcfg = gahbcfg & ~GAHBCFG_GINT;
  const bool claimed = ls_frame_claim(dwc2, channel);
  if (!claimed) {
    channel_park(dwc2, channel, next_periodic_frame ? PARK_IN_TOKEN_PERIODIC : PARK_IN_TOKEN);
  }
  dwc2->gahbcfg = gahbcfg;
  if (!claimed) {
    return DWC2_LS_PARKED; // the park itself may already be replayed by now, so callers go by this
  }
#endif
  while (0 == req_queue_avail(dwc2, channel_is_periodic(channel->hcchar))) {
    // blocking wait for request queue available
  }
  return channel_enable(dwc2, channel, next_periodic_frame, split_ss);
}

#if HCD_SPLIT
// 188-byte microframes reserved by an isochronous split endpoint
TU_ATTR_ALWAYS_INLINE static inline uint8_t split_iso_count(const hcd_endpoint_t* edpt) {
  return (uint8_t) tu_div_ceil(edpt->hcchar_bm.ep_size, HCD_SPLIT_ISO_PAYLOAD);
}

// Last complete-split of a periodic split transaction, in microframes after its start-split
TU_ATTR_ALWAYS_INLINE static inline uint8_t split_csplit_last(const hcd_endpoint_t* edpt) {
  if (edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
    return (uint8_t) (HCD_SPLIT_CSPLIT_FIRST + split_iso_count(edpt) + 1u); // one per 188 bytes plus two
  }
  return HCD_SPLIT_CSPLIT_LAST;
}

// Endpoints behind the same transaction translator
static bool split_tt_same(const hcd_endpoint_t* a, const hcd_endpoint_t* b) {
  return a->hcsplt_bm.hub_addr == b->hcsplt_bm.hub_addr &&
         (!a->tt_multi || a->hcsplt_bm.hub_port == b->hcsplt_bm.hub_port);
}

// An isochronous OUT packet on the TT of edpt has its BEGIN start-split enabled but not yet its END
static bool split_iso_out_open(dwc2_regs_t* dwc2, const hcd_endpoint_t* edpt) {
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  for (uint8_t i = 0; i < max_channel; i++) {
    const hcd_xfer_t* xfer = &_hcd_data.xfer[i];
    if (xfer->allocated && xfer->iso_open && split_tt_same(edpt, &_hcd_data.edpt[xfer->ep_id])) {
      return true;
    }
  }
  return false;
}

// An interrupt start-split due now is held while an isochronous OUT packet is open on its TT; the END of the packet
// starts it. Return true if held.
static bool split_ss_hold(dwc2_regs_t* dwc2, hcd_endpoint_t* edpt) {
  const bool hold = edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_INTERRUPT && split_iso_out_open(dwc2, edpt);
  if (hold) {
    edpt->split_held = 1;
  }
  return hold;
}

// A periodic start-split on channel ch_id may be enabled for microframe uframe: that of its slot, or for a held one
// (HCD_SPLIT_SLOT_LATE) that of slot 0 through 5, where an isochronous OUT packet ends at the latest; and not while an
// isochronous OUT packet is open on its TT for an interrupt one. Checked on the HFNUM sample of its CHENA with the
// interrupt masked, so that no ISR opens a packet in between.
static bool split_ss_start_ok(dwc2_regs_t* dwc2, uint8_t ch_id, uint16_t uframe, uint8_t split_ss) {
  const bool slot_ok = (split_ss == HCD_SPLIT_SLOT_LATE) ? (((uframe + 1u) & 7u) <= 5u) : ((uframe & 7u) == split_ss);
  return slot_ok && !split_ss_hold(dwc2, &_hcd_data.edpt[_hcd_data.xfer[ch_id].ep_id]);
}

// The start-split of channel a went out before that of channel b, both periodic splits: by microframe, then by the
// request order recorded at their admission point
static bool split_xfer_before(uint8_t a, uint8_t b) {
  const hcd_xfer_t* xa = &_hcd_data.xfer[a];
  const hcd_xfer_t* xb = &_hcd_data.xfer[b];
  const uint16_t    d  = (uint16_t) ((xb->split_uframe - xa->split_uframe) & HCD_FRAME_NUMBER_MASK);
  return (d != 0) ? (d < HCD_FRAME_COUNT / 2) : (xa->split_seq < xb->split_seq);
}

// Microframes from the current one (hfnum) to uframe, negative when past
TU_ATTR_ALWAYS_INLINE static inline int16_t uframe_ahead(uint16_t uframe, uint32_t hfnum) {
  const uint16_t d = (uint16_t) ((uframe - hfnum) & HCD_FRAME_NUMBER_MASK);
  return (int16_t) ((d < HCD_FRAME_COUNT / 2) ? d : (int32_t) d - HCD_FRAME_COUNT);
}

// An older periodic split on the TT of channel ch_id may still send a complete-split not yet in the request queue in
// the microframe this channel's complete-split is for (ahead of the current one, hfnum), or before: this channel waits,
// so that the TT sees complete-splits in start-split order (USB 2.0 11.18.8). Requests are taken to enter the queue in
// the order their channels are enabled.
static bool split_csplit_older(dwc2_regs_t* dwc2, uint8_t ch_id, uint32_t hfnum, uint16_t ahead) {
  const hcd_endpoint_t* edpt        = &_hcd_data.edpt[_hcd_data.xfer[ch_id].ep_id];
  const bool            is_dma      = dma_host_enabled(dwc2);
  const uint8_t         max_channel = dwc2_channel_count(dwc2);
  for (uint8_t i = 0; i < max_channel; i++) {
    const hcd_xfer_t*         other   = &_hcd_data.xfer[i];
    const dwc2_channel_t*     channel = &dwc2->channel[i];
    const dwc2_channel_char_t hcchar  = {.value = channel->hcchar};
    // in its complete-split phase, or about to be: a split start-split with complete-splits (an isochronous OUT has
    // none, though it is ACKed too), its ACK not handled yet. A slave channel halting other than to retry a
    // complete-split after NYET (error, stall, missed window, close) sends no further one.
    const bool csplit   = (channel->hcsplt & HCSPLT_COMPLSPLT) != 0;
    const bool ss_acked = !csplit && (channel->hcsplt & HCSPLT_SPLITEN) && (channel->hcint & HCINT_ACK) &&
                          !(hcchar.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS && hcchar.ep_dir == TUSB_DIR_OUT);
    const bool halting  = !is_dma && (channel->hcintmsk & HCINT_HALTED) && !other->halted_nyet;
    if (i == ch_id || !other->allocated || !(csplit || ss_acked) || halting || !channel_is_periodic(channel->hcchar) ||
        !split_tt_same(edpt, &_hcd_data.edpt[other->ep_id]) || !split_xfer_before(i, ch_id)) {
      continue;
    }
    // its next complete-split, or a retry of the enabled one
    const uint16_t next = (uint16_t) (other->split_uframe + (ss_acked ? HCD_SPLIT_CSPLIT_FIRST : other->csplit_next));
    if (uframe_ahead(next, hfnum) <= (int16_t) ahead) {
      return true;
    }
  }
  return false;
}

// Pending RX/HC work matters to future admission only when another periodic
// split can still produce a current-microframe complete-split. Nonperiodic
// traffic uses a separate queue and must not consume the prequeue opportunity.
// Keep this independent of TT identity: every periodic channel shares the queue.
static bool split_csplit_current(dwc2_regs_t* dwc2, uint8_t ch_id, uint32_t hfnum) {
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  for (uint8_t i = 0; i < max_channel; i++) {
    const hcd_xfer_t* other = &_hcd_data.xfer[i];
    if (i == ch_id || !other->allocated || !other->split_issued || other->closing || other->aborting) {
      continue; // a start-split not yet issued has no complete-split
    }
    const hcd_endpoint_t* edpt = &_hcd_data.edpt[other->ep_id];
    if (!edpt->hcsplt_bm.split_en || !channel_is_periodic(edpt->hcchar) ||
        (edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS && edpt->hcchar_bm.ep_dir == TUSB_DIR_OUT)) {
      continue;
    }
    const bool csplit = (dwc2->channel[i].hcsplt & HCSPLT_COMPLSPLT) != 0;
    const uint16_t next = (uint16_t) (other->split_uframe + (csplit ? other->csplit_next : HCD_SPLIT_CSPLIT_FIRST));
    if (uframe_ahead(next, hfnum) <= 0) {
      return true;
    }
  }
  return false;
}

// A control or bulk transfer ended in the microframe of hfnum. One from an earlier microframe is forgotten, since its
// microframe number recurs.
static bool split_nonperiodic_ended(uint32_t hfnum) {
  const uint16_t now = nonperiodic_uframe_tag(hfnum);
  if (_hcd_nonperiodic_uframe != now) {
    _hcd_nonperiodic_uframe = 0;
    return false;
  }
  return true;
}

// The core may have started non-periodic transactions in the microframe of hfnum: a control or bulk transfer is in
// progress, or one ended in it
static bool split_nonperiodic_active(dwc2_regs_t* dwc2, uint32_t hfnum) {
  if (split_nonperiodic_ended(hfnum)) {
    return true;
  }
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  for (uint8_t i = 0; i < max_channel; i++) {
    const hcd_xfer_t* xfer = &_hcd_data.xfer[i];
    if (xfer->allocated && !channel_is_periodic(_hcd_data.edpt[xfer->ep_id].hcchar)) {
      return true;
    }
  }
  return false;
}

// The core starts periodic transactions in a microframe only ahead of its non-periodic ones, and none in its tail,
// where an enable may also land in the next microframe. A periodic enable for the microframe of hfnum after that would
// wait two microframes for its parity, ahead of the requests for the next one.
static bool split_uframe_closed(dwc2_regs_t* dwc2, dwc2_hfnum_t hfnum) {
  return hfnum.remainning < (((dwc2->hfir & HFIR_FRIVL_Msk) + 1u) >> HCD_SPLIT_TAIL_SHIFT) ||
         split_nonperiodic_active(dwc2, hfnum.value);
}

// An isochronous split OUT piece for microframe uframe cannot go out in it any more: it has passed, or the core no
// longer serves it
static bool split_iso_out_missed(dwc2_regs_t* dwc2, uint16_t uframe) {
  const dwc2_hfnum_t hfnum = {.value = dwc2->hfnum};
  const int16_t      ahead = uframe_ahead(uframe, hfnum.value);
  return ahead < 0 || (ahead == 0 && split_uframe_closed(dwc2, hfnum));
}

// Let the SOF interrupt enable the next complete-split
static void channel_csplit_wait(dwc2_regs_t* dwc2, hcd_xfer_t* xfer) {
  xfer->csplit_wait = 1;
  if (0 == (dwc2->gintmsk & GINTMSK_SOFM)) {
    dwc2->gintsts = GINTSTS_SOF;
    dwc2->gintmsk |= GINTMSK_SOFM;
  }
}

// Enable the next complete-split of a periodic split transaction, xfer->csplit_next microframes after its start-split
// at the earliest. The channel keeps the start-split's frame parity, so enabled one microframe ahead it waits for its
// microframe; still earlier, or behind an older complete-split on its TT, it waits for the SOF interrupt or the end of
// channel interrupt handling. A late complete-split goes out at once while the core still starts periodic transactions
// in the microframe, else it is enabled for the next one; one for the current microframe the core no longer serves
// would wait two microframes for its parity, as one queued behind a request for the next microframe does: the periodic
// request queue does not pass a request of the other frame parity. Return false when the complete-split window has
// passed, or for isochronous IN when a microframe was skipped: the TT has freed what it held for it (11.18.7).
static bool channel_csplit_arm(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t*     xfer    = &_hcd_data.xfer[ch_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];

  if (xfer->closing || xfer->aborting) {
    xfer->csplit_wait = 0;
    return true; // leave it idle for the requested halt
  }

  // the request queue has room before the HFNUM sample that selects the microframe
  const bool is_in = (channel->hcchar & HCCHAR_EPDIR) != 0;
  while (is_in && 0 == req_queue_avail(dwc2, true)) {
    // blocking wait for request queue available
  }
  const dwc2_hfnum_t hfnum = {.value = dwc2->hfnum};
  const uint16_t     now   = (uint16_t) ((hfnum.value - xfer->split_uframe) & HCD_FRAME_NUMBER_MASK);
  if (now + 1u < xfer->csplit_next) {
    channel_csplit_wait(dwc2, xfer);
    return true;
  }

  const hcd_endpoint_t* edpt   = &_hcd_data.edpt[xfer->ep_id];
  uint16_t              target = tu_max16(xfer->csplit_next, now);
  const bool            iso_in = is_in && edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS;
  // A complete-split for the current microframe the core no longer serves would hold up the requests for the next
  // one, among them an older complete-split of its TT (11.18.8): take it as late for the next microframe instead.
  if (target == now && split_uframe_closed(dwc2, hfnum)) {
    target++;
  }
  if (target > split_csplit_last(edpt) || (iso_in && target > xfer->csplit_next)) {
    xfer->csplit_wait = 0;
    return false;
  }
  // Pending receive status can produce ChHltd only when popped. Let that work
  // admit current retries before a future request can block the periodic queue.
  const uint32_t gintsts = dwc2->gintsts;
  const uint32_t gintmsk = dwc2->gintmsk;
  const bool defer = target != now &&
                     (_hcd_csplit_pass || ((gintsts & gintmsk & (GINTSTS_RXFLVL | GINTSTS_HCINT)) &&
                                          split_csplit_current(dwc2, ch_id, hfnum.value)));
  if (defer || split_csplit_older(dwc2, ch_id, hfnum.value, (uint16_t) (target - now))) {
    _hcd_csplit_behind    = true;
    _hcd_csplit_late_held = _hcd_csplit_late_held || (target == now);
    channel_csplit_wait(dwc2, xfer);
    return true;
  }
  xfer->csplit_wait = 0;
  xfer->csplit_next = (uint8_t) (target + 1u);

  // a completion that this complete-split continues left ChHltd latched in slave mode
  channel->hcint     = HCINT_HALTED;
  const uint32_t odd = (uint32_t) ((xfer->split_uframe + target) & 1u);
  channel->hcchar    = (channel->hcchar & ~(HCCHAR_CHENA | HCCHAR_CHDIS | HCCHAR_ODDFRM)) | (odd << HCCHAR_ODDFRM_Pos) |
                    HCCHAR_CHENA;
  return true;
}

// Slave mode: a split OUT channel about to be disabled expects no data unless its start-split packet is in the TX FIFO.
// A disabled OUT channel drops the data it still expects from the TX FIFO it shares with the other channels, see the
// complete-split programming in handle_channel_out_slave().
static void channel_out_expect_posted(dwc2_regs_t* dwc2, uint8_t ch_id) {
  const hcd_xfer_t*         xfer    = &_hcd_data.xfer[ch_id];
  dwc2_channel_t*           channel = &dwc2->channel[ch_id];
  const dwc2_channel_char_t hcchar  = {.value = channel->hcchar};
  // a complete-split expects none already: its start-split ACK cleared XferSize
  if (dma_host_enabled(dwc2) || hcchar.ep_dir != TUSB_DIR_OUT ||
      (channel->hcsplt & (HCSPLT_SPLITEN | HCSPLT_COMPLSPLT)) != HCSPLT_SPLITEN) {
    return;
  }
  bool posted;
  if (hcchar.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
    posted = xfer->fifo_bytes > xfer->xferred_bytes; // its piece is written
  } else {
    posted = channel_is_periodic(hcchar.value) ? (xfer->split_issued != 0) : (xfer->fifo_bytes > 0);
  }
  if (!posted) {
    dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
    hctsiz.xfer_size            = 0;
    channel->hctsiz             = hctsiz.value;
  }
}
#else
TU_ATTR_ALWAYS_INLINE static inline void channel_out_expect_posted(dwc2_regs_t* dwc2, uint8_t ch_id) {
  (void) dwc2;
  (void) ch_id;
}

TU_ATTR_ALWAYS_INLINE static inline bool channel_csplit_arm(dwc2_regs_t* dwc2, uint8_t ch_id) {
  (void) dwc2;
  (void) ch_id;
  return false;
}

TU_ATTR_ALWAYS_INLINE static inline bool split_ss_hold(dwc2_regs_t* dwc2, hcd_endpoint_t* edpt) {
  (void) dwc2;
  (void) edpt;
  return false;
}
#endif

// Find currently enabled channel, other than one an abort is already halting. Note: EP0 is bidirectional
TU_ATTR_ALWAYS_INLINE static inline uint8_t channel_find_enabled(dwc2_regs_t* dwc2, uint8_t dev_addr, uint8_t ep_num, uint8_t ep_dir) {
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    if (_hcd_data.xfer[ch_id].allocated && !_hcd_data.xfer[ch_id].aborting) {
      const dwc2_channel_char_t hcchar = {.value = dwc2->channel[ch_id].hcchar};
      if (hcchar.dev_addr == dev_addr && hcchar.ep_num == ep_num && (ep_num == 0 || hcchar.ep_dir == ep_dir)) {
        return ch_id;
      }
    }
  }
  return TUSB_INDEX_INVALID_8;
}


// Allocate a new endpoint
TU_ATTR_ALWAYS_INLINE static inline uint8_t edpt_alloc(void) {
  for (uint32_t i = 0; i < CFG_TUH_DWC2_ENDPOINT_MAX; i++) {
    hcd_endpoint_t* edpt = &_hcd_data.edpt[i];
    if (edpt->hcchar_bm.enable == 0) {
      tu_memclr(edpt, sizeof(hcd_endpoint_t));
      edpt->hcchar_bm.enable = 1;
      return i;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

TU_ATTR_ALWAYS_INLINE static inline void edpt_dealloc(hcd_endpoint_t *edpt) {
  edpt->hcchar_bm.enable = 0;
}

typedef enum {
  CHANNEL_TEARDOWN_RETIRED, // deallocated now
  CHANNEL_TEARDOWN_HALTING, // its halt interrupt retires it
  CHANNEL_TEARDOWN_WAIT     // nothing changed: wait for channel_disable_ready() with the interrupt enabled, then retry
} channel_teardown_t;

// Tear down the channel of a closing or aborted transfer, with the interrupt masked so that the ISR does not retire or
// reallocate it in between. A channel with nothing posted (parked, or a complete-split waiting on a halted channel) is
// retired now, which also covers DMA mode where a periodic channel cannot be disabled. Otherwise it is marked and
// disabled, with no FIFO write between channel_out_expect_posted() and the disable. An aborted periodic transfer keeps
// its endpoint from a replacement until its halt: a started transaction still completes (Programming Guide 3.5).
static channel_teardown_t channel_teardown(dwc2_regs_t* dwc2, uint8_t ch_id, bool closing) {
  hcd_xfer_t*     xfer    = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt    = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
#if HCD_CHANNEL_PARK
  if (channel_park_retire(dwc2, ch_id)) {
    return CHANNEL_TEARDOWN_RETIRED;
  }
#endif
  if (HCD_SPLIT && xfer->csplit_wait && 0 == (channel->hcchar & HCCHAR_CHENA)) {
    const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
    if (edpt->hcchar_bm.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
      edpt->next_pid = hctsiz.pid;
    }
    xfer->csplit_wait = 0;
    channel_dealloc(dwc2, ch_id);
    return CHANNEL_TEARDOWN_RETIRED;
  }
  if (!channel_disable_ready(dwc2, channel)) {
    return CHANNEL_TEARDOWN_WAIT;
  }
  if (closing) {
    xfer->closing = 1;
  } else if (channel_is_periodic(channel->hcchar)) {
    edpt->aborting = 1;
    xfer->aborting = true;
  }
  channel_out_expect_posted(dwc2, ch_id);
  channel_disable(dwc2, channel);
  return CHANNEL_TEARDOWN_HALTING;
}

// close an opened endpoint
static void edpt_close(dwc2_regs_t *dwc2, uint8_t ep_id) {
  hcd_endpoint_t *edpt           = &_hcd_data.edpt[ep_id];
  const uint32_t  gahbcfg        = dwc2->gahbcfg;
  bool            channel_active = false;
  while (1) {
    dwc2->gahbcfg = gahbcfg & ~GAHBCFG_GINT;
    uint8_t ch_id = TUSB_INDEX_INVALID_8;
    for (uint8_t i = 0; i < DWC2_CHANNEL_COUNT_MAX; i++) {
      if (_hcd_data.xfer[i].allocated && _hcd_data.xfer[i].ep_id == ep_id) {
        ch_id = i;
        break; // only 1 active channel per endpoint
      }
    }
    if (ch_id >= DWC2_CHANNEL_COUNT_MAX) {
      break;
    }
    const channel_teardown_t teardown = channel_teardown(dwc2, ch_id, true);
    if (teardown != CHANNEL_TEARDOWN_WAIT) {
      channel_active = teardown == CHANNEL_TEARDOWN_HALTING;
      break;
    }
    dwc2->gahbcfg = gahbcfg;
    channel_disable_wait(dwc2, &dwc2->channel[ch_id]);
  }

  edpt->closing = 1;
  if (!channel_active) {
    edpt_dealloc(edpt); // no active channel, safe to de-alloc now
  }
  dwc2->gahbcfg = gahbcfg;
}

// Find an endpoint that is opened previously with hcd_edpt_open()
// Note: EP0 is bidirectional
static uint8_t edpt_find_opened(uint8_t dev_addr, uint8_t ep_num, uint8_t ep_dir,
                                bool include_closing) {
  for (uint8_t i = 0; i < (uint8_t)CFG_TUH_DWC2_ENDPOINT_MAX; i++) {
    const hcd_endpoint_t     *edpt      = &_hcd_data.edpt[i];
    const dwc2_channel_char_t hcchar_bm = edpt->hcchar_bm;
    if (hcchar_bm.enable && (include_closing || !edpt->closing) && hcchar_bm.dev_addr == dev_addr &&
        hcchar_bm.ep_num == ep_num &&
        (ep_num == 0 || hcchar_bm.ep_dir == ep_dir)) {
      return i;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

TU_ATTR_ALWAYS_INLINE static inline uint16_t cal_packet_count(uint16_t len, uint16_t ep_size) {
  if (len == 0) {
    return 1;
  } else {
    return tu_div_ceil(len, ep_size);
  }
}

// Packets a channel is programmed with for the endpoint's remaining data. A split transaction carries one packet: a
// split OUT sends one at a time, see channel_split_out_more() (isochronous OUT goes in 188-byte pieces).
static uint16_t channel_packet_count(const hcd_endpoint_t* edpt, uint16_t ep_size) {
  if (HCD_SPLIT && edpt->hcsplt_bm.split_en && edpt->hcchar_bm.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS &&
      edpt->hcchar_bm.ep_dir == TUSB_DIR_OUT) {
    return 1;
  }
  return cal_packet_count(edpt->buflen, ep_size);
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t cal_next_pid(uint8_t pid, uint8_t packet_count) {
  if (packet_count & 0x01u) {
    return pid ^ 0x02u; // toggle DATA0 and DATA1
  } else {
    return pid;
  }
}

//--------------------------------------------------------------------
// Periodic split scheduling
//--------------------------------------------------------------------
#if HCD_SPLIT

// Downstream bus time of one full/low-speed interrupt transaction in us, from the USB 2.0 section 5.11.3 formulas
// with worst-case bit stuffing (low speed includes the hub's setup time)
static uint16_t split_bus_time_us(const hcd_endpoint_t* edpt) {
  const uint32_t bits = 4u + (56u * edpt->hcchar_bm.ep_size) / 6u;
  const uint32_t ns   = edpt->hcchar_bm.low_speed_dev ? (64726u + 677u * bits) : (9107u + 84u * bits);
  return (uint16_t) ((ns + 999u) / 1000u);
}

// Another open periodic split endpoint on the TT of edpt
TU_ATTR_ALWAYS_INLINE static inline bool split_periodic_peer(const hcd_endpoint_t* edpt, const hcd_endpoint_t* other) {
  return other != edpt && other->hcchar_bm.enable && !other->closing && other->hcsplt_bm.split_en &&
         channel_is_periodic(other->hcchar) && split_tt_same(edpt, other);
}

// Slot of a newly opened interrupt split endpoint: the slot with the least downstream bus time already taken by the
// open endpoints on its TT, a microframe reserved by an isochronous endpoint counting in full. Intervals are ignored,
// as if every endpoint ran in every frame.
static uint8_t split_slot_pick(uint8_t ep_id) {
  const hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  uint16_t load[HCD_SPLIT_SLOT_COUNT] = {0};
  for (uint8_t i = 0; i < CFG_TUH_DWC2_ENDPOINT_MAX; i++) {
    const hcd_endpoint_t* other = &_hcd_data.edpt[i];
    if (!split_periodic_peer(edpt, other)) {
      continue;
    }
    if (other->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
      for (uint8_t s = other->split_slot; s < other->split_slot + split_iso_count(other); s++) {
        if (s < HCD_SPLIT_SLOT_COUNT) {
          load[s] += 125u;
        }
      }
      continue;
    }
    load[other->split_slot] += split_bus_time_us(other);
  }

  uint8_t slot = 0;
  for (uint8_t s = 1; s < HCD_SPLIT_SLOT_COUNT; s++) {
    if (load[s] < load[slot]) {
      slot = s;
    }
  }
  return slot;
}

// Microframes reserved by the isochronous endpoints on the TT of a newly opened one, as hub_iso_split_slot() expects
static uint8_t split_iso_used(uint8_t ep_id) {
  const hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  uint8_t used = 0;
  for (uint8_t i = 0; i < CFG_TUH_DWC2_ENDPOINT_MAX; i++) {
    const hcd_endpoint_t* other = &_hcd_data.edpt[i];
    if (split_periodic_peer(edpt, other) && other->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
      used |= (uint8_t) (((1u << split_iso_count(other)) - 1u) << other->split_slot);
    }
  }
  return used;
}
#endif // HCD_SPLIT

#if HCD_SPLIT_ISO_BOUNCE
// Bounce buffer for a newly opened isochronous split IN endpoint, 0 if none is free (else its index + 1)
static uint8_t split_bounce_alloc(void) {
  for (uint8_t bounce = 1; bounce <= HCD_SPLIT_ISO_BOUNCE_COUNT; bounce++) {
    bool used = false;
    for (uint8_t i = 0; i < CFG_TUH_DWC2_ENDPOINT_MAX; i++) {
      const hcd_endpoint_t* edpt = &_hcd_data.edpt[i];
      used = used || (edpt->hcchar_bm.enable && edpt->split_bounce == bounce);
    }
    if (!used) {
      return bounce;
    }
  }
  return 0;
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t* split_bounce_buf(const hcd_endpoint_t* edpt) {
  return _hcd_split_bounce[edpt->split_bounce - 1u].buf;
}

// Bytes a complete-split of an isochronous split IN packet is programmed for, after xferred bytes
TU_ATTR_ALWAYS_INLINE static inline uint16_t split_bounce_len(const hcd_endpoint_t* edpt, uint16_t xferred) {
  return tu_min16(edpt->buflen - xferred, HCD_SPLIT_ISO_BOUNCE_SIZE);
}
#endif

//--------------------------------------------------------------------
//
//--------------------------------------------------------------------

/* USB Data FIFO Layout

  The FIFO is split up into
  - EPInfo: for storing DMA metadata (check dcd_dwc2.c for more details)
  - 1 RX FIFO: for receiving data
  - 1 TX FIFO for non-periodic (NPTX)
  - 1 TX FIFO for periodic (PTX)

  We allocated TX FIFO from top to bottom (using top pointer), this to allow the RX FIFO to grow dynamically which is
  possible since the free space is located between the RX and TX FIFOs.

   ----------------- otg_dfifo_depth
  |    HCDMAn    |   (DMA only, sized per runtime DMA mode)
  |--------------|-- gdfifocfg.EPINFOBASE (= gdfifocfg.GDFIFOCfg)
  | Non-Periodic |
  |   TX FIFO    |
  |--------------|--- GNPTXFSIZ.addr (fixed size)
  |   Periodic   |
  |   TX FIFO    |
  |--------------|--- HPTXFSIZ.addr (expandable downward)
  |    FREE      |
  |              |
  |--------------|-- GRXFSIZ (expandable upward)
  |  RX FIFO     |
  ---------------- 0
*/

/* Programming Guide 2.1.2 FIFO RAM allocation
 * RX
 * - Largest-EPsize/4 + 2 (status info). recommended x2 if high bandwidth or multiple ISO are used.
 * - 2 for transfer complete and channel halted status
 * - 1 for each Control/Bulk out endpoint to Handle NAK/NYET (i.e max is number of host channel)
 *
 * TX non-periodic (NPTX)
 * - At least largest-EPsize/4, recommended x2
 *
 * TX periodic (PTX)
 * - At least largest-EPsize*MulCount/4 (MulCount up to 3 for high-bandwidth ISO/interrupt)
*/
static void dfifo_host_init(uint8_t rhport, bool is_hs_phy) {
  const dwc2_controller_t* dwc2_controller = &_dwc2_controller[rhport];
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const uint8_t channel_count = dwc2_channel_count(dwc2);

  // Scatter/Gather DMA mode is not yet supported. Buffer DMA only need 1 words per channel
  const bool is_dma = dma_host_enabled(dwc2);
  uint16_t dfifo_top = dwc2_controller->otg_dfifo_depth;
  if (is_dma) {
    dfifo_top -= channel_count;
  }

  // fixed allocation for now, improve later:
  // - ptx_largest is limited to 64 words for FS since most FS core only has 256-320 words total
  uint32_t nptx_largest;
  uint32_t ptx_largest;
  if (is_hs_phy) {
    nptx_largest = TUSB_EPSIZE_BULK_HS / 4;
    ptx_largest = TUSB_EPSIZE_ISO_HS_MAX / 4;
  } else {
    nptx_largest = TUSB_EPSIZE_BULK_FS / 4;
    ptx_largest = 256 / 4;
  }

  uint16_t nptxfsiz = 2 * nptx_largest;
  uint16_t rxfsiz = 2 * (ptx_largest + 2) + channel_count;
  TU_ASSERT(dfifo_top >= (nptxfsiz + rxfsiz),);
  uint16_t ptxfsiz = dfifo_top - (nptxfsiz + rxfsiz);

  dwc2->gdfifocfg = (dfifo_top << GDFIFOCFG_EPINFOBASE_SHIFT) | dfifo_top;

  dwc2->grxfsiz = rxfsiz;

  dfifo_top -= nptxfsiz;
  dwc2->gnptxfsiz = tu_u32_from_u16(nptxfsiz, dfifo_top);

  dfifo_top -= ptxfsiz;
  dwc2->hptxfsiz = tu_u32_from_u16(ptxfsiz, dfifo_top);
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// optional hcd configuration, called by tuh_configure()
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport;
  TU_VERIFY(cfg_id == TUH_CFGID_DWC2 && cfg_param != NULL);
  tuh_configure_param_t const* cfg = (tuh_configure_param_t const*) cfg_param;
  _tuh_cfg = cfg->dwc2;
  _tuh_cfg.max_nak = tu_min8(_tuh_cfg.max_nak, HCD_XFER_NAK_MAX);
  return true;
}

// Initialize controller to host mode
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  dwc2_clock_init(rhport, rh_init->role);

  tu_memclr(&_hcd_data, sizeof(_hcd_data));

  // Core Initialization
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const bool is_hs_phy = dwc2_core_is_highspeed_phy(dwc2, _tuh_cfg.use_hs_phy);
  const bool is_dma = dma_host_enabled(dwc2);
  TU_ASSERT(dwc2_core_init(rhport, is_hs_phy, is_dma));

  //------------- 3.1 Host Initialization -------------//
  // Enable HFIR reload
  if (dwc2->gsnpsid >= DWC2_CORE_REV_2_92a) {
    dwc2->hfir |= HFIR_RELOAD_CTRL;
  }

  // force host mode and wait for mode switch
  dwc2->gusbcfg = (dwc2->gusbcfg & ~GUSBCFG_FDMOD) | GUSBCFG_FHMOD;
  while ((dwc2->gintsts & GINTSTS_CMOD) != GINTSTS_CMODE_HOST) {}

  #ifdef TUP_USBIP_DWC2_STM32
  dwc2_stm32_gccfg_cfg(dwc2, false, true);
  #endif

  if (is_hs_phy && (rh_init->speed == TUSB_SPEED_HIGH || rh_init->speed == TUSB_SPEED_AUTO)) {
    dwc2->hcfg &= ~HCFG_FSLS_ONLY; // max speed
  } else {
    dwc2->hcfg |= HCFG_FSLS_ONLY;  // disable high speed mode
  }

  // configure a fixed-allocated fifo scheme
  dfifo_host_init(rhport, is_hs_phy);

  dwc2->hprt = HPRT_W1_MASK; // clear all write-1-clear bits
  dwc2->hprt = HPRT_POWER; // turn on VBUS

  // Enable required interrupts
  dwc2->gintmsk |= GINTSTS_OTGINT | GINTSTS_HPRTINT | GINTSTS_HCINT | GINTSTS_DISCINT;

  // NPTX can hold at least 2 packet, change interrupt level to half-empty
  uint32_t gahbcfg = dwc2->gahbcfg & ~GAHBCFG_TX_FIFO_EPMTY_LVL;
  gahbcfg |= GAHBCFG_GINT;   // Enable global interrupt
  dwc2->gahbcfg = gahbcfg;

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  // Turn off VBUS
  dwc2->hprt = HPRT_W1_MASK; // clear w1c bits without side effects
  // HPRT_POWER is not set -> VBUS off

  dwc2_core_deinit(rhport);
  return true;
}

// Enable USB interrupt
void hcd_int_enable (uint8_t rhport) {
  dwc2_int_set(rhport, TUSB_ROLE_HOST, true);
}

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport) {
  dwc2_int_set(rhport, TUSB_ROLE_HOST, false);
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  return dwc2->hfnum & HFNUM_FRNUM_Msk;
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  return dwc2->hprt & HPRT_CONN_STATUS;
}

// Reset USB bus on the port. Return immediately, bus reset sequence may not be complete.
// Some port would require hcd_port_reset_end() to be invoked after 10ms to complete the reset sequence.
void hcd_port_reset(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint32_t hprt = dwc2->hprt & ~HPRT_W1_MASK;
  hprt |= HPRT_RESET;
  dwc2->hprt = hprt;
}

// Complete bus reset sequence, may be required by some controllers
void hcd_port_reset_end(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint32_t hprt = dwc2->hprt & ~HPRT_W1_MASK; // skip w1c bits
  hprt &= ~HPRT_RESET;
  dwc2->hprt = hprt;
}

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const tusb_speed_t speed = hprt_speed_get(dwc2);
  return speed;
}

// HCD closes all opened endpoints belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  for (uint8_t ep_id = 0; ep_id < CFG_TUH_DWC2_ENDPOINT_MAX; ep_id++) {
    const hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];
    if (edpt->hcchar_bm.enable && edpt->hcchar_bm.dev_addr == dev_addr) {
      edpt_close(dwc2, ep_id);
    }
  }
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

// Open an endpoint
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t* desc_ep) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const tusb_speed_t rh_speed = hprt_speed_get(dwc2);

  tuh_bus_info_t bus_info;
  tuh_bus_info_get(dev_addr, &bus_info);

#if HCD_SPLIT
  // A split goes to the transaction translator of the nearest high-speed hub; full-speed hubs may sit in between.
  // Without a high-speed host with hubs there is no split.
  const bool split = (rh_speed == TUSB_SPEED_HIGH && bus_info.speed != TUSB_SPEED_HIGH);
  uint8_t tt_addr = bus_info.hub_addr;
  uint8_t tt_port = bus_info.hub_port;
  if (split) {
    while (tt_addr != 0) {
      tuh_bus_info_t hub_info;
      TU_ASSERT(tuh_bus_info_get(tt_addr, &hub_info));
      if (hub_info.speed == TUSB_SPEED_HIGH) {
        break;
      }
      tt_addr = hub_info.hub_addr;
      tt_port = hub_info.hub_port;
    }
    TU_ASSERT(tt_addr != 0);
  }

  const bool split_iso = split && desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS;
  if (split_iso) {
    const uint16_t mps = tu_edpt_packet_size(desc_ep);
    const bool     in  = tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN;
    TU_VERIFY(mps > 0 && mps <= (in ? HCD_SPLIT_ISO_IN_MAX : HCD_SPLIT_ISO_OUT_MAX));
  }
#endif

  // find a free endpoint
  const uint8_t ep_id = edpt_alloc();
  TU_ASSERT(ep_id < CFG_TUH_DWC2_ENDPOINT_MAX);
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];

  dwc2_channel_char_t* hcchar_bm = &edpt->hcchar_bm;
  hcchar_bm->ep_size         = tu_edpt_packet_size(desc_ep);
  hcchar_bm->ep_num          = tu_edpt_number(desc_ep->bEndpointAddress);
  hcchar_bm->ep_dir          = tu_edpt_dir(desc_ep->bEndpointAddress);
  hcchar_bm->low_speed_dev   = (bus_info.speed == TUSB_SPEED_LOW) ? 1 : 0;
  hcchar_bm->ep_type         = desc_ep->bmAttributes.xfer; // ep_type matches TUSB_XFER_*
  hcchar_bm->err_multi_count = 0;
  hcchar_bm->dev_addr        = dev_addr;
  hcchar_bm->odd_frame       = 0;
  hcchar_bm->disable         = 0;
  hcchar_bm->enable          = 1;

#if HCD_SPLIT
  dwc2_channel_split_t* hcsplt_bm = &edpt->hcsplt_bm;
  hcsplt_bm->hub_port        = tt_port;
  hcsplt_bm->hub_addr        = tt_addr;
  hcsplt_bm->xact_pos        = 0;
  hcsplt_bm->split_compl     = 0;
  hcsplt_bm->split_en        = split ? 1 : 0;
  edpt->tt_multi             = split && hub_tt_multi(tt_addr);
#endif

  edpt->speed = bus_info.speed;
  edpt->next_pid = HCTSIZ_PID_DATA0;
  switch (desc_ep->bmAttributes.xfer) {
    case TUSB_XFER_ISOCHRONOUS:
      edpt->uframe_interval = 1u << (desc_ep->bInterval - 1);
      if (bus_info.speed == TUSB_SPEED_FULL) {
        edpt->uframe_interval <<= 3;
      }
      break;

    case TUSB_XFER_INTERRUPT:
      if (bus_info.speed == TUSB_SPEED_HIGH) {
        edpt->uframe_interval = 1u << (desc_ep->bInterval - 1);
      } else {
        edpt->uframe_interval = desc_ep->bInterval << 3;
      }
      break;

    default:
      break;
  }

  if (channel_is_periodic(edpt->hcchar)) {
    // HFNUM cannot distinguish elapsed periods longer than one counter cycle. USB permits the host to provide a
    // shorter period, so bound the selected period to the history available from HFNUM.
    const uint32_t ucount = (rh_speed == TUSB_SPEED_HIGH) ? 1u : 8u;
    edpt->uframe_interval = tu_min32(edpt->uframe_interval, HCD_FRAME_COUNT * ucount);

#if HCD_SPLIT
    if (split_iso) {
      uint8_t slot;
      if (!hub_iso_split_slot(split_iso_used(ep_id), hcchar_bm->ep_size, hcchar_bm->ep_dir, &slot)) {
        edpt_dealloc(edpt);
        return false; // no window left on this TT
      }
      edpt->split_slot = slot;
  #if HCD_SPLIT_ISO_BOUNCE
      if (hcchar_bm->ep_dir == TUSB_DIR_IN && dma_host_enabled(dwc2)) {
        edpt->split_bounce = split_bounce_alloc();
        if (edpt->split_bounce == 0) {
          edpt_dealloc(edpt);
          return false; // every bounce buffer is taken
        }
      }
  #endif
    } else if (split) {
      edpt->split_slot = split_slot_pick(ep_id);
    }
#endif
  }

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  dwc2_regs_t  *dwc2   = DWC2_REG(rhport);
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);
  const uint8_t ep_id  = edpt_find_opened(daddr, ep_num, ep_dir, true);
  TU_ASSERT(ep_id < CFG_TUH_DWC2_ENDPOINT_MAX);

  edpt_close(dwc2, ep_id);

  return true;
}

// clean up channel after part of transfer is done but the whole urb is not complete
static void channel_xfer_out_wrapup(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  const dwc2_channel_t* channel = &dwc2->channel[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];

  const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
  const dwc2_channel_char_t hcchar = {.value = channel->hcchar};
  if (hcchar.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
    edpt->next_pid = hctsiz.pid; // save PID
  } else if (HCD_SPLIT && (channel->hcsplt & HCSPLT_SPLITEN)) {
    xfer->fifo_bytes = 0;
    return; // channel_iso_out_next() counts the 188-byte pieces sent
  }

  /* Since hctsiz.xfersize field reflects the number of bytes transferred via the AHB, not the USB)
   * For IN: we can use hctsiz.xfersize as remaining bytes.
   * For OUT: Must use the hctsiz.pktcnt field to determine how much data has been transferred. This field reflects the
   * number of packets that have been transferred via the USB. This is always an integral number of packets if the
   * transfer was halted before its normal completion.
   */
  const uint16_t remain_packets = hctsiz.packet_count;
  const uint16_t total_packets = channel_packet_count(edpt, hcchar.ep_size);
  const uint16_t actual_bytes = (total_packets - remain_packets) * hcchar.ep_size;

  xfer->fifo_bytes = 0;
  xfer->xferred_bytes += actual_bytes;
  edpt->buffer += actual_bytes;
  edpt->buflen -= actual_bytes;
}

#if CFG_TUH_DWC2_SLAVE_ENABLE
static bool channel_txfifo_write(dwc2_regs_t* dwc2, uint8_t ch_id, bool is_periodic);
#endif
#if HCD_SPLIT
static void split_iso_out_close(dwc2_regs_t* dwc2, uint8_t ch_id);
#endif
static void periodic_xfer_defer(dwc2_regs_t* dwc2, hcd_endpoint_t* edpt, bool next_interval);

static bool channel_xfer_start(dwc2_regs_t* dwc2, uint8_t ch_id, bool defer_periodic_out) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_char_t* hcchar_bm = &edpt->hcchar_bm;
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  bool const is_period = channel_is_periodic(edpt->hcchar);
  bool const split_iso = edpt->hcsplt_bm.split_en && hcchar_bm->ep_type == HCCHAR_EPTYPE_ISOCHRONOUS;
  (void) split_iso; // slave mode and isochronous split
  // a new periodic split transfer starts only in its start-split microframe, or after the END of the isochronous OUT
  // packet it was held behind
  const uint8_t split_ss = (HCD_SPLIT && is_period && edpt->hcsplt_bm.split_en && defer_periodic_out)
                             ? (edpt->split_held ? (uint8_t) HCD_SPLIT_SLOT_LATE : split_ss_uframe(edpt))
                             : (uint8_t) HCD_SPLIT_SLOT_NONE;
  uint8_t* dma_buf = edpt->buffer;
  const uint8_t saved_pid = edpt->next_pid;
  const uint8_t saved_do_ping = edpt->next_do_ping;
  uint16_t periodic_frame = 0;
  // clear previous state
  xfer->fifo_bytes = 0;

  // hchar: restore but don't enable yet
  channel->hcchar = (edpt->hcchar & ~HCCHAR_CHENA);

  // hctsiz: zero length packet still count as 1
  const uint16_t packet_count = channel_packet_count(edpt, hcchar_bm->ep_size);
  dwc2_channel_tsize_t hctsiz = {.value = 0};
  hctsiz.pid = edpt->next_pid; // next PID is set in transfer complete interrupt
  hctsiz.packet_count = packet_count;
  hctsiz.xfer_size = edpt->buflen;
  if (edpt->next_do_ping && edpt->speed == TUSB_SPEED_HIGH &&
     edpt->next_pid != HCTSIZ_PID_SETUP && hcchar_bm->ep_dir == TUSB_DIR_OUT) {
    hctsiz.do_ping = 1;
  }
  uint32_t hcsplt = edpt->hcsplt;
#if HCD_SPLIT
  if (split_iso) {
    dwc2_channel_split_t hcsplt_bm = {.value = hcsplt};
    if (hcchar_bm->ep_dir == TUSB_DIR_OUT) {
      // the first 188-byte start-split, its completion sends the rest (Programming Guide "Normal Isochronous OUT
      // Split Operation" programs MPS with 188 bytes)
      dwc2_channel_char_t hcchar_split = {.value = channel->hcchar};
      hcchar_split.ep_size = tu_min16(hcchar_bm->ep_size, HCD_SPLIT_ISO_PAYLOAD);
      channel->hcchar      = hcchar_split.value;
      hctsiz.xfer_size     = tu_min16(edpt->buflen, HCD_SPLIT_ISO_PAYLOAD);
      hctsiz.packet_count  = 1;
      hcsplt_bm.xact_pos   = (edpt->buflen <= HCD_SPLIT_ISO_PAYLOAD) ? HCD_SPLIT_XACTPOS_ALL : HCD_SPLIT_XACTPOS_BEGIN;
    } else {
      hcsplt_bm.xact_pos = HCD_SPLIT_XACTPOS_ALL;
  #if HCD_SPLIT_ISO_BOUNCE
      if (edpt->split_bounce) {
        hctsiz.xfer_size = split_bounce_len(edpt, 0);
        dma_buf          = split_bounce_buf(edpt);
      }
  #endif
    }
    hcsplt = hcsplt_bm.value;
  } else if (packet_count == 1) {
    hctsiz.xfer_size = tu_min16(edpt->buflen, hcchar_bm->ep_size); // a split OUT sends one packet here
  }
#endif
  channel->hctsiz = hctsiz.value;
  edpt->next_do_ping = 0;

  // Single-transaction isochronous endpoints always use DATA0. Pre-calculate the next PID for other endpoints,
  // adjusted in the transfer-complete interrupt if a short packet is received.
  if (hcchar_bm->ep_num == 0) {
    edpt->next_pid = HCTSIZ_PID_DATA1; // control data and status stage always start with DATA1
  } else if (hcchar_bm->ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
    edpt->next_pid = cal_next_pid(edpt->next_pid, packet_count);
  }

  channel->hcsplt = hcsplt;
  channel->hcint = 0xFFFFFFFFU; // clear all channel interrupts
  dwc2->gintmsk |= GINTSTS_HCINT;

  if (dma_host_enabled(dwc2)) {
    channel->hcintmsk = HCINT_HALTED;
    dwc2->haintmsk |= TU_BIT(ch_id);

    channel->hcdma = (uintptr_t) dma_buf;

    if (hcchar_bm->ep_dir == TUSB_DIR_IN) {
      periodic_frame = channel_send_in_token(dwc2, channel, is_period, split_ss);
    } else {
#if DWC2_LS_ONE_XACT_PER_FRAME
      // Claim and enable in one go, see channel_send_in_token(). Without a claim here a SETUP would leave the
      // frame free for the next stage of its own control transfer.
      const uint32_t gahbcfg = dwc2->gahbcfg;
      dwc2->gahbcfg = gahbcfg & ~GAHBCFG_GINT;
      if (!ls_frame_claim(dwc2, channel)) {
        edpt->next_pid = saved_pid;
        edpt->next_do_ping = saved_do_ping;
        channel_park(dwc2, channel, defer_periodic_out ? PARK_XFER_START_PERIODIC : PARK_XFER_START);
        dwc2->gahbcfg = gahbcfg;
        return true; // accepted, SOF will start it
      }
#endif
      hcd_dcache_clean(edpt->buffer, edpt->buflen);
      periodic_frame = channel_enable(dwc2, channel, is_period, split_ss);
#if DWC2_LS_ONE_XACT_PER_FRAME
      dwc2->gahbcfg = gahbcfg;
#endif
    }
  }
#if CFG_TUH_DWC2_SLAVE_ENABLE
  else {
    uint32_t hcintmsk = HCINT_NAK | HCINT_XACT_ERR | HCINT_STALL |
                        HCINT_XFER_COMPLETE | HCINT_DATATOGGLE_ERR;
    if (is_period) {
      hcintmsk |= HCINT_FARME_OVERRUN;
    }
    if (hcchar_bm->ep_dir == TUSB_DIR_IN) {
      hcintmsk |= HCINT_BABBLE_ERR | HCINT_DATATOGGLE_ERR | HCINT_ACK;
    } else {
      hcintmsk |= HCINT_NYET;
      if ((edpt->hcsplt_bm.split_en && !split_iso) || hctsiz.do_ping) {
        hcintmsk |= HCINT_ACK; // an isochronous OUT start-split completes with XferCompl
      }
    }
    channel->hcintmsk = hcintmsk;
    dwc2->haintmsk |= TU_BIT(ch_id);

    // enable channel for slave mode:
    // - OUT: it will enable corresponding FIFO channel
    // - IN : it will write an IN request to the Non-periodic Request Queue, this will have dwc2 trying to send
    // IN Token. If we got NAK, we have to re-enable the channel again in the interrupt. Due to the way usbh stack only
    // call hcd_edpt_xfer() once, we will need to manage de-allocate/re-allocate IN channel dynamically.
    if (hcchar_bm->ep_dir == TUSB_DIR_IN) {
      periodic_frame = channel_send_in_token(dwc2, channel, is_period, split_ss);
    } else {
      // The final FIFO word creates the OUT request. Keep CHENA and that write
      // atomic with respect to this controller's ISR.
      // This region never waits for FIFO or queue space.
      const uint32_t gahbcfg = dwc2->gahbcfg;
      dwc2->gahbcfg = gahbcfg & ~GAHBCFG_GINT;
      if (defer_periodic_out && is_period) {
        const dwc2_hfnum_t hfnum = {.value = dwc2->hfnum};
        if (hfnum.remainning < DWC2_PERIODIC_OUT_MIN_FRREM) {
          edpt->next_pid = saved_pid;
          edpt->next_do_ping = saved_do_ping;
          dwc2->gahbcfg = gahbcfg;
          return false;
        }
      }
#if DWC2_LS_ONE_XACT_PER_FRAME
      // Must come before the enable: the final FIFO word below creates the OUT request, so a channel
      // parked here would post one without ever being enabled. Retreat the way the check above does
      // and let SOF run this transfer from the top.
      if (!ls_frame_claim(dwc2, channel)) {
        edpt->next_pid = saved_pid;
        edpt->next_do_ping = saved_do_ping;
        channel_park(dwc2, channel, defer_periodic_out ? PARK_XFER_START_PERIODIC : PARK_XFER_START);
        dwc2->gahbcfg = gahbcfg;
        return true; // accepted, SOF will start it
      }
#endif
      periodic_frame = channel_enable(dwc2, channel, is_period, split_ss);
      // an isochronous split BEGIN is written after the periodic writes pending ahead of it, by FIFO-empty interrupt
      const bool behind = HCD_SPLIT && split_iso && is_period && (dwc2->gintmsk & GINTSTS_PTX_FIFO_EMPTY);
      if (periodic_frame != HCD_PERIODIC_FRAME_NOT_STARTED && edpt->buflen > 0 &&
          (behind || channel_txfifo_write(dwc2, ch_id, is_period))) {
        // The FIFO-empty interrupt handles only work that did not fit in the
        // initial synchronous write.
        dwc2->gintmsk |= (is_period ? GINTSTS_PTX_FIFO_EMPTY : GINTSTS_NPTX_FIFO_EMPTY);
      }
      dwc2->gahbcfg = gahbcfg;
    }
  }
#endif

  if (periodic_frame == HCD_PERIODIC_FRAME_NOT_STARTED) {
    // too late in the frame for a periodic start-split: the caller defers it to a later microframe
    edpt->next_pid = saved_pid;
    edpt->next_do_ping = saved_do_ping;
    return false;
  }

#if DWC2_LS_ONE_XACT_PER_FRAME
  if (periodic_frame == DWC2_LS_PARKED) {
    return true; // IN token parked, SOF replays it and picks up periodic_frame then
  }
#endif

  if (is_period && defer_periodic_out && split_ss == HCD_SPLIT_SLOT_NONE) {
    edpt->periodic_frame = periodic_frame; // a start-split recorded the frame of its slot
  }

  return true;
}

// kick-off transfer with an endpoint. The ISR also allocates channels, and may handle the started transfer before this
// returns when called from task context: claim the channel and write the endpoint state ahead of the start, with the
// interrupt masked. The start itself may wait for request queue space, which slave mode only gets with the interrupt.
static bool edpt_xfer_kickoff(dwc2_regs_t* dwc2, uint8_t ep_id) {
  hcd_endpoint_t* edpt      = &_hcd_data.edpt[ep_id];
  const bool      is_period = channel_is_periodic(edpt->hcchar);
  uint32_t        gahbcfg   = dwc2->gahbcfg;
  dwc2->gahbcfg             = gahbcfg & ~GAHBCFG_GINT;
  const uint8_t ch_id       = channel_alloc(dwc2);
  const bool    phase       = edpt->periodic_phase;
  if (ch_id < 16) {
    hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
    xfer->ep_id = ep_id;
    xfer->result = XFER_RESULT_INVALID;
    xfer->err_count = edpt->periodic_err; // a periodic retry keeps the errors of its transfer
#if HCD_SPLIT
    xfer->xferred_bytes = edpt->periodic_xferred; // and the bytes a split OUT one already sent
#endif
    if (is_period) {
      edpt->periodic_phase = 1;
      edpt->xfer_pending = 0;
    }
  }
  dwc2->gahbcfg = gahbcfg;
  TU_ASSERT(ch_id < 16); // all channel are in used

  if (!channel_xfer_start(dwc2, ch_id, true)) {
    gahbcfg       = dwc2->gahbcfg;
    dwc2->gahbcfg = gahbcfg & ~GAHBCFG_GINT;
    edpt->periodic_phase = phase;
    channel_dealloc(dwc2, ch_id);
    periodic_xfer_defer(dwc2, edpt, false);
    dwc2->gahbcfg = gahbcfg;
  }
  return true;
}

static uint32_t periodic_xfer_countdown(dwc2_regs_t* dwc2, hcd_endpoint_t const* edpt) {
  const uint32_t ucount = (hprt_speed_get(dwc2) == TUSB_SPEED_HIGH) ? 1u : 8u;
  const uint16_t frame = (uint16_t) (dwc2->hfnum & HCD_FRAME_NUMBER_MASK);
  const uint16_t elapsed_frames = (uint16_t) (frame - edpt->periodic_frame) & HCD_FRAME_NUMBER_MASK;
  const uint32_t elapsed_uframes = (uint32_t) elapsed_frames * ucount;

  if (elapsed_uframes < edpt->uframe_interval) {
    return edpt->uframe_interval - elapsed_uframes - ucount;
  }

  // The service opportunity was missed. Keep the established phase and use
  // the next interval rather than starting a new interval from this request.
  return edpt->uframe_interval - (elapsed_uframes % edpt->uframe_interval) - ucount;
}

// Defer a periodic transfer to the SOF interrupt: the next SOF, or with next_interval the endpoint's next service
// interval. Its countdown is taken once the SOF interrupt is armed, so that no SOF goes uncounted.
static void periodic_xfer_defer(dwc2_regs_t* dwc2, hcd_endpoint_t* edpt, bool next_interval) {
  const uint32_t gahbcfg = dwc2->gahbcfg;
  dwc2->gahbcfg = gahbcfg & ~GAHBCFG_GINT;

  if (0 == (dwc2->gintmsk & GINTMSK_SOFM)) {
    dwc2->gintsts = GINTSTS_SOF;
    dwc2->gintmsk |= GINTMSK_SOFM;
  }

  edpt->uframe_countdown = next_interval ? periodic_xfer_countdown(dwc2, edpt) : 0;
  edpt->xfer_pending = 1;

  dwc2->gahbcfg = gahbcfg;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);

  uint8_t ep_id = edpt_find_opened(dev_addr, ep_num, ep_dir, false);
  TU_VERIFY(ep_id < CFG_TUH_DWC2_ENDPOINT_MAX);
  hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];
  TU_VERIFY(edpt->closing == 0 && edpt->aborting == 0); // skip if endpoint is closing or aborting

  edpt->buffer = buffer;
  edpt->buflen = buflen;
  edpt->periodic_err = 0; // idle endpoint: the ISR does not write this word
#if HCD_SPLIT
  edpt->periodic_xferred = 0;
#endif

  if (ep_num == 0) {
    // update ep_dir since control endpoint can switch direction
    edpt->hcchar_bm.ep_dir = ep_dir;
  }

  if (HCD_SPLIT && channel_is_periodic(edpt->hcchar) && edpt->hcsplt_bm.split_en) {
    // Started in its start-split microframe: from here when that is the next one, otherwise by the SOF interrupt, or
    // after the END of an isochronous OUT packet that holds it
    const uint32_t gahbcfg = dwc2->gahbcfg;
    dwc2->gahbcfg          = gahbcfg & ~GAHBCFG_GINT;
    periodic_xfer_defer(dwc2, edpt, edpt->periodic_phase);
    const bool start = edpt->uframe_countdown == 0 && ((dwc2->hfnum + 1u) & 7u) == split_ss_uframe(edpt) &&
                       !split_ss_hold(dwc2, edpt);
    if (start) {
      edpt->xfer_pending = 0;
    }
    dwc2->gahbcfg = gahbcfg;
    if (start && !edpt_xfer_kickoff(dwc2, ep_id)) {
      periodic_xfer_defer(dwc2, edpt, false); // no channel free, try from SOF
    }
    return true;
  }

  if (channel_is_periodic(edpt->hcchar)) {
    const uint32_t ucount = (hprt_speed_get(dwc2) == TUSB_SPEED_HIGH) ? 1u : 8u;
#if CFG_TUH_DWC2_SLAVE_ENABLE
    // Establish a slower slave-mode OUT schedule from SOF. bInterval=1 must be queued immediately to avoid
    // losing every other service opportunity.
    if (!dma_host_enabled(dwc2) && ep_dir == TUSB_DIR_OUT && !edpt->periodic_phase &&
        edpt->uframe_interval > ucount) {
      periodic_xfer_defer(dwc2, edpt, false);
      return true;
    }
#endif
    if (edpt->periodic_phase && edpt->uframe_interval > ucount) {
      const uint32_t countdown = periodic_xfer_countdown(dwc2, edpt);
      if (countdown > 0) {
        periodic_xfer_defer(dwc2, edpt, true);
        return true;
      }
    }
  }

  return edpt_xfer_kickoff(dwc2, ep_id);
}

// Abort a queued transfer. Note: it can only abort transfer that has not been started
// Return true if a queued transfer is aborted, false if there is no transfer to abort
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);
  const uint8_t ep_id = edpt_find_opened(dev_addr, ep_num, ep_dir, false);
  TU_VERIFY(ep_id < CFG_TUH_DWC2_ENDPOINT_MAX);
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];

  while (1) {
    hcd_int_disable(rhport);
    if (edpt->xfer_pending) {
      edpt->xfer_pending     = 0;
      edpt->uframe_countdown = 0;
      edpt->split_held       = 0;
      break;
    }
    const uint8_t ch_id = channel_find_enabled(dwc2, dev_addr, ep_num, ep_dir);
    if (ch_id >= DWC2_CHANNEL_COUNT_MAX || channel_teardown(dwc2, ch_id, false) != CHANNEL_TEARDOWN_WAIT) {
      break;
    }
    hcd_int_enable(rhport);
    channel_disable_wait(dwc2, &dwc2->channel[ch_id]);
  }

  hcd_int_enable(rhport);
  return true;
}

// Submit a special transfer to send 8-byte Setup Packet, when complete hcd_event_xfer_complete() must be invoked
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
  uint8_t ep_id = edpt_find_opened(dev_addr, 0, TUSB_DIR_OUT, false);
  TU_VERIFY(ep_id < CFG_TUH_DWC2_ENDPOINT_MAX); // endpoint can close asynchronously on disconnect
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  edpt->next_pid = HCTSIZ_PID_SETUP;

  return hcd_edpt_xfer(rhport, dev_addr, 0, (uint8_t*)(uintptr_t) setup_packet, 8);
}

// clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const uint8_t ep_dir = tu_edpt_dir(ep_addr);
  const uint8_t ep_id = edpt_find_opened(dev_addr, ep_num, ep_dir, false);
  TU_VERIFY(ep_id < CFG_TUH_DWC2_ENDPOINT_MAX);
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];

  edpt->next_pid = HCTSIZ_PID_DATA0;

  return true;
}

//--------------------------------------------------------------------
// HCD Event Handler
//--------------------------------------------------------------------

// Retry an IN transfer, channel must be halted. Return true when the transfer completes instead: an isochronous split
// IN is not retried once its complete-split window has passed.
static bool channel_xfer_in_retry(dwc2_regs_t* dwc2, uint8_t ch_id, uint32_t hcint) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  dwc2_channel_char_t hcchar = {.value = channel->hcchar};

  if (channel_is_periodic(hcchar.value)){
    const dwc2_channel_split_t hcsplt = {.value = channel->hcsplt};
    // periodic split NYET: next complete-split in the following microframe, while its window lasts
    if (HCD_SPLIT && hcsplt.split_en && hcsplt.split_compl && (hcint & HCINT_NYET || xfer->halted_nyet)) {
      xfer->halted_nyet = 0;
      if (channel_csplit_arm(dwc2, ch_id)) {
        return false;
      }
      // window passed, de-allocate channel with below code
    }
    if (HCD_SPLIT && hcsplt.split_en && hcchar.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
      xfer->result = XFER_RESULT_FAILED;
      return true;
    }

    const uint32_t ucount = (hprt_speed_get(dwc2) == TUSB_SPEED_HIGH ? 1 : 8);
    if (edpt->uframe_interval == ucount) {
      // retry on next frame if bInterval is 1
      hcchar.odd_frame = 1 - (dwc2->hfnum & 1);
      channel->hcchar = hcchar.value;
      channel_send_in_token(dwc2, channel, false, HCD_SPLIT_SLOT_NONE);
    } else {
      // otherwise, de-allocate channel, enable SOF set frame counter for later transfer
      const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
      if (hcchar.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
        edpt->next_pid = hctsiz.pid; // save PID
      }
      edpt->periodic_err = xfer->err_count;
      periodic_xfer_defer(dwc2, edpt, true);
      // already halted, de-allocate channel (called from DMA isr)
      channel_dealloc(dwc2, ch_id);
    }
  } else {
#if HCD_SPLIT
    const dwc2_channel_split_t hcsplt = {.value = channel->hcsplt};
    if (hcsplt.split_en && hcsplt.split_compl && ((hcint & HCINT_NYET) || xfer->halted_nyet)) {
      xfer->halted_nyet = 0;
      split_csplit_park(dwc2, channel); // the hub is still on it
      return false;
    }
#endif
    // for control/bulk: retry immediately
    channel_send_in_token(dwc2, channel, false, HCD_SPLIT_SLOT_NONE);
  }
  return false;
}

// Restart an OUT transfer after a NAK or error, channel must be halted. A periodic split starts over in its slot of
// the next service interval, keeping the errors of its transfer.
static void channel_xfer_out_restart(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t*     xfer = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  if (HCD_SPLIT && channel_is_periodic(edpt->hcchar) && edpt->hcsplt_bm.split_en) {
    edpt->periodic_err = xfer->err_count;
  #if HCD_SPLIT
    // the channel is released: keep what the packets sent so far count. An isochronous packet starts over at BEGIN.
    edpt->periodic_xferred =
        (edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) ? 0 : xfer->xferred_bytes;
  #endif
    periodic_xfer_defer(dwc2, edpt, true);
  #if HCD_SPLIT
    split_iso_out_close(dwc2, ch_id);
  #endif
    channel_dealloc(dwc2, ch_id);
  } else {
    channel_xfer_start(dwc2, ch_id, false);
  }
}

#if HCD_SPLIT
// A packet of a split control, bulk or interrupt OUT transfer completed, channel halted. With more data to send, the
// next packet follows (one packet per split transaction, see channel_packet_count()): an interrupt one in the
// endpoint's slot of the next service interval, a control or bulk one now. Return true if the transfer continues.
static bool channel_split_out_more(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t*     xfer = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  const uint16_t  mps  = edpt->hcchar_bm.ep_size;
  if (!edpt->hcsplt_bm.split_en || edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS || edpt->buflen <= mps ||
      xfer->closing || xfer->aborting) {
    return false; // complete, or torn down: the transfer ends here
  }
  const dwc2_channel_tsize_t hctsiz = {.value = dwc2->channel[ch_id].hctsiz};
  edpt->next_pid = hctsiz.pid; // toggled by the core; the rest of a control data stage does not restart at DATA1
  xfer->xferred_bytes += mps;
  xfer->fifo_bytes = 0;
  xfer->err_count  = 0;
  edpt->buffer += mps;
  edpt->buflen -= mps;
  channel_xfer_out_restart(dwc2, ch_id);
  return true;
}
#endif

// A periodic split transaction missed its complete-split window. An isochronous IN packet fails; another transaction
// is retried in the next service interval with the data toggle it had. A channel still enabled (slave mode) is
// disabled first, its halt interrupt carrying on. Return true when the transfer is complete.
static bool channel_split_missed(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t*               xfer    = &_hcd_data.xfer[ch_id];
  dwc2_channel_t*           channel = &dwc2->channel[ch_id];
  const dwc2_channel_char_t hcchar  = {.value = channel->hcchar};
  if (hcchar.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
    xfer->result = XFER_RESULT_FAILED;
  } else if (hcchar.ep_dir == TUSB_DIR_OUT) {
    channel_xfer_out_wrapup(dwc2, ch_id);
  }
  if (hcchar.enable) {
    channel_disable(dwc2, channel);
    return false;
  }
  if (xfer->result != XFER_RESULT_INVALID) {
    return true;
  }
  if (hcchar.ep_dir == TUSB_DIR_IN) {
    return channel_xfer_in_retry(dwc2, ch_id, 0);
  }
  channel_xfer_out_restart(dwc2, ch_id);
  return false;
}

#if HCD_SPLIT
// The END start-split of the isochronous OUT packet on channel ch_id is queued, or the packet ended: start the
// interrupt start-splits held behind it on its TT, which queue after that END
static void split_iso_out_close(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  if (!xfer->iso_open) {
    return;
  }
  xfer->iso_open = 0;
  const hcd_endpoint_t* iso = &_hcd_data.edpt[xfer->ep_id];
  for (uint8_t ep_id = 0; ep_id < CFG_TUH_DWC2_ENDPOINT_MAX; ep_id++) {
    hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
    if (edpt->split_held && edpt->xfer_pending && edpt->closing == 0 && edpt->uframe_countdown == 0 &&
        split_tt_same(edpt, iso) && !split_iso_out_open(dwc2, edpt)) {
      (void) edpt_xfer_kickoff(dwc2, ep_id);
      _hcd_split_kicked = true;
    }
  }
}

// The request of the current start-split of the isochronous OUT packet on channel ch_id is queued
static void split_iso_out_queued(dwc2_regs_t* dwc2, uint8_t ch_id) {
  const hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  if (xfer->iso_open && xfer->xferred_bytes + HCD_SPLIT_ISO_PAYLOAD >= _hcd_data.edpt[xfer->ep_id].buflen) {
    split_iso_out_close(dwc2, ch_id); // it is the END
  }
}

// After a start-split of an isochronous split OUT packet, send the next 188 bytes in the following microframe. Return
// false when the packet is complete, or failed because that microframe has passed (xfer->result tells).
static bool channel_iso_out_next(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t*           xfer    = &_hcd_data.xfer[ch_id];
  const hcd_endpoint_t* edpt    = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_t*       channel = &dwc2->channel[ch_id];

  xfer->xferred_bytes += tu_min16(edpt->buflen - xfer->xferred_bytes, HCD_SPLIT_ISO_PAYLOAD);
  if (xfer->xferred_bytes >= edpt->buflen) {
    xfer->result = XFER_RESULT_SUCCESS;
    split_iso_out_close(dwc2, ch_id);
    return false;
  }

  // start-splits sent so far: each but the last carries 188 bytes
  const uint16_t target = xfer->xferred_bytes / HCD_SPLIT_ISO_PAYLOAD;
  if (split_iso_out_missed(dwc2, (uint16_t) (xfer->split_uframe + target))) {
    xfer->result = XFER_RESULT_FAILED; // the TT discards an incomplete packet
    split_iso_out_close(dwc2, ch_id);
    return false;
  }

  const uint16_t       remain = edpt->buflen - xfer->xferred_bytes;
  dwc2_channel_tsize_t hctsiz = {.value = 0};
  hctsiz.pid                  = HCTSIZ_PID_DATA0;
  hctsiz.packet_count         = 1;
  hctsiz.xfer_size            = tu_min16(remain, HCD_SPLIT_ISO_PAYLOAD);
  channel->hctsiz             = hctsiz.value;

  dwc2_channel_split_t hcsplt = {.value = channel->hcsplt};
  hcsplt.xact_pos             = (remain <= HCD_SPLIT_ISO_PAYLOAD) ? HCD_SPLIT_XACTPOS_END : HCD_SPLIT_XACTPOS_MID;
  channel->hcsplt             = hcsplt.value;

  const uint32_t odd = (uint32_t) ((xfer->split_uframe + target) & 1u);
  channel->hcchar    = (channel->hcchar & ~(HCCHAR_CHENA | HCCHAR_CHDIS | HCCHAR_ODDFRM)) | (odd << HCCHAR_ODDFRM_Pos);
  // the completion this start-split continues left ChHltd latched in slave mode
  channel->hcint = HCINT_HALTED;
  if (dma_host_enabled(dwc2)) {
    channel->hcdma = (uintptr_t) (edpt->buffer + xfer->xferred_bytes);
    channel->hcchar |= HCCHAR_CHENA;
    split_iso_out_queued(dwc2, ch_id);
  }
  #if CFG_TUH_DWC2_SLAVE_ENABLE
  else {
    // the final FIFO word creates the OUT request; a piece the FIFO-empty interrupt writes is checked there
    xfer->fifo_bytes = xfer->xferred_bytes;
    channel->hcchar |= HCCHAR_CHENA;
    if (channel_txfifo_write(dwc2, ch_id, true)) {
      dwc2->gintmsk |= GINTSTS_PTX_FIFO_EMPTY;
    } else {
      split_iso_out_queued(dwc2, ch_id);
    }
  }
  #endif
  return true;
}

  #if CFG_TUH_DWC2_SLAVE_ENABLE
// A piece of a split isochronous OUT packet still to be written to the FIFO must go out in its own microframe. Once
// that has passed, fail the packet: its halt completes it. Return true when failed.
static bool channel_iso_out_late(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t*               xfer    = &_hcd_data.xfer[ch_id];
  const hcd_endpoint_t*     edpt    = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_t*           channel = &dwc2->channel[ch_id];
  const dwc2_channel_char_t hcchar  = {.value = channel->hcchar};
  if (0 == (channel->hcsplt & HCSPLT_SPLITEN) || hcchar.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS ||
      xfer->fifo_bytes >= tu_min16(edpt->buflen, xfer->xferred_bytes + HCD_SPLIT_ISO_PAYLOAD)) {
    return false; // its piece is written, or empty
  }
  if (!split_iso_out_missed(dwc2, (uint16_t) (xfer->split_uframe + xfer->xferred_bytes / HCD_SPLIT_ISO_PAYLOAD))) {
    return false;
  }
  xfer->result     = XFER_RESULT_FAILED;
  channel_out_expect_posted(dwc2, ch_id); // its piece is not in the FIFO
  xfer->fifo_bytes = 0;
  channel_disable(dwc2, channel);
  split_iso_out_close(dwc2, ch_id);
  return true;
}

// Slave mode: an interrupt split OUT whose start-split packet is not yet written when its microframe has passed would
// go out late, its complete-splits timed from that microframe. Disable it instead (nothing of it is queued); its halt
// restarts the transfer in the next service interval, see channel_xfer_out_restart().
static bool channel_split_out_late(dwc2_regs_t* dwc2, uint8_t ch_id) {
  const hcd_xfer_t*         xfer    = &_hcd_data.xfer[ch_id];
  dwc2_channel_t*           channel = &dwc2->channel[ch_id];
  const dwc2_channel_char_t hcchar  = {.value = channel->hcchar};
  if ((channel->hcsplt & (HCSPLT_SPLITEN | HCSPLT_COMPLSPLT)) != HCSPLT_SPLITEN ||
      hcchar.ep_type != HCCHAR_EPTYPE_INTERRUPT || xfer->split_issued ||
      uframe_ahead(xfer->split_uframe, dwc2->hfnum) >= 0) {
    return false;
  }
  const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
  _hcd_data.edpt[xfer->ep_id].next_pid = hctsiz.pid; // the packet did not go out: it keeps its data toggle
  channel_out_expect_posted(dwc2, ch_id); // nothing of it is in the FIFO
  channel_disable(dwc2, channel);
  return true;
}
  #endif

// A complete-split of an isochronous split IN packet returned data. Its MDATA PID continues the packet: arm the next
// complete-split for the rest. Return false when the packet is complete, with xfer->result failed if DATA0 did not come
// before its last complete-split (USB 2.0 Figure 11-89).
static bool channel_iso_in_more(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t*                xfer    = &_hcd_data.xfer[ch_id];
  const hcd_endpoint_t*      edpt    = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_t*            channel = &dwc2->channel[ch_id];
  const dwc2_channel_tsize_t done    = {.value = channel->hctsiz};
  if (done.pid != HCTSIZ_PID_MDATA) {
    xfer->result = XFER_RESULT_SUCCESS;
    return false;
  }
  if (xfer->xferred_bytes >= edpt->buflen) {
    xfer->result = XFER_RESULT_FAILED; // more data than the buffer takes, babble for a non-split packet
    return false;
  }

  dwc2_channel_tsize_t hctsiz = {.value = 0};
  hctsiz.pid                  = HCTSIZ_PID_DATA0;
  hctsiz.packet_count         = 1;
  hctsiz.xfer_size            = edpt->buflen - xfer->xferred_bytes;
  #if HCD_SPLIT_ISO_BOUNCE
  if (edpt->split_bounce) {
    hctsiz.xfer_size = split_bounce_len(edpt, xfer->xferred_bytes);
    channel->hcdma   = (uintptr_t) split_bounce_buf(edpt);
  }
  #endif
  channel->hctsiz = hctsiz.value;
  if (channel_csplit_arm(dwc2, ch_id)) {
    return true;
  }
  xfer->result = XFER_RESULT_FAILED;
  return false;
}
#endif

#if CFG_TUSB_DEBUG && 0
TU_ATTR_ALWAYS_INLINE static inline void print_hcint(uint32_t hcint) {
  const char* str[] = {
    "XFRC", "HALTED", "AHBERR", "STALL",
    "NAK", "ACK", "NYET", "XERR",
    "BBLERR", "FRMOR", "DTERR", "BNA",
    "XCSERR", "DESC_LST"
  };

  for(uint32_t i=0; i<14; i++) {
    if (hcint & TU_BIT(i)) {
      TU_LOG1("%s ", str[i]);
    }
  }
  TU_LOG1("\r\n");
}
#endif

#if CFG_TUH_DWC2_SLAVE_ENABLE
static void handle_rxflvl_irq(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  // Pop control word off FIFO
  const dwc2_grxstsp_t grxstsp = {.value= dwc2->grxstsp};
  const uint8_t ch_id = grxstsp.ep_ch_num;

  switch (grxstsp.packet_status) {
    case GRXSTS_PKTSTS_RX_DATA: {
      // In packet received, pop this entry --> ACK interrupt
      const uint16_t byte_count = grxstsp.byte_count;
      hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
      if (!xfer->allocated) {
        // Discard data for a channel retired by disconnect.
        for (uint16_t count = 0; count < byte_count; count += sizeof(uint32_t)) {
          (void) dwc2->fifo[0][0];
        }
        break;
      }
      TU_ASSERT(xfer->ep_id < CFG_TUH_DWC2_ENDPOINT_MAX,);
      hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];

      if (byte_count > 0) {
        tu_hwfifo_read(dwc2->fifo[0], edpt->buffer + xfer->xferred_bytes, byte_count, NULL);
        xfer->xferred_bytes += byte_count;
        xfer->fifo_bytes = byte_count;
      }
      break;
    }

    case GRXSTS_PKTSTS_RX_COMPLETE:
      // In transfer complete: After this entry is popped from the rx FIFO, dwc2 asserts a Transfer Completed
      // interrupt --> handle_channel_irq()
      break;

    case GRXSTS_PKTSTS_HOST_DATATOGGLE_ERR:
      // handle in channel interrupt
      break;

    case GRXSTS_PKTSTS_HOST_CHANNEL_HALTED:
      // triggered when channel.hcchar_bm.disable is set
      // TODO handle later
      break;

    default: break; // ignore other status
  }
}

// Return true if data remains for a later FIFO-empty interrupt.
static bool channel_txfifo_write(dwc2_regs_t* dwc2, uint8_t ch_id, bool is_periodic) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  const dwc2_channel_char_t hcchar = {.value = channel->hcchar};
  TU_ASSERT(xfer->ep_id < CFG_TUH_DWC2_ENDPOINT_MAX);
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
  const uint16_t remain_packets = hctsiz.packet_count;
  uint16_t buflen = edpt->buflen;
#if HCD_SPLIT
  if ((channel->hcsplt & HCSPLT_SPLITEN) && hcchar.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
    buflen = tu_min16(buflen, xfer->xferred_bytes + HCD_SPLIT_ISO_PAYLOAD); // only the piece of this start-split
  } else if (channel->hcsplt & HCSPLT_SPLITEN) {
    buflen = tu_min16(buflen, hcchar.ep_size); // only the packet of this start-split, see channel_packet_count()
  }
#endif

  for (uint16_t i = 0; i < remain_packets; i++) {
    const uint16_t remain_bytes = buflen - xfer->fifo_bytes;
    const uint16_t xact_bytes = tu_min16(remain_bytes, hcchar.ep_size);

    // The packet's last FIFO word creates its request-queue entry.
    // HNPTXSTS differs by one request-queue bit, which is outside these fields.
    const dwc2_hptxsts_t txsts = {.value = (is_periodic ? dwc2->hptxsts : dwc2->hnptxsts)};
    if ((xact_bytes > (txsts.fifo_available << 2)) || (txsts.req_queue_available == 0)) {
      return true;
    }

    tu_hwfifo_write(dwc2->fifo[ch_id], edpt->buffer + xfer->fifo_bytes, xact_bytes, NULL);
    xfer->fifo_bytes += xact_bytes;
#if HCD_SPLIT
    if (is_periodic && xact_bytes > 0 && !xfer->split_issued &&
        (channel->hcsplt & (HCSPLT_SPLITEN | HCSPLT_COMPLSPLT)) == HCSPLT_SPLITEN) {
      split_ss_issued(dwc2, ch_id);
    }
#endif
  }

  return false;
}

// Return true if at least one matching channel needs another interrupt.
static bool handle_txfifo_empty(dwc2_regs_t* dwc2, bool is_periodic) {
  const uint8_t max_channel = dwc2_channel_count(dwc2);
#if HCD_SPLIT
  _hcd_split_kicked = false;
#endif
  // Periodic: the interrupt channels before the isochronous ones (second pass), so that an interrupt start-split
  // written here keeps its place ahead of an isochronous BEGIN
  const uint8_t passes = (HCD_SPLIT && is_periodic) ? 2u : 1u;
  for (uint16_t i = 0; i < (uint16_t) (passes * max_channel); i++) {
    const uint8_t ch_id = (uint8_t) (i % max_channel);
    hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
    dwc2_channel_t* channel = &dwc2->channel[ch_id];
    const dwc2_channel_char_t hcchar = {.value = channel->hcchar};
#if HCD_CHANNEL_PARK
    if (xfer->park != PARK_NONE) {
      continue; // parked: the final FIFO word would post a request for a channel that is not enabled
    }
#endif
    // a channel still being set up up to its CHENA writes its own first packet
    if (xfer->allocated && (hcchar.value & HCCHAR_CHENA) && channel_is_periodic(hcchar.value) == is_periodic &&
        0 == (channel->hcintmsk & HCINT_HALTED) && hcchar.ep_dir == TUSB_DIR_OUT &&
        (passes == 1u || (hcchar.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) == (i >= max_channel))) {
#if HCD_SPLIT
      if (is_periodic && (channel_iso_out_late(dwc2, ch_id) || channel_split_out_late(dwc2, ch_id))) {
        continue;
      }
#endif
      if (channel_txfifo_write(dwc2, ch_id, is_periodic)) {
        return true;
      }
#if HCD_SPLIT
      if (is_periodic) {
        split_iso_out_queued(dwc2, ch_id);
      }
#endif
    }
  }

#if HCD_SPLIT
  return _hcd_split_kicked; // a start-split begun here may still have its FIFO write to do
#else
  return false;
#endif
}

static bool handle_channel_in_slave(dwc2_regs_t* dwc2, uint8_t ch_id, uint32_t hcint) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_split_t hcsplt = {.value = channel->hcsplt};
  const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
  bool is_done = false;

  // if (hcsplt.split_en) {
  // if (edpt->hcchar_bm.ep_num == 1) {
  //   TU_LOG1("Frame %u, ch %u: ep %u, hcint 0x%04lX ", dwc2->hfnum_bm.num, ch_id, hcsplt.ep_num, hcint);
  //   print_hcint(hcint);
  // }

  if (hcint & HCINT_XFER_COMPLETE) {
#if HCD_SPLIT
    if (hcsplt.split_en && edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
      return !channel_iso_in_more(dwc2, ch_id); // the core halted the completed periodic channel
    }
#endif
    if (edpt->hcchar_bm.ep_num != 0 &&
        edpt->hcchar_bm.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
      edpt->next_pid = hctsiz.pid; // save pid (already toggled)
    }

    const uint16_t remain_packets = hctsiz.packet_count;
    if (hcsplt.split_en && remain_packets && xfer->fifo_bytes == edpt->hcchar_bm.ep_size) {
      // Split can only complete 1 transaction (up to 1 packet) at a time, schedule more
      hcsplt.split_compl = 0;
      channel->hcsplt = hcsplt.value;
    } else {
      xfer->result = XFER_RESULT_SUCCESS;
    }

    if (channel_is_periodic(channel->hcchar) && remain_packets == 0) {
      // The core has already halted a completed periodic IN channel. Complete
      // it now so the next interval can be submitted without another halt IRQ.
      is_done = true;
    } else {
      channel_disable(dwc2, channel);
    }
  } else if (hcint & HCINT_FARME_OVERRUN) {
    if (edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
      xfer->result = XFER_RESULT_FAILED;
    }
    channel_disable(dwc2, channel);
  } else if (hcint & (HCINT_XACT_ERR | HCINT_BABBLE_ERR | HCINT_STALL)) {
    if (hcint & HCINT_STALL) {
      xfer->result = XFER_RESULT_STALLED;
    } else if (hcint & HCINT_BABBLE_ERR) {
      xfer->result = XFER_RESULT_FAILED;
    } else if (hcint & HCINT_XACT_ERR) {
      xfer->err_count++;
      channel->hcintmsk |= HCINT_ACK;
    } else {
      // nothing to do
    }

    channel_disable(dwc2, channel);
  } else if (hcint & HCINT_NYET) {
    // restart complete split
    hcsplt.split_compl = 1;
    channel->hcsplt = hcsplt.value;
    xfer->halted_nyet = 1;
    channel_disable(dwc2, channel);
  } else if (hcint & HCINT_NAK) {
    // NAK received, disable channel to flush all posted request and try again
    xfer->err_count = 0; // a NAK completes the transaction normally
    if (hcsplt.split_en == 1u) {
      hcsplt.split_compl = 0; // restart with start-split
      channel->hcsplt = hcsplt.value;
    }
#if HCD_CHANNEL_PARK
    if (!channel_is_periodic(channel->hcchar)) {
      (void) channel_nak_hold(dwc2, ch_id); // the halt below parks the retry when held
    }
#endif

    channel_disable(dwc2, channel);
  } else if (hcint & HCINT_ACK) {
    xfer->err_count = 0;

    if (hcsplt.split_en == 1u) {
      if (hcsplt.split_compl == 0) {
        // start split is ACK --> do complete split
        channel->hcintmsk |= HCINT_NYET;
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        if (HCD_SPLIT && channel_is_periodic(channel->hcchar)) {
          xfer->csplit_next = HCD_SPLIT_CSPLIT_FIRST;
          if (!channel_csplit_arm(dwc2, ch_id)) {
            is_done = channel_split_missed(dwc2, ch_id);
          }
        } else {
#if HCD_SPLIT
          split_csplit_park(dwc2, channel);
#endif
        }
      } else {
        // do nothing for complete split with DATA, this will trigger XferComplete and handled there
      }
    } else {
      // ACK with data
      const uint16_t remain_packets = hctsiz.packet_count;
      if (remain_packets > 0) {
        // still more packet to receive, also reset to start split
        hcsplt.split_compl = 0;
        channel->hcsplt = hcsplt.value;
        channel_send_in_token(dwc2, channel, false, HCD_SPLIT_SLOT_NONE);
      }
    }
  } else if (hcint & HCINT_HALTED) {
    channel->hcintmsk &= ~HCINT_HALTED;
    if (xfer->result != XFER_RESULT_INVALID) {
      is_done = true;
    } else if (xfer->err_count == HCD_XFER_ERROR_MAX) {
      xfer->result = XFER_RESULT_FAILED;
      is_done      = true;
    } else if (xfer->closing == 1) {
      is_done = true;
    } else {
      // got here due to NAK or NYET
#if HCD_CHANNEL_PARK
      if (xfer->nak_hold) {
        channel_park(dwc2, channel, PARK_IN_TOKEN); // max_nak reached, retry in the next frame
        return false;
      }
#endif
      is_done = channel_xfer_in_retry(dwc2, ch_id, hcint);
    }
  } else if (hcint & HCINT_DATATOGGLE_ERR) {
    channel->hcintmsk &= ~HCINT_DATATOGGLE_ERR;
    xfer->err_count = 0;
    hcsplt.split_compl = 0; // restart with start-split
    channel->hcsplt = hcsplt.value;
    channel_disable(dwc2, channel);
  } else {
    // nothing to do
  }
  return is_done;
}

static bool handle_channel_out_slave(dwc2_regs_t* dwc2, uint8_t ch_id, uint32_t hcint) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_split_t hcsplt = {.value = channel->hcsplt};
  bool is_done = false;

  if (hcint & HCINT_XFER_COMPLETE) {
#if HCD_SPLIT
    if (hcsplt.split_en && edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
      if (channel_iso_out_next(dwc2, ch_id)) {
        return false;
      }
      xfer->fifo_bytes = 0; // channel_iso_out_next() counted the bytes
      return true;
    }
    if (channel_split_out_more(dwc2, ch_id)) {
      return false;
    }
#endif
    is_done = true;
    xfer->result = XFER_RESULT_SUCCESS;
    channel->hcintmsk &= ~HCINT_ACK;
    if (hcint & HCINT_NYET) {
      // complete transfer with NYET, do ping next time
      edpt->next_do_ping = 1;
    }
  } else if (hcint & HCINT_STALL) {
    xfer->result = XFER_RESULT_STALLED;
    channel_disable(dwc2, channel);
  } else if (hcint & HCINT_FARME_OVERRUN) {
    channel_xfer_out_wrapup(dwc2, ch_id);
    if (edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
      xfer->result = XFER_RESULT_FAILED;
    }
    channel_disable(dwc2, channel);
  } else if (hcint & HCINT_NYET) {
    xfer->err_count = 0;
    if (hcsplt.split_en == 1u) {
      // retry complete split
      hcsplt.split_compl = 1;
      channel->hcsplt = hcsplt.value;
#if HCD_SPLIT
      if (!channel_is_periodic(channel->hcchar)) {
        split_csplit_park(dwc2, channel);
      } else if (!channel_csplit_arm(dwc2, ch_id)) {
        is_done = channel_split_missed(dwc2, ch_id);
      }
#endif
    } else {
      edpt->next_do_ping = 1;
      channel_xfer_out_wrapup(dwc2, ch_id);
      channel_disable(dwc2, channel);
    }
  } else if (hcint & (HCINT_NAK | HCINT_XACT_ERR)) {
    // clean up transfer so far, disable and start again later
    channel_xfer_out_wrapup(dwc2, ch_id);
    channel_disable(dwc2, channel);
    if (hcint & HCINT_XACT_ERR) {
      xfer->err_count++;
      channel->hcintmsk |= HCINT_ACK;
    } else {
      // NAK disable channel to flush all posted request and try again
      edpt->next_do_ping = 1;
      xfer->err_count = 0;
#if HCD_CHANNEL_PARK
      if (!channel_is_periodic(channel->hcchar)) {
        (void) channel_nak_hold(dwc2, ch_id); // the halt parks the retry when held
      }
#endif
    }
  } else if (hcint & HCINT_HALTED) {
    channel->hcintmsk &= ~HCINT_HALTED;
    if (xfer->result != XFER_RESULT_INVALID) {
      is_done = true;
    } else if (xfer->err_count == HCD_XFER_ERROR_MAX) {
      xfer->result = XFER_RESULT_FAILED;
      is_done      = true;
    } else if (xfer->closing == 1) {
      is_done = true;
    } else {
      // Got here due to NAK or NYET
#if HCD_CHANNEL_PARK
      if (xfer->nak_hold) {
        channel_park(dwc2, channel, PARK_XFER_START); // max_nak reached, retry in the next frame
      } else
#endif
      {
        channel_xfer_out_restart(dwc2, ch_id);
      }
    }
  } else if (hcint & HCINT_ACK) {
    xfer->err_count = 0;
    channel->hcintmsk &= ~HCINT_ACK;
    if (hcsplt.split_en == 1u) {
      if (hcsplt.split_compl == 0) {
        // ACK for start split --> do complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
#if HCD_SPLIT
        // The complete-split carries no data: program it with none, as Linux dwc2 does. A slave-mode OUT channel
        // disabled with data still expected drops that much from the TX FIFO it shares with other channels; a
        // complete-split's 64 bytes would be another channel's posted packet, whose start-split then underruns
        // the FIFO into a 1025-byte babble that disables the root port.
        dwc2_channel_tsize_t hctsiz_cs = {.value = channel->hctsiz};
        hctsiz_cs.xfer_size = 0;
        channel->hctsiz = hctsiz_cs.value;
#endif
#if HCD_SPLIT
        if (!channel_is_periodic(channel->hcchar)) {
          split_csplit_park(dwc2, channel);
        } else {
          xfer->csplit_next = HCD_SPLIT_CSPLIT_FIRST;
          if (!channel_csplit_arm(dwc2, ch_id)) {
            is_done = channel_split_missed(dwc2, ch_id);
          }
        }
#endif
      }
    } else {
      // ACK interrupt is only enabled for Split and PING
      // ACK for PING, which mean device is ready to receive data
      channel->hctsiz &= ~HCTSIZ_DOPING; // HC already cleared PING bit, but we clear anyway
      channel->hcchar |= HCCHAR_CHENA;
    }
  } else {
    // nothing to do
  }

  if (is_done) {
    xfer->xferred_bytes += xfer->fifo_bytes;
    xfer->fifo_bytes = 0;
  }

  return is_done;
}
#endif

#if CFG_TUH_DWC2_DMA_ENABLE
static bool handle_channel_in_dma(dwc2_regs_t* dwc2, uint8_t ch_id, uint32_t hcint) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_char_t hcchar = {.value = channel->hcchar};
  dwc2_channel_split_t hcsplt = {.value = channel->hcsplt};
  const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};

  bool is_done = false;

  // TU_LOG1("in  hcint = %02lX\r\n", hcint);

  if (hcint & HCINT_HALTED) {
    if (xfer->retry_disabled) {
      // Halt from our split-NAK throttle disable (below): re-arm the start-split.
      // Programming Guide 3.5 "Halting a Channel" (p73).
      xfer->retry_disabled = 0;
#if HCD_SPLIT
      if (xfer->nak_hold) {
        channel_park(dwc2, channel, PARK_IN_TOKEN); // max_nak reached, retry in the next frame
      } else
#endif
      {
        channel_send_in_token(dwc2, channel, false, HCD_SPLIT_SLOT_NONE);
      }
    } else if (hcint & (HCINT_XFER_COMPLETE | HCINT_STALL | HCINT_BABBLE_ERR)) {
#if HCD_SPLIT_ISO_BOUNCE
      if (edpt->split_bounce) {
        if (0 == (hcint & HCINT_XFER_COMPLETE)) {
          xfer->result = XFER_RESULT_FAILED; // babble
          return true;
        }
        // received into the bounce buffer
        uint8_t* const bounce = split_bounce_buf(edpt);
        const uint16_t len    = (uint16_t) (split_bounce_len(edpt, xfer->xferred_bytes) - hctsiz.xfer_size);
        if (len > 0) {
          hcd_dcache_invalidate(bounce, len);
          memcpy(edpt->buffer + xfer->xferred_bytes, bounce, len);
          xfer->xferred_bytes += len;
        }
        return !channel_iso_in_more(dwc2, ch_id);
      }
#endif
      if (edpt->hcchar_bm.ep_num != 0 && (hcint & HCINT_XFER_COMPLETE)) {
        edpt->next_pid = hctsiz.pid; // save pid (already toggled)
      }

      const uint16_t remain_bytes = (uint16_t) hctsiz.xfer_size;
      const uint16_t remain_packets = hctsiz.packet_count;
      const uint16_t actual_len = edpt->buflen - remain_bytes;
      xfer->xferred_bytes += actual_len;

      is_done = true;

      if (hcint & HCINT_STALL) {
        xfer->result = XFER_RESULT_STALLED;
      } else if (hcint & HCINT_BABBLE_ERR) {
        xfer->result = XFER_RESULT_FAILED;
      } else if (hcsplt.split_en && remain_packets && actual_len == hcchar.ep_size) {
        // Split can only complete 1 transaction (up to 1 packet) at a time, schedule more
        is_done = false;
        edpt->buffer += actual_len;
        edpt->buflen -= actual_len;

        hcsplt.split_compl = 0;
        channel->hcsplt = hcsplt.value;
        is_done = channel_xfer_in_retry(dwc2, ch_id, hcint);
      } else {
        xfer->result = XFER_RESULT_SUCCESS;
      }

      xfer->err_count = 0;
      channel->hcintmsk &= ~HCINT_ACK;
    } else if (hcint & HCINT_XACT_ERR) {
      xfer->err_count++;
      if (xfer->err_count >=  HCD_XFER_ERROR_MAX) {
        is_done = true;
        xfer->result = XFER_RESULT_FAILED;
      } else {
        channel->hcintmsk |= HCINT_ACK | HCINT_NAK | HCINT_DATATOGGLE_ERR;
        hcsplt.split_compl = 0;
        channel->hcsplt = hcsplt.value;
        is_done = channel_xfer_in_retry(dwc2, ch_id, hcint);
      }
    } else if (hcint & HCINT_NYET) {
      // Must handle nyet before nak or ack. Could get a nyet at the same time as either of those on a BULK/CONTROL
      // OUT that started with a PING. The nyet takes precedence.
      if (hcsplt.split_en) {
        // split not yet mean hub has no data, retry complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        is_done = channel_xfer_in_retry(dwc2, ch_id, hcint);
      }
    } else if (hcint & HCINT_ACK) {
      xfer->err_count = 0;
      channel->hcintmsk &= ~HCINT_ACK;
      if (hcsplt.split_en) {
        // start split is ACK --> do complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        if (!HCD_SPLIT || !channel_is_periodic(channel->hcchar)) {
#if HCD_SPLIT
          split_csplit_park(dwc2, channel);
#endif
        } else {
          xfer->csplit_next = HCD_SPLIT_CSPLIT_FIRST;
          if (!channel_csplit_arm(dwc2, ch_id)) {
            is_done = channel_split_missed(dwc2, ch_id);
          }
        }
      }
    } else if (hcint & (HCINT_NAK | HCINT_DATATOGGLE_ERR)) {
      xfer->err_count = 0;
      channel->hcintmsk &= ~(HCINT_NAK | HCINT_DATATOGGLE_ERR);
      hcsplt.split_compl = 0; // restart with start-split
      channel->hcsplt = hcsplt.value;
      // Persistent split bulk/control IN NAK (e.g. idle polled endpoint): re-enabling immediately storms
      // the ISR and starves the task. Disable + re-arm on the resulting halt to throttle (like the slave
      // path); no frame deferral. Programming Guide 3.5 (p73) Note permits disable on NAK/FrmOvrn splits.
      if ((hcint & HCINT_NAK) && hcsplt.split_en && !channel_is_periodic(channel->hcchar)) {
        xfer->retry_disabled = 1;
#if HCD_SPLIT
        (void) channel_nak_hold(dwc2, ch_id); // the halt parks the retry when held
#endif
        channel_disable(dwc2, channel);
      } else {
        is_done = channel_xfer_in_retry(dwc2, ch_id, hcint);
      }
    } else if (hcint & HCINT_FARME_OVERRUN) {
      if (hcchar.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
        xfer->result = XFER_RESULT_FAILED;
        is_done      = true;
      } else {
        is_done = channel_xfer_in_retry(dwc2, ch_id, hcint);
      }
    }
  }

  return is_done;
}

static bool handle_channel_out_dma(dwc2_regs_t* dwc2, uint8_t ch_id, uint32_t hcint) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_split_t hcsplt = {.value = channel->hcsplt};

  bool is_done = false;

  // TU_LOG1("out hcint = %02lX\r\n", hcint);

  if (hcint & HCINT_HALTED) {
    if (xfer->retry_disabled) {
      // Halt from our split-XactErr throttle disable (below): re-issue the start-split (pointers already
      // rewound), giving the hub TT a recovery gap. Programming Guide 3.5 "Halting a Channel" (p73).
      xfer->retry_disabled = 0;
      channel_xfer_start(dwc2, ch_id, false);
    } else if (hcint & (HCINT_XFER_COMPLETE | HCINT_STALL)) {
#if HCD_SPLIT
      if ((hcint & HCINT_XFER_COMPLETE) && hcsplt.split_en && edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
        return !channel_iso_out_next(dwc2, ch_id);
      }
      if ((hcint & HCINT_XFER_COMPLETE) && channel_split_out_more(dwc2, ch_id)) {
        return false;
      }
#endif
      is_done = true;
      xfer->err_count = 0;
      if (hcint & HCINT_XFER_COMPLETE) {
        xfer->result = XFER_RESULT_SUCCESS;
        xfer->xferred_bytes += edpt->buflen;
      } else {
        xfer->result = XFER_RESULT_STALLED;
        channel_xfer_out_wrapup(dwc2, ch_id);
      }
      channel->hcintmsk &= ~HCINT_ACK;
    } else if (hcint & HCINT_XACT_ERR) {
      if (hcint & (HCINT_NAK | HCINT_NYET | HCINT_ACK)) {
        xfer->err_count = 0;
        // clean up transfer so far and start again
        channel_xfer_out_wrapup(dwc2, ch_id);
        channel_xfer_out_restart(dwc2, ch_id);
      } else {
        xfer->err_count++;
        if (xfer->err_count >= HCD_XFER_ERROR_MAX) {
          xfer->result = XFER_RESULT_FAILED;
          is_done = true;
        } else {
          // Rewind, then retry the start-split. Non-periodic SPLIT throttles via channel_disable + re-arm on
          // the halt (immediate re-fire exhausts the retry budget; the disable gives the hub TT a recovery
          // gap, like slave). Periodic split is excluded: channel_disable() is a no-op for it, so the halt
          // never fires and the channel would wedge. Non-split re-inits immediately (Programming Guide 5.1.2.3).
          channel_xfer_out_wrapup(dwc2, ch_id);
          if (hcsplt.split_en && !channel_is_periodic(channel->hcchar)) {
            xfer->retry_disabled = 1;
            channel_disable(dwc2, channel);
          } else {
            channel_xfer_out_restart(dwc2, ch_id);
          }
        }
      }
    } else if (hcint & HCINT_FARME_OVERRUN) {
      channel_xfer_out_wrapup(dwc2, ch_id);
      if (edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
        xfer->result = XFER_RESULT_FAILED;
        is_done      = true;
      } else {
        channel_xfer_out_restart(dwc2, ch_id);
      }
    } else if (hcint & HCINT_NYET) {
      if (hcsplt.split_en && hcsplt.split_compl) {
        // split not yet mean hub has no data, retry complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        if (!HCD_SPLIT || !channel_is_periodic(channel->hcchar)) {
#if HCD_SPLIT
          split_csplit_park(dwc2, channel);
#endif
        } else if (!channel_csplit_arm(dwc2, ch_id)) {
          is_done = channel_split_missed(dwc2, ch_id);
        }
      }
    } else if (hcint & HCINT_ACK) {
      xfer->err_count = 0;
      if (hcsplt.split_en && !hcsplt.split_compl) {
        // start split is ACK --> do complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        if (!HCD_SPLIT || !channel_is_periodic(channel->hcchar)) {
#if HCD_SPLIT
          split_csplit_park(dwc2, channel);
#endif
        } else {
          xfer->csplit_next = HCD_SPLIT_CSPLIT_FIRST;
          if (!channel_csplit_arm(dwc2, ch_id)) {
            is_done = channel_split_missed(dwc2, ch_id);
          }
        }
      }
    } else if ((hcint & HCINT_NAK) && hcsplt.split_en) {
      // Split OUT NAK: rewind + retry the start-split, else the channel stalls (Programming Guide 5.1.4.2).
      // Non-split OUT NAK is core-handled (5.1.2.2), so this is split-only.
      xfer->err_count = 0;
      channel_xfer_out_wrapup(dwc2, ch_id);
#if HCD_SPLIT
      if (!channel_is_periodic(channel->hcchar) && channel_nak_hold(dwc2, ch_id)) {
        channel_park(dwc2, channel, PARK_XFER_START); // max_nak reached, retry in the next frame
      } else
#endif
      {
        channel_xfer_out_restart(dwc2, ch_id);
      }
    }
  } else if (hcint & HCINT_ACK) {
    xfer->err_count = 0;
    channel->hcintmsk &= ~HCINT_ACK;
  }

  return is_done;
}
#endif

// Report the completed transfer of a channel and release the channel
static void channel_xfer_done(dwc2_regs_t* dwc2, uint8_t ch_id, bool in_isr) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  if (xfer->closing == 1) {
    edpt_dealloc(&_hcd_data.edpt[xfer->ep_id]);
  } else {
    const dwc2_channel_char_t hcchar  = {.value = dwc2->channel[ch_id].hcchar};
    const uint8_t             ep_addr = tu_edpt_addr(hcchar.ep_num, hcchar.ep_dir);
    hcd_event_xfer_complete(hcchar.dev_addr, ep_addr, xfer->xferred_bytes, (xfer_result_t) xfer->result, in_isr);
  }
#if HCD_SPLIT
  split_iso_out_close(dwc2, ch_id); // an isochronous OUT packet that failed
  if (!channel_is_periodic(_hcd_data.edpt[xfer->ep_id].hcchar)) {
    _hcd_nonperiodic_uframe = nonperiodic_uframe_tag(dwc2->hfnum);
  }
#endif
  channel_dealloc(dwc2, ch_id);
}

#if HCD_SPLIT
// Enable the periodic complete-splits waiting for their microframe: late ones for this microframe first, which the
// periodic request queue would hold behind one for the next, then each group by oldest start-split (USB 2.0 11.18.8).
// Return true if one still waits.
static bool split_csplit_service(dwc2_regs_t* dwc2, bool in_isr) {
  const uint8_t  max_channel = dwc2_channel_count(dwc2);
  const uint32_t hfnum       = dwc2->hfnum;
  uint32_t       waiting     = 0;
  uint32_t       late        = 0;
  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    const hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
    if (xfer->allocated && xfer->csplit_wait) {
      waiting |= TU_BIT(ch_id);
      if (uframe_ahead((uint16_t) (xfer->split_uframe + xfer->csplit_next), hfnum) <= 0) {
        late |= TU_BIT(ch_id);
      }
    }
  }

  bool more = false;
  while (waiting != 0) {
    const uint32_t group = (late != 0) ? late : waiting;
    uint8_t        first = TUSB_INDEX_INVALID_8;
    for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
      if (tu_bit_test(group, ch_id) && (first == TUSB_INDEX_INVALID_8 || split_xfer_before(ch_id, first))) {
        first = ch_id;
      }
    }
    waiting &= ~TU_BIT(first);
    late &= ~TU_BIT(first);

    hcd_xfer_t* xfer = &_hcd_data.xfer[first];
    if (!channel_csplit_arm(dwc2, first) && channel_split_missed(dwc2, first)) {
      channel_xfer_done(dwc2, first, in_isr);
    }
    more = more || (xfer->allocated && xfer->csplit_wait);
  }
  return more;
}
#endif

static void handle_channel_irq(uint8_t rhport, bool in_isr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const bool is_dma = dma_host_enabled(dwc2);
  const uint8_t max_channel = dwc2_channel_count(dwc2);
#if HCD_SPLIT
  _hcd_csplit_pass     = true;
  uint16_t pass_uframe = (uint16_t) (dwc2->hfnum & HCD_FRAME_NUMBER_MASK);
  bool     handled     = false;
#endif

  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
#if HCD_SPLIT
    // Complete-splits enabled now rather than at the end of the pass: late ones held behind an older one, once
    // another channel has been handled, and those for the next microframe once it has begun
    const uint16_t uframe = (uint16_t) (dwc2->hfnum & HCD_FRAME_NUMBER_MASK);
    if ((_hcd_csplit_late_held && handled) || (_hcd_csplit_behind && uframe != pass_uframe)) {
      _hcd_csplit_late_held = false;
      _hcd_csplit_behind    = false;
      handled               = false;
      (void) split_csplit_service(dwc2, in_isr);
    }
    pass_uframe = uframe;
#endif

    if (tu_bit_test(dwc2->haint, ch_id)) {
#if HCD_SPLIT
      handled = true;
#endif
      dwc2_channel_t* channel = &dwc2->channel[ch_id];
      hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
      TU_ASSERT(xfer->ep_id < CFG_TUH_DWC2_ENDPOINT_MAX,);
      dwc2_channel_char_t hcchar = {.value = channel->hcchar};

      const uint32_t hcint = channel->hcint;
      // Slave handlers process one cause per pass. If ChHltd arrived with
      // another cause, leave it pending so the next pass retires the halt.
      const uint32_t hcint_clear = (!is_dma && (hcint & ~HCINT_HALTED)) ? (hcint & ~HCINT_HALTED) : hcint;
      channel->hcint = hcint_clear;

#if DWC2_LS_ONE_XACT_PER_FRAME
      // ahead of the handlers below, which re-arm through ls_frame_claim()
      if (hcint & HCINT_HALTED) {
        ls_bus_release(dwc2, ch_id);
      }
#endif

      // Retire closing/aborted DMA channels and aborted slave channels before retry handling can re-arm them
      // (Programming Guide 3.5, p73). An aborted slave channel ignores other causes until its requested halt.
      const bool retire = xfer->aborting || (is_dma && xfer->closing);
      if (retire && !is_dma && !(hcint & HCINT_HALTED)) {
        continue;
      }
      if (retire && (hcint & HCINT_HALTED)) {
        hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
        const bool closing = xfer->closing;
        // channel_xfer_start() predicts the PID after all requested packets;
        // an aborted transfer may have completed fewer.
        if (hcchar.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
          const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
          edpt->next_pid = hctsiz.pid;
        }
        xfer->aborting = false;
#if HCD_SPLIT
        split_iso_out_close(dwc2, ch_id);
#endif
        channel_dealloc(dwc2, ch_id);
        if (closing) {
          edpt_dealloc(edpt);
        } else {
          edpt->aborting = 0;
        }
        continue;
      }

      bool is_done = false;
      if (is_dma) {
        #if CFG_TUH_DWC2_DMA_ENABLE
        if (hcchar.ep_dir == TUSB_DIR_OUT) {
          is_done = handle_channel_out_dma(dwc2, ch_id, hcint);
        } else {
          is_done = handle_channel_in_dma(dwc2, ch_id, hcint);
          bool invalidate = is_done && (channel->hcdma > xfer->xferred_bytes);
          #if HCD_SPLIT_ISO_BOUNCE
          invalidate = invalidate && 0 == _hcd_data.edpt[xfer->ep_id].split_bounce; // copied there by the CPU
          #endif
          if (invalidate) {
            // hcdma is increased by word --> need to align4
            hcd_dcache_invalidate((void*) tu_align4(channel->hcdma - xfer->xferred_bytes), xfer->xferred_bytes);
          }
        }
        #endif
      } else {
        #if CFG_TUH_DWC2_SLAVE_ENABLE
        if (hcchar.ep_dir == TUSB_DIR_OUT) {
          is_done = handle_channel_out_slave(dwc2, ch_id, hcint);
        } else {
          is_done = handle_channel_in_slave(dwc2, ch_id, hcint);
        }
  #endif
      }

      if (is_done) {
        channel_xfer_done(dwc2, ch_id, in_isr);
      }
    }
  }

#if HCD_SPLIT
  // complete-splits held for the end of the pass, or behind an older one on their TT, may go now
  _hcd_csplit_pass      = false;
  _hcd_csplit_late_held = false;
  if (_hcd_csplit_behind) {
    _hcd_csplit_behind = false;
    (void) split_csplit_service(dwc2, in_isr);
  }
#endif
}

// SOF is enabled for scheduled periodic transfer
static bool handle_sof_irq(uint8_t rhport, bool in_isr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  dwc2->gintsts = GINTSTS_SOF; // Clear the SOF interrupt flag

  // If highspeed then SOF is 125us, else 1ms
  const uint32_t ucount = (hprt_speed_get(dwc2) == TUSB_SPEED_HIGH ? 1 : 8);

#if HCD_SPLIT
  (void) split_nonperiodic_ended(dwc2->hfnum); // expires a past one, its 14-bit microframe number recurs
  // periodic complete-splits waiting for their microframe
  _hcd_csplit_pass = false; // in case channel interrupt handling ended early
  bool more_isr = split_csplit_service(dwc2, in_isr);
#else
  (void) in_isr;
  bool more_isr = false;
#endif

  // A second pass starts isochronous split OUT packets, so that start-splits for the microframe of a BEGIN go first
  for (uint16_t i = 0; i < (HCD_SPLIT ? 2u : 1u) * CFG_TUH_DWC2_ENDPOINT_MAX; i++) {
    const uint8_t   ep_id   = (uint8_t) (i % CFG_TUH_DWC2_ENDPOINT_MAX);
    hcd_endpoint_t* edpt    = &_hcd_data.edpt[ep_id];
    const bool      iso_out = HCD_SPLIT && edpt->hcsplt_bm.split_en &&
                         edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS && edpt->hcchar_bm.ep_dir == TUSB_DIR_OUT;
    if (edpt->closing == 0 && iso_out == (i >= CFG_TUH_DWC2_ENDPOINT_MAX)) {
      if (edpt->hcchar_bm.enable && channel_is_periodic(edpt->hcchar) && edpt->xfer_pending) {
        if (edpt->uframe_countdown > 0) {
          edpt->uframe_countdown -= tu_min32(ucount, edpt->uframe_countdown);
        }
        if (HCD_SPLIT && edpt->uframe_countdown == 0 && edpt->hcsplt_bm.split_en &&
            (((dwc2->hfnum + 1u) & 7u) != split_ss_uframe(edpt) || split_ss_hold(dwc2, edpt))) {
          more_isr = true; // start-split waits for its microframe, or for the END of an isochronous OUT packet
          continue;
        }
        if (edpt->uframe_countdown == 0) {
          if (!edpt_xfer_kickoff(dwc2, ep_id)) {
            edpt->uframe_countdown = ucount; // failed to start, try again next frame
          }
        }

        more_isr = more_isr || edpt->xfer_pending;
      }
    }
  }

#if HCD_CHANNEL_PARK
  // Replay the call that parked. The periodic endpoints above come back through edpt_xfer_kickoff(), but a control
  // or bulk transfer has no schedule of its own, so this is the only thing that gets it going again. Each replay on a
  // busy low-speed frame parks itself again; a control/bulk retry held by max_nak waits for the next frame.
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
    if (!xfer->allocated || xfer->park == PARK_NONE) {
      continue;
    }
    hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
    if (xfer->closing || edpt->closing || edpt->aborting) {
      // Teardown is under way. A channel that never reached the bus gets no halt to finish it, so retire it here; one
      // parked between the packets of its transfer is still enabled and finishes through its halt.
      const bool closing = xfer->closing || edpt->closing;
      if (channel_park_retire(dwc2, ch_id)) {
        if (closing) {
          edpt_dealloc(edpt);
        } else {
          edpt->aborting = 0;
        }
      } else {
        xfer->closing = closing ? 1u : 0u;
        channel_disable(dwc2, &dwc2->channel[ch_id]);
      }
      continue;
    }
    if (xfer->nak_hold) {
      if (channel_nak_frame(dwc2) == xfer->nak_frame) {
        more_isr = true;
        continue;
      }
      xfer->nak_hold = 0;
    }
  #if HCD_SPLIT
    if (xfer->uframe_hold) {
      if ((uint8_t) (dwc2->hfnum & HCD_FRAME_NUMBER_MASK) == xfer->park_uframe) {
        more_isr = true; // an SOF of the microframe it was parked in, still pending
        continue;
      }
      xfer->uframe_hold = 0;
    }
  #endif

    const channel_park_t kind = (channel_park_t) xfer->park;
    xfer->park = PARK_NONE;

    if (kind == PARK_CSPLIT_OUT) {
      dwc2->channel[ch_id].hcchar |= HCCHAR_CHENA;
    } else if (kind == PARK_CSPLIT_IN) {
      (void) channel_send_in_token(dwc2, &dwc2->channel[ch_id], false, HCD_SPLIT_SLOT_NONE);
    } else if (kind == PARK_IN_TOKEN || kind == PARK_IN_TOKEN_PERIODIC) {
      const uint16_t periodic_frame = channel_send_in_token(dwc2, &dwc2->channel[ch_id],
                                                            kind == PARK_IN_TOKEN_PERIODIC, HCD_SPLIT_SLOT_NONE);
      if (periodic_frame != DWC2_LS_PARKED && kind == PARK_IN_TOKEN_PERIODIC) {
        edpt->periodic_frame = periodic_frame;
      }
    } else if (!channel_xfer_start(dwc2, ch_id, kind == PARK_XFER_START_PERIODIC)) {
      // same retreat edpt_xfer_kickoff() does, otherwise the transfer would be stranded here
      channel_dealloc(dwc2, ch_id);
      periodic_xfer_defer(dwc2, edpt, 0);
    }
    more_isr = true;
  }
#endif

  return more_isr;
}

// Config HCFG FS/LS clock and HFIR for SOF interval according to link speed (value is in PHY clock unit)
// Databook Table 2-2: System Clock Speeds
// +------------+------------------+----------+-----------+-------------------+
// | PHY        | PHY Clock (MHz)  | Width    | HCFG.Sel  | HFIR (clk cycles) |
// +------------+------------------+----------+-----------+-------------------+
// | HS UTMI+   | 30               | 16-bit   | 30_60     | HS:3749 FS:29999  |
// | HS UTMI+   | 60               |  8-bit   | 30_60     | HS:7499 FS:59999  |
// | HS ULPI    | 60               |  8-bit   | 30_60     | HS:7499 FS:59999  |
// | FS (dead.) | 48               | internal | 48        | FS:47999          |
// | LS via FS  | 6                | internal | 6         | LS:5999           |
// +------------+------------------+----------+-----------+-------------------+
// HFIR = (interval_us * phy_clock) - 1, where interval is 125us (HS) or 1000us (FS/LS)
static void port0_enable(dwc2_regs_t* dwc2, tusb_speed_t speed) {
  uint32_t hcfg = dwc2->hcfg & ~HCFG_FSLS_PHYCLK_SEL;

  const dwc2_gusbcfg_t gusbcfg = {.value = dwc2->gusbcfg};
  uint32_t             phy_clock;

  if (gusbcfg.phy_sel == GUSBCFG_PHYSEL_FULLSPEED) {
    if (speed == TUSB_SPEED_LOW) {
      phy_clock = 6;  // LS via FS PHY is 6MHz (utmifs_clk48/8)
      hcfg |= HCFG_FSLS_PHYCLK_SEL_6MHZ;
    } else {
      phy_clock = 48; // FS is 48Mhz (utmifs_clk48)
      hcfg |= HCFG_FSLS_PHYCLK_SEL_48MHZ;
    }
  } else {
    if (gusbcfg.ulpi_utmi_sel == GUSBCFG_PHYHS_ULPI) {
      phy_clock = 60; // ULPI 8-bit is 60Mhz
    } else {
      // UTMI+ 16-bit is 30Mhz, 8-bit is 60Mhz
      phy_clock = gusbcfg.phy_if16 ? 30 : 60;

      // Enable UTMI+ low power mode 48Mhz external clock if not highspeed
      if (speed == TUSB_SPEED_HIGH) {
        dwc2->gusbcfg &= ~GUSBCFG_PHYLPCS;
      } else {
        dwc2->gusbcfg |= GUSBCFG_PHYLPCS;
        // may need to reset port
      }
    }
    hcfg |= HCFG_FSLS_PHYCLK_SEL_30_60MHZ;
  }

  dwc2->hcfg = hcfg;

  uint32_t hfir = dwc2->hfir & ~HFIR_FRIVL_Msk;
  if (speed == TUSB_SPEED_HIGH) {
    hfir |= 125*phy_clock - 1; // The "- 1" is the correct value. The Synopsys databook was corrected in 3.30a
  } else {
    hfir |= 1000*phy_clock - 1;
  }

  dwc2->hfir = hfir;
}

/* Handle Host Port interrupt, possible source are:
   - Connection Detection
   - Enable Change
   - Over Current Change
*/
static void handle_hprt_irq(uint8_t rhport, bool in_isr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const dwc2_hprt_t hprt_bm = {.value = dwc2->hprt};
  uint32_t hprt = hprt_bm.value & ~HPRT_W1_MASK;

  if (hprt_bm.conn_detected == 1u) {
    // Port Connect Detect
    hprt |= HPRT_CONN_DETECT;

    if (hprt_bm.conn_status == 1u) {
      hcd_event_device_attach(rhport, in_isr);
    }
  }

  if (hprt_bm.enable_change == 1u) {
    // Port enable change
    hprt |= HPRT_ENABLE_CHANGE;

    if (hprt_bm.enable == 1u) {
      // Port enable
      const tusb_speed_t speed = hprt_speed_get(dwc2);
      port0_enable(dwc2, speed);
    } else if (hprt_bm.conn_status == 1u && hprt_bm.conn_detected == 0u) {
      // The core disabled a still-connected port. Reuse attach handling
      // to tear down the stale device tree and restart enumeration.
      hcd_event_device_attach(rhport, in_isr);
    }
  }

  dwc2->hprt = hprt; // clear interrupt
}

/* Interrupt Hierarchy
               HCINTn       HPRT
                 |           |
               HAINT.CHn     |
                 |           |
    GINTSTS :  HCInt     | PrtInt | NPTxFEmp | PTxFEmpp | RXFLVL | SOF
*/
void hcd_int_handler(uint8_t rhport, bool in_isr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const uint32_t gintmsk = dwc2->gintmsk;
  const uint32_t gintsts = dwc2->gintsts & gintmsk;

  // TU_LOG1_HEX(gintsts);

  if (gintsts & GINTSTS_SOF) {
    const bool more_sof = handle_sof_irq(rhport, in_isr);
    if (!more_sof) {
      dwc2->gintmsk &= ~GINTSTS_SOF;
    }
  }

  if (gintsts & GINTSTS_DISCINT) {
    dwc2->gintsts = GINTSTS_DISCINT;
    channel_cleanup_on_disconnect(dwc2);
    hcd_event_device_remove(rhport, in_isr);

    // A fast replug can be visible without a pending connect-detect interrupt.
    const uint32_t hprt = dwc2->hprt;
    if (!(hprt & HPRT_CONN_DETECT) && (hprt & HPRT_CONN_STATUS)) {
      hcd_event_device_attach(rhport, in_isr);
    }
    return;
  }

  if (gintsts & GINTSTS_HPRTINT) {
    // Host port interrupt: source is cleared in HPRT register
    // TU_LOG1_HEX(dwc2->hprt);
    handle_hprt_irq(rhport, in_isr);
  }

#if CFG_TUH_DWC2_SLAVE_ENABLE
  // RxFIFO non-empty interrupt handling
  if (gintsts & GINTSTS_RXFLVL) {
    // RXFLVL bit is read-only
    dwc2->gintmsk &= ~GINTSTS_RXFLVL; // disable RXFLVL interrupt while reading

    do {
      handle_rxflvl_irq(rhport); // read all packets
    } while(dwc2->gintsts & GINTSTS_RXFLVL);

    dwc2->gintmsk |= GINTSTS_RXFLVL;
  }

  if (gintsts & GINTSTS_NPTX_FIFO_EMPTY) {
    // NPTX FIFO empty interrupt, this is read-only and cleared by hardware when FIFO is written
    const bool more_nptxfe = handle_txfifo_empty(dwc2, false);
    if (!more_nptxfe) {
      // no more pending packet, disable interrupt
      dwc2->gintmsk &= ~GINTSTS_NPTX_FIFO_EMPTY;
    }
  }

  if (gintsts & GINTSTS_PTX_FIFO_EMPTY) {
    // PTX FIFO empty interrupt, this is read-only and cleared by hardware when FIFO is written
    const bool more_ptxfe = handle_txfifo_empty(dwc2, true);
    if (!more_ptxfe) {
      // no more pending packet, disable interrupt
      dwc2->gintmsk &= ~GINTSTS_PTX_FIFO_EMPTY;
    }
  }
#endif

  // Draining the RxFIFO completion status can assert HCINT.XferCompl. Read
  // the live status here so the completion is handled in this ISR invocation.
  const uint32_t gintmsk_live = dwc2->gintmsk;
  if ((dwc2->gintsts & gintmsk_live) & GINTSTS_HCINT) {
    handle_channel_irq(rhport, in_isr);
  }
#if HCD_SPLIT
  else if (_hcd_csplit_behind) {
    // RX status need not produce a channel interrupt. Revisit requests held
    // for that work without waiting for another SOF.
    _hcd_csplit_behind = false;
    (void) split_csplit_service(dwc2, in_isr);
  }
#endif

}

#endif
