/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 Greg Davill
 * SPDX-FileCopyrightText: Copyright (c) 2023 Denis Krasutski
 * SPDX-FileCopyrightText: Copyright (c) 2022 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */
#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USBHS_V1) && defined(CFG_TUD_WCH_USBIP_USBHS) && \
  (CFG_TUD_WCH_USBIP_USBHS == 1)
  #include "wch_usbhs_v1_reg.h"

  #include "device/dcd.h"

  // Max number of bi-directional endpoints including EP0
  #define EP_MAX 16

typedef struct {
  uint8_t *buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_size;
  bool     is_iso;
  bool     valid;
} xfer_ctl_t;

  #define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]
static xfer_ctl_t xfer_status[EP_MAX][2];

  #define EP_TX_LEN(ep)      *(volatile uint16_t *)((volatile uint16_t *)&(USBHSD->UEP0_TX_LEN) + (ep) * 2)
  #define EP_TX_CTRL(ep)     *(volatile uint8_t *)((volatile uint8_t *)&(USBHSD->UEP0_TX_CTRL) + (ep) * 4)
  #define EP_RX_CTRL(ep)     *(volatile uint8_t *)((volatile uint8_t *)&(USBHSD->UEP0_RX_CTRL) + (ep) * 4)
  #define EP_RX_MAX_LEN(ep)  *(volatile uint16_t *)((volatile uint16_t *)&(USBHSD->UEP0_MAX_LEN) + (ep) * 2)

  #define EP_TX_DMA_ADDR(ep) *(volatile uint32_t *)((volatile uint32_t *)&(USBHSD->UEP1_TX_DMA) + (ep - 1))
  #define EP_RX_DMA_ADDR(ep) *(volatile uint32_t *)((volatile uint32_t *)&(USBHSD->UEP1_RX_DMA) + (ep - 1))

/* Endpoint Buffer */
TU_ATTR_ALIGNED(4) static uint8_t ep0_buffer[CFG_TUD_ENDPOINT0_SIZE];
static bool ep0_tog;
static bool reset_evt_pending;
static bool high_speed; // speed negotiated by the last bus reset, for the SOF frame count
// TX_CTRL toggle bits each bulk/interrupt IN packet in flight was armed with (one byte per endpoint:
// written from task and ISR context, so never a shared read-modify-write)
static uint8_t in_pkt_tog[EP_MAX];
static bool sof_requested; // by usbd; reset completion also needs the SOF interrupt

static void queue_in_packet(uint8_t ep_num, xfer_ctl_t* xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t tx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num == 0) {
    memcpy(ep0_buffer, &xfer->buffer[xfer->queued_len], tx_len);
  } else {
    EP_TX_DMA_ADDR(ep_num) = (uint32_t) &xfer->buffer[xfer->queued_len];
  }

  EP_TX_LEN(ep_num) = tx_len;
  xfer->queued_len += tx_len;

  if (ep_num == 0) {
    EP_TX_CTRL(0) = USBHS_EP_T_RES_ACK | (ep0_tog ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
    ep0_tog = !ep0_tog;
  } else if (xfer->is_iso) {
    EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(USBHS_EP_T_RES_MASK)) | USBHS_EP_T_RES_NYET;
  } else {
    uint8_t const tx_ctrl = EP_TX_CTRL(ep_num);
    in_pkt_tog[ep_num] = tx_ctrl & USBHS_EP_T_TOG_MASK;
    EP_TX_CTRL(ep_num) = (tx_ctrl & ~(USBHS_EP_T_RES_MASK)) | USBHS_EP_T_RES_ACK;
  }
}

static void queue_out_packet(uint8_t ep_num, xfer_ctl_t* xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t rx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num > 0) {
    EP_RX_DMA_ADDR(ep_num) = (uint32_t) &xfer->buffer[xfer->queued_len];
    EP_RX_MAX_LEN(ep_num) = rx_len;
  }

  if (ep_num == 0) {
    EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(USBHS_EP_R_RES_MASK)) | USBHS_EP_R_RES_ACK;
  } else if (xfer->is_iso) {
    EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(USBHS_EP_R_RES_MASK)) | USBHS_EP_R_RES_NYET;
  } else {
    EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(USBHS_EP_R_RES_MASK)) | USBHS_EP_R_RES_ACK;
  }
}

// The completion of a bulk/interrupt IN packet means the host ACKed it, so its data toggle must have
// advanced. USBHS drops that update when the IN ACK is closely followed by a PING/OUT on the same
// endpoint number: the next packet would go out with the old toggle and the host would discard it
// as a retransmission. Advance the toggle here when the controller did not.
TU_ATTR_ALWAYS_INLINE static inline void in_tog_advance(uint8_t ep_num) {
  uint8_t const tx_ctrl = EP_TX_CTRL(ep_num);
  if ((tx_ctrl & USBHS_EP_T_TOG_MASK) == in_pkt_tog[ep_num]) {
    EP_TX_CTRL(ep_num) = (tx_ctrl & ~(USBHS_EP_T_TOG_MASK)) | (in_pkt_tog[ep_num] ^ USBHS_EP_T_TOG_1);
  }
}

// Start an IN transfer's first packet (force), or continue or complete EP0's. Other endpoints' IN completions
// take int_handler_xfer().
static void update_in(uint8_t rhport, uint8_t ep_num, bool force) {
  xfer_ctl_t* xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
  if (!xfer->valid) {
    return;
  }

  if (force || (xfer->total_len > xfer->queued_len)) {
    queue_in_packet(ep_num, xfer);
  } else {
    xfer->valid = false;
    EP_TX_CTRL(0) = USBHS_EP_T_RES_NAK | (ep0_tog ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
    dcd_event_xfer_complete(rhport, TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }
}

static void update_out(uint8_t rhport, uint8_t ep_num, uint16_t rx_len) {
  xfer_ctl_t* xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
  if (!xfer->valid) {
    return;
  }

  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t len = TU_MIN(rx_len, TU_MIN(remaining, xfer->max_size));

  if (ep_num == 0) {
    memcpy(&xfer->buffer[xfer->queued_len], ep0_buffer, len);
  }

  xfer->queued_len += len;

  if ((xfer->queued_len == xfer->total_len) || (len < xfer->max_size)) {
    xfer->valid = false;
    if (ep_num == 0) {
      EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(USBHS_EP_R_RES_MASK)) | USBHS_EP_R_RES_NAK;
    }
    dcd_event_xfer_complete(rhport, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }

  if (ep_num != 0) {
    if (xfer->valid) {
      queue_out_packet(ep_num, xfer);
    } else {
      uint8_t rx_res = xfer->is_iso ? USBHS_EP_R_RES_NYET : USBHS_EP_R_RES_NAK;
      EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(USBHS_EP_R_RES_MASK)) | rx_res;
    }
  }
}

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;

  memset(&xfer_status, 0, sizeof(xfer_status));
  ep0_tog = true;
  reset_evt_pending = false;
  sof_requested = false;
  high_speed = false;

  USBHSD->HOST_CTRL = 0x00;
  USBHSD->HOST_CTRL = USBHS_PHY_SUSPENDM;

  USBHSD->CONTROL = 0;

  if (rh_init->speed == TUSB_SPEED_HIGH || rh_init->speed == TUSB_SPEED_AUTO) {
    USBHSD->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_HIGH_SPEED;
  } else if (rh_init->speed == TUSB_SPEED_FULL) {
    USBHSD->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_FULL_SPEED;
  } else if (rh_init->speed == TUSB_SPEED_LOW) {
    USBHSD->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_LOW_SPEED;
  } else {
    return false;
  }

  USBHSD->INT_EN = 0;
  USBHSD->INT_FG = 0xff; // a host session on this controller may have left flags
  USBHSD->INT_EN = USBHS_SETUP_ACT_EN | USBHS_TRANSFER_EN | USBHS_BUS_RST_EN | USBHS_SUSPEND_EN;

  USBHSD->ENDP_CONFIG = USBHS_EP0_T_EN | USBHS_EP0_R_EN;
  USBHSD->ENDP_TYPE   = 0x00;
  USBHSD->BUF_MODE    = 0x00;

  for (int ep = 0; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep)  = 0;
    EP_TX_CTRL(ep) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
    EP_RX_CTRL(ep) = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0;

    EP_RX_MAX_LEN(ep) = 0;
  }

  USBHSD->UEP0_DMA                      = (uint32_t)ep0_buffer;
  USBHSD->UEP0_MAX_LEN                  = CFG_TUD_ENDPOINT0_SIZE;
  xfer_status[0][TUSB_DIR_OUT].max_size = CFG_TUD_ENDPOINT0_SIZE;
  xfer_status[0][TUSB_DIR_IN].max_size  = CFG_TUD_ENDPOINT0_SIZE;

  USBHSD->DEV_AD = 0;
  dcd_connect(rhport);

  return true;
}

// tud_deinit() has disabled the interrupt and disconnected: hold the controller in reset, as hcd_deinit()
// does, until dcd_init() or hcd_init() configures it again (a run-time switch to the host role)
bool dcd_deinit(uint8_t rhport) {
  (void)rhport;
  USBHSD->INT_EN  = 0;
  USBHSD->CONTROL = USBHS_FORCE_RST | USBHS_ALL_CLR;
  USBHSD->INT_FG  = 0xff;
  return true;
}

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USBHS_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USBHS_IRQn);
}

void dcd_edpt_close_all(uint8_t rhport) {
  (void)rhport;

  for (size_t ep = 1; ep < EP_MAX; ep++) {
    // Retire pending transfers first, so neither the ISR nor a later clear-stall re-arms one
    xfer_status[ep][TUSB_DIR_OUT].valid = false;
    xfer_status[ep][TUSB_DIR_IN].valid  = false;
    EP_TX_LEN(ep)  = 0;
    EP_TX_CTRL(ep) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
    EP_RX_CTRL(ep) = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0;

    EP_RX_MAX_LEN(ep) = 0;
  }

  USBHSD->ENDP_CONFIG = USBHS_EP0_T_EN | USBHS_EP0_R_EN;
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;

  // Response with zlp status
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false);
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void)rhport;
}

void dcd_connect(uint8_t rhport) {
  (void)rhport;
  USBHSD->HOST_CTRL |= USBHS_PHY_SUSPENDM;
  USBHSD->CONTROL |= USBHS_DEV_PU_EN;
}

void dcd_disconnect(uint8_t rhport) {
  (void)rhport;
  USBHSD->CONTROL &= ~USBHS_DEV_PU_EN;
  USBHSD->HOST_CTRL &= ~USBHS_PHY_SUSPENDM;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  dcd_int_disable(rhport);
  sof_requested = en;
  if (en || reset_evt_pending) {
    USBHSD->INT_EN |= USBHS_SOF_ACT_EN;
  } else {
    USBHSD->INT_EN &= ~(USBHS_SOF_ACT_EN);
  }
  dcd_int_enable(rhport);
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;
  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD && request->bRequest == TUSB_REQ_SET_ADDRESS) {
    USBHSD->DEV_AD = (uint8_t)request->wValue;
  }
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(desc_edpt->bEndpointAddress);
  const tusb_dir_t dir    = tu_edpt_dir(desc_edpt->bEndpointAddress);

  TU_ASSERT(ep_num < EP_MAX);

  if (ep_num == 0) {
    return true;
  }

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->valid      = false;
  xfer->max_size   = tu_edpt_packet_size(desc_edpt);

  xfer->is_iso = (desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  if (dir == TUSB_DIR_OUT) {
    USBHSD->ENDP_CONFIG |= (USBHS_EP0_R_EN << ep_num);
    EP_RX_CTRL(ep_num) = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0 |
                         (xfer->is_iso ? 0 : USBHS_EP_R_AUTOTOG);
    if (xfer->is_iso == true) {
      USBHSD->ENDP_TYPE |= (USBHS_EP0_R_TYP << ep_num);
    }
    EP_RX_MAX_LEN(ep_num) = xfer->max_size;
  } else {
    if (xfer->is_iso == true) {
      USBHSD->ENDP_TYPE |= (USBHS_EP0_T_TYP << ep_num);
    }
    USBHSD->ENDP_CONFIG |= (USBHS_EP0_T_EN << ep_num);
    EP_TX_LEN(ep_num)  = 0;
    EP_TX_CTRL(ep_num) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0 |
                         (xfer->is_iso ? 0 : USBHS_EP_T_AUTOTOG);
  }

  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  xfer_status[ep_num][dir].valid = false;
  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num)    = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0;
    EP_RX_MAX_LEN(ep_num) = 0;
    USBHSD->ENDP_TYPE &= ~(USBHS_EP0_R_TYP << ep_num);
    USBHSD->ENDP_CONFIG &= ~(USBHS_EP0_R_EN << ep_num);
  } else { // TUSB_DIR_IN
    EP_TX_CTRL(ep_num) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
    EP_TX_LEN(ep_num)  = 0;
    USBHSD->ENDP_TYPE &= ~(USBHS_EP0_T_TYP << ep_num);
    USBHSD->ENDP_CONFIG &= ~(USBHS_EP0_T_EN << ep_num);
  }
}

  #if 0
bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  (void) rhport;
  (void) ep_addr;
  (void) largest_packet_size;
  return false;
}

bool dcd_edpt_iso_activate(uint8_t rhport, tusb_desc_endpoint_t const * desc_ep) {
  (void) rhport;
  (void) desc_ep;
  return false;
}
  #endif

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  // Stalling drops the queued transfer: invalidate it so neither the ISR nor a clear-stall acts on it
  dcd_int_disable(rhport);
  xfer_status[ep_num][dir].valid = false;
  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = USBHS_EP_R_RES_STALL;
  } else {
    EP_TX_LEN(ep_num) = 0;
    EP_TX_CTRL(ep_num) = USBHS_EP_T_RES_STALL;
  }
  dcd_int_enable(rhport);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);
  xfer_ctl_t      *xfer   = XFER_CTL_BASE(ep_num, dir);
  const bool       autotog = (ep_num != 0 && !xfer->is_iso);

  // A transfer still valid here was not stalled (a stall retires it): the class driver considers it submitted
  // and won't submit it again, so re-arm it with the reset toggle instead of leaving the endpoint NAKing until
  // the next configuration. usbtest's toggle test 29 clears the halt on an armed bulk OUT pipe. Masked so a
  // pending completion cannot retire the transfer between the check and the re-arm.
  dcd_int_disable(rhport);
  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0 | (autotog ? USBHS_EP_R_AUTOTOG : 0);
    if (xfer->valid) {
      queue_out_packet(ep_num, xfer);
    }
  } else {
    uint8_t tx_ctrl = USBHS_EP_T_TOG_0 | (autotog ? USBHS_EP_T_AUTOTOG : 0);
    if (xfer->valid && autotog) {
      // The armed packet (TX_LEN and DMA address are kept) goes again as DATA0
      in_pkt_tog[ep_num] = USBHS_EP_T_TOG_0;
      tx_ctrl |= USBHS_EP_T_RES_ACK;
    } else {
      tx_ctrl |= USBHS_EP_T_RES_NAK;
    }
    EP_TX_CTRL(ep_num) = tx_ctrl;
  }
  dcd_int_enable(rhport);
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  (void)rhport;
  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  xfer_ctl_t *xfer     = XFER_CTL_BASE(ep_num, dir);
  xfer->buffer         = buffer;
  xfer->total_len      = total_bytes;
  xfer->queued_len     = 0;
  xfer->valid         = true;

  if (ep_num == 0 && dir == TUSB_DIR_OUT) {
    if (total_bytes == 0) {
      EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(USBHS_EP_R_TOG_MASK)) | USBHS_EP_R_TOG_1;
    } else {
      EP_RX_CTRL(0) ^= USBHS_EP_R_TOG_1;
    }
  }

  if (dir == TUSB_DIR_IN) {
    update_in(rhport, ep_num, true);
  } else {
    queue_out_packet(ep_num, xfer);
  }

  return true;
}

//--------------------------------------------------------------------+
// Interrupt handler
//--------------------------------------------------------------------+
// Why assembly: INT_FG, INT_ST and RX_LEN describe the latest transaction, not the one that raised the
// interrupt, and the controller keeps going while the interrupt waits:
// - while the transfer flag is set, INT_BUSY NAKs every endpoint, so the next endpoint's transaction waits
//   on this handler (a host polls consecutive interrupt endpoints ~2 us apart at high speed);
// - an isochronous transaction ignores INT_BUSY and overwrites INT_ST/RX_LEN of a pending completion;
// - a NAKed follow-up transaction still rewrites INT_ST: the next OUT of a control transfer, ~1 us after
//   the ACK, clears TOG_OK, and the good packet before it would be dropped as a toggle mismatch. Entry can
//   still be that late (another interrupt, interrupts masked, SystemView recording), so ep0_out_toggle_ok()
//   also checks IS_NAK.
// A C function's prologue (and, traced, SystemView's entry record) runs before its first statement, too
// late for all three. dcd_int_handler() is therefore one assembly body that:
// 1. snapshots INT_FG and INT_ST in its first instructions, and RX_LEN for an OUT;
// 2. for a transfer on a non-EP0 endpoint (IN, or OUT with a matching toggle): NAKs the endpoint unless it
//    is isochronous or its transfer was retired (that response is final: NAK, or a STALL that must stay),
//    clears the transfer flag (ending INT_BUSY) and tail-calls int_handler_xfer();
// 3. for everything else (bus reset, SOF, EP0, OUT toggle mismatch, SETUP, suspend): tail-calls
//    int_handler_other() with the snapshot.
// It uses only caller-saved registers and returns through the helper it tail-calls, so the same body is the
// DCD's C entry (tud_int_handler() calls it) and, through dcd_int_handler_fast(), the interrupt vector:
// - RISC-V (CH32V30x): dcd_int_handler_fast() points ra at an mret and jumps in. QingKe hardware
//   prologue/epilogue (HPE), which WCH's startup enables, has stacked the caller-saved registers.
// - Thumb-2 (CH32F20x, Cortex-M3): dcd_int_handler_fast() branches in with lr = EXC_RETURN, the core having
//   stacked r0-r3 and r12, so the helper's return ends the exception.
// SystemView builds take the same path; the helpers record the interrupt, after the snapshot (recording it
// first, as tud_int_handler() does, would delay the snapshot; a span nested in that one is recorded once).
// WCH parts build with GCC only, whose naked functions and asm operands this uses.
//
// Application integration, slowest first (each is correct; declare `void dcd_int_handler_fast(void);`):
// 1. An ordinary interrupt handler calls tud_int_handler(0). On RISC-V, GCC's interrupt attribute saves 16
//    registers before the snapshot; with SystemView, tud_int_handler() records the interrupt before it.
// 2. The USBHS vector is dcd_int_handler_fast(): name it in the vector table, or define USBHS_IRQHandler to
//    branch to it (Cortex-M: a C handler that calls it; RISC-V: a naked one-instruction `j`, since a C
//    function would return with ret). RISC-V needs HPE enabled (INTSYSCR bit 0).
// 3. RISC-V: the PFIC fast vector (VTF) also skips the vector table lookup:
//    SetVTFIRQ((uint32_t) dcd_int_handler_fast, USBHS_IRQn, slot, ENABLE).
// On NanoCH32V305 with three HID streams and UAC2 (docs/reference/device_issues.rst), 1 kept 19,000 of 24,000
// HID reports/s and broke the speaker stream hundreds of times per 6 s, 2 got every report with ~150 breaks,
// 3 about 5.

// Whether to take the EP0 OUT packet that raised the interrupt. IS_NAK set means the host's next OUT came
// while the transfer flag was still set: INT_BUSY NAKed it, and INT_ST now describes that NAKed packet
// (RX_LEN still holds the ACKed one). Its TOG_OK is set once its data PID arrives, and is then the opposite
// of the ACKed packet's, since EP0's expected toggle only advances when software re-arms it (the controller
// does not auto-toggle EP0). So TOG_OK means nothing then; the ACKed packet is taken, as the host only sends
// on after it (the next packet if it saw the ACK, the same one again if not). That is wrong only for an
// ACKed retransmission followed by a NAKed packet before this runs, i.e. a lost handshake and a late
// interrupt together.
TU_ATTR_ALWAYS_INLINE static inline bool ep0_out_toggle_ok(uint8_t int_status) {
  return (int_status & (USBHS_DEV_UIS_TOG_OK | USBHS_DEV_UIS_IS_NAK)) != 0;
}

// Step 2: a non-EP0 transfer the handler has released
static void int_handler_xfer(uint8_t ep_num, uint16_t len, uint8_t token) {
  TU_SYSVIEW_ISR_ENTER();
  if (token == USBHS_TOKEN_PID_IN) {
    xfer_ctl_t* xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
    if (xfer->valid) {
      if (!xfer->is_iso) {
        in_tog_advance(ep_num);
      }
      if (xfer->total_len > xfer->queued_len) {
        queue_in_packet(ep_num, xfer);
      } else {
        xfer->valid = false;
        // Non-ISO endpoints were already NAKed before the transfer flag was released.
        if (xfer->is_iso) {
          EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~USBHS_EP_T_RES_MASK) | USBHS_EP_T_RES_NAK;
        }
        dcd_event_xfer_complete(0, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
      }
    }
  } else {
    update_out(0, ep_num, len);
  }
  TU_SYSVIEW_ISR_EXIT();
}

// Step 3: bus reset, SOF, EP0, OUT toggle mismatches, SETUP and suspend, on the handler's register snapshot
static void int_handler_other(uint8_t rhport, uint8_t int_flag, uint8_t int_status, uint16_t len) {
  (void)rhport;
  TU_SYSVIEW_ISR_ENTER();

  if (int_flag & USBHS_BUS_RST_FLAG) {
    // Reset invalidates pending completions, so handle it before TRANSFER or SETUP.
    // The first SOF follows speed negotiation and completes the reset notification.
    // usbd_reset() clears its SOF consumers without calling dcd_sof_enable(), so drop the stale request too.
    sof_requested = false;
    reset_evt_pending = true;
    USBHSD->INT_EN |= USBHS_SOF_ACT_EN;
    dcd_event_bus_signal(rhport, DCD_EVENT_BUS_RESET_START, true);

    USBHSD->DEV_AD = 0;
    ep0_tog = true;
    EP_RX_CTRL(0) = USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_0;
    EP_TX_CTRL(0) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
    // The flags do not identify which side of reset a pending transaction belongs to.
    // Clear only the reset/transaction flags observed in the entry snapshot.
    USBHSD->INT_FG = int_flag & (USBHS_BUS_RST_FLAG | USBHS_TRANSFER_FLAG | USBHS_SETUP_FLAG);
  } else if (int_flag & USBHS_TRANSFER_FLAG) {
    const uint8_t token = int_status & MASK_UIS_TOKEN;
    const uint8_t ep_num = int_status & MASK_UIS_ENDP;

    if (token == USBHS_TOKEN_PID_SOF) {
      // An SOF has nothing to read: release the transfer flag (and INT_BUSY) before the event work, so the
      // microframe's first transactions are not NAKed while it runs
      USBHSD->INT_FG = USBHS_TRANSFER_FLAG;
      if (reset_evt_pending) {
        reset_evt_pending = false;
        tusb_speed_t actual_speed;
        switch (USBHSD->SPEED_TYPE & USBHS_SPEED_TYPE_MASK) {
          case USBHS_SPEED_TYPE_FULL:
            actual_speed = TUSB_SPEED_FULL;
            break;
          case USBHS_SPEED_TYPE_LOW:
            actual_speed = TUSB_SPEED_LOW;
            break;
          default:
            actual_speed = TUSB_SPEED_HIGH;
            break;
        }
        high_speed = (actual_speed == TUSB_SPEED_HIGH);
        dcd_event_bus_reset(rhport, actual_speed, true);
        if (!sof_requested) {
          USBHSD->INT_EN &= ~USBHS_SOF_ACT_EN;
        }
      } else {
        // At high speed every microframe SOF interrupts, and FRAME_NO holds the frame number (bits 10:0)
        // and the microframe (bits 15:13): report the 14-bit microframe count, as DWC2 does
        uint16_t const frame_no = USBHSD->FRAME_NO;
        uint32_t frame_count = frame_no & USBHS_FRAME_NO_NUM_MASK;
        if (high_speed) {
          frame_count = (frame_count << 3) |
                        ((frame_no & USBHS_FRAME_NO_MICROFRAME_MASK) >> USBHS_FRAME_NO_MICROFRAME_SHIFT);
        }
        dcd_event_sof(rhport, frame_count, true);
      }
    } else {
      if (ep_num == 0 && token == USBHS_TOKEN_PID_OUT && ep0_out_toggle_ok(int_status)) {
        update_out(rhport, 0, len);
      } else if (ep_num == 0 && token == USBHS_TOKEN_PID_IN) {
        update_in(rhport, 0, false);
      }
      // Any other transfer reaching here is an OUT whose data toggle doesn't match what we expect -- a host
      // retransmit after a lost ACK, or a host that doesn't alternate DATA0/DATA1: drop it.
      USBHSD->INT_FG = (int_flag & USBHS_TRANSFER_FLAG); /* Clear flag */
    }
  } else if (int_flag & USBHS_SETUP_FLAG) {
    tusb_control_request_t const* setup =
        (tusb_control_request_t const*) ep0_buffer;
    ep0_tog = true;
    EP_RX_CTRL(0) = (setup->wLength == 0) ? USBHS_EP_R_RES_ACK : USBHS_EP_R_RES_NAK;
    EP_TX_CTRL(0) = USBHS_EP_T_RES_NAK;

    dcd_event_setup_received(0, ep0_buffer, true);

    USBHSD->INT_FG = USBHS_SETUP_FLAG; /* Clear flag */
  } else if (int_flag & USBHS_SUSPEND_FLAG) {
    // The SUSPEND interrupt fires on both suspend and resume; MIS_ST's suspend bit tells them apart
    // (set while suspended, clear once resumed). Reporting both as suspend leaves usbd suspended for
    // good: SOF interrupts are off after reset, so nothing else clears it and tud_ready() stays false.
    dcd_event_bus_signal(rhport, (USBHSD->MIS_ST & USBHS_SUSPEND) ? DCD_EVENT_SUSPEND : DCD_EVENT_RESUME, true);

    USBHSD->INT_FG = USBHS_SUSPEND_FLAG; /* Clear flag */
  } else {
    // Unhandled interrupt
    USBHSD->INT_FG = int_flag; /* Clear all flags */
  }
  TU_SYSVIEW_ISR_EXIT();
}

// The assembly takes every register offset, mask and struct offset as an operand; these are the
// assumptions an operand cannot carry.
  #define USBHS_FAST_OFF(_member) offsetof(USBHSD_TypeDef, _member)
TU_VERIFY_STATIC(sizeof(bool) == 1 && offsetof(xfer_ctl_t, valid) == offsetof(xfer_ctl_t, is_iso) + 1 &&
                 (offsetof(xfer_ctl_t, is_iso) % 2) == 0 && (sizeof(xfer_ctl_t) % 2) == 0,
                 "is_iso and valid are loaded as one little-endian halfword");
TU_VERIFY_STATIC(USBHS_EP_T_RES_MASK == USBHS_EP_R_RES_MASK && USBHS_EP_T_RES_NAK == USBHS_EP_R_RES_NAK,
                 "one NAK sequence serves both directions");

// The halfword {is_iso, valid} of an endpoint the handler NAKs: not isochronous, transfer still valid
  #define USBHS_FAST_NAK_STATE 0x100

// Constant ("i") operands only, as is usual in naked functions (no register or stack operands)
  #define USBHS_FAST_OPERANDS                                                                                 \
    [base] "i"(USBHS_BASE), [o_fg] "i"(USBHS_FAST_OFF(INT_FG)), [o_st] "i"(USBHS_FAST_OFF(INT_ST)),           \
    [o_rxlen] "i"(USBHS_FAST_OFF(RX_LEN)), [o_tx_ctrl] "i"(USBHS_FAST_OFF(UEP0_TX_CTRL)),                       \
    [o_rx_ctrl] "i"(USBHS_FAST_OFF(UEP0_RX_CTRL)), [m_rst_xfer] "i"(USBHS_BUS_RST_FLAG | USBHS_TRANSFER_FLAG), \
    [f_xfer] "i"(USBHS_TRANSFER_FLAG), [m_ep] "i"(MASK_UIS_ENDP), [m_tok] "i"(MASK_UIS_TOKEN),                 \
    [tok_in] "i"(USBHS_TOKEN_PID_IN), [tok_out] "i"(USBHS_TOKEN_PID_OUT), [tog_ok] "i"(USBHS_DEV_UIS_TOG_OK),  \
    [xfer] "i"(xfer_status), [xfer_row] "i"(sizeof(xfer_status[0])),                                         \
    [o_iso_out] "i"(TUSB_DIR_OUT * sizeof(xfer_ctl_t) + offsetof(xfer_ctl_t, is_iso)),                       \
    [o_iso_in] "i"(TUSB_DIR_IN * sizeof(xfer_ctl_t) + offsetof(xfer_ctl_t, is_iso)),                         \
    [m_res_keep] "i"((uint8_t) ~USBHS_EP_T_RES_MASK), [m_res] "i"(USBHS_EP_T_RES_MASK),                     \
    [res_nak] "i"(USBHS_EP_T_RES_NAK), [nak_state] "i"(USBHS_FAST_NAK_STATE), [h_xfer] "i"(int_handler_xfer), \
    [h_other] "i"(int_handler_other)

// The handler (see "Interrupt handler" above). rhport is ignored: the DCD drives one USBHS controller.
__attribute__((naked, used)) void dcd_int_handler(__attribute__((unused)) uint8_t rhport) {
  #if defined(__riscv)
  // Registers: t0 base, a1 INT_FG, a2 INT_ST, a3 len, a4 ep_num, t1 token, t2/t3 scratch. `tail` (a jump
  // through t1) leaves ra alone: the helper returns to the caller, or from the vector into its mret.
  __asm volatile(
    "li   t0, %[base]\n\t"
    "lbu  a1, %[o_fg](t0)\n\t"           // snapshot INT_FG
    "lbu  a2, %[o_st](t0)\n\t"           // snapshot INT_ST
    "andi t1, a1, %[m_rst_xfer]\n\t"
    "li   t2, %[f_xfer]\n\t"
    "bne  t1, t2, 9f\n\t"                // no transfer, or a bus reset pending: C path
    "andi a4, a2, %[m_ep]\n\t"
    "beqz a4, 9f\n\t"                    // EP0: C path
    "andi t1, a2, %[m_tok]\n\t"
    "li   t2, %[tok_in]\n\t"
    "beq  t1, t2, 1f\n\t"
    "bnez t1, 9f\n\t"                    // SOF or SETUP token: C path (OUT is 0)
    "andi t2, a2, %[tog_ok]\n\t"
    "beqz t2, 9f\n\t"                    // OUT toggle mismatch: C path drops it
    "lhu  a3, %[o_rxlen](t0)\n\t"        // OUT: snapshot RX_LEN
    "li   t2, %[xfer_row]\n\t"           // t2 = xfer_status[ep][OUT] {is_iso, valid}
    "mul  t2, a4, t2\n\t"
    "la   t3, %[xfer]\n\t"
    "add  t2, t2, t3\n\t"
    "lhu  t2, %[o_iso_out](t2)\n\t"
    "addi t3, t0, %[o_rx_ctrl]\n\t"      // t3 = &UEP0_RX_CTRL
    "j    2f\n\t"
    "1:\n\t"                             // IN
    "li   a3, 0\n\t"
    "li   t2, %[xfer_row]\n\t"           // t2 = xfer_status[ep][IN] {is_iso, valid}
    "mul  t2, a4, t2\n\t"
    "la   t3, %[xfer]\n\t"
    "add  t2, t2, t3\n\t"
    "lhu  t2, %[o_iso_in](t2)\n\t"
    "addi t3, t0, %[o_tx_ctrl]\n\t"      // t3 = &UEP0_TX_CTRL
    "2:\n\t"
    "addi t2, t2, -%[nak_state]\n\t"
    "bnez t2, 3f\n\t"                    // non-ISO, valid: NAK the endpoint before releasing INT_BUSY
    "slli t2, a4, 2\n\t"                 // UEPn_*_CTRL stride is 4 bytes
    "add  t3, t3, t2\n\t"
    "lbu  t2, 0(t3)\n\t"
    "andi t2, t2, %[m_res_keep]\n\t"
    "ori  t2, t2, %[res_nak]\n\t"
    "sb   t2, 0(t3)\n\t"
    "3:\n\t"
    "li   t2, %[f_xfer]\n\t"
    "sb   t2, %[o_fg](t0)\n\t"           // clear the transfer flag (W1C)
    "mv   a0, a4\n\t"
    "mv   a1, a3\n\t"
    "mv   a2, t1\n\t"
    "tail %[h_xfer]\n\t"                 // step 2: int_handler_xfer(ep_num, len, token)
    "9:\n\t"                             // step 3, with RX_LEN read only for a transfer
    "li   a3, 0\n\t"
    "andi t1, a1, %[f_xfer]\n\t"
    "beqz t1, 8f\n\t"
    "lhu  a3, %[o_rxlen](t0)\n\t"
    "8:\n\t"
    "li   a0, 0\n\t"
    "tail %[h_other]\n\t"                // step 3: int_handler_other(0, INT_FG, INT_ST, len)
    :
    : USBHS_FAST_OPERANDS);
  #else
  // Thumb-2. Registers: r12 base, r1 INT_FG, r2 INT_ST, r0 ep_num, r1 len, r3 scratch. Every exit is a
  // tail branch that leaves lr alone: the helper returns to the caller, or from the vector with EXC_RETURN.
  __asm volatile(
    "movw r12, #:lower16:%c[base]\n\t"
    "movt r12, #:upper16:%c[base]\n\t"
    "ldrb r1, [r12, %[o_fg]]\n\t"        // snapshot INT_FG
    "ldrb r2, [r12, %[o_st]]\n\t"        // snapshot INT_ST
    "and  r3, r1, %[m_rst_xfer]\n\t"
    "cmp  r3, %[f_xfer]\n\t"
    "bne  9f\n\t"                        // no transfer, or a bus reset pending: C path
    "ands r0, r2, %[m_ep]\n\t"
    "beq  9f\n\t"                        // EP0: C path
    "and  r3, r2, %[m_tok]\n\t"
    "cmp  r3, %[tok_in]\n\t"
    "beq  1f\n\t"
    "cmp  r3, %[tok_out]\n\t"
    "bne  9f\n\t"                        // SOF or SETUP token: C path
    "tst  r2, %[tog_ok]\n\t"
    "beq  9f\n\t"                        // OUT toggle mismatch: C path drops it
    "ldrh r1, [r12, %[o_rxlen]]\n\t"     // OUT: snapshot RX_LEN
    "movw r3, #:lower16:%c[xfer]\n\t"    // r3 = xfer_status[ep][OUT] {is_iso, valid}
    "movt r3, #:upper16:%c[xfer]\n\t"
    "mov  r2, %[xfer_row]\n\t"
    "mla  r3, r0, r2, r3\n\t"
    "ldrh r3, [r3, %[o_iso_out]]\n\t"
    "cmp  r3, %[nak_state]\n\t"
    "bne  3f\n\t"                        // non-ISO, valid: NAK the endpoint before releasing INT_BUSY
    "add  r2, r12, %[o_rx_ctrl]\n\t"
    "add  r2, r2, r0, lsl #2\n\t"        // UEPn_*_CTRL stride is 4 bytes
    "ldrb r3, [r2]\n\t"
    "bic  r3, r3, %[m_res]\n\t"
    "orr  r3, r3, %[res_nak]\n\t"
    "strb r3, [r2]\n\t"
    "3:\n\t"
    "mov  r3, %[f_xfer]\n\t"
    "strb r3, [r12, %[o_fg]]\n\t"        // clear the transfer flag (W1C)
    "movs r2, %[tok_out]\n\t"
    "b    %c[h_xfer]\n\t"                // step 2: int_handler_xfer(ep_num, len, token)
    "1:\n\t"                             // IN
    "movs r1, #0\n\t"
    "movw r3, #:lower16:%c[xfer]\n\t"    // r3 = xfer_status[ep][IN] {is_iso, valid}
    "movt r3, #:upper16:%c[xfer]\n\t"
    "mov  r2, %[xfer_row]\n\t"
    "mla  r3, r0, r2, r3\n\t"
    "ldrh r3, [r3, %[o_iso_in]]\n\t"
    "cmp  r3, %[nak_state]\n\t"
    "bne  4f\n\t"
    "add  r2, r12, %[o_tx_ctrl]\n\t"
    "add  r2, r2, r0, lsl #2\n\t"
    "ldrb r3, [r2]\n\t"
    "bic  r3, r3, %[m_res]\n\t"
    "orr  r3, r3, %[res_nak]\n\t"
    "strb r3, [r2]\n\t"
    "4:\n\t"
    "mov  r3, %[f_xfer]\n\t"
    "strb r3, [r12, %[o_fg]]\n\t"
    "movs r2, %[tok_in]\n\t"
    "b    %c[h_xfer]\n\t"
    "9:\n\t"                             // C path, with RX_LEN read only for a transfer
    "movs r3, #0\n\t"
    "tst  r1, %[f_xfer]\n\t"
    "it   ne\n\t"
    "ldrhne r3, [r12, %[o_rxlen]]\n\t"
    "movs r0, #0\n\t"
    "b    %c[h_other]\n\t"               // step 3: int_handler_other(0, INT_FG, INT_ST, len)
    :
    : USBHS_FAST_OPERANDS);
  #endif
}

// The interrupt vector (see "Application integration" above)
void dcd_int_handler_fast(void);
__attribute__((naked, used)) void dcd_int_handler_fast(void) {
  #if defined(__riscv)
  // The handler returns through ra: point ra at an mret and jump in
  __asm volatile(
    "la   ra, 1f\n\t"
    "j    %[handler]\n\t"
    "1:\n\t"
    "mret\n\t"
    :
    : [handler] "i"(dcd_int_handler));
  #else
  // lr holds EXC_RETURN: the handler's return ends the exception
  __asm volatile("b %c[handler]" : : [handler] "i"(dcd_int_handler));
  #endif
}

#endif
