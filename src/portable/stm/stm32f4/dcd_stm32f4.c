/**************************************************************************/
/*!
    @file     dcd_nrf5x.c
    @author   hathach

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, Scott Shawcroft for Adafruit Industries
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_STM32F4

#include "device/dcd.h"
#include "stm32f4xx.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
#define DEVICE_BASE (USB_OTG_DeviceTypeDef *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE)
#define OUT_EP_BASE (USB_OTG_OUTEndpointTypeDef *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE)
#define IN_EP_BASE (USB_OTG_INEndpointTypeDef *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE)
#define FIFO_BASE(_x) (uint32_t *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_FIFO_BASE + _x * USB_OTG_FIFO_SIZE)

static ATTR_ALIGNED(4) uint32_t _setup_packet[6];
static uint8_t _setup_offs; // We store up to 3 setup packets.

typedef struct {
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint8_t max_size;
} xfer_ctl_t;

xfer_ctl_t xfer_status[4][2];

#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]


// Setup the control endpoint 0.
static void bus_reset(void) {
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE;
  // USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE;

  for(int n = 0; n < 4; n++) {
    out_ep[n].DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
  }

  dev->DAINTMSK |= (1 << USB_OTG_DAINTMSK_OEPM_Pos) | (1 << USB_OTG_DAINTMSK_IEPM_Pos);
  dev->DOEPMSK |= USB_OTG_DOEPMSK_STUPM | USB_OTG_DOEPMSK_XFRCM;
  dev->DIEPMSK |= USB_OTG_DIEPMSK_TOM | USB_OTG_DIEPMSK_XFRCM;

  // FIFO sizes are set up by the following rules (each word 32-bits):
  // OUT FIFO uses (based on page 1354 of Rev 17 of reference manual):
  // * 10 locations in hardware for setup packets + setup control words
  // (up to 3 setup packets).
  // * 2 locations for OUT endpoint control words.
  // * 64 bytes for maximum control packet size.
  // * 1 location for global NAK (not required/used here).
  // IN FIFO uses 64 bytes for maximum control packet size.
  //
  // However, for OUT FIFO, 10 + 2 + 16 = 28 doesn't seem to work (TODO: why?).
  // Minimum that works in practice is 35, so allocate 40 32-bit locations
  // as a buffer.
  USB_OTG_FS->GRXFSIZ = 40;
  USB_OTG_FS->DIEPTXF0_HNPTXFSIZ |= (16 << USB_OTG_TX0FD_Pos); // 16 32-bit words = 64 bytes

  out_ep[0].DOEPTSIZ |= (3 << USB_OTG_DOEPTSIZ_STUPCNT_Pos);

  USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_IEPINT;
}

static void end_of_reset(void) {
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE;
  // On current silicon on the Full Speed core, speed is fixed to Full Speed.
  // However, keep for debugging and in case Low Speed is ever supported.
  uint32_t enum_spd = (dev->DSTS & USB_OTG_DSTS_ENUMSPD_Msk) >> USB_OTG_DSTS_ENUMSPD_Pos;

  // Maximum packet size for EP 0 is set for both directions by writing
  // DIEPCTL.
  if(enum_spd == 0x03) {
    // 64 bytes
    in_ep[0].DIEPCTL &= ~(0x03 << USB_OTG_DIEPCTL_MPSIZ_Pos);
  } else {
    // 8 bytes
    in_ep[0].DIEPCTL |= (0x03 << USB_OTG_DIEPCTL_MPSIZ_Pos);
  }
  xfer_status[0][TUSB_DIR_OUT].max_size = 64;
  xfer_status[0][TUSB_DIR_IN].max_size = 64;
}


/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
bool dcd_init (uint8_t rhport)
{
  (void) rhport;

  // Programming model begins on page 1336 of Rev 17 of reference manual.
  USB_OTG_FS->GAHBCFG |= USB_OTG_GAHBCFG_TXFELVL | USB_OTG_GAHBCFG_GINT;

  // No HNP/SRP (no OTG support), program timeout later, turnaround
  // programmed for 18 MHz.
  USB_OTG_FS->GUSBCFG |= (0x0C << USB_OTG_GUSBCFG_TRDT_Pos);

  // Clear all used interrupts
  USB_OTG_FS->GINTSTS |= USB_OTG_GINTSTS_OTGINT | USB_OTG_GINTSTS_MMIS | \
    USB_OTG_GINTSTS_USBRST | USB_OTG_GINTSTS_ENUMDNE | \
    USB_OTG_GINTSTS_ESUSP | USB_OTG_GINTSTS_USBSUSP | USB_OTG_GINTSTS_SOF;

  // Required as part of core initialization.
  USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_OTGINT | USB_OTG_GINTMSK_MMISM;

  USB_OTG_DeviceTypeDef * dev = ((USB_OTG_DeviceTypeDef *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE));

  // If USB host misbehaves during status portion of control xfer
  // (non zero-length packet), send STALL back and discard. Full speed.
  dev->DCFG |=  USB_OTG_DCFG_NZLSOHSK | (3 << USB_OTG_DCFG_DSPD_Pos);
  /* USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM | \
    USB_OTG_GINTMSK_ESUSPM | USB_OTG_GINTMSK_USBSUSPM | \
    USB_OTG_GINTMSK_SOFM; */
  USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM | USB_OTG_GINTMSK_RXFLVLM;

  // Enable pullup, enable peripheral.
  USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBUSBSEN | USB_OTG_GCCFG_PWRDWN;

  return true;
}

void dcd_int_enable (uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(OTG_FS_IRQn);
}

void dcd_int_disable (uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(OTG_FS_IRQn);
}

void dcd_connect (uint8_t rhport)
{

}

void dcd_disconnect (uint8_t rhport)
{

}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
}

void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  // Nothing to do
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;

  // uint8_t const epnum = edpt_number(desc_edpt->bEndpointAddress);
  // uint8_t const dir   = edpt_dir(desc_edpt->bEndpointAddress);
  //
  // UsbDeviceDescBank* bank = &sram_registers[epnum][dir];
  // uint32_t size_value = 0;
  // while (size_value < 7) {
  //   if (1 << (size_value + 3) == desc_edpt->wMaxPacketSize.size) {
  //     break;
  //   }
  //   size_value++;
  // }
  //
  // // unsupported endpoint size
  // if ( size_value == 7 && desc_edpt->wMaxPacketSize.size != 1023 ) return false;
  //
  // bank->PCKSIZE.bit.SIZE = size_value;
  //
  // UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];
  //
  // if ( dir == TUSB_DIR_OUT )
  // {
  //   ep->EPCFG.bit.EPTYPE0 = desc_edpt->bmAttributes.xfer + 1;
  //   ep->EPINTENSET.bit.TRCPT0 = true;
  // }else
  // {
  //   ep->EPCFG.bit.EPTYPE1 = desc_edpt->bmAttributes.xfer + 1;
  //   ep->EPINTENSET.bit.TRCPT1 = true;
  // }

  // return true;
  return false;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE;
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;

  uint16_t num_packets = (total_bytes / xfer->max_size);
  uint8_t short_packet_size = total_bytes % xfer->max_size;

  // Zero-size packet is special case.
  if(short_packet_size > 0 || (total_bytes == 0)) {
    num_packets++;
  }

  // IN and OUT endpoint xfers are interrupt-driven, we just schedule them
  // here.
  if(dir == TUSB_DIR_IN) {
    in_ep[epnum].DIEPTSIZ = (num_packets << USB_OTG_DIEPTSIZ_PKTCNT_Pos) | \
        ((total_bytes & USB_OTG_DIEPTSIZ_XFRSIZ_Msk) << USB_OTG_DIEPTSIZ_XFRSIZ_Pos);
    in_ep[epnum].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;
    dev->DIEPEMPMSK |= (1 << epnum);
  } else {
    out_ep[epnum].DOEPTSIZ = (num_packets << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | \
        (((xfer->max_size * num_packets) & USB_OTG_DOEPTSIZ_XFRSIZ_Msk) << USB_OTG_DOEPTSIZ_XFRSIZ_Pos);
    out_ep[epnum].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
  }

  return true;
}

bool dcd_edpt_stalled (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  // control is never got halted
  if ( ep_addr == 0 ) {
      return false;
  }

  // uint8_t const epnum = edpt_number(ep_addr);
  // UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];
  // return (edpt_dir(ep_addr) == TUSB_DIR_IN ) ? ep->EPINTFLAG.bit.STALL1 : ep->EPINTFLAG.bit.STALL0;
  return true;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  // uint8_t const epnum = edpt_number(ep_addr);
  // UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];
  //
  // if (edpt_dir(ep_addr) == TUSB_DIR_IN) {
  //     ep->EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ1;
  // } else {
  //     ep->EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ0;
  //
  //     // for control, stall both IN & OUT
  //     if (ep_addr == 0) {
  //       ep->EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ1;
  //     }
  // }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  // uint8_t const epnum = edpt_number(ep_addr);
  // UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];
  //
  // if (edpt_dir(ep_addr) == TUSB_DIR_IN) {
  //   ep->EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_STALLRQ1;
  // } else {
  //   ep->EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_STALLRQ0;
  // }
}

bool dcd_edpt_busy (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  // // USBD shouldn't check control endpoint state
  // if ( 0 == ep_addr ) return false;
  //
  // uint8_t const epnum = edpt_number(ep_addr);
  // UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];
  //
  // if (edpt_dir(ep_addr) == TUSB_DIR_IN) {
  //   return ep->EPINTFLAG.bit.TRCPT1 == 0 && ep->EPSTATUS.bit.BK1RDY == 1;
  // }
  // return ep->EPINTFLAG.bit.TRCPT0 == 0 && ep->EPSTATUS.bit.BK0RDY == 1;
  return true;
}

/*------------------------------------------------------------------*/

// TODO: Split into "receive on endpoint 0" and "receive generic"; endpoint 0's
// DOEPTSIZ register is smaller than the others, and so is insufficient for
// determining how much of an OUT transfer is actually remaining.
static void receive_packet(xfer_ctl_t * xfer, /* USB_OTG_OUTEndpointTypeDef * out_ep, */ uint16_t xfer_size) {
  uint32_t * rx_fifo = FIFO_BASE(0);

  // See above TODO
  // uint16_t remaining = (out_ep->DOEPTSIZ & USB_OTG_DOEPTSIZ_XFRSIZ_Msk) >> USB_OTG_DOEPTSIZ_XFRSIZ_Pos;
  // xfer->queued_len = xfer->total_len - remaining;

  uint16_t remaining = xfer->total_len - xfer->queued_len;

  // FIXME: Handle unexpected final packet length.
  uint16_t to_recv_size = (remaining > xfer->max_size) ? xfer->max_size : remaining;
  uint8_t to_recv_rem = to_recv_size % 4;
  uint16_t to_recv_size_aligned = to_recv_size - to_recv_rem;

  // Do not assume xfer buffer is aligned.
  uint8_t * base = (xfer->buffer + xfer->queued_len);
  for(uint16_t i = 0; i < to_recv_size_aligned; i += 4) {
    uint32_t tmp = (* rx_fifo);
    base[i] = tmp & 0x000000FF;
    base[i + 1] = (tmp & 0x0000FF00) >> 8;
    base[i + 2] = (tmp & 0x00FF0000) >> 16;
    base[i + 3] = (tmp & 0xFF000000) >> 24;
  }

  // Do not read invalid bytes from RX FIFO.
  if(to_recv_rem != 0) {
    uint32_t tmp = (* rx_fifo);
    uint8_t * last_32b_bound = base + to_recv_size_aligned;

    last_32b_bound[0] = tmp & 0x000000FF;
    if(to_recv_rem > 1) {
      last_32b_bound[1] = (tmp & 0x0000FF00) >> 8;
    }
    if(to_recv_rem > 2) {
      last_32b_bound[2] = (tmp & 0x00FF0000) >> 16;
    }
  }

  xfer->queued_len += xfer_size;
}

static void transmit_packet(xfer_ctl_t * xfer, USB_OTG_INEndpointTypeDef * in_ep, uint8_t fifo_num) {
  uint32_t * tx_fifo = FIFO_BASE(fifo_num);

  uint16_t remaining = (in_ep->DIEPTSIZ & USB_OTG_DIEPTSIZ_XFRSIZ_Msk) >> USB_OTG_DIEPTSIZ_XFRSIZ_Pos;
  xfer->queued_len = xfer->total_len - remaining;

  uint16_t to_xfer_size = (remaining > xfer->max_size) ? xfer->max_size : remaining;
  uint8_t to_xfer_rem = to_xfer_size % 4;
  uint16_t to_xfer_size_aligned = to_xfer_size - to_xfer_rem;

  // Buffer might not be aligned to 32b, so we need to force alignment
  // by copying to a temp var.
  uint8_t * base = (xfer->buffer + xfer->queued_len);
  for(uint16_t i = 0; i < to_xfer_size_aligned; i += 4) {
    uint32_t tmp = base[i] | (base[i + 1] << 8) | (base[i + 2] << 16) | (base[i + 3] << 24);
    (* tx_fifo) = tmp;
  }

  // Do not read beyond end of buffer if not divisible by 4.
  if(to_xfer_rem != 0) {
    uint32_t tmp = 0;
    uint8_t * last_32b_bound = base + to_xfer_size_aligned;

    tmp |= last_32b_bound[0];
    if(to_xfer_rem > 1) {
      tmp |= (last_32b_bound[1] << 8);
    }
    if(to_xfer_rem > 2) {
      tmp |= (last_32b_bound[2] << 16);
    }

    (* tx_fifo) = tmp;
  }
}

void OTG_FS_IRQHandler(void) {
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE;
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE;
  uint32_t * rx_fifo = FIFO_BASE(0);

  uint32_t int_status = USB_OTG_FS->GINTSTS;

  if(int_status & USB_OTG_GINTSTS_USBRST) {
    // USBRST is start of reset.
    USB_OTG_FS->GINTSTS = USB_OTG_GINTSTS_USBRST;
    bus_reset();
  }

  if(int_status & USB_OTG_GINTSTS_ENUMDNE) {
    // ENUMDNE detects speed of the link. For full-speed, we
    // always expect the same value. This interrupt is considered
    // the end of reset.
    USB_OTG_FS->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;
    end_of_reset();
    dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
  }

  // Read a packet here; the RX FIFO must be cleared in order for the core
  // to continue processing. So read into an intermediate buffer.
  if(int_status & USB_OTG_GINTSTS_RXFLVL) {
    USB_OTG_FS->GINTSTS = USB_OTG_GINTSTS_RXFLVL;

    // Receive data before reenabling interrupts.
    USB_OTG_FS->GINTMSK &= (~USB_OTG_GINTMSK_RXFLVLM);

    // Pop control word off FIFO (completed xfers will have 2 control words,
    // we only pop one ctl word each interrupt).
    uint32_t ctl_word = USB_OTG_FS->GRXSTSP;
    uint8_t pktsts = (ctl_word & USB_OTG_GRXSTSP_PKTSTS_Msk) >> USB_OTG_GRXSTSP_PKTSTS_Pos;
    uint8_t epnum = (ctl_word &  USB_OTG_GRXSTSP_EPNUM_Msk) >>  USB_OTG_GRXSTSP_EPNUM_Pos;
    uint16_t bcnt = (ctl_word & USB_OTG_GRXSTSP_BCNT_Msk) >> USB_OTG_GRXSTSP_BCNT_Pos;

    switch(pktsts) {
      case 0x01: // Global OUT NAK (Interrupt)
        break;
      case 0x02: // Out packet recvd
        {
          xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, TUSB_DIR_OUT);
          receive_packet(xfer, bcnt);
        }
        break;
      case 0x03: // Out packet done (Interrupt)
        break;
      case 0x04: // Setup packet done (Interrupt)
        _setup_offs = 2 - ((out_ep[epnum].DOEPTSIZ & USB_OTG_DOEPTSIZ_STUPCNT_Msk) >> USB_OTG_DOEPTSIZ_STUPCNT_Pos);
        out_ep[epnum].DOEPTSIZ |= (3 << USB_OTG_DOEPTSIZ_STUPCNT_Pos);
        break;
      case 0x06: // Setup packet recvd
        {
          uint8_t setup_left = ((out_ep[epnum].DOEPTSIZ & USB_OTG_DOEPTSIZ_STUPCNT_Msk) >> USB_OTG_DOEPTSIZ_STUPCNT_Pos);
          // We can receive up to three setup packets in succession, but
          // only the last one is valid.
          _setup_packet[4 - 2*setup_left] = (* rx_fifo);
          _setup_packet[5 - 2*setup_left] = (* rx_fifo);
        }
        break;
      default: // Invalid, do something here?
        break;
    }

    USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
  }

  // OUT endpoint interrupt handling.
  if(int_status & USB_OTG_GINTSTS_OEPINT) {

    // DAINT for a given EP clears when DOEPINTx is cleared.
    // OEPINT will be cleared when DAINT's out bits are cleared.
    for(int n = 0; n < 4; n++) {
      xfer_ctl_t * xfer = XFER_CTL_BASE(n, TUSB_DIR_OUT);
      if(dev->DAINT & (1 << (USB_OTG_DAINT_OEPINT_Pos + n))) {
        // SETUP packet Setup Phase done.
        if(out_ep[n].DOEPINT & USB_OTG_DOEPINT_STUP) {
          out_ep[n].DOEPINT =  USB_OTG_DOEPINT_STUP;
          dcd_event_setup_received(0, (uint8_t*) &_setup_packet[2*_setup_offs], true);
          _setup_offs = 0;
        }

        // OUT XFER complete (single packet).
        if(out_ep[n].DOEPINT & USB_OTG_DOEPINT_XFRC) {
          out_ep[n].DOEPINT = USB_OTG_DOEPINT_XFRC;

          // TODO: Because of endpoint 0's constrained size, we handle XFRC
          // on a packet-basis. It would be more efficient to only trigger
          // XFRC on a completed transfer for non-0 endpoints.
          dcd_event_xfer_complete(0, n, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }
      }
    }
  }

  // IN endpoint interrupt handling.
  if(int_status & USB_OTG_GINTSTS_IEPINT) {

    // DAINT for a given EP clears when DIEPINTx is cleared.
    // IEPINT will be cleared when DAINT's out bits are cleared.
    for(uint8_t n = 0; n < 4; n++) {
      xfer_ctl_t * xfer = XFER_CTL_BASE(n, TUSB_DIR_IN);

      if(dev->DAINT & (1 << (USB_OTG_DAINT_IEPINT_Pos + n))) {
        // IN XFER complete.
        if(in_ep[n].DIEPINT & USB_OTG_DIEPINT_XFRC) {
          in_ep[n].DIEPINT = USB_OTG_DIEPINT_XFRC;
          dev->DIEPEMPMSK &= ~(1 << n); // Turn off TXFE b/c xfer inactive.
          dcd_event_xfer_complete(0, n | TUSB_DIR_IN_MASK, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }

        // XFER FIFO empty
        if(in_ep[n].DIEPINT & USB_OTG_DIEPINT_TXFE) {
          in_ep[n].DIEPINT = USB_OTG_DIEPINT_TXFE;
          transmit_packet(xfer, &in_ep[n], n);
        }
      }
    }
  }


}

#endif
