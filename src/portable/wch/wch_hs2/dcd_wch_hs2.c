/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// Device driver for WCH's HS2 (second-generation USBHS) USB2.0 high-speed controller:
// per-endpoint DONE flags, a SETUP flag in UEP0_RX_CTRL, split UEP_TX_EN/UEP_RX_EN enables,
// response encoding NAK=00 / STALL=01 / ACK=10 and manual data toggle. Not
// register-compatible with the CH32V30x USBHS (dcd_ch32_usbhs.c). RM below means CH32V407RM.
// Clock and PHY bring-up is the BSP's job; only one controller acts as device at a time.

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_HS2)

#include "device/dcd.h"
#include "device/usbd_pvt.h" // for usbd_spin_lock()
#include "wch_hs2_reg.h"

#if TU_CHECK_MCU(OPT_MCU_CH32V4X7)
  #include "ch32v4x7.h"
static const struct {
  wch_hs2_dev_t *regs;
  IRQn_Type irqn;
} usbhs_controllers[] = {
  {(wch_hs2_dev_t *)USBHS1D_BASE, USBHS1_IRQn},
  {(wch_hs2_dev_t *)USBHS2D_BASE, USBHS2_IRQn},
};
#else
  #error "WCH HS2: MCU not supported"
#endif

#define WCH_HS2_CONTROLLER_COUNT TU_ARRAY_SIZE(usbhs_controllers)

// controller selected by dcd_init(); every other entry point serves this one
static wch_hs2_dev_t *usbhs;

#define EP_TX_LEN(ep)  (usbhs->UEP_CTRL[ep].tx_len)
#define EP_TX_CTRL(ep) (usbhs->UEP_CTRL[ep].tx_ctrl)
#define EP_RX_CTRL(ep) (usbhs->UEP_CTRL[ep].rx_ctrl)
#define EP_MAX_LEN(ep) (usbhs->UEP_MAX_LEN[ep])
#define EP_RX_LEN(ep)  (usbhs->UEP_RX[ep].rx_len)
#define EP_RX_DMA(ep)  (usbhs->UEP_RX_DMA[(ep) - 1])
#define EP_TX_DMA(ep)  (usbhs->UEP_TX_DMA[(ep) - 1])

// Max number of bi-directional endpoints including EP0 (EP0..EP7)
#define EP_MAX TUP_DCD_ENDPOINT_MAX

// UEP0_MAX_LEN and UEP0_RX_LEN are 7-bit (RM 22.2.1.25, 22.2.1.27)
#define EP0_MAX_SIZE CFG_TUD_ENDPOINT0_SIZE
TU_VERIFY_STATIC(EP0_MAX_SIZE == 8 || EP0_MAX_SIZE == 16 || EP0_MAX_SIZE == 32 || EP0_MAX_SIZE == 64,
                 "USB2 EP0 max packet size must be 8, 16, 32 or 64");

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

TU_ATTR_ALIGNED(4) static uint8_t ep0_buffer[EP0_MAX_SIZE];
static volatile bool ep0_tog;    // EP0 expected IN toggle
static volatile bool ep0_rx_tog; // EP0 expected OUT toggle
static volatile bool sof_enabled; // usbd asked for SOF (see the RX_SOF branch in the ISR)
static volatile bool bus_suspended; // last suspend state reported to usbd (see the SUSPEND branch)
static volatile bool bus_reset_pending; // bus reset seen, event held until the speed is known
static volatile bool ep_data_tog[EP_MAX][2];

// Tag bumped by every SETUP and bus reset, and the tag of the SETUP usbd is serving
// (dcd_edpt0_setup_begin()). While they differ, EP0 arms and stalls belong to a superseded request
// and are dropped. uint8_t wrap (256 SETUPs while one is served) is accepted.
static volatile uint8_t setup_rcv_tagid;
static uint8_t setup_served_tagid;

//--------------------------------------------------------------------+
// Endpoint helpers
//--------------------------------------------------------------------+

// Read-modify-write of the response / toggle field. The DONE bit (bit 7) is deliberately written
// back as 1: it is RW0 (RM 22.2.1.31/32/34/35 - "writes 0 to clear it"), so writing 1 cannot set it
// and cannot raise a completion, while writing 0 WOULD acknowledge one. Without the forced 1, a
// completion the SIE raises between this read and its write is silently cleared and never reported,
// hanging that transfer until the host times out. These helpers run in task context and are not
// confined to idle endpoints - edpt_stall()/edpt_clear_stall() call them on an endpoint that may be
// actively armed and ACKing, because a class driver may halt an endpoint with a transfer in flight
// (usbtest case 13 does exactly that).
static void set_tx_res(uint8_t ep_num, uint8_t res) {
  EP_TX_CTRL(ep_num) = (uint8_t)((EP_TX_CTRL(ep_num) & ~WCH_HS2_UEP_T_RES_MASK) | WCH_HS2_UEP_T_DONE | res);
}

static void set_rx_res(uint8_t ep_num, uint8_t res) {
  EP_RX_CTRL(ep_num) = (uint8_t)((EP_RX_CTRL(ep_num) & ~WCH_HS2_UEP_R_RES_MASK) | WCH_HS2_UEP_R_DONE | res);
}

static void set_ep_toggle(uint8_t ep_num, tusb_dir_t ep_dir, bool data1) {
  if (ep_dir == TUSB_DIR_IN) {
    EP_TX_CTRL(ep_num) = (uint8_t)((EP_TX_CTRL(ep_num) & ~WCH_HS2_UEP_T_TOG_MASK) | WCH_HS2_UEP_T_DONE |
                                   (data1 ? WCH_HS2_UEP_T_TOG_DATA1 : WCH_HS2_UEP_T_TOG_DATA0));
  } else {
    // EP1-7 only: EP0 OUT is armed by arm_ep0_out()
    EP_RX_CTRL(ep_num) = (uint8_t)((EP_RX_CTRL(ep_num) & ~WCH_HS2_UEP_R_TOG_MASK) | WCH_HS2_UEP_R_DONE |
                                   (data1 ? WCH_HS2_UEP_R_TOG_DATA1 : WCH_HS2_UEP_R_TOG_DATA0));
  }
}

// Arm EP0 OUT: full-byte write (not a read-modify-write), so the response and the expected toggle
// land without carrying over stale status bits. EP0 has no hardware auto-toggle
// (R16_UEP_R_TOG_AUTO covers EP1..EP7 only), so the expected toggle comes from ep0_rx_tog.
// NOTE: being a full-byte write this clears RB_UEP_R_DONE, so it must never run while an
// unserviced RX completion is pending. Every call site is clear of that: update_out() and the ISR's
// toggle-mismatch drop re-arm for the host's next packet, having just had DONE cleared by the
// scan, and edpt_xfer() runs with EP0 NAKing, since usbd submits a stage only once the previous
// one has completed, so no transaction is in flight to raise DONE underneath it.
static void arm_ep0_out(void) {
  EP_RX_CTRL(0) = (uint8_t)((ep0_rx_tog ? WCH_HS2_UEP_R_TOG_DATA1 : WCH_HS2_UEP_R_TOG_DATA0) | WCH_HS2_UEP_R_RES_ACK);
}

// Also true for a SETUP latched but not yet serviced by the ISR (R_DONE with SETUP_IS, the test the
// ISR uses): an EP0 IN arm would memcpy over it in ep0_buffer. Caller excludes the USB ISR.
static bool ep0_superseded(void) {
  const uint8_t setup_latched = WCH_HS2_UEP_R_DONE | WCH_HS2_UEP_R_SETUP_IS;
  return (setup_served_tagid != setup_rcv_tagid) || ((EP_RX_CTRL(0) & setup_latched) == setup_latched);
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
    EP_TX_CTRL(0) = (uint8_t)(WCH_HS2_UEP_T_RES_ACK | (ep0_tog ? WCH_HS2_UEP_T_TOG_DATA1 : WCH_HS2_UEP_T_TOG_DATA0));
    ep0_tog = !ep0_tog;
  } else if (xfer->is_iso) {
    set_tx_res(ep_num, WCH_HS2_UEP_T_RES_ACK); // isochronous: no handshake
  } else {
    set_ep_toggle(ep_num, TUSB_DIR_IN, ep_data_tog[ep_num][TUSB_DIR_IN]);
    set_tx_res(ep_num, WCH_HS2_UEP_T_RES_ACK);
  }
}

static void queue_out_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t rx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num > 0) {
    EP_RX_DMA(ep_num) = (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
    EP_MAX_LEN(ep_num) = rx_len;
  }

  if (ep_num != 0 && !xfer->is_iso) {
    set_ep_toggle(ep_num, TUSB_DIR_OUT, ep_data_tog[ep_num][TUSB_DIR_OUT]);
  }
  set_rx_res(ep_num, WCH_HS2_UEP_R_RES_ACK);
}

static void update_in(uint8_t rhport, uint8_t ep_num) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
  if (!xfer->valid) {
    return;
  }

  if (ep_num != 0 && !xfer->is_iso) {
    ep_data_tog[ep_num][TUSB_DIR_IN] = !ep_data_tog[ep_num][TUSB_DIR_IN];
  }

  if (xfer->total_len > xfer->queued_len) {
    queue_in_packet(ep_num, xfer);
  } else {
    xfer->valid = false;
    if (ep_num == 0) {
      EP_TX_CTRL(0) = (uint8_t)(WCH_HS2_UEP_T_RES_NAK | (ep0_tog ? WCH_HS2_UEP_T_TOG_DATA1 : WCH_HS2_UEP_T_TOG_DATA0));
    } else {
      set_tx_res(ep_num, WCH_HS2_UEP_T_RES_NAK);
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

  if (ep_num == 0) {
    // Manual-toggle hardware: advance the expected EP0 RX toggle on every received packet. usbd
    // submits a control-OUT data stage one EP0-sized chunk per dcd_edpt_xfer(), so the toggle must
    // survive across chunks (packet 2, 4, ... are DATA0) as well as within a multi-packet stage.
    ep0_rx_tog = !ep0_rx_tog;
  }

  // the next packet's RX DMA address must be 4-byte aligned (CH32V407RM 22.2.1.23), so end the transfer here
  const bool next_rx_dma_unaligned = (ep_num != 0) && ((xfer->queued_len & 3u) != 0);

  if ((xfer->queued_len == xfer->total_len) || (len < xfer->max_size) || next_rx_dma_unaligned) {
    // Leave the endpoint idle before reporting: with TUP_USBD_XFER_ISR a class callback runs inside
    // dcd_event_xfer_complete() and may queue the next transfer, which nothing here may overwrite.
    // RES=NAK is documented as "respond with NAK or busy": with no xfer queued this must reject
    // the packet, not accept it - ACK here would DMA the next OUT packet to EP_RX_DMA's stale
    // target (still pointing at the just-completed buffer, only queue_out_packet() moves it),
    // overwriting the completed transfer before task context delivers it.
    xfer->valid = false;
    set_rx_res(ep_num, WCH_HS2_UEP_R_RES_NAK);
    dcd_event_xfer_complete(rhport, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else if (ep_num == 0) {
    arm_ep0_out();
  } else {
    queue_out_packet(ep_num, xfer);
  }
}


//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  TU_ASSERT(rhport < WCH_HS2_CONTROLLER_COUNT);
  TU_ASSERT(rh_init->speed == TUSB_SPEED_HIGH || rh_init->speed == TUSB_SPEED_FULL ||
            rh_init->speed == TUSB_SPEED_AUTO);
  const bool high_speed = TUD_OPT_HIGH_SPEED && (rh_init->speed != TUSB_SPEED_FULL);
  // high speed requires a 64-byte EP0 (USB 2.0 5.5.3)
  TU_ASSERT(!high_speed || EP0_MAX_SIZE == 64);
  usbhs = usbhs_controllers[rhport].regs;

  memset(&xfer_status, 0, sizeof(xfer_status));
  memset((void *)ep_data_tog, 0, sizeof(ep_data_tog));
  ep0_tog = true;
  ep0_rx_tog = false;
  // Reset every latch this file keeps, not just the toggles: init rewrites INT_EN below without
  // WCH_HS2_UDIE_SOF_ACT, so a sof_enabled left over from a previous life would leave the software
  // latch on with the hardware enable off. The RX_SOF branch is reached on any interrupt that
  // leaves no transfer pending, so it would emit SOF events usbd never asked for - and usbd turns
  // an unexpected SOF into a RESUME, cancelling a genuine suspend. tud_deinit()/tud_init() runs
  // this again, so these flags genuinely survive across it.
  sof_enabled = false;
  bus_suspended = false;
  bus_reset_pending = false;

  usbhs->CONTROL = WCH_HS2_UD_RST_LINK | WCH_HS2_UD_PHY_SUSPENDM;
  // vendor-exact interrupt set: BUS_SLEEP/LPM_ACT are cleared-if-unhandled, LINK_RDY ends a bus reset
  usbhs->INT_EN = WCH_HS2_UDIE_BUS_RST | WCH_HS2_UDIE_SUSPEND | WCH_HS2_UDIE_BUS_SLEEP |
                  WCH_HS2_UDIE_LPM_ACT | WCH_HS2_UDIE_TRANSFER | WCH_HS2_UDIE_LINK_RDY;

  // Only EP0 enabled at reset; data endpoints are enabled by dcd_edpt_open
  usbhs->UEP_TX_EN = 1u;
  usbhs->UEP_RX_EN = 1u;
  usbhs->UEP_T_ISO = 0;
  usbhs->UEP_R_ISO = 0;

  usbhs->UEP0_DMA = (uint32_t)(uintptr_t)ep0_buffer;
  EP_MAX_LEN(0) = EP0_MAX_SIZE;
  xfer_status[0][TUSB_DIR_OUT].max_size = EP0_MAX_SIZE;
  xfer_status[0][TUSB_DIR_IN].max_size = EP0_MAX_SIZE;

  EP_TX_LEN(0) = 0;
  EP_TX_CTRL(0) = WCH_HS2_UEP_T_RES_NAK | WCH_HS2_UEP_T_TOG_DATA0;
  EP_RX_CTRL(0) = WCH_HS2_UEP_R_RES_ACK | WCH_HS2_UEP_R_TOG_DATA0;

  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep) = 0;
    EP_TX_CTRL(ep) = WCH_HS2_UEP_T_RES_NAK | WCH_HS2_UEP_T_TOG_DATA0;
    EP_RX_CTRL(ep) = WCH_HS2_UEP_R_RES_NAK | WCH_HS2_UEP_R_TOG_DATA0;
    EP_MAX_LEN(ep) = 0;
  }

  usbhs->BASE_MODE = high_speed ? WCH_HS2_UD_SPEED_HIGH : WCH_HS2_UD_SPEED_FULL;
  usbhs->DEV_AD = 0;
  usbhs->CONTROL = WCH_HS2_UD_DEV_EN | WCH_HS2_UD_DMA_EN | WCH_HS2_UD_LPM_EN | WCH_HS2_UD_PHY_SUSPENDM;

  return true;
}

bool dcd_deinit(uint8_t rhport) {
  dcd_int_disable(rhport);
  // Mask and acknowledge before resetting the SIE: resetting it does not touch INT_EN or INT_FG,
  // so a latched flag would otherwise survive into the next init.
  usbhs->INT_EN = 0;
  usbhs->INT_FG = 0xFF;
  usbhs->CONTROL = WCH_HS2_UD_RST_SIE | WCH_HS2_UD_RST_LINK;
  return true;
}

void dcd_int_enable(uint8_t rhport) {
  NVIC_EnableIRQ(usbhs_controllers[rhport].irqn);
}

void dcd_int_disable(uint8_t rhport) {
  NVIC_DisableIRQ(usbhs_controllers[rhport].irqn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false); // status ZLP; address applied at status-complete
}

// Resume signalling: RB_UD_REMOTE_WKUP is RW1, the hardware drives K and clears the bit itself
void dcd_remote_wakeup(uint8_t rhport) {
  (void)rhport;
  usbhs->WAKE_CTRL |= WCH_HS2_UD_REMOTE_WKUP;
}

void dcd_connect(uint8_t rhport) {
  (void)rhport;
  usbhs->CONTROL |= WCH_HS2_UD_DEV_EN; // attach: enable the device (pull-up on)
}

void dcd_disconnect(uint8_t rhport) {
  (void)rhport;
  usbhs->CONTROL &= (uint8_t)~WCH_HS2_UD_DEV_EN; // detach: disable the device (pull-up off)
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void)rhport;
  sof_enabled = en;
  if (en) {
    usbhs->INT_EN |= WCH_HS2_UDIE_SOF_ACT;
  } else {
    usbhs->INT_EN &= (uint8_t)~WCH_HS2_UDIE_SOF_ACT;
  }
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;
  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_ADDRESS) {
    // a bus reset since this request left the device at address 0 (USB 2.0 7.1.7.5): keep it there
    usbd_spin_lock(false);
    if (!ep0_superseded()) {
      usbhs->DEV_AD = (uint8_t)request->wValue;
    }
    usbd_spin_unlock(false);
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(desc_edpt->bEndpointAddress);
  const tusb_dir_t dir = tu_edpt_dir(desc_edpt->bEndpointAddress);
  TU_ASSERT(ep_num < EP_MAX);

  if (ep_num == 0) {
    return true;
  }

  // start clean: a transfer re-armed from a pre-reset completion callback must not survive the reopen
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  tu_memclr(xfer, sizeof(xfer_ctl_t));
  xfer->max_size = tu_edpt_packet_size(desc_edpt);
  xfer->is_iso = (desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  ep_data_tog[ep_num][dir] = false;

  if (dir == TUSB_DIR_OUT) {
    usbhs->UEP_RX_EN |= (uint16_t)(1u << ep_num);
    if (xfer->is_iso) {
      usbhs->UEP_R_ISO |= (uint16_t)(1u << ep_num);
    }
    EP_MAX_LEN(ep_num) = xfer->max_size;
    EP_RX_CTRL(ep_num) = WCH_HS2_UEP_R_RES_NAK | WCH_HS2_UEP_R_TOG_DATA0;
  } else {
    usbhs->UEP_TX_EN |= (uint16_t)(1u << ep_num);
    if (xfer->is_iso) {
      usbhs->UEP_T_ISO |= (uint16_t)(1u << ep_num);
    }
    EP_TX_LEN(ep_num) = 0;
    EP_TX_CTRL(ep_num) = WCH_HS2_UEP_T_RES_NAK | WCH_HS2_UEP_T_TOG_DATA0;
  }

  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  TU_ASSERT(ep_num < EP_MAX, );

  if (ep_num == 0) {
    return; // EP0 is never closed; its registers stay armed for the next control transfer
  }

  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = WCH_HS2_UEP_R_RES_NAK | WCH_HS2_UEP_R_TOG_DATA0;
    EP_MAX_LEN(ep_num) = 0;
    usbhs->UEP_R_ISO &= (uint16_t)~(1u << ep_num);
    usbhs->UEP_RX_EN &= (uint16_t)~(1u << ep_num);
  } else {
    EP_TX_CTRL(ep_num) = WCH_HS2_UEP_T_RES_NAK | WCH_HS2_UEP_T_TOG_DATA0;
    EP_TX_LEN(ep_num) = 0;
    usbhs->UEP_T_ISO &= (uint16_t)~(1u << ep_num);
    usbhs->UEP_TX_EN &= (uint16_t)~(1u << ep_num);
  }
  ep_data_tog[ep_num][dir] = false;
  XFER_CTL_BASE(ep_num, dir)->valid = false;
}

void dcd_edpt_close_all(uint8_t rhport) {
  (void)rhport;
  memset((void *)ep_data_tog, 0, sizeof(ep_data_tog));
  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep) = 0;
    EP_TX_CTRL(ep) = WCH_HS2_UEP_T_RES_NAK | WCH_HS2_UEP_T_TOG_DATA0;
    EP_RX_CTRL(ep) = WCH_HS2_UEP_R_RES_NAK | WCH_HS2_UEP_R_TOG_DATA0;
    EP_MAX_LEN(ep) = 0;
    xfer_status[ep][0].valid = false;
    xfer_status[ep][1].valid = false;
  }
  usbhs->UEP_TX_EN = 1u;
  usbhs->UEP_RX_EN = 1u;
  usbhs->UEP_T_ISO = 0;
  usbhs->UEP_R_ISO = 0;
}

bool dcd_edpt0_setup_begin(uint8_t rhport, uint8_t setup_tagid) {
  (void)rhport;
  setup_served_tagid = setup_tagid;
  return !ep0_superseded();
}

static void edpt_xfer_start(uint8_t ep_num, tusb_dir_t dir, uint8_t *buffer, uint16_t total_bytes) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  xfer->valid = true;

  if (ep_num == 0 && dir == TUSB_DIR_OUT) {
    // Data or status OUT stage: ep0_rx_tog holds DATA1 right after a SETUP and is advanced per
    // received packet, so a multi-chunk control-write stays synchronized with the host.
    arm_ep0_out();
  } else if (dir == TUSB_DIR_IN) {
    queue_in_packet(ep_num, xfer);
  } else {
    queue_out_packet(ep_num, xfer);
  }
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  if (ep_num == 0) {
    // the SETUP ISR must not land between the check and the arm
    usbd_spin_lock(is_isr);
    if (!ep0_superseded()) {
      edpt_xfer_start(ep_num, dir, buffer, total_bytes);
    }
    // a superseded stage is dropped, not refused: the newer SETUP resets EP0 in usbd
    usbd_spin_unlock(is_isr);
  } else {
    // UEPn_TX_DMA/RX_DMA must be 4-byte aligned (CH32V407RM 22.2.1.23-24), at every packet start
    TU_ASSERT(((uintptr_t)buffer & 3u) == 0);
    // IN would program buffer+mps after a full first packet; OUT ends early there instead (update_out)
    const uint16_t mps = XFER_CTL_BASE(ep_num, dir)->max_size;
    TU_VERIFY(dir == TUSB_DIR_OUT || total_bytes <= mps || (mps & 3u) == 0);
    edpt_xfer_start(ep_num, dir, buffer, total_bytes);
  }

  return true;
}

// A halt cancels the transfer, as usbd_edpt_abort() relies on: invalidate it before stalling so the
// ISR cannot re-arm the endpoint with ACK over the STALL, and clear-halt cannot resume it.
static void edpt_halt(uint8_t ep_num, tusb_dir_t dir) {
  XFER_CTL_BASE(ep_num, dir)->valid = false;
  if (dir == TUSB_DIR_OUT) {
    set_rx_res(ep_num, WCH_HS2_UEP_R_RES_STALL);
  } else {
    EP_TX_LEN(ep_num) = 0;
    set_tx_res(ep_num, WCH_HS2_UEP_T_RES_STALL);
  }
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  TU_ASSERT(ep_num < EP_MAX, );

  if (ep_num == 0) {
    // a stall for an abandoned control transfer must not stall the newer one
    usbd_spin_lock(false);
    if (!ep0_superseded()) {
      edpt_halt(ep_num, dir);
    }
    usbd_spin_unlock(false);
  } else {
    edpt_halt(ep_num, dir);
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  TU_ASSERT(ep_num < EP_MAX, );

  // a completion ISR between the valid check and the re-arm would re-arm a finished transfer with ACK
  usbd_spin_lock(false);
  if (dir == TUSB_DIR_OUT) {
    ep_data_tog[ep_num][TUSB_DIR_OUT] = false; // clear-halt resets the toggle to DATA0
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
    if (xfer->valid) {
      // Not halted (a halt cancels the transfer): a toggle reset on an armed pipe, as usbtest case
      // 29 issues on bulk-OUT. Re-arm the receive with DATA0.
      queue_out_packet(ep_num, xfer);
    } else {
      EP_RX_CTRL(ep_num) = WCH_HS2_UEP_R_RES_NAK | WCH_HS2_UEP_R_TOG_DATA0;
    }
  } else {
    ep_data_tog[ep_num][TUSB_DIR_IN] = false; // clear-halt resets the toggle to DATA0
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
    if (xfer->valid) {
      // Not halted (a halt cancels the transfer): a toggle reset on an armed pipe. queued_len
      // tracks ARMED bytes: rewind only the armed packet and re-arm it with DATA0, so packets the
      // host already ACKed are not retransmitted.
      uint16_t rearm = (uint16_t)(xfer->queued_len % xfer->max_size);
      if (rearm == 0 && xfer->queued_len != 0) {
        rearm = xfer->max_size;
      }
      xfer->queued_len -= rearm;
      queue_in_packet(ep_num, xfer);
    } else {
      EP_TX_CTRL(ep_num) = WCH_HS2_UEP_T_RES_NAK | WCH_HS2_UEP_T_TOG_DATA0;
    }
  }
  usbd_spin_unlock(false);
}


//--------------------------------------------------------------------+
// Interrupt handler
//--------------------------------------------------------------------+

// MIS_ST HS_MOD is only valid once the reset handshake has finished: at BUS_RST it reads 0, or still
// 1 from the previous link (measured on CH32V407). BUS_RST reports the reset start; its end, with
// the speed, is LINK_RDY with READY set, at least TRSTRCY (10 ms) before the host's first SETUP.
static void report_bus_reset_end(uint8_t rhport) {
  bus_reset_pending = false;
  const bool high_speed = (usbhs->MIS_ST & WCH_HS2_UDMS_HS_MOD) != 0;
  dcd_event_bus_reset(rhport, high_speed ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL, true);
}

void dcd_int_handler(uint8_t rhport) {
  const uint8_t intflag = usbhs->INT_FG;

  if (intflag & WCH_HS2_UDIF_TRANSFER) {
    // RB_UDIF_RTX_ACT (INT_FG bit 4) is read-only and is the wired-OR of every endpoint's DONE
    // flag - RM 22.2.1.8: "Receive cleared by RB_UEP_R_DONE; Transmit cleared by RB_UEP_T_DONE" -
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
      if (EP_RX_CTRL(ep_num) & WCH_HS2_UEP_R_DONE) {
        // SETUP or OUT transaction.
        // RB_UEP_R_SETUP_IS (RM 22.2.1.32) is read-only and survives a software write, but the
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
        if (ep_num == 0 && (EP_RX_CTRL(0) & WCH_HS2_UEP_R_SETUP_IS)) {
          tusb_control_request_t const *setup = (tusb_control_request_t const *)ep0_buffer;
          if (bus_reset_pending) {
            // LINK_RDY missed: report the speed late. usbd discards a SETUP queued together with the
            // reset end, so this request stalls until the host times it out; later ones see the speed.
            report_bus_reset_end(rhport);
          }
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
          // A finished data-stage chunk is still delivered: the next chunk or status stage usbd
          // arms for it is dropped by the tag bumped below, so it cannot disturb ep0_tog.
          const xfer_ctl_t *ep0_in = XFER_CTL_BASE(0, TUSB_DIR_IN);
          if ((EP_TX_CTRL(0) & WCH_HS2_UEP_T_DONE) && ep0_in->valid &&
              (ep0_in->total_len <= ep0_in->queued_len)) {
            EP_TX_CTRL(0) &= (uint8_t)~WCH_HS2_UEP_T_DONE;
            update_in(rhport, 0);
          }
          setup_rcv_tagid++;
          ep0_tog = true;
          ep0_rx_tog = true;
          // A SETUP resets the control pipe: drop any stage still armed from a transfer the host
          // just abandoned, so it cannot later swallow a packet belonging to this new one.
          xfer_status[0][TUSB_DIR_OUT].valid = false;
          xfer_status[0][TUSB_DIR_IN].valid = false;
          // Full-byte writes (DATA1) clear DONE and set the post-SETUP toggle; SETUP_IS survives them.
          EP_TX_CTRL(0) = WCH_HS2_UEP_T_RES_NAK | WCH_HS2_UEP_T_TOG_DATA1;
          EP_RX_CTRL(0) = (uint8_t)(((setup->wLength == 0) ? WCH_HS2_UEP_R_RES_ACK : WCH_HS2_UEP_R_RES_NAK) |
                                    WCH_HS2_UEP_R_TOG_DATA1);
          dcd_event_setup_received_tagid(rhport, ep0_buffer, setup_rcv_tagid, true);
        } else {
          // A packet whose PID does not match the endpoint's expected toggle is a host retransmission
          // after a lost ACK: the SIE stores it and raises this interrupt anyway. RM 22.2.1.32/35
          // RB_UEP_R_TOG_MATCH (RO, bit 4 of UEPx_RX_CTRL): 1 = synchronized, 0 = not synchronized -
          // consuming an unsynchronized packet would inject it into the transfer twice, so it is
          // dropped. The SIE ACKs it but, as after any received packet, sets RES to NAK (measured:
          // RX_CTRL 0x80 on EP0, 0xC0 on bulk OUT; the RM is silent), so the endpoint is re-armed
          // unchanged for the host's next packet; left NAKing, the transfer stalls until the host
          // times out. Iso endpoints run without handshake or toggle.
          const uint8_t rx_ctrl = EP_RX_CTRL(ep_num);
          EP_RX_CTRL(ep_num) = (uint8_t)(rx_ctrl & ~WCH_HS2_UEP_R_DONE);
          if (xfer_status[ep_num][TUSB_DIR_OUT].is_iso || (rx_ctrl & WCH_HS2_UEP_R_TOG_MATCH)) {
            update_out(rhport, ep_num, EP_RX_LEN(ep_num));
          } else if (xfer_status[ep_num][TUSB_DIR_OUT].valid) {
            if (ep_num == 0) {
              arm_ep0_out();
            } else {
              queue_out_packet(ep_num, XFER_CTL_BASE(ep_num, TUSB_DIR_OUT));
            }
          }
        }
      }

      if (EP_TX_CTRL(ep_num) & WCH_HS2_UEP_T_DONE) {
        // IN transaction completed
        EP_TX_CTRL(ep_num) &= (uint8_t)~WCH_HS2_UEP_T_DONE;
        update_in(rhport, ep_num);
      }
    }
    // The transfer interrupt is acknowledged by clearing the per-endpoint DONE bits above (matching
    // the vendor driver); do NOT also write INT_FG - RB_UDIF_RTX_ACT is read-only anyway.
  } else if (intflag & WCH_HS2_UDIF_RX_SOF) {
    // The hardware sets RB_UDIF_RX_SOF every frame whether or not the SOF interrupt is enabled, so
    // this branch is reached on ANY interrupt that leaves no transfer pending. Emitting a SOF event
    // unasked flooded usbd's event queue within one enumeration (queue_event asserts, events lost,
    // the host sees the device drop) - and because the branch also short-circuits the chain, the
    // terminal else never ran and an enabled-but-unhandled flag kept the IRQ line asserted, so the
    // ISR re-entered immediately. Always clear the flag; only report it when usbd asked for it.
    usbhs->INT_FG = WCH_HS2_UDIF_RX_SOF;
    if (sof_enabled) {
      dcd_event_sof(rhport, usbhs->FRAME_NO & WCH_HS2_UD_FRAME_NO, true);
    }
  } else if (intflag & WCH_HS2_UDIF_BUS_RST) {
    bus_suspended = false;
    bus_reset_pending = true;
    setup_rcv_tagid++; // a reset supersedes any control transfer usbd is still on
    usbhs->DEV_AD = 0;
    // usbd closes endpoints only on a configuration change: drop every transfer the reset aborted,
    // or a later clear-halt would re-arm a pre-reset buffer
    dcd_edpt_close_all(rhport);
    xfer_status[0][TUSB_DIR_OUT].valid = false;
    xfer_status[0][TUSB_DIR_IN].valid = false;
    dcd_sof_enable(rhport, false);
    ep0_tog = true;
    ep0_rx_tog = false;
    EP_RX_CTRL(0) = WCH_HS2_UEP_R_RES_ACK | WCH_HS2_UEP_R_TOG_DATA0;
    EP_TX_CTRL(0) = WCH_HS2_UEP_T_RES_NAK | WCH_HS2_UEP_T_TOG_DATA0;
    usbhs->INT_FG = WCH_HS2_UDIF_BUS_RST;
    dcd_event_bus_signal(rhport, DCD_EVENT_BUS_RESET_START, true);
  } else if (intflag & WCH_HS2_UDIF_SUSPEND) {
    // This single interrupt fires on both suspend and resume; the live suspend status bit
    // distinguishes them. Clear first, then sample, like the vendor demo (CH372Device).
    // EDGE-TRIGGERED: the controller raises this constantly (measured on hardware: ~2000 times
    // during one enumeration), and queueing a usbd event each time overflows the event queue -
    // events are dropped, enumeration dies and the host sees the device disappear. usbd only
    // needs the transitions, so report a change of state and swallow the repeats.
    usbhs->INT_FG = WCH_HS2_UDIF_SUSPEND;
    const bool now_suspended = (usbhs->MIS_ST & WCH_HS2_UDMS_SUSPEND) != 0;
    if (now_suspended != bus_suspended) {
      bus_suspended = now_suspended;
      dcd_event_bus_signal(rhport, now_suspended ? DCD_EVENT_SUSPEND : DCD_EVENT_RESUME, true);
    }
  } else if (intflag & WCH_HS2_UDIF_LINK_RDY) {
    usbhs->INT_FG = WCH_HS2_UDIF_LINK_RDY;
    if (bus_reset_pending && (usbhs->MIS_ST & WCH_HS2_UDMS_READY)) {
      report_bus_reset_end(rhport);
    }
  } else {
    usbhs->INT_FG = intflag;
  }
}

#endif
