/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji Kitayama
 * Portions copyrighted (c) 2021 Roland Winistoerfer
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

#if TUSB_OPT_HOST_ENABLED && ( CFG_TUSB_MCU == OPT_MCU_RX63X || \
                               CFG_TUSB_MCU == OPT_MCU_RX65X || \
                               CFG_TUSB_MCU == OPT_MCU_RX72N )
#include "host/hcd.h"
#include "iodefine.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
#define SYSTEM_PRCR_PRC1     (1<<1)
#define SYSTEM_PRCR_PRKEY    (0xA5u<<8)

#define USB_DVSTCTR0_LOW     (1u)
#define USB_DVSTCTR0_FULL    (2u)

#define USB_FIFOSEL_TX       ((uint16_t)(1u<<5))
#define USB_FIFOSEL_BIGEND   ((uint16_t)(1u<<8))
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
#define USB_IS0_DVSQ_SUSP0   (4u<<4)
#define USB_IS0_DVSQ_SUSP1   (5u<<4)
#define USB_IS0_DVSQ_SUSP2   (6u<<4)
#define USB_IS0_DVSQ_SUSP3   (7u<<4)

#define USB_PIPECTR_PID_MSK   (3u)
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

#define USB_DEVADD_LOW       (1u<<6)
#define USB_DEVADD_FULL      (2u<<6)

#define FIFO_REQ_CLR         (1u)
#define FIFO_COMPLETE        (1u<<1)

// Start of definition of packed structs (used by the CCRX toolchain)
TU_ATTR_PACKED_BEGIN
TU_ATTR_BIT_FIELD_ORDER_BEGIN

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
  void      *buf;      /* the start address of a transfer data buffer */
  uint16_t  length;    /* the number of bytes in the buffer */
  uint16_t  remaining; /* the number of bytes remaining in the buffer */
  struct {
    uint32_t ep  : 8;  /* an assigned endpoint address */
    uint32_t dev : 8;  /* an assigned device address */
    uint32_t ff  : 1;  /* `buf` is TU_FUFO or POD */
    uint32_t     : 0;
  };
} pipe_state_t;

TU_ATTR_PACKED_END  // End of definition of packed structs (used by the CCRX toolchain)
TU_ATTR_BIT_FIELD_ORDER_END

typedef struct
{
  bool         need_reset; /* The device has not been reset after connection. */
  pipe_state_t pipe[10];
  uint8_t ep[4][2][15];   /* a lookup table for a pipe index from an endpoint address */
  uint8_t      ctl_mps[5]; /* EP0 max packet size for each device */
} hcd_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static hcd_data_t _hcd;

static uint32_t disable_interrupt(void)
{
  uint32_t pswi;
#if defined(__CCRX__)
  pswi = get_psw() & 0x010000;
  clrpsw_i();
#else
  pswi = __builtin_rx_mvfc(0) & 0x010000;
  __builtin_rx_clrpsw('I');
#endif
  return pswi;
}

static void enable_interrupt(uint32_t pswi)
{
#if defined(__CCRX__)
  set_psw(get_psw() | pswi);
#else
  __builtin_rx_mvtc(0, __builtin_rx_mvfc(0) | pswi);
#endif
}

static unsigned find_pipe(unsigned xfer)
{
  switch (xfer) {
  case TUSB_XFER_ISOCHRONOUS:
    for (int i = 1; i <= 2; ++i) {
      if (0 == _hcd.pipe[i].ep) return  i;
    }
    break;
  case TUSB_XFER_BULK:
    for (int i = 3; i <= 5; ++i) {
      if (0 == _hcd.pipe[i].ep) return  i;
    }
    for (int i = 1; i <= 1; ++i) {
      if (0 == _hcd.pipe[i].ep) return  i;
    }
    break;
  case TUSB_XFER_INTERRUPT:
    for (int i = 6; i <= 9; ++i) {
      if (0 == _hcd.pipe[i].ep) return  i;
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

static volatile uint16_t* addr_to_pipectr(uint8_t dev_addr, unsigned ep_addr)
{
  volatile uint16_t *ctr = NULL;
  const unsigned epn   = tu_edpt_number(ep_addr);
  if (epn) {
    const unsigned dir_in = tu_edpt_dir(ep_addr);
    const unsigned num    = _hcd.ep[dev_addr][dir_in][epn - 1];
    if (num) {
      ctr = (volatile uint16_t*)&USB0.PIPE1CTR.WORD;
      ctr += num - 1;
    }
  } else {
    ctr = (volatile uint16_t*)&USB0.DCPCTR.WORD;
  }
  return ctr;
}

static unsigned edpt0_max_packet_size(void)
{
  return USB0.DCPMAXP.BIT.MXPS;
}

static unsigned edpt_max_packet_size(unsigned num)
{
  USB0.PIPESEL.WORD = num;
  return USB0.PIPEMAXP.BIT.MXPS;
}

static inline void pipe_wait_for_ready(unsigned num)
{
  while (USB0.D0FIFOSEL.BIT.CURPIPE != num) ;
  while (!USB0.D0FIFOCTR.BIT.FRDY) ;
}

static void pipe_write_packet(void *buf, volatile void *fifo, unsigned len)
{
  volatile hw_fifo_t *reg = (volatile hw_fifo_t*)fifo;
  uintptr_t addr = (uintptr_t)buf;
  while (len >= 2) {
    reg->u16 = *(const uint16_t *)addr;
    addr += 2;
    len  -= 2;
  }
  if (len) {
    reg->u8 = *(const uint8_t *)addr;
    ++addr;
  }
}

static void pipe_read_packet(void *buf, volatile void *fifo, unsigned len)
{
  uint8_t *p   = (uint8_t*)buf;
  volatile uint8_t *reg = (volatile uint8_t*)fifo;  /* byte access is always at base register address */
  while (len--) *p++ = *reg;
}

static bool pipe0_xfer_in(void)
{
  pipe_state_t *pipe = &_hcd.pipe[0];
  const unsigned rem = pipe->remaining;

  const unsigned mps = edpt0_max_packet_size();
  const unsigned vld = USB0.CFIFOCTR.BIT.DTLN;
  const unsigned len = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf = pipe->buf;
  if (len) {
    USB0.DCPCTR.WORD = USB_PIPECTR_PID_NAK;
    pipe_read_packet(buf, (volatile void*)&USB0.CFIFO.WORD, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps) USB0.CFIFOCTR.WORD = USB_FIFOCTR_BCLR;
  pipe->remaining = rem - len;
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return true;
  }
  USB0.DCPCTR.WORD = USB_PIPECTR_PID_BUF;
  return false;
}

static bool pipe0_xfer_out(void)
{
  pipe_state_t *pipe = &_hcd.pipe[0];
  const unsigned rem = pipe->remaining;
  if (!rem) {
    pipe->buf = NULL;
    return true;
  }
  const unsigned mps = edpt0_max_packet_size();
  const unsigned len = TU_MIN(mps, rem);
  void          *buf = pipe->buf;
  if (len) {
    pipe_write_packet(buf, (volatile void*)&USB0.CFIFO.WORD, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps) USB0.CFIFOCTR.WORD = USB_FIFOCTR_BVAL;
  pipe->remaining = rem - len;
  return false;
}

static bool pipe_xfer_in(unsigned num)
{
  pipe_state_t  *pipe = &_hcd.pipe[num];
  const unsigned rem  = pipe->remaining;

  USB0.D0FIFOSEL.WORD = num | USB_FIFOSEL_MBW_8;
  const unsigned mps  = edpt_max_packet_size(num);
  pipe_wait_for_ready(num);
  const unsigned vld  = USB0.D0FIFOCTR.BIT.DTLN;
  const unsigned len  = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf  = pipe->buf;
  if (len) {
    pipe_read_packet(buf, (volatile void*)&USB0.D0FIFO.WORD, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps) USB0.D0FIFOCTR.WORD = USB_FIFOCTR_BCLR;
  USB0.D0FIFOSEL.WORD = 0;
  while (USB0.D0FIFOSEL.BIT.CURPIPE) ; /* if CURPIPE bits changes, check written value */
  pipe->remaining     = rem - len;
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return NULL != buf;
  }
  return false;
}

static bool pipe_xfer_out(unsigned num)
{
  pipe_state_t  *pipe = &_hcd.pipe[num];
  const unsigned rem  = pipe->remaining;

  if (!rem) {
    pipe->buf = NULL;
    return true;
  }

  USB0.D0FIFOSEL.WORD = num | USB_FIFOSEL_MBW_16 | (TU_BYTE_ORDER == TU_BIG_ENDIAN ? USB_FIFOSEL_BIGEND : 0);
  const unsigned mps  = edpt_max_packet_size(num);
  pipe_wait_for_ready(num);
  const unsigned len  = TU_MIN(rem, mps);
  void          *buf  = pipe->buf;
  if (len) {
    pipe_write_packet(buf, (volatile void*)&USB0.D0FIFO.WORD, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps) USB0.D0FIFOCTR.WORD = USB_FIFOCTR_BVAL;
  USB0.D0FIFOSEL.WORD = 0;
  while (USB0.D0FIFOSEL.BIT.CURPIPE) ; /* if CURPIPE bits changes, check written value */
  pipe->remaining = rem - len;
  return false;
}

static bool process_pipe0_xfer(uint8_t dev_addr, uint8_t ep_addr, void* buffer, uint16_t buflen)
{
  (void)dev_addr;
  const unsigned dir_in = tu_edpt_dir(ep_addr);

  /* configure fifo direction and access unit settings */
  if (dir_in) { /* IN, a byte */
    USB0.CFIFOSEL.WORD  = USB_FIFOSEL_MBW_8;
    while (USB0.CFIFOSEL.WORD & USB_FIFOSEL_TX) ;
  } else {       /* OUT, 2 bytes */
    USB0.CFIFOSEL.WORD  = USB_FIFOSEL_TX | USB_FIFOSEL_MBW_16 | (TU_BYTE_ORDER == TU_BIG_ENDIAN ? USB_FIFOSEL_BIGEND : 0);
    while (!(USB0.CFIFOSEL.WORD & USB_FIFOSEL_TX)) ;
  }

  pipe_state_t *pipe = &_hcd.pipe[0];
  pipe->ep        = ep_addr;
  pipe->length    = buflen;
  pipe->remaining = buflen;
  if (buflen) {
    pipe->buf     = buffer;
    if (!dir_in) { /* OUT */
      TU_ASSERT(USB0.DCPCTR.BIT.BSTS && (USB0.USBREQ.WORD & 0x80));
      pipe0_xfer_out();
    }
  } else { /* ZLP */
    pipe->buf        = NULL;
    if (!dir_in) { /* OUT */
      USB0.CFIFOCTR.WORD = USB_FIFOCTR_BVAL;
    }
    if (dir_in == USB0.DCPCFG.BIT.DIR) {
      TU_ASSERT(USB_PIPECTR_PID_NAK == USB0.DCPCTR.BIT.PID);
      USB0.DCPCTR.BIT.SQSET = 1;
      USB0.DCPCFG.BIT.DIR = dir_in ^ 1;
    }
  }
  USB0.DCPCTR.WORD = USB_PIPECTR_PID_BUF;
  return true;
}

static bool process_pipe_xfer(uint8_t dev_addr, uint8_t ep_addr, void* buffer, uint16_t buflen)
{
  const unsigned epn    = tu_edpt_number(ep_addr);
  const unsigned dir_in = tu_edpt_dir(ep_addr);
  const unsigned num    = _hcd.ep[dev_addr - 1][dir_in][epn - 1];

  TU_ASSERT(num);

  pipe_state_t *pipe  = &_hcd.pipe[num];
  pipe->buf       = buffer;
  pipe->length    = buflen;
  pipe->remaining = buflen;
  if (!dir_in) { /* OUT */
    if (buflen) {
      pipe_xfer_out(num);
    } else { /* ZLP */
      USB0.D0FIFOSEL.WORD = num;
      pipe_wait_for_ready(num);
      USB0.D0FIFOCTR.WORD = USB_FIFOCTR_BVAL;
      USB0.D0FIFOSEL.WORD = 0;
      while (USB0.D0FIFOSEL.BIT.CURPIPE) ; /* if CURPIPE bits changes, check written value */
    }
  } else {
    volatile uint16_t     *ctr = get_pipectr(num);
    volatile reg_pipetre_t *pt = get_pipetre(num);
    if (pt) {
      const unsigned     mps = edpt_max_packet_size(num);
      if (*ctr & 0x3) *ctr = USB_PIPECTR_PID_NAK;
      pt->TRE   = TU_BIT(8);
      pt->TRN   = (buflen + mps - 1) / mps;
      pt->TRENB = 1;
    }
    *ctr = USB_PIPECTR_PID_BUF;
  }
  return true;
}

static bool process_edpt_xfer(uint8_t dev_addr, uint8_t ep_addr, void* buffer, uint16_t buflen)
{
  const unsigned epn = tu_edpt_number(ep_addr);
  if (0 == epn) {
    return process_pipe0_xfer(dev_addr, ep_addr, buffer, buflen);
  } else {
    return process_pipe_xfer(dev_addr, ep_addr, buffer, buflen);
  }
}

static void process_pipe0_bemp(uint8_t rhport)
{
  (void)rhport;
  bool completed = pipe0_xfer_out();
  if (completed) {
    pipe_state_t *pipe = &_hcd.pipe[0];
    hcd_event_xfer_complete(pipe->dev,
                            tu_edpt_addr(0, TUSB_DIR_OUT),
                            pipe->length - pipe->remaining,
                            XFER_RESULT_SUCCESS, true);
  }
}

static void process_pipe_nrdy(uint8_t rhport, unsigned num)
{
  (void)rhport;
  unsigned result;
  uint16_t volatile *ctr = get_pipectr(num);
  // TU_LOG1("NRDY %d %x\n", num, *ctr);
  switch (*ctr & USB_PIPECTR_PID_MSK) {
    default: return;
    case USB_PIPECTR_PID_STALL: result = XFER_RESULT_STALLED; break;
    case USB_PIPECTR_PID_NAK:   result = XFER_RESULT_FAILED;  break;
  }
  pipe_state_t *pipe = &_hcd.pipe[num];
  hcd_event_xfer_complete(pipe->dev, pipe->ep,
                          pipe->length - pipe->remaining,
                          result, true);
}

static void process_pipe_brdy(uint8_t rhport, unsigned num)
{
  (void)rhport;
  pipe_state_t  *pipe   = &_hcd.pipe[num];
  const unsigned dir_in = tu_edpt_dir(pipe->ep);
  bool completed;

  if (dir_in) { /* IN */
    if (num) {
      completed = pipe_xfer_in(num);
    } else {
      completed = pipe0_xfer_in();
    }
  } else {
    completed = pipe_xfer_out(num);
  }
  if (completed) {
    hcd_event_xfer_complete(pipe->dev, pipe->ep,
                            pipe->length - pipe->remaining,
                            XFER_RESULT_SUCCESS, true);
    //  TU_LOG1("C %d %d\r\n", num, pipe->length - pipe->remaining);
  }
}


/*------------------------------------------------------------------*/
/* Host API
 *------------------------------------------------------------------*/
bool hcd_init(uint8_t rhport)
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
  USB0.SYSCFG.BIT.DPRPU = 0;
  USB0.SYSCFG.BIT.DRPD = 0;
  USB0.SYSCFG.BIT.DCFM = 1;

  USB0.DVSTCTR0.BIT.VBUSEN = 1;

  USB0.SYSCFG.BIT.DRPD = 1;
  for (volatile int i = 0; i < 30000; ++i) ;
  USB0.SYSCFG.BIT.USBE = 1;

  USB.DPUSR0R.BIT.FIXPHY0 = 0u;    /* USB0 Transceiver Output fixed */
#if ( CFG_TUSB_MCU == OPT_MCU_RX72N )
  USB0.PHYSLEW.LONG = 0x5;
  IR(PERIB, INTB185) = 0;
#else
  IR(USB0, USBI0)   = 0;
#endif

  /* Setup default control pipe */
  USB0.DCPCFG.WORD  = USB_PIPECFG_SHTNAK;
  USB0.DCPMAXP.WORD = 64;
  USB0.INTENB0.WORD = USB_IS0_BRDY | USB_IS0_NRDY | USB_IS0_BEMP;
  USB0.INTENB1.WORD = USB_IS1_SACK | USB_IS1_SIGN |
    USB_IS1_ATTCH | USB_IS1_DTCH;
  USB0.BEMPENB.WORD = 1;
  USB0.NRDYENB.WORD = 1;
  USB0.BRDYENB.WORD = 1;
  return true;
}

void hcd_int_enable(uint8_t rhport)
{
  (void)rhport;
#if ( CFG_TUSB_MCU == OPT_MCU_RX72N )
  IEN(PERIB, INTB185) = 1;
#else
  IEN(USB0, USBI0) = 1;
#endif
}

void hcd_int_disable(uint8_t rhport)
{
  (void)rhport;
#if ( CFG_TUSB_MCU == OPT_MCU_RX72N )
  IEN(PERIB, INTB185) = 0;
#else
  IEN(USB0, USBI0) = 0;
#endif
}

uint32_t hcd_frame_number(uint8_t rhport)
{
  (void)rhport;
  /* The device must be reset at least once after connection 
   * in order to start the frame counter. */
  if (_hcd.need_reset) hcd_port_reset(rhport);
  return USB0.FRMNUM.BIT.FRNM;
}

/*--------------------------------------------------------------------+
 * Port API
 *--------------------------------------------------------------------+*/
bool hcd_port_connect_status(uint8_t rhport)
{
  (void)rhport;
  return USB0.INTSTS1.BIT.ATTCH ? true: false;
}

void hcd_port_reset(uint8_t rhport)
{
  USB0.DCPCTR.WORD = USB_PIPECTR_PID_NAK;
  while (USB0.DCPCTR.BIT.PBUSY) ;
  hcd_int_disable(rhport);
  USB0.DVSTCTR0.BIT.UACT   = 0;
  if (USB0.DCPCTR.BIT.SUREQ)
    USB0.DCPCTR.BIT.SUREQCLR = 1;
  hcd_int_enable(rhport);
  /* Reset should be asserted 10-20ms. */
  USB0.DVSTCTR0.BIT.USBRST = 1;
  for (volatile int i = 0; i < 2400000; ++i) ;
  USB0.DVSTCTR0.BIT.USBRST = 0;
  USB0.DVSTCTR0.BIT.UACT   = 1;
  _hcd.need_reset = false;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
  (void)rhport;
  switch (USB0.DVSTCTR0.BIT.RHST) {
    default: return TUSB_SPEED_INVALID;
    case USB_DVSTCTR0_FULL: return TUSB_SPEED_FULL;
    case USB_DVSTCTR0_LOW:  return TUSB_SPEED_LOW;
  }
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;
  uint16_t volatile *ctr;
  TU_ASSERT(dev_addr < 6,); /* USBa can only handle addresses from 0 to 5. */
  if (!dev_addr) return;
  _hcd.ctl_mps[dev_addr] = 0;
  uint8_t *ep = &_hcd.ep[dev_addr - 1][0][0];
  for (int i = 0; i < 2 * 15; ++i, ++ep) {
    unsigned num = *ep;
    if (!num || dev_addr != _hcd.pipe[num].dev) continue;

    ctr  = (uint16_t volatile*)&USB0.PIPE1CTR.WORD + num - 1;
    *ctr = 0;
    USB0.NRDYENB.WORD &= ~TU_BIT(num);
    USB0.BRDYENB.WORD &= ~TU_BIT(num);
    USB0.PIPESEL.WORD  = num;
    USB0.PIPECFG.WORD  = 0;
    USB0.PIPEMAXP.WORD = 0;

    _hcd.pipe[num].ep  = 0;
    _hcd.pipe[num].dev = 0;
    *ep                = 0;
  }
}

/*--------------------------------------------------------------------+
 * Endpoints API
 *--------------------------------------------------------------------+*/
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
  (void)rhport;
  //  TU_LOG1("S %d %x\n", dev_addr, USB0.DCPCTR.WORD);

  TU_ASSERT(dev_addr < 6); /* USBa can only handle addresses from 0 to 5. */
  TU_ASSERT(0 == USB0.DCPCTR.BIT.SUREQ);

  USB0.DCPCTR.WORD = USB_PIPECTR_PID_NAK;

  _hcd.pipe[0].buf       = NULL;
  _hcd.pipe[0].length    = 8;
  _hcd.pipe[0].remaining = 0;
  _hcd.pipe[0].dev       = dev_addr;

  while (USB0.DCPCTR.BIT.PBUSY) ;
  USB0.DCPMAXP.WORD      = (dev_addr << 12) | _hcd.ctl_mps[dev_addr];

  /* Set direction in advance for DATA stage */
  uint8_t const bmRequesttype = setup_packet[0];
  USB0.DCPCFG.BIT.DIR = tu_edpt_dir(bmRequesttype) ? 0: 1;

  uint16_t const* p = (uint16_t const*)(uintptr_t)&setup_packet[0];
  USB0.USBREQ.WORD = tu_htole16(p[0]);
  USB0.USBVAL      = p[1];
  USB0.USBINDX     = p[2];
  USB0.USBLENG     = p[3];

  USB0.DCPCTR.BIT.SUREQ = 1;
  return true;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
  (void)rhport;
  TU_ASSERT(dev_addr < 6); /* USBa can only handle addresses from 0 to 5. */

  const unsigned ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn     = tu_edpt_number(ep_addr);
  const unsigned mps     = tu_edpt_packet_size(ep_desc);
  if (0 == epn) {
    USB0.DCPCTR.WORD = USB_PIPECTR_PID_NAK;
    hcd_devtree_info_t devtree;
    hcd_devtree_get_info(dev_addr, &devtree);
    uint16_t volatile *devadd = (uint16_t volatile *)(uintptr_t)&USB0.DEVADD0.WORD;
    devadd += dev_addr;
    while (USB0.DCPCTR.BIT.PBUSY) ;
    USB0.DCPMAXP.WORD = (dev_addr << 12) | mps;
    *devadd = (TUSB_SPEED_FULL == devtree.speed) ? USB_DEVADD_FULL : USB_DEVADD_LOW;
    _hcd.ctl_mps[dev_addr] = mps;
    return true;
  }

  const unsigned dir_in = tu_edpt_dir(ep_addr);
  const unsigned xfer   = ep_desc->bmAttributes.xfer;
  if (xfer == TUSB_XFER_ISOCHRONOUS && mps > 256) {
    /* USBa supports up to 256 bytes */
    return false;
  }
  const unsigned num = find_pipe(xfer);
  if (!num) return false;
  _hcd.pipe[num].dev = dev_addr;
  _hcd.pipe[num].ep  = ep_addr;
  _hcd.ep[dev_addr - 1][dir_in][epn - 1] = num;

  /* setup pipe */
  hcd_int_disable(rhport);
  USB0.PIPESEL.WORD  = num;
  USB0.PIPEMAXP.WORD = (dev_addr << 12) | mps;
  volatile uint16_t *ctr = get_pipectr(num);
  *ctr = USB_PIPECTR_ACLRM | USB_PIPECTR_SQCLR;
  *ctr = 0;
  unsigned cfg = ((1 ^ dir_in) << 4) | epn;
  if (xfer == TUSB_XFER_BULK) {
    cfg |= USB_PIPECFG_BULK | USB_PIPECFG_SHTNAK | USB_PIPECFG_DBLB;
  } else if (xfer == TUSB_XFER_INTERRUPT) {
    cfg |= USB_PIPECFG_INT;
  } else {
    cfg |= USB_PIPECFG_ISO | USB_PIPECFG_DBLB;
  }
  USB0.PIPECFG.WORD  = cfg;
  USB0.BRDYSTS.WORD  = 0x1FFu ^ TU_BIT(num);
  USB0.NRDYENB.WORD |= TU_BIT(num);
  USB0.BRDYENB.WORD |= TU_BIT(num);
  if (!dir_in) {
    *ctr = USB_PIPECTR_PID_BUF;
  }
  hcd_int_enable(rhport);

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen)
{
  bool r;
  hcd_int_disable(rhport);
  // TU_LOG1("X %d %x %u\n", dev_addr, ep_addr, buflen);
  r = process_edpt_xfer(dev_addr, ep_addr, buffer, buflen);
  hcd_int_enable(rhport);
  return r;
}

bool hcd_edpt_clear_stall(uint8_t dev_addr, uint8_t ep_addr)
{
  uint16_t volatile *ctr = addr_to_pipectr(dev_addr, ep_addr);
  TU_ASSERT(ctr);

  const uint32_t pid = *ctr & 0x3;
  if (pid & 2) {
    *ctr = pid & 2;
    *ctr = 0;
  }
  *ctr = USB_PIPECTR_SQCLR;
  unsigned const epn = tu_edpt_number(ep_addr);
  if (!epn) return true;

  if (!tu_edpt_dir(ep_addr)) { /* OUT */
    *ctr = USB_PIPECTR_PID_BUF;
  }
  return true;
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+
void hcd_int_handler(uint8_t rhport)
{
  (void)rhport;
#if defined(__CCRX__)
  static const int Mod37BitPosition[] = {
    -1, 0, 1, 26, 2, 23, 27, 0, 3, 16, 24, 30, 28, 11, 0, 13, 4,
    7, 17, 0, 25, 22, 31, 15, 29, 10, 12, 6, 0, 21, 14, 9, 5,
    20, 8, 19, 18};
#endif

  unsigned is1 = USB0.INTSTS1.WORD;
  unsigned is0 = USB0.INTSTS0.WORD;
  /* clear active bits except VALID (don't write 0 to already cleared bits according to the HW manual) */
  USB0.INTSTS1.WORD = ~((USB_IS1_SACK | USB_IS1_SIGN | USB_IS1_ATTCH | USB_IS1_DTCH) & is1);
  USB0.INTSTS0.WORD = ~((USB_IS0_BRDY | USB_IS0_NRDY | USB_IS0_BEMP) & is0);
  // TU_LOG1("IS %04x %04x\n", is0, is1);
  is1 &= USB0.INTENB1.WORD;
  is0 &= USB0.INTENB0.WORD;

  if (is1 & USB_IS1_SACK) {
    /* Set DATA1 in advance for the next transfer. */
    USB0.DCPCTR.BIT.SQSET = 1;
    hcd_event_xfer_complete(USB0.DCPMAXP.BIT.DEVSEL,
                            tu_edpt_addr(0, TUSB_DIR_OUT),
                            8, XFER_RESULT_SUCCESS, true);
  }
  if (is1 & USB_IS1_SIGN) {
    hcd_event_xfer_complete(USB0.DCPMAXP.BIT.DEVSEL,
                            tu_edpt_addr(0, TUSB_DIR_OUT),
                            8, XFER_RESULT_FAILED, true);
  }
  if (is1 & USB_IS1_ATTCH) {
    USB0.DVSTCTR0.BIT.UACT = 1;
    _hcd.need_reset = true;
    USB0.INTENB1.WORD = (USB0.INTENB1.WORD & ~USB_IS1_ATTCH) | USB_IS1_DTCH;
    hcd_event_device_attach(rhport, true);
  }
  if (is1 & USB_IS1_DTCH) {
    USB0.DVSTCTR0.BIT.UACT = 0;
    if (USB0.DCPCTR.BIT.SUREQ)
      USB0.DCPCTR.BIT.SUREQCLR = 1;
    USB0.INTENB1.WORD = (USB0.INTENB1.WORD & ~USB_IS1_DTCH) | USB_IS1_ATTCH;
    hcd_event_device_remove(rhport, true);
  }

  if (is0 & USB_IS0_BEMP) {
    const unsigned s = USB0.BEMPSTS.WORD;
    USB0.BEMPSTS.WORD = 0;
    if (s & 1) {
      process_pipe0_bemp(rhport);
    }
  }
  if (is0 & USB_IS0_NRDY) {
    const unsigned m = USB0.NRDYENB.WORD;
    unsigned s       = USB0.NRDYSTS.WORD & m;
    USB0.NRDYSTS.WORD = ~s;
    while (s) {
#if defined(__CCRX__)
      const unsigned num = Mod37BitPosition[(-s & s) % 37];
#else
      const unsigned num = __builtin_ctz(s);
#endif
      process_pipe_nrdy(rhport, num);
      s &= ~TU_BIT(num);
    }
  }
  if (is0 & USB_IS0_BRDY) {
    const unsigned m = USB0.BRDYENB.WORD;
    unsigned s       = USB0.BRDYSTS.WORD & m;
    /* clear active bits (don't write 0 to already cleared bits according to the HW manual) */
    USB0.BRDYSTS.WORD = ~s;
    while (s) {
#if defined(__CCRX__)
      const unsigned num = Mod37BitPosition[(-s & s) % 37];
#else
      const unsigned num = __builtin_ctz(s);
#endif
      process_pipe_brdy(rhport, num);
      s &= ~TU_BIT(num);
    }
  }
}

#endif
