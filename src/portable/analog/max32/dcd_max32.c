/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji KITAYAMA
 * Copyright (c) 2024 Brent Kowal (Analog Devices, Inc)
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

#if CFG_TUD_ENABLED && TU_CHECK_MCU(OPT_MCU_MAX32690)

  #if __GNUC__ > 8 && defined(__ARM_FEATURE_UNALIGNED)
/* GCC warns that an address may be unaligned, even though
 * the target CPU has the capability for unaligned memory access. */
_Pragma("GCC diagnostic ignored \"-Waddress-of-packed-member\"");
  #endif

  #include "device/dcd.h"

  #include "mxc_delay.h"
  #include "mxc_device.h"
  #include "mxc_sys.h"
  #include "nvic_table.h"
  #include "usbhs_regs.h"

  #define USBHS_M31_CLOCK_RECOVERY

  /*------------------------------------------------------------------
 * MACRO TYPEDEF CONSTANT ENUM DECLARATION
 *------------------------------------------------------------------*/
  #define REQUEST_TYPE_INVALID (0xFFu)


typedef union {
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
} hw_fifo_t;

typedef struct TU_ATTR_PACKED {
  void *buf;          /* the start address of a transfer data buffer */
  uint16_t length;    /* the number of bytes in the buffer */
  uint16_t remaining; /* the number of bytes remaining in the buffer */
} pipe_state_t;

typedef struct
{
  tusb_control_request_t setup_packet;
  uint16_t remaining_ctrl; /* The number of bytes remaining in data stage of control transfer. */
  int8_t status_out;
  pipe_state_t pipe0;
  pipe_state_t pipe[2][TUP_DCD_ENDPOINT_MAX - 1]; /* pipe[direction][endpoint number - 1] */
  uint16_t pipe_buf_is_fifo[2];                   /* Bitmap. Each bit means whether 1:TU_FIFO or 0:POD. */
} dcd_data_t;

/*------------------------------------------------------------------
 * INTERNAL OBJECT & FUNCTION DECLARATION
 *------------------------------------------------------------------*/
static dcd_data_t _dcd;


static volatile void *edpt_get_fifo_ptr(unsigned epnum) {
  volatile uint32_t *ptr;

  ptr = &MXC_USBHS->fifo0;
  ptr += epnum; /* Pointer math: multiplies ep by sizeof(uint32_t) */

  return (volatile void *) ptr;
}

static void pipe_write_packet(void *buf, volatile void *fifo, unsigned len) {
  volatile hw_fifo_t *reg = (volatile hw_fifo_t *) fifo;
  uintptr_t addr = (uintptr_t) buf;
  while (len >= 4) {
    reg->u32 = *(uint32_t const *) addr;
    addr += 4;
    len -= 4;
  }
  if (len >= 2) {
    reg->u16 = *(uint16_t const *) addr;
    addr += 2;
    len -= 2;
  }
  if (len) {
    reg->u8 = *(uint8_t const *) addr;
  }
}

static void pipe_read_packet(void *buf, volatile void *fifo, unsigned len) {
  volatile hw_fifo_t *reg = (volatile hw_fifo_t *) fifo;
  uintptr_t addr = (uintptr_t) buf;
  while (len >= 4) {
    *(uint32_t *) addr = reg->u32;
    addr += 4;
    len -= 4;
  }
  if (len >= 2) {
    *(uint16_t *) addr = reg->u16;
    addr += 2;
    len -= 2;
  }
  if (len) {
    *(uint8_t *) addr = reg->u8;
  }
}

static void pipe_read_write_packet_ff(tu_fifo_t *f, volatile void *fifo, unsigned len, unsigned dir) {
  static const struct {
    void (*tu_fifo_get_info)(tu_fifo_t *f, tu_fifo_buffer_info_t *info);
    void (*tu_fifo_advance)(tu_fifo_t *f, uint16_t n);
    void (*pipe_read_write)(void *buf, volatile void *fifo, unsigned len);
  } ops[] = {
      /* OUT */ {tu_fifo_get_write_info, tu_fifo_advance_write_pointer, pipe_read_packet},
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

static void process_setup_packet(uint8_t rhport) {
  uint32_t *p = (void *) &_dcd.setup_packet;
  p[0] = MXC_USBHS->fifo0;
  p[1] = MXC_USBHS->fifo0;

  _dcd.pipe0.buf = NULL;
  _dcd.pipe0.length = 0;
  _dcd.pipe0.remaining = 0;
  dcd_event_setup_received(rhport, (const uint8_t *) (uintptr_t) &_dcd.setup_packet, true);

  const unsigned len = _dcd.setup_packet.wLength;
  _dcd.remaining_ctrl = len;
  const unsigned dir_in = tu_edpt_dir(_dcd.setup_packet.bmRequestType);
  /* Clear RX FIFO and reverse the transaction direction */
  if (len && dir_in) {
    MXC_USBHS->index = 0;
    MXC_USBHS->csr0 = MXC_F_USBHS_CSR0_SERV_OUTPKTRDY;
  }
}

static bool handle_xfer_in(uint_fast8_t ep_addr) {
  unsigned epnum = tu_edpt_number(ep_addr);
  unsigned epnum_minus1 = epnum - 1;
  pipe_state_t *pipe = &_dcd.pipe[tu_edpt_dir(ep_addr)][epnum_minus1];
  const unsigned rem = pipe->remaining;

  //This function should not be for ep0
  TU_ASSERT(epnum);

  if (!rem) {
    pipe->buf = NULL;
    return true;
  }

  MXC_USBHS->index = epnum;
  const unsigned mps = MXC_USBHS->inmaxp;
  const unsigned len = TU_MIN(mps, rem);
  void *buf = pipe->buf;
  volatile void *fifo_ptr = edpt_get_fifo_ptr(epnum);
  if (len) {
    if (_dcd.pipe_buf_is_fifo[TUSB_DIR_IN] & TU_BIT(epnum_minus1)) {
      pipe_read_write_packet_ff(buf, fifo_ptr, len, TUSB_DIR_IN);
    } else {
      pipe_write_packet(buf, fifo_ptr, len);
      pipe->buf = buf + len;
    }
    pipe->remaining = rem - len;
  }
  MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_INPKTRDY;//TODO: Verify a | isnt needed

  return false;
}

static bool handle_xfer_out(uint_fast8_t ep_addr) {
  unsigned epnum = tu_edpt_number(ep_addr);
  unsigned epnum_minus1 = epnum - 1;
  pipe_state_t *pipe = &_dcd.pipe[tu_edpt_dir(ep_addr)][epnum_minus1];

  //This function should not be for ep0
  TU_ASSERT(epnum);

  MXC_USBHS->index = epnum;

  TU_ASSERT(MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY);

  const unsigned mps = MXC_USBHS->outmaxp;
  const unsigned rem = pipe->remaining;
  const unsigned vld = MXC_USBHS->outcount;
  const unsigned len = TU_MIN(TU_MIN(rem, mps), vld);
  void *buf = pipe->buf;
  volatile void *fifo_ptr = edpt_get_fifo_ptr(epnum);
  if (len) {
    if (_dcd.pipe_buf_is_fifo[TUSB_DIR_OUT] & TU_BIT(epnum_minus1)) {
      pipe_read_write_packet_ff(buf, fifo_ptr, len, TUSB_DIR_OUT);
    } else {
      pipe_read_packet(buf, fifo_ptr, len);
      pipe->buf = buf + len;
    }
    pipe->remaining = rem - len;
  }
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return NULL != buf;
  }
  MXC_USBHS->outcsrl = 0; /* Clear RXRDY bit *///TODO: Verify just setting to 0 is ok
  return false;
}

static bool edpt_n_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  (void) rhport;

  unsigned epnum = tu_edpt_number(ep_addr);
  unsigned epnum_minus1 = epnum - 1;
  unsigned dir_in = tu_edpt_dir(ep_addr);

  pipe_state_t *pipe = &_dcd.pipe[dir_in][epnum_minus1];
  pipe->buf = buffer;
  pipe->length = total_bytes;
  pipe->remaining = total_bytes;

  if (dir_in) {
    handle_xfer_in(ep_addr);
  } else {
    MXC_USBHS->index = epnum;
    if (MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY) {
      MXC_USBHS->outcsrl = 0;//TODO: Verify just setting to 0 is ok
    }
  }
  return true;
}

static bool edpt0_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  (void) rhport;
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
  MXC_USBHS->index = 0;
  if (tu_edpt_dir(req) == dir_in) { /* DATA stage */
    TU_ASSERT(total_bytes <= _dcd.remaining_ctrl);
    const unsigned rem = _dcd.remaining_ctrl;
    const unsigned len = TU_MIN(TU_MIN(rem, 64), total_bytes);
    volatile void *fifo_ptr = edpt_get_fifo_ptr(0);
    if (dir_in) {
      pipe_write_packet(buffer, fifo_ptr, len);

      _dcd.pipe0.buf = buffer + len;
      _dcd.pipe0.length = len;
      _dcd.pipe0.remaining = 0;

      _dcd.remaining_ctrl = rem - len;
      if ((len < 64) || (rem == len)) {
        _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID; /* Change to STATUS/SETUP stage */
        _dcd.status_out = 1;
        /* Flush TX FIFO and reverse the transaction direction. */
        MXC_USBHS->csr0 = MXC_F_USBHS_CSR0_INPKTRDY | MXC_F_USBHS_CSR0_DATA_END;
      } else {
        MXC_USBHS->csr0 = MXC_F_USBHS_CSR0_INPKTRDY; /* Flush TX FIFO to return ACK. */
      }
    } else {
      _dcd.pipe0.buf = buffer;
      _dcd.pipe0.length = len;
      _dcd.pipe0.remaining = len;
      MXC_USBHS->csr0 = MXC_F_USBHS_CSR0_SERV_OUTPKTRDY; /* Clear RX FIFO to return ACK. */
    }
  } else if (dir_in) {
    _dcd.pipe0.buf = NULL;
    _dcd.pipe0.length = 0;
    _dcd.pipe0.remaining = 0;
    /* Clear RX FIFO and reverse the transaction direction */
    MXC_USBHS->csr0 = MXC_F_USBHS_CSR0_SERV_OUTPKTRDY | MXC_F_USBHS_CSR0_DATA_END;
  }
  return true;
}

static void process_ep0(uint8_t rhport) {
  MXC_USBHS->index = 0;
  uint_fast8_t csrl = MXC_USBHS->csr0;

  if (csrl & MXC_F_USBHS_CSR0_SENT_STALL) {
    /* Returned STALL packet to HOST. */
    MXC_USBHS->csr0 = 0; /* Clear STALL */
    return;
  }

  unsigned req = _dcd.setup_packet.bmRequestType;
  if (csrl & MXC_F_USBHS_CSR0_SETUP_END) {
    TU_LOG1("   ABORT by the next packets\r\n");
    MXC_USBHS->csr0 = MXC_F_USBHS_CSR0_SERV_SETUP_END;
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
    if (!(csrl & MXC_F_USBHS_CSR0_OUTPKTRDY)) return; /* Received SETUP packet */
  }

  if (csrl & MXC_F_USBHS_CSR0_OUTPKTRDY) {
    /* Received SETUP or DATA OUT packet */
    if (req == REQUEST_TYPE_INVALID) {
      /* SETUP */
      TU_ASSERT(sizeof(tusb_control_request_t) == MXC_USBHS->count0, );
      process_setup_packet(rhport);
      return;
    }
    if (_dcd.pipe0.buf) {
      /* DATA OUT */
      const unsigned vld = MXC_USBHS->count0;
      const unsigned rem = _dcd.pipe0.remaining;
      const unsigned len = TU_MIN(TU_MIN(rem, 64), vld);
      volatile void *fifo_ptr = edpt_get_fifo_ptr(0);
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

  /* When CSRL0 is zero, it means that completion of sending a any length packet
   * or receiving a zero length packet. */
  if (req != REQUEST_TYPE_INVALID && !tu_edpt_dir(req)) {
    /* STATUS IN */
    if (*(const uint16_t *) (uintptr_t) &_dcd.setup_packet == 0x0500) {
      /* The address must be changed on completion of the control transfer. */
      MXC_USBHS->faddr = (uint8_t) _dcd.setup_packet.wValue;
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

static void process_edpt_n(uint8_t rhport, uint_fast8_t ep_addr) {
  bool completed;
  const unsigned dir_in = tu_edpt_dir(ep_addr);
  const unsigned epnum = tu_edpt_number(ep_addr);

  MXC_USBHS->index = epnum;

  if (dir_in) {
    if (MXC_USBHS->incsrl & MXC_F_USBHS_INCSRL_SENTSTALL) {
      MXC_USBHS->incsrl &= ~(MXC_F_USBHS_INCSRL_SENTSTALL | MXC_F_USBHS_INCSRL_UNDERRUN);
      return;
    }
    completed = handle_xfer_in(ep_addr);
  } else {
    if (MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_SENTSTALL) {
      MXC_USBHS->outcsrl &= ~(MXC_F_USBHS_OUTCSRL_SENTSTALL | MXC_F_USBHS_OUTCSRL_OVERRUN);
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

static void process_bus_reset(uint8_t rhport) {
  (void) rhport;
  /* When bmRequestType is REQUEST_TYPE_INVALID(0xFF),
   * a control transfer state is SETUP or STATUS stage. */
  _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
  _dcd.status_out = 0;
  /* When pipe0.buf has not NULL, DATA stage works in progress. */
  _dcd.pipe0.buf = NULL;

  MXC_USBHS->intrinen = 1; /* Enable only EP0 */
  MXC_USBHS->introuten = 0;


  /* Clear FIFO settings */
  for (unsigned i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    MXC_USBHS->index = i;
    if (MXC_USBHS->incsrl & MXC_F_USBHS_INCSRL_INPKTRDY) {
      /* Per musbhsfc_pg, only flush FIFO if IN packet loaded */
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_FLUSHFIFO;
    }

    if (MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY) {
      /* Per musbhsfc_pg, only flush FIFO if OUT packet is ready */
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_FLUSHFIFO;
    }
  }
  dcd_event_bus_reset(0, (MXC_USBHS->power & MXC_F_USBHS_POWER_HS_MODE) ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL, true);
}

/*------------------------------------------------------------------
 * Device API
 *------------------------------------------------------------------*/

void dcd_init(uint8_t rhport) {
  (void) rhport;
  MXC_USBHS->intrusben |= MXC_F_USBHS_INTRUSBEN_SUSPEND_INT_EN;

  //Interrupt for VBUS disconnect
  MXC_USBHS->mxm_int_en |= MXC_F_USBHS_MXM_INT_EN_NOVBUS;

  NVIC_ClearPendingIRQ(USB_IRQn);
  dcd_edpt_close_all(rhport);

  //Unsuspend the MAC
  MXC_USBHS->mxm_suspend = 0;

  /* Configure PHY */
  MXC_USBHS->m31_phy_xcfgi_31_0 = (0x1 << 3) | (0x1 << 11);
  MXC_USBHS->m31_phy_xcfgi_63_32 = 0;
  MXC_USBHS->m31_phy_xcfgi_95_64 = 0x1 << (72 - 64);
  MXC_USBHS->m31_phy_xcfgi_127_96 = 0;


  #ifdef USBHS_M31_CLOCK_RECOVERY
  MXC_USBHS->m31_phy_noncry_rstb = 1;
  MXC_USBHS->m31_phy_noncry_en = 1;
  MXC_USBHS->m31_phy_outclksel = 0;
  MXC_USBHS->m31_phy_coreclkin = 0;
  MXC_USBHS->m31_phy_xtlsel = 2; /* Select 25 MHz clock */
  #else
  /* Use this option to feed the PHY a 30 MHz clock, which is them used as a PLL reference */
  /* As it depends on the system core clock, this should probably be done at the SYS level */
  MXC_USBHS->m31_phy_noncry_rstb = 0;
  MXC_USBHS->m31_phy_noncry_en = 0;
  MXC_USBHS->m31_phy_outclksel = 1;
  MXC_USBHS->m31_phy_coreclkin = 1;
  MXC_USBHS->m31_phy_xtlsel = 3; /* Select 30 MHz clock */
  #endif
  MXC_USBHS->m31_phy_pll_en = 1;
  MXC_USBHS->m31_phy_oscouten = 1;

  /* Reset PHY */
  MXC_USBHS->m31_phy_ponrst = 0;
  MXC_USBHS->m31_phy_ponrst = 1;

  dcd_connect(rhport);
}

void dcd_int_enable(uint8_t rhport) {
  (void) rhport;
  NVIC_EnableIRQ(USB_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
  (void) rhport;
  NVIC_DisableIRQ(USB_IRQn);
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;
  (void) dev_addr;
  _dcd.pipe0.buf = NULL;
  _dcd.pipe0.length = 0;
  _dcd.pipe0.remaining = 0;
  /* Clear RX FIFO to return ACK. */
  MXC_USBHS->index = 0;
  MXC_USBHS->csr0 = MXC_F_USBHS_CSR0_SERV_OUTPKTRDY | MXC_F_USBHS_CSR0_DATA_END;
}

// Wake up host
void dcd_remote_wakeup(uint8_t rhport) {
  (void) rhport;
  MXC_USBHS->power |= MXC_F_USBHS_POWER_RESUME;

  #if CFG_TUSB_OS != OPT_OS_NONE
  osal_task_delay(10);
  #else
  MXC_Delay(MXC_DELAY_MSEC(10));
  #endif

  MXC_USBHS->power &= ~MXC_F_USBHS_POWER_RESUME;
}

// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport) {
  (void) rhport;
  MXC_USBHS->power |= TUD_OPT_HIGH_SPEED ? MXC_F_USBHS_POWER_HS_ENABLE : 0;
  MXC_USBHS->power |= MXC_F_USBHS_POWER_SOFTCONN;
}

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport) {
  (void) rhport;
  MXC_USBHS->power &= ~MXC_F_USBHS_POWER_SOFTCONN;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void) rhport;
  (void) en;

  // TODO implement later
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const *ep_desc) {
  (void) rhport;

  const unsigned ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned dir_in = tu_edpt_dir(ep_addr);
  const unsigned xfer = ep_desc->bmAttributes.xfer;
  const unsigned mps = tu_edpt_packet_size(ep_desc);

  TU_ASSERT(epn < TUP_DCD_ENDPOINT_MAX);

  pipe_state_t *pipe = &_dcd.pipe[dir_in][epn - 1];
  pipe->buf = NULL;
  pipe->length = 0;
  pipe->remaining = 0;

  MXC_USBHS->index = epn;

  if (dir_in) {
    MXC_USBHS->inmaxp = mps;
    MXC_USBHS->incsru = (MXC_F_USBHS_INCSRU_DPKTBUFDIS | MXC_F_USBHS_INCSRU_MODE) | ((xfer == TUSB_XFER_ISOCHRONOUS) ? MXC_F_USBHS_INCSRU_ISO : 0);
    if (MXC_USBHS->incsrl & MXC_F_USBHS_INCSRL_INPKTRDY) {
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG | MXC_F_USBHS_INCSRL_FLUSHFIFO;
    } else {
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG;
    }
    MXC_USBHS->intrinen |= TU_BIT(epn);
  } else {
    MXC_USBHS->outmaxp = mps;
    MXC_USBHS->outcsru = (MXC_F_USBHS_OUTCSRU_DPKTBUFDIS) | ((xfer == TUSB_XFER_ISOCHRONOUS) ? MXC_F_USBHS_OUTCSRU_ISO : 0);
    if (MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY) {
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG | MXC_F_USBHS_OUTCSRL_FLUSHFIFO;
    } else {
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG;
    }
    MXC_USBHS->introuten |= TU_BIT(epn);
  }

  return true;
}

void dcd_edpt_close_all(uint8_t rhport) {
  (void) rhport;

  MXC_SYS_Crit_Enter();
  MXC_USBHS->intrinen = 1; /* Enable only EP0 */
  MXC_USBHS->introuten = 0;

  for (unsigned i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    MXC_USBHS->index = i;
    MXC_USBHS->inmaxp = 0;
    MXC_USBHS->incsru = MXC_F_USBHS_INCSRU_DPKTBUFDIS;

    if (MXC_USBHS->incsrl & MXC_F_USBHS_INCSRL_INPKTRDY) {
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG | MXC_F_USBHS_INCSRL_FLUSHFIFO;
    } else {
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG;
    }

    MXC_USBHS->outmaxp = 0;
    MXC_USBHS->outcsru = MXC_F_USBHS_OUTCSRU_DPKTBUFDIS;

    if (MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY) {
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG | MXC_F_USBHS_OUTCSRL_FLUSHFIFO;
    } else {
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG;
    }
  }
  MXC_SYS_Crit_Exit();
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  unsigned const epn = tu_edpt_number(ep_addr);
  unsigned const dir_in = tu_edpt_dir(ep_addr);

  MXC_SYS_Crit_Enter();
  MXC_USBHS->index = epn;
  if (dir_in) {
    MXC_USBHS->intrinen &= ~TU_BIT(epn);
    MXC_USBHS->inmaxp = 0;
    MXC_USBHS->incsru = MXC_F_USBHS_INCSRU_DPKTBUFDIS;
    if (MXC_USBHS->incsrl & MXC_F_USBHS_INCSRL_INPKTRDY) {
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG | MXC_F_USBHS_INCSRL_FLUSHFIFO;
    } else {
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG;
    }
  } else {
    MXC_USBHS->introuten &= ~TU_BIT(epn);
    MXC_USBHS->outmaxp = 0;
    MXC_USBHS->outcsru = MXC_F_USBHS_OUTCSRU_DPKTBUFDIS;
    if (MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY) {
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG | MXC_F_USBHS_OUTCSRL_FLUSHFIFO;
    } else {
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG;
    }
  }
  MXC_SYS_Crit_Exit();
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  (void) rhport;
  bool ret;
  unsigned const epnum = tu_edpt_number(ep_addr);
  MXC_SYS_Crit_Enter();
  if (epnum) {
    _dcd.pipe_buf_is_fifo[tu_edpt_dir(ep_addr)] &= ~TU_BIT(epnum - 1);
    ret = edpt_n_xfer(rhport, ep_addr, buffer, total_bytes);
  } else
    ret = edpt0_xfer(rhport, ep_addr, buffer, total_bytes);
  MXC_SYS_Crit_Exit();
  return ret;
}

// Submit a transfer where is managed by FIFO, When complete dcd_event_xfer_complete() is invoked to notify the stack - optional, however, must be listed in usbd.c
bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t *ff, uint16_t total_bytes) {
  (void) rhport;
  bool ret;
  unsigned const epnum = tu_edpt_number(ep_addr);
  TU_ASSERT(epnum);
  MXC_SYS_Crit_Enter();
  _dcd.pipe_buf_is_fifo[tu_edpt_dir(ep_addr)] |= TU_BIT(epnum - 1);
  ret = edpt_n_xfer(rhport, ep_addr, (uint8_t *) ff, total_bytes);
  MXC_SYS_Crit_Exit();
  return ret;
}

// Stall endpoint
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  unsigned const epn = tu_edpt_number(ep_addr);
  MXC_SYS_Crit_Enter();
  MXC_USBHS->index = epn;
  if (0 == epn) {
    if (!ep_addr) { /* Ignore EP80 */
      _dcd.setup_packet.bmRequestType = REQUEST_TYPE_INVALID;
      _dcd.pipe0.buf = NULL;
      MXC_USBHS->csr0 = MXC_F_USBHS_CSR0_SEND_STALL;
    }
  } else {
    if (tu_edpt_dir(ep_addr)) { /* IN */
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_SENDSTALL;
    } else { /* OUT */
      TU_ASSERT(!(MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY), );
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_SENDSTALL;
    }
  }
  MXC_SYS_Crit_Exit();
}

// clear stall, data toggle is also reset to DATA0
void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  unsigned const epn = tu_edpt_number(ep_addr);
  MXC_SYS_Crit_Enter();
  MXC_USBHS->index = epn;
  if (tu_edpt_dir(ep_addr)) { /* IN */
    /* IN endpoint */
    if (MXC_USBHS->incsrl & MXC_F_USBHS_INCSRL_INPKTRDY) {
      /* Per musbhsfc_pg, only flush FIFO if IN packet loaded */
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG | MXC_F_USBHS_INCSRL_FLUSHFIFO;
    } else {
      MXC_USBHS->incsrl = MXC_F_USBHS_INCSRL_CLRDATATOG;
    }
  } else { /* OUT */
    /* Otherwise, must be OUT endpoint */
    if (MXC_USBHS->outcsrl & MXC_F_USBHS_OUTCSRL_OUTPKTRDY) {
      /* Per musbhsfc_pg, only flush FIFO if OUT packet is ready */
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG | MXC_F_USBHS_OUTCSRL_FLUSHFIFO;
    } else {
      MXC_USBHS->outcsrl = MXC_F_USBHS_OUTCSRL_CLRDATATOG;
    }
  }
  MXC_SYS_Crit_Exit();
}

/*-------------------------------------------------------------------
 * ISR
 *-------------------------------------------------------------------*/
void dcd_int_handler(uint8_t rhport) {
  uint_fast8_t is, txis, rxis;
  uint32_t mxm_int, mxm_int_en, mxm_is;
  uint32_t saved_index;

  /* Save current index register */
  saved_index = MXC_USBHS->index;

  is = MXC_USBHS->intrusb;   /* read and clear interrupt status */
  txis = MXC_USBHS->intrin;  /* read and clear interrupt status */
  rxis = MXC_USBHS->introut; /* read and clear interrupt status */

  /* These USB interrupt flags are W1C. */
  /* Order of volatile accesses must be separated for IAR */
  mxm_int = MXC_USBHS->mxm_int;
  mxm_int_en = MXC_USBHS->mxm_int_en;
  mxm_is = mxm_int & mxm_int_en;
  MXC_USBHS->mxm_int = mxm_is;

  is &= MXC_USBHS->intrusben; /* Clear disabled interrupts */

  if (mxm_is & MXC_F_USBHS_MXM_INT_NOVBUS) {
    dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
  }
  if (is & MXC_F_USBHS_INTRUSB_SOF_INT) {
    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
  }
  if (is & MXC_F_USBHS_INTRUSB_RESET_INT) {
    process_bus_reset(rhport);
  }
  if (is & MXC_F_USBHS_INTRUSB_RESUME_INT) {
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
  }
  if (is & MXC_F_USBHS_INTRUSB_SUSPEND_INT) {
    dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
  }

  txis &= MXC_USBHS->intrinen; /* Clear disabled interrupts */
  if (txis & MXC_F_USBHS_INTRIN_EP0_IN_INT) {
    process_ep0(rhport);
    txis &= ~TU_BIT(0);
  }
  while (txis) {
    unsigned const num = __builtin_ctz(txis);
    process_edpt_n(rhport, tu_edpt_addr(num, TUSB_DIR_IN));
    txis &= ~TU_BIT(num);
  }
  rxis &= MXC_USBHS->introuten; /* Clear disabled interrupts */
  while (rxis) {
    unsigned const num = __builtin_ctz(rxis);
    process_edpt_n(rhport, tu_edpt_addr(num, TUSB_DIR_OUT));
    rxis &= ~TU_BIT(num);
  }

  /* Restore register index before exiting ISR */
  MXC_USBHS->index = saved_index;
}

#endif
