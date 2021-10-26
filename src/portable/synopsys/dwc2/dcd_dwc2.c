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
#include "device/dcd_attr.h"

#if TUSB_OPT_DEVICE_ENABLED && \
    ( defined(DCD_ATTR_DWC2_STM32) || TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3, OPT_MCU_GD32VF103) )

#include "device/dcd.h"
#include "dwc2_type.h"

#if defined(DCD_ATTR_DWC2_STM32)
  #include "dwc2_stm32.h"
#elif TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
  #include "dwc2_esp32.h"
#elif TU_CHECK_MCU(OPT_MCU_GD32VF103)
  #include "dwc2_gd32.h"
#else
  #error "Unsupported MCUs"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

#define DWC2_REG(_port)       ((dwc2_regs_t*) DWC2_REG_BASE)

enum
{
  DCD_HIGH_SPEED        = 0, // Highspeed mode
  DCD_FULL_SPEED_USE_HS = 1, // Full speed in Highspeed port (probably with internal PHY)
  DCD_FULL_SPEED        = 3, // Full speed with internal PHY
};

// PHYSEL, ULPISEL
// UTMI internal HS PHY
// ULPI external HS PHY

static TU_ATTR_ALIGNED(4) uint32_t _setup_packet[2];

typedef struct {
  uint8_t * buffer;
  tu_fifo_t * ff;
  uint16_t total_len;
  uint16_t max_size;
  uint8_t interval;
} xfer_ctl_t;

xfer_ctl_t xfer_status[DWC2_EP_MAX][2];
#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]

// EP0 transfers are limited to 1 packet - larger sizes has to be split
static uint16_t ep0_pending[2];                   // Index determines direction as tusb_dir_t type

// TX FIFO RAM allocation so far in words - RX FIFO size is readily available from dwc2->grxfsiz
static uint16_t _allocated_fifo_words_tx;         // TX FIFO size in words (IN EPs)
static bool _out_ep_closed;                       // Flag to check if RX FIFO size needs an update (reduce its size)

// Calculate the RX FIFO size according to recommendations from reference manual
static inline uint16_t calc_rx_ff_size(uint16_t ep_size)
{
  return 15 + 2*(ep_size/4) + 2*DWC2_EP_MAX;
}

static void update_grxfsiz(uint8_t rhport)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  // Determine largest EP size for RX FIFO
  uint16_t max_epsize = 0;
  for (uint8_t epnum = 0; epnum < DWC2_EP_MAX; epnum++)
  {
    max_epsize = tu_max16(max_epsize, xfer_status[epnum][TUSB_DIR_OUT].max_size);
  }

  // Update size of RX FIFO
  dwc2->grxfsiz = calc_rx_ff_size(max_epsize);
}

// Setup the control endpoint 0.
static void bus_reset(uint8_t rhport)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  tu_memclr(xfer_status, sizeof(xfer_status));
  _out_ep_closed = false;

  // clear device address
  dwc2->dcfg &= ~DCFG_DAD_Msk;

  // 1. NAK for all OUT endpoints
  for(uint8_t n = 0; n < DWC2_EP_MAX; n++) {
    dwc2->epout[n].doepctl |= DOEPCTL_SNAK;
  }

  // 2. Un-mask interrupt bits
  dwc2->daintmsk = (1 << DAINTMSK_OEPM_Pos) | (1 << DAINTMSK_IEPM_Pos);
  dwc2->doepmsk  = DOEPMSK_STUPM | DOEPMSK_XFRCM;
  dwc2->diepmsk  = DIEPMSK_TOM | DIEPMSK_XFRCM;

  // "USB Data FIFOs" section in reference manual
  // Peripheral FIFO architecture
  //
  // The FIFO is split up in a lower part where the RX FIFO is located and an upper part where the TX FIFOs start.
  // We do this to allow the RX FIFO to grow dynamically which is possible since the free space is located
  // between the RX and TX FIFOs. This is required by ISO OUT EPs which need a bigger FIFO than the standard
  // configuration done below.
  //
  // Dynamically FIFO sizes are of interest only for ISO EPs since all others are usually not opened and closed.
  // All EPs other than ISO are opened as soon as the driver starts up i.e. when the host sends a
  // configure interface command. Hence, all IN EPs other the ISO will be located at the top. IN ISO EPs are usually
  // opened when the host sends an additional command: setInterface. At this point in time
  // the ISO EP will be located next to the free space and can change its size. In case more IN EPs change its size
  // an additional memory
  //
  // --------------- 320 or 1024 ( 1280 or 4096 bytes )
  // | IN FIFO 0   |
  // --------------- (320 or 1024) - 16
  // | IN FIFO 1   |
  // --------------- (320 or 1024) - 16 - x
  // |   . . . .   |
  // --------------- (320 or 1024) - 16 - x - y - ... - z
  // | IN FIFO MAX |
  // ---------------
  // |    FREE     |
  // --------------- GRXFSIZ
  // | OUT FIFO    |
  // | ( Shared )  |
  // --------------- 0
  //
  // According to "FIFO RAM allocation" section in RM, FIFO RAM are allocated as follows (each word 32-bits):
  // - Each EP IN needs at least max packet size, 16 words is sufficient for EP0 IN
  //
  // - All EP OUT shared a unique OUT FIFO which uses
  //   - 13 for setup packets + control words (up to 3 setup packets).
  //   - 1 for global NAK (not required/used here).
  //   - Largest-EPsize / 4 + 1. ( FS: 64 bytes, HS: 512 bytes). Recommended is  "2 x (Largest-EPsize/4) + 1"
  //   - 2 for each used OUT endpoint
  //
  //   Therefore GRXFSIZ = 13 + 1 + 1 + 2 x (Largest-EPsize/4) + 2 x EPOUTnum
  //   - FullSpeed (64 Bytes ): GRXFSIZ = 15 + 2 x  16 + 2 x DWC2_EP_MAX = 47  + 2 x DWC2_EP_MAX
  //   - Highspeed (512 bytes): GRXFSIZ = 15 + 2 x 128 + 2 x DWC2_EP_MAX = 271 + 2 x DWC2_EP_MAX
  //
  //   NOTE: Largest-EPsize & EPOUTnum is actual used endpoints in configuration. Since DCD has no knowledge
  //   of the overall picture yet. We will use the worst scenario: largest possible + DWC2_EP_MAX
  //
  //   For Isochronous, largest EP size can be 1023/1024 for FS/HS respectively. In addition if multiple ISO
  //   are enabled at least "2 x (Largest-EPsize/4) + 1" are recommended.  Maybe provide a macro for application to
  //   overwrite this.

  dwc2->grxfsiz = calc_rx_ff_size(TUD_OPT_HIGH_SPEED ? 512 : 64);

  _allocated_fifo_words_tx = 16;

  // Control IN uses FIFO 0 with 64 bytes ( 16 32-bit word )
  dwc2->dieptxf0 = (16 << TX0FD_Pos) | (DWC2_EP_FIFO_SIZE/4 - _allocated_fifo_words_tx);

  // Fixed control EP0 size to 64 bytes
  dwc2->epin[0].diepctl &= ~(0x03 << DIEPCTL_MPSIZ_Pos);
  xfer_status[0][TUSB_DIR_OUT].max_size = xfer_status[0][TUSB_DIR_IN].max_size = 64;

  dwc2->epout[0].doeptsiz |= (3 << DOEPTSIZ_STUPCNT_Pos);

  dwc2->gintmsk |= GINTMSK_OEPINT | GINTMSK_IEPINT;
}


static tusb_speed_t get_speed(uint8_t rhport)
{
  (void) rhport;
  dwc2_regs_t * dwc2 = DWC2_REG(rhport);
  uint32_t const enum_spd = (dwc2->dsts & DSTS_ENUMSPD_Msk) >> DSTS_ENUMSPD_Pos;
  return (enum_spd == DCD_HIGH_SPEED) ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL;
}

static void set_speed(uint8_t rhport, tusb_speed_t speed)
{
  uint32_t bitvalue;

  if ( rhport == 1 )
  {
    bitvalue = ((TUSB_SPEED_HIGH == speed) ? DCD_HIGH_SPEED : DCD_FULL_SPEED_USE_HS);
  }
  else
  {
    bitvalue = DCD_FULL_SPEED;
  }

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  // Clear and set speed bits
  dwc2->dcfg &= ~(3 << DCFG_DSPD_Pos);
  dwc2->dcfg |= (bitvalue << DCFG_DSPD_Pos);
}

#if defined(USB_HS_PHYC)
static bool USB_HS_PHYCInit(void)
{
  USB_HS_PHYC_GlobalTypeDef *usb_hs_phyc = (USB_HS_PHYC_GlobalTypeDef*) USB_HS_PHYC_CONTROLLER_BASE;

  // Enable LDO
  usb_hs_phyc->USB_HS_PHYC_LDO |= USB_HS_PHYC_LDO_ENABLE;

  // Wait until LDO ready
  while ( 0 == (usb_hs_phyc->USB_HS_PHYC_LDO & USB_HS_PHYC_LDO_STATUS) ) {}

  uint32_t phyc_pll = 0;

  // TODO Try to get HSE_VALUE from registers instead of depending CFLAGS
  switch ( HSE_VALUE )
  {
    case 12000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_12MHZ   ; break;
    case 12500000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_12_5MHZ ; break;
    case 16000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_16MHZ   ; break;
    case 24000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_24MHZ   ; break;
    case 25000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_25MHZ   ; break;
    case 32000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_Msk     ; break; // Value not defined in header
    default:
      TU_ASSERT(0);
  }
  usb_hs_phyc->USB_HS_PHYC_PLL = phyc_pll;

  // Control the tuning interface of the High Speed PHY
  // Use magic value (USB_HS_PHYC_TUNE_VALUE) from ST driver
  usb_hs_phyc->USB_HS_PHYC_TUNE |= 0x00000F13U;

  // Enable PLL internal PHY
  usb_hs_phyc->USB_HS_PHYC_PLL |= USB_HS_PHYC_PLL_PLLEN;

  // Original ST code has 2 ms delay for PLL stabilization.
  // Primitive test shows that more than 10 USB un/replug cycle showed no error with enumeration

  return true;
}
#endif

static void edpt_schedule_packets(uint8_t rhport, uint8_t const epnum, uint8_t const dir, uint16_t const num_packets, uint16_t total_bytes)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  // EP0 is limited to one packet each xfer
  // We use multiple transaction of xfer->max_size length to get a whole transfer done
  if ( epnum == 0 )
  {
    xfer_ctl_t *const xfer = XFER_CTL_BASE(epnum, dir);
    total_bytes = tu_min16(ep0_pending[dir], xfer->max_size);
    ep0_pending[dir] -= total_bytes;
  }

  // IN and OUT endpoint xfers are interrupt-driven, we just schedule them here.
  if ( dir == TUSB_DIR_IN )
  {
    dwc2_epin_t* epin = dwc2->epin;

    // A full IN transfer (multiple packets, possibly) triggers XFRC.
    epin[epnum].dieptsiz = (num_packets << DIEPTSIZ_PKTCNT_Pos) |
                            ((total_bytes << DIEPTSIZ_XFRSIZ_Pos) & DIEPTSIZ_XFRSIZ_Msk);

    epin[epnum].diepctl |= DIEPCTL_EPENA | DIEPCTL_CNAK;

    // For ISO endpoint set correct odd/even bit for next frame.
    if ( (epin[epnum].diepctl & DIEPCTL_EPTYP) == DIEPCTL_EPTYP_0 && (XFER_CTL_BASE(epnum, dir))->interval == 1 )
    {
      // Take odd/even bit from frame counter.
      uint32_t const odd_frame_now = (dwc2->dsts & (1u << DSTS_FNSOF_Pos));
      epin[epnum].diepctl |= (odd_frame_now ? DIEPCTL_SD0PID_SEVNFRM_Msk : DIEPCTL_SODDFRM_Msk);
    }
    // Enable fifo empty interrupt only if there are something to put in the fifo.
    if ( total_bytes != 0 )
    {
      dwc2->diepempmsk |= (1 << epnum);
    }
  }
  else
  {
    dwc2_epout_t* epout = dwc2->epout;

    // A full OUT transfer (multiple packets, possibly) triggers XFRC.
    epout[epnum].doeptsiz &= ~(DOEPTSIZ_PKTCNT_Msk | DOEPTSIZ_XFRSIZ);
    epout[epnum].doeptsiz |= (num_packets << DOEPTSIZ_PKTCNT_Pos) |
                                   ((total_bytes << DOEPTSIZ_XFRSIZ_Pos) & DOEPTSIZ_XFRSIZ_Msk);

    epout[epnum].doepctl |= DOEPCTL_EPENA | DOEPCTL_CNAK;
    if ( (epout[epnum].doepctl & DOEPCTL_EPTYP) == DOEPCTL_EPTYP_0 && (XFER_CTL_BASE(epnum, dir))->interval == 1 )
    {
      // Take odd/even bit from frame counter.
      uint32_t const odd_frame_now = (dwc2->dsts & (1u << DSTS_FNSOF_Pos));
      epout[epnum].doepctl |= (odd_frame_now ? DOEPCTL_SD0PID_SEVNFRM_Msk : DOEPCTL_SODDFRM_Msk);
    }
  }
}

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
void dcd_init (uint8_t rhport)
{
  // Programming model begins in the last section of the chapter on the USB
  // peripheral in each Reference Manual.
  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  // check GSNPSID

  // No HNP/SRP (no OTG support), program timeout later.
  if ( rhport == 1 )
  {
    // On selected MCUs HS port1 can be used with external PHY via ULPI interface
#if CFG_TUSB_RHPORT1_MODE & OPT_MODE_HIGH_SPEED
    // deactivate internal PHY
    dwc2->stm32_gccfg &= ~GCCFG_PWRDWN;

    // Init The UTMI Interface
    dwc2->gusbcfg &= ~(GUSBCFG_TSDPS | GUSBCFG_ULPIFSLS | GUSBCFG_PHYSEL);

    // Select default internal VBUS Indicator and Drive for ULPI
    dwc2->gusbcfg &= ~(GUSBCFG_ULPIEVBUSD | GUSBCFG_ULPIEVBUSI);
#else
    dwc2->gusbcfg |= GUSBCFG_PHYSEL;
#endif

#if defined(USB_HS_PHYC)
    // Highspeed with embedded UTMI PHYC

    // Select UTMI Interface
    dwc2->gusbcfg &= ~GUSBCFG_ULPI_UTMI_SEL;
    dwc2->stm32_gccfg |= GCCFG_PHYHSEN;

    // Enables control of a High Speed USB PHY
    USB_HS_PHYCInit();
#endif
  } else
  {
    // Enable internal PHY
    dwc2->gusbcfg |= GUSBCFG_PHYSEL;
  }

  // Reset core after selecting PHYst
  // Wait AHB IDLE, reset then wait until it is cleared
  while ((dwc2->grstctl & GRSTCTL_AHBIDL) == 0U) {}
  dwc2->grstctl |= GRSTCTL_CSRST;
  while ((dwc2->grstctl & GRSTCTL_CSRST) == GRSTCTL_CSRST) {}

  // Restart PHY clock
  dwc2->pcgctrl = 0;

  // Clear all interrupts
  dwc2->gintsts |= dwc2->gintsts;

  // Required as part of core initialization.
  // TODO: How should mode mismatch be handled? It will cause
  // the core to stop working/require reset.
  dwc2->gintmsk |= GINTMSK_OTGINT | GINTMSK_MMISM;

  // If USB host misbehaves during status portion of control xfer
  // (non zero-length packet), send STALL back and discard.
  dwc2->dcfg |= DCFG_NZLSOHSK;

  set_speed(rhport, TUD_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL);

  // Enable internal USB transceiver, unless using HS core (port 1) with external PHY.
  if (!(rhport == 1 && (CFG_TUSB_RHPORT1_MODE & OPT_MODE_HIGH_SPEED))) dwc2->stm32_gccfg |= GCCFG_PWRDWN;

  dwc2->gintmsk |= GINTMSK_USBRST | GINTMSK_ENUMDNEM | GINTMSK_USBSUSPM |
                   GINTMSK_WUIM   | GINTMSK_RXFLVLM;

  // Enable global interrupt
  dwc2->gahbcfg |= GAHBCFG_GINT;

  dcd_connect(rhport);
}

void dcd_int_enable (uint8_t rhport)
{
  dwc2_dcd_int_enable(rhport);
}

void dcd_int_disable (uint8_t rhport)
{
  dwc2_dcd_int_disable(rhport);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  dwc2_regs_t * dwc2 = DWC2_REG(rhport);
  dwc2->dcfg = (dwc2->dcfg & ~DCFG_DAD_Msk) | (dev_addr << DCFG_DAD_Pos);

  // Response with status after changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  // set remote wakeup
  dwc2->dctl |= DCTL_RWUSIG;

  // enable SOF to detect bus resume
  dwc2->gintsts = GINTSTS_SOF;
  dwc2->gintmsk |= GINTMSK_SOFM;

  // Per specs: remote wakeup signal bit must be clear within 1-15ms
  dwc2_remote_wakeup_delay();

  dwc2->dctl &= ~DCTL_RWUSIG;
}

void dcd_connect(uint8_t rhport)
{
  (void) rhport;
  dwc2_regs_t * dwc2 = DWC2_REG(rhport);
  dwc2->dctl &= ~DCTL_SDIS;
}

void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;
  dwc2_regs_t * dwc2 = DWC2_REG(rhport);
  dwc2->dctl |= DCTL_SDIS;
}


/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);

  TU_ASSERT(epnum < DWC2_EP_MAX);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->max_size = tu_edpt_packet_size(desc_edpt);
  xfer->interval = desc_edpt->bInterval;

  uint16_t const fifo_size = (xfer->max_size + 3) / 4; // Round up to next full word

  if(dir == TUSB_DIR_OUT)
  {
    // Calculate required size of RX FIFO
    uint16_t const sz = calc_rx_ff_size(4*fifo_size);

    // If size_rx needs to be extended check if possible and if so enlarge it
    if (dwc2->grxfsiz < sz)
    {
      TU_ASSERT(sz + _allocated_fifo_words_tx <= DWC2_EP_FIFO_SIZE/4);

      // Enlarge RX FIFO
      dwc2->grxfsiz = sz;
    }

    dwc2->epout[epnum].doepctl |= (1 << DOEPCTL_USBAEP_Pos) |
                                  (desc_edpt->bmAttributes.xfer << DOEPCTL_EPTYP_Pos) |
                                  (desc_edpt->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS ? DOEPCTL_SD0PID_SEVNFRM : 0) |
                                  (xfer->max_size << DOEPCTL_MPSIZ_Pos);

    dwc2->daintmsk |= (1 << (DAINTMSK_OEPM_Pos + epnum));
  }
  else
  {
    // "USB Data FIFOs" section in reference manual
    // Peripheral FIFO architecture
    //
    // --------------- 320 or 1024 ( 1280 or 4096 bytes )
    // | IN FIFO 0   |
    // --------------- (320 or 1024) - 16
    // | IN FIFO 1   |
    // --------------- (320 or 1024) - 16 - x
    // |   . . . .   |
    // --------------- (320 or 1024) - 16 - x - y - ... - z
    // | IN FIFO MAX |
    // ---------------
    // |    FREE     |
    // --------------- GRXFSIZ
    // | OUT FIFO    |
    // | ( Shared )  |
    // --------------- 0
    //
    // In FIFO is allocated by following rules:
    // - IN EP 1 gets FIFO 1, IN EP "n" gets FIFO "n".

    // Check if free space is available
    TU_ASSERT(_allocated_fifo_words_tx + fifo_size + dwc2->grxfsiz <= DWC2_EP_FIFO_SIZE/4);

    _allocated_fifo_words_tx += fifo_size;

    TU_LOG(2, "    Allocated %u bytes at offset %u", fifo_size*4, DWC2_EP_FIFO_SIZE-_allocated_fifo_words_tx*4);

    // DIEPTXF starts at FIFO #1.
    // Both TXFD and TXSA are in unit of 32-bit words.
    dwc2->dieptxf[epnum - 1] = (fifo_size << DIEPTXF_INEPTXFD_Pos) | (DWC2_EP_FIFO_SIZE/4 - _allocated_fifo_words_tx);

    dwc2->epin[epnum].diepctl |= (1 << DIEPCTL_USBAEP_Pos) |
                                 (epnum << DIEPCTL_TXFNUM_Pos) |
                                 (desc_edpt->bmAttributes.xfer << DIEPCTL_EPTYP_Pos) |
                                 (desc_edpt->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS ? DIEPCTL_SD0PID_SEVNFRM : 0) |
                                 (xfer->max_size << DIEPCTL_MPSIZ_Pos);

    dwc2->daintmsk |= (1 << (DAINTMSK_IEPM_Pos + epnum));
  }

  return true;
}

// Close all non-control endpoints, cancel all pending transfers if any.
void dcd_edpt_close_all (uint8_t rhport)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  // Disable non-control interrupt
  dwc2->daintmsk = (1 << DAINTMSK_OEPM_Pos) | (1 << DAINTMSK_IEPM_Pos);

  for(uint8_t n = 1; n < DWC2_EP_MAX; n++)
  {
    // disable OUT endpoint
    dwc2->epout[n].doepctl = 0;
    xfer_status[n][TUSB_DIR_OUT].max_size = 0;

    // disable IN endpoint
    dwc2->epin[n].diepctl = 0;
    xfer_status[n][TUSB_DIR_IN].max_size = 0;
  }

  // reset allocated fifo IN
  _allocated_fifo_words_tx = 16;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->buffer      = buffer;
  xfer->ff          = NULL;
  xfer->total_len   = total_bytes;

  // EP0 can only handle one packet
  if(epnum == 0)
  {
    ep0_pending[dir] = total_bytes;
    // Schedule the first transaction for EP0 transfer
    edpt_schedule_packets(rhport, epnum, dir, 1, ep0_pending[dir]);
    return true;
  }

  uint16_t num_packets = (total_bytes / xfer->max_size);
  uint16_t const short_packet_size = total_bytes % xfer->max_size;

  // Zero-size packet is special case.
  if ( short_packet_size > 0 || (total_bytes == 0) ) num_packets++;

  // Schedule packets to be sent within interrupt
  edpt_schedule_packets(rhport, epnum, dir, num_packets, total_bytes);

  return true;
}

// The number of bytes has to be given explicitly to allow more flexible control of how many
// bytes should be written and second to keep the return value free to give back a boolean
// success message. If total_bytes is too big, the FIFO will copy only what is available
// into the USB buffer!
bool dcd_edpt_xfer_fifo (uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes)
{
  // USB buffers always work in bytes so to avoid unnecessary divisions we demand item_size = 1
  TU_ASSERT(ff->item_size == 1);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->buffer      = NULL;
  xfer->ff          = ff;
  xfer->total_len   = total_bytes;

  uint16_t num_packets = (total_bytes / xfer->max_size);
  uint16_t const short_packet_size = total_bytes % xfer->max_size;

  // Zero-size packet is special case.
  if ( short_packet_size > 0 || (total_bytes == 0) ) num_packets++;

  // Schedule packets to be sent within interrupt
  edpt_schedule_packets(rhport, epnum, dir, num_packets, total_bytes);

  return true;
}

static void dcd_edpt_disable (uint8_t rhport, uint8_t ep_addr, bool stall)
{
  (void) rhport;

  dwc2_regs_t *dwc2 = DWC2_REG(rhport);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if ( dir == TUSB_DIR_IN )
  {
    dwc2_epin_t* epin = dwc2->epin;

    // Only disable currently enabled non-control endpoint
    if ( (epnum == 0) || !(epin[epnum].diepctl & DIEPCTL_EPENA) )
    {
      epin[epnum].diepctl |= DIEPCTL_SNAK | (stall ? DIEPCTL_STALL : 0);
    }
    else
    {
      // Stop transmitting packets and NAK IN xfers.
      epin[epnum].diepctl |= DIEPCTL_SNAK;
      while ( (epin[epnum].diepint & DIEPINT_INEPNE) == 0 ) {}

      // Disable the endpoint.
      epin[epnum].diepctl |= DIEPCTL_EPDIS | (stall ? DIEPCTL_STALL : 0);
      while ( (epin[epnum].diepint & DIEPINT_EPDISD_Msk) == 0 ) {}

      epin[epnum].diepint = DIEPINT_EPDISD;
    }

    // Flush the FIFO, and wait until we have confirmed it cleared.
    dwc2->grstctl |= (epnum << GRSTCTL_TXFNUM_Pos);
    dwc2->grstctl |= GRSTCTL_TXFFLSH;
    while ( (dwc2->grstctl & GRSTCTL_TXFFLSH_Msk) != 0 ) {}
  }
  else
  {
    dwc2_epout_t* epout = dwc2->epout;

    // Only disable currently enabled non-control endpoint
    if ( (epnum == 0) || !(epout[epnum].doepctl & DOEPCTL_EPENA) )
    {
      epout[epnum].doepctl |= stall ? DOEPCTL_STALL : 0;
    }
    else
    {
      // Asserting GONAK is required to STALL an OUT endpoint.
      // Simpler to use polling here, we don't use the "B"OUTNAKEFF interrupt
      // anyway, and it can't be cleared by user code. If this while loop never
      // finishes, we have bigger problems than just the stack.
      dwc2->dctl |= DCTL_SGONAK;
      while ( (dwc2->gintsts & GINTSTS_BOUTNAKEFF_Msk) == 0 ) {}

      // Ditto here- disable the endpoint.
      epout[epnum].doepctl |= DOEPCTL_EPDIS | (stall ? DOEPCTL_STALL : 0);
      while ( (epout[epnum].doepint & DOEPINT_EPDISD_Msk) == 0 ) {}

      epout[epnum].doepint = DOEPINT_EPDISD;

      // Allow other OUT endpoints to keep receiving.
      dwc2->dctl |= DCTL_CGONAK;
    }
  }
}

/**
 * Close an endpoint.
 */
void dcd_edpt_close (uint8_t rhport, uint8_t ep_addr)
{
  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  dcd_edpt_disable(rhport, ep_addr, false);

  // Update max_size
  xfer_status[epnum][dir].max_size = 0;  // max_size = 0 marks a disabled EP - required for changing FIFO allocation

  if (dir == TUSB_DIR_IN)
  {
    uint16_t const fifo_size = (dwc2->dieptxf[epnum - 1] & DIEPTXF_INEPTXFD_Msk) >> DIEPTXF_INEPTXFD_Pos;
    uint16_t const fifo_start = (dwc2->dieptxf[epnum - 1] & DIEPTXF_INEPTXSA_Msk) >> DIEPTXF_INEPTXSA_Pos;
    // For now only the last opened endpoint can be closed without fuss.
    TU_ASSERT(fifo_start == DWC2_EP_FIFO_SIZE/4 - _allocated_fifo_words_tx,);
    _allocated_fifo_words_tx -= fifo_size;
  }
  else
  {
    _out_ep_closed = true;     // Set flag such that RX FIFO gets reduced in size once RX FIFO is empty
  }
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  dcd_edpt_disable(rhport, ep_addr, true);
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  // Clear stall and reset data toggle
  if ( dir == TUSB_DIR_IN )
  {
    dwc2->epin[epnum].diepctl &= ~DIEPCTL_STALL;
    dwc2->epin[epnum].diepctl |= DIEPCTL_SD0PID_SEVNFRM;
  }
  else
  {
    dwc2->epout[epnum].doepctl &= ~DOEPCTL_STALL;
    dwc2->epout[epnum].doepctl |= DOEPCTL_SD0PID_SEVNFRM;
  }
}

/*------------------------------------------------------------------*/

// Read a single data packet from receive FIFO
static void read_fifo_packet(uint8_t rhport, uint8_t * dst, uint16_t len)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);
  volatile uint32_t * rx_fifo = dwc2->fifo[0];

  // Reading full available 32 bit words from fifo
  uint16_t full_words = len >> 2;
  for ( uint16_t i = 0; i < full_words; i++ )
  {
    uint32_t tmp = *rx_fifo;
    dst[0] = tmp & 0x000000FF;
    dst[1] = (tmp & 0x0000FF00) >> 8;
    dst[2] = (tmp & 0x00FF0000) >> 16;
    dst[3] = (tmp & 0xFF000000) >> 24;
    dst += 4;
  }

  // Read the remaining 1-3 bytes from fifo
  uint8_t bytes_rem = len & 0x03;
  if ( bytes_rem != 0 )
  {
    uint32_t tmp = *rx_fifo;
    dst[0] = tmp & 0x000000FF;
    if ( bytes_rem > 1 )
    {
      dst[1] = (tmp & 0x0000FF00) >> 8;
    }
    if ( bytes_rem > 2 )
    {
      dst[2] = (tmp & 0x00FF0000) >> 16;
    }
  }
}

// Write a single data packet to EPIN FIFO
static void write_fifo_packet(uint8_t rhport, uint8_t fifo_num, uint8_t * src, uint16_t len)
{
  (void) rhport;

  dwc2_regs_t * dwc2 = DWC2_REG(rhport);
  volatile uint32_t * tx_fifo = dwc2->fifo[fifo_num];

  // Pushing full available 32 bit words to fifo
  uint16_t full_words = len >> 2;
  for ( uint16_t i = 0; i < full_words; i++ )
  {
    *tx_fifo = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
    src += 4;
  }

  // Write the remaining 1-3 bytes into fifo
  uint8_t bytes_rem = len & 0x03;
  if ( bytes_rem )
  {
    uint32_t tmp_word = 0;
    tmp_word |= src[0];
    if ( bytes_rem > 1 )
    {
      tmp_word |= src[1] << 8;
    }
    if ( bytes_rem > 2 )
    {
      tmp_word |= src[2] << 16;
    }
    *tx_fifo = tmp_word;
  }
}

static void handle_rxflvl_ints(uint8_t rhport)
{
  dwc2_regs_t * dwc2 = DWC2_REG(rhport);
  volatile uint32_t * rx_fifo = dwc2->fifo[0];

  // Pop control word off FIFO
  uint32_t ctl_word = dwc2->grxstsp;
  uint8_t pktsts    = (ctl_word & GRXSTSP_PKTSTS_Msk) >> GRXSTSP_PKTSTS_Pos;
  uint8_t epnum     = (ctl_word & GRXSTSP_EPNUM_Msk) >>  GRXSTSP_EPNUM_Pos;
  uint16_t bcnt     = (ctl_word & GRXSTSP_BCNT_Msk) >> GRXSTSP_BCNT_Pos;

  switch ( pktsts )
  {
    case 0x01:    // Global OUT NAK (Interrupt)
    break;

    case 0x02:    // Out packet received
    {
      xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, TUSB_DIR_OUT);

      // Read packet off RxFIFO
      if ( xfer->ff )
      {
        // Ring buffer
        tu_fifo_write_n_const_addr_full_words(xfer->ff, (const void*) (uintptr_t) rx_fifo, bcnt);
      }
      else
      {
        // Linear buffer
        read_fifo_packet(rhport, xfer->buffer, bcnt);

        // Increment pointer to xfer data
        xfer->buffer += bcnt;
      }

      // Truncate transfer length in case of short packet
      if ( bcnt < xfer->max_size )
      {
        xfer->total_len -= (dwc2->epout[epnum].doeptsiz & DOEPTSIZ_XFRSIZ_Msk) >> DOEPTSIZ_XFRSIZ_Pos;
        if ( epnum == 0 )
        {
          xfer->total_len -= ep0_pending[TUSB_DIR_OUT];
          ep0_pending[TUSB_DIR_OUT] = 0;
        }
      }
    }
    break;

    case 0x03:    // Out packet done (Interrupt)
    break;

    case 0x04:    // Setup packet done (Interrupt)
      dwc2->epout[epnum].doeptsiz |= (3 << DOEPTSIZ_STUPCNT_Pos);
    break;

    case 0x06:    // Setup packet recvd
      // We can receive up to three setup packets in succession, but
      // only the last one is valid.
      _setup_packet[0] = (*rx_fifo);
      _setup_packet[1] = (*rx_fifo);
    break;

    default:    // Invalid
      TU_BREAKPOINT();
    break;
  }
}

static void handle_epout_ints (uint8_t rhport, dwc2_regs_t *dwc2)
{
  dwc2_epout_t* epout = dwc2->epout;

  // DAINT for a given EP clears when DOEPINTx is cleared.
  // OEPINT will be cleared when DAINT's out bits are cleared.
  for ( uint8_t n = 0; n < DWC2_EP_MAX; n++ )
  {
    xfer_ctl_t *xfer = XFER_CTL_BASE(n, TUSB_DIR_OUT);

    if ( dwc2->daint & (1 << (DAINT_OEPINT_Pos + n)) )
    {
      // SETUP packet Setup Phase done.
      if ( epout[n].doepint & DOEPINT_STUP )
      {
        epout[n].doepint = DOEPINT_STUP;
        dcd_event_setup_received(rhport, (uint8_t*) &_setup_packet[0], true);
      }

      // OUT XFER complete
      if ( epout[n].doepint & DOEPINT_XFRC )
      {
        epout[n].doepint = DOEPINT_XFRC;

        // EP0 can only handle one packet
        if ( (n == 0) && ep0_pending[TUSB_DIR_OUT] )
        {
          // Schedule another packet to be received.
          edpt_schedule_packets(rhport, n, TUSB_DIR_OUT, 1, ep0_pending[TUSB_DIR_OUT]);
        }
        else
        {
          dcd_event_xfer_complete(rhport, n, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }
      }
    }
  }
}

static void handle_epin_ints (uint8_t rhport, dwc2_regs_t *dwc2)
{
  dwc2_epin_t* epin = dwc2->epin;

  // DAINT for a given EP clears when DIEPINTx is cleared.
  // IEPINT will be cleared when DAINT's out bits are cleared.
  for ( uint8_t n = 0; n < DWC2_EP_MAX; n++ )
  {
    xfer_ctl_t *xfer = XFER_CTL_BASE(n, TUSB_DIR_IN);

    if ( dwc2->daint & (1 << (DAINT_IEPINT_Pos + n)) )
    {
      // IN XFER complete (entire xfer).
      if ( epin[n].diepint & DIEPINT_XFRC )
      {
        epin[n].diepint = DIEPINT_XFRC;

        // EP0 can only handle one packet
        if ( (n == 0) && ep0_pending[TUSB_DIR_IN] )
        {
          // Schedule another packet to be transmitted.
          edpt_schedule_packets(rhport, n, TUSB_DIR_IN, 1, ep0_pending[TUSB_DIR_IN]);
        }
        else
        {
          dcd_event_xfer_complete(rhport, n | TUSB_DIR_IN_MASK, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }
      }

      // XFER FIFO empty
      if ( (epin[n].diepint & DIEPINT_TXFE) && (dwc2->diepempmsk & (1 << n)) )
      {
        // diepint's TXFE bit is read-only, software cannot clear it.
        // It will only be cleared by hardware when written bytes is more than
        // - 64 bytes or
        // - Half of TX FIFO size (configured by DIEPTXF)

        uint16_t remaining_packets = (epin[n].dieptsiz & DIEPTSIZ_PKTCNT_Msk) >> DIEPTSIZ_PKTCNT_Pos;

        // Process every single packet (only whole packets can be written to fifo)
        for ( uint16_t i = 0; i < remaining_packets; i++ )
        {
          uint16_t const remaining_bytes = (epin[n].dieptsiz & DIEPTSIZ_XFRSIZ_Msk) >> DIEPTSIZ_XFRSIZ_Pos;

          // Packet can not be larger than ep max size
          uint16_t const packet_size = tu_min16(remaining_bytes, xfer->max_size);

          // It's only possible to write full packets into FIFO. Therefore DTXFSTS register of current
          // EP has to be checked if the buffer can take another WHOLE packet
          if ( packet_size > ((epin[n].dtxfsts & DTXFSTS_INEPTFSAV_Msk) << 2) ) break;

          // Push packet to Tx-FIFO
          if ( xfer->ff )
          {
            volatile uint32_t *tx_fifo = dwc2->fifo[n];
            tu_fifo_read_n_const_addr_full_words(xfer->ff, (void*) (uintptr_t) tx_fifo, packet_size);
          }
          else
          {
            write_fifo_packet(rhport, n, xfer->buffer, packet_size);

            // Increment pointer to xfer data
            xfer->buffer += packet_size;
          }
        }

        // Turn off TXFE if all bytes are written.
        if ( ((epin[n].dieptsiz & DIEPTSIZ_XFRSIZ_Msk) >> DIEPTSIZ_XFRSIZ_Pos) == 0 )
        {
          dwc2->diepempmsk &= ~(1 << n);
        }
      }
    }
  }
}

void dcd_int_handler(uint8_t rhport)
{
  dwc2_regs_t *dwc2 = DWC2_REG(rhport);

  uint32_t const int_status = dwc2->gintsts & dwc2->gintmsk;

  if(int_status & GINTSTS_USBRST)
  {
    // USBRST is start of reset.
    dwc2->gintsts = GINTSTS_USBRST;
    bus_reset(rhport);
  }

  if(int_status & GINTSTS_ENUMDNE)
  {
    // ENUMDNE is the end of reset where speed of the link is detected

    dwc2->gintsts = GINTSTS_ENUMDNE;

    tusb_speed_t const speed = get_speed(rhport);

    dwc2_set_turnaround(dwc2, speed);
    dcd_event_bus_reset(rhport, speed, true);
  }

  if(int_status & GINTSTS_USBSUSP)
  {
    dwc2->gintsts = GINTSTS_USBSUSP;
    dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
  }

  if(int_status & GINTSTS_WKUINT)
  {
    dwc2->gintsts = GINTSTS_WKUINT;
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
  }

  // TODO check GINTSTS_DISCINT for disconnect detection
  // if(int_status & GINTSTS_DISCINT)

  if(int_status & GINTSTS_OTGINT)
  {
    // OTG INT bit is read-only
    uint32_t const otg_int = dwc2->gotgint;

    if (otg_int & GOTGINT_SEDET)
    {
      dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
    }

    dwc2->gotgint = otg_int;
  }

  if(int_status & GINTSTS_SOF)
  {
    dwc2->gotgint = GINTSTS_SOF;

    // Disable SOF interrupt since currently only used for remote wakeup detection
    dwc2->gintmsk &= ~GINTMSK_SOFM;

    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
  }

  // RxFIFO non-empty interrupt handling.
  if(int_status & GINTSTS_RXFLVL)
  {
    // RXFLVL bit is read-only

    // Mask out RXFLVL while reading data from FIFO
    dwc2->gintmsk &= ~GINTMSK_RXFLVLM;

    // Loop until all available packets were handled
    do
    {
      handle_rxflvl_ints(rhport);
    } while(dwc2->gotgint & GINTSTS_RXFLVL);

    // Manage RX FIFO size
    if (_out_ep_closed)
    {
      update_grxfsiz(rhport);

      // Disable flag
      _out_ep_closed = false;
    }

    dwc2->gintmsk |= GINTMSK_RXFLVLM;
  }

  // OUT endpoint interrupt handling.
  if(int_status & GINTSTS_OEPINT)
  {
    // OEPINT is read-only
    handle_epout_ints(rhport, dwc2);
  }

  // IN endpoint interrupt handling.
  if(int_status & GINTSTS_IEPINT)
  {
    // IEPINT bit read-only
    handle_epin_ints(rhport, dwc2);
  }

  //  // Check for Incomplete isochronous IN transfer
  //  if(int_status & GINTSTS_IISOIXFR) {
  //    printf("      IISOIXFR!\r\n");
  ////    TU_LOG2("      IISOIXFR!\r\n");
  //  }
}

#endif
