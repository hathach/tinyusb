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

#if CFG_TUD_ENABLED && (CFG_TUSB_MCU == OPT_MCU_RX63X || \
                        CFG_TUSB_MCU == OPT_MCU_RX65X || \
                        CFG_TUSB_MCU == OPT_MCU_RX72N || \
                        CFG_TUSB_MCU == OPT_MCU_RAXXX)

#include "host/hcd.h"
#include "link_type.h"

#if TU_CHECK_MCU(OPT_MCU_RX63X, OPT_MCU_RX65X, OPT_MCU_RX72N)
#include "link_rx.h"
#elif TU_CHECK_MCU(OPT_MCU_RAXXX)
#include "link_ra.h"
#else
#error "Unsupported MCU"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

/* LINK core registers */
#define LINK_REG ((LINK_REG_t*)LINK_REG_BASE)

TU_ATTR_PACKED_BEGIN
TU_ATTR_BIT_FIELD_ORDER_BEGIN

typedef struct TU_ATTR_PACKED {
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

typedef union TU_ATTR_PACKED {
  struct {
    volatile uint16_t u8: 8;
    volatile uint16_t   : 0;
  };
  volatile uint16_t u16;
} hw_fifo_t;

typedef struct TU_ATTR_PACKED {
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
  if (num) {
    return (volatile uint16_t*)&(LINK_REG->PIPE_CTR[num - 1]);
  } else {
    return (volatile uint16_t*)&(LINK_REG->DCPCTR);
  }
}

static volatile reg_pipetre_t* get_pipetre(unsigned num)
{
  volatile reg_pipetre_t* tre = NULL;
  if ((1 <= num) && (num <= 5)) {
    tre = (volatile reg_pipetre_t*)&(LINK_REG->PIPE_TR[num - 1].E);
  }
  return tre;
}

static volatile uint16_t* addr_to_pipectr(uint8_t dev_addr, unsigned ep_addr)
{
  const unsigned epn = tu_edpt_number(ep_addr);
  if (epn) {
    const unsigned dir_in = tu_edpt_dir(ep_addr);
    const unsigned num = _hcd.ep[dev_addr][dir_in][epn - 1];
    return get_pipectr(num);
  } else {
    return get_pipectr(0);
  }
}

static unsigned edpt0_max_packet_size(void)
{
  return LINK_REG->DCPMAXP_b.MXPS;
}

static unsigned edpt_max_packet_size(unsigned num)
{
  LINK_REG->PIPESEL = num;
  return LINK_REG->PIPEMAXP_b.MXPS;
}

static inline void pipe_wait_for_ready(unsigned num)
{
  while (LINK_REG->D0FIFOSEL_b.CURPIPE != num) ;
  while (!LINK_REG->D0FIFOCTR_b.FRDY) ;
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
  const unsigned vld = LINK_REG->CFIFOCTR_b.DTLN;
  const unsigned len = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf = pipe->buf;
  if (len) {
    LINK_REG->DCPCTR = LINK_REG_PIPE_CTR_PID_NAK;
    pipe_read_packet(buf, (volatile void*)&LINK_REG->CFIFO, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps)
    LINK_REG->CFIFOCTR = LINK_REG_CFIFOCTR_BCLR_Msk;
  pipe->remaining = rem - len;
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return true;
  }
  LINK_REG->DCPCTR = LINK_REG_PIPE_CTR_PID_BUF;
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
    pipe_write_packet(buf, (volatile void*)&LINK_REG->CFIFO, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps)
    LINK_REG->CFIFOCTR = LINK_REG_CFIFOCTR_BVAL_Msk;
  pipe->remaining = rem - len;
  return false;
}

static bool pipe_xfer_in(unsigned num)
{
  pipe_state_t  *pipe = &_hcd.pipe[num];
  const unsigned rem  = pipe->remaining;

  LINK_REG->D0FIFOSEL = num | LINK_REG_FIFOSEL_MBW_8BIT;
  const unsigned mps  = edpt_max_packet_size(num);
  pipe_wait_for_ready(num);
  const unsigned vld  = LINK_REG->D0FIFOCTR_b.DTLN;
  const unsigned len  = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf  = pipe->buf;
  if (len) {
    pipe_read_packet(buf, (volatile void*)&LINK_REG->D0FIFO, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps)
    LINK_REG->D0FIFOCTR = LINK_REG_CFIFOCTR_BCLR_Msk;
  LINK_REG->D0FIFOSEL = 0;
  while (LINK_REG->D0FIFOSEL_b.CURPIPE) ; /* if CURPIPE bits changes, check written value */
  pipe->remaining = rem - len;
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

  LINK_REG->D0FIFOSEL = num | LINK_REG_FIFOSEL_MBW_16BIT | (TU_BYTE_ORDER == TU_BIG_ENDIAN ? LINK_REG_FIFOSEL_BIGEND : 0);
  const unsigned mps  = edpt_max_packet_size(num);
  pipe_wait_for_ready(num);
  const unsigned len  = TU_MIN(rem, mps);
  void          *buf  = pipe->buf;
  if (len) {
    pipe_write_packet(buf, (volatile void*)&LINK_REG->D0FIFO, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps)
    LINK_REG->D0FIFOCTR = LINK_REG_CFIFOCTR_BVAL_Msk;
  LINK_REG->D0FIFOSEL = 0;
  while (LINK_REG->D0FIFOSEL_b.CURPIPE) ; /* if CURPIPE bits changes, check written value */
  pipe->remaining = rem - len;
  return false;
}

static bool process_pipe0_xfer(uint8_t dev_addr, uint8_t ep_addr, void* buffer, uint16_t buflen)
{
  (void)dev_addr;
  const unsigned dir_in = tu_edpt_dir(ep_addr);

  /* configure fifo direction and access unit settings */
  if (dir_in) { /* IN, a byte */
    LINK_REG->CFIFOSEL = LINK_REG_FIFOSEL_MBW_8BIT;
    while (LINK_REG->CFIFOSEL & LINK_REG_CFIFOSEL_ISEL_WRITE) ;
  } else { /* OUT, 2 bytes */
    LINK_REG->CFIFOSEL = LINK_REG_CFIFOSEL_ISEL_WRITE | LINK_REG_FIFOSEL_MBW_16BIT |
             (TU_BYTE_ORDER == TU_BIG_ENDIAN ? LINK_REG_FIFOSEL_BIGEND : 0);
    while (!(LINK_REG->CFIFOSEL & LINK_REG_CFIFOSEL_ISEL_WRITE)) ;
  }

  pipe_state_t *pipe = &_hcd.pipe[0];
  pipe->ep        = ep_addr;
  pipe->length    = buflen;
  pipe->remaining = buflen;
  if (buflen) {
    pipe->buf     = buffer;
    if (!dir_in) { /* OUT */
      TU_ASSERT(LINK_REG->DCPCTR_b.BSTS && (LINK_REG->USBREQ & 0x80));
      pipe0_xfer_out();
    }
  } else { /* ZLP */
    pipe->buf        = NULL;
    if (!dir_in) { /* OUT */
      LINK_REG->CFIFOCTR = LINK_REG_CFIFOCTR_BVAL_Msk;
    }
    if (dir_in == LINK_REG->DCPCFG_b.DIR) {
      TU_ASSERT(LINK_REG_PIPE_CTR_PID_NAK == LINK_REG->DCPCTR_b.PID);
      LINK_REG->DCPCTR_b.SQSET = 1;
      LINK_REG->DCPCFG_b.DIR = dir_in ^ 1;
    }
  }
  LINK_REG->DCPCTR = LINK_REG_PIPE_CTR_PID_BUF;
  return true;
}

static bool process_pipe_xfer(uint8_t dev_addr, uint8_t ep_addr, void *buffer, uint16_t buflen)
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
      LINK_REG->D0FIFOSEL = num;
      pipe_wait_for_ready(num);
      LINK_REG->D0FIFOCTR = LINK_REG_CFIFOCTR_BVAL_Msk;
      LINK_REG->D0FIFOSEL = 0;
      while (LINK_REG->D0FIFOSEL_b.CURPIPE) continue; /* if CURPIPE bits changes, check written value */
    }
  } else {
    volatile uint16_t     *ctr = get_pipectr(num);
    volatile reg_pipetre_t *pt = get_pipetre(num);
    if (pt) {
      const unsigned     mps = edpt_max_packet_size(num);
      if (*ctr & 0x3) *ctr = LINK_REG_PIPE_CTR_PID_NAK;
      pt->TRE   = TU_BIT(8);
      pt->TRN   = (buflen + mps - 1) / mps;
      pt->TRENB = 1;
    }
    *ctr = LINK_REG_PIPE_CTR_PID_BUF;
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
  switch (*ctr & LINK_REG_PIPE_CTR_PID_Msk) {
    default: return;
    case LINK_REG_PIPE_CTR_PID_STALL: result = XFER_RESULT_STALLED; break;
    case LINK_REG_PIPE_CTR_PID_NAK:   result = XFER_RESULT_FAILED;  break;
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

  LINK_REG->SYSCFG_b.SCKE = 1;
  while (!LINK_REG->SYSCFG_b.SCKE) ;
  LINK_REG->SYSCFG_b.DPRPU = 0;
  LINK_REG->SYSCFG_b.DRPD = 0;
  LINK_REG->SYSCFG_b.DCFM = 1;

  LINK_REG->DVSTCTR0_b.VBUSEN = 1;

  LINK_REG->SYSCFG_b.DRPD = 1;
  for (volatile int i = 0; i < 30000; ++i) ;
  LINK_REG->SYSCFG_b.USBE = 1;

  // MCU specific PHY init
  link_phy_init();

  LINK_REG->PHYSLEW = 0x5;
  LINK_REG->DPUSR0R_FS_b.FIXPHY0 = 0u; /* Transceiver Output fixed */

  /* Setup default control pipe */
  LINK_REG->DCPCFG = LINK_REG_PIPECFG_SHTNAK_Msk;
  LINK_REG->DCPMAXP = 64;
  LINK_REG->INTENB0 = LINK_REG_INTSTS0_BRDY_Msk | LINK_REG_INTSTS0_NRDY_Msk | LINK_REG_INTSTS0_BEMP_Msk;
  LINK_REG->INTENB1 = LINK_REG_INTSTS1_SACK_Msk | LINK_REG_INTSTS1_SIGN_Msk | LINK_REG_INTSTS1_ATTCH_Msk | LINK_REG_INTSTS1_DTCH_Msk;
  LINK_REG->BEMPENB = 1;
  LINK_REG->NRDYENB = 1;
  LINK_REG->BRDYENB = 1;

  return true;
}

void hcd_int_enable(uint8_t rhport)
{
  link_int_enable(rhport);
}

void hcd_int_disable(uint8_t rhport)
{
  link_int_disable(rhport);
}

uint32_t hcd_frame_number(uint8_t rhport)
{
  (void)rhport;
  /* The device must be reset at least once after connection
   * in order to start the frame counter. */
  if (_hcd.need_reset) hcd_port_reset(rhport);
  return LINK_REG->FRMNUM_b.FRNM;
}

/*--------------------------------------------------------------------+
 * Port API
 *--------------------------------------------------------------------+*/
bool hcd_port_connect_status(uint8_t rhport)
{
  (void)rhport;
  return LINK_REG->INTSTS1_b.ATTCH ? true : false;
}

void hcd_port_reset(uint8_t rhport)
{
  LINK_REG->DCPCTR = LINK_REG_PIPE_CTR_PID_NAK;
  while (LINK_REG->DCPCTR_b.PBUSY) ;
  hcd_int_disable(rhport);
  LINK_REG->DVSTCTR0_b.UACT = 0;
  if (LINK_REG->DCPCTR_b.SUREQ)
    LINK_REG->DCPCTR_b.SUREQCLR = 1;
  hcd_int_enable(rhport);
  /* Reset should be asserted 10-20ms. */
  LINK_REG->DVSTCTR0_b.USBRST = 1;
  for (volatile int i = 0; i < 2400000; ++i) ;
  LINK_REG->DVSTCTR0_b.USBRST = 0;
  LINK_REG->DVSTCTR0_b.UACT = 1;
  _hcd.need_reset = false;
}

void hcd_port_reset_end(uint8_t rhport)
{
  (void) rhport;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
  (void)rhport;
  switch (LINK_REG->DVSTCTR0_b.RHST) {
    default: return TUSB_SPEED_INVALID;
    case LINK_REG_DVSTCTR0_RHST_FS: return TUSB_SPEED_FULL;
    case LINK_REG_DVSTCTR0_RHST_LS:  return TUSB_SPEED_LOW;
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

    ctr = (uint16_t volatile*)&LINK_REG->PIPE_CTR[num - 1];
    *ctr = 0;
    LINK_REG->NRDYENB &= ~TU_BIT(num);
    LINK_REG->BRDYENB &= ~TU_BIT(num);
    LINK_REG->PIPESEL = num;
    LINK_REG->PIPECFG = 0;
    LINK_REG->PIPEMAXP = 0;

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
  //  TU_LOG1("S %d %x\n", dev_addr, LINK_REG->DCPCTR);

  TU_ASSERT(dev_addr < 6); /* USBa can only handle addresses from 0 to 5. */
  TU_ASSERT(0 == LINK_REG->DCPCTR_b.SUREQ);

  LINK_REG->DCPCTR = LINK_REG_PIPE_CTR_PID_NAK;

  _hcd.pipe[0].buf = NULL;
  _hcd.pipe[0].length = 8;
  _hcd.pipe[0].remaining = 0;
  _hcd.pipe[0].dev = dev_addr;

  while (LINK_REG->DCPCTR_b.PBUSY) ;
  LINK_REG->DCPMAXP = (dev_addr << 12) | _hcd.ctl_mps[dev_addr];

  /* Set direction in advance for DATA stage */
  uint8_t const bmRequesttype = setup_packet[0];
  LINK_REG->DCPCFG_b.DIR = tu_edpt_dir(bmRequesttype) ? 0: 1;

  uint16_t const* p = (uint16_t const*)(uintptr_t)&setup_packet[0];
  LINK_REG->USBREQ = tu_htole16(p[0]);
  LINK_REG->USBVAL = p[1];
  LINK_REG->USBINDX = p[2];
  LINK_REG->USBLENG = p[3];

  LINK_REG->DCPCTR_b.SUREQ = 1;
  return true;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const *ep_desc)
{
  (void)rhport;
  TU_ASSERT(dev_addr < 6); /* USBa can only handle addresses from 0 to 5. */

  const unsigned ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn     = tu_edpt_number(ep_addr);
  const unsigned mps     = tu_edpt_packet_size(ep_desc);
  if (0 == epn) {
    LINK_REG->DCPCTR = LINK_REG_PIPE_CTR_PID_NAK;
    hcd_devtree_info_t devtree;
    hcd_devtree_get_info(dev_addr, &devtree);
    uint16_t volatile *devadd = (uint16_t volatile *)(uintptr_t) &LINK_REG->DEVADD[0];
    devadd += dev_addr;
    while (LINK_REG->DCPCTR_b.PBUSY) ;
    LINK_REG->DCPMAXP = (dev_addr << 12) | mps;
    *devadd = (TUSB_SPEED_FULL == devtree.speed) ? LINK_REG_DEVADD_USBSPD_FS : LINK_REG_DEVADD_USBSPD_LS;
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
  LINK_REG->PIPESEL = num;
  LINK_REG->PIPEMAXP = (dev_addr << 12) | mps;
  volatile uint16_t *ctr = get_pipectr(num);
  *ctr = LINK_REG_PIPE_CTR_ACLRM_Msk | LINK_REG_PIPE_CTR_SQCLR_Msk;
  *ctr = 0;
  unsigned cfg = ((1 ^ dir_in) << 4) | epn;
  if (xfer == TUSB_XFER_BULK) {
    cfg |= LINK_REG_PIPECFG_TYPE_BULK | LINK_REG_PIPECFG_SHTNAK_Msk | LINK_REG_PIPECFG_DBLB_Msk;
  } else if (xfer == TUSB_XFER_INTERRUPT) {
    cfg |= LINK_REG_PIPECFG_TYPE_ISO;
  } else {
    cfg |= LINK_REG_PIPECFG_TYPE_INT | LINK_REG_PIPECFG_DBLB_Msk;
  }
  LINK_REG->PIPECFG = cfg;
  LINK_REG->BRDYSTS = 0x1FFu ^ TU_BIT(num);
  LINK_REG->NRDYENB |= TU_BIT(num);
  LINK_REG->BRDYENB |= TU_BIT(num);
  if (!dir_in) {
    *ctr = LINK_REG_PIPE_CTR_PID_BUF;
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
  *ctr = LINK_REG_PIPE_CTR_SQCLR_Msk;
  unsigned const epn = tu_edpt_number(ep_addr);
  if (!epn) return true;

  if (!tu_edpt_dir(ep_addr)) { /* OUT */
    *ctr = LINK_REG_PIPE_CTR_PID_BUF;
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

  unsigned is1 = LINK_REG->INTSTS1;
  unsigned is0 = LINK_REG->INTSTS0;
  /* clear active bits except VALID (don't write 0 to already cleared bits according to the HW manual) */
  LINK_REG->INTSTS1 = ~((LINK_REG_INTSTS1_SACK_Msk | LINK_REG_INTSTS1_SIGN_Msk | LINK_REG_INTSTS1_ATTCH_Msk | LINK_REG_INTSTS1_DTCH_Msk) & is1);
  LINK_REG->INTSTS0 = ~((LINK_REG_INTSTS0_BRDY_Msk | LINK_REG_INTSTS0_NRDY_Msk | LINK_REG_INTSTS0_BEMP_Msk) & is0);
  // TU_LOG1("IS %04x %04x\n", is0, is1);
  is1 &= LINK_REG->INTENB1;
  is0 &= LINK_REG->INTENB0;

  if (is1 & LINK_REG_INTSTS1_SACK_Msk) {
    /* Set DATA1 in advance for the next transfer. */
    LINK_REG->DCPCTR_b.SQSET = 1;
    hcd_event_xfer_complete(
      LINK_REG->DCPMAXP_b.DEVSEL, tu_edpt_addr(0, TUSB_DIR_OUT), 8, XFER_RESULT_SUCCESS, true);
  }
  if (is1 & LINK_REG_INTSTS1_SIGN_Msk) {
    hcd_event_xfer_complete(
      LINK_REG->DCPMAXP_b.DEVSEL, tu_edpt_addr(0, TUSB_DIR_OUT), 8, XFER_RESULT_FAILED, true);
  }
  if (is1 & LINK_REG_INTSTS1_ATTCH_Msk) {
    LINK_REG->DVSTCTR0_b.UACT = 1;
    _hcd.need_reset = true;
    LINK_REG->INTENB1 = (LINK_REG->INTENB1 & ~LINK_REG_INTSTS1_ATTCH_Msk) | LINK_REG_INTSTS1_DTCH_Msk;
    hcd_event_device_attach(rhport, true);
  }
  if (is1 & LINK_REG_INTSTS1_DTCH_Msk) {
    LINK_REG->DVSTCTR0_b.UACT = 0;
    if (LINK_REG->DCPCTR_b.SUREQ)
      LINK_REG->DCPCTR_b.SUREQCLR = 1;
    LINK_REG->INTENB1 = (LINK_REG->INTENB1 & ~LINK_REG_INTSTS1_DTCH_Msk) | LINK_REG_INTSTS1_ATTCH_Msk;
    hcd_event_device_remove(rhport, true);
  }

  if (is0 & LINK_REG_INTSTS0_BEMP_Msk) {
    const unsigned s = LINK_REG->BEMPSTS;
    LINK_REG->BEMPSTS = 0;
    if (s & 1) {
      process_pipe0_bemp(rhport);
    }
  }
  if (is0 & LINK_REG_INTSTS0_NRDY_Msk) {
    const unsigned m = LINK_REG->NRDYENB;
    unsigned s = LINK_REG->NRDYSTS & m;
    LINK_REG->NRDYSTS = ~s;
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
  if (is0 & LINK_REG_INTSTS0_BRDY_Msk) {
    const unsigned m = LINK_REG->BRDYENB;
    unsigned s = LINK_REG->BRDYSTS & m;
    /* clear active bits (don't write 0 to already cleared bits according to the HW manual) */
    LINK_REG->BRDYSTS = ~s;
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
