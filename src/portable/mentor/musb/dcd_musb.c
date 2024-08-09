/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji KITAYAMA
 * Copyright (c) 2024, Brent Kowal (Analog Devices, Inc)
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

#if CFG_TUD_ENABLED

#if __GNUC__ > 8 && defined(__ARM_FEATURE_UNALIGNED)
/* GCC warns that an address may be unaligned, even though
 * the target CPU has the capability for unaligned memory access. */
_Pragma("GCC diagnostic ignored \"-Waddress-of-packed-member\"");
#endif

#include "musb_type.h"
#include "device/dcd.h"

// Following symbols must be defined by port header
// - musb_dcd_int_enable/disable/clear/get_enable
// - musb_dcd_int_handler_enter/exit
// - musb_dcd_epn_regs: Get memory mapped struct of end point registers
// - musb_dcd_ep0_regs: Get memory mapped struct of EP0 registers
// - musb_dcd_ctl_regs: Get memory mapped struct of control registers
// - musb_dcd_ep_get_fifo_ptr: Gets the address of the provided EP's FIFO
// - musb_dcd_setup_fifo/reset_fifo: Configuration of the EP's FIFO
#if TU_CHECK_MCU(OPT_MCU_MSP432E4, OPT_MCU_TM4C123, OPT_MCU_TM4C129)
  #include "musb_ti.h"
#elif TU_CHECK_MCU(OPT_MCU_MAX32690, OPT_MCU_MAX32650, OPT_MCU_MAX32666, OPT_MCU_MAX78002)
  #include "musb_max32.h"
#else
  #error "Unsupported MCU"
#endif

/*------------------------------------------------------------------
 * MACRO TYPEDEF CONSTANT ENUM DECLARATION
 *------------------------------------------------------------------*/

#define REQUEST_TYPE_INVALID  (0xFFu)

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
  pipe_state_t pipe[2][TUP_DCD_ENDPOINT_MAX-1];   /* pipe[direction][endpoint number - 1] */
  uint16_t     pipe_buf_is_fifo[2]; /* Bitmap. Each bit means whether 1:TU_FIFO or 0:POD. */
} dcd_data_t;

/*------------------------------------------------------------------
 * INTERNAL OBJECT & FUNCTION DECLARATION
 *------------------------------------------------------------------*/
static dcd_data_t _dcd;


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
  volatile uint32_t *fifo_ptr = musb_dcd_ep_get_fifo_ptr(rhport, 0);
  volatile musb_dcd_ep0_regs_t* ep0_regs = musb_dcd_ep0_regs(rhport);
  p[0]        = *fifo_ptr;
  p[1]        = *fifo_ptr;

  _dcd.pipe0.buf       = NULL;
  _dcd.pipe0.length    = 0;
  _dcd.pipe0.remaining = 0;
  dcd_event_setup_received(rhport, (const uint8_t*)(uintptr_t)&_dcd.setup_packet, true);

  const unsigned len    = _dcd.setup_packet.wLength;
  _dcd.remaining_ctrl   = len;
  const unsigned dir_in = tu_edpt_dir(_dcd.setup_packet.bmRequestType);
  /* Clear RX FIFO and reverse the transaction direction */
  if (len && dir_in) ep0_regs->CSRL0 = USB_CSRL0_RXRDYC;
}

static bool handle_xfer_in(uint8_t rhport, uint_fast8_t ep_addr)
{
  unsigned epnum = tu_edpt_number(ep_addr);
  unsigned epnum_minus1 = epnum - 1;
  pipe_state_t  *pipe = &_dcd.pipe[tu_edpt_dir(ep_addr)][epnum_minus1];
  const unsigned rem  = pipe->remaining;

  if (!rem) {
    pipe->buf = NULL;
    return true;
  }

  volatile musb_dcd_epn_regs_t *regs = musb_dcd_epn_regs(rhport, epnum);
  const unsigned mps = regs->TXMAXP;
  const unsigned len = TU_MIN(mps, rem);
  void          *buf = pipe->buf;
  volatile void *fifo_ptr = musb_dcd_ep_get_fifo_ptr(rhport, epnum);
  // TU_LOG1("   %p mps %d len %d rem %d\r\n", buf, mps, len, rem);
  if (len) {
    if (_dcd.pipe_buf_is_fifo[TUSB_DIR_IN] & TU_BIT(epnum_minus1)) {
      pipe_read_write_packet_ff(buf, fifo_ptr, len, TUSB_DIR_IN);
    } else {
      pipe_write_packet(buf, fifo_ptr, len);
      pipe->buf       = buf + len;
    }
    pipe->remaining = rem - len;
  }
  regs->TXCSRL = USB_TXCSRL1_TXRDY;
  // TU_LOG1(" TXCSRL%d = %x %d\r\n", epnum, regs->TXCSRL, rem - len);
  return false;
}

static bool handle_xfer_out(uint8_t rhport, uint_fast8_t ep_addr)
{
  unsigned epnum = tu_edpt_number(ep_addr);
  unsigned epnum_minus1 = epnum - 1;
  pipe_state_t  *pipe = &_dcd.pipe[tu_edpt_dir(ep_addr)][epnum_minus1];
  volatile musb_dcd_epn_regs_t *regs = musb_dcd_epn_regs(rhport, epnum);
  // TU_LOG1(" RXCSRL%d = %x\r\n", epnum_minus1 + 1, regs->RXCSRL);

  TU_ASSERT(regs->RXCSRL & USB_RXCSRL1_RXRDY);

  const unsigned mps = regs->RXMAXP;
  const unsigned rem = pipe->remaining;
  const unsigned vld = regs->RXCOUNT;
  const unsigned len = TU_MIN(TU_MIN(rem, mps), vld);
  void          *buf = pipe->buf;
  volatile void *fifo_ptr = musb_dcd_ep_get_fifo_ptr(rhport, epnum);
  if (len) {
    if (_dcd.pipe_buf_is_fifo[TUSB_DIR_OUT] & TU_BIT(epnum_minus1)) {
      pipe_read_write_packet_ff(buf, fifo_ptr, len, TUSB_DIR_OUT);
    } else {
      pipe_read_packet(buf, fifo_ptr, len);
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
  unsigned epnum = tu_edpt_number(ep_addr);
  unsigned epnum_minus1 = epnum - 1;
  unsigned dir_in       = tu_edpt_dir(ep_addr);

  pipe_state_t *pipe = &_dcd.pipe[dir_in][epnum_minus1];
  pipe->buf          = buffer;
  pipe->length       = total_bytes;
  pipe->remaining    = total_bytes;

  if (dir_in) {
    handle_xfer_in(rhport, ep_addr);
  } else {
    volatile musb_dcd_epn_regs_t *regs = musb_dcd_epn_regs(rhport, epnum);
    if (regs->RXCSRL & USB_RXCSRL1_RXRDY) regs->RXCSRL = 0;
  }
  return true;
}

static bool edpt0_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes)
{
  (void)rhport;
  TU_ASSERT(total_bytes <= 64); /* Current implementation supports for only up to 64 bytes. */
  volatile musb_dcd_ep0_regs_t* ep0_regs = musb_dcd_ep0_regs(rhport);
  const unsigned req = _dcd.setup_packet.bmRequestType;
  TU_ASSERT(req != REQUEST_TYPE_INVALID || total_bytes == 0);

  if (req == REQUEST_TYPE_INVALID || _dcd.status_out) {
    /* STATUS OUT stage.
     * MUSB controller automatically handles STATUS OUT packets without
     * software helps. We do not have to do anything. And STATUS stage
     * may have already finished and received the next setup packet
     * without calling this function, so we have no choice but to
     * invoke the callback function of status packet here. */
    // TU_LOG1(" STATUS OUT ep0_regs->CSRL0 = %x\r\n", ep0_regs->CSRL0);
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
    volatile void *fifo_ptr = musb_dcd_ep_get_fifo_ptr(rhport, 0);
    if (dir_in) {
      pipe_write_packet(buffer, fifo_ptr, len);

      _dcd.pipe0.buf       = buffer + len;
      _dcd.pipe0.length    = len;
      _dcd.pipe0.remaining = 0;

      _dcd.remaining_ctrl  = rem - len;
      if ((len < 64) || (rem == len)) {
        _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID; /* Change to STATUS/SETUP stage */
        _dcd.status_out = 1;
        /* Flush TX FIFO and reverse the transaction direction. */
        ep0_regs->CSRL0 = USB_CSRL0_TXRDY | USB_CSRL0_DATAEND;
      } else {
        ep0_regs->CSRL0 = USB_CSRL0_TXRDY; /* Flush TX FIFO to return ACK. */
      }
      // TU_LOG1(" IN ep0_regs->CSRL0 = %x\r\n", ep0_regs->CSRL0);
    } else {
      // TU_LOG1(" OUT ep0_regs->CSRL0 = %x\r\n", ep0_regs->CSRL0);
      _dcd.pipe0.buf       = buffer;
      _dcd.pipe0.length    = len;
      _dcd.pipe0.remaining = len;
      ep0_regs->CSRL0 = USB_CSRL0_RXRDYC; /* Clear RX FIFO to return ACK. */
    }
  } else if (dir_in) {
    // TU_LOG1(" STATUS IN ep0_regs->CSRL0  = %x\r\n", ep0_regs->CSRL0);
    _dcd.pipe0.buf = NULL;
    _dcd.pipe0.length    = 0;
    _dcd.pipe0.remaining = 0;
    /* Clear RX FIFO and reverse the transaction direction */
    ep0_regs->CSRL0 = USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND;
  }
  return true;
}

static void process_ep0(uint8_t rhport)
{
  volatile musb_dcd_ep0_regs_t* ep0_regs = musb_dcd_ep0_regs(rhport);
  uint_fast8_t csrl = ep0_regs->CSRL0;

  // TU_LOG1(" EP0 ep0_regs->CSRL0 = %x\r\n", csrl);

  if (csrl & USB_CSRL0_STALLED) {
    /* Returned STALL packet to HOST. */
    ep0_regs->CSRL0 = 0; /* Clear STALL */
    return;
  }

  unsigned req = _dcd.setup_packet.bmRequestType;
  if (csrl & USB_CSRL0_SETEND) {
    TU_LOG1("   ABORT by the next packets\r\n");
    ep0_regs->CSRL0 = USB_CSRL0_SETENDC;
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
      TU_ASSERT(sizeof(tusb_control_request_t) == ep0_regs->COUNT0,);
      process_setup_packet(rhport);
      return;
    }
    if (_dcd.pipe0.buf) {
      /* DATA OUT */
      const unsigned vld = ep0_regs->COUNT0;
      const unsigned rem = _dcd.pipe0.remaining;
      const unsigned len = TU_MIN(TU_MIN(rem, 64), vld);
      volatile void *fifo_ptr = musb_dcd_ep_get_fifo_ptr(rhport, 0);
      pipe_read_packet(_dcd.pipe0.buf, fifo_ptr, len);

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

  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);

  /* When CSRL0 is zero, it means that completion of sending a any length packet
   * or receiving a zero length packet. */
  if (req != REQUEST_TYPE_INVALID && !tu_edpt_dir(req)) {
    /* STATUS IN */
    if (*(const uint16_t*)(uintptr_t)&_dcd.setup_packet == 0x0500) {
      /* The address must be changed on completion of the control transfer. */
      ctrl_regs->FADDR = (uint8_t)_dcd.setup_packet.wValue;
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
  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned epn_minus1 = epn - 1;

  volatile musb_dcd_epn_regs_t *regs = musb_dcd_epn_regs(rhport, epn);
  if (dir_in) {
    // TU_LOG1(" TXCSRL%d = %x\r\n", epn, regs->TXCSRL);
    if (regs->TXCSRL & USB_TXCSRL1_STALLED) {
      regs->TXCSRL &= ~(USB_TXCSRL1_STALLED | USB_TXCSRL1_UNDRN);
      return;
    }
    completed = handle_xfer_in(rhport, ep_addr);
  } else {
    // TU_LOG1(" RXCSRL%d = %x\r\n", epn, regs->RXCSRL);
    if (regs->RXCSRL & USB_RXCSRL1_STALLED) {
      regs->RXCSRL &= ~(USB_RXCSRL1_STALLED | USB_RXCSRL1_OVER);
      return;
    }
    completed = handle_xfer_out(rhport, ep_addr);
  }

  if (completed) {
    pipe_state_t *pipe = &_dcd.pipe[dir_in][epn_minus1];
    dcd_event_xfer_complete(rhport, ep_addr,
                            pipe->length - pipe->remaining,
                            XFER_RESULT_SUCCESS, true);
  }
}

static void process_bus_reset(uint8_t rhport)
{
  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);
  /* When bmRequestType is REQUEST_TYPE_INVALID(0xFF),
   * a control transfer state is SETUP or STATUS stage. */
  _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
  _dcd.status_out = 0;
  /* When pipe0.buf has not NULL, DATA stage works in progress. */
  _dcd.pipe0.buf = NULL;

  ctrl_regs->TXIE = 1; /* Enable only EP0 */
  ctrl_regs->RXIE = 0;

  /* Clear FIFO settings */
  for (unsigned i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    musb_dcd_reset_fifo(rhport, i, 0);
    musb_dcd_reset_fifo(rhport, i, 1);
  }
  dcd_event_bus_reset(rhport, (ctrl_regs->POWER & USB_POWER_HSMODE) ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL, true);
}

/*------------------------------------------------------------------
 * Device API
 *------------------------------------------------------------------*/

void dcd_init(uint8_t rhport)
{
  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);
  ctrl_regs->IE |= USB_IE_SUSPND;
  musb_dcd_int_clear(rhport);
  musb_dcd_phy_init(rhport);
  dcd_connect(rhport);
}

void dcd_int_enable(uint8_t rhport)
{
  musb_dcd_int_enable(rhport);
}

void dcd_int_disable(uint8_t rhport)
{
  musb_dcd_int_disable(rhport);
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)dev_addr;
  volatile musb_dcd_ep0_regs_t* ep0_regs = musb_dcd_ep0_regs(rhport);
  _dcd.pipe0.buf       = NULL;
  _dcd.pipe0.length    = 0;
  _dcd.pipe0.remaining = 0;
  /* Clear RX FIFO to return ACK. */
  ep0_regs->CSRL0 = USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND;
}

// Wake up host
void dcd_remote_wakeup(uint8_t rhport)
{
  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);
  ctrl_regs->POWER |= USB_POWER_RESUME;

  unsigned cnt = SystemCoreClock / 1000;
  while (cnt--) __NOP();

  ctrl_regs->POWER &= ~USB_POWER_RESUME;
}

// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);
  ctrl_regs->POWER |= TUD_OPT_HIGH_SPEED ? USB_POWER_HSENAB : 0;
  ctrl_regs->POWER |= USB_POWER_SOFTCONN;
}

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);
  ctrl_regs->POWER &= ~USB_POWER_SOFTCONN;
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

  volatile musb_dcd_epn_regs_t *regs = musb_dcd_epn_regs(rhport, epn);
  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);
  if (dir_in) {
    regs->TXMAXP = mps;
    regs->TXCSRH = (xfer == TUSB_XFER_ISOCHRONOUS) ? USB_TXCSRH1_ISO : 0;
    if (regs->TXCSRL & USB_TXCSRL1_TXRDY)
      regs->TXCSRL = USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH;
    else
      regs->TXCSRL = USB_TXCSRL1_CLRDT;
    ctrl_regs->TXIE |= TU_BIT(epn);
  } else {
    regs->RXMAXP = mps;
    regs->RXCSRH = (xfer == TUSB_XFER_ISOCHRONOUS) ? USB_RXCSRH1_ISO : 0;
    if (regs->RXCSRL & USB_RXCSRL1_RXRDY)
      regs->RXCSRL = USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH;
    else
      regs->RXCSRL = USB_RXCSRL1_CLRDT;
    ctrl_regs->RXIE |= TU_BIT(epn);
  }

  /* Setup FIFO */
  musb_dcd_setup_fifo(rhport, epn, dir_in, mps);

  return true;
}

void dcd_edpt_close_all(uint8_t rhport)
{
  volatile musb_dcd_epn_regs_t *regs;
  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);
  ctrl_regs->TXIE = 1; /* Enable only EP0 */
  ctrl_regs->RXIE = 0;
  for (unsigned i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    regs = musb_dcd_epn_regs(rhport, i);
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

    musb_dcd_reset_fifo(rhport, i, 0);
    musb_dcd_reset_fifo(rhport, i, 1);

  }
  if (ie) musb_dcd_int_enable(rhport);
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  unsigned const epn    = tu_edpt_number(ep_addr);
  unsigned const dir_in = tu_edpt_dir(ep_addr);

  volatile musb_dcd_epn_regs_t *regs = musb_dcd_epn_regs(rhport, epn);
  volatile musb_dcd_ctl_regs_t *ctrl_regs = musb_dcd_ctl_regs(rhport);
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);
  if (dir_in) {
    ctrl_regs->TXIE  &= ~TU_BIT(epn);
    regs->TXMAXP = 0;
    regs->TXCSRH = 0;
    if (regs->TXCSRL & USB_TXCSRL1_TXRDY)
      regs->TXCSRL = USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH;
    else
      regs->TXCSRL = USB_TXCSRL1_CLRDT;
  } else {
    ctrl_regs->RXIE  &= ~TU_BIT(epn);
    regs->RXMAXP = 0;
    regs->RXCSRH = 0;
    if (regs->RXCSRL & USB_RXCSRL1_RXRDY)
      regs->RXCSRL = USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH;
    else
      regs->RXCSRL = USB_RXCSRL1_CLRDT;
  }
  musb_dcd_reset_fifo(rhport, epn, dir_in);
  if (ie) musb_dcd_int_enable(rhport);
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void)rhport;
  bool ret;
  // TU_LOG1("X %x %d\r\n", ep_addr, total_bytes);
  unsigned const epnum = tu_edpt_number(ep_addr);
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);
  if (epnum) {
    _dcd.pipe_buf_is_fifo[tu_edpt_dir(ep_addr)] &= ~TU_BIT(epnum - 1);
    ret = edpt_n_xfer(rhport, ep_addr, buffer, total_bytes);
  } else
    ret = edpt0_xfer(rhport, ep_addr, buffer, total_bytes);
  if (ie) musb_dcd_int_enable(rhport);
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
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);
  _dcd.pipe_buf_is_fifo[tu_edpt_dir(ep_addr)] |= TU_BIT(epnum - 1);
  ret = edpt_n_xfer(rhport, ep_addr, (uint8_t*)ff, total_bytes);
  if (ie) musb_dcd_int_enable(rhport);
  return ret;
}

// Stall endpoint
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  unsigned const epn = tu_edpt_number(ep_addr);
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);
  if (0 == epn) {
    volatile musb_dcd_ep0_regs_t* ep0_regs = musb_dcd_ep0_regs(rhport);
    if (!ep_addr) { /* Ignore EP80 */
      _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
      _dcd.pipe0.buf = NULL;
      ep0_regs->CSRL0 = USB_CSRL0_STALL;
    }
  } else {
    volatile musb_dcd_epn_regs_t *regs = musb_dcd_epn_regs(rhport, epn);
    if (tu_edpt_dir(ep_addr)) { /* IN */
      regs->TXCSRL = USB_TXCSRL1_STALL;
    } else { /* OUT */
      TU_ASSERT(!(regs->RXCSRL & USB_RXCSRL1_RXRDY),);
      regs->RXCSRL = USB_RXCSRL1_STALL;
    }
  }
  if (ie) musb_dcd_int_enable(rhport);
}

// clear stall, data toggle is also reset to DATA0
void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;
  unsigned const epn = tu_edpt_number(ep_addr);
  musb_dcd_epn_regs_t volatile *regs = musb_dcd_epn_regs(rhport, epn);
  unsigned const ie = musb_dcd_get_int_enable(rhport);
  musb_dcd_int_disable(rhport);
  if (tu_edpt_dir(ep_addr)) { /* IN */
    regs->TXCSRL = USB_TXCSRL1_CLRDT;
  } else { /* OUT */
    regs->RXCSRL = USB_RXCSRL1_CLRDT;
  }
  if (ie) musb_dcd_int_enable(rhport);
}

/*-------------------------------------------------------------------
 * ISR
 *-------------------------------------------------------------------*/
void dcd_int_handler(uint8_t rhport)
{
  uint_fast8_t is, txis, rxis;
  volatile musb_dcd_ctl_regs_t *ctrl_regs;

  //Part specific ISR setup/entry
  musb_dcd_int_handler_enter(rhport);

  ctrl_regs = musb_dcd_ctl_regs(rhport);
  is   = ctrl_regs->IS;   /* read and clear interrupt status */
  txis = ctrl_regs->TXIS; /* read and clear interrupt status */
  rxis = ctrl_regs->RXIS; /* read and clear interrupt status */
  // TU_LOG1("D%2x T%2x R%2x\r\n", is, txis, rxis);

  is &= ctrl_regs->IE; /* Clear disabled interrupts */
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

  txis &= ctrl_regs->TXIE; /* Clear disabled interrupts */
  if (txis & USB_TXIE_EP0) {
    process_ep0(rhport);
    txis &= ~TU_BIT(0);
  }
  while (txis) {
    unsigned const num = __builtin_ctz(txis);
    process_edpt_n(rhport, tu_edpt_addr(num, TUSB_DIR_IN));
    txis &= ~TU_BIT(num);
  }
  rxis &= ctrl_regs->RXIE; /* Clear disabled interrupts */
  while (rxis) {
    unsigned const num = __builtin_ctz(rxis);
    process_edpt_n(rhport, tu_edpt_addr(num, TUSB_DIR_OUT));
    rxis &= ~TU_BIT(num);
  }

  //Part specific ISR exit
  musb_dcd_int_handler_exit(rhport);
}

#endif
