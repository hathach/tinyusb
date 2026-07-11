/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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

// USB2.0 high-speed device driver for the WCH CH32H417/CH32H416 USBHS controller
// (@0x40030000). This is WCH's newer USBHS device IP (also on CH564/CH585): per-endpoint
// completion via INT_ST (EP_DIR + EP_ID), a SETUP flag in UEP0_RX_CTRL (RB_UEP_R_SETUP_IS),
// per-endpoint DONE flags, split UEP_TX_EN/UEP_RX_EN enable registers, response encoding
// NAK=00 / STALL=01 / ACK=10, and manual data-toggle. It is NOT register-compatible with the
// CH32V307 (dcd_ch32_usbhs.c) or CH56x (dcd_ch56x_usbhs.c) USBHS blocks. The overall transfer
// state machine mirrors those drivers; the register access and interrupt dispatch are H417.
//
// The driver core is exposed as ch32h417_usb2_* internals (see dcd_ch32h417.h) so the USB3 dcd
// can dispatch to it for the runtime USB2 fallback; the standalone dcd_* entry points below are
// compiled when the USBHS controller is the selected controller (SPEED=high).

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USBHS_H417) && \
    ((defined(CFG_TUD_WCH_USBIP_USBHS) && CFG_TUD_WCH_USBIP_USBHS == 1) || \
     (defined(CFG_TUD_WCH_USBIP_USB30) && CFG_TUD_WCH_USBIP_USB30 == 1 && \
      defined(CFG_TUD_WCH_USB30_FALLBACK) && CFG_TUD_WCH_USB30_FALLBACK == 1))

#include "device/dcd.h"
#include "dcd_ch32h417.h"
#include "ch32h417_usbhs_reg.h"

// Max number of bi-directional endpoints including EP0 (EP0..EP7)
#define EP_MAX 8

typedef struct {
  uint8_t *buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_size;
  bool     is_iso;
  bool     valid;
} xfer_ctl_t;

#define XFER_CTL_BASE(_ep, _dir) (&xfer_status[_ep][_dir])
static xfer_ctl_t xfer_status[EP_MAX][2];

TU_ATTR_ALIGNED(4) static uint8_t ep0_buffer[CFG_TUD_ENDPOINT0_SIZE];
static bool ep0_tog;
static bool ep_data_tog[EP_MAX][2];

//--------------------------------------------------------------------+
// Endpoint helpers
//--------------------------------------------------------------------+

static void set_tx_res(uint8_t ep_num, uint8_t res) {
  EP_TX_CTRL(ep_num) = (uint8_t)((EP_TX_CTRL(ep_num) & ~USBHS_UEP_T_RES_MASK) | res);
}

static void set_rx_res(uint8_t ep_num, uint8_t res) {
  EP_RX_CTRL(ep_num) = (uint8_t)((EP_RX_CTRL(ep_num) & ~USBHS_UEP_R_RES_MASK) | res);
}

static void set_ep_toggle(uint8_t ep_num, tusb_dir_t ep_dir, bool data1) {
  if (ep_dir == TUSB_DIR_IN) {
    EP_TX_CTRL(ep_num) = (uint8_t)((EP_TX_CTRL(ep_num) & ~USBHS_UEP_T_TOG_MASK) |
                                   (data1 ? USBHS_UEP_T_TOG_DATA1 : USBHS_UEP_T_TOG_DATA0));
  } else {
    EP_RX_CTRL(ep_num) = (uint8_t)((EP_RX_CTRL(ep_num) & ~USBHS_UEP_R_TOG_MASK) |
                                   (data1 ? USBHS_UEP_R_TOG_DATA1 : USBHS_UEP_R_TOG_DATA0));
  }
}

static void queue_in_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t tx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num == 0) {
    memcpy(ep0_buffer, &xfer->buffer[xfer->queued_len], tx_len);
  } else {
    EP_TX_DMA(ep_num) = (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
  }

  EP_TX_LEN(ep_num) = tx_len;
  xfer->queued_len += tx_len;

  if (ep_num == 0) {
    EP_TX_CTRL(0) = (uint8_t)(USBHS_UEP_T_RES_ACK | (ep0_tog ? USBHS_UEP_T_TOG_DATA1 : USBHS_UEP_T_TOG_DATA0));
    ep0_tog = !ep0_tog;
  } else if (xfer->is_iso) {
    set_tx_res(ep_num, USBHS_UEP_T_RES_ACK); // isochronous: no handshake
  } else {
    set_ep_toggle(ep_num, TUSB_DIR_IN, ep_data_tog[ep_num][TUSB_DIR_IN]);
    set_tx_res(ep_num, USBHS_UEP_T_RES_ACK);
  }
}

static void queue_out_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t rx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num > 0) {
    EP_RX_DMA(ep_num) = (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
    EP_MAX_LEN(ep_num) = rx_len;
  }

  if (ep_num == 0) {
    set_rx_res(0, USBHS_UEP_R_RES_ACK);
  } else if (xfer->is_iso) {
    set_rx_res(ep_num, USBHS_UEP_R_RES_ACK);
  } else {
    set_ep_toggle(ep_num, TUSB_DIR_OUT, ep_data_tog[ep_num][TUSB_DIR_OUT]);
    set_rx_res(ep_num, USBHS_UEP_R_RES_ACK);
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
      EP_TX_CTRL(0) = (uint8_t)(USBHS_UEP_T_RES_NAK | (ep0_tog ? USBHS_UEP_T_TOG_DATA1 : USBHS_UEP_T_TOG_DATA0));
    } else {
      set_tx_res(ep_num, USBHS_UEP_T_RES_NAK);
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
  }

  xfer->queued_len += len;

  if (ep_num != 0 && !xfer->is_iso) {
    ep_data_tog[ep_num][TUSB_DIR_OUT] = !ep_data_tog[ep_num][TUSB_DIR_OUT];
  }

  if ((xfer->queued_len == xfer->total_len) || (len < xfer->max_size)) {
    xfer->valid = false;
    if (ep_num == 0) {
      set_rx_res(0, USBHS_UEP_R_RES_NAK);
    }
    dcd_event_xfer_complete(rhport, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }

  if (ep_num != 0) {
    if (xfer->valid) {
      queue_out_packet(ep_num, xfer);
    } else {
      set_rx_res(ep_num, xfer->is_iso ? USBHS_UEP_R_RES_ACK : USBHS_UEP_R_RES_NAK);
    }
  }
}

//--------------------------------------------------------------------+
// Controller internals, shared with the USB3.0 dcd for runtime fallback
//--------------------------------------------------------------------+

bool ch32h417_usb2_init(uint8_t rhport) {
  (void)rhport;

  memset(&xfer_status, 0, sizeof(xfer_status));
  memset(ep_data_tog, 0, sizeof(ep_data_tog));
  ep0_tog = true;

  USBHSD->CONTROL = USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM;
  USBHSD->INT_EN = USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND | USBHS_UDIE_TRANSFER;

  // Only EP0 enabled at reset; data endpoints are enabled by dcd_edpt_open
  USBHSD->UEP_TX_EN = USBHS_UEP0_T_EN;
  USBHSD->UEP_RX_EN = USBHS_UEP0_R_EN;
  USBHSD->UEP_TX_ISO = 0;
  USBHSD->UEP_RX_ISO = 0;

  USBHSD->UEP0_DMA = (uint32_t)(uintptr_t)ep0_buffer;
  USBHSD->UEP0_MAX_LEN = CFG_TUD_ENDPOINT0_SIZE;
  xfer_status[0][TUSB_DIR_OUT].max_size = CFG_TUD_ENDPOINT0_SIZE;
  xfer_status[0][TUSB_DIR_IN].max_size = CFG_TUD_ENDPOINT0_SIZE;

  EP_TX_LEN(0) = 0;
  EP_TX_CTRL(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
  EP_RX_CTRL(0) = USBHS_UEP_R_RES_ACK | USBHS_UEP_R_TOG_DATA0;

  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep) = 0;
    EP_TX_CTRL(ep) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    EP_RX_CTRL(ep) = USBHS_UEP_R_RES_NAK | USBHS_UEP_R_TOG_DATA0;
    EP_MAX_LEN(ep) = 0;
  }

  USBHSD->BASE_MODE = USBHS_UD_SPEED_HIGH;
  USBHSD->DEV_AD = 0;
  USBHSD->CONTROL = USBHS_UD_DEV_EN | USBHS_UD_DMA_EN | USBHS_UD_PHY_SUSPENDM;

  return true;
}

void ch32h417_usb2_deinit(void) {
  USBHSD->CONTROL = USBHS_UD_RST_SIE | USBHS_UD_RST_LINK;
}

void ch32h417_usb2_int_enable(void) {
  NVIC_EnableIRQ(USBHS_IRQn);
}

void ch32h417_usb2_int_disable(void) {
  NVIC_DisableIRQ(USBHS_IRQn);
}

void ch32h417_usb2_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;
  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_ADDRESS) {
    USBHSD->DEV_AD = (uint8_t)request->wValue;
  }
}

bool ch32h417_usb2_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(desc_edpt->bEndpointAddress);
  const tusb_dir_t dir = tu_edpt_dir(desc_edpt->bEndpointAddress);
  TU_ASSERT(ep_num < EP_MAX);

  if (ep_num == 0) {
    return true;
  }

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->max_size = tu_edpt_packet_size(desc_edpt);
  xfer->is_iso = (desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  ep_data_tog[ep_num][dir] = false;

  if (dir == TUSB_DIR_OUT) {
    USBHSD->UEP_RX_EN |= (uint16_t)(USBHS_UEP0_R_EN << ep_num);
    if (xfer->is_iso) {
      USBHSD->UEP_RX_ISO |= (uint16_t)(1u << ep_num);
    }
    EP_MAX_LEN(ep_num) = xfer->max_size;
    EP_RX_CTRL(ep_num) = USBHS_UEP_R_RES_NAK | USBHS_UEP_R_TOG_DATA0;
  } else {
    USBHSD->UEP_TX_EN |= (uint16_t)(USBHS_UEP0_T_EN << ep_num);
    if (xfer->is_iso) {
      USBHSD->UEP_TX_ISO |= (uint16_t)(1u << ep_num);
    }
    EP_TX_LEN(ep_num) = 0;
    EP_TX_CTRL(ep_num) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
  }

  return true;
}

void ch32h417_usb2_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = USBHS_UEP_R_RES_NAK | USBHS_UEP_R_TOG_DATA0;
    EP_MAX_LEN(ep_num) = 0;
    USBHSD->UEP_RX_ISO &= (uint16_t)~(1u << ep_num);
    USBHSD->UEP_RX_EN &= (uint16_t)~(USBHS_UEP0_R_EN << ep_num);
  } else {
    EP_TX_CTRL(ep_num) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    EP_TX_LEN(ep_num) = 0;
    USBHSD->UEP_TX_ISO &= (uint16_t)~(1u << ep_num);
    USBHSD->UEP_TX_EN &= (uint16_t)~(USBHS_UEP0_T_EN << ep_num);
  }
  ep_data_tog[ep_num][dir] = false;
  XFER_CTL_BASE(ep_num, dir)->valid = false;
}

void ch32h417_usb2_edpt_close_all(uint8_t rhport) {
  (void)rhport;
  memset(ep_data_tog, 0, sizeof(ep_data_tog));
  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep) = 0;
    EP_TX_CTRL(ep) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    EP_RX_CTRL(ep) = USBHS_UEP_R_RES_NAK | USBHS_UEP_R_TOG_DATA0;
    EP_MAX_LEN(ep) = 0;
    xfer_status[ep][0].valid = false;
    xfer_status[ep][1].valid = false;
  }
  USBHSD->UEP_TX_EN = USBHS_UEP0_T_EN;
  USBHSD->UEP_RX_EN = USBHS_UEP0_R_EN;
  USBHSD->UEP_TX_ISO = 0;
  USBHSD->UEP_RX_ISO = 0;
}

bool ch32h417_usb2_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  xfer->valid = true;

  if (ep_num == 0 && dir == TUSB_DIR_OUT) {
    // The EP0 data/status OUT stage after a SETUP is DATA1. Write the full control byte (not a
    // read-modify-write) so RB_UEP_R_SETUP_IS is cleared - otherwise the next OUT would be
    // re-detected as a SETUP in the ISR. Applies to both the zero-length status and a
    // (single-packet, EP0-sized) control-write data stage.
    EP_RX_CTRL(0) = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
    return true;
  }

  if (dir == TUSB_DIR_IN) {
    update_in(rhport, ep_num, true);
  } else {
    queue_out_packet(ep_num, xfer);
  }

  return true;
}

void ch32h417_usb2_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_OUT) {
    set_rx_res(ep_num, USBHS_UEP_R_RES_STALL);
  } else {
    EP_TX_LEN(ep_num) = 0;
    set_tx_res(ep_num, USBHS_UEP_T_RES_STALL);
  }
}

void ch32h417_usb2_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_OUT) {
    ep_data_tog[ep_num][TUSB_DIR_OUT] = false; // clear-halt resets the toggle to DATA0
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
    if (xfer->valid) {
      // A receive is still armed; re-queue it instead of leaving it NAKing (usbtest case 29
      // clears the halt on an armed bulk-OUT pipe).
      queue_out_packet(ep_num, xfer);
    } else {
      EP_RX_CTRL(ep_num) = USBHS_UEP_R_RES_NAK | USBHS_UEP_R_TOG_DATA0;
    }
  } else {
    EP_TX_CTRL(ep_num) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    ep_data_tog[ep_num][TUSB_DIR_IN] = false;
  }
}

void ch32h417_usb2_sof_enable(uint8_t rhport, bool en) {
  (void)rhport;
  if (en) {
    USBHSD->INT_EN |= USBHS_UDIE_SOF_ACT;
  } else {
    USBHSD->INT_EN &= (uint8_t)~USBHS_UDIE_SOF_ACT;
  }
}

void ch32h417_usb2_int_handler(uint8_t rhport) {
  uint8_t intflag = USBHSD->INT_FG;
  uint8_t intst = USBHSD->INT_ST;

  if (intflag & USBHS_UDIF_TRANSFER) {
    const uint8_t ep_num = intst & USBHS_UDIS_EP_ID_MASK;
    if (intst & USBHS_UDIS_EP_DIR) {
      // IN transaction completed
      EP_TX_CTRL(ep_num) &= (uint8_t)~USBHS_UEP_T_DONE;
      update_in(rhport, ep_num, false);
    } else {
      // SETUP or OUT transaction
      if (ep_num == 0 && (EP_RX_CTRL(0) & USBHS_UEP_R_SETUP_IS)) {
        tusb_control_request_t const *setup = (tusb_control_request_t const *)ep0_buffer;
        ep0_tog = true;
        // Full-byte writes (DATA1) clear RB_UEP_R_SETUP_IS + DONE and set the post-SETUP toggle
        EP_TX_CTRL(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA1;
        EP_RX_CTRL(0) = (uint8_t)(((setup->wLength == 0) ? USBHS_UEP_R_RES_ACK : USBHS_UEP_R_RES_NAK) |
                                  USBHS_UEP_R_TOG_DATA1);
        dcd_event_setup_received(rhport, ep0_buffer, true);
      } else {
        EP_RX_CTRL(ep_num) &= (uint8_t)~USBHS_UEP_R_DONE;
        update_out(rhport, ep_num, EP_RX_LEN(ep_num));
      }
    }
    // The transfer interrupt is acknowledged by clearing the per-endpoint DONE bit above (matching
    // the vendor driver); do NOT also write INT_FG, which would clear the aggregate flag and could
    // drop a second endpoint's pending completion (only one is serviced per IRQ).
  } else if (intflag & USBHS_UDIF_RX_SOF) {
    dcd_event_sof(rhport, USBHSD->FRAME_NO & USBHS_UD_FRAME_NO, true);
    USBHSD->INT_FG = USBHS_UDIF_RX_SOF;
  } else if (intflag & USBHS_UDIF_BUS_RST) {
    dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, true);
    USBHSD->DEV_AD = 0;
    memset(ep_data_tog, 0, sizeof(ep_data_tog));
    ep0_tog = true;
    EP_RX_CTRL(0) = USBHS_UEP_R_RES_ACK | USBHS_UEP_R_TOG_DATA0;
    EP_TX_CTRL(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    USBHSD->INT_FG = USBHS_UDIF_BUS_RST;
  } else if (intflag & USBHS_UDIF_SUSPEND) {
    dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_SUSPEND};
    dcd_event_handler(&event, true);
    USBHSD->INT_FG = USBHS_UDIF_SUSPEND;
  } else {
    USBHSD->INT_FG = intflag;
  }
}

//--------------------------------------------------------------------+
// dcd API - compiled only when the USBHS controller is the active dcd (SPEED=high)
//--------------------------------------------------------------------+

#if defined(CFG_TUD_WCH_USBIP_USBHS) && CFG_TUD_WCH_USBIP_USBHS == 1

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rh_init;
  return ch32h417_usb2_init(rhport);
}

void dcd_int_enable(uint8_t rhport) { (void)rhport; ch32h417_usb2_int_enable(); }
void dcd_int_disable(uint8_t rhport) { (void)rhport; ch32h417_usb2_int_disable(); }
void dcd_int_handler(uint8_t rhport) { ch32h417_usb2_int_handler(rhport); }

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false); // status ZLP; address applied at status-complete
}

void dcd_remote_wakeup(uint8_t rhport) { (void)rhport; }
void dcd_sof_enable(uint8_t rhport, bool en) { ch32h417_usb2_sof_enable(rhport, en); }

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  ch32h417_usb2_edpt0_status_complete(rhport, request);
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
  return ch32h417_usb2_edpt_open(rhport, desc_edpt);
}
void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) { ch32h417_usb2_edpt_close(rhport, ep_addr); }
void dcd_edpt_close_all(uint8_t rhport) { ch32h417_usb2_edpt_close_all(rhport); }

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  return ch32h417_usb2_edpt_xfer(rhport, ep_addr, buffer, total_bytes);
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) { ch32h417_usb2_edpt_stall(rhport, ep_addr); }
void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) { ch32h417_usb2_edpt_clear_stall(rhport, ep_addr); }

#endif

#endif
