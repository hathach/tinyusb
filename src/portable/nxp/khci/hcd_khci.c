/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji Kitayama
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

#if TUSB_OPT_HOST_ENABLED && ( \
      ( CFG_TUSB_MCU == OPT_MCU_MKL25ZXX ) || ( CFG_TUSB_MCU == OPT_MCU_K32L2BXX ) \
    )

#include "fsl_device_registers.h"
#define KHCI        USB0

#include "host/hcd.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

enum {
  TOK_PID_OUT   = 0x1u,
  TOK_PID_IN    = 0x9u,
  TOK_PID_SETUP = 0xDu,
  TOK_PID_DATA0 = 0x3u,
  TOK_PID_DATA1 = 0xbu,
  TOK_PID_ACK   = 0x2u,
  TOK_PID_STALL = 0xeu,
  TOK_PID_NAK   = 0xau,
  TOK_PID_BUSTO = 0x0u,
  TOK_PID_ERR   = 0xfu,
};

typedef struct TU_ATTR_PACKED
{
  union {
    uint32_t head;
    struct {
      union {
        struct {
               uint16_t           :  2;
          __IO uint16_t tok_pid   :  4;
               uint16_t data      :  1;
          __IO uint16_t own       :  1;
               uint16_t           :  8;
        };
        struct {
               uint16_t           :  2;
               uint16_t bdt_stall :  1;
               uint16_t dts       :  1;
               uint16_t ninc      :  1;
               uint16_t keep      :  1;
               uint16_t           : 10;
        };
      };
      __IO uint16_t bc : 10;
           uint16_t    :  6;
    };
  };
  uint8_t *addr;
}buffer_descriptor_t;

TU_VERIFY_STATIC( sizeof(buffer_descriptor_t) == 8, "size is not correct" );

typedef struct TU_ATTR_PACKED
{
  union {
    uint32_t state;
    struct {
      uint32_t max_packet_size :11;
      uint32_t                 : 5;
      uint32_t odd             : 1;
      uint32_t                 :15;
    };
  };
  uint16_t length;
  uint16_t remaining;
} endpoint_state_t;

TU_VERIFY_STATIC( sizeof(endpoint_state_t) == 8, "size is not correct" );

typedef struct TU_ATTR_PACKED
{
  uint8_t  dev_addr;
  uint8_t  ep_addr;
  uint16_t max_packet_size;
  union {
    uint8_t flags;
    struct {
      uint8_t data : 1;
      uint8_t xfer : 2;
      uint8_t      : 0;
    };
  };
} pipe_state_t;


typedef struct
{
  union {
    /* [#EP][OUT,IN][EVEN,ODD] */
    buffer_descriptor_t bdt[16][2][2];
    uint16_t            bda[512];
  };
  endpoint_state_t endpoint[2];
  pipe_state_t pipe[HCD_MAX_XFER];
} hcd_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
// BDT(Buffer Descriptor Table) must be 256-byte aligned
CFG_TUSB_MEM_SECTION TU_ATTR_ALIGNED(512) static hcd_data_t _hcd;

TU_VERIFY_STATIC( sizeof(_hcd.bdt) == 512, "size is not correct" );

static pipe_state_t *find_pipe(uint8_t dev_addr, uint8_t ep_addr)
{
  /* Find the target pipe */
  pipe_state_t *p   = _hcd.pipe;
  pipe_state_t *end = p + HCD_MAX_XFER;
  for (;p != end; ++p) {
    if ((p->dev_addr == dev_addr) && (p->ep_addr != ep_addr))
      return p;
  }
  return NULL;
}

static int prepare_packets(uint8_t rhport, uint_fast8_t dir_in, uint8_t* buffer, uint_fast16_t total_bytes)
{
  (void)rhport;
  const unsigned dir_tx   = dir_in ? 0 : 1;
  endpoint_state_t    *ep = &_hcd.endpoint[dir_tx];
  buffer_descriptor_t *bd = &_hcd.bdt[0][dir_tx][ep->odd];
  TU_ASSERT(0 == bd->own, 0);

  ep->length    = total_bytes;
  ep->remaining = total_bytes;

  int num_pkts = 0; /* The number of prepared packets */
  const unsigned mps = ep->max_packet_size;
  if (total_bytes > mps) {
    buffer_descriptor_t *next = ep->odd ? bd - 1: bd + 1;
    /* When total_bytes is greater than the max packet size,
     * it prepares to the next transfer to avoid NAK in advance. */
    next->bc   = total_bytes >= 2 * mps ? mps: total_bytes - mps;
    next->addr = buffer + mps;
    next->own  = 1;
    ++num_pkts;
  }
  bd->bc   = total_bytes >= mps ? mps: total_bytes;
  bd->addr = buffer;
  __DSB();
  bd->own  = 1; /* This bit must be set last */
  ++num_pkts;
  return num_pkts;
}

static void process_tokdne(uint8_t rhport)
{
  (void)rhport;
  const unsigned s = KHCI->STAT;
  KHCI->ISTAT = USB_ISTAT_TOKDNE_MASK; /* fetch the next token if received */

  uint8_t const epnum  = (s >> USB_STAT_ENDP_SHIFT);
  TU_ASSERT(0 == epnum,);
  uint8_t const dir_in = (s & USB_STAT_TX_MASK) ? TUSB_DIR_OUT: TUSB_DIR_IN;
  unsigned const odd   = (s & USB_STAT_ODD_MASK) ? 1 : 0;

  buffer_descriptor_t *bd = (buffer_descriptor_t *)&_hcd.bda[s];
  endpoint_state_t    *ep = &_hcd.endpoint[s >> 3];

  /* fetch status before discarded by the next steps */
  const unsigned pid = bd->tok_pid;

  /* reset values for a next transfer */
  bd->bdt_stall = 0;
  bd->dts       = 1;
  bd->ninc      = 0;
  bd->keep      = 0;
  /* Update the odd variable to prepare for the next transfer */
  ep->odd       = odd ^ 1;

  const unsigned bc = bd->bc;
  const unsigned remaining = ep->remaining - bc;
  if ((TOK_PID_DATA0 == pid) || (TOK_PID_DATA1 == pid) || (TOK_PID_ACK == pid)) {
    /* Go on the next packet transfer */
    if (remaining && bc == ep->max_packet_size) {
      ep->remaining = remaining;
      const int next_remaining = remaining - ep->max_packet_size;
      if (next_remaining > 0) {
        /* Prepare to the after next transfer */
        bd->addr += ep->max_packet_size * 2;
        bd->bc    = next_remaining > ep->max_packet_size ? ep->max_packet_size: next_remaining;
        __DSB();
        bd->own   = 1; /* This bit must be set last */
        while (KHCI->CTL & USB_CTL_TXSUSPENDTOKENBUSY_MASK) ;
        KHCI->TOKEN = KHCI->TOKEN; /* Queue the same token as the last */
      }
      return;
    }
  }
  const unsigned length = ep->length;
  xfer_result_t result;
  switch (pid) {
    default:
      result = XFER_RESULT_SUCCESS;
      break;
    case TOK_PID_STALL:
      result = XFER_RESULT_STALLED;
      break;
    case TOK_PID_NAK:
    case TOK_PID_ERR:
    case TOK_PID_BUSTO:
      result = XFER_RESULT_FAILED;
      break;
  }
  hcd_event_xfer_complete(KHCI->ADDR & USB_ADDR_ADDR_MASK,
                          tu_edpt_addr(KHCI->TOKEN & USB_TOKEN_TOKENENDPT_MASK, dir_in),
                          length - remaining, result, true);
}

static void process_attach(uint8_t rhport)
{
  unsigned ctl = KHCI->CTL;
  if (!(ctl & USB_CTL_JSTATE_MASK)) {
    /* The attached device is a low speed device. */
    KHCI->ADDR = USB_ADDR_LSEN_MASK;
    KHCI->ENDPOINT[0].ENDPT = USB_ENDPT_HOSTWOHUB_MASK;
  }
  hcd_event_device_attach(rhport, true);
}

/*------------------------------------------------------------------*/
/* Host API
 *------------------------------------------------------------------*/
bool hcd_init(uint8_t rhport)
{
  (void)rhport;

  KHCI->USBTRC0 |= USB_USBTRC0_USBRESET_MASK;
  while (KHCI->USBTRC0 & USB_USBTRC0_USBRESET_MASK);

  tu_memclr(&_hcd, sizeof(_hcd));
  KHCI->USBTRC0 |= TU_BIT(6); /* software must set this bit to 1 */
  KHCI->BDTPAGE1 = (uint8_t)((uintptr_t)_hcd.bdt >>  8);
  KHCI->BDTPAGE2 = (uint8_t)((uintptr_t)_hcd.bdt >> 16);
  KHCI->BDTPAGE3 = (uint8_t)((uintptr_t)_hcd.bdt >> 24);

  KHCI->USBCTRL &= ~USB_USBCTRL_SUSP_MASK;
  KHCI->CTL     |= USB_CTL_ODDRST_MASK;
  for (unsigned i = 0; i < 16; ++i) {
    KHCI->ENDPOINT[i].ENDPT = 0;
  }
  const endpoint_state_t ep0 = {
    .max_packet_size = CFG_TUD_ENDPOINT0_SIZE,
    .odd             = 0,
    .length          = 0,
    .remaining       = 0,
  };
  _hcd.endpoint[0] = ep0;
  _hcd.endpoint[1] = ep0;
  KHCI->CTL &= ~USB_CTL_ODDRST_MASK;

  KHCI->SOFTHLD = 74; /* for 64-byte packets */
  KHCI->CTL     = USB_CTL_HOSTMODEEN_MASK | USB_CTL_SE0_MASK;
  KHCI->USBCTRL = USB_USBCTRL_PDE_MASK;

  NVIC_ClearPendingIRQ(USB0_IRQn);
  KHCI->INTEN = USB_INTEN_ATTACHEN_MASK;
  return true;
}

void hcd_int_enable(uint8_t rhport)
{
  (void)rhport;
  NVIC_EnableIRQ(USB0_IRQn);
}

void hcd_int_disable(uint8_t rhport)
{
  (void)rhport;
  NVIC_DisableIRQ(USB0_IRQn);
}

uint32_t hcd_frame_number(uint8_t rhport)
{
  (void)rhport;
  uint32_t frmnum = KHCI->FRMNUML;
  frmnum |= KHCI->FRMNUMH << 8u;
  return frmnum;
}

/*--------------------------------------------------------------------+
 * Port API
 *--------------------------------------------------------------------+ */
bool hcd_port_connect_status(uint8_t rhport)
{
  (void)rhport;
  return false;
}

void hcd_port_reset(uint8_t rhport)
{
  (void)rhport;
  KHCI->CTL &= ~USB_CTL_USBENSOFEN_MASK;
  KHCI->CTL |= USB_CTL_RESET_MASK;
  unsigned cnt = SystemCoreClock / 100;
  while (cnt--) __NOP();
  KHCI->CTL &= ~USB_CTL_RESET_MASK;
  KHCI->CTL |= USB_CTL_USBENSOFEN_MASK;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
  (void)rhport;
  tusb_speed_t speed = TUSB_SPEED_FULL;
  const unsigned ie = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);
  if (KHCI->ADDR & USB_ADDR_LSEN_MASK)
    speed = TUSB_SPEED_LOW;
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
  return speed;
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;
  pipe_state_t *p   = _hcd.pipe;
  pipe_state_t *end = p + HCD_MAX_XFER;
  for (;p != end; ++p) {
    if (p->dev_addr == dev_addr)
      tu_memclr(p, sizeof(*p));
  }
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
  (void)rhport;
  const unsigned rx_odd = _hcd.endpoint[0].odd;
  const unsigned tx_odd = _hcd.endpoint[1].odd;
  TU_ASSERT(0 == _hcd.bdt[0][0][tx_odd].own);

  const unsigned ie = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);

  _hcd.bdt[0][0][rx_odd    ].data = 1;
  _hcd.bdt[0][0][rx_odd ^ 1].data = 0;
  _hcd.bdt[0][1][tx_odd    ].data = 0;
  _hcd.bdt[0][1][tx_odd ^ 1].data = 1;

  unsigned hostwohub = KHCI->ENDPOINT[0].ENDPT & USB_ENDPT_HOSTWOHUB_MASK;
  KHCI->ENDPOINT[0].ENDPT = hostwohub |
    USB_ENDPT_EPHSHK_MASK | USB_ENDPT_EPRXEN_MASK | USB_ENDPT_EPTXEN_MASK;
  bool ret = false;
  if (prepare_packets(rhport, TUSB_DIR_OUT, (void*)(uintptr_t)setup_packet, 8)) {
    KHCI->ADDR  = (KHCI->ADDR & USB_ADDR_LSEN_MASK) | dev_addr;
    while (KHCI->CTL & USB_CTL_TXSUSPENDTOKENBUSY_MASK) ;
    KHCI->TOKEN = (TOK_PID_SETUP << USB_TOKEN_TOKENPID_SHIFT);
    ret = true;
  }
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
  return ret;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
  (void)rhport;
  /* Find a free pipe */
  pipe_state_t *p   = _hcd.pipe;
  pipe_state_t *end = p + HCD_MAX_XFER;
  for (;p < end && p->dev_addr; ++p) ;
  if (p == end) return false;
  p->dev_addr        = dev_addr;
  p->ep_addr         = ep_desc->bEndpointAddress;
  p->max_packet_size = ep_desc->wMaxPacketSize;
  p->xfer            = ep_desc->bmAttributes.xfer;
  p->data            = 0;
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
  (void)rhport;
  const unsigned dir_in   = tu_edpt_dir(ep_addr);
  const unsigned odd      = _hcd.endpoint[dir_in ^ 1].odd;
  buffer_descriptor_t *bd = _hcd.bdt[0][dir_in ^ 1];
  TU_ASSERT(0 == bd[odd].own);

  unsigned flags = USB_ENDPT_EPHSHK_MASK | USB_ENDPT_EPRXEN_MASK | USB_ENDPT_EPTXEN_MASK;
  if (tu_edpt_number(ep_addr)) {
    pipe_state_t *p = find_pipe(dev_addr, ep_addr);
    if (!p) return false;
    bd[odd    ].data = p->data;
    bd[odd ^ 1].data = p->data ^ 1;
    bd[odd ^ 1].own  = 0;
    flags |= USB_ENDPT_EPCTLDIS_MASK;
    /* Disable retry for a interrupt transfer.  */
    if (TUSB_XFER_INTERRUPT == p->xfer)
      flags |= USB_ENDPT_RETRYDIS_MASK;
  }
  unsigned hostwohub = KHCI->ENDPOINT[0].ENDPT & USB_ENDPT_HOSTWOHUB_MASK;
  KHCI->ENDPOINT[0].ENDPT = hostwohub | flags;
  int num_pkts = prepare_packets(rhport, dir_in, buffer, buflen);
  if (!num_pkts) return false;
  KHCI->ADDR  = (KHCI->ADDR & USB_ADDR_LSEN_MASK) | dev_addr;
  const unsigned token = tu_edpt_number(ep_addr) |
    ((dir_in ? TOK_PID_IN: TOK_PID_OUT) << USB_TOKEN_TOKENPID_SHIFT);
  const unsigned ie = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);
  do {
    while (KHCI->CTL & USB_CTL_TXSUSPENDTOKENBUSY_MASK) ;
    KHCI->TOKEN = token;
  } while (--num_pkts);
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
  return true;
}

bool hcd_edpt_clear_stall(uint8_t dev_addr, uint8_t ep_addr)
{
  if (!tu_edpt_number(ep_addr)) return true;
  pipe_state_t *p = find_pipe(dev_addr, ep_addr);
  if (!p) return false;
  p->data = 0; /* Reset data toggle */
  return true;
}

/*--------------------------------------------------------------------+
 * ISR
 *--------------------------------------------------------------------+*/
void hcd_int_handler(uint8_t rhport)
{
  uint32_t is  = KHCI->ISTAT;
  uint32_t msk = KHCI->INTEN;

  /* clear disabled interrupts */
  KHCI->ISTAT = is & ~msk;
  is &= msk;

  if (is & USB_ISTAT_ATTACH_MASK) {
    process_attach(rhport);
  }
  if (is & USB_ISTAT_STALL_MASK) {
    KHCI->ISTAT = USB_ISTAT_STALL_MASK;
  }
  if (is & USB_ISTAT_TOKDNE_MASK) {
    process_tokdne(rhport);
  }
}

#endif
