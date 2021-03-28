/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Koji Kitayama
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

#if TUSB_OPT_DEVICE_ENABLED && ( CFG_TUSB_MCU == OPT_MCU_RX63X )

#include "device/dcd.h"
#include "iodefine.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
#define SYSTEM_PRCR_PRC1     (1<<1)
#define SYSTEM_PRCR_PRKEY    (0xA5u<<8)

#define USB_FIFOSEL_TX       ((uint16_t)(1u<<5))
#define USB_FIFOSEL_MBW_8    ((uint16_t)(0u<<10))
#define USB_FIFOSEL_MBW_16   ((uint16_t)(1u<<10))
#define USB_IS0_CTSQ         ((uint16_t)(7u))
#define USB_IS0_DVSQ         ((uint16_t)(7u<<4))
#define USB_IS0_VALID        ((uint16_t)(1u<<3))
#define USB_IS0_BRDY         ((uint16_t)(1u<<8))
#define USB_IS0_NRDY         ((uint16_t)(1u<<9))
#define USB_IS0_BEMP         ((uint16_t)(1u<<10))
#define USB_IS0_CTRT         ((uint16_t)(1u<<11))
#define USB_IS0_DVST         ((uint16_t)(1u<<12))
#define USB_IS0_SOFR         ((uint16_t)(1u<<13))
#define USB_IS0_RESM         ((uint16_t)(1u<<14))
#define USB_IS0_VBINT        ((uint16_t)(1u<<15))
#define USB_IS1_SACK         ((uint16_t)(1u<<4))
#define USB_IS1_SIGN         ((uint16_t)(1u<<5))
#define USB_IS1_EOFERR       ((uint16_t)(1u<<6))
#define USB_IS1_ATTCH        ((uint16_t)(1u<<11))
#define USB_IS1_DTCH         ((uint16_t)(1u<<12))
#define USB_IS1_BCHG         ((uint16_t)(1u<<14))
#define USB_IS1_OVRCR        ((uint16_t)(1u<<15))

#define USB_IS0_CTSQ_MSK     (7u)
#define USB_IS0_CTSQ_SETUP   (1u)
#define USB_IS0_DVSQ_DEF     (1u<<4)
#define USB_IS0_DVSQ_ADDR    (2u<<4)
#define USB_IS0_DVSQ_SUSP    (4u<<4)

#define USB_PIPECTR_PID_NAK   (0u)
#define USB_PIPECTR_PID_BUF   (1u)
#define USB_PIPECTR_PID_STALL (2u)
#define USB_PIPECTR_CCPL      (1u<<2)
#define USB_PIPECTR_SQMON     (1u<<6)
#define USB_PIPECTR_SQCLR     (1u<<8)
#define USB_PIPECTR_ACLRM     (1u<<9)
#define USB_PIPECTR_INBUFM    (1u<<14)
#define USB_PIPECTR_BSTS      (1u<<15)

#define USB_FIFOCTR_DTLN     (0x1FF)
#define USB_FIFOCTR_FRDY     (1u<<13)
#define USB_FIFOCTR_BCLR     (1u<<14)
#define USB_FIFOCTR_BVAL     (1u<<15)

#define USB_PIPECFG_SHTNAK   (1u<<7)
#define USB_PIPECFG_DBLB     (1u<<9)
#define USB_PIPECFG_BULK     (1u<<14)
#define USB_PIPECFG_ISO      (3u<<14)
#define USB_PIPECFG_INT      (2u<<14)

#define FIFO_REQ_CLR         (1u)
#define FIFO_COMPLETE        (1u<<1)

typedef struct {
  union {
    struct {
      uint16_t      : 8;
      uint16_t TRCLR: 1;
      uint16_t TRENB: 1;
      uint16_t      : 0;
    };
    uint16_t TRE;
  };
  uint16_t TRN;
} reg_pipetre_t;

typedef union {
  struct {
    volatile uint16_t u8: 8;
    volatile uint16_t   : 0;
  };
  volatile uint16_t u16;
} hw_fifo_t;

typedef struct TU_ATTR_PACKED
{
  uintptr_t addr;      /* the start address of a transfer data buffer */
  uint16_t  length;    /* the number of bytes in the buffer */
  uint16_t  remaining; /* the number of bytes remaining in the buffer */
  struct {
    uint32_t ep  : 8;  /* an assigned endpoint address */
    uint32_t     : 0;
  };
} pipe_state_t;

typedef struct
{
  pipe_state_t pipe[9];
  uint8_t ep[2][16];   /* a lookup table for a pipe index from an endpoint address */
} dcd_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION static dcd_data_t _dcd;

static uint32_t disable_interrupt(void)
{
  uint32_t pswi;
  pswi = __builtin_rx_mvfc(0) & 0x010000;
  __builtin_rx_clrpsw('I');
  return pswi;
}

static void enable_interrupt(uint32_t pswi)
{
  __builtin_rx_mvtc(0, __builtin_rx_mvfc(0) | pswi);
}

static unsigned find_pipe(unsigned xfer)
{
  switch (xfer) {
  case TUSB_XFER_ISOCHRONOUS:
    for (int i = 1; i <= 2; ++i) {
      if (0 == _dcd.pipe[i].ep) return  i;
    }
    break;
  case TUSB_XFER_BULK:
    for (int i = 3; i <= 5; ++i) {
      if (0 == _dcd.pipe[i].ep) return  i;
    }
    for (int i = 1; i <= 1; ++i) {
      if (0 == _dcd.pipe[i].ep) return  i;
    }
    break;
  case TUSB_XFER_INTERRUPT:
    for (int i = 6; i <= 9; ++i) {
      if (0 == _dcd.pipe[i].ep) return  i;
    }
    break;
  default:
    /* No support for control transfer */
    break;
  }
  return 0;
}

static volatile uint16_t* get_pipectr(unsigned num)
{
  volatile uint16_t *ctr = NULL;
  if (num) {
    ctr = (volatile uint16_t*)&USB0.PIPE1CTR.WORD;
    ctr += num - 1;
  } else {
    ctr = (volatile uint16_t*)&USB0.DCPCTR.WORD;
  }
  return ctr;
}

static volatile reg_pipetre_t* get_pipetre(unsigned num)
{
  volatile reg_pipetre_t* tre = NULL;
  if ((1 <= num) && (num <= 5)) {
    tre = (volatile reg_pipetre_t*)&USB0.PIPE1TRE.WORD;
    tre += num - 1;
  }
  return tre;
}

static volatile uint16_t* ep_addr_to_pipectr(uint8_t rhport, unsigned ep_addr)
{
  (void)rhport;
  volatile uint16_t *ctr = NULL;
  const unsigned epn   = tu_edpt_number(ep_addr);
  if (epn) {
    const unsigned dir = tu_edpt_dir(ep_addr);
    const unsigned num = _dcd.ep[dir][epn];
    if (num) {
      ctr = (volatile uint16_t*)&USB0.PIPE1CTR.WORD;
      ctr += num - 1;
    }
  } else {
    ctr = (volatile uint16_t*)&USB0.DCPCTR.WORD;
  }
  return ctr;
}

static unsigned wait_for_pipe_ready(void)
{
  unsigned ctr;
  do {
    ctr = USB0.D0FIFOCTR.WORD;
  } while (!(ctr & USB_FIFOCTR_FRDY));
  return ctr;
}

static unsigned select_pipe(unsigned num, unsigned attr)
{
  USB0.PIPESEL.WORD  = num;
  USB0.D0FIFOSEL.WORD = num | attr;
  while (!(USB0.D0FIFOSEL.BIT.CURPIPE != num)) ;
  return wait_for_pipe_ready();
}

/* 1 less than mps bytes were written to FIFO
 * 2 no bytes were written to FIFO
 * 0 mps bytes were written to FIFO */
static int fifo_write(volatile void *fifo, pipe_state_t* pipe, unsigned mps)
{
  unsigned rem  = pipe->remaining;
  if (!rem) return 2;
  unsigned len  = TU_MIN(rem, mps);

  hw_fifo_t *reg = (hw_fifo_t*)fifo;
  uintptr_t addr = pipe->addr + pipe->length - rem;
  if (addr & 1u) {
    /* addr is not 2-byte aligned */
    reg->u8 = *(const uint8_t *)addr;
    ++addr;
    --len;
  }
  while (len >= 2) {
    reg->u16 = *(const uint16_t *)addr;
    addr += 2;
    len  -= 2;
  }
  if (len) {
    reg->u8 = *(const uint8_t *)addr;
    ++addr;
  }
  if (rem < mps) return 1;
  return 0;
}

/* 1 less than mps bytes were read from FIFO
 * 2 the end of the buffer reached.
 * 0 mps bytes were read from FIFO */
static int fifo_read(volatile void *fifo, pipe_state_t* pipe, unsigned mps, size_t len)
{
  unsigned rem  = pipe->remaining;
  if (!rem) return 2;
  if (rem < len) len = rem;
  pipe->remaining = rem - len;

  hw_fifo_t *reg = (hw_fifo_t*)fifo;
  uintptr_t addr = pipe->addr;
  unsigned  loop = len;
  while (loop--) {
    *(uint8_t *)addr = reg->u8;
    ++addr;
  }
  pipe->addr = addr;
  if (rem < mps)  return 1;
  if (rem == len) return 2;
  return 0;
}

static void process_setup_packet(uint8_t rhport)
{
  uint16_t setup_packet[4];
  if (0 == (USB0.INTSTS0.WORD & USB_IS0_VALID)) return;
  USB0.CFIFOCTR.WORD = USB_FIFOCTR_BCLR;
  setup_packet[0] = USB0.USBREQ.WORD;
  setup_packet[1] = USB0.USBVAL;
  setup_packet[2] = USB0.USBINDX;
  setup_packet[3] = USB0.USBLENG;
  USB0.INTSTS0.WORD = ~USB_IS0_VALID;
  dcd_event_setup_received(rhport, (const uint8_t*)&setup_packet[0], true);
}

static void process_status_completion(uint8_t rhport)
{
  uint8_t ep_addr;
  /* Check the data stage direction */
  if (USB0.CFIFOSEL.WORD & USB_FIFOSEL_TX) {
    /* IN transfer. */
    ep_addr = tu_edpt_addr(0, TUSB_DIR_IN);
  } else {
    /* OUT transfer. */
    ep_addr = tu_edpt_addr(0, TUSB_DIR_OUT);
  }
  dcd_event_xfer_complete(rhport, ep_addr, 0, XFER_RESULT_SUCCESS, true);
}

static bool process_edpt0_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  (void)rhport;

  pipe_state_t *pipe = &_dcd.pipe[0];
  /* configure fifo direction and access unit settings */
  if (ep_addr) { /* IN, 2 bytes */
    USB0.CFIFOSEL.WORD = USB_FIFOSEL_TX | USB_FIFOSEL_MBW_16;
    while (!(USB0.CFIFOSEL.WORD & USB_FIFOSEL_TX)) ;
  } else {       /* OUT, a byte */
    USB0.CFIFOSEL.WORD = USB_FIFOSEL_MBW_8;
    while (USB0.CFIFOSEL.WORD & USB_FIFOSEL_TX) ;
  }
  if (total_bytes) {
    pipe->addr      = (uintptr_t)buffer;
    pipe->length    = total_bytes;
    pipe->remaining = total_bytes;
    if (ep_addr) { /* IN */
      TU_ASSERT(USB0.DCPCTR.BIT.BSTS && (USB0.USBREQ.WORD & 0x80));
      if (fifo_write(&USB0.CFIFO.WORD, pipe, 64)) {
        USB0.CFIFOCTR.WORD = USB_FIFOCTR_BVAL;
      }
    }
    USB0.DCPCTR.WORD = USB_PIPECTR_PID_BUF;
  } else {
    /* ZLP */
    pipe->addr       = 0;
    pipe->length     = 0;
    pipe->remaining  = 0;
    USB0.DCPCTR.WORD = USB_PIPECTR_CCPL | USB_PIPECTR_PID_BUF;
  }
  return true;
}

static void process_edpt0_bemp(uint8_t rhport)
{
  pipe_state_t *pipe = &_dcd.pipe[0];
  const unsigned rem = pipe->remaining;
  if (rem > 64) {
    pipe->remaining = rem - 64;
    int r = fifo_write(&USB0.CFIFO.WORD, &_dcd.pipe[0], 64);
    if (r) USB0.CFIFOCTR.WORD = USB_FIFOCTR_BVAL;
    return;
  }
  pipe->addr      = 0;
  pipe->remaining = 0;
  dcd_event_xfer_complete(rhport, tu_edpt_addr(0, TUSB_DIR_IN),
                          pipe->length, XFER_RESULT_SUCCESS, true);
}

static void process_edpt0_brdy(uint8_t rhport)
{
  size_t len = USB0.CFIFOCTR.BIT.DTLN;
  int cplt = fifo_read(&USB0.CFIFO.WORD, &_dcd.pipe[0], 64, len);
  if (cplt || (len < 64)) {
    if (2 != cplt) {
      USB0.CFIFOCTR.WORD = USB_FIFOCTR_BCLR;
    }
    dcd_event_xfer_complete(rhport, tu_edpt_addr(0, TUSB_DIR_OUT),
                            _dcd.pipe[0].length - _dcd.pipe[0].remaining,
                            XFER_RESULT_SUCCESS, true);
  }
}

static bool process_pipe_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  (void)rhport;

  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned dir = tu_edpt_dir(ep_addr);
  const unsigned num = _dcd.ep[dir][epn];

  TU_ASSERT(num);

  pipe_state_t *pipe = &_dcd.pipe[num];
  pipe->addr      = (uintptr_t)buffer;
  pipe->length    = total_bytes;
  pipe->remaining = total_bytes;

  USB0.PIPESEL.WORD  = num;
  const unsigned mps = USB0.PIPEMAXP.WORD;
  if (dir) { /* IN */
    USB0.D0FIFOSEL.WORD = num | USB_FIFOSEL_MBW_16;
    while (!(USB0.D0FIFOSEL.BIT.CURPIPE != num)) ;
    int r = fifo_write(&USB0.D0FIFO.WORD, pipe, mps);
    if (r) USB0.D0FIFOCTR.WORD = USB_FIFOCTR_BVAL;
    USB0.D0FIFOSEL.WORD = 0;
  } else {
    volatile reg_pipetre_t *pt = get_pipetre(num);
    if (pt) {
      volatile uint16_t *ctr = get_pipectr(num);
      if (*ctr & 0x3) *ctr = USB_PIPECTR_PID_NAK;
      pt->TRE   = TU_BIT(8);
      pt->TRN   = (total_bytes + mps - 1) / mps;
      pt->TRENB = 1;
      *ctr = USB_PIPECTR_PID_BUF;
    }
  }
  //  TU_LOG1("X %x %d\r\n", ep_addr, total_bytes);
  return true;
}

static void process_pipe_brdy(uint8_t rhport, unsigned num)
{
  pipe_state_t *pipe = &_dcd.pipe[num];
  if (tu_edpt_dir(pipe->ep)) { /* IN */
    select_pipe(num, USB_FIFOSEL_MBW_16);
    const unsigned mps = USB0.PIPEMAXP.WORD;
    unsigned rem       = pipe->remaining;
    rem               -= TU_MIN(rem, mps);
    pipe->remaining    = rem;
    if (rem) {
      int r = 0;
      r = fifo_write(&USB0.D0FIFO.WORD, pipe, mps);
      if (r) USB0.D0FIFOCTR.WORD = USB_FIFOCTR_BVAL;
      USB0.D0FIFOSEL.WORD = 0;
      return;
    }
    USB0.D0FIFOSEL.WORD = 0;
    pipe->addr      = 0;
    pipe->remaining = 0;
    dcd_event_xfer_complete(rhport, pipe->ep, pipe->length,
                            XFER_RESULT_SUCCESS, true);
  } else {
    const unsigned ctr = select_pipe(num, USB_FIFOSEL_MBW_8);
    const unsigned len = ctr & USB_FIFOCTR_DTLN;
    const unsigned mps = USB0.PIPEMAXP.WORD;
    int cplt = fifo_read(&USB0.D0FIFO.WORD, pipe, mps, len);
    if (cplt || (len < mps)) {
      if (2 != cplt) {
        USB0.D0FIFO.WORD = USB_FIFOCTR_BCLR;
      }
      USB0.D0FIFOSEL.WORD = 0;
      dcd_event_xfer_complete(rhport, pipe->ep,
                              pipe->length - pipe->remaining,
                              XFER_RESULT_SUCCESS, true);
      return;
    }
    USB0.D0FIFOSEL.WORD = 0;
  }
}

static void process_bus_reset(uint8_t rhport)
{
  USB0.BEMPENB.WORD   = 1;
  USB0.BRDYENB.WORD   = 1;
  USB0.CFIFOCTR.WORD  = USB_FIFOCTR_BCLR;
  USB0.D0FIFOSEL.WORD = 0;
  USB0.D1FIFOSEL.WORD = 0;
  volatile uint16_t *ctr = (volatile uint16_t*)((uintptr_t)(&USB0.PIPE1CTR.WORD));
  volatile uint16_t *tre = (volatile uint16_t*)((uintptr_t)(&USB0.PIPE1TRE.WORD));
  for (int i = 1; i <= 5; ++i) {
    USB0.PIPESEL.WORD  = i;
    USB0.PIPECFG.WORD  = 0;
    *ctr = USB_PIPECTR_ACLRM;
    *ctr = 0;
    ++ctr;
    *tre = TU_BIT(8);
    tre += 2;
  }
  for (int i = 6; i <= 9; ++i) {
    USB0.PIPESEL.WORD  = i;
    USB0.PIPECFG.WORD  = 0;
    *ctr = USB_PIPECTR_ACLRM;
    *ctr = 0;
    ++ctr;
  }
  tu_varclr(&_dcd);
  dcd_event_bus_reset(rhport, TUSB_SPEED_FULL, true);
}

static void process_set_address(uint8_t rhport)
{
  const uint32_t addr = USB0.USBADDR.BIT.USBADDR;
  if (!addr) return;
  const tusb_control_request_t setup_packet = {
    .bmRequestType = 0,
    .bRequest      = 5,
    .wValue        = addr,
    .wIndex        = 0,
    .wLength       = 0,
  };
  dcd_event_setup_received(rhport, (const uint8_t*)&setup_packet, true);
}

/*------------------------------------------------------------------*/
/* Device API
 *------------------------------------------------------------------*/
void dcd_init(uint8_t rhport)
{
  (void)rhport;
  /* Enable USB0 */
  uint32_t pswi = disable_interrupt();
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY | SYSTEM_PRCR_PRC1;
  MSTP(USB0) = 0;
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY;
  enable_interrupt(pswi);
  USB0.SYSCFG.BIT.SCKE = 1;
  while (!USB0.SYSCFG.BIT.SCKE) ;
  USB0.SYSCFG.BIT.DRPD = 0;
  USB0.SYSCFG.BIT.DCFM = 0;
  USB0.SYSCFG.BIT.USBE = 1;

  IR(USB0, USBI0)   = 0;

  /* Setup default control pipe */
  USB0.DCPMAXP.BIT.MXPS  = 64;
  USB0.INTENB0.WORD = USB_IS0_VBINT | USB_IS0_BRDY | USB_IS0_BEMP | USB_IS0_DVST | USB_IS0_CTRT;
  USB0.BEMPENB.WORD = 1;
  USB0.BRDYENB.WORD = 1;

  if (USB0.INTSTS0.BIT.VBSTS) {
    dcd_connect(rhport);
  }
}

void dcd_int_enable(uint8_t rhport)
{
  (void)rhport;
  IEN(USB0, USBI0) = 1;
}

void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;
  IEN(USB0, USBI0) = 0;
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;
  (void)dev_addr;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void)rhport;
  /* TODO */
}

void dcd_connect(uint8_t rhport)
{
  (void)rhport;
  USB0.SYSCFG.BIT.DPRPU = 1;
}

void dcd_disconnect(uint8_t rhport)
{
  (void)rhport;
  USB0.SYSCFG.BIT.DPRPU = 0;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * ep_desc)
{
  (void)rhport;

  const unsigned ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn     = tu_edpt_number(ep_addr);
  const unsigned dir     = tu_edpt_dir(ep_addr);
  const unsigned xfer    = ep_desc->bmAttributes.xfer;

  const unsigned mps = ep_desc->wMaxPacketSize.size;
  if (xfer == TUSB_XFER_ISOCHRONOUS && mps > 256) {
    /* USBa supports up to 256 bytes */
    return false;
  }

  const unsigned num = find_pipe(xfer);
  if (!num) return false;
  _dcd.pipe[num].ep = ep_addr;
  _dcd.ep[dir][epn] = num;

  /* setup pipe */
  dcd_int_disable(rhport);
  USB0.PIPESEL.WORD  = num;
  USB0.PIPEMAXP.WORD = mps;
  volatile uint16_t *ctr = get_pipectr(num);
  *ctr = USB_PIPECTR_ACLRM;
  *ctr = 0;
  unsigned cfg = (dir << 4) | epn;
  if (xfer == TUSB_XFER_BULK) {
    cfg |= USB_PIPECFG_BULK | USB_PIPECFG_SHTNAK | USB_PIPECFG_DBLB;
  } else if (xfer == TUSB_XFER_INTERRUPT) {
    cfg |= USB_PIPECFG_INT;
  } else {
    cfg |= USB_PIPECFG_ISO | USB_PIPECFG_DBLB;
  }
  USB0.PIPECFG.WORD  = cfg;
  USB0.BRDYSTS.WORD  = 0x1FFu ^ TU_BIT(num);
  USB0.BRDYENB.WORD |= TU_BIT(num);
  if (dir || (xfer != TUSB_XFER_BULK)) {
    *ctr = USB_PIPECTR_PID_BUF;
  }
  //  TU_LOG1("O %d %x %x\r\n", USB0.PIPESEL.WORD, USB0.PIPECFG.WORD, USB0.PIPEMAXP.WORD);
  dcd_int_enable(rhport);

  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;
  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned dir = tu_edpt_dir(ep_addr);
  const unsigned num = _dcd.ep[dir][epn];

  USB0.BRDYENB.WORD &= ~TU_BIT(num);
  volatile uint16_t *ctr = get_pipectr(num);
  *ctr = 0;
  USB0.PIPESEL.WORD = num;
  USB0.PIPECFG.WORD = 0;
  _dcd.pipe[num].ep = 0;
  _dcd.ep[dir][epn] = 0;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  bool r;
  const unsigned epn = tu_edpt_number(ep_addr);
  dcd_int_disable(rhport);
  if (0 == epn) {
    r = process_edpt0_xfer(rhport, ep_addr, buffer, total_bytes);
  } else {
    r = process_pipe_xfer(rhport, ep_addr, buffer, total_bytes);
  }
  dcd_int_enable(rhport);
  return r;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  volatile uint16_t *ctr = ep_addr_to_pipectr(rhport, ep_addr);
  if (!ctr) return;
  dcd_int_disable(rhport);
  const uint32_t pid = *ctr & 0x3;
  *ctr = pid | USB_PIPECTR_PID_STALL;
  *ctr = USB_PIPECTR_PID_STALL;
  dcd_int_enable(rhport);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  volatile uint16_t *ctr = ep_addr_to_pipectr(rhport, ep_addr);
  if (!ctr) return;
  dcd_int_disable(rhport);
  *ctr = USB_PIPECTR_SQCLR;

  if (tu_edpt_dir(ep_addr)) { /* IN */
    *ctr = USB_PIPECTR_PID_BUF;
  } else {
    const unsigned num = _dcd.ep[0][tu_edpt_number(ep_addr)];
    USB0.PIPESEL.WORD  = num;
    if (USB0.PIPECFG.BIT.TYPE != 1) {
      *ctr = USB_PIPECTR_PID_BUF;
    }
  }
  dcd_int_enable(rhport);
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+
void dcd_int_handler(uint8_t rhport)
{
  (void)rhport;

  unsigned is0 = USB0.INTSTS0.WORD;
  /* clear bits except VALID */
  USB0.INTSTS0.WORD = USB_IS0_VALID;
  if (is0 & USB_IS0_VBINT) {
    if (USB0.INTSTS0.BIT.VBSTS) {
      dcd_connect(rhport);
    } else {
      dcd_disconnect(rhport);
    }
  }
  if (is0 & USB_IS0_DVST) {
    switch (is0 & USB_IS0_DVSQ) {
    case USB_IS0_DVSQ_DEF:
      process_bus_reset(rhport);
      break;
    case USB_IS0_DVSQ_ADDR:
      process_set_address(rhport);
      break;
    default:
      break;
    }
  }
  if (is0 & USB_IS0_CTRT) {
    if (is0 & USB_IS0_CTSQ_SETUP) {
      /* A setup packet has been received. */
      process_setup_packet(rhport);
    } else if (0 == (is0 & USB_IS0_CTSQ_MSK)) {
      /* A ZLP has been sent/received. */
      process_status_completion(rhport);
    }
  }
  if (is0 & USB_IS0_BEMP) {
    const unsigned s = USB0.BEMPSTS.WORD;
    USB0.BEMPSTS.WORD = 0;
    if (s & 1) {
      process_edpt0_bemp(rhport);
    }
  }
  if (is0 & USB_IS0_BRDY) {
    const unsigned m = USB0.BRDYENB.WORD;
    unsigned s       = USB0.BRDYSTS.WORD & m;
    USB0.BRDYSTS.WORD = 0;
    if (s & 1) {
      process_edpt0_brdy(rhport);
      s &= ~1;
    }
    while (s) {
      const unsigned num = __builtin_ctz(s);
      process_pipe_brdy(rhport, num);
      s &= ~TU_BIT(num);
    }
  }
}

#endif
