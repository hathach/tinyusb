/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
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
// USB 2.0 EP0 max packet size is 64; a SuperSpeed-capable build sets CFG_TUD_ENDPOINT0_SIZE to 512
#define EP0_MAX_SIZE TU_MIN(CFG_TUD_ENDPOINT0_SIZE, 64)

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

// EP0 DMA buffer: sized by the USB2 EP0 max packet, not CFG_TUD_ENDPOINT0_SIZE (512 on a
// SuperSpeed-capable build) - UEP0_MAX_LEN is a 7-bit field (RM 25.2.1.25), EP0 never moves more
TU_ATTR_ALIGNED(4) static uint8_t ep0_buffer[EP0_MAX_SIZE];
static volatile bool ep0_tog;    // EP0 expected IN toggle
static volatile bool ep0_rx_tog; // EP0 expected OUT toggle
static volatile bool sof_enabled; // usbd asked for SOF (see the RX_SOF branch in the ISR)
static volatile bool bus_suspended; // last suspend state reported to usbd (see the SUSPEND branch)
static volatile bool ep_data_tog[EP_MAX][2];

//--------------------------------------------------------------------+
// Endpoint helpers
//--------------------------------------------------------------------+

// Read-modify-write of the response / toggle field. The DONE bit (bit 7) is deliberately written
// back as 1: it is RW0 (RM 25.2.1.31/32/34/35 - "writes 0 to clear it"), so writing 1 cannot set it
// and cannot raise a completion, while writing 0 WOULD acknowledge one. Without the forced 1, a
// completion the SIE raises between this read and its write is silently cleared and never reported,
// hanging that transfer until the host times out. These helpers run in task context and are not
// confined to idle endpoints - edpt_stall()/edpt_clear_stall() call them on an endpoint that may be
// actively armed and ACKing, because a class driver may halt an endpoint with a transfer in flight
// (usbtest case 13 does exactly that).
static void set_tx_res(uint8_t ep_num, uint8_t res) {
  EP_TX_CTRL(ep_num) = (uint8_t)((EP_TX_CTRL(ep_num) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_DONE | res);
}

static void set_rx_res(uint8_t ep_num, uint8_t res) {
  EP_RX_CTRL(ep_num) = (uint8_t)((EP_RX_CTRL(ep_num) & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_DONE | res);
}

static void set_ep_toggle(uint8_t ep_num, tusb_dir_t ep_dir, bool data1) {
  if (ep_dir == TUSB_DIR_IN) {
    EP_TX_CTRL(ep_num) = (uint8_t)((EP_TX_CTRL(ep_num) & ~USBHS_UEP_T_TOG_MASK) | USBHS_UEP_T_DONE |
                                   (data1 ? USBHS_UEP_T_TOG_DATA1 : USBHS_UEP_T_TOG_DATA0));
  } else {
    EP_RX_CTRL(ep_num) = (uint8_t)((EP_RX_CTRL(ep_num) & ~USBHS_UEP_R_TOG_MASK) | USBHS_UEP_R_DONE |
                                   (data1 ? USBHS_UEP_R_TOG_DATA1 : USBHS_UEP_R_TOG_DATA0));
  }
}

// Arm EP0 OUT: full-byte write (not a read-modify-write), so the response and the expected toggle
// land without carrying over stale status bits. EP0 has no hardware auto-toggle
// (R16_UEP_R_TOG_AUTO covers EP1..EP7 only), so the expected toggle comes from ep0_rx_tog.
// NOTE: being a full-byte write this clears RB_UEP_R_DONE, so it must never run while an
// unserviced RX completion is pending. Both call sites are clear of that: update_out() re-arms for
// the next packet of a multi-packet control-OUT, having just had that packet's DONE cleared by the
// scan, and edpt_xfer() runs with EP0 NAKing, since usbd submits a stage only once the previous
// one has completed, so no transaction is in flight to raise DONE underneath it.
static void arm_ep0_out(void) {
  EP_RX_CTRL(0) = (uint8_t)((ep0_rx_tog ? USBHS_UEP_R_TOG_DATA1 : USBHS_UEP_R_TOG_DATA0) | USBHS_UEP_R_RES_ACK);
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

  if (ep_num == 0) {
    // Manual-toggle hardware: advance the expected EP0 RX toggle on every received packet. usbd
    // submits a control-OUT data stage one EP0-sized chunk per dcd_edpt_xfer(), so the toggle must
    // survive across chunks (packet 2, 4, ... are DATA0) as well as within a multi-packet stage.
    ep0_rx_tog = !ep0_rx_tog;
    if (xfer->valid) {
      arm_ep0_out();
    }
  } else if (xfer->valid) {
    queue_out_packet(ep_num, xfer);
  } else {
    set_rx_res(ep_num, xfer->is_iso ? USBHS_UEP_R_RES_ACK : USBHS_UEP_R_RES_NAK);
  }
}

//--------------------------------------------------------------------+
// Controller internals, shared with the USB3.0 dcd for runtime fallback
//--------------------------------------------------------------------+

bool ch32h417_usb2_init(uint8_t rhport) {
  (void)rhport;

  memset(&xfer_status, 0, sizeof(xfer_status));
  memset((void *)ep_data_tog, 0, sizeof(ep_data_tog));
  ep0_tog = true;
  ep0_rx_tog = false;
  // Reset every latch this file keeps, not just the toggles: init rewrites INT_EN below without
  // USBHS_UDIE_SOF_ACT, so a sof_enabled left over from a previous life would leave the software
  // latch on with the hardware enable off. The RX_SOF branch is reached on any interrupt that
  // leaves no transfer pending, so it would emit SOF events usbd never asked for - and usbd turns
  // an unexpected SOF into a RESUME, cancelling a genuine suspend. This runs on the USB3 fallback
  // path and on tud_deinit()/tud_init(), so these flags genuinely survive across it.
  sof_enabled = false;
  bus_suspended = false;

  // 480 MHz USBHS PLL (HSE 25 MHz reference) + UTMI + peripheral clock, transcribed from the
  // vendor USBHS_RCC_Init. Without this the controller accepts register writes but the PHY
  // never drives D+/D-: the host then detects only the idle debug-probe level on the shared
  // PB8/PB9 pins as a phantom low-speed device (error -71 churn; seen on hardware). The USB3
  // fallback path also needs it: usbss_device_init(false) turns UTMI off again on the way here.
  if ((RCC->PLLCFGR & RCC_SYSPLL_SEL) != RCC_SYSPLL_USBHS) {
    RCC_USBHS_PLLCmd(DISABLE);
    RCC_USBHSPLLCLKConfig(RCC_USBHSPLLSource_HSE);
    RCC_USBHSPLLReferConfig(RCC_USBHSPLLRefer_25M);
    RCC_USBHSPLLClockSourceDivConfig(RCC_USBHSPLL_IN_Div1);
    RCC_USBHS_PLLCmd(ENABLE);
    // bounded, best effort: a PLL that never locks (e.g. bad HSE) must not hang the core - in the
    // USB3 fallback path this runs from the LINK ISR
    uint32_t timeout = 1000000;
    while (!(RCC->CTLR & RCC_USBHS_PLLRDY) && --timeout) {}
  }
  RCC_UTMIcmd(ENABLE);
  RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS, ENABLE);

  USBHSD->CONTROL = USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM;
  // vendor-exact interrupt set: BUS_SLEEP/LPM_ACT/LINK_RDY are cleared-if-unhandled by the ISR
  USBHSD->INT_EN = USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND | USBHS_UDIE_BUS_SLEEP |
                   USBHS_UDIE_LPM_ACT | USBHS_UDIE_TRANSFER | USBHS_UDIE_LINK_RDY;

  // Only EP0 enabled at reset; data endpoints are enabled by dcd_edpt_open
  USBHSD->UEP_TX_EN = USBHS_UEP0_T_EN;
  USBHSD->UEP_RX_EN = USBHS_UEP0_R_EN;
  USBHSD->UEP_TX_ISO = 0;
  USBHSD->UEP_RX_ISO = 0;

  USBHSD->UEP0_DMA = (uint32_t)(uintptr_t)ep0_buffer;
  // clamp: a SuperSpeed-capable build sets CFG_TUD_ENDPOINT0_SIZE to 512, but on this USB2
  // fallback link EP0 max packet size is 64 (matches usbd's ep0_xact_limit and EP0_SIZE_FSHS)
  USBHSD->UEP0_MAX_LEN = EP0_MAX_SIZE;
  xfer_status[0][TUSB_DIR_OUT].max_size = EP0_MAX_SIZE;
  xfer_status[0][TUSB_DIR_IN].max_size = EP0_MAX_SIZE;

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
  USBHSD->CONTROL = USBHS_UD_DEV_EN | USBHS_UD_DMA_EN | USBHS_UD_LPM_EN | USBHS_UD_PHY_SUSPENDM;

  return true;
}

void ch32h417_usb2_deinit(void) {
  USBHSD->CONTROL = USBHS_UD_RST_SIE | USBHS_UD_RST_LINK;
}

void ch32h417_usb2_connect(void) {
  USBHSD->CONTROL |= USBHS_UD_DEV_EN; // attach: enable the device (pull-up on)
}

void ch32h417_usb2_disconnect(void) {
  USBHSD->CONTROL &= (uint32_t)~USBHS_UD_DEV_EN; // detach: disable the device (pull-up off)
}

// Resume signalling: RB_UD_REMOTE_WKUP is RW1Z, the hardware drives K and clears the bit itself
// (vendor USBHS_Send_Resume)
void ch32h417_usb2_remote_wakeup(void) {
  USBHSD->WAKE_CTRL |= USBHS_UD_REMOTE_WKUP;
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
  TU_ASSERT(ep_num < EP_MAX, ); // tu_edpt_number() returns 0..15: an out-of-range id would write
                                // past the endpoint register block (mirror edpt_open)

  if (ep_num == 0) {
    return; // EP0 is never closed; its registers stay armed for the next control transfer
  }

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
  memset((void *)ep_data_tog, 0, sizeof(ep_data_tog));
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
    // Data or status OUT stage: ep0_rx_tog holds DATA1 right after a SETUP and is advanced per
    // received packet, so a multi-chunk control-write stays synchronized with the host.
    arm_ep0_out();
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
      EP_TX_CTRL(ep_num) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    }
  }
}

void ch32h417_usb2_sof_enable(uint8_t rhport, bool en) {
  sof_enabled = en;
  (void)rhport;
  if (en) {
    USBHSD->INT_EN |= USBHS_UDIE_SOF_ACT;
  } else {
    USBHSD->INT_EN &= (uint8_t)~USBHS_UDIE_SOF_ACT;
  }
}

void ch32h417_usb2_int_handler(uint8_t rhport) {
  uint8_t intflag = USBHSD->INT_FG;

  if (intflag & USBHS_UDIF_TRANSFER) {
    // RB_UDIF_RTX_ACT (INT_FG bit 4) is read-only and is the wired-OR of every endpoint's DONE
    // flag - RM 25.2.1.8: "Receive cleared by RB_UEP_R_DONE; Transmit cleared by RB_UEP_T_DONE" -
    // while INT_ST names only ONE endpoint. Servicing just that endpoint leaves the other
    // completion pending, and the SIE then refuses new transactions: a SETUP that arrives while
    // another endpoint still has DONE set gets no handshake at all. That is fatal, because a SETUP
    // may not be NAKed or STALLed - the host retries three times and fails with -EPROTO
    // (wire-confirmed: SETUP dropped 64 us after a bulk-IN on EP1 completed). So walk every
    // endpoint and drain all pending completions before leaving the handler, instead of trusting
    // INT_ST to name the only one that matters. RX is serviced before TX for each endpoint so that
    // a SETUP is dispatched first: an EP0 IN completion and the next SETUP are routinely pending
    // together, and the SETUP branch delivers that IN completion itself before resetting the
    // control pipe (see below) rather than discarding it.
    for (uint8_t ep_num = 0; ep_num < EP_MAX; ep_num++) {
      if (EP_RX_CTRL(ep_num) & USBHS_UEP_R_DONE) {
        // SETUP or OUT transaction.
        // RB_UEP_R_SETUP_IS (RM 25.2.1.32) is read-only and survives a software write, but the
        // hardware DOES clear it as soon as a non-SETUP packet is received - verified on hardware:
        // EP0 RX reads 0x34 (flag clear) immediately after a status OUT lands, while it still reads
        // set on a genuine SETUP completion. So the flag alone identifies a SETUP, which is also
        // what every WCH demo relies on.
        // Two narrower tests were tried here and are WRONG, do not reintroduce them:
        //   - gating on !xfer_status[0][OUT].valid suppresses the SETUP a host uses to abort an
        //     in-progress control-OUT (USB 2.0 8.5.3), where valid is legitimately still true;
        //   - gating on EP_RX_LEN(0) == 8 misreads any 8-byte control-OUT data packet (an 8-byte
        //     class/vendor payload, or usbtest's ctrl_out which varies the length 1..512) as a
        //     SETUP, feeding usbd garbage and discarding the real transfer.
        if (ep_num == 0 && (EP_RX_CTRL(0) & USBHS_UEP_R_SETUP_IS)) {
          tusb_control_request_t const *setup = (tusb_control_request_t const *)ep0_buffer;
          // The status-IN of the previous transfer and this SETUP are often pending together, and
          // the scan services RX before TX. Deliver that completion first: the pipe reset below
          // would drop it (xfer->valid cleared) and the full-byte TX write would clear its DONE,
          // so usbd would never run the transfer's CONTROL_STAGE_ACK - which is where classes
          // apply a received payload (hid_device.c applies SET_REPORT only on that stage).
          // Only when that transfer is actually finished, though. On an unfinished multi-packet
          // control-IN, update_in() would queue the NEXT packet, and queue_in_packet() memcpys it
          // into ep0_buffer - the very buffer holding the SETUP about to be handed to usbd, which
          // would arrive as stale descriptor bytes. A host may legally abort a control-IN with a
          // new SETUP (USB 2.0 8.5.3), e.g. cutting a GET_DESCRIPTOR short, so this is reachable.
          // An abandoned transfer has no completion worth reporting; the pipe reset below drops it.
          const xfer_ctl_t *ep0_in = XFER_CTL_BASE(0, TUSB_DIR_IN);
          if ((EP_TX_CTRL(0) & USBHS_UEP_T_DONE) && ep0_in->valid &&
              (ep0_in->total_len <= ep0_in->queued_len)) {
            EP_TX_CTRL(0) &= (uint8_t)~USBHS_UEP_T_DONE;
            update_in(rhport, 0, false);
          }
          ep0_tog = true;
          ep0_rx_tog = true;
          // A SETUP resets the control pipe: drop any stage still armed from a transfer the host
          // just abandoned, so it cannot later swallow a packet belonging to this new one.
          xfer_status[0][TUSB_DIR_OUT].valid = false;
          xfer_status[0][TUSB_DIR_IN].valid = false;
          // Full-byte writes (DATA1) clear DONE and set the post-SETUP toggle. RB_UEP_R_SETUP_IS is
          // read-only (RM 25.2.1.32) and survives this write - measured on hardware, EP0 RX reads
          // back with the flag still set. The hardware clears it when the next non-SETUP packet is
          // received, which is what keeps the SETUP test below from re-firing on a data-stage OUT.
          EP_TX_CTRL(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA1;
          EP_RX_CTRL(0) = (uint8_t)(((setup->wLength == 0) ? USBHS_UEP_R_RES_ACK : USBHS_UEP_R_RES_NAK) |
                                    USBHS_UEP_R_TOG_DATA1);
          dcd_event_setup_received(rhport, ep0_buffer, true);
        } else {
          // A packet whose PID does not match the endpoint's expected toggle is a host retransmission
          // after a lost ACK: the SIE stores it and raises this interrupt anyway. RM 25.2.1.35
          // RB_UEP_R_TOG_MATCH (RO, bit 4 of UEPn_RX_CTRL): 1 = synchronized, 0 = not synchronized -
          // consuming an unsynchronized packet would inject it into the transfer twice. Dropping it
          // leaves the endpoint armed (DMA address and RES untouched) for the host's next attempt.
          // Only data endpoints are gated: their expected toggle is programmed on every arm, while
          // EP0 derives it from the control stages and iso endpoints run without handshake or toggle
          // (mirrors dcd_ch56x_usbhs.c).
          //
          // RB_UEP_R_DONE is cleared only AFTER update_out() has sampled the length and re-armed
          // DMA: it is the sole interlock on this part. RM 25.2.1.1 lists no equivalent of the
          // CH569's RB_USB_INT_BUSY, so between clearing DONE and queue_out_packet() reprogramming
          // the address the endpoint would sit ACK-armed at the finished packet's address, and a
          // back-to-back OUT would DMA over the packet being accounted for. This mirrors where
          // dcd_ch56x_usbhs.c clears R8_USB_INT_FG. EP_RX_CTRL is re-read because update_out()
          // re-arms the endpoint and rewrites it - clearing from the stale snapshot would undo
          // that arm.
          const uint8_t rx_ctrl = EP_RX_CTRL(ep_num);
          if ((ep_num == 0) || xfer_status[ep_num][TUSB_DIR_OUT].is_iso || (rx_ctrl & USBHS_UEP_R_TOG_MATCH)) {
            update_out(rhport, ep_num, EP_RX_LEN(ep_num));
          }
          EP_RX_CTRL(ep_num) = (uint8_t)(EP_RX_CTRL(ep_num) & ~USBHS_UEP_R_DONE);
        }
      }

      if (EP_TX_CTRL(ep_num) & USBHS_UEP_T_DONE) {
        // IN transaction completed
        EP_TX_CTRL(ep_num) &= (uint8_t)~USBHS_UEP_T_DONE;
        update_in(rhport, ep_num, false);
      }
    }
    // The transfer interrupt is acknowledged by clearing the per-endpoint DONE bits above (matching
    // the vendor driver); do NOT also write INT_FG - RB_UDIF_RTX_ACT is read-only anyway.
  } else if (intflag & USBHS_UDIF_RX_SOF) {
    // The hardware sets RB_UDIF_RX_SOF every frame whether or not the SOF interrupt is enabled, so
    // this branch is reached on ANY interrupt that leaves no transfer pending. Emitting a SOF event
    // unasked flooded usbd's event queue within one enumeration (queue_event asserts, events lost,
    // the host sees the device drop) - and because the branch also short-circuits the chain, the
    // terminal else never ran and an enabled-but-unhandled flag kept the IRQ line asserted, so the
    // ISR re-entered immediately. Always clear the flag; only report it when usbd asked for it.
    USBHSD->INT_FG = USBHS_UDIF_RX_SOF;
    if (sof_enabled) {
      dcd_event_sof(rhport, USBHSD->FRAME_NO & USBHS_UD_FRAME_NO, true);
    }
  } else if (intflag & USBHS_UDIF_BUS_RST) {
    bus_suspended = false;
    dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, true);
    USBHSD->DEV_AD = 0;
    memset((void *)ep_data_tog, 0, sizeof(ep_data_tog));
    ep0_tog = true;
    ep0_rx_tog = false;
    EP_RX_CTRL(0) = USBHS_UEP_R_RES_ACK | USBHS_UEP_R_TOG_DATA0;
    EP_TX_CTRL(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    USBHSD->INT_FG = USBHS_UDIF_BUS_RST;
  } else if (intflag & USBHS_UDIF_SUSPEND) {
    // This single interrupt fires on both suspend and resume; the live suspend status bit
    // distinguishes them. Clear first, then sample, like the vendor demo (CH372Device).
    // EDGE-TRIGGERED: the controller raises this constantly (measured on hardware: ~2000 times
    // during one enumeration), and queueing a usbd event each time overflows the event queue -
    // events are dropped, enumeration dies and the host sees the device disappear. usbd only
    // needs the transitions, so report a change of state and swallow the repeats.
    USBHSD->INT_FG = USBHS_UDIF_SUSPEND;
    const bool now_suspended = (USBHSD->MIS_ST & USBHS_UDMS_SUSPEND) != 0;
    if (now_suspended != bus_suspended) {
      bus_suspended = now_suspended;
      dcd_event_t event = {.rhport = rhport,
                           .event_id = now_suspended ? DCD_EVENT_SUSPEND : DCD_EVENT_RESUME};
      dcd_event_handler(&event, true);
    }
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

bool dcd_deinit(uint8_t rhport) {
  (void)rhport;
  ch32h417_usb2_int_disable();
  ch32h417_usb2_deinit();
  return true;
}

void dcd_int_enable(uint8_t rhport) { (void)rhport; ch32h417_usb2_int_enable(); }
void dcd_int_disable(uint8_t rhport) { (void)rhport; ch32h417_usb2_int_disable(); }
void dcd_int_handler(uint8_t rhport) { ch32h417_usb2_int_handler(rhport); }

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false); // status ZLP; address applied at status-complete
}

void dcd_remote_wakeup(uint8_t rhport) { (void)rhport; ch32h417_usb2_remote_wakeup(); }
void dcd_connect(uint8_t rhport) { (void)rhport; ch32h417_usb2_connect(); }
void dcd_disconnect(uint8_t rhport) { (void)rhport; ch32h417_usb2_disconnect(); }
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
