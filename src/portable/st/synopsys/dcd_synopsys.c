/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft, 2019 William D. Jones for Adafruit Industries
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#if defined (STM32L475xx) || defined (STM32L476xx) ||                          \
    defined (STM32L485xx) || defined (STM32L486xx) || defined (STM32L496xx) || \
    defined (STM32L4R5xx) || defined (STM32L4R7xx) || defined (STM32L4R9xx) || \
    defined (STM32L4S5xx) || defined (STM32L4S7xx) || defined (STM32L4S9xx)
#define STM32L4_SYNOPSYS
#endif

#if TUSB_OPT_DEVICE_ENABLED && \
    ( CFG_TUSB_MCU == OPT_MCU_STM32F2 || \
      CFG_TUSB_MCU == OPT_MCU_STM32F4 || \
      CFG_TUSB_MCU == OPT_MCU_STM32F7 || \
      CFG_TUSB_MCU == OPT_MCU_STM32H7 || \
      (CFG_TUSB_MCU == OPT_MCU_STM32L4 && defined(STM32L4_SYNOPSYS)) \
    )

// TODO Support OTG_HS
// EP_MAX       : Max number of bi-directional endpoints including EP0
// EP_FIFO_SIZE : Size of dedicated USB SRAM
#if CFG_TUSB_MCU == OPT_MCU_STM32F2
  #include "stm32f2xx.h"
  #define EP_MAX          USB_OTG_FS_MAX_IN_ENDPOINTS
  #define EP_FIFO_SIZE    USB_OTG_FS_TOTAL_FIFO_SIZE
#elif CFG_TUSB_MCU == OPT_MCU_STM32F4
  #include "stm32f4xx.h"
  #define EP_MAX          USB_OTG_FS_MAX_IN_ENDPOINTS
  #define EP_FIFO_SIZE    USB_OTG_FS_TOTAL_FIFO_SIZE
#elif CFG_TUSB_MCU == OPT_MCU_STM32H7
  #include "stm32h7xx.h"
  #define EP_MAX          9
  #define EP_FIFO_SIZE    4096
  // TODO The official name of the USB FS peripheral on H7 is "USB2_OTG_FS".
#elif CFG_TUSB_MCU == OPT_MCU_STM32F7
  #include "stm32f7xx.h"
  #define EP_MAX          6
  #define EP_FIFO_SIZE    1280
#elif CFG_TUSB_MCU == OPT_MCU_STM32L4
  #include "stm32l4xx.h"
  #define EP_MAX          6
  #define EP_FIFO_SIZE    1280
#else
  #error "Unsupported MCUs"
#endif

#include "device/dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
#define DEVICE_BASE     (USB_OTG_DeviceTypeDef *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE)
#define OUT_EP_BASE     (USB_OTG_OUTEndpointTypeDef *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE)
#define IN_EP_BASE      (USB_OTG_INEndpointTypeDef *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE)
#define FIFO_BASE(_x)   ((volatile uint32_t *) (USB_OTG_FS_PERIPH_BASE + USB_OTG_FIFO_BASE + (_x) * USB_OTG_FIFO_SIZE))

static TU_ATTR_ALIGNED(4) uint32_t _setup_packet[6];
static uint8_t _setup_offs; // We store up to 3 setup packets.

typedef struct {
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_size;
  bool short_packet;
} xfer_ctl_t;

typedef volatile uint32_t * usb_fifo_t;

xfer_ctl_t xfer_status[EP_MAX][2];
#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]


// Setup the control endpoint 0.
static void bus_reset(void) {
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE;

  for(uint8_t n = 0; n < EP_MAX; n++) {
    out_ep[n].DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
  }

  dev->DAINTMSK |= (1 << USB_OTG_DAINTMSK_OEPM_Pos) | (1 << USB_OTG_DAINTMSK_IEPM_Pos);
  dev->DOEPMSK |= USB_OTG_DOEPMSK_STUPM | USB_OTG_DOEPMSK_XFRCM;
  dev->DIEPMSK |= USB_OTG_DIEPMSK_TOM | USB_OTG_DIEPMSK_XFRCM;

  // "USB Data FIFOs" section in reference manual
  // Peripheral FIFO architecture
  //
  // --------------- 320 or 1024 ( 1280 or 4096 bytes )
  // | IN FIFO MAX |
  // ---------------
  // |    ...      |
  // --------------- y + x + 16 + GRXFSIZ
  // | IN FIFO 2   |
  // --------------- x + 16 + GRXFSIZ
  // | IN FIFO 1   |
  // --------------- 16 + GRXFSIZ
  // | IN FIFO 0   |
  // --------------- GRXFSIZ
  // | OUT FIFO    |
  // | ( Shared )  |
  // --------------- 0
  //
  // According to "FIFO RAM allocation" section in RM, FIFO RAM are allocated as follows (each word 32-bits):
  // - Each EP IN needs at least max packet size, 16 words is sufficient for EP0 IN
  //
  // - All EP OUT shared a unique OUT FIFO which uses
  //   * 10 locations in hardware for setup packets + setup control words (up to 3 setup packets).
  //   * 2 locations for OUT endpoint control words.
  //   * 16 for largest packet size of 64 bytes. ( TODO Highspeed is 512 bytes)
  //   * 1 location for global NAK (not required/used here).
  //   * It is recommended to allocate 2 times the largest packet size, therefore
  //   Recommended value = 10 + 1 + 2 x (16+2) = 47 --> Let's make it 52
  USB_OTG_FS->GRXFSIZ = 52;

  // Control IN uses FIFO 0 with 64 bytes ( 16 32-bit word )
  USB_OTG_FS->DIEPTXF0_HNPTXFSIZ = (16 << USB_OTG_TX0FD_Pos) | (USB_OTG_FS->GRXFSIZ & 0x0000ffffUL);

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
    xfer_status[0][TUSB_DIR_OUT].max_size = 64;
    xfer_status[0][TUSB_DIR_IN].max_size = 64;
  } else {
    // 8 bytes
    in_ep[0].DIEPCTL |= (0x03 << USB_OTG_DIEPCTL_MPSIZ_Pos);
    xfer_status[0][TUSB_DIR_OUT].max_size = 8;
    xfer_status[0][TUSB_DIR_IN].max_size = 8;
  }
}


/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
void dcd_init (uint8_t rhport)
{
  (void) rhport;

  // Programming model begins in the last section of the chapter on the USB
  // peripheral in each Reference Manual.
  USB_OTG_FS->GAHBCFG |= USB_OTG_GAHBCFG_TXFELVL | USB_OTG_GAHBCFG_GINT;

  // No HNP/SRP (no OTG support), program timeout later, turnaround
  // programmed for 32+ MHz.
  // TODO: PHYSEL is read-only on some cores (STM32F407). Worth gating?
  USB_OTG_FS->GUSBCFG |= (0x06 << USB_OTG_GUSBCFG_TRDT_Pos) | USB_OTG_GUSBCFG_PHYSEL;

  // Clear all used interrupts
  USB_OTG_FS->GINTSTS |= USB_OTG_GINTSTS_OTGINT | USB_OTG_GINTSTS_MMIS | \
    USB_OTG_GINTSTS_USBRST | USB_OTG_GINTSTS_ENUMDNE | \
    USB_OTG_GINTSTS_ESUSP | USB_OTG_GINTSTS_USBSUSP | USB_OTG_GINTSTS_SOF;

  // Required as part of core initialization. Disable OTGINT as we don't use
  // it right now. TODO: How should mode mismatch be handled? It will cause
  // the core to stop working/require reset.
  USB_OTG_FS->GINTMSK |= /* USB_OTG_GINTMSK_OTGINT | */ USB_OTG_GINTMSK_MMISM;

  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;

  // If USB host misbehaves during status portion of control xfer
  // (non zero-length packet), send STALL back and discard. Full speed.
  dev->DCFG |=  USB_OTG_DCFG_NZLSOHSK | (3 << USB_OTG_DCFG_DSPD_Pos);

  USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM | \
    USB_OTG_GINTMSK_SOFM | USB_OTG_GINTMSK_RXFLVLM /* SB_OTG_GINTMSK_ESUSPM | \
    USB_OTG_GINTMSK_USBSUSPM */;

  // Enable VBUS hardware sensing, enable pullup, enable peripheral.
#ifdef USB_OTG_GCCFG_VBDEN
  USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBDEN | USB_OTG_GCCFG_PWRDWN;
#else
  USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBUSBSEN | USB_OTG_GCCFG_PWRDWN;
#endif

  // Soft Connect -> Enable pullup on D+/D-.
  // This step does not appear to be specified in the programmer's model.
  dev->DCTL &= ~USB_OTG_DCTL_SDIS;
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

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;

  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;

  dev->DCFG |= (dev_addr << USB_OTG_DCFG_DAD_Pos) & USB_OTG_DCFG_DAD_Msk;

  // Response with status after changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  // Nothing to do
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE;
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE;

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);

  TU_ASSERT(desc_edpt->wMaxPacketSize.size <= 64);
  TU_ASSERT(epnum < EP_MAX);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->max_size = desc_edpt->wMaxPacketSize.size;

  if(dir == TUSB_DIR_OUT)
  {
    out_ep[epnum].DOEPCTL |= (1 << USB_OTG_DOEPCTL_USBAEP_Pos) | \
      desc_edpt->bmAttributes.xfer << USB_OTG_DOEPCTL_EPTYP_Pos | \
      desc_edpt->wMaxPacketSize.size << USB_OTG_DOEPCTL_MPSIZ_Pos;
    dev->DAINTMSK |= (1 << (USB_OTG_DAINTMSK_OEPM_Pos + epnum));
  }
  else
  {
    // "USB Data FIFOs" section in reference manual
    // Peripheral FIFO architecture
    //
    // --------------- 320 or 1024 ( 1280 or 4096 bytes )
    // | IN FIFO MAX |
    // ---------------
    // |    ...      |
    // --------------- y + x + 16 + GRXFSIZ
    // | IN FIFO 2   |
    // --------------- x + 16 + GRXFSIZ
    // | IN FIFO 1   |
    // --------------- 16 + GRXFSIZ
    // | IN FIFO 0   |
    // --------------- GRXFSIZ
    // | OUT FIFO    |
    // | ( Shared )  |
    // --------------- 0
    //
    // Since OUT FIFO = GRXFSIZ, FIFO 0 = 16, for simplicity, we equally allocated for the rest of endpoints
    // - Size  : (FIFO_SIZE/4 - GRXFSIZ - 16) / (EP_MAX-1)
    // - Offset: GRXFSIZ + 16 + Size*(epnum-1)
    // - IN EP 1 gets FIFO 1, IN EP "n" gets FIFO "n".

    in_ep[epnum].DIEPCTL |= (1 << USB_OTG_DIEPCTL_USBAEP_Pos) | \
      epnum << USB_OTG_DIEPCTL_TXFNUM_Pos | \
      desc_edpt->bmAttributes.xfer << USB_OTG_DIEPCTL_EPTYP_Pos | \
      (desc_edpt->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS ? USB_OTG_DOEPCTL_SD0PID_SEVNFRM : 0) | \
      desc_edpt->wMaxPacketSize.size << USB_OTG_DIEPCTL_MPSIZ_Pos;
    dev->DAINTMSK |= (1 << (USB_OTG_DAINTMSK_IEPM_Pos + epnum));

    // Both TXFD and TXSA are in unit of 32-bit words.
    // IN FIFO 0 was configured during enumeration, hence the "+ 16".
    uint16_t const allocated_size = (USB_OTG_FS->GRXFSIZ & 0x0000ffff) + 16;
    uint16_t const fifo_size = (EP_FIFO_SIZE/4 - allocated_size) / (EP_MAX-1);
    uint32_t const fifo_offset = allocated_size + fifo_size*(epnum-1);

    // DIEPTXF starts at FIFO #1.
    USB_OTG_FS->DIEPTXF[epnum - 1] = (fifo_size << USB_OTG_DIEPTXF_INEPTXFD_Pos) | fifo_offset;
  }

  return true;
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
  xfer->short_packet = false;

  uint16_t num_packets = (total_bytes / xfer->max_size);
  uint8_t short_packet_size = total_bytes % xfer->max_size;

  // Zero-size packet is special case.
  if(short_packet_size > 0 || (total_bytes == 0)) {
    num_packets++;
  }

  // IN and OUT endpoint xfers are interrupt-driven, we just schedule them
  // here.
  if(dir == TUSB_DIR_IN) {
    // A full IN transfer (multiple packets, possibly) triggers XFRC.
    in_ep[epnum].DIEPTSIZ = (num_packets << USB_OTG_DIEPTSIZ_PKTCNT_Pos) | \
        ((total_bytes & USB_OTG_DIEPTSIZ_XFRSIZ_Msk) << USB_OTG_DIEPTSIZ_XFRSIZ_Pos);
    in_ep[epnum].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;
    dev->DIEPEMPMSK |= (1 << epnum);
  } else {
    // Each complete packet for OUT xfers triggers XFRC.
    out_ep[epnum].DOEPTSIZ |= (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | \
        ((xfer->max_size & USB_OTG_DOEPTSIZ_XFRSIZ_Msk) << USB_OTG_DOEPTSIZ_XFRSIZ_Pos);
    out_ep[epnum].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
  }

  return true;
}

// TODO: The logic for STALLing and disabling an endpoint is very similar
// (send STALL versus NAK handshakes back). Refactor into resuable function.
void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE;
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if(dir == TUSB_DIR_IN) {
    // Only disable currently enabled non-control endpoint
    if ( (epnum == 0) || !(in_ep[epnum].DIEPCTL & USB_OTG_DIEPCTL_EPENA) ){
      in_ep[epnum].DIEPCTL |= (USB_OTG_DIEPCTL_SNAK | USB_OTG_DIEPCTL_STALL);
    } else {
      // Stop transmitting packets and NAK IN xfers.
      in_ep[epnum].DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
      while((in_ep[epnum].DIEPINT & USB_OTG_DIEPINT_INEPNE) == 0);

      // Disable the endpoint.
      in_ep[epnum].DIEPCTL |= (USB_OTG_DIEPCTL_STALL | USB_OTG_DIEPCTL_EPDIS);
      while((in_ep[epnum].DIEPINT & USB_OTG_DIEPINT_EPDISD_Msk) == 0);
      in_ep[epnum].DIEPINT = USB_OTG_DIEPINT_EPDISD;
    }

    // Flush the FIFO, and wait until we have confirmed it cleared.
    USB_OTG_FS->GRSTCTL |= ((epnum - 1) << USB_OTG_GRSTCTL_TXFNUM_Pos);
    USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_TXFFLSH;
    while((USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH_Msk) != 0);
  } else {
    // Only disable currently enabled non-control endpoint
    if ( (epnum == 0) || !(out_ep[epnum].DOEPCTL & USB_OTG_DOEPCTL_EPENA) ){
      out_ep[epnum].DOEPCTL |= USB_OTG_DOEPCTL_STALL;
    } else {
      // Asserting GONAK is required to STALL an OUT endpoint.
      // Simpler to use polling here, we don't use the "B"OUTNAKEFF interrupt
      // anyway, and it can't be cleared by user code. If this while loop never
      // finishes, we have bigger problems than just the stack.
      dev->DCTL |= USB_OTG_DCTL_SGONAK;
      while((USB_OTG_FS->GINTSTS & USB_OTG_GINTSTS_BOUTNAKEFF_Msk) == 0);

      // Ditto here- disable the endpoint.
      out_ep[epnum].DOEPCTL |= (USB_OTG_DOEPCTL_STALL | USB_OTG_DOEPCTL_EPDIS);
      while((out_ep[epnum].DOEPINT & USB_OTG_DOEPINT_EPDISD_Msk) == 0);
      out_ep[epnum].DOEPINT = USB_OTG_DOEPINT_EPDISD;

      // Allow other OUT endpoints to keep receiving.
      dev->DCTL |= USB_OTG_DCTL_CGONAK;
    }
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE;
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if(dir == TUSB_DIR_IN) {
    in_ep[epnum].DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;

    uint8_t eptype = (in_ep[epnum].DIEPCTL & USB_OTG_DIEPCTL_EPTYP_Msk) >> \
      USB_OTG_DIEPCTL_EPTYP_Pos;
    // Required by USB spec to reset DATA toggle bit to DATA0 on interrupt
    // and bulk endpoints.
    if(eptype == 2 || eptype == 3) {
      in_ep[epnum].DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
    }
  } else {
    out_ep[epnum].DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;

    uint8_t eptype = (out_ep[epnum].DOEPCTL & USB_OTG_DOEPCTL_EPTYP_Msk) >> \
      USB_OTG_DOEPCTL_EPTYP_Pos;
    // Required by USB spec to reset DATA toggle bit to DATA0 on interrupt
    // and bulk endpoints.
    if(eptype == 2 || eptype == 3) {
      out_ep[epnum].DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
    }
  }
}

/*------------------------------------------------------------------*/

// TODO: Split into "receive on endpoint 0" and "receive generic"; endpoint 0's
// DOEPTSIZ register is smaller than the others, and so is insufficient for
// determining how much of an OUT transfer is actually remaining.
static void receive_packet(xfer_ctl_t * xfer, /* USB_OTG_OUTEndpointTypeDef * out_ep, */ uint16_t xfer_size) {
  usb_fifo_t rx_fifo = FIFO_BASE(0);

  // See above TODO
  // uint16_t remaining = (out_ep->DOEPTSIZ & USB_OTG_DOEPTSIZ_XFRSIZ_Msk) >> USB_OTG_DOEPTSIZ_XFRSIZ_Pos;
  // xfer->queued_len = xfer->total_len - remaining;

  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t to_recv_size;

  if(remaining <= xfer->max_size) {
    // Avoid buffer overflow.
    to_recv_size = (xfer_size > remaining) ? remaining : xfer_size;
  } else {
    // Room for full packet, choose recv_size based on what the microcontroller
    // claims.
    to_recv_size = (xfer_size > xfer->max_size) ? xfer->max_size : xfer_size;
  }

  uint8_t to_recv_rem = to_recv_size % 4;
  uint16_t to_recv_size_aligned = to_recv_size - to_recv_rem;

  // Do not assume xfer buffer is aligned.
  uint8_t * base = (xfer->buffer + xfer->queued_len);

  // This for loop always runs at least once- skip if less than 4 bytes
  // to collect.
  if(to_recv_size >= 4) {
    for(uint16_t i = 0; i < to_recv_size_aligned; i += 4) {
      uint32_t tmp = (* rx_fifo);
      base[i] = tmp & 0x000000FF;
      base[i + 1] = (tmp & 0x0000FF00) >> 8;
      base[i + 2] = (tmp & 0x00FF0000) >> 16;
      base[i + 3] = (tmp & 0xFF000000) >> 24;
    }
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

  // Per USB spec, a short OUT packet (including length 0) is always
  // indicative of the end of a transfer (at least for ctl, bulk, int).
  xfer->short_packet = (xfer_size < xfer->max_size);
}

static void transmit_packet(xfer_ctl_t * xfer, USB_OTG_INEndpointTypeDef * in_ep, uint8_t fifo_num) {
  usb_fifo_t tx_fifo = FIFO_BASE(fifo_num);

  uint16_t remaining = (in_ep->DIEPTSIZ & USB_OTG_DIEPTSIZ_XFRSIZ_Msk) >> USB_OTG_DIEPTSIZ_XFRSIZ_Pos;
  xfer->queued_len = xfer->total_len - remaining;

  uint16_t to_xfer_size = (remaining > xfer->max_size) ? xfer->max_size : remaining;
  uint8_t to_xfer_rem = to_xfer_size % 4;
  uint16_t to_xfer_size_aligned = to_xfer_size - to_xfer_rem;

  // Buffer might not be aligned to 32b, so we need to force alignment
  // by copying to a temp var.
  uint8_t * base = (xfer->buffer + xfer->queued_len);

  // This for loop always runs at least once- skip if less than 4 bytes
  // to send off.
  if(to_xfer_size >= 4) {
    for(uint16_t i = 0; i < to_xfer_size_aligned; i += 4) {
      uint32_t tmp = base[i] | (base[i + 1] << 8) | \
        (base[i + 2] << 16) | (base[i + 3] << 24);
      (* tx_fifo) = tmp;
    }
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

static void read_rx_fifo(USB_OTG_OUTEndpointTypeDef * out_ep) {
  usb_fifo_t rx_fifo = FIFO_BASE(0);

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
    default: // Invalid
      TU_BREAKPOINT();
      break;
  }
}

static void handle_epout_ints(USB_OTG_DeviceTypeDef * dev, USB_OTG_OUTEndpointTypeDef * out_ep) {
  // DAINT for a given EP clears when DOEPINTx is cleared.
  // OEPINT will be cleared when DAINT's out bits are cleared.
  for(uint8_t n = 0; n < EP_MAX; n++) {
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
        // on a packet-basis. The core can internally handle multiple OUT
        // packets; it would be more efficient to only trigger XFRC on a
        // completed transfer for non-0 endpoints.

        // Transfer complete if short packet or total len is transferred
        if(xfer->short_packet || (xfer->queued_len == xfer->total_len)) {
          xfer->short_packet = false;
          dcd_event_xfer_complete(0, n, xfer->queued_len, XFER_RESULT_SUCCESS, true);
        } else {
          // Schedule another packet to be received.
          out_ep[n].DOEPTSIZ |= (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | \
              ((xfer->max_size & USB_OTG_DOEPTSIZ_XFRSIZ_Msk) << USB_OTG_DOEPTSIZ_XFRSIZ_Pos);
          out_ep[n].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
        }
      }
    }
  }
}

static void handle_epin_ints(USB_OTG_DeviceTypeDef * dev, USB_OTG_INEndpointTypeDef * in_ep) {
  // DAINT for a given EP clears when DIEPINTx is cleared.
  // IEPINT will be cleared when DAINT's out bits are cleared.
  for(uint8_t n = 0; n < EP_MAX; n++) {
    xfer_ctl_t * xfer = XFER_CTL_BASE(n, TUSB_DIR_IN);

    if(dev->DAINT & (1 << (USB_OTG_DAINT_IEPINT_Pos + n))) {
      // IN XFER complete (entire xfer).
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

void OTG_FS_IRQHandler(void) {
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE;
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE;

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

  if(int_status & USB_OTG_GINTSTS_SOF) {
    USB_OTG_FS->GINTSTS = USB_OTG_GINTSTS_SOF;
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }

  if(int_status & USB_OTG_GINTSTS_RXFLVL) {
    read_rx_fifo(out_ep);
  }

  // OUT endpoint interrupt handling.
  if(int_status & USB_OTG_GINTSTS_OEPINT) {
    handle_epout_ints(dev, out_ep);
  }

  // IN endpoint interrupt handling.
  if(int_status & USB_OTG_GINTSTS_IEPINT) {
    handle_epin_ints(dev, in_ep);
  }
}

#endif
