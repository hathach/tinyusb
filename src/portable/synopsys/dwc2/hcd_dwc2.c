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

enum {
  HPRT_W1_MASK = HPRT_CONN_DETECT | HPRT_ENABLE | HPRT_ENABLE_CHANGE | HPRT_OVER_CURRENT_CHANGE | HPRT_SUSPEND
};

enum {
  HCD_XFER_ERROR_MAX = 3
};

enum {
  HCD_XFER_PERIOD_SPLIT_NYET_MAX = 3,
  HCD_FRAME_NUMBER_MASK = 0x3fff,
  HCD_FRAME_COUNT = HCD_FRAME_NUMBER_MASK + 1
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
    uint32_t aborting        : 1; // periodic DMA channel is waiting for its automatic halt
    uint32_t periodic_phase  : 1; // periodic transfer phase is established
    uint32_t xfer_pending    : 1; // periodic transfer waiting for its service interval
    // uint32_t : 4;
  };

  uint32_t uframe_countdown; // micro-frame count down to transfer for periodic, only need 19-bit

  uint8_t* buffer;
  uint16_t buflen;
  uint16_t periodic_frame; // frame/microframe number of the last scheduled periodic transaction
} hcd_endpoint_t;

// Additional info for each channel when it is active
typedef struct {
  volatile bool allocated;
  uint8_t ep_id;
  struct TU_ATTR_PACKED {
    uint8_t err_count : 3;
    uint8_t period_split_nyet_count : 3;
    uint8_t halted_nyet : 1;
    uint8_t closing : 1; // closing channel
  };
  uint8_t result;

  uint16_t xferred_bytes;  // bytes that accumulate transferred though USB bus for the whole hcd_edpt_xfer(), which can
                           // be composed of multiple channel_xfer_start() (retry with NAK/NYET)
  uint16_t fifo_bytes;     // bytes written/read from/to FIFO (may not be transferred on USB bus).
  uint8_t  retry_disabled; // 1: channel was disabled to throttle a split retry (NAK in / XactErr out); re-arm on its halt
  volatile bool aborting;  // periodic DMA abort waiting for the channel's automatic halt
} hcd_xfer_t;

typedef struct {
  hcd_xfer_t xfer[DWC2_CHANNEL_COUNT_MAX];
  hcd_endpoint_t edpt[CFG_TUH_DWC2_ENDPOINT_MAX];
} hcd_data_t;

static hcd_data_t _hcd_data;
static tuh_configure_dwc2_t _tuh_cfg = {.use_hs_phy = TUH_OPT_HIGH_SPEED};

//--------------------------------------------------------------------
//
//--------------------------------------------------------------------
TU_ATTR_ALWAYS_INLINE static inline uint8_t dwc2_channel_count(const dwc2_regs_t* dwc2) {
  const dwc2_ghwcfg2_t ghwcfg2 = {.value = dwc2->ghwcfg2};
  return tu_min8(ghwcfg2.num_host_ch + 1, DWC2_CHANNEL_COUNT_MAX);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_speed_t hprt_speed_get(dwc2_regs_t* dwc2) {
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

TU_ATTR_ALWAYS_INLINE static inline void channel_dealloc(dwc2_regs_t* dwc2, uint8_t ch_id) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  xfer->allocated = false;
  dwc2->haintmsk &= ~TU_BIT(ch_id);
}

TU_ATTR_ALWAYS_INLINE static inline bool channel_disable(const dwc2_regs_t* dwc2, dwc2_channel_t* channel) {
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

// Enable a channel, selecting the following frame for a new periodic transfer.
// Return that frame from the same HFNUM sample used for ODDFRM selection.
// Clear CHDIS explicitly: a halted channel may retain it in HCCHAR.
TU_ATTR_ALWAYS_INLINE static inline uint16_t channel_enable(dwc2_regs_t* dwc2, dwc2_channel_t* channel,
                                                            bool next_periodic_frame) {
  uint32_t hcchar = channel->hcchar & ~HCCHAR_CHDIS;
  uint16_t periodic_frame = 0;
  if (next_periodic_frame) {
    // Prevent the USB interrupt from consuming the selected frame before
    // HCCHAR.CHENA is written. Queue-space waits happen before this helper.
    const uint32_t gahbcfg = dwc2->gahbcfg;
    dwc2->gahbcfg          = gahbcfg & ~GAHBCFG_GINT;
    const uint32_t hfnum   = dwc2->hfnum;
    hcchar                 = (hcchar & ~HCCHAR_ODDFRM) | (((hfnum & 1u) ^ 1u) << HCCHAR_ODDFRM_Pos);
    channel->hcchar        = hcchar | HCCHAR_CHENA;
    periodic_frame         = (uint16_t) ((hfnum + 1u) & HCD_FRAME_NUMBER_MASK);
    dwc2->gahbcfg          = gahbcfg;
  } else {
    channel->hcchar = hcchar | HCCHAR_CHENA;
  }
  return periodic_frame;
}

// Attempt to send an IN token to receive data. For a new periodic transfer,
// select its frame only after request-queue space is available.
TU_ATTR_ALWAYS_INLINE static inline uint16_t channel_send_in_token(dwc2_regs_t* dwc2, dwc2_channel_t* channel,
                                                                   bool next_periodic_frame) {
  while (0 == req_queue_avail(dwc2, channel_is_periodic(channel->hcchar))) {
    // blocking wait for request queue available
  }
  return channel_enable(dwc2, channel, next_periodic_frame);
}

// Find currently enabled channel. Note: EP0 is bidirectional
TU_ATTR_ALWAYS_INLINE static inline uint8_t channel_find_enabled(dwc2_regs_t* dwc2, uint8_t dev_addr, uint8_t ep_num, uint8_t ep_dir) {
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    if (_hcd_data.xfer[ch_id].allocated) {
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

// close an opened endpoint
static void edpt_close(dwc2_regs_t *dwc2, uint8_t ep_id) {
  hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];
  edpt->closing        = 1; // mark endpoint as closing

  // disable active channel belong to this endpoint
  for (uint8_t ch_id = 0; ch_id < DWC2_CHANNEL_COUNT_MAX; ch_id++) {
    hcd_xfer_t *xfer = &_hcd_data.xfer[ch_id];
    if (xfer->allocated && xfer->ep_id == ep_id) {
      dwc2_channel_t *channel = &dwc2->channel[ch_id];
      xfer->closing           = 1;
      channel_disable(dwc2, channel);
      return; // only 1 active channel per endpoint
    }
  }

  edpt_dealloc(edpt); // no active channel, safe to de-alloc now
}

// Find an endpoint that is opened previously with hcd_edpt_open()
// Note: EP0 is bidirectional
TU_ATTR_ALWAYS_INLINE static inline uint8_t edpt_find_opened(uint8_t dev_addr, uint8_t ep_num, uint8_t ep_dir,
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

TU_ATTR_ALWAYS_INLINE static inline uint8_t cal_next_pid(uint8_t pid, uint8_t packet_count) {
  if (packet_count & 0x01u) {
    return pid ^ 0x02u; // toggle DATA0 and DATA1
  } else {
    return pid;
  }
}

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

  dwc2_channel_split_t* hcsplt_bm = &edpt->hcsplt_bm;
  hcsplt_bm->hub_port        = bus_info.hub_port;
  hcsplt_bm->hub_addr        = bus_info.hub_addr;
  hcsplt_bm->xact_pos        = 0;
  hcsplt_bm->split_compl     = 0;
  hcsplt_bm->split_en        = (rh_speed == TUSB_SPEED_HIGH && bus_info.speed != TUSB_SPEED_HIGH) ? 1 : 0;

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
  }

  /* Since hctsiz.xfersize field reflects the number of bytes transferred via the AHB, not the USB)
   * For IN: we can use hctsiz.xfersize as remaining bytes.
   * For OUT: Must use the hctsiz.pktcnt field to determine how much data has been transferred. This field reflects the
   * number of packets that have been transferred via the USB. This is always an integral number of packets if the
   * transfer was halted before its normal completion.
   */
  const uint16_t remain_packets = hctsiz.packet_count;
  const uint16_t total_packets = cal_packet_count(edpt->buflen, hcchar.ep_size);
  const uint16_t actual_bytes = (total_packets - remain_packets) * hcchar.ep_size;

  xfer->fifo_bytes = 0;
  xfer->xferred_bytes += actual_bytes;
  edpt->buffer += actual_bytes;
  edpt->buflen -= actual_bytes;
}

#if CFG_TUH_DWC2_SLAVE_ENABLE
static bool channel_txfifo_write(dwc2_regs_t* dwc2, uint8_t ch_id, bool is_periodic);
#endif
static void periodic_xfer_defer(dwc2_regs_t* dwc2, hcd_endpoint_t* edpt, uint32_t uframe_countdown);

static bool channel_xfer_start(dwc2_regs_t* dwc2, uint8_t ch_id, bool defer_periodic_out) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_char_t* hcchar_bm = &edpt->hcchar_bm;
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  bool const is_period = channel_is_periodic(edpt->hcchar);
#if CFG_TUH_DWC2_SLAVE_ENABLE
  const uint8_t saved_pid = edpt->next_pid;
  const uint8_t saved_do_ping = edpt->next_do_ping;
#endif
  uint16_t periodic_frame = 0;
  // clear previous state
  xfer->fifo_bytes = 0;

  // hchar: restore but don't enable yet
  channel->hcchar = (edpt->hcchar & ~HCCHAR_CHENA);

  // hctsiz: zero length packet still count as 1
  const uint16_t packet_count = cal_packet_count(edpt->buflen, hcchar_bm->ep_size);
  dwc2_channel_tsize_t hctsiz = {.value = 0};
  hctsiz.pid = edpt->next_pid; // next PID is set in transfer complete interrupt
  hctsiz.packet_count = packet_count;
  hctsiz.xfer_size = edpt->buflen;
  if (edpt->next_do_ping && edpt->speed == TUSB_SPEED_HIGH &&
     edpt->next_pid != HCTSIZ_PID_SETUP && hcchar_bm->ep_dir == TUSB_DIR_OUT) {
    hctsiz.do_ping = 1;
  }
  channel->hctsiz = hctsiz.value;
  edpt->next_do_ping = 0;

  // Single-transaction isochronous endpoints always use DATA0. Pre-calculate the next PID for other endpoints,
  // adjusted in the transfer-complete interrupt if a short packet is received.
  if (hcchar_bm->ep_num == 0) {
    edpt->next_pid = HCTSIZ_PID_DATA1; // control data and status stage always start with DATA1
  } else if (hcchar_bm->ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
    edpt->next_pid = cal_next_pid(edpt->next_pid, packet_count);
  }

  channel->hcsplt = edpt->hcsplt;
  channel->hcint = 0xFFFFFFFFU; // clear all channel interrupts
  dwc2->gintmsk |= GINTSTS_HCINT;

  if (dma_host_enabled(dwc2)) {
    channel->hcintmsk = HCINT_HALTED;
    dwc2->haintmsk |= TU_BIT(ch_id);

    channel->hcdma = (uint32_t) edpt->buffer;

    if (hcchar_bm->ep_dir == TUSB_DIR_IN) {
      periodic_frame = channel_send_in_token(dwc2, channel, is_period);
    } else {
      hcd_dcache_clean(edpt->buffer, edpt->buflen);
      periodic_frame = channel_enable(dwc2, channel, is_period);
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
      if (edpt->hcsplt_bm.split_en || hctsiz.do_ping) {
        hcintmsk |= HCINT_ACK;
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
      periodic_frame = channel_send_in_token(dwc2, channel, is_period);
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
      periodic_frame = channel_enable(dwc2, channel, is_period);
      if (edpt->buflen > 0 && channel_txfifo_write(dwc2, ch_id, is_period)) {
        // The FIFO-empty interrupt handles only work that did not fit in the
        // initial synchronous write.
        dwc2->gintmsk |= (is_period ? GINTSTS_PTX_FIFO_EMPTY : GINTSTS_NPTX_FIFO_EMPTY);
      }
      dwc2->gahbcfg = gahbcfg;
    }
  }
#endif

  if (is_period && defer_periodic_out) {
    edpt->periodic_frame = periodic_frame;
  }

  return true;
}

// kick-off transfer with an endpoint
static bool edpt_xfer_kickoff(dwc2_regs_t* dwc2, uint8_t ep_id) {
  uint8_t ch_id = channel_alloc(dwc2);
  TU_ASSERT(ch_id < 16); // all channel are in used
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  xfer->ep_id = ep_id;
  xfer->result = XFER_RESULT_INVALID;
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  const bool result = channel_xfer_start(dwc2, ch_id, true);
  if (!result) {
    channel_dealloc(dwc2, ch_id);
    periodic_xfer_defer(dwc2, edpt, 0);
    return true;
  }
  if (channel_is_periodic(_hcd_data.edpt[ep_id].hcchar)) {
    edpt->periodic_phase = 1;
    edpt->xfer_pending = 0;
  }
  return result;
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

static void periodic_xfer_defer(dwc2_regs_t* dwc2, hcd_endpoint_t* edpt, uint32_t uframe_countdown) {
  const uint32_t gahbcfg = dwc2->gahbcfg;
  dwc2->gahbcfg = gahbcfg & ~GAHBCFG_GINT;

  edpt->uframe_countdown = uframe_countdown;
  edpt->xfer_pending = 1;

  if (0 == (dwc2->gintmsk & GINTMSK_SOFM)) {
    dwc2->gintsts = GINTSTS_SOF;
    dwc2->gintmsk |= GINTMSK_SOFM;
  }

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

  if (ep_num == 0) {
    // update ep_dir since control endpoint can switch direction
    edpt->hcchar_bm.ep_dir = ep_dir;
  }

  if (channel_is_periodic(edpt->hcchar)) {
    const uint32_t ucount = (hprt_speed_get(dwc2) == TUSB_SPEED_HIGH) ? 1u : 8u;
#if CFG_TUH_DWC2_SLAVE_ENABLE
    // Establish a slower slave-mode OUT schedule from SOF. bInterval=1 must be queued immediately to avoid
    // losing every other service opportunity.
    if (!dma_host_enabled(dwc2) && ep_dir == TUSB_DIR_OUT && !edpt->periodic_phase &&
        edpt->uframe_interval > ucount) {
      periodic_xfer_defer(dwc2, edpt, 0);
      return true;
    }
#endif
    if (edpt->periodic_phase && edpt->uframe_interval > ucount) {
      const uint32_t countdown = periodic_xfer_countdown(dwc2, edpt);
      if (countdown > 0) {
        periodic_xfer_defer(dwc2, edpt, countdown);
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

  hcd_int_disable(rhport);

  const bool xfer_pending = edpt->xfer_pending;
  if (xfer_pending) {
    edpt->xfer_pending = 0;
    edpt->uframe_countdown = 0;
  }

  if (xfer_pending) {
    hcd_int_enable(rhport);
    return true;
  }

  // A periodic DMA channel must halt naturally at the next service boundary. Prevent a replacement transfer until the
  // halt ISR retires the channel, and suppress completion for the aborted transfer.
  if (dma_host_enabled(dwc2) && channel_is_periodic(edpt->hcchar)) {
    const uint8_t ch_id = channel_find_enabled(dwc2, dev_addr, ep_num, ep_dir);
    if (ch_id < 16) {
      hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
      edpt->aborting = 1;
      xfer->aborting = true;
      hcd_int_enable(rhport);
      return true;
    }
  }

  hcd_int_enable(rhport);

  // Channel disable may wait for request-queue space in slave mode.
  // Find enabled channeled and disable it, channel will be de-allocated in the interrupt handler
  const uint8_t ch_id = channel_find_enabled(dwc2, dev_addr, ep_num, ep_dir);
  if (ch_id < 16) {
    dwc2_channel_t* channel = &dwc2->channel[ch_id];
    channel_disable(dwc2, channel);
  }

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

// retry an IN transfer, channel must be halted
static void channel_xfer_in_retry(dwc2_regs_t* dwc2, uint8_t ch_id, uint32_t hcint) {
  hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
  hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
  dwc2_channel_t* channel = &dwc2->channel[ch_id];
  dwc2_channel_char_t hcchar = {.value = channel->hcchar};

  if (channel_is_periodic(hcchar.value)){
    const dwc2_channel_split_t hcsplt = {.value = channel->hcsplt};
    // retry immediately for periodic split NYET if we haven't reach max retry
    if (hcsplt.split_en && hcsplt.split_compl && (hcint & HCINT_NYET || xfer->halted_nyet)) {
      xfer->period_split_nyet_count++;
      xfer->halted_nyet = 0;
      if (xfer->period_split_nyet_count < HCD_XFER_PERIOD_SPLIT_NYET_MAX) {
        hcchar.odd_frame = 1 - (dwc2->hfnum & 1); // transfer on next frame
        channel->hcchar = hcchar.value;
        channel_send_in_token(dwc2, channel, false);
        return;
      } else {
        // too many NYET, de-allocate channel with below code
        xfer->period_split_nyet_count = 0;
      }
    }

    const uint32_t ucount = (hprt_speed_get(dwc2) == TUSB_SPEED_HIGH ? 1 : 8);
    if (edpt->uframe_interval == ucount) {
      // retry on next frame if bInterval is 1
      hcchar.odd_frame = 1 - (dwc2->hfnum & 1);
      channel->hcchar = hcchar.value;
      channel_send_in_token(dwc2, channel, false);
    } else {
      // otherwise, de-allocate channel, enable SOF set frame counter for later transfer
      const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
      if (hcchar.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
        edpt->next_pid = hctsiz.pid; // save PID
      }
      periodic_xfer_defer(dwc2, edpt, periodic_xfer_countdown(dwc2, edpt));
      // already halted, de-allocate channel (called from DMA isr)
      channel_dealloc(dwc2, ch_id);
    }
  } else {
    // for control/bulk: retry immediately
    channel_send_in_token(dwc2, channel, false);
  }
}

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

  for (uint16_t i = 0; i < remain_packets; i++) {
    const uint16_t remain_bytes = edpt->buflen - xfer->fifo_bytes;
    const uint16_t xact_bytes = tu_min16(remain_bytes, hcchar.ep_size);

    // The packet's last FIFO word creates its request-queue entry.
    // HNPTXSTS differs by one request-queue bit, which is outside these fields.
    const dwc2_hptxsts_t txsts = {.value = (is_periodic ? dwc2->hptxsts : dwc2->hnptxsts)};
    if ((xact_bytes > (txsts.fifo_available << 2)) || (txsts.req_queue_available == 0)) {
      return true;
    }

    tu_hwfifo_write(dwc2->fifo[ch_id], edpt->buffer + xfer->fifo_bytes, xact_bytes, NULL);
    xfer->fifo_bytes += xact_bytes;
  }

  return false;
}

// Return true if at least one matching channel needs another interrupt.
static bool handle_txfifo_empty(dwc2_regs_t* dwc2, bool is_periodic) {
  const uint8_t max_channel = dwc2_channel_count(dwc2);
  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
    dwc2_channel_t* channel = &dwc2->channel[ch_id];
    const dwc2_channel_char_t hcchar = {.value = channel->hcchar};
    if (xfer->allocated && channel_is_periodic(hcchar.value) == is_periodic &&
        0 == (channel->hcintmsk & HCINT_HALTED) && hcchar.ep_dir == TUSB_DIR_OUT) {
      if (channel_txfifo_write(dwc2, ch_id, is_periodic)) {
        return true;
      }
    }
  }

  return false;
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
    if (hcsplt.split_en == 1u) {
      hcsplt.split_compl = 0; // restart with start-split
      channel->hcsplt = hcsplt.value;
    }

    channel_disable(dwc2, channel);
  } else if (hcint & HCINT_ACK) {
    xfer->err_count = 0;

    if (hcsplt.split_en == 1u) {
      if (hcsplt.split_compl == 0) {
        // start split is ACK --> do complete split
        channel->hcintmsk |= HCINT_NYET;
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        channel_send_in_token(dwc2, channel, false);
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
        channel_send_in_token(dwc2, channel, false);
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
      channel_xfer_in_retry(dwc2, ch_id, hcint);
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
      channel->hcchar |= HCCHAR_CHENA;
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
      TU_ASSERT(channel_xfer_start(dwc2, ch_id, false));
    }
  } else if (hcint & HCINT_ACK) {
    xfer->err_count = 0;
    channel->hcintmsk &= ~HCINT_ACK;
    if (hcsplt.split_en == 1u) {
      if (hcsplt.split_compl == 0) {
        // ACK for start split --> do complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        channel->hcchar |= HCCHAR_CHENA;
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
      // Halt from our split-NAK throttle disable (below): re-arm the start-split, or let teardown finish
      // if the endpoint is closing. Programming Guide 3.5 "Halting a Channel" (p73).
      xfer->retry_disabled = 0;
      if (xfer->closing) {
        is_done = true;
      } else {
        channel_send_in_token(dwc2, channel, false);
      }
    } else if (hcint & (HCINT_XFER_COMPLETE | HCINT_STALL | HCINT_BABBLE_ERR)) {
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
        channel_xfer_in_retry(dwc2, ch_id, hcint);
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
        channel_xfer_in_retry(dwc2, ch_id, hcint);
      }
    } else if (hcint & HCINT_NYET) {
      // Must handle nyet before nak or ack. Could get a nyet at the same time as either of those on a BULK/CONTROL
      // OUT that started with a PING. The nyet takes precedence.
      if (hcsplt.split_en) {
        // split not yet mean hub has no data, retry complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        channel_xfer_in_retry(dwc2, ch_id, hcint);
      }
    } else if (hcint & HCINT_ACK) {
      xfer->err_count = 0;
      channel->hcintmsk &= ~HCINT_ACK;
      if (hcsplt.split_en) {
        // start split is ACK --> do complete split
        // TODO: for ISO must use xact_pos to plan complete split based on microframe (up to 187.5 bytes/uframe)
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        if (channel_is_periodic(channel->hcchar)) {
          hcchar.odd_frame = 1 - (dwc2->hfnum & 1); // transfer on next frame
          channel->hcchar = hcchar.value;
        }
        channel_send_in_token(dwc2, channel, false);
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
        channel_disable(dwc2, channel);
      } else {
        channel_xfer_in_retry(dwc2, ch_id, hcint);
      }
    } else if (hcint & HCINT_FARME_OVERRUN) {
      if (hcchar.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
        xfer->result = XFER_RESULT_FAILED;
        is_done      = true;
      } else {
        channel_xfer_in_retry(dwc2, ch_id, hcint);
      }
    }

    if (xfer->closing == 1) {
      is_done = true;
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
      if (xfer->closing) {
        is_done = true;
      } else {
        channel_xfer_start(dwc2, ch_id, false);
      }
    } else if (hcint & (HCINT_XFER_COMPLETE | HCINT_STALL)) {
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
        channel_xfer_start(dwc2, ch_id, false);
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
            channel_xfer_start(dwc2, ch_id, false);
          }
        }
      }
    } else if (hcint & HCINT_FARME_OVERRUN) {
      channel_xfer_out_wrapup(dwc2, ch_id);
      if (edpt->hcchar_bm.ep_type == HCCHAR_EPTYPE_ISOCHRONOUS) {
        xfer->result = XFER_RESULT_FAILED;
        is_done      = true;
      } else {
        channel_xfer_start(dwc2, ch_id, false);
      }
    } else if (hcint & HCINT_NYET) {
      if (hcsplt.split_en && hcsplt.split_compl) {
        // split not yet mean hub has no data, retry complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        channel->hcchar |= HCCHAR_CHENA;
      }
    } else if (hcint & HCINT_ACK) {
      xfer->err_count = 0;
      if (hcsplt.split_en && !hcsplt.split_compl) {
        // start split is ACK --> do complete split
        hcsplt.split_compl = 1;
        channel->hcsplt = hcsplt.value;
        channel->hcchar |= HCCHAR_CHENA;
      }
    } else if ((hcint & HCINT_NAK) && hcsplt.split_en) {
      // Split OUT NAK: rewind + retry the start-split, else the channel stalls (Programming Guide 5.1.4.2).
      // Non-split OUT NAK is core-handled (5.1.2.2), so this is split-only.
      xfer->err_count = 0;
      channel_xfer_out_wrapup(dwc2, ch_id);
      channel_xfer_start(dwc2, ch_id, false);
    }

    if (xfer->closing == 1) {
      is_done = true;
    }
  } else if (hcint & HCINT_ACK) {
    xfer->err_count = 0;
    channel->hcintmsk &= ~HCINT_ACK;
  }

  return is_done;
}
#endif

static void handle_channel_irq(uint8_t rhport, bool in_isr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const bool is_dma = dma_host_enabled(dwc2);
  const uint8_t max_channel = dwc2_channel_count(dwc2);

  for (uint8_t ch_id = 0; ch_id < max_channel; ch_id++) {
    if (tu_bit_test(dwc2->haint, ch_id)) {
      dwc2_channel_t* channel = &dwc2->channel[ch_id];
      hcd_xfer_t* xfer = &_hcd_data.xfer[ch_id];
      TU_ASSERT(xfer->ep_id < CFG_TUH_DWC2_ENDPOINT_MAX,);
      dwc2_channel_char_t hcchar = {.value = channel->hcchar};

      const uint32_t hcint = channel->hcint;
      // Slave handlers process one cause per pass. If ChHltd arrived with
      // another cause, leave it pending so the next pass retires the halt.
      const uint32_t hcint_clear = (!is_dma && (hcint & ~HCINT_HALTED)) ? (hcint & ~HCINT_HALTED) : hcint;
      channel->hcint = hcint_clear;

      if (is_dma && xfer->aborting && (hcint & HCINT_HALTED)) {
        hcd_endpoint_t* edpt = &_hcd_data.edpt[xfer->ep_id];
        const bool closing = xfer->closing;
        // channel_xfer_start() predicts the PID after all requested packets;
        // an aborted transfer may have completed fewer.
        if (hcchar.ep_type != HCCHAR_EPTYPE_ISOCHRONOUS) {
          const dwc2_channel_tsize_t hctsiz = {.value = channel->hctsiz};
          edpt->next_pid = hctsiz.pid;
        }
        xfer->aborting = false;
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
          if (is_done && (channel->hcdma > xfer->xferred_bytes)) {
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
        if (xfer->closing == 1) {
          hcd_endpoint_t *edpt = &_hcd_data.edpt[xfer->ep_id];
          edpt_dealloc(edpt);
        } else {
          const uint8_t ep_addr = tu_edpt_addr(hcchar.ep_num, hcchar.ep_dir);
          hcd_event_xfer_complete(hcchar.dev_addr, ep_addr, xfer->xferred_bytes, (xfer_result_t)xfer->result, in_isr);
        }
        channel_dealloc(dwc2, ch_id);
      }
    }
  }
}

// SOF is enabled for scheduled periodic transfer
static bool handle_sof_irq(uint8_t rhport, bool in_isr) {
  (void) in_isr;
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  dwc2->gintsts = GINTSTS_SOF; // Clear the SOF interrupt flag

  bool more_isr = false;

  // If highspeed then SOF is 125us, else 1ms
  const uint32_t ucount = (hprt_speed_get(dwc2) == TUSB_SPEED_HIGH ? 1 : 8);

  for(uint8_t ep_id = 0; ep_id < CFG_TUH_DWC2_ENDPOINT_MAX; ep_id++) {
    hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];
    if (edpt->closing == 0) {
      if (edpt->hcchar_bm.enable && channel_is_periodic(edpt->hcchar) && edpt->xfer_pending) {
        if (edpt->uframe_countdown > 0) {
          edpt->uframe_countdown -= tu_min32(ucount, edpt->uframe_countdown);
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
    } else {
      // TU_ASSERT(false, );
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
  if ((dwc2->gintsts & dwc2->gintmsk) & GINTSTS_HCINT) {
    handle_channel_irq(rhport, in_isr);
  }

}

#endif
