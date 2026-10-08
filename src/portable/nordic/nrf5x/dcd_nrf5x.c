/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUD_ENABLED && CFG_TUSB_MCU == OPT_MCU_NRF5X

// Suppress warning caused by nrfx driver
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include "nrf.h"
#include "nrfx_clock.h"
#include "nrf_erratas.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "device/dcd.h"

// TODO remove later
#include "device/usbd.h"
#include "device/usbd_pvt.h" // for usbd_spin_lock()

#if CFG_TUSB_OS == OPT_OS_MYNEWT
#include "mcu/mcu.h"
#endif

/* Try to detect nrfx version if not configured with CFG_TUD_NRF_NRFX_VERSION
 * nrfx v1 and v2 are concurrently developed. There is no NRFX_VERSION only MDK VERSION which is as follows:
 * - v3.0.0: 8.53.1 (conflict with v2.11.0), v3.1.0: 8.55.0 ...
 * - v2.11.0: 8.53.1, v2.6.0: 8.44.1, v2.5.0: 8.40.2, v2.4.0: 8.37.0, v2.3.0: 8.35.0, v2.2.0: 8.32.1, v2.1.0: 8.30.2,
 * v2.0.0: 8.29.0
 * - v1.9.0: 8.40.3, v1.8.6: 8.35.0 (conflict with v2.3.0), v1.8.5: 8.32.3, v1.8.4: 8.32.1 (conflict with v2.2.0),
 *   v1.8.2: 8.32.1 (conflict with v2.2.0), v1.8.1: 8.27.1
 * Therefore the check for v1 would be:
 * - MDK < 8.29.0 (v2.0), MDK == 8.32.3, 8.40.3
 * - in case of conflict User of those version must upgrade to other 1.x version or set CFG_TUD_NRF_NRFX_VERSION
 */
#ifndef CFG_TUD_NRF_NRFX_VERSION
  #define MDK_VERSION (10000 * MDK_MAJOR_VERSION + 100 * MDK_MINOR_VERSION + MDK_MICRO_VERSION)

  #if MDK_VERSION < 82900 || MDK_VERSION == 83203 || MDK_VERSION == 84003
    // nrfx <= 1.8.1, or 1.8.5 or 1.9.0
    #define CFG_TUD_NRF_NRFX_VERSION 1
  #elif MDK_VERSION < 85301
    #define CFG_TUD_NRF_NRFX_VERSION 2
  #elif MDK_VERSION < 87300
    #define CFG_TUD_NRF_NRFX_VERSION 3
  #else
    #define CFG_TUD_NRF_NRFX_VERSION 4
  #endif
#endif

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
enum {
  // Max allowed by USB specs
  MAX_PACKET_SIZE = 64,

  // Mask of all END event (IN & OUT) for all endpoints. ENDEPIN0-7, ENDEPOUT0-7, ENDISOIN, ENDISOOUT
  EDPT_END_ALL_MASK = (0xff << USBD_INTEN_ENDEPIN0_Pos) | (0xff << USBD_INTEN_ENDEPOUT0_Pos) |
                      USBD_INTENCLR_ENDISOIN_Msk | USBD_INTEN_ENDISOOUT_Msk
};

enum {
  EP_ISO_NUM = 8, // Endpoint number is fixed (8) for ISOOUT and ISOIN
  EP_CBI_COUNT = 8  // Control Bulk Interrupt endpoints count
};

// Transfer Descriptor
typedef struct {
  uint8_t* buffer;
  uint16_t total_len;
  volatile uint16_t actual_len;
  uint16_t mps; // max packet size

  // nRF will auto accept OUT packet after DMA is done, indicate packet is already ACK
  volatile bool data_received;
  volatile bool started;

  // Bumped at every arm and retire (stall, SETUP), wrapping at 256: an ENDEPOUT carrying another
  // id belongs to a retired transfer, whatever the td holds by then.
  volatile uint8_t xferid;
  uint8_t dma_xferid;

  // Set to true when data was transferred from RAM to ISO IN output buffer.
  // New data can be put in ISO IN output buffer after SOF.
  bool iso_in_transfer_ready;

} xfer_td_t;

// Data for managing dcd
static struct {
  // All 8 endpoints including control IN & OUT (offset 1)
  // +1 for ISO endpoints
  xfer_td_t xfer[EP_CBI_COUNT + 1][2];

  // EasyDMA requests waiting for the channel (DMA_REQ_*), started by dma_dispatch_isr() in the USBD ISR.
  // Written only by the USBD ISR and by task code holding usbd_spin_lock().
  uint32_t dma_pending;

  // nRF can only carry one DMA at a time; owned by the USBD ISR
  bool dma_running;
  bool dma_discard; // the running DMA's transfer was torn down: its END only releases the channel
  uint8_t dma_rr; // bit position of the last bulk/interrupt request started, for round-robin

  // Track whether sof has been manually enabled
  bool sof_enabled;
} _dcd;

/*------------------------------------------------------------------*/
/* Control / Bulk / Interrupt (CBI) Transfer
 *------------------------------------------------------------------*/

// check if we are in ISR
TU_ATTR_ALWAYS_INLINE static inline bool is_in_isr(void) {
  return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) ? true : false;
}

// Errata 199 "USBD cannot receive tasks during DMA": while an EasyDMA transfer is in progress the
// controller may drop an incoming SETUP/IN/OUT token (lost event -> stuck EP0, esp. under rapid
// back-to-back control transfers). The workaround latches an undocumented "DMA in progress" test
// register (0x40027C1C) so tokens are held instead. Gated on the anomaly being present (all
// nRF52840 revisions; absent on other nRF52 parts). Mirrors nrfx usbd_dma_pending_set/clear().
#define NRF_USBD_ERRATA_199_REG (*((volatile uint32_t*) 0x40027C1CUL))

// helper to start DMA
static void dma_trigger_isr(volatile uint32_t* reg_startep) {
  _dcd.dma_running = true;
  if (nrf52_errata_199()) {
    NRF_USBD_ERRATA_199_REG = 0x00000082UL;
  }

  (*reg_startep) = 1;
  __ISB();
  __DSB();
}

// helper getting td
TU_ATTR_ALWAYS_INLINE static inline xfer_td_t* get_td(uint8_t epnum, uint8_t dir) {
  return &_dcd.xfer[epnum][dir];
}

// EasyDMA requests, one bit each: EPIN n at bit n, EPOUT n at bit 16+n (ISO is n = 8), plus the two
// EP0 tasks that only need the channel to be free.
#define DMA_REQ_EP0STATUS TU_BIT(9)
#define DMA_REQ_EP0RCVOUT TU_BIT(10)
#define DMA_REQ_EP0       (TU_BIT(0) | TU_BIT(16) | DMA_REQ_EP0STATUS | DMA_REQ_EP0RCVOUT)
#define DMA_REQ_ISO       (TU_BIT(EP_ISO_NUM) | TU_BIT(16 + EP_ISO_NUM))

TU_ATTR_ALWAYS_INLINE static inline uint32_t dma_req_bit(uint8_t epnum, uint8_t dir) {
  return TU_BIT(epnum + (dir == TUSB_DIR_OUT ? 16u : 0u));
}

// USBD ISR, or task holding usbd_spin_lock() then pending the USBD IRQ
TU_ATTR_ALWAYS_INLINE static inline void dma_request(uint32_t req) {
  _dcd.dma_pending |= req;
}

// its completion, re-arm and DMA request must no longer see the retired transfer; same context as dma_request()
static inline void retire_xfer(uint8_t epnum, uint8_t dir) {
  xfer_td_t* xfer = get_td(epnum, dir);
  xfer->started = false;
  xfer->xferid++;
  _dcd.dma_pending &= ~(epnum == 0 ? DMA_REQ_EP0 : dma_req_bit(epnum, dir));
}

static void dma_start_out_isr(uint8_t epnum);
static void dma_start_in_isr(uint8_t epnum);

// Start queued requests while the channel is free: EP0 first, then ISO, then the other endpoints
// round-robin. USBD ISR only, after its END events released the channel.
static void dma_dispatch_isr(void) {
  // NRF_USBD->ENABLE: an unplug disabled USBD; what is still pending is dropped by the next bus reset
  while (!_dcd.dma_running && _dcd.dma_pending && NRF_USBD->ENABLE) {
    uint32_t const pending = _dcd.dma_pending;
    uint32_t req = pending & DMA_REQ_EP0;
    if (req == 0) {
      req = pending & DMA_REQ_ISO;
    }
    bool const is_cbi = (req == 0);
    if (is_cbi) {
      req = pending & (UINT32_MAX << _dcd.dma_rr << 1);
      if (req == 0) {
        req = pending;
      }
    }

    uint8_t const pos = (uint8_t) __CLZ(__RBIT(req));
    _dcd.dma_pending &= ~TU_BIT(pos);
    if (is_cbi) {
      _dcd.dma_rr = pos;
    }

    if (TU_BIT(pos) & (DMA_REQ_EP0STATUS | DMA_REQ_EP0RCVOUT)) {
      // these seem to need EasyDMA to be available, however they don't trigger any DMA transfer:
      // the channel stays free and no ERRATA-199 latch is needed
      if (TU_BIT(pos) == DMA_REQ_EP0STATUS) {
        NRF_USBD->TASKS_EP0STATUS = 1;
      } else {
        NRF_USBD->TASKS_EP0RCVOUT = 1;
      }
      __ISB();
      __DSB();
    } else if (pos >= 16) {
      dma_start_out_isr(pos - 16);
    } else {
      dma_start_in_isr(pos);
    }
  }
}

// DMA is complete, dma_dispatch_isr() starts the next one. Returns whether its transfer was torn down.
static bool dma_release(void) {
  // Clear the ERRATA-199 "DMA in progress" latch set in dma_trigger_isr().
  if (nrf52_errata_199()) {
    NRF_USBD_ERRATA_199_REG = 0x00000000UL;
  }
  _dcd.dma_running = false;
  bool const discard = _dcd.dma_discard;
  _dcd.dma_discard = false;
  return discard;
}

// Whether the running DMA's END is latched, consuming it if asked. Only one DMA runs and the ISR consumes
// every END (all enabled since bus reset) before starting another, so any latched END is that DMA's.
static bool dma_end_latched(bool consume) {
  volatile uint32_t* const regevt = &NRF_USBD->EVENTS_USBRESET;
  bool latched = false;
  for (uint8_t i = USBD_INTEN_ENDEPIN0_Pos; i <= USBD_INTEN_ENDISOOUT_Pos; i++) {
    if (tu_bit_test(EDPT_END_ALL_MASK, i) && regevt[i]) {
      latched = true;
      if (consume) {
        regevt[i] = 0;
      }
    }
  }
  return latched;
}

// Start DMA to move data from Endpoint -> RAM. Called by dma_dispatch_isr() with the channel free, as
// SIZE.EPOUT or SIZE.ISOOUT can't be read while a DMA is active.
static void dma_start_out_isr(uint8_t epnum) {
  xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);
  uint32_t xact_len;

  xfer->dma_xferid = xfer->xferid;
  if (epnum == EP_ISO_NUM) {
    xact_len = NRF_USBD->SIZE.ISOOUT;
    // If ZERO bit is set, ignore ISOOUT length
    if (!(xact_len & USBD_SIZE_ISOOUT_ZERO_Msk) && xfer->started) {
      // Trigger DMA move data from Endpoint -> SRAM
      NRF_USBD->ISOOUT.PTR = (uint32_t) xfer->buffer;
      NRF_USBD->ISOOUT.MAXCNT = xact_len;

      dma_trigger_isr(&NRF_USBD->TASKS_STARTISOOUT);
    }
  } else {
    // limit xact len to remaining length
    xact_len = tu_min16((uint16_t) NRF_USBD->SIZE.EPOUT[epnum], xfer->total_len - xfer->actual_len);

    // Trigger DMA move data from Endpoint -> SRAM
    NRF_USBD->EPOUT[epnum].PTR = (uint32_t) xfer->buffer;
    NRF_USBD->EPOUT[epnum].MAXCNT = xact_len;

    dma_trigger_isr(&NRF_USBD->TASKS_STARTEPOUT[epnum]);
  }
}

// Prepare for a CBI transaction IN, called by dma_dispatch_isr()
// it start DMA to transfer data from RAM -> Endpoint
static void dma_start_in_isr(uint8_t epnum) {
  xfer_td_t* xfer = get_td(epnum, TUSB_DIR_IN);

  // Each transaction is up to Max Packet Size
  uint16_t const xact_len = tu_min16(xfer->total_len - xfer->actual_len, xfer->mps);

  NRF_USBD->EPIN[epnum].PTR = (uint32_t) xfer->buffer;
  NRF_USBD->EPIN[epnum].MAXCNT = xact_len;

  dma_trigger_isr(&NRF_USBD->TASKS_STARTEPIN[epnum]);
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+
bool dcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rhport;
  (void) rh_init;
  TU_LOG2("dcd init\r\n");
  return true;
}

void dcd_int_enable(uint8_t rhport) {
  (void) rhport;
  NVIC_EnableIRQ(USBD_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
  (void) rhport;
  NVIC_DisableIRQ(USBD_IRQn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;
  (void) dev_addr;
  // Set Address is automatically update by hw controller, nothing to do

  // Enable usbevent for suspend and resume detection
  // Since the bus signal D+/D- are stable now.

  // Clear current pending first
  NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;
  NRF_USBD->EVENTS_USBEVENT = 0;

  NRF_USBD->INTENSET = USBD_INTEN_USBEVENT_Msk;
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void) rhport;

  // Bring controller out of low power mode
  // will start wakeup when USBWUALLOWED is set
  NRF_USBD->LOWPOWER = 0;
}

// disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport) {
  (void) rhport;
  NRF_USBD->USBPULLUP = 0;

  // Disable Pull-up does not trigger Power USB Removed, in fact it have no
  // impact on the USB Power status at all -> need to submit unplugged event to the stack.
  dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, false);
}

// connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport) {
  (void) rhport;
  NRF_USBD->USBPULLUP = 1;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void) rhport;
  if (en) {
    _dcd.sof_enabled = true;
    NRF_USBD->INTENSET = USBD_INTENSET_SOF_Msk;
  } else {
    _dcd.sof_enabled = false;
    NRF_USBD->INTENCLR = USBD_INTENCLR_SOF_Msk;
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const* desc_edpt) {
  (void) rhport;

  uint8_t const ep_addr = desc_edpt->bEndpointAddress;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  _dcd.xfer[epnum][dir].mps = tu_edpt_packet_size(desc_edpt);

  if (desc_edpt->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS) {
    if (dir == TUSB_DIR_OUT) {
      NRF_USBD->EPOUTEN |= TU_BIT(epnum);

      // Write any value to SIZE register will allow nRF to ACK/accept data
      NRF_USBD->SIZE.EPOUT[epnum] = 0;
    } else {
      NRF_USBD->EPINEN |= TU_BIT(epnum);
    }
    // clear stall and reset DataToggle
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | ep_addr;
    NRF_USBD->DTOGGLE = (USBD_DTOGGLE_VALUE_Data0 << USBD_DTOGGLE_VALUE_Pos) | ep_addr;
  } else {
    TU_ASSERT(epnum == EP_ISO_NUM);
    if (dir == TUSB_DIR_OUT) {
      // SPLIT ISO buffer when ISO IN endpoint is already opened.
      if (_dcd.xfer[EP_ISO_NUM][TUSB_DIR_IN].mps) NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

      // Clear SOF event in case interrupt was not enabled yet.
      if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0) NRF_USBD->EVENTS_SOF = 0;

      // Enable SOF interrupt and ISOOUT endpoint (END interrupts are on since bus reset).
      NRF_USBD->INTENSET = USBD_INTENSET_SOF_Msk;
      NRF_USBD->EPOUTEN |= USBD_EPOUTEN_ISOOUT_Msk;
    } else {
      // SPLIT ISO buffer when ISO OUT endpoint is already opened.
      if (_dcd.xfer[EP_ISO_NUM][TUSB_DIR_OUT].mps) NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

      // Clear SOF event in case interrupt was not enabled yet.
      if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0) NRF_USBD->EVENTS_SOF = 0;

      // Enable SOF interrupt and ISOIN endpoint (END interrupts are on since bus reset).
      NRF_USBD->INTENSET = USBD_INTENSET_SOF_Msk;
      NRF_USBD->EPINEN |= USBD_EPINEN_ISOIN_Msk;
    }
  }

  __ISB();
  __DSB();

  return true;
}

void dcd_edpt_close_all(uint8_t rhport) {
  (void) rhport;
  // disable interrupt to prevent race condition
  usbd_spin_lock(false);
  _dcd.dma_pending &= DMA_REQ_EP0;

  // A running DMA keeps the channel until its END, which stays enabled and only releases it: its transfer
  // goes now, an EP0 one included (handle_setup_isr() retired EP0 for this SET_CONFIGURATION). Wait for
  // that END before the stack reuses the buffers (PS 6.35.8), unless a bus reset already took it over.
  while (_dcd.dma_running && !_dcd.dma_discard && !dma_end_latched(false) && !NRF_USBD->EVENTS_USBRESET) {}
  _dcd.dma_discard = _dcd.dma_running;

  // disable all non-control (bulk + interrupt) endpoints
  for (uint8_t ep = 1; ep < EP_CBI_COUNT; ep++) {
    NRF_USBD->TASKS_STARTEPIN[ep] = 0;
    NRF_USBD->TASKS_STARTEPOUT[ep] = 0;

    tu_memclr(_dcd.xfer[ep], 2 * sizeof(xfer_td_t));
  }

  // disable both ISO
  NRF_USBD->INTENCLR = USBD_INTENCLR_SOF_Msk;
  NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_OneDir;

  NRF_USBD->TASKS_STARTISOIN = 0;
  NRF_USBD->TASKS_STARTISOOUT = 0;

  tu_memclr(_dcd.xfer[EP_ISO_NUM], 2 * sizeof(xfer_td_t));

  // de-activate all non-control
  NRF_USBD->EPOUTEN = 1UL;
  NRF_USBD->EPINEN = 1UL;

  usbd_spin_unlock(false);
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  (void)rhport;
  (void)largest_packet_size;
  // nRF ISO endpoints are hardware-fixed to EP8 and use EasyDMA, so there is no packet buffer to
  // pre-allocate here; the endpoint is enabled on dcd_edpt_iso_activate().
  TU_ASSERT(tu_edpt_number(ep_addr) == EP_ISO_NUM);
  return true;
}

bool dcd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
  (void)rhport;
  uint8_t const ep_addr = desc_ep->bEndpointAddress;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);
  TU_ASSERT(epnum == EP_ISO_NUM);

  // A transfer armed before SET_INTERFACE survives to here (this port has no dcd close); usbd has
  // just reset the endpoint's claim/busy state, so drop the stale descriptor too — otherwise the
  // class's next arm trips TU_ASSERT(!xfer->started) in dcd_edpt_xfer().
  xfer_td_t* xfer = get_td(epnum, dir);
  usbd_spin_lock(false);
  retire_xfer(epnum, dir);
  xfer->data_received         = false;
  xfer->iso_in_transfer_ready = false;
  usbd_spin_unlock(false);

  xfer->mps = tu_edpt_packet_size(desc_ep);

  if (dir == TUSB_DIR_OUT) {
    // SPLIT ISO buffer when the ISO IN endpoint is already active.
    if (_dcd.xfer[EP_ISO_NUM][TUSB_DIR_IN].mps) NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;
    if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0) NRF_USBD->EVENTS_SOF = 0;
    NRF_USBD->INTENSET = USBD_INTENSET_SOF_Msk;
    NRF_USBD->EPOUTEN |= USBD_EPOUTEN_ISOOUT_Msk;
  } else {
    // SPLIT ISO buffer when the ISO OUT endpoint is already active.
    if (_dcd.xfer[EP_ISO_NUM][TUSB_DIR_OUT].mps) NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;
    if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0) NRF_USBD->EVENTS_SOF = 0;
    NRF_USBD->INTENSET = USBD_INTENSET_SOF_Msk;
    NRF_USBD->EPINEN |= USBD_EPINEN_ISOIN_Msk;
  }

  __ISB();
  __DSB();
  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes, bool is_isr) {
  (void) rhport;
  (void) is_isr;
  // VECTACTIVE is the exception number, IRQn + 16 (ARMv7-M B3.2.4, ARMv8-M D1.2 ICSR)
  uint32_t const vectactive = SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk;
  bool const in_isr = vectactive != 0;
  bool const in_usbd_isr = vectactive == (uint32_t) USBD_IRQn + 16u;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  xfer_td_t* xfer = get_td(epnum, dir);

  TU_ASSERT(!xfer->started);

  // Control endpoint with zero-length packet and opposite direction to 1st request byte --> status stage
  bool const control_status = (epnum == 0 && total_bytes == 0 && dir != tu_edpt_dir((uint8_t)NRF_USBD->BMREQUESTTYPE));

  if (control_status) {
    // The nRF doesn't interrupt on status transmit so we queue up a success response.
    dcd_event_xfer_complete(0, ep_addr, 0, XFER_RESULT_SUCCESS, in_isr);
  }

  // a SETUP or EPDATA handled by the ISR must not interleave with arming the td and its DMA request
  uint32_t req = 0;
  usbd_spin_lock(in_usbd_isr);
  xfer->xferid++;
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->actual_len = 0;

  if (control_status) {
    // Status Phase also requires EasyDMA has to be available as well !!!!
    req = DMA_REQ_EP0STATUS;
  } else if (dir == TUSB_DIR_OUT) {
    xfer->started = true;
    if (epnum == 0) {
      // Accept next Control Out packet. TASKS_EP0RCVOUT also require EasyDMA
      req = DMA_REQ_EP0RCVOUT;
    } else if (xfer->data_received) {
      // a packet nRF auto-accepted before this transfer was armed waits in the endpoint
      xfer->data_received = false;
      req = dma_req_bit(epnum, TUSB_DIR_OUT);
    } else {
      // nRF auto accept next Bulk/Interrupt OUT packet, EPDATA starts its DMA
    }
  } else {
    // Start DMA to copy data from RAM -> Endpoint
    xfer->started = true;
    req = dma_req_bit(epnum, TUSB_DIR_IN);
  }
  dma_request(req);
  usbd_spin_unlock(in_usbd_isr);

  // dma_dispatch_isr() runs in the USBD ISR: in it, dcd_int_handler() calls it last
  if (req && !in_usbd_isr) {
    NVIC_SetPendingIRQ(USBD_IRQn);
  }

  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  if (epnum == EP_ISO_NUM) {
    return; // EPSTALL selects endpoints 0..7 only: isochronous has no halt
  }

  xfer_td_t* xfer = get_td(epnum, dir);

  // retire before the ISR can continue a multi-packet transfer into the disarmed buffer
  usbd_spin_lock(false);
  if (epnum == 0) {
    NRF_USBD->TASKS_EP0STALL = 1;
  } else {
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_Stall << USBD_EPSTALL_STALL_Pos) | ep_addr;

    if (dir == TUSB_DIR_OUT) {
      // a packet nRF auto-ACKed before the stall stays in the endpoint buffer until
      // dcd_edpt_clear_stall() writes SIZE.EPOUT, which lets the next one overwrite it
      xfer->data_received = false;
    } else {
      // EPSTALL does not discard a packet already loaded into the IN buffer: it would go out
      // as soon as the halt clears. Disarm it through the undocumented test register nrfx 3.14's
      // usbd_ep_abort() uses on every USBD part, nRF5340 included (0x7B6 + 2*(n-1), bit 1).
      // The access sequence is nrfx's verbatim, including its read before the read-modify-write.
      volatile uint32_t* const test_reg = (volatile uint32_t*) ((uintptr_t) NRF_USBD + 0x800);
      test_reg[0] = 0x7B6 + 2u * (epnum - 1u);
      const uint8_t disarm = (uint8_t) (test_reg[1] | TU_BIT(1));
      test_reg[1] |= disarm;
      (void) test_reg[1];
    }
  }
  retire_xfer(epnum, dir);
  usbd_spin_unlock(false);

  __ISB();
  __DSB();
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  if (epnum != 0 && epnum != EP_ISO_NUM) {
    // reset data toggle to DATA0
    // First write this register with VALUE=Nop to select the endpoint, then either read it to get the status from
    // VALUE, or write it again with VALUE=Data0 or Data1
    NRF_USBD->DTOGGLE = ep_addr;
    NRF_USBD->DTOGGLE = (USBD_DTOGGLE_VALUE_Data0 << USBD_DTOGGLE_VALUE_Pos) | ep_addr;

    // clear stall
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | ep_addr;

    // Write any value to SIZE register will allow nRF to ACK/accept data
    if (dir == TUSB_DIR_OUT) NRF_USBD->SIZE.EPOUT[epnum] = 0;

    __ISB();
    __DSB();
  }
}

/*------------------------------------------------------------------*/
/* Interrupt Handler
 *------------------------------------------------------------------*/
static void bus_reset_isr(void) {
  // 6.35.6 USB controller automatically disabled all endpoints (except control)
  NRF_USBD->EPOUTEN = 1UL;
  NRF_USBD->EPINEN = 1UL;

  for (int i = 0; i < 8; i++) {
    NRF_USBD->TASKS_STARTEPIN[i] = 0;
    NRF_USBD->TASKS_STARTEPOUT[i] = 0;
  }

  NRF_USBD->TASKS_STARTISOIN = 0;
  NRF_USBD->TASKS_STARTISOOUT = 0;

  // Clear USB Event Interrupt
  NRF_USBD->EVENTS_USBEVENT = 0;
  NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;

  // Reset interrupt
  NRF_USBD->INTENCLR = NRF_USBD->INTEN;
  NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_EPDATA_Msk |
                       USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | EDPT_END_ALL_MASK;

  // A DMA still running keeps the channel until its END, which only releases it. Whether USBRESET aborts
  // a DMA without END is undocumented (PS 6.35.6); if so the channel stays owned.
  bool const dma_running = _dcd.dma_running;
  tu_varclr(&_dcd);
  _dcd.dma_running = dma_running;
  _dcd.dma_discard = dma_running;
  _dcd.xfer[0][TUSB_DIR_IN].mps = MAX_PACKET_SIZE;
  _dcd.xfer[0][TUSB_DIR_OUT].mps = MAX_PACKET_SIZE;
}

static void handle_sof_isr(uint32_t int_status) {
  bool iso_enabled = false;

  // ISOOUT: Transfer data gathered in previous frame from buffer to RAM
  if (NRF_USBD->EPOUTEN & USBD_EPOUTEN_ISOOUT_Msk) {
    iso_enabled = true;
    // Transfer from endpoint to RAM only if data is not corrupted
    if ((int_status & USBD_INTEN_USBEVENT_Msk) == 0 ||
        (NRF_USBD->EVENTCAUSE & USBD_EVENTCAUSE_ISOOUTCRC_Msk) == 0) {
      dma_request(dma_req_bit(EP_ISO_NUM, TUSB_DIR_OUT));
    }
  }

  // ISOIN: Notify client that data was transferred
  if (NRF_USBD->EPINEN & USBD_EPINEN_ISOIN_Msk) {
    iso_enabled = true;

    xfer_td_t* xfer = get_td(EP_ISO_NUM, TUSB_DIR_IN);
    if (xfer->iso_in_transfer_ready) {
      xfer->iso_in_transfer_ready = false;
      xfer->started = false;
      dcd_event_xfer_complete(0, EP_ISO_NUM | TUSB_DIR_IN_MASK, xfer->actual_len, XFER_RESULT_SUCCESS, true);
    }
  }

  if (!iso_enabled && !_dcd.sof_enabled) {
    // SOF interrupt not manually enabled and ISO endpoint is not used,
    // SOF is only enabled one-time for remote wakeup so we disable it now

    NRF_USBD->INTENCLR = USBD_INTENCLR_SOF_Msk;
  }

  const uint32_t frame = NRF_USBD->FRAMECNTR;
  dcd_event_sof(0, frame, true);
  //dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
}

static void handle_usbevent_isr(void) {
  TU_LOG(3, "EVENTCAUSE = 0x%04" PRIX32 "\r\n", NRF_USBD->EVENTCAUSE);

  enum {
    EVT_CAUSE_MASK = USBD_EVENTCAUSE_SUSPEND_Msk | USBD_EVENTCAUSE_RESUME_Msk | USBD_EVENTCAUSE_USBWUALLOWED_Msk |
                     USBD_EVENTCAUSE_ISOOUTCRC_Msk
  };
  uint32_t const evt_cause = NRF_USBD->EVENTCAUSE & EVT_CAUSE_MASK;
  NRF_USBD->EVENTCAUSE = evt_cause; // clear interrupt

  if (evt_cause & USBD_EVENTCAUSE_SUSPEND_Msk) {
    // Put controller into low power mode
    // Leave HFXO disable to application, since it may be used by other peripherals
    NRF_USBD->LOWPOWER = 1;

    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }

  if (evt_cause & USBD_EVENTCAUSE_USBWUALLOWED_Msk) {
    // USB is out of low power mode, and wakeup is allowed
    // Initiate RESUME signal
    NRF_USBD->DPDMVALUE = USBD_DPDMVALUE_STATE_Resume;
    NRF_USBD->TASKS_DPDMDRIVE = 1;

    // There is no Resume interrupt for remote wakeup, enable SOF for to report bus ready state
    // Clear SOF event in case interrupt was not enabled yet.
    if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0) NRF_USBD->EVENTS_SOF = 0;
    NRF_USBD->INTENSET = USBD_INTENSET_SOF_Msk;
  }

  if (evt_cause & USBD_EVENTCAUSE_RESUME_Msk) {
    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }
}

static void handle_setup_isr(void) {
  // a SETUP supersedes an EP0 transfer the host abandoned, e.g. a data stage it stopped reading
  for (uint8_t dir = 0; dir < 2; dir++) {
    retire_xfer(0, dir);
  }
  uint8_t const setup[8] = {
      NRF_USBD->BMREQUESTTYPE, NRF_USBD->BREQUEST, NRF_USBD->WVALUEL, NRF_USBD->WVALUEH,
      NRF_USBD->WINDEXL, NRF_USBD->WINDEXH, NRF_USBD->WLENGTHL, NRF_USBD->WLENGTHH
  };

  // nrf5x hw auto handle set address, there is no need to inform usb stack
  tusb_control_request_t const* request = (tusb_control_request_t const*) setup;

  if (!(TUSB_REQ_RCPT_DEVICE == request->bmRequestType_bit.recipient &&
        TUSB_REQ_TYPE_STANDARD == request->bmRequestType_bit.type &&
        TUSB_REQ_SET_ADDRESS == request->bRequest)) {
    dcd_event_setup_received(0, setup, true);
  }
}

//--------------------------------------------------------------------+
/* Control/Bulk/Interrupt (CBI) Transfer
 *
 * Data flow is:
 *           (bus)              (dma)
 *    Host <-------> Endpoint <-------> RAM
 *
 * For CBI OUT:
 *  - Host -> Endpoint
 *      EPDATA (or EP0DATADONE) interrupted, check EPDATASTATUS.EPOUT[i]
 *      to start DMA. For Bulk/Interrupt, this step can occur automatically (without sw),
 *      which means data may or may not be ready (data_received flag).
 *  - Endpoint -> RAM
 *      ENDEPOUT[i] interrupted, transaction complete, sw prepare next transaction
 *
 * For CBI IN:
 *  - RAM -> Endpoint
 *      ENDEPIN[i] interrupted indicate DMA is complete. HW will start
 *      to move data to host
 *  - Endpoint -> Host
 *      EPDATA (or EP0DATADONE) interrupted, check EPDATASTATUS.EPIN[i].
 *      Transaction is complete, sw prepare next transaction
 *
 * Note: in both Control In and Out of Data stage from Host <-> Endpoint
 * EP0DATADONE will be set as interrupt source
 */
//--------------------------------------------------------------------+

static void handle_epdata_isr(uint32_t int_status) {
  uint32_t data_status = NRF_USBD->EPDATASTATUS;
  NRF_USBD->EPDATASTATUS = data_status;
  __ISB();
  __DSB();

  // EP0DATADONE is set with either Control Out on IN Data
  // Since EPDATASTATUS cannot be used to determine whether it is control OUT or IN.
  // We will use BMREQUESTTYPE in setup packet to determine the direction
  bool const is_control_in = (int_status & USBD_INTEN_EP0DATADONE_Msk) && (NRF_USBD->BMREQUESTTYPE & TUSB_DIR_IN_MASK);
  bool const is_control_out = (int_status & USBD_INTEN_EP0DATADONE_Msk) && !(NRF_USBD->BMREQUESTTYPE & TUSB_DIR_IN_MASK);

  // CBI In: Endpoint -> Host (transaction complete)
  for (uint8_t epnum = 0; epnum < EP_CBI_COUNT; epnum++) {
    if (tu_bit_test(data_status, epnum) || (epnum == 0 && is_control_in)) {
      xfer_td_t* xfer = get_td(epnum, TUSB_DIR_IN);
      if (!xfer->started) {
        continue; // retired by dcd_edpt_stall() before the packet went out
      }
      uint8_t const xact_len = NRF_USBD->EPIN[epnum].AMOUNT;

      xfer->buffer += xact_len;
      xfer->actual_len += xact_len;

      if (xfer->actual_len < xfer->total_len) {
        // Start DMA to copy next data packet
        dma_request(dma_req_bit(epnum, TUSB_DIR_IN));
      } else {
        // CBI IN complete
        xfer->started = false;
        dcd_event_xfer_complete(0, epnum | TUSB_DIR_IN_MASK, xfer->actual_len, XFER_RESULT_SUCCESS, true);
      }
    }
  }

  // CBI OUT: Host -> Endpoint
  for (uint8_t epnum = 0; epnum < EP_CBI_COUNT; epnum++) {
    if (tu_bit_test(data_status, 16 + epnum) || (epnum == 0 && is_control_out)) {
      xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);

      // an armed zero-length read still needs the 0-byte DMA to complete
      if (xfer->started && (xfer->total_len == 0 || xfer->actual_len < xfer->total_len)) {
        dma_request(dma_req_bit(epnum, TUSB_DIR_OUT));
      } else {
        // Data overflow !!! Nah, nRF will auto accept next Bulk/Interrupt OUT packet
        // Mark this endpoint with data received
        xfer->data_received = true;
      }
    }
  }
}

static void handle_out_end_isr(uint32_t int_status) {
  /* CBI OUT: Endpoint -> SRAM (aka transaction complete)
   * Note: Since nRF controller auto ACK next packet without SW awareness
   * We must handle this stage before Host -> Endpoint just in case 2 event happens at once
   *
   * ISO OUT: Transaction must fit in single packet, it can be shorter then total
   * len if Host decides to sent fewer bytes, it this case transaction is also
   * complete and next transfer is not initiated here like for CBI.
   */
  for (uint8_t epnum = 0; epnum < EP_CBI_COUNT + 1; epnum++) {
    if (tu_bit_test(int_status, USBD_INTEN_ENDEPOUT0_Pos + epnum)) {
      xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);
      if (!xfer->started || xfer->dma_xferid != xfer->xferid) {
        continue; // no armed transfer, or the DMA was started for one since retired: the packet is dropped
      }
      uint16_t const xact_len = NRF_USBD->EPOUT[epnum].AMOUNT;

      xfer->buffer += xact_len;
      xfer->actual_len += xact_len;

      // Transfer complete if transaction len < Max Packet Size or total len is transferred
      if ((epnum != EP_ISO_NUM) && (xact_len == xfer->mps) && (xfer->actual_len < xfer->total_len)) {
        if (epnum == 0) {
          // Accept next Control Out packet. TASKS_EP0RCVOUT also require EasyDMA
          dma_request(DMA_REQ_EP0RCVOUT);
        } else {
          // nRF auto accept next Bulk/Interrupt OUT packet
          // nothing to do
        }
      } else {
        xfer->total_len = xfer->actual_len;
        xfer->started = false;

        // CBI OUT complete
        dcd_event_xfer_complete(0, epnum, xfer->actual_len, XFER_RESULT_SUCCESS, true);
      }
    }

    // Ended event for CBI IN : nothing to do
  }
}

void dcd_int_handler(uint8_t rhport) {
  (void) rhport;
  uint32_t const inten = NRF_USBD->INTEN;
  uint32_t int_status = 0;

  volatile uint32_t* regevt = &NRF_USBD->EVENTS_USBRESET;

  for (uint8_t i = 0; i < USBD_INTEN_EPDATA_Pos + 1; i++) {
    if (tu_bit_test(inten, i) && regevt[i]) {
      int_status |= TU_BIT(i);

      // event clear
      regevt[i] = 0;
      __ISB();
      __DSB();
    }
  }

  if (int_status & USBD_INTEN_USBRESET_Msk) {
    // transfer events captured with the reset belong to transfers it discards; an END still releases below
    int_status &= ~(USBD_INTEN_EPDATA_Msk | USBD_INTEN_EP0DATADONE_Msk);
    bus_reset_isr();
    dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
  }

  // DMA complete move data from SRAM <-> Endpoint. Release before the END handlers below, which must not
  // see the END of a torn-down transfer, and before dma_dispatch_isr() starts the next request.
  if ((int_status & EDPT_END_ALL_MASK) && dma_release()) {
    int_status &= ~EDPT_END_ALL_MASK;
  }

  // ISOIN: Data was moved to endpoint buffer, client will be notified in SOF
  if (int_status & USBD_INTEN_ENDISOIN_Msk) {
    xfer_td_t* xfer = get_td(EP_ISO_NUM, TUSB_DIR_IN);

    xfer->actual_len = NRF_USBD->ISOIN.AMOUNT;
    // Data transferred from RAM to endpoint output buffer.
    // Next transfer can be scheduled after SOF.
    xfer->iso_in_transfer_ready = true;
  }

  if (int_status & USBD_INTEN_SOF_Msk) {
    handle_sof_isr(int_status);
  }

  if (int_status & USBD_INTEN_USBEVENT_Msk) {
    handle_usbevent_isr();
  }

  if (int_status & USBD_INTEN_EP0SETUP_Msk) {
    handle_setup_isr();
  }

  // OUT END first: EPDATA may already report the next packet
  handle_out_end_isr(int_status);

  if (int_status & (USBD_INTEN_EPDATA_Msk | USBD_INTEN_EP0DATADONE_Msk)) {
    handle_epdata_isr(int_status);
  }

  // after END events released the channel, also on a USBD IRQ pended by dcd_edpt_xfer()
  dma_dispatch_isr();
}

//--------------------------------------------------------------------+
// HFCLK helper
//--------------------------------------------------------------------+
#ifdef SOFTDEVICE_PRESENT

// For enable/disable hfclk with SoftDevice
#include "nrf_mbr.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"

#ifndef SD_MAGIC_NUMBER
  #define SD_MAGIC_NUMBER   0x51B1E5DB
#endif

TU_ATTR_ALWAYS_INLINE static inline bool is_sd_existed(void) {
  return *((uint32_t*)(SOFTDEVICE_INFO_STRUCT_ADDRESS+4)) == SD_MAGIC_NUMBER;
}

// check if SD is existed and enabled
TU_ATTR_ALWAYS_INLINE static inline bool is_sd_enabled(void) {
  if ( !is_sd_existed() ) return false;
  uint8_t sd_en = false;
  (void) sd_softdevice_is_enabled(&sd_en);
  return sd_en;
}
#endif

static bool hfclk_running(void) {
  #ifdef SOFTDEVICE_PRESENT
  if (is_sd_enabled()) {
    uint32_t is_running = 0;
    (void)sd_clock_hfclk_is_running(&is_running);
    return (is_running ? true : false);
  }
  #endif

  #if CFG_TUD_NRF_NRFX_VERSION == 1
  return nrf_clock_hf_is_running(NRF_CLOCK_HFCLK_HIGH_ACCURACY);
  #elif CFG_TUD_NRF_NRFX_VERSION == 2
  // nrfx 2.0.0 (MDK 8.29.0) has no nrf_clock_is_running(); it arrived in 2.1.0.
  // nrf_clock_hf_is_running() is present in all of 2.0.0-2.11.0 (deprecated from 2.1.0).
  return nrf_clock_hf_is_running(NRF_CLOCK, NRF_CLOCK_HFCLK_HIGH_ACCURACY);
  #else
  return nrf_clock_is_running(NRF_CLOCK, NRF_CLOCK_DOMAIN_HFCLK, NULL);
  #endif
}

static void hfclk_enable(void) {
#if CFG_TUSB_OS == OPT_OS_MYNEWT
  usb_clock_request();
  return;
#else

  // already running, nothing to do
  if (hfclk_running()) {
    return;
  }

  #ifdef SOFTDEVICE_PRESENT
  if (is_sd_enabled()) {
    (void)sd_clock_hfclk_request();
    return;
  }
  #endif

  #if CFG_TUD_NRF_NRFX_VERSION == 1
  nrf_clock_event_clear(NRF_CLOCK_EVENT_HFCLKSTARTED);
  nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTART);
  #else
  nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED);
  nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTART);
  #endif
#endif
}

static void hfclk_disable(void) {
#if CFG_TUSB_OS == OPT_OS_MYNEWT
  usb_clock_release();
  return;
#else

#ifdef SOFTDEVICE_PRESENT
  if ( is_sd_enabled() ) {
    (void)sd_clock_hfclk_release();
    return;
  }
#endif

#if CFG_TUD_NRF_NRFX_VERSION == 1
  nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTOP);
#else
  nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
#endif
#endif
}

// Power & Clock Peripheral on nRF5x to manage USB
//
// USB Bus power is managed by Power module, there are 3 VBUS power events:
// Detected, Ready, Removed. Upon these power events, This function will
// enable ( or disable ) usb & hfclk peripheral, set the usb pin pull up
// accordingly to the controller Startup/Standby Sequence in USBD 51.4 specs.
//
// Therefore this function must be called to handle USB power event by
// - nrfx_power_usbevt_init() : if Softdevice is not used or enabled
// - SoftDevice SOC event : if SD is used and enabled
void tusb_hal_nrf_power_event(uint32_t event);
void tusb_hal_nrf_power_event(uint32_t event) {
  // Value is chosen to be as same as NRFX_POWER_USB_EVT_* in nrfx_power.h
  enum {
    USB_EVT_DETECTED = 0,
    USB_EVT_REMOVED = 1,
    USB_EVT_READY = 2
  };

#if CFG_TUSB_DEBUG >= 3
  const char* const power_evt_str[] = {"Detected", "Removed", "Ready"};
  TU_LOG(3, "Power USB event: %s\r\n", power_evt_str[event]);
#endif

  switch (event) {
    case USB_EVT_DETECTED:
      if (!NRF_USBD->ENABLE) {
        // Prepare for receiving READY event: disable interrupt since we will blocking wait
        NRF_USBD->INTENCLR = USBD_INTEN_USBEVENT_Msk;
        NRF_USBD->EVENTCAUSE = USBD_EVENTCAUSE_READY_Msk;
        __ISB();
        __DSB(); // for sync

#ifdef NRF52_SERIES // NRF53 does not need this errata
        // ERRATA 171, 187, 166
        if (nrf52_errata_187()) {
          // CRITICAL_REGION_ENTER();
          if (*((volatile uint32_t*) (0x4006EC00)) == 0x00000000) {
            *((volatile uint32_t*) (0x4006EC00)) = 0x00009375;
            *((volatile uint32_t*) (0x4006ED14)) = 0x00000003;
            *((volatile uint32_t*) (0x4006EC00)) = 0x00009375;
          } else {
            *((volatile uint32_t*) (0x4006ED14)) = 0x00000003;
          }
          // CRITICAL_REGION_EXIT();
        }

        if (nrf52_errata_171()) {
          // CRITICAL_REGION_ENTER();
          if (*((volatile uint32_t*) (0x4006EC00)) == 0x00000000) {
            *((volatile uint32_t*) (0x4006EC00)) = 0x00009375;
            *((volatile uint32_t*) (0x4006EC14)) = 0x000000C0;
            *((volatile uint32_t*) (0x4006EC00)) = 0x00009375;
          } else {
            *((volatile uint32_t*) (0x4006EC14)) = 0x000000C0;
          }
          // CRITICAL_REGION_EXIT();
        }
#endif

        // Enable the peripheral (will cause Ready event)
        NRF_USBD->ENABLE = 1;
        __ISB();
        __DSB(); // for sync

        // Enable HFCLK
        hfclk_enable();
      }
      break;

    case USB_EVT_READY:
      // Skip if pull-up is enabled and HCLK is already running.
      // Application probably call this more than necessary.
      if (NRF_USBD->USBPULLUP && hfclk_running()) break;

      // Waiting for USBD peripheral enabled
      while (!(USBD_EVENTCAUSE_READY_Msk & NRF_USBD->EVENTCAUSE)) {}

      NRF_USBD->EVENTCAUSE = USBD_EVENTCAUSE_READY_Msk;
      __ISB();
      __DSB(); // for sync

#ifdef NRF52_SERIES
      if (nrf52_errata_171()) {
        // CRITICAL_REGION_ENTER();
        if (*((volatile uint32_t*) (0x4006EC00)) == 0x00000000) {
          *((volatile uint32_t*) (0x4006EC00)) = 0x00009375;
          *((volatile uint32_t*) (0x4006EC14)) = 0x00000000;
          *((volatile uint32_t*) (0x4006EC00)) = 0x00009375;
        } else {
          *((volatile uint32_t*) (0x4006EC14)) = 0x00000000;
        }

        // CRITICAL_REGION_EXIT();
      }

      if (nrf52_errata_187()) {
        // CRITICAL_REGION_ENTER();
        if (*((volatile uint32_t*) (0x4006EC00)) == 0x00000000) {
          *((volatile uint32_t*) (0x4006EC00)) = 0x00009375;
          *((volatile uint32_t*) (0x4006ED14)) = 0x00000000;
          *((volatile uint32_t*) (0x4006EC00)) = 0x00009375;
        } else {
          *((volatile uint32_t*) (0x4006ED14)) = 0x00000000;
        }
        // CRITICAL_REGION_EXIT();
      }

      if (nrf52_errata_166()) {
        *((volatile uint32_t*) (NRF_USBD_BASE + 0x800)) = 0x7E3;
        *((volatile uint32_t*) (NRF_USBD_BASE + 0x804)) = 0x40;

        __ISB();
        __DSB();
      }
#endif

      // ISO buffer Lower half for IN, upper half for OUT
      NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

      // Enable bus-reset interrupt
      NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk;

      // Enable interrupt, priorities should be set by application
      NVIC_ClearPendingIRQ(USBD_IRQn);

      // Don't enable USBD interrupt yet, if dcd_init() did not finish yet
      // Interrupt will be enabled by tud_init(), when USB stack is ready
      // to handle interrupts.
      if (tud_inited()) {
        NVIC_EnableIRQ(USBD_IRQn);
      }

      // Ensure HFCLK is requested in the current context. The hfclk_enable() in
      // USB_EVT_DETECTED may have been pre-SoftDevice. After Softdevice is
      // enabled, HFXO is physically off again. So any caller that fires
      // USB_EVT_READY post-SD would hang here.
      hfclk_enable();

      // Wait for HFCLK
      while (!hfclk_running()) {}

      // Enable pull up
      NRF_USBD->USBPULLUP = 1;
      __ISB();
      __DSB(); // for sync
      break;

    case USB_EVT_REMOVED:
      if (NRF_USBD->ENABLE) {
        // Abort all transfers

        // Disable pull up
        NRF_USBD->USBPULLUP = 0;
        __ISB();
        __DSB(); // for sync

        // Disable Interrupt
        NVIC_DisableIRQ(USBD_IRQn);

        // disable all interrupt
        NRF_USBD->INTENCLR = NRF_USBD->INTEN;

        // PS 6.35.4: let a running EasyDMA end before disabling USBD. This handler must not preempt the USBD
        // ISR (BSP: POWER/SoftDevice below USBD priority). A DMA whose END does not come in time is abandoned
        // with USBD: no END would ever release the channel, which bus reset keeps owned after the next plug.
        if (_dcd.dma_running) {
          for (uint32_t n = SystemCoreClock / 1000; n > 0 && !dma_end_latched(false); n--) {}
          (void) dma_end_latched(true);
          (void) dma_release();
        }
        _dcd.dma_pending = 0;

        NRF_USBD->ENABLE = 0;
        __ISB();
        __DSB(); // for sync

        hfclk_disable();

        dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, is_in_isr());
      }
      break;

    default:
      break;
  }
}
#endif
