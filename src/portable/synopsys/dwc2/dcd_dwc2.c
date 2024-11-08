/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 William D. Jones
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Jan Duempelmann
 * Copyright (c) 2020 Reinhard Panhuber
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

#if CFG_TUD_ENABLED && defined(TUP_USBIP_DWC2)

// Debug level for DWC2
#define DWC2_DEBUG    2

#include "device/dcd.h"
#include "dwc2_common.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

static CFG_TUD_MEM_SECTION TU_ATTR_ALIGNED(4) uint32_t _setup_packet[2];

typedef struct {
  uint8_t* buffer;
  tu_fifo_t* ff;
  uint16_t total_len;
  uint16_t max_size;
  uint8_t interval;
} xfer_ctl_t;

static xfer_ctl_t xfer_status[DWC2_EP_MAX][2];
#define XFER_CTL_BASE(_ep, _dir) (&xfer_status[_ep][_dir])

// EP0 transfers are limited to 1 packet - larger sizes has to be split
static uint16_t ep0_pending[2];  // Index determines direction as tusb_dir_t type
static uint16_t _dfifo_top;      // top free location in DFIFO in words

// Number of IN endpoints active
static uint8_t _allocated_ep_in_count;

// SOF enabling flag - required for SOF to not get disabled in ISR when SOF was enabled by
static bool _sof_en;

//--------------------------------------------------------------------
// DMA
//--------------------------------------------------------------------

TU_ATTR_ALWAYS_INLINE static inline bool dma_device_enabled(const dwc2_regs_t* dwc2) {
  (void) dwc2;
  // Internal DMA only
  return CFG_TUD_DWC2_DMA && dwc2->ghwcfg2_bm.arch == GHWCFG2_ARCH_INTERNAL_DMA;
}

static void dma_setup_prepare(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  if (dwc2->gsnpsid >= DWC2_CORE_REV_3_00a) {
    if(dwc2->epout[0].doepctl & DOEPCTL_EPENA) {
      return;
    }
  }

  // Receive only 1 packet
  dwc2->epout[0].doeptsiz = (1 << DOEPTSIZ_STUPCNT_Pos) | (1 << DOEPTSIZ_PKTCNT_Pos) | (8 << DOEPTSIZ_XFRSIZ_Pos);
  dwc2->epout[0].doepdma = (uintptr_t)_setup_packet;
  dwc2->epout[0].doepctl |= DOEPCTL_EPENA | DOEPCTL_USBAEP;
}

//--------------------------------------------------------------------+
// Data FIFO
//--------------------------------------------------------------------+


/* Device Data FIFO scheme

  The FIFO is split up into
  - EPInfo: for storing DMA metadata, only required when use DMA. Maximum size is called
    EP_LOC_CNT = ep_fifo_size - ghwcfg3.dfifo_depth. For value less than EP_LOC_CNT, gdfifocfg must be configured before
    gahbcfg.dmaen is set
      - Buffer mode: 1 word per endpoint direction
      - Scatter/Gather DMA: 4 words per endpoint direction
  - TX FIFO: one fifo for each IN endpoint. Size is dynamic depending on packet size, starting from top with EP0 IN.
  - Shared RX FIFO: a shared fifo for all OUT endpoints. Typically, can hold up to 2 packets of the largest EP size.

  We allocated TX FIFO from top to bottom (using top pointer), this to allow the RX FIFO to grow dynamically which is
  possible since the free space is located between the RX and TX FIFOs.

   ---------------- ep_fifo_size
  |  DxEPIDMAn  |
  |-------------|-- gdfifocfg.EPINFOBASE (max is ghwcfg3.dfifo_depth)
  | IN FIFO 0   |       control EP
  |-------------|
  | IN FIFO 1   |
  |-------------|
  |   . . . .   |
  |-------------|
  | IN FIFO n   |
  |-------------|
  |    FREE     |
  |-------------|-- GRXFSIZ (expandable)
  |  OUT FIFO   |
  | ( Shared )  |
  --------------- 0

  According to "FIFO RAM allocation" section in RM, FIFO RAM are allocated as follows (each word 32-bits):
  - Each EP IN needs at least max packet size
  - All EP OUT shared a unique OUT FIFO which uses (for Slave or Buffer DMA, Scatt/Gather DMA use different formula):
    - 13 for setup packets + control words (up to 3 setup packets).
    - 1 for global NAK (not required/used here).
    - Largest-EPsize/4 + 1. ( FS: 64 bytes, HS: 512 bytes). Recommended is  "2 x (Largest-EPsize/4 + 1)"
    - 2 for each used OUT endpoint

    Therefore GRXFSIZ = 13 + 1 + 2 x (Largest-EPsize/4 + 1) + 2 x EPOUTnum
*/

TU_ATTR_ALWAYS_INLINE static inline uint16_t calc_device_grxfsiz(uint16_t largest_ep_size, uint8_t ep_count) {
  return 13 + 1 + 2 * ((largest_ep_size / 4) + 1) + 2 * ep_count;
}

static bool dfifo_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t packet_size) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const dwc2_controller_t* dwc2_controller = &_dwc2_controller[rhport];
  uint8_t const ep_count = dwc2_controller->ep_count;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  TU_ASSERT(epnum < ep_count);

  uint16_t fifo_size = tu_div_ceil(packet_size, 4);
  if (dir == TUSB_DIR_OUT) {
    // Calculate required size of RX FIFO
    uint16_t const new_sz = calc_device_grxfsiz(4 * fifo_size, ep_count);

    // If size_rx needs to be extended check if there is enough free space
    if (dwc2->grxfsiz < new_sz) {
      TU_ASSERT(new_sz <= _dfifo_top);
      dwc2->grxfsiz = new_sz; // Enlarge RX FIFO
    }
  } else {
    // Check IN endpoints concurrently active limit
    if(_dwc2_controller->ep_in_count) {
      TU_ASSERT(_allocated_ep_in_count < _dwc2_controller->ep_in_count);
      _allocated_ep_in_count++;
    }

    // If The TXFELVL is configured as half empty, the fifo must be twice the max_size.
    if ((dwc2->gahbcfg & GAHBCFG_TX_FIFO_EPMTY_LVL) == 0) {
      fifo_size *= 2;
    }

    // Check if free space is available
    TU_ASSERT(_dfifo_top >= fifo_size + dwc2->grxfsiz);
    _dfifo_top -= fifo_size;
    TU_LOG(DWC2_DEBUG, "    TX FIFO %u: allocated %u words at offset %u\r\n", epnum, fifo_size, _dfifo_top);

    // Both TXFD and TXSA are in unit of 32-bit words.
    if (epnum == 0) {
      dwc2->dieptxf0 = (fifo_size << DIEPTXF0_TX0FD_Pos) | _dfifo_top;
    } else {
      // DIEPTXF starts at FIFO #1.
      dwc2->dieptxf[epnum - 1] = (fifo_size << DIEPTXF_INEPTXFD_Pos) | _dfifo_top;
    }
  }

  return true;
}

static void dfifo_device_init(uint8_t rhport) {
  const dwc2_controller_t* dwc2_controller = &_dwc2_controller[rhport];
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  dwc2->grxfsiz = calc_device_grxfsiz(CFG_TUD_ENDPOINT0_SIZE, dwc2_controller->ep_count);

  // Scatter/Gather DMA mode is not yet supported. Buffer DMA only need 1 words per endpoint direction
  const bool is_dma = dma_device_enabled(dwc2);
  _dfifo_top = dwc2_controller->ep_fifo_size/4;
  if (is_dma) {
    _dfifo_top -= 2 * dwc2_controller->ep_count;
  }
  dwc2->gdfifocfg = (_dfifo_top << GDFIFOCFG_EPINFOBASE_SHIFT) | _dfifo_top;

  // Allocate FIFO for EP0 IN
  dfifo_alloc(rhport, 0x80, CFG_TUD_ENDPOINT0_SIZE);
}


//--------------------------------------------------------------------
// Endpoint
//--------------------------------------------------------------------
static void edpt_activate(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint8_t const epnum = tu_edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t const dir = tu_edpt_dir(p_endpoint_desc->bEndpointAddress);

  xfer_ctl_t* xfer = XFER_CTL_BASE(epnum, dir);
  xfer->max_size = tu_edpt_packet_size(p_endpoint_desc);
  xfer->interval = p_endpoint_desc->bInterval;

  // USBAEP, EPTYP, SD0PID_SEVNFRM, MPSIZ are the same for IN and OUT endpoints.
  uint32_t epctl = (1 << DOEPCTL_USBAEP_Pos) |
                   (p_endpoint_desc->bmAttributes.xfer << DOEPCTL_EPTYP_Pos) |
                   (p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS ? DOEPCTL_SD0PID_SEVNFRM : 0) |
                   (xfer->max_size << DOEPCTL_MPSIZ_Pos);
  if (dir == TUSB_DIR_IN) {
    epctl |= (epnum << DIEPCTL_TXFNUM_Pos);
  }

  dwc2_dep_t* dep = &dwc2->ep[1 - dir][epnum];
  dep->ctl = epctl;
  dwc2->daintmsk |= TU_BIT(epnum + DAINT_SHIFT(dir));
}

static void edpt_disable(uint8_t rhport, uint8_t ep_addr, bool stall) {
  (void) rhport;

  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const uint8_t epnum = tu_edpt_number(ep_addr);
  const uint8_t dir = tu_edpt_dir(ep_addr);
  dwc2_dep_t* dep = &dwc2->ep[1 - dir][epnum];

  if (dir == TUSB_DIR_IN) {
    // Only disable currently enabled non-control endpoint
    if ((epnum == 0) || !(dep->diepctl & DIEPCTL_EPENA)) {
      dep->diepctl |= DIEPCTL_SNAK | (stall ? DIEPCTL_STALL : 0);
    } else {
      // Stop transmitting packets and NAK IN xfers.
      dep->diepctl |= DIEPCTL_SNAK;
      while ((dep->diepint & DIEPINT_INEPNE) == 0) {}

      // Disable the endpoint.
      dep->diepctl |= DIEPCTL_EPDIS | (stall ? DIEPCTL_STALL : 0);
      while ((dep->diepint & DIEPINT_EPDISD_Msk) == 0) {}

      dep->diepint = DIEPINT_EPDISD;
    }

    // Flush the FIFO, and wait until we have confirmed it cleared.
    dfifo_flush_tx(dwc2, epnum);
  } else {
    // Only disable currently enabled non-control endpoint
    if ((epnum == 0) || !(dep->doepctl & DOEPCTL_EPENA)) {
      dep->doepctl |= stall ? DOEPCTL_STALL : 0;
    } else {
      // Asserting GONAK is required to STALL an OUT endpoint.
      // Simpler to use polling here, we don't use the "B"OUTNAKEFF interrupt
      // anyway, and it can't be cleared by user code. If this while loop never
      // finishes, we have bigger problems than just the stack.
      dwc2->dctl |= DCTL_SGONAK;
      while ((dwc2->gintsts & GINTSTS_BOUTNAKEFF_Msk) == 0) {}

      // Ditto here disable the endpoint.
      dep->doepctl |= DOEPCTL_EPDIS | (stall ? DOEPCTL_STALL : 0);
      while ((dep->doepint & DOEPINT_EPDISD_Msk) == 0) {}

      dep->doepint = DOEPINT_EPDISD;

      // Allow other OUT endpoints to keep receiving.
      dwc2->dctl |= DCTL_CGONAK;
    }
  }
}

// Start of Bus Reset
static void bus_reset(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint8_t const ep_count = _dwc2_controller[rhport].ep_count;

  tu_memclr(xfer_status, sizeof(xfer_status));

  _sof_en = false;
  _allocated_ep_in_count = 1;

  // 1. NAK for all OUT endpoints
  for (uint8_t n = 0; n < ep_count; n++) {
    dwc2->epout[n].doepctl |= DOEPCTL_SNAK;
  }

  // 2. Disable all IN endpoints
  for (uint8_t n = 0; n < ep_count; n++) {
    if (dwc2->epin[n].diepctl & DIEPCTL_EPENA) {
      dwc2->epin[n].diepctl |= DIEPCTL_SNAK | DIEPCTL_EPDIS;
    }
  }

  dfifo_flush_tx(dwc2, 0x10); // all tx fifo
  dfifo_flush_rx(dwc2);

  // 3. Set up interrupt mask for EP0
  dwc2->daintmsk = TU_BIT(DAINTMSK_OEPM_Pos) | TU_BIT(DAINTMSK_IEPM_Pos);
  dwc2->doepmsk = DOEPMSK_STUPM | DOEPMSK_XFRCM;
  dwc2->diepmsk = DIEPMSK_TOM | DIEPMSK_XFRCM;

  // 4. Set up DFIFO
  dfifo_device_init(rhport);

  // 5. Reset device address
  dwc2->dcfg &= ~DCFG_DAD_Msk;

  // Fixed both control EP0 size to 64 bytes
  dwc2->epin[0].diepctl &= ~(0x03 << DIEPCTL_MPSIZ_Pos);
  dwc2->epout[0].doepctl &= ~(0x03 << DOEPCTL_MPSIZ_Pos);

  xfer_status[0][TUSB_DIR_OUT].max_size = 64;
  xfer_status[0][TUSB_DIR_IN].max_size = 64;

  if(dma_device_enabled(dwc2)) {
    dma_setup_prepare(rhport);
  } else {
    dwc2->epout[0].doeptsiz |= (3 << DOEPTSIZ_STUPCNT_Pos);
  }

  dwc2->gintmsk |= GINTMSK_OEPINT | GINTMSK_IEPINT;
}

static void edpt_schedule_packets(uint8_t rhport, uint8_t const epnum, uint8_t const dir, uint16_t const num_packets,
                                  uint16_t total_bytes) {
  (void) rhport;

  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  xfer_ctl_t* const xfer = XFER_CTL_BASE(epnum, dir);

  // EP0 is limited to one packet each xfer
  // We use multiple transaction of xfer->max_size length to get a whole transfer done
  if (epnum == 0) {
    total_bytes = tu_min16(ep0_pending[dir], xfer->max_size);
    ep0_pending[dir] -= total_bytes;
  }

  // IN and OUT endpoint xfers are interrupt-driven, we just schedule them here.
  const uint8_t is_epout = 1 - dir;
  dwc2_dep_t* dep = &dwc2->ep[is_epout][epnum];

  if (dir == TUSB_DIR_IN) {
    // A full IN transfer (multiple packets, possibly) triggers XFRC.
    dep->dieptsiz = (num_packets << DIEPTSIZ_PKTCNT_Pos) |
                    ((total_bytes << DIEPTSIZ_XFRSIZ_Pos) & DIEPTSIZ_XFRSIZ_Msk);

    if(dma_device_enabled(dwc2)) {
      dep->diepdma = (uintptr_t)xfer->buffer;

      // For ISO endpoint set correct odd/even bit for next frame.
      if ((dep->diepctl & DIEPCTL_EPTYP) == DIEPCTL_EPTYP_0 && (XFER_CTL_BASE(epnum, dir))->interval == 1) {
        // Take odd/even bit from frame counter.
        uint32_t const odd_frame_now = (dwc2->dsts & (1u << DSTS_FNSOF_Pos));
        dep->diepctl |= (odd_frame_now ? DIEPCTL_SD0PID_SEVNFRM_Msk : DIEPCTL_SODDFRM_Msk);
      }

      dep->diepctl |= DIEPCTL_EPENA | DIEPCTL_CNAK;
    } else {
      dep->diepctl |= DIEPCTL_EPENA | DIEPCTL_CNAK;

      // For ISO endpoint set correct odd/even bit for next frame.
      if ((dep->diepctl & DIEPCTL_EPTYP) == DIEPCTL_EPTYP_0 && (XFER_CTL_BASE(epnum, dir))->interval == 1) {
        // Take odd/even bit from frame counter.
        uint32_t const odd_frame_now = (dwc2->dsts & (1u << DSTS_FNSOF_Pos));
        dep->diepctl |= (odd_frame_now ? DIEPCTL_SD0PID_SEVNFRM_Msk : DIEPCTL_SODDFRM_Msk);
      }
      // Enable fifo empty interrupt only if there are something to put in the fifo.
      if (total_bytes != 0) {
        dwc2->diepempmsk |= (1 << epnum);
      }
    }
  } else {
    // A full OUT transfer (multiple packets, possibly) triggers XFRC.
    dep->doeptsiz &= ~(DOEPTSIZ_PKTCNT_Msk | DOEPTSIZ_XFRSIZ);
    dep->doeptsiz |= (num_packets << DOEPTSIZ_PKTCNT_Pos) |
                             ((total_bytes << DOEPTSIZ_XFRSIZ_Pos) & DOEPTSIZ_XFRSIZ_Msk);

    if ((dep->doepctl & DOEPCTL_EPTYP) == DOEPCTL_EPTYP_0 &&
        XFER_CTL_BASE(epnum, dir)->interval == 1) {
      // Take odd/even bit from frame counter.
      uint32_t const odd_frame_now = (dwc2->dsts & (1u << DSTS_FNSOF_Pos));
      dep->doepctl |= (odd_frame_now ? DOEPCTL_SD0PID_SEVNFRM_Msk : DOEPCTL_SODDFRM_Msk);
    }

    if(dma_device_enabled(dwc2)) {
      dep->doepdma = (uintptr_t)xfer->buffer;
    }

    dep->doepctl |= DOEPCTL_EPENA | DOEPCTL_CNAK;
  }
}

//--------------------------------------------------------------------
// Controller API
//--------------------------------------------------------------------
bool dcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  // Core Initialization
  const bool is_highspeed = dwc2_core_is_highspeed(dwc2, TUSB_ROLE_DEVICE);
  TU_ASSERT(dwc2_core_init(rhport, is_highspeed));

  if (dma_device_enabled(dwc2)) {
    // DMA seems to be only settable after a core reset, and not possible to switch on-the-fly
    dwc2->gahbcfg |= GAHBCFG_DMAEN | GAHBCFG_HBSTLEN_2;
  } else {
    dwc2->gintmsk |= GINTSTS_RXFLVL;
  }

  // Device Initialization
  dcd_disconnect(rhport);

  // Set device max speed
  uint32_t dcfg = dwc2->dcfg & ~DCFG_DSPD_Msk;
  if (is_highspeed) {
    dcfg |= DCFG_DSPD_HS << DCFG_DSPD_Pos;

    // XCVRDLY: transceiver delay between xcvr_sel and txvalid during device chirp is required
    // when using with some PHYs such as USB334x (USB3341, USB3343, USB3346, USB3347)
    if (dwc2->ghwcfg2_bm.hs_phy_type == GHWCFG2_HSPHY_ULPI) {
      dcfg |= DCFG_XCVRDLY;
    }
  }else {
    dcfg |= DCFG_DSPD_FS << DCFG_DSPD_Pos;
  }
  dwc2->dcfg = dcfg;

  // Force device mode
  dwc2->gusbcfg = (dwc2->gusbcfg & ~GUSBCFG_FHMOD) | GUSBCFG_FDMOD;

  // Clear A override, force B Valid
  dwc2->gotgctl = (dwc2->gotgctl & ~GOTGCTL_AVALOEN) | GOTGCTL_BVALOEN | GOTGCTL_BVALOVAL;

  // If USB host misbehaves during status portion of control xfer (non zero-length packet), send STALL back and discard
  dwc2->dcfg |= DCFG_NZLSOHSK;

  // Enable required interrupts
  dwc2->gintmsk |= GINTMSK_OTGINT | GINTMSK_USBSUSPM | GINTMSK_USBRST | GINTMSK_ENUMDNEM | GINTMSK_WUIM;

  // TX FIFO empty level for interrupt is complete empty
  uint32_t gahbcfg = dwc2->gahbcfg;
  gahbcfg |= GAHBCFG_TX_FIFO_EPMTY_LVL;
  gahbcfg |= GAHBCFG_GINT; // Enable global interrupt
  dwc2->gahbcfg = gahbcfg;

  dcd_connect(rhport);
  return true;
}

void dcd_int_enable(uint8_t rhport) {
  dwc2_dcd_int_enable(rhport);
}

void dcd_int_disable(uint8_t rhport) {
  dwc2_dcd_int_disable(rhport);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  dwc2->dcfg = (dwc2->dcfg & ~DCFG_DAD_Msk) | (dev_addr << DCFG_DAD_Pos);

  // Response with status after changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void) rhport;

  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  // set remote wakeup
  dwc2->dctl |= DCTL_RWUSIG;

  // enable SOF to detect bus resume
  dwc2->gintsts = GINTSTS_SOF;
  dwc2->gintmsk |= GINTMSK_SOFM;

  // Per specs: remote wakeup signal bit must be clear within 1-15ms
  dwc2_remote_wakeup_delay();

  dwc2->dctl &= ~DCTL_RWUSIG;
}

void dcd_connect(uint8_t rhport) {
  (void) rhport;
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

#ifdef TUP_USBIP_DWC2_ESP32
  usb_wrap_otg_conf_reg_t conf = USB_WRAP.otg_conf;
  conf.pad_pull_override = 0;
  conf.dp_pullup = 0;
  conf.dp_pulldown = 0;
  conf.dm_pullup = 0;
  conf.dm_pulldown = 0;
  USB_WRAP.otg_conf = conf;
#endif

  dwc2->dctl &= ~DCTL_SDIS;
}

void dcd_disconnect(uint8_t rhport) {
  (void) rhport;
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

#ifdef TUP_USBIP_DWC2_ESP32
  usb_wrap_otg_conf_reg_t conf = USB_WRAP.otg_conf;
  conf.pad_pull_override = 1;
  conf.dp_pullup = 0;
  conf.dp_pulldown = 1;
  conf.dm_pullup = 0;
  conf.dm_pulldown = 1;
  USB_WRAP.otg_conf = conf;
#endif

  dwc2->dctl |= DCTL_SDIS;
}

// Be advised: audio, video and possibly other iso-ep classes use dcd_sof_enable() to enable/disable its corresponding ISR on purpose!
void dcd_sof_enable(uint8_t rhport, bool en) {
  (void) rhport;
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  _sof_en = en;

  if (en) {
    dwc2->gintsts = GINTSTS_SOF;
    dwc2->gintmsk |= GINTMSK_SOFM;
  } else {
    dwc2->gintmsk &= ~GINTMSK_SOFM;
  }
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const* desc_edpt) {
  TU_ASSERT(dfifo_alloc(rhport, desc_edpt->bEndpointAddress, tu_edpt_packet_size(desc_edpt)));
  edpt_activate(rhport, desc_edpt);
  return true;
}

// Close all non-control endpoints, cancel all pending transfers if any.
void dcd_edpt_close_all(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint8_t const ep_count = _dwc2_controller[rhport].ep_count;

  _allocated_ep_in_count = 1;

  // Disable non-control interrupt
  dwc2->daintmsk = (1 << DAINTMSK_OEPM_Pos) | (1 << DAINTMSK_IEPM_Pos);

  for (uint8_t n = 1; n < ep_count; n++) {
    for (uint8_t d = 0; d < 2; d++) {
      dwc2_dep_t* dep = &dwc2->ep[d][n];
      if (dep->ctl & EPCTL_EPENA) {
        dep->ctl |= EPCTL_SNAK | EPCTL_EPDIS;
      }
      xfer_status[n][1-d].max_size = 0;
    }
  }

  dfifo_flush_tx(dwc2, 0x10); // all tx fifo
  dfifo_flush_rx(dwc2);

  dfifo_device_init(rhport); // re-init dfifo
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  TU_ASSERT(dfifo_alloc(rhport, ep_addr, largest_packet_size));
  return true;
}

bool dcd_edpt_iso_activate(uint8_t rhport,  tusb_desc_endpoint_t const * p_endpoint_desc) {
  // Disable EP to clear potential incomplete transfers
  edpt_disable(rhport, p_endpoint_desc->bEndpointAddress, false);
  edpt_activate(rhport, p_endpoint_desc);
  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes) {
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  xfer_ctl_t* xfer = XFER_CTL_BASE(epnum, dir);
  xfer->buffer = buffer;
  xfer->ff = NULL;
  xfer->total_len = total_bytes;

  // EP0 can only handle one packet
  if (epnum == 0) {
    ep0_pending[dir] = total_bytes;

    // Schedule the first transaction for EP0 transfer
    edpt_schedule_packets(rhport, epnum, dir, 1, ep0_pending[dir]);
  } else {
    uint16_t num_packets = tu_div_ceil(total_bytes, xfer->max_size);
    if (num_packets == 0) {
      num_packets = 1; // zero length packet still count as 1
    }

    // Schedule packets to be sent within interrupt
    edpt_schedule_packets(rhport, epnum, dir, num_packets, total_bytes);
  }

  return true;
}

// The number of bytes has to be given explicitly to allow more flexible control of how many
// bytes should be written and second to keep the return value free to give back a boolean
// success message. If total_bytes is too big, the FIFO will copy only what is available
// into the USB buffer!
bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t* ff, uint16_t total_bytes) {
  // USB buffers always work in bytes so to avoid unnecessary divisions we demand item_size = 1
  TU_ASSERT(ff->item_size == 1);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  xfer_ctl_t* xfer = XFER_CTL_BASE(epnum, dir);
  xfer->buffer = NULL;
  xfer->ff = ff;
  xfer->total_len = total_bytes;

  uint16_t num_packets = (total_bytes / xfer->max_size);
  uint16_t const short_packet_size = total_bytes % xfer->max_size;

  // Zero-size packet is special case.
  if (short_packet_size > 0 || (total_bytes == 0)) {
    num_packets++;
  }

  // Schedule packets to be sent within interrupt
  edpt_schedule_packets(rhport, epnum, dir, num_packets, total_bytes);

  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  edpt_disable(rhport, ep_addr, false);
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  edpt_disable(rhport, ep_addr, true);
  if((tu_edpt_number(ep_addr) == 0) && dma_device_enabled(dwc2)) {
    dma_setup_prepare(rhport);
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);
  dwc2_dep_t* dep = &dwc2->ep[1 - dir][epnum];

  // Clear stall and reset data toggle
  dep->ctl &= ~EPCTL_STALL;;
  dep->ctl |= EPCTL_SD0PID_SEVNFRM;
}

//--------------------------------------------------------------------
// Interrupt Handler
//--------------------------------------------------------------------

// Process shared receive FIFO, this interrupt is only used in Slave mode
static void handle_rxflvl_irq(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const volatile uint32_t* rx_fifo = dwc2->fifo[0];

  // Pop control word off FIFO
  const dwc2_grxstsp_t grxstsp_bm = dwc2->grxstsp_bm;
  const uint8_t epnum = grxstsp_bm.ep_ch_num;
  const uint16_t byte_count = grxstsp_bm.byte_count;
  dwc2_epout_t* epout = &dwc2->epout[epnum];

  switch (grxstsp_bm.packet_status) {
    // Global OUT NAK: do nothing
    case GRXSTS_PKTSTS_GLOBALOUTNAK:
      break;

    case GRXSTS_PKTSTS_SETUPRX:
      // Setup packet received
      // We can receive up to three setup packets in succession, but  only the last one is valid.
      _setup_packet[0] = (*rx_fifo);
      _setup_packet[1] = (*rx_fifo);
      break;

    case GRXSTS_PKTSTS_SETUPDONE:
      // Setup packet done:
      // After popping this out, dwc2 asserts a DOEPINT_SETUP interrupt which is handled by handle_epout_irq()
      epout->doeptsiz |= (3 << DOEPTSIZ_STUPCNT_Pos);
      break;

    case GRXSTS_PKTSTS_OUTRX: {
      // Out packet received
      xfer_ctl_t* xfer = XFER_CTL_BASE(epnum, TUSB_DIR_OUT);

      // Read packet off RxFIFO
      if (xfer->ff) {
        // Ring buffer
        tu_fifo_write_n_const_addr_full_words(xfer->ff, (const void*) (uintptr_t) rx_fifo, byte_count);
      } else {
        // Linear buffer
        dfifo_read_packet(dwc2, xfer->buffer, byte_count);

        // Increment pointer to xfer data
        xfer->buffer += byte_count;
      }

      // short packet, minus remaining bytes (xfer_size)
      if (byte_count < xfer->max_size) {
        xfer->total_len -= epout->doeptsiz_bm.xfer_size;
        if (epnum == 0) {
          xfer->total_len -= ep0_pending[TUSB_DIR_OUT];
          ep0_pending[TUSB_DIR_OUT] = 0;
        }
      }
      break;
    }

    case GRXSTS_PKTSTS_OUTDONE:
      /* Out packet done
         After this entry is popped from the receive FIFO, dwc2 asserts a Transfer Completed interrupt on
         the specified OUT endpoint which will be handled by handle_epout_irq() */
      break;

    default:
      TU_BREAKPOINT();
      break;
  }
}

static void handle_epout_irq(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint8_t const ep_count = _dwc2_controller[rhport].ep_count;

  // DAINT for a given EP clears when DOEPINTx is cleared.
  // OEPINT will be cleared when DAINT's out bits are cleared.
  for (uint8_t epnum = 0; epnum < ep_count; epnum++) {
    if (dwc2->daint & TU_BIT(DAINT_OEPINT_Pos + epnum)) {
      dwc2_epout_t* epout = &dwc2->epout[epnum];
      const uint32_t doepint = epout->doepint;
      TU_ASSERT((epout->doepint & DOEPINT_AHBERR) == 0, );

      // Setup and/or STPKTRX/STSPHSRX (from 3.00a) can be set along with XFRC, and also set independently.
      if (dwc2->gsnpsid >= DWC2_CORE_REV_3_00a) {
        if (doepint & DOEPINT_STSPHSRX) {
          // Status phase received for control write: In token received from Host
          epout->doepint = DOEPINT_STSPHSRX;
        }

        if (doepint & DOEPINT_STPKTRX) {
          // New setup packet received, but wait for Setup done, since we can receive up to 3 setup consecutively
          epout->doepint = DOEPINT_STPKTRX;
        }
      }

      if (doepint & DOEPINT_SETUP) {
        epout->doepint = DOEPINT_SETUP;

        if(dma_device_enabled(dwc2)) {
          dma_setup_prepare(rhport);
        }

        dcd_event_setup_received(rhport, (uint8_t*) _setup_packet, true);
      }

      // OUT XFER complete
      if (doepint & DOEPINT_XFRC) {
        epout->doepint = DOEPINT_XFRC;

        // only handle data skip if it is setup or status related
        // Normal OUT transfer complete
        if (!(doepint & (DOEPINT_SETUP | DOEPINT_STPKTRX | DOEPINT_STSPHSRX))) {
          xfer_ctl_t* xfer = XFER_CTL_BASE(epnum, TUSB_DIR_OUT);

          if(dma_device_enabled(dwc2)) {
            if ((epnum == 0) && ep0_pending[TUSB_DIR_OUT]) {
              // EP0 can only handle one packet Schedule another packet to be received.
              edpt_schedule_packets(rhport, epnum, TUSB_DIR_OUT, 1, ep0_pending[TUSB_DIR_OUT]);
            } else {
              // Fix packet length
              uint16_t remain = (epout->doeptsiz & DOEPTSIZ_XFRSIZ_Msk) >> DOEPTSIZ_XFRSIZ_Pos;
              xfer->total_len -= remain;
              // this is ZLP, so prepare EP0 for next setup
              if(epnum == 0 && xfer->total_len == 0) {
                dma_setup_prepare(rhport);
              }

              dcd_event_xfer_complete(rhport, epnum, xfer->total_len, XFER_RESULT_SUCCESS, true);
            }
          } else {
            // EP0 can only handle one packet
            if ((epnum == 0) && ep0_pending[TUSB_DIR_OUT]) {
              // Schedule another packet to be received.
              edpt_schedule_packets(rhport, epnum, TUSB_DIR_OUT, 1, ep0_pending[TUSB_DIR_OUT]);
            } else {
              dcd_event_xfer_complete(rhport, epnum, xfer->total_len, XFER_RESULT_SUCCESS, true);
            }
          }
        }
      }
    }
  }
}

static void handle_epin_irq(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const uint8_t ep_count = _dwc2_controller[rhport].ep_count;

  // DAINT for a given EP clears when DIEPINTx is cleared.
  // IEPINT will be cleared when DAINT's out bits are cleared.
  for (uint8_t n = 0; n < ep_count; n++) {
    if (dwc2->daint & TU_BIT(DAINT_IEPINT_Pos + n)) {
      // IN XFER complete (entire xfer).
      xfer_ctl_t* xfer = XFER_CTL_BASE(n, TUSB_DIR_IN);
      dwc2_epin_t* epin = &dwc2->epin[n];

      if (epin->diepint & DIEPINT_XFRC) {
        epin->diepint = DIEPINT_XFRC;

        // EP0 can only handle one packet
        if ((n == 0) && ep0_pending[TUSB_DIR_IN]) {
          // Schedule another packet to be transmitted.
          edpt_schedule_packets(rhport, n, TUSB_DIR_IN, 1, ep0_pending[TUSB_DIR_IN]);
        } else {
          if((n == 0) && dma_device_enabled(dwc2)) {
            dma_setup_prepare(rhport);
          }
          dcd_event_xfer_complete(rhport, n | TUSB_DIR_IN_MASK, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }
      }

      // XFER FIFO empty
      if ((epin->diepint & DIEPINT_TXFE) && (dwc2->diepempmsk & (1 << n))) {
        // diepint's TXFE bit is read-only, software cannot clear it.
        // It will only be cleared by hardware when written bytes is more than
        // - 64 bytes or
        // - Half/Empty of TX FIFO size (configured by GAHBCFG.TXFELVL)
        const uint16_t remain_packets = epin->dieptsiz_bm.packet_count;

        // Process every single packet (only whole packets can be written to fifo)
        for (uint16_t i = 0; i < remain_packets; i++) {
          const uint16_t remain_bytes = (uint16_t) epin->dieptsiz_bm.xfer_size;

          // Packet can not be larger than ep max size
          const uint16_t xact_bytes = tu_min16(remain_bytes, xfer->max_size);

          // It's only possible to write full packets into FIFO. Therefore DTXFSTS register of current
          // EP has to be checked if the buffer can take another WHOLE packet
          if (xact_bytes > ((epin->dtxfsts & DTXFSTS_INEPTFSAV_Msk) << 2)) {
            break;
          }

          // Push packet to Tx-FIFO
          if (xfer->ff) {
            volatile uint32_t* tx_fifo = dwc2->fifo[n];
            tu_fifo_read_n_const_addr_full_words(xfer->ff, (void*) (uintptr_t) tx_fifo, xact_bytes);
          } else {
            dfifo_write_packet(dwc2, n, xfer->buffer, xact_bytes);
            xfer->buffer += xact_bytes;
          }
        }

        // Turn off TXFE if all bytes are written.
        if (epin->dieptsiz_bm.xfer_size == 0) {
          dwc2->diepempmsk &= ~(1 << n);
        }
      }
    }
  }
}

/* Interrupt Hierarchy

                DxEPINTn
                   |
                DAINT.xEPn
                   |
     GINTSTS:    xEPInt

  Note: when OTG_MULTI_PROC_INTRPT = 1, Device Each endpoint interrupt deachint/deachmsk/diepeachmsk/doepeachmsk
  are combined to generate dedicated interrupt line for each endpoint.
 */


void dcd_int_handler(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  uint32_t const int_mask = dwc2->gintmsk;
  uint32_t const int_status = dwc2->gintsts & int_mask;

  if (int_status & GINTSTS_USBRST) {
    // USBRST is start of reset.
    dwc2->gintsts = GINTSTS_USBRST;
    bus_reset(rhport);
  }

  if (int_status & GINTSTS_ENUMDNE) {
    // ENUMDNE is the end of reset where speed of the link is detected
    dwc2->gintsts = GINTSTS_ENUMDNE;

    tusb_speed_t speed;
    switch ((dwc2->dsts & DSTS_ENUMSPD_Msk) >> DSTS_ENUMSPD_Pos) {
      case DSTS_ENUMSPD_HS:
        speed = TUSB_SPEED_HIGH;
        break;

      case DSTS_ENUMSPD_LS:
        speed = TUSB_SPEED_LOW;
        break;

      case DSTS_ENUMSPD_FS_HSPHY:
      case DSTS_ENUMSPD_FS:
      default:
        speed = TUSB_SPEED_FULL;
        break;
    }

    // TODO must update GUSBCFG_TRDT according to link speed

    dcd_event_bus_reset(rhport, speed, true);
  }

  if (int_status & GINTSTS_USBSUSP) {
    dwc2->gintsts = GINTSTS_USBSUSP;
    dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
  }

  if (int_status & GINTSTS_WKUINT) {
    dwc2->gintsts = GINTSTS_WKUINT;
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
  }

  // TODO check GINTSTS_DISCINT for disconnect detection
  // if(int_status & GINTSTS_DISCINT)

  if (int_status & GINTSTS_OTGINT) {
    // OTG INT bit is read-only
    uint32_t const otg_int = dwc2->gotgint;

    if (otg_int & GOTGINT_SEDET) {
      dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
    }

    dwc2->gotgint = otg_int;
  }

  if(int_status & GINTSTS_SOF) {
    dwc2->gintsts = GINTSTS_SOF;
    const uint32_t frame = (dwc2->dsts & DSTS_FNSOF) >> DSTS_FNSOF_Pos;

    // Disable SOF interrupt if SOF was not explicitly enabled since SOF was used for remote wakeup detection
    if (!_sof_en) {
      dwc2->gintmsk &= ~GINTMSK_SOFM;
    }

    dcd_event_sof(rhport, frame, true);
  }

  // RxFIFO non-empty interrupt handling.
  if (int_status & GINTSTS_RXFLVL) {
    // RXFLVL bit is read-only
    dwc2->gintmsk &= ~GINTMSK_RXFLVLM; // disable RXFLVL interrupt while reading

    do {
      handle_rxflvl_irq(rhport); // read all packets
    } while(dwc2->gintsts & GINTSTS_RXFLVL);

    dwc2->gintmsk |= GINTMSK_RXFLVLM;
  }

  // OUT endpoint interrupt handling.
  if (int_status & GINTSTS_OEPINT) {
    // OEPINT is read-only, clear using DOEPINTn
    handle_epout_irq(rhport);
  }

  // IN endpoint interrupt handling.
  if (int_status & GINTSTS_IEPINT) {
    // IEPINT bit read-only, clear using DIEPINTn
    handle_epin_irq(rhport);
  }

  //  // Check for Incomplete isochronous IN transfer
  //  if(int_status & GINTSTS_IISOIXFR) {
  //    printf("      IISOIXFR!\r\n");
  ////    TU_LOG(DWC2_DEBUG, "      IISOIXFR!\r\n");
  //  }
}

#if CFG_TUD_TEST_MODE
void dcd_enter_test_mode(uint8_t rhport, tusb_feature_test_mode_t test_selector) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  // Enable the test mode
  dwc2->dctl = (dwc2->dctl & ~DCTL_TCTL_Msk) | (((uint8_t) test_selector) << DCTL_TCTL_Pos);
}
#endif

#endif
