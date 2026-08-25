/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 Koji Kitayama
 * SPDX-FileCopyrightText: Portions copyrighted (c) 2021 Roland Winistoerfer
 * SPDX-FileCopyrightText: Copyright (c) 2020 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_RUSB2)

#include "device/dcd.h"
#include "rusb2_common.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
enum {
  PIPE_COUNT = 10,
};

typedef struct {
  void      *buf;      /* the start address of a transfer data buffer */
  uint16_t  length;    /* the number of bytes in the buffer */
  uint16_t  remaining; /* the number of bytes remaining in the buffer */

  uint8_t ep; /* an assigned endpoint address */
  uint8_t ff; /* `buf` is TU_FUFO or POD */
  bool queued;      /* a transfer is submitted and not yet completed (independent of `buf`, which is
                       NULL for a zero-length read) -- used to decide clear-stall re-arm */
  bool zlp_pending; /* a zero-length IN packet couldn't be queued at submit (FIFO full); retry on BRDY */
} pipe_state_t;

typedef struct
{
  pipe_state_t pipe[PIPE_COUNT];
  uint8_t ep[2][16];   /* a lookup table for a pipe index from an endpoint address */
  // Track whether sof has been manually enabled
  bool sof_enabled;
} dcd_data_t;

static dcd_data_t _dcd;


//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+


// Transfer conditions specifiable for each pipe for most MCUs
// - Pipe 0: Control transfer with 64-byte single buffer
// - Pipes 1 and 2: Bulk or ISO
// - Pipes 3 to 5: Bulk
// - Pipes 6 to 9: Interrupt
//
// Note: for small mcu such as
// - RA2A1: only pipe 4-7 are available, and no support for ISO
static unsigned find_pipe(unsigned xfer_type) {
  #if defined(BSP_MCU_GROUP_RA2A1)
  const uint8_t pipe_idx_arr[4][2] = {
      { 0, 0 }, // Control
      { 0, 0 }, // Isochronous not supported
      { 4, 5 }, // Bulk
      { 6, 7 }, // Interrupt
  };
  #else
  const uint8_t pipe_idx_arr[4][2] = {
      { 0, 0 }, // Control
      { 1, 2 }, // Isochronous
      { 1, 5 }, // Bulk
      { 6, 9 }, // Interrupt
  };
  #endif

  // find backward since only pipe 1, 2 support ISO
  const uint8_t idx_first = pipe_idx_arr[xfer_type][0];
  const uint8_t idx_last  = pipe_idx_arr[xfer_type][1];

  for (int i = idx_last; i >= idx_first; i--) {
    if (0 == _dcd.pipe[i].ep) {
      return (unsigned)i;
    }
  }

  return 0;
}

static volatile uint16_t* get_pipectr(rusb2_reg_t *rusb, unsigned num) {
  if (num) {
    return (volatile uint16_t*)&(rusb->PIPE_CTR[num - 1]);
  } else {
    return (volatile uint16_t*)&(rusb->DCPCTR);
  }
}

static volatile reg_pipetre_t* get_pipetre(rusb2_reg_t *rusb, unsigned num) {
  volatile reg_pipetre_t* tre = NULL;
  if ((1 <= num) && (num <= 5)) {
    tre = (volatile reg_pipetre_t*)&(rusb->PIPE_TR[num - 1].E);
  }
  return tre;
}

static volatile uint16_t* ep_addr_to_pipectr(uint8_t rhport, unsigned ep_addr) {
  rusb2_reg_t *rusb = RUSB2_REG(rhport);
  const unsigned epn = tu_edpt_number((uint8_t)ep_addr);

  if (epn) {
    const unsigned dir = tu_edpt_dir((uint8_t)ep_addr);
    const unsigned num = _dcd.ep[dir][epn];
    return get_pipectr(rusb, num);
  } else {
    return get_pipectr(rusb, 0);
  }
}

static uint16_t edpt0_max_packet_size(rusb2_reg_t* rusb) {
  return (uint16_t)rusb->DCPMAXP_b.MXPS;
}

static uint16_t edpt_max_packet_size(rusb2_reg_t *rusb, unsigned num) {
  rusb->PIPESEL = (uint16_t)num;
  return rusb->PIPEMAXP;
}

// Select the D0FIFO for `num` and wait until its buffer is ready for CPU access. Both flags
// normally settle within a few cycles (the pipe was just armed, or a BRDY freed a plane). But an
// IN pipe whose double buffer is already full stalls FRDY until the host drains it, and a
// no-handshake iso IN endpoint the host has stopped polling never drains at all — so FRDY would
// hang forever. This runs with the USB IRQ masked, so a naked spin freezes the whole stack; bound
// it and let the caller abort the FIFO access. Returns false on timeout.
#define RUSB2_FIFO_READY_SPIN 100000u
static inline bool pipe_wait_for_ready(rusb2_reg_t *rusb, unsigned num) {
  uint32_t spin = RUSB2_FIFO_READY_SPIN;
  while ( rusb->D0FIFOSEL_b.CURPIPE != num ) { if (!spin--) return false; }
  spin = RUSB2_FIFO_READY_SPIN;
  while ( !rusb->D0FIFOCTR_b.FRDY ) { if (!spin--) return false; }
  return true;
}

//--------------------------------------------------------------------+
// Pipe Transfer
//--------------------------------------------------------------------+
static bool pipe0_xfer_in(rusb2_reg_t *rusb) {
  pipe_state_t  *pipe = &_dcd.pipe[0];
  const unsigned rem  = pipe->remaining;

  if (!rem) {
    pipe->buf = NULL;
    return true;
  }

  const uint16_t mps = edpt0_max_packet_size(rusb);
  const uint16_t len = tu_min16(mps, rem);
  void          *buf = pipe->buf;

  if (len) {
    // uint16_t           fifo_sel = RUSB2_CFIFOSEL_ISEL_WRITE | FIFOSEL_BIGEND;
    tu_hwfifo_access_t access_mode;
    access_mode.param = (uintptr_t)rusb;
    //
    if (rusb2_is_highspeed_reg(rusb)) {
      //   fifo_sel |= RUSB2_FIFOSEL_MBW_32BIT;
      access_mode.data_stride = 4u;
    } else {
      //   fifo_sel |= RUSB2_FIFOSEL_MBW_16BIT;
      access_mode.data_stride = 2u;
    }
    // rusb->CFIFOSEL = fifo_sel;
    // while (0 == (rusb->CFIFOSEL & RUSB2_CFIFOSEL_ISEL_WRITE)) {}

    if (pipe->ff) {
      tu_hwfifo_write_from_fifo(&rusb->CFIFO, (tu_fifo_t *)buf, len, &access_mode);
    } else {
      tu_hwfifo_write(&rusb->CFIFO, buf, len, &access_mode);
      pipe->buf = (uint8_t *)buf + len;
    }
  }

  if (len < mps) {
    rusb->CFIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;
  }

  pipe->remaining = rem - len;
  return false;
}

static bool pipe0_xfer_out(rusb2_reg_t *rusb) {
  pipe_state_t  *pipe = &_dcd.pipe[0];
  const unsigned rem  = pipe->remaining;

  // BRDY with no armed transfer: a back-to-back data-stage packet beat the PID=NAK below (the
  // host has already ACKed it). Park it in the DCP buffer — an unread buffer NAKs further OUTs —
  // and let process_pipe0_xfer deliver it when usbd arms the next chunk. BCLR here would silently
  // drop the packet and shift every later chunk by one (usbtest ctrl_out corruption at ra4m1).
  if (pipe->buf == NULL && rem == 0) {
    rusb->DCPCTR = RUSB2_PIPE_CTR_PID_NAK;
    return false;
  }

  const uint16_t mps = edpt0_max_packet_size(rusb);
  const uint16_t vld = rusb->CFIFOCTR_b.DTLN;
  const uint16_t len = tu_min16(tu_min16(rem, mps), vld);
  void          *buf = pipe->buf;

  if (len) {
    tu_hwfifo_access_t access_mode = {.data_stride = (rusb2_is_highspeed_reg(rusb) ? 4u : 2u),
                                      .param       = (uintptr_t)rusb};

    if (pipe->ff) {
      tu_hwfifo_read_to_fifo(&rusb->CFIFO, (tu_fifo_t *)buf, len, &access_mode);
    } else {
      tu_hwfifo_read(&rusb->CFIFO, buf, len, &access_mode);
      pipe->buf = (uint8_t *)buf + len;
    }
  }

  if (len < mps) {
    rusb->CFIFOCTR = RUSB2_CFIFOCTR_BCLR_Msk;
  }

  pipe->remaining = rem - len;
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    // Flow-control the single-buffer control pipe: NAK further OUT until usbd arms the next
    // data-stage chunk. usbd receives a multi-packet control-OUT one CFG_TUD_ENDPOINT0_SIZE
    // packet per submit; without this the DCP auto-accepts the next back-to-back packet into the
    // just-emptied buffer and the following BRDY (remaining==0) BCLR-discards it, dropping 64
    // bytes mid-transfer (e.g. usbtest ctrl_out 512B). RA4M1 UM R01UH0887 DCPCTR.PID.
    rusb->DCPCTR = RUSB2_PIPE_CTR_PID_NAK;
    return true;
  }

  return false;
}

static bool pipe_xfer_in(rusb2_reg_t* rusb, unsigned num)
{
  pipe_state_t  *pipe = &_dcd.pipe[num];
  const unsigned rem  = pipe->remaining;

  if (!rem) {
    pipe->buf = NULL;
    return true;
  }

  const uint16_t fifo_sel     = num | FIFOSEL_BIGEND;
  const bool     is_highspeed = rusb2_is_highspeed_reg(rusb);
  if (is_highspeed) {
    rusb->D0FIFOSEL = fifo_sel | RUSB2_FIFOSEL_MBW_32BIT;
  } else {
    rusb->D0FIFOSEL = fifo_sel | RUSB2_FIFOSEL_MBW_16BIT;
  }

  const uint16_t mps = edpt_max_packet_size(rusb, num);
  if (!pipe_wait_for_ready(rusb, num)) {
    // Buffer never came ready (double-buffered IN pipe full, host not draining). Drop this load;
    // the transfer stays pending and is retried when a BRDY frees a plane or the pipe is re-armed.
    rusb->D0FIFOSEL = 0;
    return false;
  }
  uint16_t len = tu_min16(rem, mps);
  void    *buf = pipe->buf;

  if (len) {
    tu_hwfifo_access_t access_mode = {.data_stride = (rusb2_is_highspeed_reg(rusb) ? 4u : 2u),
                                      .param       = (uintptr_t)rusb};
    if (pipe->ff) {
      tu_hwfifo_write_from_fifo(&rusb->D0FIFO, (tu_fifo_t *)buf, len, &access_mode);
    } else {
      tu_hwfifo_write(&rusb->D0FIFO, buf, len, &access_mode);
      pipe->buf = (uint8_t *)buf + len;
    }
  }

  if (len < mps) {
    rusb->D0FIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;
  }

  rusb->D0FIFOSEL = 0;
  while (rusb->D0FIFOSEL_b.CURPIPE) {} /* if CURPIPE bits changes, check written value */

  pipe->remaining = rem - len;

  return false;
}

static bool pipe_xfer_out(rusb2_reg_t* rusb, unsigned num)
{
  pipe_state_t  *pipe = &_dcd.pipe[num];
  const uint16_t rem  = pipe->remaining;

  uint16_t fifo_sel = num | FIFOSEL_BIGEND;
  if (rusb2_is_highspeed_reg(rusb)) {
    fifo_sel |= RUSB2_FIFOSEL_MBW_32BIT;
  } else {
    fifo_sel |= RUSB2_FIFOSEL_MBW_16BIT;
  }
  rusb->D0FIFOSEL = fifo_sel;

  const uint16_t mps = edpt_max_packet_size(rusb, num);
  if (!pipe_wait_for_ready(rusb, num)) {
    rusb->D0FIFOSEL = 0;
    return false; // FIFO not ready; leave the receive pending (BRDY re-enters when data arrives)
  }

  const uint16_t vld  = (uint16_t)rusb->D0FIFOCTR_b.DTLN;
  const uint16_t len  = tu_min16(tu_min16(rem, mps), vld);
  void          *buf  = pipe->buf;

  if (len) {
    tu_hwfifo_access_t access_mode = {.data_stride = (rusb2_is_highspeed_reg(rusb) ? 4u : 2u),
                                      .param       = (uintptr_t)rusb};
    if (pipe->ff) {
      tu_hwfifo_read_to_fifo(&rusb->D0FIFO, (tu_fifo_t *)buf, len, &access_mode);
    } else {
      tu_hwfifo_read(&rusb->D0FIFO, buf, len, &access_mode);
      pipe->buf = (uint8_t *)buf + len;
    }
  }

  if (len < mps) {
    rusb->D0FIFOCTR = RUSB2_CFIFOCTR_BCLR_Msk;
  }

  rusb->D0FIFOSEL = 0;
  while (rusb->D0FIFOSEL_b.CURPIPE) {} /* if CURPIPE bits changes, check written value */

  pipe->remaining = rem - len;
  if ((len < mps) || (rem == len)) {
    pipe->buf = NULL;
    return NULL != buf;
  }

  return false;
}

static void process_setup_packet(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  if (0 == (rusb->INTSTS0 & RUSB2_INTSTS0_VALID_Msk)) return;

  rusb->CFIFOCTR = RUSB2_CFIFOCTR_BCLR_Msk;
  uint16_t setup_packet[4] = {
      tu_htole16(rusb->USBREQ),
      tu_htole16(rusb->USBVAL),
      tu_htole16(rusb->USBINDX),
      tu_htole16(rusb->USBLENG)
  };

  rusb->INTSTS0 = ~((uint16_t) RUSB2_INTSTS0_VALID_Msk);
  dcd_event_setup_received(rhport, (const uint8_t*)&setup_packet[0], true);
}

static void process_status_completion(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  uint8_t ep_addr;
  /* Check the data stage direction */
  if (rusb->CFIFOSEL & RUSB2_CFIFOSEL_ISEL_WRITE) {
    /* IN transfer. */
    ep_addr = tu_edpt_addr(0, TUSB_DIR_IN);
  } else {
    /* OUT transfer. */
    ep_addr = tu_edpt_addr(0, TUSB_DIR_OUT);
  }

  dcd_event_xfer_complete(rhport, ep_addr, 0, XFER_RESULT_SUCCESS, true);
}

// Report a completed transfer on `num` and reset its bookkeeping. Single completion path for the
// BRDY handler and the EP0 parked-packet drain, so they can't diverge (e.g. on clearing `queued`).
static void pipe_xfer_complete(uint8_t rhport, unsigned num, bool in_isr) {
  pipe_state_t *pipe = &_dcd.pipe[num];
  pipe->queued = false;
  dcd_event_xfer_complete(rhport, pipe->ep, pipe->length - pipe->remaining,
                          XFER_RESULT_SUCCESS, in_isr);
}

static bool process_pipe0_xfer(uint8_t rhport, rusb2_reg_t *rusb, int buffer_type, uint8_t ep_addr, void *buffer,
                               uint16_t total_bytes) {
  uint16_t fifo_sel =
    (rusb2_is_highspeed_reg(rusb) ? RUSB2_FIFOSEL_MBW_32BIT : RUSB2_FIFOSEL_MBW_16BIT) | FIFOSEL_BIGEND;

  /* configure fifo direction and access unit settings */
  if (ep_addr != 0) {
    // Control IN
    fifo_sel |= RUSB2_CFIFOSEL_ISEL_WRITE;
  }
  rusb->CFIFOSEL = fifo_sel;
  while ((rusb->CFIFOSEL & RUSB2_CFIFOSEL_ISEL_WRITE) != (fifo_sel & RUSB2_CFIFOSEL_ISEL_WRITE)) {
    // wait until ISEL_WRITE take effect
  }

  pipe_state_t *pipe = &_dcd.pipe[0];
  pipe->ff           = buffer_type;
  pipe->length       = total_bytes;
  pipe->remaining    = total_bytes;

  if (total_bytes) {
    pipe->buf = buffer;
    if (ep_addr) {
      /* IN */
      TU_ASSERT(rusb->DCPCTR_b.BSTS && (rusb->USBREQ & 0x80));
      pipe0_xfer_in(rusb);
    } else if (rusb->CFIFOCTR_b.DTLN > 0) {
      /* OUT: a back-to-back packet parked by pipe0_xfer_out already sits in the DCP buffer (its
         BRDY has fired and been cleared) — deliver it into this chunk now; no new BRDY will come
         for it. Runs with the USB IRQ masked (dcd_edpt_xfer). Detected via the hardware DTLN
         rather than a driver flag: the BCLR at SETUP/bus-reset then self-heals any parked state. */
      if (pipe0_xfer_out(rusb)) {
        pipe_xfer_complete(rhport, 0, false);
        return true; // PID stays NAK (set by pipe0_xfer_out) until the next chunk is armed
      }
    }
    rusb->DCPCTR = RUSB2_PIPE_CTR_PID_BUF;
  } else {
    /* ZLP */
    pipe->buf = NULL;
    rusb->DCPCTR = RUSB2_DCPCTR_CCPL_Msk | RUSB2_PIPE_CTR_PID_BUF;
  }

  return true;
}

// Queue a zero-length IN packet. Returns false if the FIFO buffer wasn't free (double-buffered pipe
// full, host not draining) so BVAL couldn't be written -- the caller retries on the next BRDY.
static bool pipe_zlp_in(rusb2_reg_t *rusb, unsigned num) {
  rusb->D0FIFOSEL = (uint16_t) num;
  const bool ready = pipe_wait_for_ready(rusb, num);
  if (ready) {
    rusb->D0FIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;
  }
  rusb->D0FIFOSEL = 0;
  // deselect completes within a few bus cycles (not host-dependent), but bound it anyway: this
  // runs with the USB IRQ masked, where any stuck spin freezes the whole stack
  uint32_t spin = RUSB2_FIFO_READY_SPIN;
  while (rusb->D0FIFOSEL_b.CURPIPE) {
    if (!spin--) { break; }
  }
  return ready;
}

static bool process_pipe_xfer(rusb2_reg_t* rusb, int buffer_type, uint8_t ep_addr, void* buffer, uint16_t total_bytes)
{
  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned dir = tu_edpt_dir(ep_addr);
  const unsigned num = _dcd.ep[dir][epn];

  TU_ASSERT(num);

  pipe_state_t *pipe  = &_dcd.pipe[num];
  pipe->ff          = buffer_type;
  pipe->buf         = buffer;
  pipe->length      = total_bytes;
  pipe->remaining   = total_bytes;
  pipe->queued      = true;
  pipe->zlp_pending = false;

  if (dir) {
    /* IN */
    if (total_bytes) {
      pipe_xfer_in(rusb, num);
    } else {
      /* ZLP: if the FIFO buffer isn't free yet, defer the queue to the next BRDY (see process_pipe_brdy) */
      pipe->zlp_pending = !pipe_zlp_in(rusb, num);
    }
  } else {
    // OUT
    volatile reg_pipetre_t *pt = get_pipetre(rusb, num);

    if (pt) {
      const uint16_t     mps = edpt_max_packet_size(rusb, num);
      volatile uint16_t *ctr = get_pipectr(rusb, num);

      if (*ctr & 0x3) *ctr = RUSB2_PIPE_CTR_PID_NAK;

      pt->TRE   = TU_BIT(8);
      pt->TRN   = (total_bytes + mps - 1) / mps;
      pt->TRENB = 1;
      *ctr = RUSB2_PIPE_CTR_PID_BUF;
    }
  }

  //  TU_LOG2("X %x %d %d\r\n", ep_addr, total_bytes, buffer_type);
  return true;
}

static bool process_edpt_xfer(uint8_t rhport, rusb2_reg_t* rusb, int buffer_type, uint8_t ep_addr, void* buffer, uint16_t total_bytes)
{
  const unsigned epn = tu_edpt_number(ep_addr);
  if (0 == epn) {
    return process_pipe0_xfer(rhport, rusb, buffer_type, ep_addr, buffer, total_bytes);
  } else {
    return process_pipe_xfer(rusb, buffer_type, ep_addr, buffer, total_bytes);
  }
}

static void process_pipe0_bemp(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  bool         completed = pipe0_xfer_in(rusb);
  if (completed) {
    pipe_state_t *pipe = &_dcd.pipe[0];
    dcd_event_xfer_complete(rhport, tu_edpt_addr(0, TUSB_DIR_IN),
                            pipe->length, XFER_RESULT_SUCCESS, true);
  }
}

static void process_pipe_brdy(uint8_t rhport, unsigned num)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  pipe_state_t  *pipe = &_dcd.pipe[num];
  const unsigned dir  = tu_edpt_dir(pipe->ep);
  bool completed;

  if (dir) {
    /* IN */
    if (pipe->zlp_pending) {
      // The submit-time ZLP couldn't be queued (FIFO full); a freed buffer plane lets us queue it
      // now. Don't report completion until the ZLP is actually queued (and then sent, next BRDY),
      // otherwise a spurious BRDY would complete a zero-length IN the host never received.
      pipe->zlp_pending = !pipe_zlp_in(rusb, num);
      completed = false;
    } else {
      completed = pipe_xfer_in(rusb, num);
    }
  } else {
    // OUT
    if (num) {
      completed = pipe_xfer_out(rusb, num);
    } else {
      completed = pipe0_xfer_out(rusb);
    }
  }
  if (completed) {
    pipe_xfer_complete(rhport, num, true);
    //  TU_LOG1("C %d %d\r\n", num, pipe->length - pipe->remaining);
  }
}

static void process_bus_reset(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);

  rusb->BEMPENB = 1;
  rusb->BRDYENB = 1;
  rusb->CFIFOCTR = RUSB2_CFIFOCTR_BCLR_Msk;

  rusb->D0FIFOSEL = 0;
  while (rusb->D0FIFOSEL_b.CURPIPE) {} /* if CURPIPE bits changes, check written value */

  rusb->D1FIFOSEL = 0;
  while (rusb->D1FIFOSEL_b.CURPIPE) {} /* if CURPIPE bits changes, check written value */

  volatile uint16_t *ctr = (volatile uint16_t*)((uintptr_t) (&rusb->PIPE_CTR[0]));
  volatile uint16_t *tre = (volatile uint16_t*)((uintptr_t) (&rusb->PIPE_TR[0].E));

  for (uint16_t i = 1; i <= 5; ++i) {
    rusb->PIPESEL = i;
    rusb->PIPECFG = 0;
    *ctr = RUSB2_PIPE_CTR_ACLRM_Msk;
    *ctr = 0;
    ++ctr;
    *tre = TU_BIT(8);
    tre += 2;
  }

  for (uint16_t i = 6; i <= 9; ++i) {
    rusb->PIPESEL = i;
    rusb->PIPECFG = 0;
    *ctr = RUSB2_PIPE_CTR_ACLRM_Msk;
    *ctr = 0;
    ++ctr;
  }
  tu_varclr(&_dcd);

  TU_LOG3("Bus reset, RHST = %u\r\n", rusb->DVSTCTR0_b.RHST);
  tusb_speed_t speed;
  switch(rusb->DVSTCTR0 & RUSB2_DVSTCTR0_RHST_Msk) {
    case RUSB2_DVSTCTR0_RHST_LS:
      speed = TUSB_SPEED_LOW;
      break;

    case RUSB2_DVSTCTR0_RHST_FS:
      speed = TUSB_SPEED_FULL;
      break;

    case RUSB2_DVSTCTR0_RHST_HS:
      speed = TUSB_SPEED_HIGH;
      break;

    default:
      TU_ASSERT(false, );
  }

  dcd_event_bus_reset(rhport, speed, true);
}

static void process_set_address(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  const uint16_t addr = (uint16_t)rusb->USBADDR_b.USBADDR;
  if (!addr) {
    return;
  }

  const tusb_control_request_t setup_packet = {
#if defined(__CCRX__)
      .bmRequestType = { 0 },  /* Note: CCRX needs the braces over this struct member */
  #else
    .bmRequestType = 0,
  #endif
    .bRequest = TUSB_REQ_SET_ADDRESS,
    .wValue   = addr,
    .wIndex   = 0,
    .wLength  = 0,
  };

  dcd_event_setup_received(rhport, (const uint8_t *) &setup_packet, true);
}

/*------------------------------------------------------------------*/
/* Device API
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

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  rusb2_module_start(rhport, true);

  // We disable SOF for now until needed later on.
  // Since TinyUSB doesn't use SOF for now, and this interrupt often (1ms interval)
  _dcd.sof_enabled = false;

#ifdef RUSB2_SUPPORT_HIGHSPEED
  if ( rusb2_is_highspeed_rhport(rhport) ) {
    rusb->SYSCFG_b.HSE = TUD_OPT_HIGH_SPEED ? 1 : 0; // FS-only build: no HS chirp
    rusb2_utmi_phy_powerup(rusb);

    rusb->SYSCFG_b.DRPD = 0;
    rusb->SYSCFG_b.USBE = 1;

    // Set CPU bus wait time (fine tunne later)
    // rusb2->BUSWAIT |= 0x0F00U;

    rusb->PHYSET_b.REPSEL = 1;
  } else
#endif
  {
    rusb->SYSCFG_b.SCKE = 1;
    while (!rusb->SYSCFG_b.SCKE) {}
    rusb->SYSCFG_b.DRPD = 0;
    rusb->SYSCFG_b.DCFM = 0;
    rusb->SYSCFG_b.USBE = 1;

    // MCU specific PHY init
    rusb2_phy_init();

    rusb->PHYSLEW = 0x5;
    rusb->DPUSR0R_FS_b.FIXPHY0 = 0u; /* USB_BASE Transceiver Output fixed */
  }

  /* Setup default control pipe */
  rusb->DCPMAXP_b.MXPS = 64;

  rusb->INTSTS0 = 0;
  rusb->INTENB0 = RUSB2_INTSTS0_VBINT_Msk | RUSB2_INTSTS0_BRDY_Msk | RUSB2_INTSTS0_BEMP_Msk |
                  RUSB2_INTSTS0_DVST_Msk | RUSB2_INTSTS0_CTRT_Msk | (_dcd.sof_enabled ? RUSB2_INTSTS0_SOFR_Msk : 0) |
                  RUSB2_INTSTS0_RESM_Msk;
  rusb->BEMPENB = 1;
  rusb->BRDYENB = 1;

  // If VBUS (detect) pin is not used, application need to call tud_connect() manually after tud_init()
  if (rusb->INTSTS0_b.VBSTS) {
    dcd_connect(rhport);
  }

  return true;
}

void dcd_int_enable(uint8_t rhport) {
  rusb2_int_enable(rhport);
}

void dcd_int_disable(uint8_t rhport) {
  rusb2_int_disable(rhport);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;
  (void) dev_addr;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  rusb->DVSTCTR0_b.WKUP = 1;
}

void dcd_connect(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);

  if ( rusb2_is_highspeed_rhport(rhport)) {
    rusb->SYSCFG_b.CNEN = 1;
  }
  rusb->SYSCFG_b.DPRPU = 1;
}

void dcd_disconnect(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  rusb->SYSCFG_b.DPRPU = 0;
}

void dcd_sof_enable(uint8_t rhport, bool en)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);
  _dcd.sof_enabled = en;
  rusb->INTENB0_b.SOFE = en ? 1: 0;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * ep_desc)
{
  (void)rhport;

  rusb2_reg_t * rusb = RUSB2_REG(rhport);
  const uint8_t  ep_addr = ep_desc->bEndpointAddress;
  const unsigned epn     = tu_edpt_number(ep_addr);
  const unsigned dir     = tu_edpt_dir(ep_addr);
  const unsigned xfer    = ep_desc->bmAttributes.xfer;

  const unsigned mps = tu_edpt_packet_size(ep_desc);

  if (xfer == TUSB_XFER_ISOCHRONOUS) {
    // Fullspeed ISO is limit to 256 bytes
    if ( !rusb2_is_highspeed_rhport(rhport) && mps > 256) {
      return false;
    }
  } else if (xfer == TUSB_XFER_INTERRUPT) {
    // Interrupt pipes (6-9) have a fixed 64-byte buffer even in high speed (RA6M5 UM 29.1);
    // a larger PIPEMAXP would enumerate, then silently truncate every transfer
    TU_ASSERT(mps <= 64);
  }

  // Re-opening an endpoint must reuse its pipe: usbd_edpt_close() is a no-op on ISO_ALLOC ports,
  // so a class's close/open across SET_INTERFACE (e.g. video's notification endpoint) would
  // otherwise allocate a second pipe with the same EPNUM and leak pipes until exhaustion.
  unsigned num = _dcd.ep[dir][epn];
  if (num == 0) {
    num = find_pipe(xfer);
    TU_ASSERT(num);
  }

  _dcd.pipe[num].ep = ep_addr;
  _dcd.ep[dir][epn] = num;

  /* setup pipe */
  dcd_int_disable(rhport);

  rusb->PIPESEL = num;
  if ( rusb2_is_highspeed_rhport(rhport) ) {
    // PIPEBUF is PIPESEL-windowed (RA6M5 UM 29.2.35): write it after selecting the pipe.
    // FIXME BUFNMB is a fixed 0x08 for every pipe; a real per-pipe allocation scheme is needed.
    rusb->PIPEBUF = 0x7C08;
  }
  rusb->PIPEMAXP = mps;
  volatile uint16_t *ctr = get_pipectr(rusb, num);
  *ctr = RUSB2_PIPE_CTR_ACLRM_Msk | RUSB2_PIPE_CTR_SQCLR_Msk;
  *ctr = 0;
  unsigned cfg = (dir << 4) | epn;

  if (xfer == TUSB_XFER_BULK) {
    cfg |= (RUSB2_PIPECFG_TYPE_BULK | RUSB2_PIPECFG_SHTNAK_Msk | RUSB2_PIPECFG_DBLB_Msk);
  } else if (xfer == TUSB_XFER_INTERRUPT) {
    cfg |= RUSB2_PIPECFG_TYPE_INT;
  } else {
    cfg |= (RUSB2_PIPECFG_TYPE_ISO | RUSB2_PIPECFG_DBLB_Msk);
  }

  rusb->PIPECFG = cfg;
  rusb->BRDYSTS = 0x3FFu ^ TU_BIT(num);
  rusb->BRDYENB |= TU_BIT(num);

  if (dir || (xfer != TUSB_XFER_BULK)) {
    *ctr = RUSB2_PIPE_CTR_PID_BUF;
  }

  // TU_LOG1("O %d %x %x\r\n", rusb->PIPESEL, rusb->PIPECFG, rusb->PIPEMAXP);
  dcd_int_enable(rhport);

  return true;
}

static void edpt_close(uint8_t rhport, uint8_t ep_addr);

void dcd_edpt_close_all(uint8_t rhport)
{
  unsigned i = TU_ARRAY_SIZE(_dcd.pipe);
  dcd_int_disable(rhport);
  while (--i) { /* Close all pipes except 0 */
    const unsigned ep_addr = _dcd.pipe[i].ep;
    if (!ep_addr) {
      continue;
    }
    edpt_close(rhport, (uint8_t)ep_addr);
  }
  dcd_int_enable(rhport);
}

// Internal helper: on this (ISO_ALLOC) IP the stack no longer calls dcd_edpt_close(); only
// dcd_edpt_close_all() uses it to tear down each pipe.
static void edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  rusb2_reg_t * rusb = RUSB2_REG(rhport);
  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned dir = tu_edpt_dir(ep_addr);
  const unsigned num = _dcd.ep[dir][epn];

  rusb->BRDYENB &= (uint16_t)~TU_BIT(num);
  volatile uint16_t *ctr = get_pipectr(rusb, num);
  *ctr = 0;
  rusb->PIPESEL = (uint16_t)num;
  rusb->PIPECFG = 0;
  _dcd.pipe[num].ep          = 0;
  _dcd.pipe[num].queued      = false;
  _dcd.pipe[num].zlp_pending = false;
  _dcd.ep[dir][epn] = 0;
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  rusb2_reg_t * rusb = RUSB2_REG(rhport);
  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned dir = tu_edpt_dir(ep_addr);

  // Fullspeed ISO is limited to 256 bytes
  if (!rusb2_is_highspeed_rhport(rhport) && largest_packet_size > 256) {
    return false;
  }

  // Reserve an ISO-capable pipe (1 or 2) once; it persists across altsetting changes so
  // dcd_edpt_iso_activate() only has to re-arm it in place (no pipe free/realloc, which on this
  // shared-register IP would churn PIPESEL/PIPECFG and disturb the other pipes).
  const unsigned num = find_pipe(TUSB_XFER_ISOCHRONOUS);
  TU_ASSERT(num);
  _dcd.pipe[num].ep = ep_addr;
  _dcd.ep[dir][epn] = num;

  dcd_int_disable(rhport);
  rusb->PIPESEL = (uint16_t) num;
  if (rusb2_is_highspeed_rhport(rhport)) {
    // PIPEBUF is PIPESEL-windowed (RA6M5 UM 29.2.35): write it after selecting the pipe.
    // FIXME (as in dcd_edpt_open): BUFNMB is a fixed 0x08 for every pipe; a real allocator is needed.
    rusb->PIPEBUF = 0x7C08;
  }
  rusb->PIPEMAXP = largest_packet_size;
  volatile uint16_t *ctr = get_pipectr(rusb, num);
  *ctr = RUSB2_PIPE_CTR_ACLRM_Msk | RUSB2_PIPE_CTR_SQCLR_Msk;
  *ctr = 0; // leave the pipe NAKing until activated
  rusb->PIPECFG = (uint16_t) ((dir << 4) | epn | RUSB2_PIPECFG_TYPE_ISO | RUSB2_PIPECFG_DBLB_Msk);
  rusb->BRDYSTS = (uint16_t) (0x3FFu ^ TU_BIT(num));
  rusb->BRDYENB |= TU_BIT(num);
  dcd_int_enable(rhport);
  return true;
}

bool dcd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
  rusb2_reg_t * rusb = RUSB2_REG(rhport);
  const uint8_t ep_addr = desc_ep->bEndpointAddress;
  const unsigned epn = tu_edpt_number(ep_addr);
  const unsigned dir = tu_edpt_dir(ep_addr);
  const unsigned num = _dcd.ep[dir][epn];
  TU_ASSERT(num); // must have been iso-alloc'd

  dcd_int_disable(rhport);
  rusb->PIPESEL = (uint16_t) num;
  rusb->PIPEMAXP = tu_edpt_packet_size(desc_ep);
  volatile uint16_t *ctr = get_pipectr(rusb, num);
  *ctr = RUSB2_PIPE_CTR_ACLRM_Msk | RUSB2_PIPE_CTR_SQCLR_Msk; // abort in-flight + reset data toggle
  *ctr = 0;
  // a transfer armed before SET_INTERFACE survives to here (no dcd close on this port): drop the
  // stale bookkeeping so a BRDY firing before the class re-arms can't replay it
  pipe_state_t *pipe = &_dcd.pipe[num];
  pipe->buf = NULL;
  pipe->remaining = 0;
  pipe->queued = false;
  pipe->zlp_pending = false;
  *ctr = RUSB2_PIPE_CTR_PID_BUF; // enable
  dcd_int_enable(rhport);
  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes, bool is_isr)
{
  (void) is_isr;
  rusb2_reg_t* rusb = RUSB2_REG(rhport);

  dcd_int_disable(rhport);
  bool r = process_edpt_xfer(rhport, rusb, 0, ep_addr, buffer, total_bytes);
  dcd_int_enable(rhport);

  return r;
}

bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes, bool is_isr)
{
  (void) is_isr;
  // USB buffers always work in bytes so to avoid unnecessary divisions we demand item_size = 1
  rusb2_reg_t* rusb = RUSB2_REG(rhport);

  dcd_int_disable(rhport);
  bool r = process_edpt_xfer(rhport, rusb, 1, ep_addr, ff, total_bytes);
  dcd_int_enable(rhport);

  return r;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  volatile uint16_t *ctr = ep_addr_to_pipectr(rhport, ep_addr);
  if (!ctr) {
    return;
  }
  dcd_int_disable(rhport);
  const uint32_t pid = *ctr & 0x3;
  *ctr = pid | RUSB2_PIPE_CTR_PID_STALL;
  *ctr = RUSB2_PIPE_CTR_PID_STALL;
  dcd_int_enable(rhport);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  rusb2_reg_t * rusb = RUSB2_REG(rhport);
  volatile uint16_t *ctr = ep_addr_to_pipectr(rhport, ep_addr);
  if (!ctr) {
    return;
  }

  dcd_int_disable(rhport);
  *ctr = RUSB2_PIPE_CTR_SQCLR_Msk;

  if (tu_edpt_dir(ep_addr)) { /* IN */
    *ctr = RUSB2_PIPE_CTR_PID_BUF;
  } else {
    const unsigned num = _dcd.ep[0][tu_edpt_number(ep_addr)];
    rusb->PIPESEL = (uint16_t)num;
    // Drop any packet parked in the buffer while halted: a data-OUT packet the host sent before
    // aborting its transfer would otherwise be delivered into the next read after recovery
    // (BOT reset + clear-halt re-arms a 31-byte CBW read which then receives stale WRITE data,
    // "SCSI CBW is not valid" -> stall -> reset loop; ra6m5 msc write wedge).
    *ctr = RUSB2_PIPE_CTR_ACLRM_Msk;
    *ctr = 0;
    // Non-bulk OUT re-enables straight away. Bulk OUT is normally armed together with its transaction
    // counter (TRE) by process_pipe_xfer(), so we don't blindly re-enable it here — but if a receive
    // was already armed (still queued), SQCLR above just left it NAKing. Re-assert BUF so it keeps
    // receiving; the class driver still considers that read submitted and never re-arms it, so
    // otherwise the endpoint NAKs forever (usbtest toggle test 29 clears the halt on an armed pipe).
    // `queued` (not `buf`) is the armed test: a zero-length OUT read has buf==NULL yet is armed.
    if (rusb->PIPECFG_b.TYPE != 1 || _dcd.pipe[num].queued) {
      *ctr = RUSB2_PIPE_CTR_PID_BUF;
    }
  }
  dcd_int_enable(rhport);
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+

#if defined(__CCRX__)
TU_ATTR_ALWAYS_INLINE static inline unsigned __builtin_ctz(unsigned int value) {
  unsigned int count = 0;
  while ((value & 1) == 0) {
    value >>= 1;
    count++;
  }
  return count;
}
#endif

void dcd_int_handler(uint8_t rhport)
{
  rusb2_reg_t* rusb = RUSB2_REG(rhport);

  uint16_t is0 = rusb->INTSTS0;

  /* clear active bits except VALID (don't write 0 to already cleared bits according to the HW manual) */
  rusb->INTSTS0 = ~((RUSB2_INTSTS0_CTRT_Msk | RUSB2_INTSTS0_DVST_Msk | RUSB2_INTSTS0_SOFR_Msk |
                     RUSB2_INTSTS0_RESM_Msk | RUSB2_INTSTS0_VBINT_Msk) & is0) | RUSB2_INTSTS0_VALID_Msk;

  // VBUS changes
  if ( is0 & RUSB2_INTSTS0_VBINT_Msk ) {
    if ( rusb->INTSTS0_b.VBSTS ) {
      dcd_connect(rhport);
    } else {
      dcd_disconnect(rhport);
    }
  }

  // Resumed
  if ( is0 & RUSB2_INTSTS0_RESM_Msk ) {
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
    if (!_dcd.sof_enabled) {
      rusb->INTENB0_b.SOFE = 0;
    }
  }

  // SOF received
  if ( (is0 & RUSB2_INTSTS0_SOFR_Msk) && rusb->INTENB0_b.SOFE ) {
    // USBD will exit suspended mode when SOF event is received
    const uint32_t frame = rusb->FRMNUM_b.FRNM;
    dcd_event_sof(rhport, frame, true);
    if (!_dcd.sof_enabled) {
      rusb->INTENB0_b.SOFE = 0;
    }
  }

  // Device state changes
  if ( is0 & RUSB2_INTSTS0_DVST_Msk ) {
    switch (is0 & RUSB2_INTSTS0_DVSQ_Msk) {
      case RUSB2_INTSTS0_DVSQ_STATE_DEF:
        process_bus_reset(rhport);
        break;

      case RUSB2_INTSTS0_DVSQ_STATE_ADDR:
        process_set_address(rhport);
        break;

      case RUSB2_INTSTS0_DVSQ_STATE_SUSP0:
      case RUSB2_INTSTS0_DVSQ_STATE_SUSP1:
      case RUSB2_INTSTS0_DVSQ_STATE_SUSP2:
      case RUSB2_INTSTS0_DVSQ_STATE_SUSP3:
        dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
        if (!_dcd.sof_enabled) {
          rusb->INTENB0_b.SOFE = 1;
        }

      default: break;
    }
  }

//  if ( is0 & RUSB2_INTSTS0_NRDY_Msk ) {
//    rusb->NRDYSTS = 0;
//  }

  // Control transfer stage changes
  if ( is0 & RUSB2_INTSTS0_CTRT_Msk ) {
    if ( is0 & RUSB2_INTSTS0_CTSQ_CTRL_RDATA ) {
      /* A setup packet has been received. */
      process_setup_packet(rhport);
    } else if ( 0 == (is0 & RUSB2_INTSTS0_CTSQ_Msk) ) {
      /* A ZLP has been sent/received. */
      process_status_completion(rhport);
    }
  }

  // Buffer empty
  if ( is0 & RUSB2_INTSTS0_BEMP_Msk ) {
    const uint16_t s = rusb->BEMPSTS;
    rusb->BEMPSTS = 0;
    if ( s & 1 ) {
      process_pipe0_bemp(rhport);
    }
  }

  // Buffer ready
  if ( is0 & RUSB2_INTSTS0_BRDY_Msk ) {
    const unsigned m = rusb->BRDYENB;
    unsigned s = rusb->BRDYSTS & m;
    /* clear active bits (don't write 0 to already cleared bits according to the HW manual) */
    rusb->BRDYSTS = ~s;
    while (s) {
      const unsigned num = __builtin_ctz(s);
      process_pipe_brdy(rhport, num);
      s &= ~TU_BIT(num);
    }
  }
}
#endif
