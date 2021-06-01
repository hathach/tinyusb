/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Rafael Silva (@perigoso)
 * Copyright (c) 2021 Ha Thach (tinyusb.org)
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

#if TUSB_OPT_DEVICE_ENABLED && ( \
  (CFG_TUSB_MCU == OPT_MCU_EFM32GG) || \
  (CFG_TUSB_MCU == OPT_MCU_EFM32GG11) || \
  (CFG_TUSB_MCU == OPT_MCU_EFM32GG12) )

/* Silabs */
#include "em_device.h"

#include "device/dcd.h"

/* 
 * Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
 * We disable SOF for now until needed later on
 */
#define USE_SOF     0

/* 
 * Number of endpoints
 * 12 software-configurable endpoints (6 IN, 6 OUT) in addition to endpoint 0
 */
#define EP_COUNT        7

/* FIFO size in bytes */
#define EP_FIFO_SIZE    2048

/* Max number of IN EP FIFOs */
#define EP_FIFO_NUM     7

/* */
typedef struct {
    uint8_t *buffer;
    uint16_t total_len;
    uint16_t queued_len;
    uint16_t max_size;
    bool short_packet;
} xfer_ctl_t;

static uint32_t _setup_packet[2];

#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]
static xfer_ctl_t xfer_status[EP_COUNT][2];

/* Keep count of how many FIFOs are in use */
static uint8_t _allocated_fifos = 1; /* FIFO0 is always in use */

static volatile uint32_t* tx_fifo[EP_FIFO_NUM] = {
  USB->FIFO0D,
  USB->FIFO1D,
  USB->FIFO2D,
  USB->FIFO3D,
  USB->FIFO4D,
  USB->FIFO5D,
  USB->FIFO6D,
};

/* Register Helpers */
#define DCTL_WO_BITMASK     (USB_DCTL_CGOUTNAK | USB_DCTL_SGOUTNAK | USB_DCTL_CGNPINNAK | USB_DCTL_SGNPINNAK)
#define GUSBCFG_WO_BITMASK  (USB_GUSBCFG_CORRUPTTXPKT)
#define DEPCTL_WO_BITMASK   (USB_DIEP_CTL_CNAK | USB_DIEP_CTL_SNAK | USB_DIEP_CTL_SETD0PIDEF | USB_DIEP_CTL_SETD1PIDOF)

/* Will either return an unused FIFO number, or 0 if all are used. */
static uint8_t get_free_fifo(void)
{
  if(_allocated_fifos < EP_FIFO_NUM) return _allocated_fifos++;
  return 0;
}

/*
static void flush_rx_fifo(void)
{
  USB->GRSTCTL = USB_GRSTCTL_RXFFLSH;
  while(USB->GRSTCTL & USB_GRSTCTL_RXFFLSH);
} 
*/

static void flush_tx_fifo(uint8_t fifo_num)
{
  USB->GRSTCTL = USB_GRSTCTL_TXFFLSH | (fifo_num << _USB_GRSTCTL_TXFNUM_SHIFT);
  while(USB->GRSTCTL & USB_GRSTCTL_TXFFLSH);
}

/* Setup the control endpoint 0. */
static void bus_reset(void)
{
  USB->DOEP0CTL |= USB_DIEP_CTL_SNAK;
  for(uint8_t i = 0; i < EP_COUNT - 1; i++)
  {
    USB->DOEP[i].CTL |= USB_DIEP_CTL_SNAK;
  }
  
  /* reset address */
  USB->DCFG &= ~_USB_DCFG_DEVADDR_MASK;

  USB->DAINTMSK |= USB_DAINTMSK_OUTEPMSK0 | USB_DAINTMSK_INEPMSK0;
  USB->DOEPMSK |= USB_DOEPMSK_SETUPMSK | USB_DOEPMSK_XFERCOMPLMSK;
  USB->DIEPMSK |= USB_DIEPMSK_TIMEOUTMSK | USB_DIEPMSK_XFERCOMPLMSK;

  /* 
   * - All EP OUT shared a unique OUT FIFO which uses
   *   * 10 locations in hardware for setup packets + setup control words (up to 3 setup packets).
   *   * 2 locations for OUT endpoint control words.
   *   * 16 for largest packet size of 64 bytes. ( TODO Highspeed is 512 bytes)
   *   * 1 location for global NAK (not required/used here).
   *   * It is recommended to allocate 2 times the largest packet size, therefore
   *  Recommended value = 10 + 1 + 2 x (16+2) = 47 --> Let's make it 52
   */
  flush_tx_fifo(_USB_GRSTCTL_TXFNUM_FALL);  // Flush All
  USB->GRXFSIZ = 52;

  /* Control IN uses FIFO 0 with 64 bytes ( 16 32-bit word ) */
  USB->GNPTXFSIZ = (16 << _USB_GNPTXFSIZ_NPTXFINEPTXF0DEP_SHIFT) | (USB->GRXFSIZ & _USB_GNPTXFSIZ_NPTXFSTADDR_MASK);

  /* Ready to receive SETUP packet */
  USB->DOEP0TSIZ |= (1 << _USB_DOEP0TSIZ_SUPCNT_SHIFT);

  USB->GINTMSK |= USB_GINTMSK_IEPINTMSK | USB_GINTMSK_OEPINTMSK;
}

static void enum_done_processing(void)
{
  /* Maximum packet size for EP 0 is set for both directions by writing DIEPCTL */
  if((USB->DSTS & _USB_DSTS_ENUMSPD_MASK) == USB_DSTS_ENUMSPD_FS)
  { 
    /* Full Speed (PHY on 48 MHz) */
    USB->DOEP0CTL = (USB->DOEP0CTL & ~_USB_DOEP0CTL_MPS_MASK) | _USB_DOEP0CTL_MPS_64B; /* Maximum Packet Size 64 bytes */
    USB->DOEP0CTL &= ~_USB_DOEP0CTL_STALL_MASK; /* clear Stall */
    xfer_status[0][TUSB_DIR_OUT].max_size = 64;
    xfer_status[0][TUSB_DIR_IN].max_size = 64;
  }
  else
  { 
    /* Low Speed (PHY on 6 MHz) */
    USB->DOEP0CTL = (USB->DOEP0CTL & ~_USB_DOEP0CTL_MPS_MASK) | _USB_DOEP0CTL_MPS_8B; /* Maximum Packet Size 64 bytes */
    USB->DOEP0CTL &= ~_USB_DOEP0CTL_STALL_MASK; /* clear Stall */
    xfer_status[0][TUSB_DIR_OUT].max_size = 8;
    xfer_status[0][TUSB_DIR_IN].max_size = 8;
  }
}


/*------------------------------------------------------------------*/
/* Controller API                                                   */
/*------------------------------------------------------------------*/
void dcd_init(uint8_t rhport)
{
  (void) rhport;

  /* Reset Core */
  USB->PCGCCTL &= ~USB_PCGCCTL_STOPPCLK;
  USB->PCGCCTL &= ~(USB_PCGCCTL_PWRCLMP | USB_PCGCCTL_RSTPDWNMODULE);

  /* Core Soft Reset */
  USB->GRSTCTL |= USB_GRSTCTL_CSFTRST;
  while(USB->GRSTCTL & USB_GRSTCTL_CSFTRST);

  while(!(USB->GRSTCTL & USB_GRSTCTL_AHBIDLE));

  /* Enable PHY pins */
  USB->ROUTE = USB_ROUTE_PHYPEN;

  dcd_disconnect(rhport);

  /* 
   * Set device speed (Full speed PHY)
   * Stall on non-zero len status OUT packets (ctrl transfers)
   * periodic frame interval to 80% 
   */
  USB->DCFG = (USB->DCFG & ~(_USB_DCFG_DEVSPD_MASK | _USB_DCFG_PERFRINT_MASK)) | USB_DCFG_DEVSPD_FS | USB_DCFG_NZSTSOUTHSHK;
  
  /* Enable Global Interrupts */
  USB->GAHBCFG = (USB->GAHBCFG & ~_USB_GAHBCFG_HBSTLEN_MASK) | USB_GAHBCFG_GLBLINTRMSK;

  /* Force Device Mode */
  USB->GUSBCFG = (USB->GUSBCFG & ~(GUSBCFG_WO_BITMASK | USB_GUSBCFG_FORCEHSTMODE)) | USB_GUSBCFG_FORCEDEVMODE;

  /* No Overrides */
  USB->GOTGCTL &= ~(USB_GOTGCTL_BVALIDOVVAL | USB_GOTGCTL_BVALIDOVEN | USB_GOTGCTL_VBVALIDOVVAL);

  /* Ignore frame numbers on ISO transfers. */
  USB->DCTL = (USB->DCTL & ~DCTL_WO_BITMASK) | USB_DCTL_IGNRFRMNUM;

  /* Setting SNAKs */
  USB->DOEP0CTL |= USB_DIEP_CTL_SNAK;
  for(uint8_t i = 0; i < EP_COUNT - 1; i++)
  {
    USB->DOEP[i].CTL |= USB_DIEP_CTL_SNAK;
  }

  /* D. Interruption masking */
  /* Disable all device interrupts */
  USB->DIEPMSK  = 0;
  USB->DOEPMSK  = 0;
  USB->DAINTMSK = 0;
  USB->DIEPEMPMSK = 0;
  USB->GINTMSK = 0;
  USB->GOTGINT = ~0U; /* clear OTG ints */
  USB->GINTSTS = ~0U; /* clear pending ints */
  USB->GINTMSK = USB_GINTMSK_MODEMISMSK  |
              #if USE_SOF
                 USB_GINTMSK_SOFMSK      |
              #endif
                 USB_GINTMSK_ERLYSUSPMSK |
                 USB_GINTMSK_USBSUSPMSK  |
                 USB_GINTMSK_USBRSTMSK   |
                 USB_GINTMSK_ENUMDONEMSK |
                 USB_GINTMSK_RESETDETMSK |
                 USB_GINTMSK_DISCONNINTMSK;

  NVIC_ClearPendingIRQ(USB_IRQn);

  dcd_connect(rhport);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;

  USB->DCFG = (USB->DCFG & ~_USB_DCFG_DEVADDR_MASK) | (dev_addr << _USB_DCFG_DEVADDR_SHIFT);

  /* Response with status after changing device address */
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

void dcd_connect(uint8_t rhport)
{
  (void) rhport;

  /* connect by enabling internal pull-up resistor on D+/D- */
  USB->DCTL &= ~(DCTL_WO_BITMASK | USB_DCTL_SFTDISCON);
}

void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;

  /* disconnect by disabling internal pull-up resistor on D+/D- */
  USB->DCTL = (USB->DCTL & ~(DCTL_WO_BITMASK)) | USB_DCTL_SFTDISCON;
}

/*------------------------------------------------------------------*/
/* DCD Endpoint Port                                                */
/*------------------------------------------------------------------*/
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  if(dir == TUSB_DIR_IN)
  {
    if(epnum == 0)
    {
      USB->DIEP0CTL = (USB->DIEP0CTL & ~DEPCTL_WO_BITMASK) | USB_DIEP0CTL_SNAK | USB_DIEP0CTL_STALL;

      flush_tx_fifo(_USB_GRSTCTL_TXFNUM_F0);
    }
    else
    {
      /* Only disable currently enabled non-control endpoint */
      if(USB->DIEP[epnum - 1].CTL & USB_DIEP_CTL_EPENA) 
      {
        USB->DIEP[epnum - 1].CTL = (USB->DIEP[epnum - 1].CTL & ~DEPCTL_WO_BITMASK) | USB_DIEP_CTL_EPDIS | USB_DIEP_CTL_SNAK | USB_DIEP_CTL_STALL;
        while(!(USB->DIEP[epnum - 1].INT & USB_DIEP_INT_EPDISBLD));
        USB->DIEP[epnum - 1].INT |= USB_DIEP_INT_EPDISBLD;
      }
      else
      {
        USB->DIEP[epnum - 1].CTL = (USB->DIEP[epnum - 1].CTL & ~DEPCTL_WO_BITMASK) | USB_DIEP_CTL_SNAK | USB_DIEP_CTL_STALL;
      }

      /* Flush the FIFO */
      uint8_t const fifo_num = ((USB->DIEP[epnum - 1].CTL & _USB_DIEP_CTL_TXFNUM_MASK) >> _USB_DIEP_CTL_TXFNUM_SHIFT);
      flush_tx_fifo(fifo_num);
    }
  }
  else
  {
    if(epnum == 0)
    {
      USB->DOEP0CTL = (USB->DOEP0CTL & ~DEPCTL_WO_BITMASK) | USB_DIEP0CTL_STALL;
    }
    else
    {
      /* Only disable currently enabled non-control endpoint */
      if(USB->DOEP[epnum - 1].CTL & USB_DIEP_CTL_EPENA) 
      {
        /* Asserting GONAK is required to STALL an OUT endpoint. */
        USB->DCTL |= USB_DCTL_SGOUTNAK;
        while(!(USB->GINTSTS & USB_GINTSTS_GOUTNAKEFF));
        
        /* Disable the endpoint. Note that only STALL and not SNAK is set here. */
        USB->DOEP[epnum - 1].CTL = (USB->DOEP[epnum - 1].CTL & ~DEPCTL_WO_BITMASK) | USB_DIEP_CTL_EPDIS | USB_DIEP_CTL_STALL;
        while(USB->DOEP[epnum - 1].INT & USB_DIEP_INT_EPDISBLD);
        USB->DOEP[epnum - 1].INT |= USB_DIEP_INT_EPDISBLD;

        /* Allow other OUT endpoints to keep receiving. */
        USB->DCTL |= USB_DCTL_CGOUTNAK;
      }
      else
      {
        USB->DIEP[epnum - 1].CTL = (USB->DIEP[epnum - 1].CTL & ~DEPCTL_WO_BITMASK) | USB_DIEP_CTL_STALL;
      }
    }
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  if(dir == TUSB_DIR_IN)
  {
    if(epnum == 0)
    {
      USB->DIEP0CTL &= ~(DEPCTL_WO_BITMASK | USB_DIEP0CTL_STALL);
    }
    else
    {
      USB->DIEP[epnum - 1].CTL &= ~(DEPCTL_WO_BITMASK | USB_DIEP_CTL_STALL);
    
      /* Required by USB spec to reset DATA toggle bit to DATA0 on interrupt and bulk endpoints. */
      uint8_t eptype = (USB->DIEP[epnum - 1].CTL & _USB_DIEP_CTL_EPTYPE_MASK) >> _USB_DIEP_CTL_EPTYPE_SHIFT;

      if((eptype == _USB_DIEP_CTL_EPTYPE_BULK) || (eptype == _USB_DIEP_CTL_EPTYPE_INT))
      {
        USB->DIEP[epnum - 1].CTL |= USB_DIEP_CTL_SETD0PIDEF;
      }
    }
  }
  else
  {
    if(epnum == 0)
    {
      USB->DOEP0CTL &= ~(DEPCTL_WO_BITMASK | USB_DOEP0CTL_STALL);
    }
    else
    {
      USB->DOEP[epnum - 1].CTL &= ~(DEPCTL_WO_BITMASK | USB_DOEP_CTL_STALL);
    
      /* Required by USB spec to reset DATA toggle bit to DATA0 on interrupt and bulk endpoints. */
      uint8_t eptype = (USB->DOEP[epnum - 1].CTL & _USB_DOEP_CTL_EPTYPE_MASK) >> _USB_DOEP_CTL_EPTYPE_SHIFT;

      if((eptype == _USB_DOEP_CTL_EPTYPE_BULK) || (eptype == _USB_DOEP_CTL_EPTYPE_INT))
      {
        USB->DOEP[epnum - 1].CTL |= USB_DOEP_CTL_SETD0PIDEF;
      }
    }
  }
}

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  (void)rhport;

  uint8_t const epnum = tu_edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t const dir = tu_edpt_dir(p_endpoint_desc->bEndpointAddress);

  TU_ASSERT(p_endpoint_desc->wMaxPacketSize.size <= 64);
  TU_ASSERT(epnum < EP_COUNT);
  TU_ASSERT(epnum != 0);

  xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, dir);
  xfer->max_size = p_endpoint_desc->wMaxPacketSize.size;

  if(dir == TUSB_DIR_OUT)
  {
    USB->DOEP[epnum - 1].CTL |= USB_DOEP_CTL_USBACTEP |
                                (p_endpoint_desc->bmAttributes.xfer << _USB_DOEP_CTL_EPTYPE_SHIFT) |
                                (p_endpoint_desc->wMaxPacketSize.size << _USB_DOEP_CTL_MPS_SHIFT);
    USB->DAINTMSK |= (1 << (_USB_DAINTMSK_OUTEPMSK0_SHIFT + epnum));
  }
  else
  {
    uint8_t fifo_num = get_free_fifo();
    TU_ASSERT(fifo_num != 0);

    USB->DIEP[epnum - 1].CTL &= ~(_USB_DIEP_CTL_TXFNUM_MASK | _USB_DIEP_CTL_EPTYPE_MASK | USB_DIEP_CTL_SETD0PIDEF | _USB_DIEP_CTL_MPS_MASK);
    USB->DIEP[epnum - 1].CTL |= USB_DIEP_CTL_USBACTEP |
                                (fifo_num << _USB_DIEP_CTL_TXFNUM_SHIFT) |
                                (p_endpoint_desc->bmAttributes.xfer << _USB_DIEP_CTL_EPTYPE_SHIFT) |
                                ((p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS) ? USB_DIEP_CTL_SETD0PIDEF : 0) |
                                (p_endpoint_desc->wMaxPacketSize.size << 0);

    USB->DAINTMSK |= (1 << epnum);

    /* Both TXFD and TXSA are in unit of 32-bit words. */
    /* IN FIFO 0 was configured during enumeration, hence the "+ 16". */
    uint16_t const allocated_size = (USB->GRXFSIZ & _USB_GRXFSIZ_RXFDEP_MASK) + 16;
    uint16_t const fifo_size = (EP_FIFO_SIZE/4 - allocated_size) / (EP_FIFO_NUM-1);
    uint32_t const fifo_offset = allocated_size + fifo_size*(fifo_num-1);

    /* DIEPTXF starts at FIFO #1. */
    volatile uint32_t* usb_dieptxf = &USB->DIEPTXF1;
    usb_dieptxf[epnum - 1] = (fifo_size << _USB_DIEPTXF1_INEPNTXFDEP_SHIFT) | fifo_offset;
  }
  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  (void)rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->buffer       = buffer;
  xfer->total_len    = total_bytes;
  xfer->queued_len   = 0;
  xfer->short_packet = false;

  uint16_t num_packets = (total_bytes / xfer->max_size);
  uint8_t short_packet_size = total_bytes % xfer->max_size;

  // Zero-size packet is special case.
  if(short_packet_size > 0 || (total_bytes == 0))
  {
    num_packets++;
  }

  // IN and OUT endpoint xfers are interrupt-driven, we just schedule them
  // here.
  if(dir == TUSB_DIR_IN)
  {
    if(epnum == 0)
    {
      // A full IN transfer (multiple packets, possibly) triggers XFRC.
      USB->DIEP0TSIZ = (num_packets << _USB_DIEP0TSIZ_PKTCNT_SHIFT) | total_bytes;
      USB->DIEP0CTL |= USB_DIEP0CTL_EPENA | USB_DIEP0CTL_CNAK; // Enable | CNAK
    }
    else
    {
      // A full IN transfer (multiple packets, possibly) triggers XFRC.
      USB->DIEP[epnum - 1].TSIZ = (num_packets << _USB_DIEP_TSIZ_PKTCNT_SHIFT) | total_bytes;
      USB->DIEP[epnum - 1].CTL |= USB_DIEP_CTL_EPENA | USB_DIEP_CTL_CNAK; // Enable | CNAK
    }
    
    // Enable fifo empty interrupt only if there are something to put in the fifo.
    if(total_bytes != 0)
    {
      USB->DIEPEMPMSK |= (1 << epnum);
    }
  }
  else
  {
    if(epnum == 0)
    {
      // A full IN transfer (multiple packets, possibly) triggers XFRC.
      USB->DOEP0TSIZ |= (1 << _USB_DOEP0TSIZ_PKTCNT_SHIFT) | ((xfer->max_size & _USB_DOEP0TSIZ_XFERSIZE_MASK) << _USB_DOEP0TSIZ_XFERSIZE_SHIFT);
      USB->DOEP0CTL |= USB_DOEP0CTL_EPENA | USB_DOEP0CTL_CNAK;
    }
    else
    {
      // A full IN transfer (multiple packets, possibly) triggers XFRC.
      USB->DOEP[epnum - 1].TSIZ |= (1 << _USB_DOEP_TSIZ_PKTCNT_SHIFT) | ((xfer->max_size & _USB_DOEP_TSIZ_XFERSIZE_MASK) << _USB_DOEP_TSIZ_XFERSIZE_SHIFT);
      USB->DOEP[epnum - 1].CTL |= USB_DOEP_CTL_EPENA | USB_DOEP_CTL_CNAK;
    }
  }
  return true;
}

/*------------------------------------------------------------------*/
/* IRQ                                                              */
/*------------------------------------------------------------------*/
void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;

  NVIC_EnableIRQ(USB_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;

  NVIC_DisableIRQ(USB_IRQn);
}

static void receive_packet(xfer_ctl_t *xfer, uint16_t xfer_size)
{
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t to_recv_size;

  if(remaining <= xfer->max_size)
  {
    /* Avoid buffer overflow. */
    to_recv_size = (xfer_size > remaining) ? remaining : xfer_size;
  }
  else
  {
    /* Room for full packet, choose recv_size based on what the microcontroller claims. */
    to_recv_size = (xfer_size > xfer->max_size) ? xfer->max_size : xfer_size;
  }

  uint8_t to_recv_rem = to_recv_size % 4;
  uint16_t to_recv_size_aligned = to_recv_size - to_recv_rem;

  /* Do not assume xfer buffer is aligned. */
  uint8_t *base = (xfer->buffer + xfer->queued_len);

  /* This for loop always runs at least once- skip if less than 4 bytes to collect. */
  if(to_recv_size >= 4)
  {
    for(uint16_t i = 0; i < to_recv_size_aligned; i += 4)
    {
      uint32_t tmp = (*USB->FIFO0D);
      base[i] = tmp & 0x000000FF;
      base[i + 1] = (tmp & 0x0000FF00) >> 8;
      base[i + 2] = (tmp & 0x00FF0000) >> 16;
      base[i + 3] = (tmp & 0xFF000000) >> 24;
    }
  }

  /* Do not read invalid bytes from RX FIFO. */
  if(to_recv_rem != 0)
  {
    uint32_t tmp = (*USB->FIFO0D);
    uint8_t *last_32b_bound = base + to_recv_size_aligned;

    last_32b_bound[0] = tmp & 0x000000FF;
    if(to_recv_rem > 1)
    {
      last_32b_bound[1] = (tmp & 0x0000FF00) >> 8;
    }
    if(to_recv_rem > 2)
    {
      last_32b_bound[2] = (tmp & 0x00FF0000) >> 16;
    }
  }

  xfer->queued_len += xfer_size;

  /* Per USB spec, a short OUT packet (including length 0) is always */
  /* indicative of the end of a transfer (at least for ctl, bulk, int). */
  xfer->short_packet = (xfer_size < xfer->max_size);
}

static void transmit_packet(xfer_ctl_t *xfer, uint8_t fifo_num)
{
  uint16_t remaining;
  if(fifo_num == 0)
  {
    remaining = (USB->DIEP0TSIZ & 0x7FFFFU) >> _USB_DIEP0TSIZ_XFERSIZE_SHIFT;
  }
  else
  {
    remaining = (USB->DIEP[fifo_num - 1].TSIZ & 0x7FFFFU) >> _USB_DIEP_TSIZ_XFERSIZE_SHIFT;
  }
  xfer->queued_len = xfer->total_len - remaining;

  uint16_t to_xfer_size = (remaining > xfer->max_size) ? xfer->max_size : remaining;
  uint8_t to_xfer_rem = to_xfer_size % 4;
  uint16_t to_xfer_size_aligned = to_xfer_size - to_xfer_rem;

  /* Buffer might not be aligned to 32b, so we need to force alignment by copying to a temp var. */
  uint8_t *base = (xfer->buffer + xfer->queued_len);

  /* This for loop always runs at least once- skip if less than 4 bytes to send off. */
  if(to_xfer_size >= 4)
  {
    for(uint16_t i = 0; i < to_xfer_size_aligned; i += 4)
    {
      uint32_t tmp = base[i] | (base[i + 1] << 8) | (base[i + 2] << 16) | (base[i + 3] << 24);
      *tx_fifo[fifo_num] = tmp;
    }
  }

  /* Do not read beyond end of buffer if not divisible by 4. */
  if(to_xfer_rem != 0)
  {
    uint32_t tmp = 0;
    uint8_t *last_32b_bound = base + to_xfer_size_aligned;

    tmp |= last_32b_bound[0];
    if(to_xfer_rem > 1)
    {
      tmp |= (last_32b_bound[1] << 8);
    }
    if(to_xfer_rem > 2)
    {
      tmp |= (last_32b_bound[2] << 16);
    }

    *tx_fifo[fifo_num] = tmp;
  }
}

static void read_rx_fifo(void)
{
  /*
   * Pop control word off FIFO (completed xfers will have 2 control words,
   * we only pop one ctl word each interrupt).
   */
  uint32_t const ctl_word = USB->GRXSTSP;
  uint8_t  const pktsts   = (ctl_word & _USB_GRXSTSP_PKTSTS_MASK) >> _USB_GRXSTSP_PKTSTS_SHIFT;
  uint8_t  const epnum    = (ctl_word & _USB_GRXSTSP_CHNUM_MASK ) >> _USB_GRXSTSP_CHNUM_SHIFT;
  uint16_t const bcnt     = (ctl_word & _USB_GRXSTSP_BCNT_MASK  ) >> _USB_GRXSTSP_BCNT_SHIFT;

  switch(pktsts)
  {
    case 0x01: /* Global OUT NAK (Interrupt) */
      break;

    case 0x02:
    { 
      /* Out packet recvd */
      xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, TUSB_DIR_OUT);
      receive_packet(xfer, bcnt);
    }
    break;

    case 0x03:
      /* Out packet done (Interrupt) */
      break;

    case 0x04: 
      /* Step 2: Setup transaction completed (Interrupt) */
      /* After this event, OEPINT interrupt will occur with SETUP bit set */
      if(epnum == 0)
      {
        USB->DOEP0TSIZ |= (1 << _USB_DOEP0TSIZ_SUPCNT_SHIFT);
      }
      
      break;

    case 0x06:
    { 
      /* Step1: Setup data packet received */

      /*
       * We can receive up to three setup packets in succession, but
       * only the last one is valid. Therefore we just overwrite it
       */
      _setup_packet[0] = (*USB->FIFO0D);
      _setup_packet[1] = (*USB->FIFO0D);
    }
    break;

    default: 
      /* Invalid, breakpoint. */
      TU_BREAKPOINT();
      break;
  }
}

static void handle_epout_ints(void)
{
  // GINTSTS will be cleared with DAINT == 0
  // DAINT for a given EP clears when DOEPINTx is cleared.
  // DOEPINT will be cleared when DAINT's out bits are cleared.

  for(uint8_t n = 0; n < EP_COUNT; n++)
  {
    xfer_ctl_t *xfer = XFER_CTL_BASE(n, TUSB_DIR_OUT);

    if(n == 0)
    {
      if(USB->DAINT & (1 << (_USB_DAINT_OUTEPINT0_SHIFT + n)))
      {
        // SETUP packet Setup Phase done.
        if((USB->DOEP0INT & USB_DOEP0INT_SETUP))
        {
          USB->DOEP0INT = USB_DOEP0INT_STUPPKTRCVD | USB_DOEP0INT_SETUP; // clear
          dcd_event_setup_received(0, (uint8_t *)&_setup_packet[0], true);
        }

        // OUT XFER complete (single packet).q
        if(USB->DOEP0INT & USB_DOEP0INT_XFERCOMPL)
        {
          USB->DOEP0INT = USB_DOEP0INT_XFERCOMPL;

          // Transfer complete if short packet or total len is transferred
          if(xfer->short_packet || (xfer->queued_len == xfer->total_len))
          {
            xfer->short_packet = false;
            dcd_event_xfer_complete(0, n, xfer->queued_len, XFER_RESULT_SUCCESS, true);
          }
          else
          {
            // Schedule another packet to be received.
            USB->DOEP0TSIZ |= (1 << _USB_DOEP0TSIZ_PKTCNT_SHIFT) | ((xfer->max_size & _USB_DOEP0TSIZ_XFERSIZE_MASK) << _USB_DOEP0TSIZ_XFERSIZE_SHIFT);
            USB->DOEP0CTL |= USB_DOEP0CTL_EPENA | USB_DOEP0CTL_CNAK;
          }
        }
      }
    }
    else
    {
      if(USB->DAINT & (1 << (_USB_DAINT_OUTEPINT0_SHIFT + n)))
      {
        // SETUP packet Setup Phase done.
        if((USB->DOEP[n - 1].INT & USB_DOEP_INT_SETUP))
        {
          USB->DOEP[n - 1].INT = USB_DOEP_INT_STUPPKTRCVD | USB_DOEP_INT_SETUP; // clear
          dcd_event_setup_received(0, (uint8_t *)&_setup_packet[0], true);
        }

        // OUT XFER complete (single packet).q
        if(USB->DOEP[n - 1].INT & USB_DOEP_INT_XFERCOMPL)
        {
          USB->DOEP[n - 1].INT = USB_DOEP_INT_XFERCOMPL;

          // Transfer complete if short packet or total len is transferred
          if(xfer->short_packet || (xfer->queued_len == xfer->total_len))
          {
            xfer->short_packet = false;
            dcd_event_xfer_complete(0, n, xfer->queued_len, XFER_RESULT_SUCCESS, true);
          }
          else
          {
            // Schedule another packet to be received.
            USB->DOEP[n - 1].TSIZ |= (1 << _USB_DOEP_TSIZ_PKTCNT_SHIFT) | ((xfer->max_size & _USB_DOEP_TSIZ_XFERSIZE_MASK) << _USB_DOEP_TSIZ_XFERSIZE_SHIFT);
            USB->DOEP[n - 1].CTL |= USB_DOEP_CTL_EPENA | USB_DOEP_CTL_CNAK;
          }
        }
      }
    }
  }
}

static void handle_epin_ints(void)
{

  for(uint32_t n = 0; n < EP_COUNT; n++)
  {
    xfer_ctl_t *xfer = &xfer_status[n][TUSB_DIR_IN];

    if(n == 0)
    {
      if(USB->DAINT & (1 << n))
      {
        /* IN XFER complete (entire xfer). */
        if(USB->DIEP0INT & USB_DIEP0INT_XFERCOMPL)
        {
          USB->DIEP0INT = USB_DIEP0INT_XFERCOMPL;
          dcd_event_xfer_complete(0, n | TUSB_DIR_IN_MASK, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }

        /* XFER FIFO empty */
        if(USB->DIEP0INT & USB_DIEP0INT_TXFEMP)
        {
          USB->DIEP0INT = USB_DIEP0INT_TXFEMP;
          transmit_packet(xfer, n);

          /* Turn off TXFE if all bytes are written. */
          if(xfer->queued_len == xfer->total_len)
          {
            USB->DIEPEMPMSK  &= ~(1 << n);
          }
        }

        /* XFER Timeout */
        if(USB->DIEP0INT & USB_DIEP0INT_TIMEOUT)
        {
          /* Clear interrupt or enpoint will hang. */
          USB->DIEP0INT = USB_DIEP0INT_TIMEOUT;
        }
      }
    }
    else
    {
      if(USB->DAINT & (1 << n))
      {
        /* IN XFER complete (entire xfer). */
        if(USB->DIEP[n - 1].INT & USB_DIEP_INT_XFERCOMPL)
        {
          USB->DIEP[n - 1].INT = USB_DIEP_INT_XFERCOMPL;
          dcd_event_xfer_complete(0, n | TUSB_DIR_IN_MASK, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }

        /* XFER FIFO empty */
        if(USB->DIEP[n - 1].INT & USB_DIEP_INT_TXFEMP)
        {
          USB->DIEP[n - 1].INT = USB_DIEP_INT_TXFEMP;
          transmit_packet(xfer, n);

          /* Turn off TXFE if all bytes are written. */
          if(xfer->queued_len == xfer->total_len)
          {
            USB->DIEPEMPMSK  &= ~(1 << n);
          }
        }

        /* XFER Timeout */
        if(USB->DIEP[n - 1].INT & USB_DIEP_INT_TIMEOUT)
        {
          /* Clear interrupt or enpoint will hang. */
          USB->DIEP[n - 1].INT = USB_DIEP_INT_TIMEOUT;
        }
      }
    }
  }
}

void dcd_int_handler(uint8_t rhport)
{
  (void) rhport;

  const uint32_t int_status = USB->GINTSTS;

  /* USB Reset */
  if(int_status & USB_GINTSTS_USBRST)
  {
    /* start of reset */
    USB->GINTSTS = USB_GINTSTS_USBRST;
    /* FIFOs will be reassigned when the endpoints are reopen */
    _allocated_fifos = 1;
    bus_reset();
  }

  /* Reset detected Interrupt */
  if(int_status & USB_GINTSTS_RESETDET)
  {
    USB->GINTSTS = USB_GINTSTS_RESETDET;
    bus_reset();
  }

  /* Enumeration Done */
  if(int_status & USB_GINTSTS_ENUMDONE)
  {
    /* This interrupt is considered the end of reset. */
    USB->GINTSTS = USB_GINTSTS_ENUMDONE;
    enum_done_processing();
    dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
  }

  /* OTG Interrupt */
  if(int_status & USB_GINTSTS_OTGINT)
  {
    /* OTG INT bit is read-only */

    uint32_t const otg_int = USB->GOTGINT;

    if(otg_int & USB_GOTGINT_SESENDDET)
    {
      dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, true);
    }

    USB->GOTGINT = otg_int;
  }

  #if USE_SOF
  if(int_status & USB_GINTSTS_SOF)
  {
    USB->GINTSTS = USB_GINTSTS_SOF;
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }
  #endif

  /* RxFIFO Non-Empty */
  if(int_status & USB_GINTSTS_RXFLVL)
  {
    /* RXFLVL bit is read-only */

    /* Mask out RXFLVL while reading data from FIFO */
    USB->GINTMSK &= ~USB_GINTMSK_RXFLVLMSK;
    read_rx_fifo();
    USB->GINTMSK |= USB_GINTMSK_RXFLVLMSK;
  }

  /* OUT Endpoints Interrupt */
  if(int_status & USB_GINTMSK_OEPINTMSK)
  {
    /* OEPINT is read-only */
    handle_epout_ints();
  }

  /* IN Endpoints Interrupt */
  if(int_status & USB_GINTMSK_IEPINTMSK)
  {
    /* IEPINT bit read-only */
    handle_epin_ints();
  }

  /* unhandled */
  USB->GINTSTS |= USB_GINTSTS_CURMOD      |
                  USB_GINTSTS_MODEMIS     |
                  USB_GINTSTS_OTGINT      |
                  USB_GINTSTS_NPTXFEMP    |
                  USB_GINTSTS_GINNAKEFF   |
                  USB_GINTSTS_GOUTNAKEFF  |
                  USB_GINTSTS_ERLYSUSP    |
                  USB_GINTSTS_USBSUSP     |
                  USB_GINTSTS_ISOOUTDROP  |
                  USB_GINTSTS_EOPF        |
                  USB_GINTSTS_EPMIS       |
                  USB_GINTSTS_INCOMPISOIN |
                  USB_GINTSTS_INCOMPLP    |
                  USB_GINTSTS_FETSUSP     |
                  USB_GINTSTS_PTXFEMP;
}

#endif
