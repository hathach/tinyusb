/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji KITAYAMA
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

#if CFG_TUD_ENABLED && \
  TU_CHECK_MCU(OPT_MCU_MSP432E4, OPT_MCU_TM4C123, OPT_MCU_TM4C129)

#if __GNUC__ > 8 && defined(__ARM_FEATURE_UNALIGNED)
/* GCC warns that an address may be unaligned, even though
 * the target CPU has the capability for unaligned memory access. */
_Pragma("GCC diagnostic ignored \"-Waddress-of-packed-member\"");
#endif

#include "device/dcd.h"

#if TU_CHECK_MCU(OPT_MCU_MSP432E4)
  #include "musb_msp432e.h"

#elif TU_CHECK_MCU(OPT_MCU_TM4C123, OPT_MCU_TM4C129)
  #include "musb_tm4c.h"

  // HACK generalize later
  #include "musb_type.h"
  #define FIFO0_WORD FIFO0
  #define FIFO1_WORD FIFO1

#else
  #error "Unsupported MCUs"
#endif

/*------------------------------------------------------------------
 * MACRO TYPEDEF CONSTANT ENUM DECLARATION
 *------------------------------------------------------------------*/
#define REQUEST_TYPE_INVALID  (0xFFu)

typedef struct {
  uint_fast16_t beg; /* offset of including first element */
  uint_fast16_t end; /* offset of excluding the last element */
} free_block_t;

typedef struct TU_ATTR_PACKED {
  uint16_t TXMAXP;
  uint8_t  TXCSRL;
  uint8_t  TXCSRH;
  uint16_t RXMAXP;
  uint8_t  RXCSRL;
  uint8_t  RXCSRH;
  uint16_t RXCOUNT;
  uint16_t RESERVED[3];
} hw_endpoint_t;

typedef union {
  uint8_t   u8;
  uint16_t  u16;
  uint32_t  u32;
} hw_fifo_t;

typedef struct TU_ATTR_PACKED
{
  void      *buf;      /* the start address of a transfer data buffer */
  uint16_t  length;    /* the number of bytes in the buffer */
  uint16_t  remaining; /* the number of bytes remaining in the buffer */
} pipe_state_t;

typedef struct
{
  tusb_control_request_t setup_packet;
  uint16_t     remaining_ctrl; /* The number of bytes remaining in data stage of control transfer. */
  int8_t       status_out;
  pipe_state_t pipe0;
  pipe_state_t pipe[2][7];   /* pipe[direction][endpoint number - 1] */
  uint16_t     pipe_buf_is_fifo[2]; /* Bitmap. Each bit means whether 1:TU_FIFO or 0:POD. */
} dcd_data_t;

/*------------------------------------------------------------------
 * INTERNAL OBJECT & FUNCTION DECLARATION
 *------------------------------------------------------------------*/
static dcd_data_t _dcd;


static inline free_block_t *find_containing_block(free_block_t *beg, free_block_t *end, uint_fast16_t addr)
{
  free_block_t *cur = beg;
  for (; cur < end && ((addr < cur->beg) || (cur->end <= addr)); ++cur) ;
  return cur;
}

static inline int update_free_block_list(free_block_t *blks, unsigned num, uint_fast16_t addr, uint_fast16_t size)
{
  free_block_t *p = find_containing_block(blks, blks + num, addr);
  TU_ASSERT(p != blks + num, -2);
  if (p->beg == addr) {
    /* Shrink block */
    p->beg = addr + size;
    if (p->beg != p->end) return 0;
    /* remove block */
    free_block_t *end = blks + num;
    while (p + 1 < end) {
      *p = *(p + 1);
      ++p;
    }
    return -1;
  } else {
    /* Split into 2 blocks */
    free_block_t tmp = {
      .beg = addr + size,
      .end = p->end
    };
    p->end = addr;
    if (p->beg == p->end) {
      if (tmp.beg != tmp.end) {
        *p = tmp;
        return 0;
      }
      /* remove block */
      free_block_t *end = blks + num;
      while (p + 1 < end) {
        *p = *(p + 1);
        ++p;
      }
      return -1;
    }
    if (tmp.beg == tmp.end) return 0;
    blks[num] = tmp;
    return 1;
  }
}

static inline unsigned free_block_size(free_block_t const *blk)
{
  return blk->end - blk->beg;
}

#if 0
static inline void print_block_list(free_block_t const *blk, unsigned num)
{
  TU_LOG1("*************\r\n");
  for (unsigned i = 0; i < num; ++i) {
    TU_LOG1(" Blk%u %u %u\r\n", i, blk->beg, blk->end);
    ++blk;
  }
}
#else
#define print_block_list(a,b)
#endif

static unsigned find_free_memory(uint_fast16_t size_in_log2_minus3)
{
  free_block_t free_blocks[2 * (TUP_DCD_ENDPOINT_MAX - 1)];
  unsigned num_blocks = 1;

  /* Initialize free memory block list */
  free_blocks[0].beg = 64 / 8;
  free_blocks[0].end = (4 << 10) / 8; /* 4KiB / 8 bytes */
  for (int i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    uint_fast16_t addr;
    int num;
    USB0->EPIDX = i;
    addr = USB0->TXFIFOADD;
    if (addr) {
      unsigned sz  = USB0->TXFIFOSZ;
      unsigned sft = (sz & USB_TXFIFOSZ_SIZE_M) + ((sz & USB_TXFIFOSZ_DPB) ? 1: 0);
      num = update_free_block_list(free_blocks, num_blocks, addr, 1 << sft);
      TU_ASSERT(-2 < num, 0);
      num_blocks += num;
      print_block_list(free_blocks, num_blocks);
    }
    addr = USB0->RXFIFOADD;
    if (addr) {
      unsigned sz  = USB0->RXFIFOSZ;
      unsigned sft = (sz & USB_RXFIFOSZ_SIZE_M) + ((sz & USB_RXFIFOSZ_DPB) ? 1: 0);
      num = update_free_block_list(free_blocks, num_blocks, addr, 1 << sft);
      TU_ASSERT(-2 < num, 0);
      num_blocks += num;
      print_block_list(free_blocks, num_blocks);
    }
  }
  print_block_list(free_blocks, num_blocks);

  /* Find the best fit memory block */
  uint_fast16_t size_in_8byte_unit = 1 << size_in_log2_minus3;
  free_block_t const *min = NULL;
  uint_fast16_t    min_sz = 0xFFFFu;
  free_block_t const *end = &free_blocks[num_blocks];
  for (free_block_t const *cur = &free_blocks[0]; cur < end; ++cur) {
    uint_fast16_t sz = free_block_size(cur);
    if (sz < size_in_8byte_unit) continue;
    if (size_in_8byte_unit == sz) return cur->beg;
    if (sz < min_sz) min = cur;
  }
  TU_ASSERT(min, 0);
  return min->beg;
}

static inline volatile hw_endpoint_t* edpt_regs(unsigned epnum_minus1)
{
  volatile hw_endpoint_t *regs = (volatile hw_endpoint_t*)((uintptr_t)&USB0->TXMAXP1);
  return regs + epnum_minus1;
}

static void pipe_write_packet(void *buf, volatile void *fifo, unsigned len)
{
  volatile hw_fifo_t *reg = (volatile hw_fifo_t*)fifo;
  uintptr_t addr = (uintptr_t)buf;
  while (len >= 4) {
    reg->u32 = *(uint32_t const *)addr;
    addr += 4;
    len  -= 4;
  }
  if (len >= 2) {
    reg->u16 = *(uint16_t const *)addr;
    addr += 2;
    len  -= 2;
  }
  if (len) {
    reg->u8 = *(uint8_t const *)addr;
  }
}

static void pipe_read_packet(void *buf, volatile void *fifo, unsigned len)
{
  volatile hw_fifo_t *reg = (volatile hw_fifo_t*)fifo;
  uintptr_t addr = (uintptr_t)buf;
  while (len >= 4) {
    *(uint32_t *)addr = reg->u32;
    addr += 4;
    len  -= 4;
  }
  if (len >= 2) {
    *(uint16_t *)addr = reg->u16;
    addr += 2;
    len  -= 2;
  }
  if (len) {
    *(uint8_t *)addr = reg->u8;
  }
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

static void process_setup_packet(uint8_t rhport)
{
  uint32_t *p = (void*)&_dcd.setup_packet;
  p[0]        = USB0->FIFO0_WORD;
  p[1]        = USB0->FIFO0_WORD;

  _dcd.pipe0.buf       = NULL;
  _dcd.pipe0.length    = 0;
  _dcd.pipe0.remaining = 0;
  dcd_event_setup_received(rhport, (const uint8_t*)(uintptr_t)&_dcd.setup_packet, true);

  const unsigned len    = _dcd.setup_packet.wLength;
  _dcd.remaining_ctrl   = len;
  const unsigned dir_in = tu_edpt_dir(_dcd.setup_packet.bmRequestType);
  /* Clear RX FIFO and reverse the transaction direction */
  if (len && dir_in) USB0->CSRL0 = USB_CSRL0_RXRDYC;
}

static bool handle_xfer_in(uint_fast8_t ep_addr)
{
  unsigned epnum_minus1 = tu_edpt_number(ep_addr) - 1;
  pipe_state_t  *pipe = &_dcd.pipe[tu_edpt_dir(ep_addr)][epnum_minus1];
  const unsigned rem  = pipe->remaining;

  if (!rem) {
    pipe->buf = NULL;
    return true;
  }

  volatile hw_endpoint_t *regs = edpt_regs(epnum_minus1);
  const unsigned mps = regs->TXMAXP;
  const unsigned len = TU_MIN(mps, rem);
  void          *buf = pipe->buf;
  // TU_LOG1("   %p mps %d len %d rem %d\r\n", buf, mps, len, rem);
  if (len) {
    if (_dcd.pipe_buf_is_fifo[TUSB_DIR_IN] & TU_BIT(epnum_minus1)) {
      pipe_read_write_packet_ff(buf, &USB0->FIFO1_WORD + epnum_minus1, len, TUSB_DIR_IN);
    } else {
      pipe_write_packet(buf, &USB0->FIFO1_WORD + epnum_minus1, len);
      pipe->buf       = buf + len;
    }
    pipe->remaining = rem - len;
  }
  regs->TXCSRL = USB_TXCSRL1_TXRDY;
  // TU_LOG1(" TXCSRL%d = %x %d\r\n", epnum_minus1 + 1, regs->TXCSRL, rem - len);
  return false;
}

static bool handle_xfer_out(uint_fast8_t ep_addr)
{
  unsigned epnum_minus1 = tu_edpt_number(ep_addr) - 1;
  pipe_state_t  *pipe = &_dcd.pipe[tu_edpt_dir(ep_addr)][epnum_minus1];
  volatile hw_endpoint_t *regs = edpt_regs(epnum_minus1);
  // TU_LOG1(" RXCSRL%d = %x\r\n", epnum_minus1 + 1, regs->RXCSRL);

  TU_ASSERT(regs->RXCSRL & USB_RXCSRL1_RXRDY);

  const unsigned mps = regs->RXMAXP;
  const unsigned rem = pipe->remaining;
  const unsigned vld = regs->RXCOUNT;
  const unsigned len = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf = pipe->buf;
  if (len) {
    if (_dcd.pipe_buf_is_fifo[TUSB_DIR_OUT] & TU_BIT(epnum_minus1)) {
      pipe_read_write_packet_ff(buf, &USB0->FIFO1_WORD + epnum_minus1, len, TUSB_DIR_OUT);
    } else {
      pipe_read_packet(buf, &USB0->FIFO1_WORD + epnum_minus1, len);
      pipe->buf       = buf + len;
    }
    pipe->remaining = rem - len;
  }
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return NULL != buf;
  }
  regs->RXCSRL = 0; /* Clear RXRDY bit */
  return false;
}

static bool edpt_n_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes)
{
  (void)rhport;

  unsigned epnum_minus1 = tu_edpt_number(ep_addr) - 1;
  unsigned dir_in       = tu_edpt_dir(ep_addr);

  pipe_state_t *pipe = &_dcd.pipe[dir_in][epnum_minus1];
  pipe->buf          = buffer;
  pipe->length       = total_bytes;
  pipe->remaining    = total_bytes;

  if (dir_in) {
    handle_xfer_in(ep_addr);
  } else {
    volatile hw_endpoint_t *regs = edpt_regs(epnum_minus1);
    if (regs->RXCSRL & USB_RXCSRL1_RXRDY) regs->RXCSRL = 0;
  }
  return true;
}

static bool edpt0_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes)
{
  (void)rhport;
  TU_ASSERT(total_bytes <= 64); /* Current implementation supports for only up to 64 bytes. */

  const unsigned req = _dcd.setup_packet.bmRequestType;
  TU_ASSERT(req != REQUEST_TYPE_INVALID || total_bytes == 0);

  if (req == REQUEST_TYPE_INVALID || _dcd.status_out) {
    /* STATUS OUT stage.
     * MUSB controller automatically handles STATUS OUT packets without
     * software helps. We do not have to do anything. And STATUS stage
     * may have already finished and received the next setup packet
     * without calling this function, so we have no choice but to
     * invoke the callback function of status packet here. */
    // TU_LOG1(" STATUS OUT USB0->CSRL0 = %x\r\n", USB0->CSRL0);
    _dcd.status_out = 0;
    if (req == REQUEST_TYPE_INVALID) {
      dcd_event_xfer_complete(rhport, ep_addr, total_bytes, XFER_RESULT_SUCCESS, false);
    } else {
      /* The next setup packet has already been received, it aborts
       * invoking callback function to avoid confusing TUSB stack. */
      TU_LOG1("Drop CONTROL_STAGE_ACK\r\n");
    }
    return true;
  }
  const unsigned dir_in = tu_edpt_dir(ep_addr);
  if (tu_edpt_dir(req) == dir_in) { /* DATA stage */
    TU_ASSERT(total_bytes <= _dcd.remaining_ctrl);
    const unsigned rem = _dcd.remaining_ctrl;
    const unsigned len = TU_MIN(TU_MIN(rem, 64), total_bytes);
    if (dir_in) {
      pipe_write_packet(buffer, &USB0->FIFO0_WORD, len);

      _dcd.pipe0.buf       = buffer + len;
      _dcd.pipe0.length    = len;
      _dcd.pipe0.remaining = 0;

      _dcd.remaining_ctrl  = rem - len;
      if ((len < 64) || (rem == len)) {
        _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID; /* Change to STATUS/SETUP stage */
        _dcd.status_out = 1;
        /* Flush TX FIFO and reverse the transaction direction. */
        USB0->CSRL0 = USB_CSRL0_TXRDY | USB_CSRL0_DATAEND;
      } else {
        USB0->CSRL0 = USB_CSRL0_TXRDY; /* Flush TX FIFO to return ACK. */
      }
      // TU_LOG1(" IN USB0->CSRL0 = %x\r\n", USB0->CSRL0);
    } else {
      // TU_LOG1(" OUT USB0->CSRL0 = %x\r\n", USB0->CSRL0);
      _dcd.pipe0.buf       = buffer;
      _dcd.pipe0.length    = len;
      _dcd.pipe0.remaining = len;
      USB0->CSRL0 = USB_CSRL0_RXRDYC; /* Clear RX FIFO to return ACK. */
    }
  } else if (dir_in) {
    // TU_LOG1(" STATUS IN USB0->CSRL0 = %x\r\n", USB0->CSRL0);
    _dcd.pipe0.buf = NULL;
    _dcd.pipe0.length    = 0;
    _dcd.pipe0.remaining = 0;
    /* Clear RX FIFO and reverse the transaction direction */
    USB0->CSRL0 = USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND;
  }
  return true;
}

static void process_ep0(uint8_t rhport)
{
  uint_fast8_t csrl = USB0->CSRL0;

  // TU_LOG1(" EP0 USB0->CSRL0 = %x\r\n", csrl);

  if (csrl & USB_CSRL0_STALLED) {
    /* Returned STALL packet to HOST. */
    USB0->CSRL0 = 0; /* Clear STALL */
    return;
  }

  unsigned req = _dcd.setup_packet.bmRequestType;
  if (csrl & USB_CSRL0_SETEND) {
    TU_LOG1("   ABORT by the next packets\r\n");
    USB0->CSRL0 = USB_CSRL0_SETENDC;
    if (req != REQUEST_TYPE_INVALID && _dcd.pipe0.buf) {
      /* DATA stage was aborted by receiving STATUS or SETUP packet. */
      _dcd.pipe0.buf = NULL;
      _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
      dcd_event_xfer_complete(rhport,
                              req & TUSB_DIR_IN_MASK,
                              _dcd.pipe0.length - _dcd.pipe0.remaining,
                              XFER_RESULT_SUCCESS, true);
    }
    req = REQUEST_TYPE_INVALID;
    if (!(csrl & USB_CSRL0_RXRDY)) return; /* Received SETUP packet */
  }

  if (csrl & USB_CSRL0_RXRDY) {
    /* Received SETUP or DATA OUT packet */
    if (req == REQUEST_TYPE_INVALID) {
      /* SETUP */
      TU_ASSERT(sizeof(tusb_control_request_t) == USB0->COUNT0,);
      process_setup_packet(rhport);
      return;
    }
    if (_dcd.pipe0.buf) {
      /* DATA OUT */
      const unsigned vld = USB0->COUNT0;
      const unsigned rem = _dcd.pipe0.remaining;
      const unsigned len = TU_MIN(TU_MIN(rem, 64), vld);
      pipe_read_packet(_dcd.pipe0.buf, &USB0->FIFO0_WORD, len);

      _dcd.pipe0.remaining = rem - len;
      _dcd.remaining_ctrl -= len;

      _dcd.pipe0.buf = NULL;
      dcd_event_xfer_complete(rhport,
                              tu_edpt_addr(0, TUSB_DIR_OUT),
                              _dcd.pipe0.length - _dcd.pipe0.remaining,
                              XFER_RESULT_SUCCESS, true);
    }
    return;
  }

  /* When CSRL0 is zero, it means that completion of sending a any length packet
   * or receiving a zero length packet. */
  if (req != REQUEST_TYPE_INVALID && !tu_edpt_dir(req)) {
    /* STATUS IN */
    if (*(const uint16_t*)(uintptr_t)&_dcd.setup_packet == 0x0500) {
      /* The address must be changed on completion of the control transfer. */
      USB0->FADDR = (uint8_t)_dcd.setup_packet.wValue;
    }
    _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
    dcd_event_xfer_complete(rhport,
                            tu_edpt_addr(0, TUSB_DIR_IN),
                            _dcd.pipe0.length - _dcd.pipe0.remaining,
                            XFER_RESULT_SUCCESS, true);
    return;
  }
  if (_dcd.pipe0.buf) {
    /* DATA IN */
    _dcd.pipe0.buf = NULL;
    dcd_event_xfer_complete(rhport,
                            tu_edpt_addr(0, TUSB_DIR_IN),
                            _dcd.pipe0.length - _dcd.pipe0.remaining,
                            XFER_RESULT_SUCCESS, true);
  }
}

static void process_edpt_n(uint8_t rhport, uint_fast8_t ep_addr)
{
  bool completed;
  const unsigned dir_in     = tu_edpt_dir(ep_addr);
  const unsigned epn_minus1 = tu_edpt_number(ep_addr) - 1;

  volatile hw_endpoint_t *regs = edpt_regs(epn_minus1);
  if (dir_in) {
    // TU_LOG1(" TXCSRL%d = %x\r\n", epn_minus1 + 1, regs->TXCSRL);
    if (regs->TXCSRL & USB_TXCSRL1_STALLED) {
      regs->TXCSRL &= ~(USB_TXCSRL1_STALLED | USB_TXCSRL1_UNDRN);
      return;
    }
    completed = handle_xfer_in(ep_addr);
  } else {
    // TU_LOG1(" RXCSRL%d = %x\r\n", epn_minus1 + 1, regs->RXCSRL);
    if (regs->RXCSRL & USB_RXCSRL1_STALLED) {
      regs->RXCSRL &= ~(USB_RXCSRL1_STALLED | USB_RXCSRL1_OVER);
      return;
    }
    completed = handle_xfer_out(ep_addr);
  }

  if (completed) {
    pipe_state_t *pipe = &_dcd.pipe[dir_in][tu_edpt_number(ep_addr) - 1];
    dcd_event_xfer_complete(rhport, ep_addr,
                            pipe->length - pipe->remaining,
                            XFER_RESULT_SUCCESS, true);
  }
}

static void process_bus_reset(uint8_t rhport)
{
  /* When bmRequestType is REQUEST_TYPE_INVALID(0xFF),
   * a control transfer state is SETUP or STATUS stage. */
  _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
  _dcd.status_out = 0;
  /* When pipe0.buf has not NULL, DATA stage works in progress. */
  _dcd.pipe0.buf = NULL;

  USB0->TXIE = 1; /* Enable only EP0 */
  USB0->RXIE = 0;

  /* Clear FIFO settings */
  for (unsigned i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    USB0->EPIDX     = i;
    USB0->TXFIFOSZ  = 0;
    USB0->TXFIFOADD = 0;
    USB0->RXFIFOSZ  = 0;
    USB0->RXFIFOADD = 0;
  }
  dcd_event_bus_reset(rhport, TUSB_SPEED_FULL, true);
}

/*------------------------------------------------------------------
 * Device API
 *------------------------------------------------------------------*/

void dcd_init(uint8_t rhport)
{
  (void)rhport;
  USB0->IE |= USB_IE_SUSPND;
  NVIC_ClearPendingIRQ(USB0_IRQn);

  dcd_connect(rhport);
}

void dcd_int_enable(uint8_t rhport)
{
  (void)rhport;
  NVIC_EnableIRQ(USB0_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;
  NVIC_DisableIRQ(USB0_IRQn);
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;
  (void)dev_addr;
  _dcd.pipe0.buf       = NULL;
  _dcd.pipe0.length    = 0;
  _dcd.pipe0.remaining = 0;
  /* Clear RX FIFO to return ACK. */
  USB0->CSRL0 = USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND;
}

// Wake up host
void dcd_remote_wakeup(uint8_t rhport)
{
  (void)rhport;
  USB0->POWER |= USB_POWER_RESUME;

  unsigned cnt = SystemCoreClock / 1000;
  while (cnt--) __NOP();

  USB0->POWER &= ~USB_POWER_RESUME;
}

// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
  (void)rhport;
  USB0->POWER |= USB_POWER_SOFTCONN;
}

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
  (void)rhport;
  USB0->POWER &= ~USB_POWER_SOFTCONN;
}

void dcd_sof_enable(uint8_t rhport, bool en)
{
  (void) rhport;
  (void) en;

  // TODO implement later
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * ep_desc)
{
  (void) rhport;

  const unsigned ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn     = tu_edpt_number(ep_addr);
  const unsigned dir_in  = tu_edpt_dir(ep_addr);
  const unsigned xfer    = ep_desc->bmAttributes.xfer;
  const unsigned mps     = tu_edpt_packet_size(ep_desc);

  TU_ASSERT(epn < TUP_DCD_ENDPOINT_MAX);

  pipe_state_t *pipe = &_dcd.pipe[dir_in][epn - 1];
  pipe->buf       = NULL;
  pipe->length    = 0;
  pipe->remaining = 0;

  volatile hw_endpoint_t *regs = edpt_regs(epn - 1);
  if (dir_in) {
    regs->TXMAXP = mps;
    regs->TXCSRH = (xfer == TUSB_XFER_ISOCHRONOUS) ? USB_TXCSRH1_ISO : 0;
    if (regs->TXCSRL & USB_TXCSRL1_TXRDY)
      regs->TXCSRL = USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH;
    else
      regs->TXCSRL = USB_TXCSRL1_CLRDT;
    USB0->TXIE |= TU_BIT(epn);
  } else {
    regs->RXMAXP = mps;
    regs->RXCSRH = (xfer == TUSB_XFER_ISOCHRONOUS) ? USB_RXCSRH1_ISO : 0;
    if (regs->RXCSRL & USB_RXCSRL1_RXRDY)
      regs->RXCSRL = USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH;
    else
      regs->RXCSRL = USB_RXCSRL1_CLRDT;
    USB0->RXIE |= TU_BIT(epn);
  }

  /* Setup FIFO */
  int size_in_log2_minus3 = 28 - TU_MIN(28, __CLZ((uint32_t)mps));
  if ((8u << size_in_log2_minus3) < mps) ++size_in_log2_minus3;
  unsigned addr = find_free_memory(size_in_log2_minus3);
  TU_ASSERT(addr);

  USB0->EPIDX = epn;
  if (dir_in) {
    USB0->TXFIFOADD = addr;
    USB0->TXFIFOSZ  = size_in_log2_minus3;
  } else {
    USB0->RXFIFOADD = addr;
    USB0->RXFIFOSZ  = size_in_log2_minus3;
  }

  return true;
}

void dcd_edpt_close_all(uint8_t rhport)
{
  (void) rhport;
  volatile hw_endpoint_t *regs = (volatile hw_endpoint_t *)(uintptr_t)&USB0->TXMAXP1;
  unsigned const ie = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);
  USB0->TXIE = 1; /* Enable only EP0 */
  USB0->RXIE = 0;
  for (unsigned i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    regs->TXMAXP = 0;
    regs->TXCSRH = 0;
    if (regs->TXCSRL & USB_TXCSRL1_TXRDY)
      regs->TXCSRL = USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH;
    else
      regs->TXCSRL = USB_TXCSRL1_CLRDT;

    regs->RXMAXP = 0;
    regs->RXCSRH = 0;
    if (regs->RXCSRL & USB_RXCSRL1_RXRDY)
      regs->RXCSRL = USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH;
    else
      regs->RXCSRL = USB_RXCSRL1_CLRDT;

    USB0->EPIDX     = i;
    USB0->TXFIFOSZ  = 0;
    USB0->TXFIFOADD = 0;
    USB0->RXFIFOSZ  = 0;
    USB0->RXFIFOADD = 0;
  }
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;
  unsigned const epn    = tu_edpt_number(ep_addr);
  unsigned const dir_in = tu_edpt_dir(ep_addr);

  hw_endpoint_t volatile *regs = edpt_regs(epn - 1);
  unsigned const ie = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);
  if (dir_in) {
    USB0->TXIE  &= ~TU_BIT(epn);
    regs->TXMAXP = 0;
    regs->TXCSRH = 0;
    if (regs->TXCSRL & USB_TXCSRL1_TXRDY)
      regs->TXCSRL = USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH;
    else
      regs->TXCSRL = USB_TXCSRL1_CLRDT;

    USB0->EPIDX     = epn;
    USB0->TXFIFOSZ  = 0;
    USB0->TXFIFOADD = 0;
  } else {
    USB0->RXIE  &= ~TU_BIT(epn);
    regs->RXMAXP = 0;
    regs->RXCSRH = 0;
    if (regs->RXCSRL & USB_RXCSRL1_RXRDY)
      regs->RXCSRL = USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH;
    else
      regs->RXCSRL = USB_RXCSRL1_CLRDT;

    USB0->EPIDX     = epn;
    USB0->RXFIFOSZ  = 0;
    USB0->RXFIFOADD = 0;
  }
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void)rhport;
  bool ret;
  // TU_LOG1("X %x %d\r\n", ep_addr, total_bytes);
  unsigned const epnum = tu_edpt_number(ep_addr);
  unsigned const ie = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);
  if (epnum) {
    _dcd.pipe_buf_is_fifo[tu_edpt_dir(ep_addr)] &= ~TU_BIT(epnum - 1);
    ret = edpt_n_xfer(rhport, ep_addr, buffer, total_bytes);
  } else
    ret = edpt0_xfer(rhport, ep_addr, buffer, total_bytes);
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
  return ret;
}

// Submit a transfer where is managed by FIFO, When complete dcd_event_xfer_complete() is invoked to notify the stack - optional, however, must be listed in usbd.c
bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes)
{
  (void)rhport;
  bool ret;
  // TU_LOG1("X %x %d\r\n", ep_addr, total_bytes);
  unsigned const epnum = tu_edpt_number(ep_addr);
  TU_ASSERT(epnum);
  unsigned const ie = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);
  _dcd.pipe_buf_is_fifo[tu_edpt_dir(ep_addr)] |= TU_BIT(epnum - 1);
  ret = edpt_n_xfer(rhport, ep_addr, (uint8_t*)ff, total_bytes);
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
  return ret;
}

// Stall endpoint
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;
  unsigned const epn = tu_edpt_number(ep_addr);
  unsigned const ie  = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);
  if (0 == epn) {
    if (!ep_addr) { /* Ignore EP80 */
      _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
      _dcd.pipe0.buf = NULL;
      USB0->CSRL0 = USB_CSRL0_STALL;
    }
  } else {
    volatile hw_endpoint_t *regs = edpt_regs(epn - 1);
    if (tu_edpt_dir(ep_addr)) { /* IN */
      regs->TXCSRL = USB_TXCSRL1_STALL;
    } else { /* OUT */
      TU_ASSERT(!(regs->RXCSRL & USB_RXCSRL1_RXRDY),);
      regs->RXCSRL = USB_RXCSRL1_STALL;
    }
  }
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
}

// clear stall, data toggle is also reset to DATA0
void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;
  unsigned const epn = tu_edpt_number(ep_addr);
  hw_endpoint_t volatile *regs = edpt_regs(epn - 1);
  unsigned const ie = NVIC_GetEnableIRQ(USB0_IRQn);
  NVIC_DisableIRQ(USB0_IRQn);
  if (tu_edpt_dir(ep_addr)) { /* IN */
    regs->TXCSRL = USB_TXCSRL1_CLRDT;
  } else { /* OUT */
    regs->RXCSRL = USB_RXCSRL1_CLRDT;
  }
  if (ie) NVIC_EnableIRQ(USB0_IRQn);
}

/*-------------------------------------------------------------------
 * ISR
 *-------------------------------------------------------------------*/
void dcd_int_handler(uint8_t rhport)
{
  uint_fast8_t is, txis, rxis;

  is   = USB0->IS;   /* read and clear interrupt status */
  txis = USB0->TXIS; /* read and clear interrupt status */
  rxis = USB0->RXIS; /* read and clear interrupt status */
  // TU_LOG1("D%2x T%2x R%2x\r\n", is, txis, rxis);

  is &= USB0->IE; /* Clear disabled interrupts */
  if (is & USB_IS_DISCON) {
  }
  if (is & USB_IS_SOF) {
    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
  }
  if (is & USB_IS_RESET) {
    process_bus_reset(rhport);
  }
  if (is & USB_IS_RESUME) {
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
  }
  if (is & USB_IS_SUSPEND) {
    dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
  }

  txis &= USB0->TXIE; /* Clear disabled interrupts */
  if (txis & USB_TXIE_EP0) {
    process_ep0(rhport);
    txis &= ~TU_BIT(0);
  }
  while (txis) {
    unsigned const num = __builtin_ctz(txis);
    process_edpt_n(rhport, tu_edpt_addr(num, TUSB_DIR_IN));
    txis &= ~TU_BIT(num);
  }
  rxis &= USB0->RXIE; /* Clear disabled interrupts */
  while (rxis) {
    unsigned const num = __builtin_ctz(rxis);
    process_edpt_n(rhport, tu_edpt_addr(num, TUSB_DIR_OUT));
    rxis &= ~TU_BIT(num);
  }
}

#endif
