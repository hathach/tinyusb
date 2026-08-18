/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// CH565/CH569 USB2.0 high-speed device controller (USBHS @0x40009000). The endpoint control
// bit encoding matches the CH32V307 USBHS (dcd_ch32_usbhs.c, which this driver is ported from),
// but the register block layout is CH56x-specific: R8_UEPn_MOD enable registers instead of
// ENDP_CONFIG/ENDP_TYPE, different DMA/MAX_LEN/control offsets, shared EP0 RX/TX DMA and 8
// endpoints instead of 16. USB DMA buffers must live in RAMX (see dcd_ch56x.h); transfers from
// buffers outside RAMX are bounced through per-endpoint slots.

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USBHS_CH56X) && \
    ((CFG_TUD_WCH_USBIP_USBHS == 1) || ((CFG_TUD_WCH_USBIP_USB30 == 1) && CFG_TUD_WCH_USB30_FALLBACK))

#include "device/dcd.h"
#include "dcd_ch56x.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <CH56xSFR.h>
#include <core_riscv.h>
#pragma GCC diagnostic pop

// Max number of bi-directional endpoints including EP0
#define EP_MAX          8
#define EP0_MAX_SIZE    TU_MIN(CFG_TUD_ENDPOINT0_SIZE, 64)

// Per-endpoint bounce slot size for transfer buffers that are not RAMX/16-byte aligned.
// Must hold one max packet: bulk/interrupt <= 512; larger (HS isochronous) endpoints
// require DMA-capable buffers (CFG_TUSB_MEM_SECTION in RAMX).
#define BOUNCE_BUF_SIZE 512

typedef struct {
  uint8_t *buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_size;
  bool     is_iso;
  bool     bounce;
  bool     valid;
} xfer_ctl_t;

#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]
static xfer_ctl_t xfer_status[EP_MAX][2];

#define EP_TX_LEN(ep)      (*((volatile uint16_t*)(USB_BASE_ADDR + 0x70 + 4 * (ep))))
#define EP_TX_CTRL(ep)     (*((volatile uint8_t*)(USB_BASE_ADDR + 0x72 + 4 * (ep))))
#define EP_RX_CTRL(ep)     (*((volatile uint8_t*)(USB_BASE_ADDR + 0x73 + 4 * (ep))))
#define EP_MAX_LEN(ep)     (*((volatile uint16_t*)(USB_BASE_ADDR + 0x50 + 4 * (ep))))
#define EP_RX_DMA_ADDR(ep) (*((volatile uint32_t*)(USB_BASE_ADDR + 0x18 + 4 * ((ep) - 1))))
#define EP_TX_DMA_ADDR(ep) (*((volatile uint32_t*)(USB_BASE_ADDR + 0x34 + 4 * ((ep) - 1))))

// R8_UEPn_MOD enable bits for EP1..EP7: mode register offset from USB_BASE_ADDR and whether the
// endpoint uses the high nibble (TX 0x40 / RX 0x80) or the low nibble (TX 0x04 / RX 0x08)
static const struct {
  uint8_t reg_ofs;
  uint8_t hi_nibble;
} ep_mod[EP_MAX] = {
  {0x00, 0}, // EP0: no mode register
  {0x10, 1}, // EP1 in R8_UEP4_1_MOD high nibble
  {0x11, 0}, // EP2 in R8_UEP2_3_MOD low nibble
  {0x11, 1}, // EP3 in R8_UEP2_3_MOD high nibble
  {0x10, 0}, // EP4 in R8_UEP4_1_MOD low nibble
  {0x12, 0}, // EP5 in R8_UEP5_6_MOD low nibble
  {0x12, 1}, // EP6 in R8_UEP5_6_MOD high nibble
  {0x13, 0}, // EP7 in R8_UEP7_MOD low nibble
};

static void ep_mode_set(uint8_t ep_num, tusb_dir_t dir, bool en) {
  volatile uint8_t* reg = (volatile uint8_t*)(USB_BASE_ADDR + ep_mod[ep_num].reg_ofs);
  uint8_t bit = (dir == TUSB_DIR_IN) ? (ep_mod[ep_num].hi_nibble ? 0x40 : 0x04)
                                     : (ep_mod[ep_num].hi_nibble ? 0x80 : 0x08);
  if (en) {
    *reg |= bit;
  } else {
    *reg &= (uint8_t)~bit;
  }
}

/* Usable endpoint numbers (0..CFG_TUD_WCH_USBHS_EP_MAX-1): the RAMX bounce buffers scale
 * with this (1 KB per data endpoint); applications using few endpoints can reduce it to
 * reclaim RAMX */
#ifndef CFG_TUD_WCH_USBHS_EP_MAX
  #define CFG_TUD_WCH_USBHS_EP_MAX EP_MAX
#endif
TU_VERIFY_STATIC(CFG_TUD_WCH_USBHS_EP_MAX >= 2 && CFG_TUD_WCH_USBHS_EP_MAX <= EP_MAX, "invalid CFG_TUD_WCH_USBHS_EP_MAX");

/* Endpoint buffers: must be in RAMX and 16-byte aligned (USB DMA constraint) */
CFG_TUD_WCH_DMA_SECTION TU_ATTR_ALIGNED(16) static uint8_t ep0_buffer[EP0_MAX_SIZE];
CFG_TUD_WCH_DMA_SECTION TU_ATTR_ALIGNED(16) static uint8_t bounce_buffer[CFG_TUD_WCH_USBHS_EP_MAX - 1][2][BOUNCE_BUF_SIZE];
// Written by both the ISR (SETUP, bus reset) and task context (arming a transfer)
static volatile bool ep0_tog;
static volatile bool ep_data_tog[EP_MAX][2];

static void set_ep_toggle(uint8_t ep_num, tusb_dir_t ep_dir, bool data1) {
  if (ep_dir == TUSB_DIR_IN) {
    EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(RB_UEP_T_TOG_MASK)) |
                         (data1 ? RB_UEP_T_TOG_1 : RB_UEP_T_TOG_0);
  } else {
    EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(RB_UEP_R_TOG_MASK)) |
                         (data1 ? RB_UEP_R_TOG_1 : RB_UEP_R_TOG_0);
  }
}

static void queue_in_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t tx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num == 0) {
    memcpy(ep0_buffer, &xfer->buffer[xfer->queued_len], tx_len);
  } else if (xfer->bounce) {
    memcpy(bounce_buffer[ep_num - 1][TUSB_DIR_IN], &xfer->buffer[xfer->queued_len], tx_len);
    EP_TX_DMA_ADDR(ep_num) = (uint32_t)(uintptr_t)bounce_buffer[ep_num - 1][TUSB_DIR_IN];
  } else {
    EP_TX_DMA_ADDR(ep_num) = (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
  }

  EP_TX_LEN(ep_num) = tx_len;
  xfer->queued_len += tx_len;

  if (ep_num == 0) {
    EP_TX_CTRL(0) = UEP_T_RES_ACK | (ep0_tog ? RB_UEP_T_TOG_1 : RB_UEP_T_TOG_0);
    ep0_tog = !ep0_tog;
  } else if (xfer->is_iso) {
    // isochronous: no handshake expected
    EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(RB_UEP_TRES_MASK)) | RB_UEP_TRES_NO | UEP_T_RES_ACK;
  } else {
    set_ep_toggle(ep_num, TUSB_DIR_IN, ep_data_tog[ep_num][TUSB_DIR_IN]);
    EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(RB_UEP_TRES_MASK)) | UEP_T_RES_ACK;
  }
}

static void queue_out_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t rx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num > 0) {
    if (xfer->bounce) {
      EP_RX_DMA_ADDR(ep_num) = (uint32_t)(uintptr_t)bounce_buffer[ep_num - 1][TUSB_DIR_OUT];
    } else {
      EP_RX_DMA_ADDR(ep_num) = (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
    }
    EP_MAX_LEN(ep_num) = rx_len;
  }

  if (ep_num == 0) {
    EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(RB_UEP_RRES_MASK)) | UEP_R_RES_ACK;
  } else if (xfer->is_iso) {
    EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(RB_UEP_RRES_MASK)) | RB_UEP_RRES_NO | UEP_R_RES_ACK;
  } else {
    set_ep_toggle(ep_num, TUSB_DIR_OUT, ep_data_tog[ep_num][TUSB_DIR_OUT]);
    EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(RB_UEP_RRES_MASK)) | UEP_R_RES_ACK;
  }
}

static void update_in(uint8_t rhport, uint8_t ep_num, bool force) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
  if (!xfer->valid) {
    return;
  }

  if (!force && ep_num != 0 && !xfer->is_iso) {
    ep_data_tog[ep_num][TUSB_DIR_IN] = !ep_data_tog[ep_num][TUSB_DIR_IN];
  }

  if (force || (xfer->total_len > xfer->queued_len)) {
    queue_in_packet(ep_num, xfer);
  } else {
    xfer->valid = false;
    if (ep_num == 0) {
      EP_TX_CTRL(0) = UEP_T_RES_NAK | (ep0_tog ? RB_UEP_T_TOG_1 : RB_UEP_T_TOG_0);
    } else {
      // clear RB_UEP_TRES_NO (set by an iso arm) together with the RES field: while it is set the
      // hardware ignores RES and keeps re-sending the last EP_TX_LEN bytes on every IN token
      EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(RB_UEP_TRES_NO | RB_UEP_TRES_MASK)) | UEP_T_RES_NAK;
    }
    dcd_event_xfer_complete(rhport, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }
}

static void update_out(uint8_t rhport, uint8_t ep_num, uint16_t rx_len) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
  if (!xfer->valid) {
    return;
  }

  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t len = TU_MIN(rx_len, TU_MIN(remaining, xfer->max_size));

  if (ep_num == 0) {
    memcpy(&xfer->buffer[xfer->queued_len], ep0_buffer, len);
  } else if (xfer->bounce) {
    memcpy(&xfer->buffer[xfer->queued_len], bounce_buffer[ep_num - 1][TUSB_DIR_OUT], len);
  }

  xfer->queued_len += len;

  if (ep_num != 0 && !xfer->is_iso) {
    ep_data_tog[ep_num][TUSB_DIR_OUT] = !ep_data_tog[ep_num][TUSB_DIR_OUT];
  }

  if ((xfer->queued_len == xfer->total_len) || (len < xfer->max_size)) {
    xfer->valid = false;
    if (ep_num == 0) {
      EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(RB_UEP_RRES_MASK)) | UEP_R_RES_NAK;
    }
    dcd_event_xfer_complete(rhport, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }

  if (ep_num != 0) {
    if (xfer->valid) {
      queue_out_packet(ep_num, xfer);
    } else {
      uint8_t rx_res = xfer->is_iso ? (RB_UEP_RRES_NO | UEP_R_RES_ACK) : UEP_R_RES_NAK;
      EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(RB_UEP_RRES_NO | RB_UEP_RRES_MASK)) | rx_res;
    }
  }
}

//--------------------------------------------------------------------+
// Controller internals, shared with the USB3.0 dcd for runtime fallback
//--------------------------------------------------------------------+

bool ch56x_usb2_init(uint8_t rhport) {
  (void)rhport;

  memset(&xfer_status, 0, sizeof(xfer_status));
  memset((void *)ep_data_tog, 0, sizeof(ep_data_tog));
  ep0_tog = true;

  R8_USB_CTRL = 0;
  R8_USB_CTRL = UCST_HS | RB_DEV_PU_EN | RB_USB_INT_BUSY | RB_USB_DMA_EN;
  R8_USB_INT_EN = RB_USB_IE_SETUPACT | RB_USB_IE_TRANS | RB_USB_IE_SUSPEND | RB_USB_IE_BUSRST;

  R8_UEP4_1_MOD = RB_UEP1_RX_EN | RB_UEP1_TX_EN | RB_UEP4_RX_EN | RB_UEP4_TX_EN;
  R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN | RB_UEP3_RX_EN | RB_UEP3_TX_EN;
  R8_UEP5_6_MOD = RB_UEP5_RX_EN | RB_UEP5_TX_EN | RB_UEP6_RX_EN | RB_UEP6_TX_EN;
  R8_UEP7_MOD   = RB_UEP7_RX_EN | RB_UEP7_TX_EN;

  R16_UEP0_MAX_LEN = EP0_MAX_SIZE;
  R32_UEP0_RT_DMA = (uint32_t)(uintptr_t)ep0_buffer;
  R16_UEP0_T_LEN = 0;
  R8_UEP0_TX_CTRL = 0;
  R8_UEP0_RX_CTRL = 0;

  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep)  = 0;
    EP_TX_CTRL(ep) = UEP_T_RES_NAK | RB_UEP_T_TOG_0;
    EP_RX_CTRL(ep) = UEP_R_RES_NAK | RB_UEP_R_TOG_0;
    EP_MAX_LEN(ep) = 0;
  }

  xfer_status[0][TUSB_DIR_OUT].max_size = EP0_MAX_SIZE;
  xfer_status[0][TUSB_DIR_IN].max_size  = EP0_MAX_SIZE;

  R8_USB_DEV_AD = 0;

  return true;
}

void ch56x_usb2_deinit(void) {
  R8_USB_INT_EN = 0;
  R8_USB_CTRL = RB_USB_CLR_ALL | RB_USB_RESET_SIE; // reset SIE, disconnect pull-up
}

void ch56x_usb2_connect(void) {
  R8_USB_CTRL |= RB_DEV_PU_EN; // attach: enable the device pull-up
}

void ch56x_usb2_disconnect(void) {
  R8_USB_CTRL &= (uint8_t)~RB_DEV_PU_EN; // detach: disable the device pull-up
}

void ch56x_usb2_remote_wakeup(void) {
  // Datasheet (R8_USB_SUSPEND): "pull the RB_DEV_WAKEUP bit up and then down" - the K state lasts as
  // long as the bit is set and USB 2.0 requires 1..15 ms of resume signalling (no vendor demo drives
  // this register; the busy wait mirrors dcd_ci_fs.c)
  R8_USB_SUSPEND |= RB_DEV_WAKEUP;
  tusb_time_delay_ms_api(2);
  R8_USB_SUSPEND &= (uint8_t)~RB_DEV_WAKEUP;
}

void ch56x_usb2_int_enable(void) {
  PFIC_EnableIRQ(USBHS_IRQn);
}

void ch56x_usb2_int_disable(void) {
  PFIC_DisableIRQ(USBHS_IRQn);
}

void ch56x_usb2_edpt_close_all(uint8_t rhport) {
  (void)rhport;

  memset((void *)ep_data_tog, 0, sizeof(ep_data_tog));

  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep)  = 0;
    EP_TX_CTRL(ep) = UEP_T_RES_NAK | RB_UEP_T_TOG_0;
    EP_RX_CTRL(ep) = UEP_R_RES_NAK | RB_UEP_R_TOG_0;
    EP_MAX_LEN(ep) = 0;
    // Drop any in-flight transfer so a later clear-halt cannot DMA into this stale buffer
    // (same reason as the single-endpoint close below)
    xfer_status[ep][TUSB_DIR_OUT].valid = false;
    xfer_status[ep][TUSB_DIR_IN].valid  = false;
  }

  R8_UEP4_1_MOD = 0;
  R8_UEP2_3_MOD = 0;
  R8_UEP5_6_MOD = 0;
  R8_UEP7_MOD   = 0;
}

void ch56x_usb2_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;
  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD && request->bRequest == TUSB_REQ_SET_ADDRESS) {
    R8_USB_DEV_AD = (uint8_t)request->wValue;
  }
}

bool ch56x_usb2_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(desc_edpt->bEndpointAddress);
  const tusb_dir_t dir    = tu_edpt_dir(desc_edpt->bEndpointAddress);

  TU_ASSERT(ep_num < CFG_TUD_WCH_USBHS_EP_MAX);

  if (ep_num == 0) {
    return true;
  }

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->max_size = tu_edpt_packet_size(desc_edpt);
  ep_data_tog[ep_num][dir] = false;

  xfer->is_iso = (desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  ep_mode_set(ep_num, dir, true);
  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = UEP_R_RES_NAK | RB_UEP_R_TOG_0;
    EP_MAX_LEN(ep_num) = xfer->max_size;
  } else {
    EP_TX_LEN(ep_num)  = 0;
    EP_TX_CTRL(ep_num) = UEP_T_RES_NAK | RB_UEP_T_TOG_0;
  }

  return true;
}

void ch56x_usb2_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  // tu_edpt_number() returns 0..15: an out-of-range id would index past ep_mod[]/the endpoint
  // register window, and ep_mod[0] resolves to R8_USB_CTRL - closing EP0 would clear
  // RB_USB_INT_BUSY/RB_USB_RESET_SIE in the main control register (mirror edpt_open)
  TU_ASSERT(ep_num < CFG_TUD_WCH_USBHS_EP_MAX, );

  if (ep_num == 0) {
    return;
  }

  ep_mode_set(ep_num, dir, false);
  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = UEP_R_RES_NAK | RB_UEP_R_TOG_0;
    EP_MAX_LEN(ep_num) = 0;
    ep_data_tog[ep_num][TUSB_DIR_OUT] = false;
  } else { // TUSB_DIR_IN
    EP_TX_CTRL(ep_num) = UEP_T_RES_NAK | RB_UEP_T_TOG_0;
    EP_TX_LEN(ep_num)  = 0;
    ep_data_tog[ep_num][TUSB_DIR_IN] = false;
  }
  // Drop any in-flight transfer so a later clear-halt cannot DMA into this stale buffer
  xfer_status[ep_num][dir].valid = false;
}

void ch56x_usb2_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);
  // Same bound edpt_open()/edpt_close() should apply: tu_edpt_number() yields 0..15, while the
  // endpoint register window and xfer_status hold EP_MAX. usbd already rejects a larger id
  // before it reaches here, so this is defence in depth, not a reachable hole.
  TU_ASSERT(ep_num < EP_MAX, );

  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = UEP_R_RES_STALL;
  } else {
    EP_TX_LEN(ep_num) = 0;
    EP_TX_CTRL(ep_num) = UEP_T_RES_STALL;
  }
}

void ch56x_usb2_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);
  // Same bound edpt_open()/edpt_close() should apply: tu_edpt_number() yields 0..15, while the
  // endpoint register window and xfer_status hold EP_MAX. usbd already rejects a larger id
  // before it reaches here, so this is defence in depth, not a reachable hole.
  TU_ASSERT(ep_num < EP_MAX, );

  if (dir == TUSB_DIR_OUT) {
    ep_data_tog[ep_num][TUSB_DIR_OUT] = false; // clear-halt resets the toggle to DATA0
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
    if (xfer->valid) {
      // A receive is still armed (the class driver considers it submitted and won't re-arm it);
      // re-queue it instead of leaving it NAKing, or the endpoint NAKs forever after clear-halt
      // (usbtest toggle test 29 clears the halt on an armed bulk-OUT pipe)
      queue_out_packet(ep_num, xfer);
    } else {
      EP_RX_CTRL(ep_num) = UEP_R_RES_NAK | RB_UEP_R_TOG_0;
    }
  } else {
    ep_data_tog[ep_num][TUSB_DIR_IN] = false; // clear-halt resets the toggle to DATA0
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
    if (xfer->valid) {
      // A transmit is still armed (the class driver considers it submitted and won't re-arm it);
      // re-queue it instead of leaving it NAKing (usbtest halt test 13 clears the halt on an armed
      // bulk-IN pipe). queued_len tracks ARMED bytes and edpt_stall (EP_TX_LEN=0) discarded exactly
      // the one armed packet: rewind only that packet, so a mid-transfer halt-clear does not
      // retransmit packets the host already ACKed.
      uint16_t rearm = (uint16_t)(xfer->queued_len % xfer->max_size);
      if (rearm == 0 && xfer->queued_len != 0) {
        rearm = xfer->max_size;
      }
      xfer->queued_len -= rearm;
      queue_in_packet(ep_num, xfer);
    } else {
      EP_TX_CTRL(ep_num) = UEP_T_RES_NAK | RB_UEP_T_TOG_0;
    }
  }
}

bool ch56x_usb2_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->buffer     = buffer;
  xfer->total_len  = total_bytes;
  xfer->queued_len = 0;
  // DMA directly from/to the transfer buffer only if it is RAMX + 16-byte aligned AND every
  // re-arm stays aligned: re-arms advance by max_size multiples, so a multi-packet transfer
  // needs max_size to be a 16-byte multiple. Else bounce packet-by-packet.
  xfer->bounce     = (total_bytes != 0) &&
                     (!ch56x_is_dma_capable(buffer) ||
                      ((total_bytes > xfer->max_size) && (xfer->max_size & 15u) != 0u));
  if (xfer->bounce && ep_num != 0) {
    TU_ASSERT(xfer->max_size <= BOUNCE_BUF_SIZE);
  }
  xfer->valid = true;

  if (ep_num == 0 && dir == TUSB_DIR_OUT) {
    if (total_bytes == 0) {
      EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(RB_UEP_R_TOG_MASK)) | RB_UEP_R_TOG_1;
    } else {
      EP_RX_CTRL(0) ^= RB_UEP_R_TOG_1;
    }
  }

  if (dir == TUSB_DIR_IN) {
    update_in(rhport, ep_num, true);
  } else {
    queue_out_packet(ep_num, xfer);
  }

  return true;
}

void ch56x_usb2_sof_enable(uint8_t rhport, bool en) {
  (void)rhport;
  if (en) {
    R8_USB_INT_EN |= RB_USB_IE_SOF;
  } else {
    R8_USB_INT_EN &= (uint8_t)~(RB_USB_IE_SOF);
  }
}

void ch56x_usb2_int_handler(uint8_t rhport) {
  uint8_t int_flag   = R8_USB_INT_FG;
  uint8_t int_status = R8_USB_INT_ST;

  if (int_flag & RB_USB_IF_TRANSFER) {
    const uint8_t token  = int_status & RB_DEV_TOKEN_MASK;
    const uint8_t ep_num = int_status & RB_DEV_ENDP_MASK;
    const uint16_t len   = R16_USB_RX_LEN;

    if (token == UIS_TOKEN_SOF) {
      uint32_t frame_count = R16_USB_FRAME_NO & 0x7FF;
      dcd_event_sof(rhport, frame_count, true);
    } else if (ep_num >= EP_MAX) {
      // RB_DEV_ENDP_MASK is 4-bit but this block implements 8 endpoints: a spurious/reserved
      // endpoint id would index past xfer_status and the endpoint register window - discard
    } else if (token == UIS_TOKEN_OUT) {
      // A packet whose PID does not match the endpoint's expected toggle is a host retransmission
      // after a lost ACK: the SIE stores it and raises this interrupt anyway, and the datasheet
      // requires software to discard unsynchronized data. Dropping it here leaves the endpoint armed
      // (DMA address and RES untouched) for the next attempt. Only data endpoints are gated: their
      // expected toggle is programmed on every arm, while EP0 derives it from the control stages and
      // iso endpoints run without handshake or toggle.
      if ((ep_num == 0) || xfer_status[ep_num][TUSB_DIR_OUT].is_iso || (int_status & RB_USB_ST_TOGOK)) {
        update_out(rhport, ep_num, len);
      }
    } else if (token == UIS_TOKEN_IN) {
      update_in(rhport, ep_num, false);
    }
    R8_USB_INT_FG = RB_USB_IF_TRANSFER; /* Clear flag */
  } else if (int_flag & RB_USB_IF_SETUOACT) {
    const tusb_control_request_t *setup = (const tusb_control_request_t *)(uintptr_t)ep0_buffer;
    ep0_tog = true;
    // A SETUP resets the control pipe: drop any stage still armed from a transfer the host just
    // abandoned (a new SETUP may legally abort one, USB 2.0 8.5.3). Otherwise a completion landing
    // between this SETUP and usbd's next submission would be reported against the stale transfer,
    // advancing the new control request's state machine with the old one's result.
    xfer_status[0][TUSB_DIR_OUT].valid = false;
    xfer_status[0][TUSB_DIR_IN].valid = false;
    EP_RX_CTRL(0) = (setup->wLength == 0) ? UEP_R_RES_ACK : UEP_R_RES_NAK;
    EP_TX_CTRL(0) = UEP_T_RES_NAK;

    dcd_event_setup_received(rhport, ep0_buffer, true);

    R8_USB_INT_FG = RB_USB_IF_SETUOACT; /* Clear flag */
  } else if (int_flag & RB_USB_IF_BUSRST) {
    // Same as CH32 USBHS: actual speed is not known yet at bus reset start; the controller is
    // configured for high speed (UCST_HS), report high speed
    dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, true);

    R8_USB_DEV_AD = 0;
    memset((void *)ep_data_tog, 0, sizeof(ep_data_tog));
    ep0_tog = true;
    EP_RX_CTRL(0) = UEP_R_RES_ACK | RB_UEP_R_TOG_0;
    EP_TX_CTRL(0) = UEP_T_RES_NAK | RB_UEP_T_TOG_0;

    R8_USB_INT_FG = RB_USB_IF_BUSRST; /* Clear flag */
  } else if (int_flag & RB_USB_IF_SUSPEND) {
    // This single interrupt fires on both suspend and resume; the live suspend status bit
    // distinguishes them (mirror dcd_ch32_usbfs.c).
    const dcd_event_t event = {.rhport = rhport,
                               .event_id = (R8_USB_MIS_ST & RB_USBBUS_SUSPEND) ? DCD_EVENT_SUSPEND : DCD_EVENT_RESUME};
    dcd_event_handler(&event, true);

    R8_USB_INT_FG = RB_USB_IF_SUSPEND; /* Clear flag */
  } else {
    // Unhandled interrupt
    R8_USB_INT_FG = int_flag; /* Clear all flags */
  }
}

//--------------------------------------------------------------------+
// Device API: standalone USB2 high-speed build (SPEED=high)
//--------------------------------------------------------------------+
#if CFG_TUD_WCH_USBIP_USBHS

#if !TUD_OPT_HIGH_SPEED
  #error OPT_MODE_FULL_SPEED not currently supported on CH56x USBHS
#endif

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rh_init;
  return ch56x_usb2_init(rhport);
}

bool dcd_deinit(uint8_t rhport) {
  (void)rhport;
  ch56x_usb2_int_disable();
  ch56x_usb2_deinit();
  return true;
}

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;
  ch56x_usb2_int_enable();
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;
  ch56x_usb2_int_disable();
}

void dcd_int_handler(uint8_t rhport) {
  ch56x_usb2_int_handler(rhport);
}

void dcd_edpt_close_all(uint8_t rhport) {
  ch56x_usb2_edpt_close_all(rhport);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;
  // Response with zlp status; address is applied in dcd_edpt0_status_complete()
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false);
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void)rhport;
  ch56x_usb2_remote_wakeup();
}

void dcd_connect(uint8_t rhport) {
  (void)rhport;
  ch56x_usb2_connect();
}

void dcd_disconnect(uint8_t rhport) {
  (void)rhport;
  ch56x_usb2_disconnect();
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  ch56x_usb2_sof_enable(rhport, en);
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  ch56x_usb2_edpt0_status_complete(rhport, request);
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
  return ch56x_usb2_edpt_open(rhport, desc_edpt);
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  ch56x_usb2_edpt_close(rhport, ep_addr);
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  ch56x_usb2_edpt_stall(rhport, ep_addr);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  ch56x_usb2_edpt_clear_stall(rhport, ep_addr);
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  return ch56x_usb2_edpt_xfer(rhport, ep_addr, buffer, total_bytes);
}

#endif // CFG_TUD_WCH_USBIP_USBHS

#endif
