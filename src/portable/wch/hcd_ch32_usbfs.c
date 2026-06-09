/*
 * hcd_ch32_usbfs.c — async single-engine HCD for the CH32 USBFS host
 * controller (CH32V20x / CH32V307).
 *
 * Modeled on portable/analog/max3421/hcd_max3421.c: an ISR-driven round-robin
 * scheduler over a flat ep[] array, with at most ONE transaction outstanding on
 * the single CH32 USBFS host transfer engine (one DEV_ADDR latch, one
 * HOST_EP_PID token, one shared toggle pair, one RX/TX DMA pair). A per-(daddr,
 * ep) endpoint model lets hubs + multiple downstream devices + hotplug work.
 *
 * Control transfers run through WCH's reference transaction layer
 * (wch_usbfs_ll.c, USBFSH_CtrlTransfer); interrupt endpoints are driven by the
 * async ep[] engine here. The ISR avoids any SOF-PRES busy-wait at IRQ entry,
 * does no logging in interrupt context, and bounds every retry/wait.
 *
 * Licensed MIT. Low-level transaction code derived from WCH's HOST_KM
 * reference (wch_usbfs_ll.c).
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_WCH_USBFS) && CFG_TUH_WCH_USBIP_USBFS

#include "ch32_usbfs_reg.h"
#include "host/hcd.h"
#include "host/usbh.h"
#include "wch_usbfs_ll.h"   // Delay_Us/Ms, USBFS_RX/TX_Buf, USBFSH_* control primitives

//--------------------------------------------------------------------+
// Config
//--------------------------------------------------------------------+

#ifndef CFG_TUH_CH32_ENDPOINT_TOTAL
// ep[0] = addr0 default control pipe + ~15 for {root|hub + downstream}.
// A 4-port hub of HID controllers fits in 16. Do NOT oversize (64KB SRAM).
#define CFG_TUH_CH32_ENDPOINT_TOTAL  16
#endif

#ifndef CFG_TUH_CH32_NAK_MAX
// NAKs per EP per frame before yielding the engine to another EP; 0 = infinite.
#define CFG_TUH_CH32_NAK_MAX  1
#endif

// Consecutive no-response transfers (engine armed, transfer-done IRQ never
// arrived within the bounded ISR retry window) before reporting device removal.
// A connected-idle device NAKs (a real response, IRQ fires); only a physically
// removed device gives zero response.
#define CH32_HOST_DISCON_POLLS  16

// Bound EVERY busy-wait (single-core, RTOS-less: an unbounded wait hangs the
// whole loop). 2e6 is a generous upper bound.
#define CH32_WAIT_TIMEOUT  2000000u

// In-ISR bounded retry count on a no-response/error before giving up on the
// current transaction (a small staged retry, kept
// small so a truly-dead device surfaces quickly to the discon counter).
#ifndef CH32_ISR_MAX_RETRIES
#define CH32_ISR_MAX_RETRIES  3
#endif

#define CH32_WAIT_SIE_IDLE() \
  do { uint32_t _to = CH32_WAIT_TIMEOUT; while (!(USBFSH->MIS_ST & USBFS_UMS_SIE_FREE) && --_to) {} } while (0)

//--------------------------------------------------------------------+
// Data model (§B): one slot per (daddr, ep_num, dir)
//--------------------------------------------------------------------+

typedef enum {
  EP_STATE_IDLE       = 0,   // no transfer queued
  EP_STATE_ABORTING   = 2,   // abort requested; next idle clears it
  EP_STATE_ATTEMPT_1  = 3,   // active/pending; values above count NAKs this frame
  EP_STATE_ATTEMPT_MAX = 15,
} ep_state_t;

typedef struct TU_ATTR_PACKED {
  // identity / allocation
  uint8_t  daddr;            // owning device addr; 0 == free slot (sentinel)
  uint8_t  ep_num   : 4;     // endpoint number 0..15
  uint8_t  is_out   : 1;     // 1 = OUT/SETUP direction (host->dev)
  uint8_t  is_setup : 1;     // current token is SETUP
  uint8_t  is_iso   : 1;     // reserved (iso not driven)
  uint8_t  rsvd     : 1;

  // pre-built CH32 token byte: (USB_PID_xxx<<4)|ep_num — write to HOST_EP_PID.
  uint8_t  pid;

  // state machine; >ATTEMPT_1 also counts NAKs this frame
  uint8_t  state;

  uint8_t  data_toggle : 1;  // DATA0/DATA1, persisted per-EP across xfers
  uint16_t packet_size : 11; // wMaxPacketSize; ALSO the "allocated" flag (!=0)

  // current transfer
  uint16_t total_len;        // bytes requested this transfer
  uint16_t xferred_len;      // bytes done so far
  uint8_t* buf;              // caller buffer base (cursor = buf + xferred_len)

  // interrupt-EP interval pacing: poll only when frame_count >= next_frame, then
  // re-arm at +interval. interval=1 (control/bulk) polls every frame.
  uint8_t  interval;         // bInterval in frames (interrupt); 1 otherwise
  uint16_t next_frame;       // earliest frame this EP may be polled again

  uint8_t  setup[8];         // staged 8-byte SETUP packet (ep0 only)
} ch32_ep_t;

typedef struct {
  volatile uint16_t frame_count;        // SOF tick -> hcd_frame_number
  ch32_ep_t ep[CFG_TUH_CH32_ENDPOINT_TOTAL];

  volatile bool busy_lock;              // single-engine "in use" gate
  int8_t   cur_idx;                     // ep[] index currently on the wire (-1 none)
  uint8_t  cur_daddr;                   // shadow of DEV_ADDR (skip redundant writes)

  // no-response disconnect tracking. Set when the engine is armed;
  // the SOF tick declares no-response if it stays armed past a frame deadline.
  uint8_t  discon_polls;
  volatile bool armed;                  // a transaction is on the wire
  volatile uint16_t armed_frame;        // frame_count when armed (deadline base)

  bool     attached;                    // device present (set by the SOF/DETECT attach poll)
  tusb_speed_t root_speed;              // survives hcd_device_close(0)
} ch32_hcd_t;

static ch32_hcd_t _hcd;

// Shims for the vendor LL (wch_usbfs_ll.c), which calls the WCH SDK Delay_Us/Ms.
// The LL only needs ballpark settle timing; derive a busy-wait from the core
// clock so it holds across parts/clocks (~6 cycles per iteration).
void Delay_Ms(uint32_t n) { tusb_time_delay_ms_api(n); }
void Delay_Us(uint32_t n) {
  uint32_t iters = (uint32_t) (((uint64_t) n * SystemCoreClock) / 6000000u);
  while (iters--) { __asm__ volatile("nop"); }
}

// The DMA buffers (USBFS_RX/TX_Buf) and control primitives (USBFSH_CtrlTransfer,
// USBFSH_EnableRootHubPort, USBFSH_ResetRootHubPort) are declared in
// wch_usbfs_ll.h. The async ep[] engine drives INTERRUPT EPs; CONTROL goes
// through the WCH synchronous path, which enumerates hubs/devices the raw async
// SETUP could not. Both share the same DMA buffers so host DMA is never
// re-pointed between control and interrupt transfers.
static bool    s_ctrl_pending = false;
static uint8_t s_ctrl_daddr   = 0;

//--------------------------------------------------------------------+
// ep[] helpers
//--------------------------------------------------------------------+

// ep0 of addr0 (the default enumeration pipe) is reserved at index 0.
static ch32_ep_t* find_ep(uint8_t daddr, uint8_t ep_num, uint8_t dir) {
  if (daddr == 0 && ep_num == 0) return &_hcd.ep[0];
  for (uint8_t i = 1; i < CFG_TUH_CH32_ENDPOINT_TOTAL; i++) {
    ch32_ep_t* ep = &_hcd.ep[i];
    if (ep->packet_size == 0) continue;           // free
    if (ep->daddr != daddr || ep->ep_num != ep_num) continue;
    // control endpoint is bidirectional: match on (daddr,ep_num) only.
    if (ep_num == 0 || ep->is_out == (dir == TUSB_DIR_OUT)) return ep;
  }
  return NULL;
}

static ch32_ep_t* alloc_ep(void) {
  for (uint8_t i = 1; i < CFG_TUH_CH32_ENDPOINT_TOTAL; i++) {
    if (_hcd.ep[i].packet_size == 0) return &_hcd.ep[i];
  }
  return NULL;
}

static void free_ep_daddr(uint8_t daddr) {
  for (uint8_t i = 1; i < CFG_TUH_CH32_ENDPOINT_TOTAL; i++) {
    if (_hcd.ep[i].daddr == daddr && _hcd.ep[i].packet_size) {
      tu_memclr(&_hcd.ep[i], sizeof(ch32_ep_t));
    }
  }
}

static inline bool is_ep_pending(const ch32_ep_t* ep) {
  if (ep->packet_size == 0 || ep->state < EP_STATE_ATTEMPT_1) return false;
  // Interval gate: an EP that NAKed/timed out is re-pollable only once its
  // bInterval has elapsed (signed diff handles frame_count wrap).
  return (int16_t)(_hcd.frame_count - ep->next_frame) >= 0;
}

// Round-robin scan starting AFTER cur so no endpoint monopolizes the engine.
static int8_t find_next_pending(int8_t cur) {
  for (uint8_t k = 1; k <= CFG_TUH_CH32_ENDPOINT_TOTAL; k++) {
    int8_t i = (int8_t)(((int)cur + k) % CFG_TUH_CH32_ENDPOINT_TOTAL);
    if (is_ep_pending(&_hcd.ep[i])) return i;
  }
  return -1;
}

//--------------------------------------------------------------------+
// Register leaves (arm-SETUP / arm-IN/OUT)
//--------------------------------------------------------------------+

// §D row 1: latch DEV_ADDR, preserving the GP bit. Shadowed to skip redundant
// writes — but always re-write on a real EP switch so a fresh device is selected.
static inline void select_dev(uint8_t daddr) {
  _hcd.cur_daddr = daddr;
  USBFSH->DEV_ADDR = (uint8_t)((USBFSH->DEV_ADDR & USBFS_UDA_GP_BIT) | (daddr & USBFS_USB_ADDR_MASK));
}

// Program device address, port timing and host-port enable state for the
// selected device. Low-speed direct attach uses LOW_SPEED with no PRE;
// low-speed behind a full-speed hub needs PRE while tokens still go out at
// full-speed.
static inline void select_dev_speed(uint8_t daddr) {
  select_dev(daddr);

  tuh_bus_info_t bus_info;
  tuh_bus_info_get(daddr, &bus_info);

  tusb_speed_t const dev_speed = bus_info.speed;
  bool const via_hub = (bus_info.hub_addr != 0);
  bool const low_speed_direct = (!via_hub && dev_speed == TUSB_SPEED_LOW);
  bool const low_speed_via_hub = (via_hub &&
                                  _hcd.root_speed == TUSB_SPEED_FULL &&
                                  dev_speed == TUSB_SPEED_LOW);

  if (low_speed_direct || low_speed_via_hub) {
    USBFSH->BASE_CTRL |= USBFS_UC_LOW_SPEED;
  } else {
    USBFSH->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
  }

  if (low_speed_direct) {
    USBFSH->HOST_CTRL  |= USBFS_UH_LOW_SPEED;
    USBFSH->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
  } else if (low_speed_via_hub) {
    USBFSH->HOST_CTRL  &= ~USBFS_UH_LOW_SPEED;
    USBFSH->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
  } else {
    USBFSH->HOST_CTRL  &= ~USBFS_UH_LOW_SPEED;
    USBFSH->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
  }

  // Re-assert the host port state before every launch. CH32 USBFS can miss the
  // first EP0 transaction after reset/hub-port reset unless both bits are
  // armed again immediately before the transfer.
  USBFSH->HOST_CTRL  |= USBFS_UH_PORT_EN;
  USBFSH->HOST_SETUP |= USBFS_UH_SOF_EN;
}

// Mark the engine as armed and (re)set the no-response deadline base. Called by
// every register-level launch, including in-place control/continuation re-arms,
// so a slow-but-responding device is never falsely declared disconnected.
static inline void mark_armed(void) {
  _hcd.armed       = true;
  _hcd.armed_frame = _hcd.frame_count;
}

// Arm an 8-byte SETUP packet. SETUP is always DATA0.
static void xact_setup(ch32_ep_t* ep) {
  mark_armed();
  select_dev_speed(ep->daddr);

  memcpy(USBFS_TX_Buf, ep->setup, 8);

  CH32_WAIT_SIE_IDLE();
  USBFSH->HOST_EP_PID  = 0;
  USBFSH->HOST_RX_DMA  = (uint32_t) USBFS_RX_Buf;
  USBFSH->HOST_TX_DMA  = (uint32_t) USBFS_TX_Buf;
  USBFSH->HOST_TX_LEN  = 8;
  // SETUP forces DATA0 on both directions, AUTO_TOG so the HW flips after ACK.
  USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = USBFS_UH_T_AUTO_TOG | USBFS_UH_R_AUTO_TOG;
  USBFSH->INT_FG       = 0xFF;
  USBFSH->HOST_EP_PID  = (uint8_t)((USB_PID_SETUP << 4) | 0);  // launch
}

// Arm an IN or OUT data transaction. On an EP switch we reload
// DEV_ADDR + speed + the per-EP data toggle into the shared HW toggle latch.
static void xact_inout(ch32_ep_t* ep, bool switch_ep) {
  mark_armed();
  if (switch_ep) {
    select_dev_speed(ep->daddr);
  }

  CH32_WAIT_SIE_IDLE();
  USBFSH->HOST_EP_PID = 0;
  USBFSH->HOST_RX_DMA = (uint32_t) USBFS_RX_Buf;
  USBFSH->HOST_TX_DMA = (uint32_t) USBFS_TX_Buf;

  uint8_t tog = (uint8_t)(ep->data_toggle ? USBFS_UH_T_TOG : 0);  // bit2 same pos in T/R

  if (ep->is_out) {
    uint16_t chunk = (uint16_t)(ep->total_len - ep->xferred_len);
    if (chunk > ep->packet_size) chunk = ep->packet_size;
    if (chunk > MAX_PACKET_SIZE) chunk = MAX_PACKET_SIZE;
    memcpy(USBFS_TX_Buf, ep->buf + ep->xferred_len, chunk);
    USBFSH->HOST_TX_LEN  = chunk;
    USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = (uint8_t)(USBFS_UH_T_AUTO_TOG | USBFS_UH_R_AUTO_TOG | tog);
    USBFSH->INT_FG       = 0xFF;
    USBFSH->HOST_EP_PID  = ep->pid;                             // launch
  } else {
    USBFSH->HOST_TX_LEN  = USBFSH->RX_LEN = 0;
    USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = (uint8_t)(USBFS_UH_T_AUTO_TOG | USBFS_UH_R_AUTO_TOG | tog);
    USBFSH->INT_FG       = 0xFF;
    USBFSH->HOST_EP_PID  = ep->pid;                             // launch
  }
}

static void launch_ep(int8_t idx, bool switch_ep) {
  _hcd.cur_idx = idx;
  ch32_ep_t* ep = &_hcd.ep[idx];
  if (ep->is_setup) xact_setup(ep);
  else              xact_inout(ep, switch_ep);
}

// Start the engine if idle. Must run with the host IRQ masked OR in ISR ctx.
static void try_kick(void) {
  if (_hcd.busy_lock) return;
  int8_t idx = find_next_pending(_hcd.cur_idx);
  if (idx < 0) return;
  _hcd.busy_lock = true;
  launch_ep(idx, true);
}

//--------------------------------------------------------------------+
// Completion handling
//--------------------------------------------------------------------+

enum { CH32_OK, CH32_NAK, CH32_STALL, CH32_ERR, CH32_TIMEOUT };

static uint8_t read_result(void) {
  uint8_t st = USBFSH->INT_ST;
  if (st & USBFS_UIS_TOG_OK) return CH32_OK;          // toggle-matched ACK fast path
  switch (st & USBFS_UIS_H_RES_MASK) {
    case USB_PID_NAK:   return CH32_NAK;
    case USB_PID_STALL: return CH32_STALL;
    case USB_PID_NULL:  return CH32_TIMEOUT;          // 0 = no response / device timed out
    default:            return CH32_ERR;
  }
}

// Finalize the current EP, hand off to usbh, then round-robin to the next.
static void xfer_complete(int8_t idx, xfer_result_t result, bool in_isr) {
  ch32_ep_t* ep = &_hcd.ep[idx];
  uint8_t  ep_addr = (uint8_t)(ep->ep_num | (ep->is_out ? 0 : TUSB_DIR_IN_MASK));
  uint8_t  daddr   = ep->daddr;
  uint16_t xlen    = ep->xferred_len;

  ep->state    = EP_STATE_IDLE;
  ep->is_setup = 0;
  _hcd.armed   = false;

  hcd_event_xfer_complete(daddr, ep_addr, xlen, result, in_isr);

  int8_t nxt = find_next_pending(idx);
  if (nxt < 0) {
    _hcd.busy_lock = false;
    _hcd.cur_idx   = -1;
  } else {
    launch_ep(nxt, true);
  }
}

// Called from the transfer-done IRQ. Stop the engine, decode, advance/finalize.
static void handle_xfer_done(bool in_isr) {
  int8_t idx = _hcd.cur_idx;
  if (idx < 0) return;                                // spurious

  // USBFS host stops the transfer when HOST_EP_PID is zeroed.
  USBFSH->HOST_EP_PID = 0x00;

  ch32_ep_t* ep = &_hcd.ep[idx];

  // Bounds guard: a stale/unexpected daddr would index out of range elsewhere.
  if (ep->daddr >= 0x80 || ep->packet_size == 0) {
    _hcd.armed = false;
    _hcd.busy_lock = false;
    _hcd.cur_idx = -1;
    return;
  }

  uint8_t r = read_result();
  // Hot-unplug signal: a present device's INTERRUPT poll alternates NAK/data; a
  // pulled device returns ONLY timeouts. Count consecutive interrupt-EP timeouts;
  // the SOF tick declares the device gone past the threshold. NOTE: control (ep0)
  // timeouts during enumeration must NOT gate disconnect — a slow-to-enumerate hub
  // would be false-removed. Only steady-state interrupt/bulk polling counts.
  if (ep->ep_num != 0) {
    if (r == CH32_TIMEOUT) _hcd.discon_polls++;
    else                   _hcd.discon_polls = 0;
  }

  if (r == CH32_OK) {
    if (!ep->is_out) {
      // IN: drain RX_LEN bytes into the caller buffer cursor.
      uint16_t n    = (uint16_t) USBFSH->RX_LEN;
      uint16_t room = (uint16_t)(ep->total_len - ep->xferred_len);
      if (n > room) n = room;
      if (n) memcpy(ep->buf + ep->xferred_len, USBFS_RX_Buf, n);
      ep->xferred_len = (uint16_t)(ep->xferred_len + n);
      // AUTO_TOG already flipped the HW toggle; read it back for persistence.
      ep->data_toggle = (USBFSH->HOST_RX_CTRL & USBFS_UH_R_TOG) ? 1 : 0;
      bool done = (n < ep->packet_size) || (ep->xferred_len >= ep->total_len);
      if (done) xfer_complete(idx, XFER_RESULT_SUCCESS, in_isr);
      else      xact_inout(ep, false);                // fetch next IN packet (no EP switch)
    } else {
      // OUT/SETUP advanced by the chunk we just sent.
      uint16_t sent;
      if (ep->is_setup) {
        sent = 8;
      } else {
        sent = (uint16_t)(ep->total_len - ep->xferred_len);
        if (sent > ep->packet_size) sent = ep->packet_size;
      }
      ep->xferred_len = (uint16_t)(ep->xferred_len + sent);
      ep->data_toggle = (USBFSH->HOST_TX_CTRL & USBFS_UH_T_TOG) ? 1 : 0;
      bool was_setup = ep->is_setup;
      ep->is_setup = 0;
      // SETUP is a single packet -> always complete here. OUT continues until done.
      bool done = was_setup || (ep->xferred_len >= ep->total_len);
      if (done) xfer_complete(idx, XFER_RESULT_SUCCESS, in_isr);
      else      xact_inout(ep, false);
    }
  } else if (r == CH32_NAK || r == CH32_TIMEOUT) {
    // NAK = device not ready; TIMEOUT (H_RES=0) = no response this poll (an idle
    // interrupt EP that doesn't even NAK between reports). Both mean "no data yet"
    // -> back off and retry, NEVER complete the transfer (completing would make
    // usbh re-queue and busy-loop the engine at bus rate).
    if (ep->ep_num == 0) {
      // Control: retry in place, do NOT yield the pipe (control owns it across
      // stages). Re-arm the same token at the current cursor.
      if (ep->is_setup) xact_setup(ep);
      else              xact_inout(ep, false);
    } else {
      // Interrupt/bulk: no data this poll. Re-schedule at the EP's bInterval (so
      // we don't hammer the bus every frame) and let other EPs run meanwhile.
      ep->next_frame = (uint16_t)(_hcd.frame_count + ep->interval);
      int8_t nxt = find_next_pending(idx);
      if (nxt < 0) {
        _hcd.busy_lock = false;
        _hcd.cur_idx   = -1;
        _hcd.armed     = false;
      } else {
        launch_ep(nxt, true);
      }
    }
  } else if (r == CH32_STALL) {
    xfer_complete(idx, XFER_RESULT_STALLED, in_isr);
  } else {
// Other handshake error. No logging in ISR.
    xfer_complete(idx, XFER_RESULT_FAILED, in_isr);
  }
}

//--------------------------------------------------------------------+
// Interrupt handler (ISR structure + V307 fixes)
//--------------------------------------------------------------------+

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void) rhport;
  // V307 FIX: do NOT busy-wait for SOF_PRES at IRQ entry — burns ~20ms/IRQ and
  // starves the host stack. Process the pending flags directly.
  uint8_t fg = USBFSH->INT_FG;

  // DETECT edge: fast-path hot-plug attach. The edge toggles spuriously DURING
  // transactions, but only when a device is already attached; while unattached
  // (no transactions in flight) it is a real connect. Clear it either way.
  if (fg & USBFS_UIF_DETECT) {
    USBFSH->INT_FG = USBFS_UIF_DETECT;
    if (!_hcd.attached && (USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH)) {
      _hcd.attached     = true;
      _hcd.discon_polls = 0;
      hcd_event_device_attach(rhport, true);
    }
  }

  // 1ms frame tick (SOF). This is the driver's periodic hook: SOF is generated
  // from hcd_init() on (USBFS_UH_SOF_EN), so the tick runs even with no device.
  if (fg & USBFS_UIF_HST_SOF) {
    USBFSH->INT_FG = USBFS_UIF_HST_SOF;
    _hcd.frame_count++;

    if (!_hcd.attached) {
      // Attach poll — catches a device already present at power-up, which makes
      // no DETECT edge. DEV_ATTACH reflects line state, valid with no SOF reply.
      if (USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) {
        _hcd.attached     = true;
        _hcd.discon_polls = 0;
        hcd_event_device_attach(rhport, true);
      }
    } else if (_hcd.discon_polls >= CH32_HOST_DISCON_POLLS) {
      // No-response disconnect: handle_xfer_done() counts consecutive timeouts
      // (H_RES=0). A present device alternates NAK/data (resets the counter);
      // only a pulled device sustains timeouts. Tear down and report removal.
      USBFSH->HOST_EP_PID = 0;            // stop the dead transaction
      _hcd.discon_polls = 0;
      _hcd.attached     = false;
      _hcd.busy_lock    = false;
      _hcd.armed        = false;
      _hcd.cur_idx      = -1;
      hcd_event_device_remove(rhport, true);
    }

    // Then (if idle) kick the next EP whose interval has elapsed. Per-EP pacing
    // lives in is_ep_pending(next_frame), so each interrupt EP polls at bInterval.
    if (!_hcd.busy_lock) {
      int8_t first = find_next_pending(_hcd.cur_idx);
      if (first >= 0) {
        _hcd.busy_lock = true;
        launch_ep(first, true);
      }
    }
  }

  if (fg & USBFS_UIF_TRANSFER) {
    USBFSH->INT_FG = USBFS_UIF_TRANSFER;
    handle_xfer_done(in_isr);
  }
}

//--------------------------------------------------------------------+
// Controller init / de-init
//--------------------------------------------------------------------+

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;
  tu_memclr(&_hcd, sizeof(_hcd));
  _hcd.cur_idx    = -1;
  _hcd.cur_daddr  = 0xFF;
  _hcd.root_speed = TUSB_SPEED_FULL;

  hcd_int_disable(rhport);

  // Do NOT call USBFS_RCC_Init: the BSP board_init already set a
  // working 48MHz USBFS clock; re-running it clobbers it and the SIE goes silent.

  // Reset SIE, then bring up host mode. All waits bounded.
  USBFSH->BASE_CTRL = USBFS_CTRL_RESET_SIE | USBFS_CTRL_CLR_ALL;
  tusb_time_delay_ms_api(100);
  CH32_WAIT_SIE_IDLE();

  USBFSH->BASE_CTRL = USBFS_CTRL_HOST_MODE;
  tusb_time_delay_ms_api(1);
  CH32_WAIT_SIE_IDLE();

  USBFSH->HOST_CTRL    = 0;
  USBFSH->DEV_ADDR     = 0;
  USBFSH->HOST_EP_MOD  = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;
  USBFSH->HOST_RX_DMA  = (uint32_t) USBFS_RX_Buf;
  USBFSH->HOST_TX_DMA  = (uint32_t) USBFS_TX_Buf;
  USBFSH->HOST_RX_CTRL = 0;
  USBFSH->HOST_TX_CTRL = 0;
  USBFSH->INT_FG       = 0xFF;
  USBFSH->BASE_CTRL    = USBFS_CTRL_HOST_MODE | USBFS_CTRL_INT_BUSY | USBFS_CTRL_DMA_EN;
  // Source VBUS to the downstream device (OTG_CR CHARGE_VBUS, bit1). Without it,
  // bus-powered devices brown out: a lone controller (PSC) and a self-powered hub
  // survive on residual rail, but bus-powered hubs draw more and never answer the
  // first SETUP. The WCH LL USBFS_Host_Init sets this; the async init had dropped
  // it. CHARGE_VBUS only — no OTG_EN session gating.
  USBFSH->OTG_CR       = 0x02u;  // USBFS_CR_CHARGE_VBUS

  // Generate SOF continuously so the SOF interrupt — the driver's periodic tick —
  // runs even before any device attaches. That tick polls DEV_ATTACH, which is
  // how a device already present at power-up (no DETECT edge) gets enumerated.
  USBFSH->HOST_SETUP |= USBFS_UH_SOF_EN;

  hcd_int_enable(rhport);
  USBFSH->INT_EN = USBFS_INT_EN_HST_SOF | USBFS_INT_EN_TRANSFER | USBFS_INT_EN_DETECT;
  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void) rhport;
  USBFSH->BASE_CTRL = USBFS_CTRL_RESET_SIE | USBFS_CTRL_CLR_ALL;
  return true;
}

bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport; (void) cfg_id; (void) cfg_param;
  return false;
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;
  return _hcd.frame_count;
}

void hcd_int_enable(uint8_t rhport)  { (void) rhport; NVIC_EnableIRQ(USBHD_IRQn); }
void hcd_int_disable(uint8_t rhport) { (void) rhport; NVIC_DisableIRQ(USBHD_IRQn); }

//--------------------------------------------------------------------+
// Root port
//--------------------------------------------------------------------+

bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return (USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) != 0;
}

// Begin bus reset. usbh calls hcd_port_reset_end() ~10-50ms later. Keep the
// 100ms attach debounce before reset (complex controllers boot slower).
void hcd_port_reset(uint8_t rhport) {
  // USB spec debounce is ~100ms before reset. The actual reset PULSE is issued in
  // reset_end via the vendor routine (a tight blocking 11ms pulse) — CtrlTransfer
  // only succeeds after the vendor's own reset/enable sequence.
  tusb_time_delay_ms_api(100);
  hcd_int_disable(rhport);
}

void hcd_port_reset_end(uint8_t rhport) {
  // Vendor reset + enable: USBFSH_ResetRootHubPort(0) does SetAddr0 + force-FS +
  // an 11ms BUS_RESET pulse + DETECT-swallow; EnableRootHubPort enables PORT_EN +
  // SOF and reads the speed. This is the exact sequence the WCH host stack uses,
  // and the one CtrlTransfer needs to get a handshake from hubs.
  USBFSH_ResetRootHubPort(0);

  uint8_t sp = 0x01;                                   // USB_FULL_SPEED
  for (int i = 0; i < 20; i++) {
    if (USBFSH_EnableRootHubPort(&sp) == 0 /*ERR_SUCCESS*/) break;
    tusb_time_delay_ms_api(1);
  }
  // USB_FULL_SPEED==1 -> TUSB_SPEED_FULL(0); USB_LOW_SPEED==0 -> TUSB_SPEED_LOW(1).
  _hcd.root_speed = (sp == 0x00) ? TUSB_SPEED_LOW : TUSB_SPEED_FULL;

  // Let SOF run so slow controllers can power up their USB engine before the
  // first SETUP. Bus is active (SOF on), so no suspend risk.
  tusb_time_delay_ms_api(200);

  USBFSH->HOST_RX_DMA = (uint32_t) USBFS_RX_Buf;
  USBFSH->HOST_TX_DMA = (uint32_t) USBFS_TX_Buf;
  USBFSH->INT_FG      = 0xFF;
  hcd_int_enable(rhport);
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  return _hcd.root_speed;
}

void hcd_device_close(uint8_t rhport, uint8_t daddr) {
  (void) rhport;
  // Hot-unplug / address change: flush every ep[] slot owned by the device.
  free_ep_daddr(daddr);
  if (daddr == _hcd.cur_daddr) _hcd.cur_daddr = 0xFF;
}

//--------------------------------------------------------------------+
// Endpoints
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, const tusb_desc_endpoint_t* ep_desc) {
  (void) rhport;
  TU_ASSERT(daddr < 128);

  uint8_t  ep_num = tu_edpt_number(ep_desc->bEndpointAddress);
  uint8_t  dir    = tu_edpt_dir(ep_desc->bEndpointAddress);
  uint16_t mps    = tu_edpt_packet_size(ep_desc);
  if (mps < 8) mps = 8;                               // MPS-floor guard

  ch32_ep_t* ep = find_ep(daddr, ep_num, dir);
  if (ep == NULL) {
    ep = (daddr == 0 && ep_num == 0) ? &_hcd.ep[0] : alloc_ep();
    TU_ASSERT(ep);
  }

  tu_memclr(ep, sizeof(ch32_ep_t));
  ep->daddr       = daddr;
  ep->ep_num      = ep_num;
  ep->is_out      = (dir == TUSB_DIR_OUT);
  ep->is_iso      = (ep_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  ep->packet_size = mps;                              // marks allocated
  ep->data_toggle = 0;
  ep->state       = EP_STATE_IDLE;
  ep->pid         = (uint8_t)(((ep->is_out ? USB_PID_OUT : USB_PID_IN) << 4) | ep_num);
  // Interrupt EPs poll at their bInterval (frames); control/bulk poll every frame.
  ep->interval    = (ep_desc->bmAttributes.xfer == TUSB_XFER_INTERRUPT && ep_desc->bInterval)
                    ? ep_desc->bInterval : 1;
  ep->next_frame  = 0;                                 // pollable immediately
  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  ch32_ep_t* ep = find_ep(daddr, tu_edpt_number(ep_addr), tu_edpt_dir(ep_addr));
  if (ep && ep != &_hcd.ep[0]) tu_memclr(ep, sizeof(ch32_ep_t));
  else if (ep) ep->packet_size = 0;                  // slot0: keep addr0 sentinel
  return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  // CONTROL runs through the vendor LL (USBFSH_CtrlTransfer reads the SETUP from
  // USBFS_TX_Buf, which pUSBFS_SetupRequest aliases). Stage it and tell usbh the
  // SETUP "completed" so it advances to the data/status stage, where we run the
  // whole transfer at once.
  memcpy(USBFS_TX_Buf, setup_packet, 8);
  s_ctrl_pending = true;
  s_ctrl_daddr   = daddr;
  hcd_event_xfer_complete(daddr, 0, 8, XFER_RESULT_SUCCESS, false);
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr,
                   uint8_t* buffer, uint16_t buflen) {
  (void) rhport;
  TU_ASSERT(daddr < 128);

  uint8_t ep_num = tu_edpt_number(ep_addr);
  uint8_t dir    = tu_edpt_dir(ep_addr);

  if (ep_num == 0) {
    // ---- control endpoint: run SETUP+DATA+STATUS via the vendor LL ----
    if (s_ctrl_pending && s_ctrl_daddr == daddr) {
      hcd_int_disable(rhport);
      // Quiesce the async engine so the SIE is free for the synchronous transfer.
      USBFSH->HOST_EP_PID = 0;
      CH32_WAIT_SIE_IDLE();
      _hcd.busy_lock = false; _hcd.cur_idx = -1; _hcd.armed = false;
      // Recover a port the WCH auto-cleared on a disconnect-detect glitch.
      if ((USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) && !(USBFSH->HOST_CTRL & USBFS_UH_PORT_EN)) {
        uint8_t sp = (uint8_t)(_hcd.root_speed == TUSB_SPEED_LOW ? 0 : 1);
        USBFSH_EnableRootHubPort(&sp);
        tusb_time_delay_ms_api(2);
      }
      select_dev_speed(daddr);
      ch32_ep_t* e0 = find_ep(daddr, 0, 0);
      uint8_t  ep0sz = (e0 && e0->packet_size) ? (uint8_t) e0->packet_size : 8;
      uint16_t got   = 0;
      uint8_t  s     = USBFSH_CtrlTransfer(ep0sz, buffer, &got);
      USBFSH->INT_FG = USBFS_UIF_DETECT;                  // swallow DETECT it toggled
      s_ctrl_pending = false;
      hcd_int_enable(rhport);
      xfer_result_t r = (s == 0)                                 ? XFER_RESULT_SUCCESS
                      : (((s & 0x0F) == USB_PID_STALL)           ? XFER_RESULT_STALLED
                                                                 : XFER_RESULT_FAILED);
      hcd_event_xfer_complete(daddr, ep_addr, got, r, false);
    } else {
      // status stage already done inside CtrlTransfer -> complete immediately.
      hcd_event_xfer_complete(daddr, ep_addr, 0, XFER_RESULT_SUCCESS, false);
    }
    return true;
  }

  // ---- interrupt / bulk: async ep[] scheduler ----
  ch32_ep_t* ep = find_ep(daddr, ep_num, dir);
  TU_ASSERT(ep);
  ep->buf         = buffer;
  ep->total_len   = buflen;
  ep->xferred_len = 0;
  ep->state       = EP_STATE_ATTEMPT_1;
  ep->next_frame  = _hcd.frame_count;                  // first poll this frame

  hcd_int_disable(rhport);
  try_kick();
  hcd_int_enable(rhport);
  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  ch32_ep_t* ep = find_ep(daddr, tu_edpt_number(ep_addr), tu_edpt_dir(ep_addr));
  if (!ep) return false;

  hcd_int_disable(rhport);
  if (_hcd.cur_idx >= 0 && &_hcd.ep[_hcd.cur_idx] == ep) {
    // in flight: stop the engine and free the pipe.
    USBFSH->HOST_EP_PID = 0;
    CH32_WAIT_SIE_IDLE();
    _hcd.armed     = false;
    _hcd.busy_lock = false;
    _hcd.cur_idx   = -1;
  }
  ep->state = EP_STATE_IDLE;
  try_kick();
  hcd_int_enable(rhport);
  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  // usbh clears stall via a ClearFeature control transfer; just reset our toggle.
  ch32_ep_t* ep = find_ep(daddr, tu_edpt_number(ep_addr), tu_edpt_dir(ep_addr));
  if (ep) ep->data_toggle = 0;
  return true;
}

#endif
