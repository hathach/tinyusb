/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Koji Kitayama
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

// Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
// We disable SOF for now until needed later on
#define USE_SOF     0

#if TUSB_OPT_DEVICE_ENABLED && ( CFG_TUSB_MCU == OPT_MCU_RX63X || \
                                 CFG_TUSB_MCU == OPT_MCU_RX65X || \
                                 CFG_TUSB_MCU == OPT_MCU_RX72N )
#include "device/dcd.h"
#include "iodefine.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
#define SYSTEM_PRCR_PRC1     (1<<1)
#define SYSTEM_PRCR_PRKEY    (0xA5u<<8)

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
    uint32_t ff  : 1;  /* `buf` is TU_FUFO or POD */
    uint32_t     : 0;
  };
} pipe_state_t;

TU_ATTR_PACKED_END  // End of definition of packed structs (used by the CCRX toolchain)
TU_ATTR_BIT_FIELD_ORDER_END

typedef struct
{
  pipe_state_t pipe[10];
  uint8_t ep[2][16];   /* a lookup table for a pipe index from an endpoint address */
} dcd_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static dcd_data_t _dcd;

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

static unsigned edpt0_max_packet_size(void)
{
  return USB0.DCPMAXP.BIT.MXPS;
}

static unsigned edpt_max_packet_size(unsigned num)
{
  USB0.PIPESEL.WORD = num;
  return USB0.PIPEMAXP.WORD;
}

static inline void pipe_wait_for_ready(unsigned num)
{
  while (USB0.D0FIFOSEL.BIT.CURPIPE != num) ;
  while (!USB0.D0FIFOCTR.BIT.FRDY) ;
}

static void pipe_write_packet(void *buf, volatile void *fifo, unsigned len)
{
  volatile hw_fifo_t *reg = (volatile hw_fifo_t*) fifo;
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

static void pipe_read_write_packet_ff(tu_fifo_t *f, volatile void *fifo, unsigned len, unsigned dir)
{
  static const struct {
    void (*tu_fifo_get_info)(tu_fifo_t *f, tu_fifo_buffer_info_t *info);
    void (*tu_fifo_advance)(tu_fifo_t *f, uint16_t n);
    void (*pipe_read_write)(void *buf, volatile void *fifo, unsigned len);
  } ops[] = {
    /* OUT */ {tu_fifo_get_write_info,tu_fifo_advance_write_pointer,pipe_read_packet},
    /* IN  */ {tu_fifo_get_read_info, tu_fifo_advance_read_pointer, pipe_write_packet},
  };
  tu_fifo_buffer_info_t info;
  ops[dir].tu_fifo_get_info(f, &info);
  unsigned total_len = len;
  len = TU_MIN(total_len, info.len_lin);
  ops[dir].pipe_read_write(info.ptr_lin, fifo, len);
  unsigned rem = total_len - len;
  if (rem) {
    len = TU_MIN(rem, info.len_wrap);
    ops[dir].pipe_read_write(info.ptr_wrap, fifo, len);
    rem -= len;
  }
  ops[dir].tu_fifo_advance(f, total_len - rem);
}

static bool pipe0_xfer_in(void)
{
  pipe_state_t *pipe = &_dcd.pipe[0];
  const unsigned rem = pipe->remaining;
  if (!rem) {
    pipe->buf = NULL;
    return true;
  }
  const unsigned mps = edpt0_max_packet_size();
  const unsigned len = TU_MIN(mps, rem);
  void          *buf = pipe->buf;
  if (len) {
    if (pipe->ff) {
      pipe_read_write_packet_ff((tu_fifo_t*)buf, (volatile void*)&USB0.CFIFO.WORD, len, TUSB_DIR_IN);
    } else {
      pipe_write_packet(buf, (volatile void*)&USB0.CFIFO.WORD, len);
      pipe->buf = (uint8_t*)buf + len;
    }
  }
  if (len < mps) USB0.CFIFOCTR.WORD = USB_FIFOCTR_BVAL;
  pipe->remaining = rem - len;
  return false;
}

static bool pipe0_xfer_out(void)
{
  pipe_state_t *pipe = &_dcd.pipe[0];
  const unsigned rem = pipe->remaining;

  const unsigned mps = edpt0_max_packet_size();
  const unsigned vld = USB0.CFIFOCTR.BIT.DTLN;
  const unsigned len = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf = pipe->buf;
  if (len) {
    if (pipe->ff) {
      pipe_read_write_packet_ff((tu_fifo_t*)buf, (volatile void*)&USB0.CFIFO.WORD, len, TUSB_DIR_OUT);
    } else {
      pipe_read_packet(buf, (volatile void*)&USB0.CFIFO.WORD, len);
      pipe->buf = (uint8_t*)buf + len;
    }
  }
  if (len < mps) USB0.CFIFOCTR.WORD = USB_FIFOCTR_BCLR;
  pipe->remaining = rem - len;
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return true;
  }
  return false;
}

static bool pipe_xfer_in(unsigned num)
{
  pipe_state_t  *pipe = &_dcd.pipe[num];
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
    if (pipe->ff) {
      pipe_read_write_packet_ff((tu_fifo_t*)buf, (volatile void*)&USB0.D0FIFO.WORD, len, TUSB_DIR_IN);
    } else {
      pipe_write_packet(buf, (volatile void*)&USB0.D0FIFO.WORD, len);
      pipe->buf = (uint8_t*)buf + len;
    }
  }
  if (len < mps) USB0.D0FIFOCTR.WORD = USB_FIFOCTR_BVAL;
  USB0.D0FIFOSEL.WORD = 0;
  while (USB0.D0FIFOSEL.BIT.CURPIPE) ; /* if CURPIPE bits changes, check written value */
  pipe->remaining = rem - len;
  return false;
}

static bool pipe_xfer_out(unsigned num)
{
  pipe_state_t  *pipe = &_dcd.pipe[num];
  const unsigned rem  = pipe->remaining;

  USB0.D0FIFOSEL.WORD = num | USB_FIFOSEL_MBW_8;
  const unsigned mps  = edpt_max_packet_size(num);
  pipe_wait_for_ready(num);
  const unsigned vld  = USB0.D0FIFOCTR.BIT.DTLN;
  const unsigned len  = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf  = pipe->buf;
  if (len) {
    if (pipe->ff) {
      pipe_read_write_packet_ff((tu_fifo_t*)buf, (volatile void*)&USB0.D0FIFO.WORD, len, TUSB_DIR_OUT);
    } else {
      pipe_read_packet(buf, (volatile void*)&USB0.D0FIFO.WORD, len);
      pipe->buf = (uint8_t*)buf + len;
    }
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

static void process_setup_packet(uint8_t rhport)
{
  uint16_t setup_packet[4];
  if (0 == (USB0.INTSTS0.WORD & USB_IS0_VALID)) return;
  USB0.CFIFOCTR.WORD = USB_FIFOCTR_BCLR;
  setup_packet[0] = tu_le16toh(USB0.USBREQ.WORD);
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

static bool process_pipe0_xfer(int buffer_type, uint8_t ep_addr, void* buffer, uint16_t total_bytes)
{
  /* configure fifo direction and access unit settings */
  if (ep_addr) { /* IN, 2 bytes */
    USB0.CFIFOSEL.WORD = USB_FIFOSEL_TX | USB_FIFOSEL_MBW_16 | (TU_BYTE_ORDER == TU_BIG_ENDIAN ? USB_FIFOSEL_BIGEND : 0);
    while (!(USB0.CFIFOSEL.WORD & USB_FIFOSEL_TX)) ;
  } else {       /* OUT, a byte */
    USB0.CFIFOSEL.WORD = USB_FIFOSEL_MBW_8;
    while (USB0.CFIFOSEL.WORD & USB_FIFOSEL_TX) ;
  }

  pipe_state_t *pipe = &_dcd.pipe[0];
  pipe->ff        = buffer_type;
  pipe->length    = total_bytes;
  pipe->remaining = total_bytes;
  if (total_bytes) {
    pipe->buf     = buffer;
    if (ep_addr) { /* IN */
      TU_ASSERT(USB0.DCPCTR.BIT.BSTS && (USB0.USBREQ.WORD & 0x80));
      pipe0_xfer_in();
    }
    USB0.DCPCTR.WORD = USB_PIPECTR_PID_BUF;
  } else {
    /* ZLP */
    pipe->buf        = NULL;
    USB0.DCPCTR.WORD = USB_PIPECTR_CCPL | USB_PIPECTR_PID_BUF;
  }
  return true;
}

static bool process_pipe_xfer(int buffer_type, uint8_t ep_addr, void* buffer, uint16_t total_bytes)
{
  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned dir = tu_edpt_dir(ep_addr);
  const unsigned num = _dcd.ep[dir][epn];

  TU_ASSERT(num);

  pipe_state_t *pipe  = &_dcd.pipe[num];
  pipe->ff        = buffer_type;
  pipe->buf       = buffer;
  pipe->length    = total_bytes;
  pipe->remaining = total_bytes;
  if (dir) { /* IN */
    if (total_bytes) {
      pipe_xfer_in(num);
    } else { /* ZLP */
      USB0.D0FIFOSEL.WORD = num;
      pipe_wait_for_ready(num);
      USB0.D0FIFOCTR.WORD = USB_FIFOCTR_BVAL;
      USB0.D0FIFOSEL.WORD = 0;
      while (USB0.D0FIFOSEL.BIT.CURPIPE) ; /* if CURPIPE bits changes, check written value */
    }
  } else {
    volatile reg_pipetre_t *pt = get_pipetre(num);
    if (pt) {
      const unsigned     mps = edpt_max_packet_size(num);
      volatile uint16_t *ctr = get_pipectr(num);
      if (*ctr & 0x3) *ctr = USB_PIPECTR_PID_NAK;
      pt->TRE   = TU_BIT(8);
      pt->TRN   = (total_bytes + mps - 1) / mps;
      pt->TRENB = 1;
      *ctr = USB_PIPECTR_PID_BUF;
    }
  }
  //  TU_LOG1("X %x %d %d\r\n", ep_addr, total_bytes, buffer_type);
  return true;
}

static bool process_edpt_xfer(int buffer_type, uint8_t ep_addr, void* buffer, uint16_t total_bytes)
{
  const unsigned epn = tu_edpt_number(ep_addr);
  if (0 == epn) {
    return process_pipe0_xfer(buffer_type, ep_addr, buffer, total_bytes);
  } else {
    return process_pipe_xfer(buffer_type, ep_addr, buffer, total_bytes);
  }
}

static void process_pipe0_bemp(uint8_t rhport)
{
  bool completed = pipe0_xfer_in();
  if (completed) {
    pipe_state_t *pipe = &_dcd.pipe[0];
    dcd_event_xfer_complete(rhport, tu_edpt_addr(0, TUSB_DIR_IN),
                            pipe->length, XFER_RESULT_SUCCESS, true);
  }
}

static void process_pipe_brdy(uint8_t rhport, unsigned num)
{
  pipe_state_t  *pipe = &_dcd.pipe[num];
  const unsigned dir  = tu_edpt_dir(pipe->ep);
  bool completed;

  if (dir) { /* IN */
    completed = pipe_xfer_in(num);
  } else {
    if (num) {
      completed = pipe_xfer_out(num);
    } else {
      completed = pipe0_xfer_out();
    }
  }
  if (completed) {
    dcd_event_xfer_complete(rhport, pipe->ep,
                            pipe->length - pipe->remaining,
                            XFER_RESULT_SUCCESS, true);
    //  TU_LOG1("C %d %d\r\n", num, pipe->length - pipe->remaining);
  }
}

static void process_bus_reset(uint8_t rhport)
{
  USB0.BEMPENB.WORD   = 1;
  USB0.BRDYENB.WORD   = 1;
  USB0.CFIFOCTR.WORD  = USB_FIFOCTR_BCLR;
  USB0.D0FIFOSEL.WORD = 0;
  while (USB0.D0FIFOSEL.BIT.CURPIPE) ; /* if CURPIPE bits changes, check written value */
  USB0.D1FIFOSEL.WORD = 0;
  while (USB0.D1FIFOSEL.BIT.CURPIPE) ; /* if CURPIPE bits changes, check written value */
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
#if defined(__CCRX__)
      .bmRequestType = { 0 },  /* Note: CCRX needs the braces over this struct member */
#else
      .bmRequestType = 0,
#endif
      .bRequest      = TUSB_REQ_SET_ADDRESS,
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

  USB.DPUSR0R.BIT.FIXPHY0 = 0u;    /* USB0 Transceiver Output fixed */
#if ( CFG_TUSB_MCU == OPT_MCU_RX72N )
  USB0.PHYSLEW.LONG = 0x5;
  IR(PERIB, INTB185) = 0;
#else
  IR(USB0, USBI0)   = 0;
#endif

  /* Setup default control pipe */
  USB0.DCPMAXP.BIT.MXPS  = 64;
  USB0.INTENB0.WORD = USB_IS0_VBINT | USB_IS0_BRDY | USB_IS0_BEMP |
    USB_IS0_DVST | USB_IS0_CTRT | (USE_SOF ? USB_IS0_SOFR: 0) | USB_IS0_RESM;
  USB0.BEMPENB.WORD = 1;
  USB0.BRDYENB.WORD = 1;

  if (USB0.INTSTS0.BIT.VBSTS) {
    dcd_connect(rhport);
  }
}

void dcd_int_enable(uint8_t rhport)
{
  (void)rhport;
#if ( CFG_TUSB_MCU == OPT_MCU_RX72N )
  IEN(PERIB, INTB185) = 1;
#else
  IEN(USB0, USBI0) = 1;
#endif
}

void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;
#if ( CFG_TUSB_MCU == OPT_MCU_RX72N )
  IEN(PERIB, INTB185) = 0;
#else
  IEN(USB0, USBI0) = 0;
#endif
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;
  (void)dev_addr;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void)rhport;
  USB0.DVSTCTR0.BIT.WKUP = 1;
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

  const unsigned mps = tu_edpt_packet_size(ep_desc);
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
  *ctr = USB_PIPECTR_ACLRM | USB_PIPECTR_SQCLR;
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
  // TU_LOG1("O %d %x %x\r\n", USB0.PIPESEL.WORD, USB0.PIPECFG.WORD, USB0.PIPEMAXP.WORD);
  dcd_int_enable(rhport);

  return true;
}

void dcd_edpt_close_all(uint8_t rhport)
{
  unsigned i = TU_ARRAY_SIZE(_dcd.pipe);
  dcd_int_disable(rhport);
  while (--i) { /* Close all pipes except 0 */
    const unsigned ep_addr = _dcd.pipe[i].ep;
    if (!ep_addr) continue;
    dcd_edpt_close(rhport, ep_addr);
  }
  dcd_int_enable(rhport);
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
  dcd_int_disable(rhport);
  r = process_edpt_xfer(0, ep_addr, buffer, total_bytes);
  dcd_int_enable(rhport);
  return r;
}

bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes)
{
  // USB buffers always work in bytes so to avoid unnecessary divisions we demand item_size = 1
  TU_ASSERT(ff->item_size == 1);
  bool r;
  dcd_int_disable(rhport);
  r = process_edpt_xfer(1, ep_addr, ff, total_bytes);
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
  /* clear active bits except VALID (don't write 0 to already cleared bits according to the HW manual) */
  USB0.INTSTS0.WORD = ~((USB_IS0_CTRT | USB_IS0_DVST | USB_IS0_SOFR | USB_IS0_RESM | USB_IS0_VBINT) & is0) | USB_IS0_VALID;
  if (is0 & USB_IS0_VBINT) {
    if (USB0.INTSTS0.BIT.VBSTS) {
      dcd_connect(rhport);
    } else {
      dcd_disconnect(rhport);
    }
  }
  if (is0 & USB_IS0_RESM) {
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
#if (0==USE_SOF)
    USB0.INTENB0.BIT.SOFE = 0;
#endif
  }
  if ((is0 & USB_IS0_SOFR) && USB0.INTENB0.BIT.SOFE) {
    // USBD will exit suspended mode when SOF event is received
    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
#if (0==USE_SOF)
    USB0.INTENB0.BIT.SOFE = 0;
#endif
  }
  if (is0 & USB_IS0_DVST) {
    switch (is0 & USB_IS0_DVSQ) {
    case USB_IS0_DVSQ_DEF:
      process_bus_reset(rhport);
      break;
    case USB_IS0_DVSQ_ADDR:
      process_set_address(rhport);
      break;
    case USB_IS0_DVSQ_SUSP0:
    case USB_IS0_DVSQ_SUSP1:
    case USB_IS0_DVSQ_SUSP2:
    case USB_IS0_DVSQ_SUSP3:
       dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
#if (0==USE_SOF)
       USB0.INTENB0.BIT.SOFE = 1;
#endif
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
      process_pipe0_bemp(rhport);
    }
  }
  if (is0 & USB_IS0_BRDY) {
    const unsigned m = USB0.BRDYENB.WORD;
    unsigned s       = USB0.BRDYSTS.WORD & m;
    /* clear active bits (don't write 0 to already cleared bits according to the HW manual) */
    USB0.BRDYSTS.WORD = ~s;
    while (s) {
#if defined(__CCRX__)
      static const int Mod37BitPosition[] = {
        -1, 0, 1, 26, 2, 23, 27, 0, 3, 16, 24, 30, 28, 11, 0, 13, 4,
        7, 17, 0, 25, 22, 31, 15, 29, 10, 12, 6, 0, 21, 14, 9, 5,
        20, 8, 19, 18
      };

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
