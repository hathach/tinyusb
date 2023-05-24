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

#if CFG_TUH_ENABLED && (TU_CHECK_MCU(OPT_MCU_RX63X, OPT_MCU_RX65X, OPT_MCU_RX72N) || \
                        TU_CHECK_MCU(OPT_MCU_RAXXX))

#include "host/hcd.h"
#include "rusb2_type.h"

#if TU_CHECK_MCU(OPT_MCU_RX63X, OPT_MCU_RX65X, OPT_MCU_RX72N)
  #include "rusb2_rx.h"
#elif TU_CHECK_MCU(OPT_MCU_RAXXX)
  #include "rusb2_ra.h"
#else
  #error "Unsupported MCU"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

/* LINK core registers */
#if defined(__CCRX__)
  #define RUSB2 ((RUSB2_REG_t __evenaccess*) RUSB2_REG_BASE)
#else
  #define RUSB2 ((RUSB2_REG_t*) RUSB2_REG_BASE)
#endif

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
    return (volatile uint16_t*)&(RUSB2->PIPE_CTR[num - 1]);
  } else {
    return (volatile uint16_t*)&(RUSB2->DCPCTR);
  }
}

static volatile reg_pipetre_t* get_pipetre(unsigned num)
{
  volatile reg_pipetre_t* tre = NULL;
  if ((1 <= num) && (num <= 5)) {
    tre = (volatile reg_pipetre_t*)&(RUSB2->PIPE_TR[num - 1].E);
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
  return RUSB2->DCPMAXP_b.MXPS;
}

static unsigned edpt_max_packet_size(unsigned num)
{
  RUSB2->PIPESEL = num;
  return RUSB2->PIPEMAXP_b.MXPS;
}

static inline void pipe_wait_for_ready(unsigned num)
{
  while (RUSB2->D0FIFOSEL_b.CURPIPE != num) ;
  while (!RUSB2->D0FIFOCTR_b.FRDY) ;
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
  const unsigned vld = RUSB2->CFIFOCTR_b.DTLN;
  const unsigned len = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf = pipe->buf;
  if (len) {
    RUSB2->DCPCTR = RUSB2_PIPE_CTR_PID_NAK;
    pipe_read_packet(buf, (volatile void*)&RUSB2->CFIFO, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps) {
    RUSB2->CFIFOCTR = RUSB2_CFIFOCTR_BCLR_Msk;
  }
  pipe->remaining = rem - len;
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return true;
  }
  RUSB2->DCPCTR = RUSB2_PIPE_CTR_PID_BUF;
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
    pipe_write_packet(buf, (volatile void*)&RUSB2->CFIFO, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps) {
    RUSB2->CFIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;
  }
  pipe->remaining = rem - len;
  return false;
}

static bool pipe_xfer_in(unsigned num)
{
  pipe_state_t  *pipe = &_hcd.pipe[num];
  const unsigned rem  = pipe->remaining;

  RUSB2->D0FIFOSEL = num | RUSB2_FIFOSEL_MBW_8BIT;
  const unsigned mps  = edpt_max_packet_size(num);
  pipe_wait_for_ready(num);
  const unsigned vld  = RUSB2->D0FIFOCTR_b.DTLN;
  const unsigned len  = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf  = pipe->buf;
  if (len) {
    pipe_read_packet(buf, (volatile void*)&RUSB2->D0FIFO, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps) {
    RUSB2->D0FIFOCTR = RUSB2_CFIFOCTR_BCLR_Msk;
  }
  RUSB2->D0FIFOSEL = 0;
  while (RUSB2->D0FIFOSEL_b.CURPIPE) ; /* if CURPIPE bits changes, check written value */
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

  RUSB2->D0FIFOSEL = num | RUSB2_FIFOSEL_MBW_16BIT | (TU_BYTE_ORDER == TU_BIG_ENDIAN ? RUSB2_FIFOSEL_BIGEND : 0);
  const unsigned mps  = edpt_max_packet_size(num);
  pipe_wait_for_ready(num);
  const unsigned len  = TU_MIN(rem, mps);
  void          *buf  = pipe->buf;
  if (len) {
    pipe_write_packet(buf, (volatile void*)&RUSB2->D0FIFO, len);
    pipe->buf = (uint8_t*)buf + len;
  }
  if (len < mps)
    RUSB2->D0FIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;
  RUSB2->D0FIFOSEL = 0;
  while (RUSB2->D0FIFOSEL_b.CURPIPE) ; /* if CURPIPE bits changes, check written value */
  pipe->remaining = rem - len;
  return false;
}

static bool process_pipe0_xfer(uint8_t dev_addr, uint8_t ep_addr, void* buffer, uint16_t buflen)
{
  (void)dev_addr;
  const unsigned dir_in = tu_edpt_dir(ep_addr);

  /* configure fifo direction and access unit settings */
  if (dir_in) { /* IN, a byte */
    RUSB2->CFIFOSEL = RUSB2_FIFOSEL_MBW_8BIT;
    while (RUSB2->CFIFOSEL & RUSB2_CFIFOSEL_ISEL_WRITE) ;
  } else { /* OUT, 2 bytes */
    RUSB2->CFIFOSEL = RUSB2_CFIFOSEL_ISEL_WRITE | RUSB2_FIFOSEL_MBW_16BIT |
                         (TU_BYTE_ORDER == TU_BIG_ENDIAN ? RUSB2_FIFOSEL_BIGEND : 0);
    while (!(RUSB2->CFIFOSEL & RUSB2_CFIFOSEL_ISEL_WRITE)) ;
  }

  pipe_state_t *pipe = &_hcd.pipe[0];
  pipe->ep        = ep_addr;
  pipe->length    = buflen;
  pipe->remaining = buflen;
  if (buflen) {
    pipe->buf     = buffer;
    if (!dir_in) { /* OUT */
      TU_ASSERT(RUSB2->DCPCTR_b.BSTS && (RUSB2->USBREQ & 0x80));
      pipe0_xfer_out();
    }
  } else { /* ZLP */
    pipe->buf        = NULL;
    if (!dir_in) { /* OUT */
      RUSB2->CFIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;
    }
    if (dir_in == RUSB2->DCPCFG_b.DIR) {
      TU_ASSERT(RUSB2_PIPE_CTR_PID_NAK == RUSB2->DCPCTR_b.PID);
      RUSB2->DCPCTR_b.SQSET = 1;
      RUSB2->DCPCFG_b.DIR = dir_in ^ 1;
    }
  }
  RUSB2->DCPCTR = RUSB2_PIPE_CTR_PID_BUF;
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
      RUSB2->D0FIFOSEL = num;
      pipe_wait_for_ready(num);
      RUSB2->D0FIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;
      RUSB2->D0FIFOSEL = 0;
      while (RUSB2->D0FIFOSEL_b.CURPIPE) {} /* if CURPIPE bits changes, check written value */
    }
  } else {
    volatile uint16_t     *ctr = get_pipectr(num);
    volatile reg_pipetre_t *pt = get_pipetre(num);
    if (pt) {
      const unsigned     mps = edpt_max_packet_size(num);
      if (*ctr & 0x3) *ctr = RUSB2_PIPE_CTR_PID_NAK;
      pt->TRE   = TU_BIT(8);
      pt->TRN   = (buflen + mps - 1) / mps;
      pt->TRENB = 1;
    }
    *ctr = RUSB2_PIPE_CTR_PID_BUF;
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
  xfer_result_t result;
  uint16_t volatile *ctr = get_pipectr(num);
  // TU_LOG1("NRDY %d %x\n", num, *ctr);
  switch (*ctr & RUSB2_PIPE_CTR_PID_Msk) {
    default: return;
    case RUSB2_PIPE_CTR_PID_STALL: result = XFER_RESULT_STALLED; break;
    case RUSB2_PIPE_CTR_PID_NAK:   result = XFER_RESULT_FAILED;  break;
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

#if 0 // previously present in the rx driver before generalization
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
#endif

bool hcd_init(uint8_t rhport)
{
  (void)rhport;

#if 0 // previously present in the rx driver before generalization
  uint32_t pswi = disable_interrupt();
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY | SYSTEM_PRCR_PRC1;
  MSTP(USB0) = 0;
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY;
  enable_interrupt(pswi);
#endif

  RUSB2->SYSCFG_b.SCKE = 1;
  while (!RUSB2->SYSCFG_b.SCKE) ;
  RUSB2->SYSCFG_b.DPRPU = 0;
  RUSB2->SYSCFG_b.DRPD = 0;
  RUSB2->SYSCFG_b.DCFM = 1;

  RUSB2->DVSTCTR0_b.VBUSEN = 1;

  RUSB2->SYSCFG_b.DRPD = 1;
  for (volatile int i = 0; i < 30000; ++i) ;
  RUSB2->SYSCFG_b.USBE = 1;

  // MCU specific PHY init
  rusb2_phy_init();

  RUSB2->PHYSLEW = 0x5;
  RUSB2->DPUSR0R_FS_b.FIXPHY0 = 0u; /* Transceiver Output fixed */

  /* Setup default control pipe */
  RUSB2->DCPCFG  = RUSB2_PIPECFG_SHTNAK_Msk;
  RUSB2->DCPMAXP = 64;
  RUSB2->INTENB0 = RUSB2_INTSTS0_BRDY_Msk | RUSB2_INTSTS0_NRDY_Msk | RUSB2_INTSTS0_BEMP_Msk;
  RUSB2->INTENB1 = RUSB2_INTSTS1_SACK_Msk | RUSB2_INTSTS1_SIGN_Msk | RUSB2_INTSTS1_ATTCH_Msk | RUSB2_INTSTS1_DTCH_Msk;
  RUSB2->BEMPENB = 1;
  RUSB2->NRDYENB = 1;
  RUSB2->BRDYENB = 1;

  return true;
}

void hcd_int_enable(uint8_t rhport)
{
  rusb2_int_enable(rhport);
}

void hcd_int_disable(uint8_t rhport)
{
  rusb2_int_disable(rhport);
}

uint32_t hcd_frame_number(uint8_t rhport)
{
  (void)rhport;
  /* The device must be reset at least once after connection
   * in order to start the frame counter. */
  if (_hcd.need_reset) hcd_port_reset(rhport);
  return RUSB2->FRMNUM_b.FRNM;
}

/*--------------------------------------------------------------------+
 * Port API
 *--------------------------------------------------------------------+*/
bool hcd_port_connect_status(uint8_t rhport)
{
  (void)rhport;
  return RUSB2->INTSTS1_b.ATTCH ? true : false;
}

void hcd_port_reset(uint8_t rhport)
{
  RUSB2->DCPCTR = RUSB2_PIPE_CTR_PID_NAK;
  while (RUSB2->DCPCTR_b.PBUSY) ;
  hcd_int_disable(rhport);
  RUSB2->DVSTCTR0_b.UACT = 0;
  if (RUSB2->DCPCTR_b.SUREQ) {
    RUSB2->DCPCTR_b.SUREQCLR = 1;
  }
  hcd_int_enable(rhport);
  /* Reset should be asserted 10-20ms. */
  RUSB2->DVSTCTR0_b.USBRST = 1;
  for (volatile int i = 0; i < 2400000; ++i) ;
  RUSB2->DVSTCTR0_b.USBRST = 0;
  RUSB2->DVSTCTR0_b.UACT = 1;
  _hcd.need_reset = false;
}

void hcd_port_reset_end(uint8_t rhport)
{
  (void) rhport;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
  (void)rhport;
  switch (RUSB2->DVSTCTR0_b.RHST) {
    default: return TUSB_SPEED_INVALID;
    case RUSB2_DVSTCTR0_RHST_FS: return TUSB_SPEED_FULL;
    case RUSB2_DVSTCTR0_RHST_LS:  return TUSB_SPEED_LOW;
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

    ctr = (uint16_t volatile*)&RUSB2->PIPE_CTR[num - 1];
    *ctr = 0;
    RUSB2->NRDYENB &= ~TU_BIT(num);
    RUSB2->BRDYENB &= ~TU_BIT(num);
    RUSB2->PIPESEL = num;
    RUSB2->PIPECFG = 0;
    RUSB2->PIPEMAXP = 0;

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
  //  TU_LOG1("S %d %x\n", dev_addr, RUSB2->DCPCTR);

  TU_ASSERT(dev_addr < 6); /* USBa can only handle addresses from 0 to 5. */
  TU_ASSERT(0 == RUSB2->DCPCTR_b.SUREQ);

  RUSB2->DCPCTR = RUSB2_PIPE_CTR_PID_NAK;

  _hcd.pipe[0].buf       = NULL;
  _hcd.pipe[0].length    = 8;
  _hcd.pipe[0].remaining = 0;
  _hcd.pipe[0].dev       = dev_addr;

  while (RUSB2->DCPCTR_b.PBUSY) ;
  RUSB2->DCPMAXP = (dev_addr << 12) | _hcd.ctl_mps[dev_addr];

  /* Set direction in advance for DATA stage */
  uint8_t const bmRequesttype = setup_packet[0];
  RUSB2->DCPCFG_b.DIR = tu_edpt_dir(bmRequesttype) ? 0: 1;

  uint16_t const* p = (uint16_t const*)(uintptr_t)&setup_packet[0];
  RUSB2->USBREQ  = tu_htole16(p[0]);
  RUSB2->USBVAL  = p[1];
  RUSB2->USBINDX = p[2];
  RUSB2->USBLENG = p[3];

  RUSB2->DCPCTR_b.SUREQ = 1;
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
    RUSB2->DCPCTR = RUSB2_PIPE_CTR_PID_NAK;
    hcd_devtree_info_t devtree;
    hcd_devtree_get_info(dev_addr, &devtree);
    uint16_t volatile *devadd = (uint16_t volatile *)(uintptr_t) &RUSB2->DEVADD[0];
    devadd += dev_addr;
    while (RUSB2->DCPCTR_b.PBUSY) ;
    RUSB2->DCPMAXP = (dev_addr << 12) | mps;
    *devadd = (TUSB_SPEED_FULL == devtree.speed) ? RUSB2_DEVADD_USBSPD_FS : RUSB2_DEVADD_USBSPD_LS;
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
  RUSB2->PIPESEL = num;
  RUSB2->PIPEMAXP = (dev_addr << 12) | mps;
  volatile uint16_t *ctr = get_pipectr(num);
  *ctr = RUSB2_PIPE_CTR_ACLRM_Msk | RUSB2_PIPE_CTR_SQCLR_Msk;
  *ctr = 0;
  unsigned cfg = ((1 ^ dir_in) << 4) | epn;
  if (xfer == TUSB_XFER_BULK) {
    cfg |= RUSB2_PIPECFG_TYPE_BULK | RUSB2_PIPECFG_SHTNAK_Msk | RUSB2_PIPECFG_DBLB_Msk;
  } else if (xfer == TUSB_XFER_INTERRUPT) {
    cfg |= RUSB2_PIPECFG_TYPE_INT;
  } else {
    cfg |= RUSB2_PIPECFG_TYPE_ISO | RUSB2_PIPECFG_DBLB_Msk;
  }
  RUSB2->PIPECFG = cfg;
  RUSB2->BRDYSTS = 0x1FFu ^ TU_BIT(num);
  RUSB2->NRDYENB |= TU_BIT(num);
  RUSB2->BRDYENB |= TU_BIT(num);
  if (!dir_in) {
    *ctr = RUSB2_PIPE_CTR_PID_BUF;
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
  *ctr = RUSB2_PIPE_CTR_SQCLR_Msk;
  unsigned const epn = tu_edpt_number(ep_addr);
  if (!epn) return true;

  if (!tu_edpt_dir(ep_addr)) { /* OUT */
    *ctr = RUSB2_PIPE_CTR_PID_BUF;
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

  unsigned is1 = RUSB2->INTSTS1;
  unsigned is0 = RUSB2->INTSTS0;
  /* clear active bits except VALID (don't write 0 to already cleared bits according to the HW manual) */
  RUSB2->INTSTS1 = ~((RUSB2_INTSTS1_SACK_Msk | RUSB2_INTSTS1_SIGN_Msk | RUSB2_INTSTS1_ATTCH_Msk | RUSB2_INTSTS1_DTCH_Msk) & is1);
  RUSB2->INTSTS0 = ~((RUSB2_INTSTS0_BRDY_Msk | RUSB2_INTSTS0_NRDY_Msk | RUSB2_INTSTS0_BEMP_Msk) & is0);
  // TU_LOG1("IS %04x %04x\n", is0, is1);
  is1 &= RUSB2->INTENB1;
  is0 &= RUSB2->INTENB0;

  if (is1 & RUSB2_INTSTS1_SACK_Msk) {
    /* Set DATA1 in advance for the next transfer. */
    RUSB2->DCPCTR_b.SQSET = 1;
    hcd_event_xfer_complete(RUSB2->DCPMAXP_b.DEVSEL, tu_edpt_addr(0, TUSB_DIR_OUT), 8, XFER_RESULT_SUCCESS, true);
  }
  if (is1 & RUSB2_INTSTS1_SIGN_Msk) {
    hcd_event_xfer_complete(RUSB2->DCPMAXP_b.DEVSEL, tu_edpt_addr(0, TUSB_DIR_OUT), 8, XFER_RESULT_FAILED, true);
  }
  if (is1 & RUSB2_INTSTS1_ATTCH_Msk) {
    RUSB2->DVSTCTR0_b.UACT = 1;
    _hcd.need_reset = true;
    RUSB2->INTENB1 = (RUSB2->INTENB1 & ~RUSB2_INTSTS1_ATTCH_Msk) | RUSB2_INTSTS1_DTCH_Msk;
    hcd_event_device_attach(rhport, true);
  }
  if (is1 & RUSB2_INTSTS1_DTCH_Msk) {
    RUSB2->DVSTCTR0_b.UACT = 0;
    if (RUSB2->DCPCTR_b.SUREQ) {
      RUSB2->DCPCTR_b.SUREQCLR = 1;
    }
    RUSB2->INTENB1 = (RUSB2->INTENB1 & ~RUSB2_INTSTS1_DTCH_Msk) | RUSB2_INTSTS1_ATTCH_Msk;
    hcd_event_device_remove(rhport, true);
  }

  if (is0 & RUSB2_INTSTS0_BEMP_Msk) {
    const unsigned s = RUSB2->BEMPSTS;
    RUSB2->BEMPSTS = 0;
    if (s & 1) {
      process_pipe0_bemp(rhport);
    }
  }
  if (is0 & RUSB2_INTSTS0_NRDY_Msk) {
    const unsigned m = RUSB2->NRDYENB;
    unsigned s = RUSB2->NRDYSTS & m;
    RUSB2->NRDYSTS = ~s;
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
  if (is0 & RUSB2_INTSTS0_BRDY_Msk) {
    const unsigned m = RUSB2->BRDYENB;
    unsigned s = RUSB2->BRDYSTS & m;
    /* clear active bits (don't write 0 to already cleared bits according to the HW manual) */
    RUSB2->BRDYSTS = ~s;
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
