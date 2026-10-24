/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Mitsumine Suzu (verylowfreq)
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ha Thach (tinyusb.org)
 * SPDX-FileCopyrightText: Copyright (c) 2026 Robert Dale Smith
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/*
 * Asynchronous HCD for the WCH USBFS host controller (CH32V20x, CH32V30x).
 *
 * The controller has a single transfer engine: one DEV_ADDR latch, one HOST_EP_PID token
 * register, one toggle pair and one RX/TX DMA buffer pair. The driver keeps a flat ep[]
 * table keyed by (daddr, ep_num, dir) and schedules it round-robin from the interrupt
 * handler, with at most one transaction on the wire, as hcd_max3421.c does. This lets a hub
 * with several downstream devices share the engine.
 *
 * Every transaction completes from the transfer-done interrupt and nothing blocks the
 * caller. A NAK, or no response, reschedules the endpoint for its next service frame: the
 * next frame for control and bulk, bInterval frames later for interrupt endpoints.
 *
 * An isochronous endpoint runs one transaction per service interval, without handshake and
 * always DATA0, and one packet per transfer. It is served before any other endpoint, also
 * between the packets of a longer transfer, so that it is not pushed out of its frame.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_WCH_USBFS) && defined(CFG_TUH_WCH_USBIP_USBFS) && CFG_TUH_WCH_USBIP_USBFS

#include "ch32_usbfs_reg.h"

#if CFG_TUSB_MCU == OPT_MCU_CH32V20X
  #include <ch32v20x_usb.h>
#elif CFG_TUSB_MCU == OPT_MCU_CH32V307
  #include <ch32v30x_usb.h>
#else
  #error "USBFS host is not supported on this MCU"
#endif

#include "host/hcd.h"
#include "host/usbh.h"

//--------------------------------------------------------------------+
// Config
//--------------------------------------------------------------------+

// ep[0] is the default control pipe of address 0, the rest are allocated on open.
#ifndef CFG_TUH_WCH_USBFS_ENDPOINT_MAX
  #define CFG_TUH_WCH_USBFS_ENDPOINT_MAX 16
#endif

// Bound for every wait on the SIE
#define SIE_WAIT_TIMEOUT 2000000u

//--------------------------------------------------------------------+
// Data
//--------------------------------------------------------------------+

typedef struct {
  TU_ATTR_ALIGNED(4) uint8_t setup[8]; // DMA source of the SETUP packet

  uint8_t daddr;
  uint8_t ep_num   : 4;
  uint8_t is_out   : 1; // current direction, also SETUP
  uint8_t is_setup : 1; // current token is SETUP
  uint8_t pending  : 1; // transfer queued
  uint8_t toggle   : 1; // DATA0/DATA1

  uint8_t xfer_type : 2;
  uint8_t bounce    : 1; // current IN packet lands in bounce_buf
  uint8_t  interval;    // service interval in frames
  uint16_t packet_size; // 0 = free slot
  uint16_t next_frame;  // earliest frame for the next attempt

  uint16_t total_len;
  uint16_t xferred_len;
  uint8_t *buf;
} hcd_ep_t;

typedef struct {
  volatile uint16_t frame_count;
  int8_t   cur_idx;        // ep[] index on the wire, -1 = engine idle
  int8_t   last_idx;       // last started ep[] index, for round-robin
  bool     attached;       // root port connection reported to usbh
  bool     in_reset;       // root port bus reset in progress
  tusb_speed_t root_speed;

  hcd_ep_t ep[CFG_TUH_WCH_USBFS_ENDPOINT_MAX];
} hcd_data_t;

static hcd_data_t _hcd;

// The DMA works on the transfer buffer directly where it can: the address must be 4-byte
// aligned within the first 32 KB of SRAM (15-bit DMA address). An IN has no receive limit,
// the SIE writes the whole packet whatever its length (a 392-byte packet was seen to land in
// full), so it needs a max packet of room. Other packets go through this buffer, shared by
// both directions since only one transaction is ever on the wire.
TU_ATTR_ALIGNED(4) static uint8_t bounce_buf[MAX_PACKET_SIZE];

enum {
  XACT_OK = 0,
  XACT_NAK,
  XACT_STALL,
  XACT_TIMEOUT,
  XACT_ERROR
};

//--------------------------------------------------------------------+
// ep[] helpers
//--------------------------------------------------------------------+

static hcd_ep_t *ep_find(uint8_t daddr, uint8_t ep_num, uint8_t dir) {
  if (daddr == 0 && ep_num == 0) {
    return &_hcd.ep[0];
  }
  for (uint8_t i = 1; i < CFG_TUH_WCH_USBFS_ENDPOINT_MAX; i++) {
    hcd_ep_t *ep = &_hcd.ep[i];
    // control endpoint is bidirectional
    if (ep->packet_size != 0 && ep->daddr == daddr && ep->ep_num == ep_num &&
        (ep_num == 0 || ep->is_out == (dir == TUSB_DIR_OUT))) {
      return ep;
    }
  }
  return NULL;
}

static hcd_ep_t *ep_alloc(void) {
  for (uint8_t i = 1; i < CFG_TUH_WCH_USBFS_ENDPOINT_MAX; i++) {
    if (_hcd.ep[i].packet_size == 0) {
      return &_hcd.ep[i];
    }
  }
  return NULL;
}

static inline bool ep_is_ready(const hcd_ep_t *ep) {
  return ep->packet_size != 0 && ep->pending && (int16_t)(_hcd.frame_count - ep->next_frame) >= 0;
}

static int8_t ep_find_ready_iso(void) {
  for (uint8_t i = 1; i < CFG_TUH_WCH_USBFS_ENDPOINT_MAX; i++) {
    if (_hcd.ep[i].xfer_type == TUSB_XFER_ISOCHRONOUS && ep_is_ready(&_hcd.ep[i])) {
      return (int8_t)i;
    }
  }
  return -1;
}

// isochronous endpoints first, then a round-robin scan starting after cur so that no
// endpoint monopolizes the engine
static int8_t ep_find_next_ready(int8_t cur) {
  int8_t const iso = ep_find_ready_iso();
  if (iso >= 0) {
    return iso;
  }
  for (uint8_t k = 1; k <= CFG_TUH_WCH_USBFS_ENDPOINT_MAX; k++) {
    int8_t i = (int8_t)(((int)cur + k) % CFG_TUH_WCH_USBFS_ENDPOINT_MAX);
    if (ep_is_ready(&_hcd.ep[i])) {
      return i;
    }
  }
  return -1;
}

//--------------------------------------------------------------------+
// Transaction
//--------------------------------------------------------------------+

static inline bool dma_capable(const uint8_t *addr) {
  uintptr_t const a = (uintptr_t)addr;
  return (a & 3u) == 0 && (a & ~(uintptr_t)0x7FFFu) == 0x20000000u;
}

static inline void sie_wait_idle(void) {
  uint32_t timeout = SIE_WAIT_TIMEOUT;
  while (!(USBFSH->MIS_ST & USBFS_UMS_SIE_FREE) && --timeout) {}
}

// With INT_BUSY the SIE holds off while UIF_TRANSFER is set, and clearing the flag lets it
// run the token in HOST_EP_PID. So a token is always written first and the flag cleared
// after it, and the token is zeroed before the flag of a finished transaction is cleared.
static inline void engine_stop(void) {
  USBFSH->HOST_EP_PID = 0;
  USBFSH->INT_FG = USBFS_UIF_TRANSFER;
  _hcd.cur_idx = -1;
}

// Select device address and signalling: a low speed device on the root port uses low
// speed signalling, one behind a full speed hub needs PRE with full speed tokens.
static void select_device(uint8_t daddr) {
  USBFSH->DEV_ADDR = (uint8_t)((USBFSH->DEV_ADDR & USBFS_UDA_GP_BIT) | (daddr & USBFS_USB_ADDR_MASK));

  tuh_bus_info_t bus_info;
  tuh_bus_info_get(daddr, &bus_info);

  bool const is_low = (bus_info.speed == TUSB_SPEED_LOW);
  bool const via_hub = (bus_info.hub_addr != 0);

  if (is_low) {
    USBFSH->BASE_CTRL |= USBFS_UC_LOW_SPEED;
  } else {
    USBFSH->BASE_CTRL &= (uint8_t)~USBFS_UC_LOW_SPEED;
  }

  if (is_low && !via_hub) {
    USBFSH->HOST_CTRL |= USBFS_UH_LOW_SPEED;
  } else {
    USBFSH->HOST_CTRL &= (uint8_t)~USBFS_UH_LOW_SPEED;
  }

  if (is_low && via_hub) {
    USBFSH->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
  } else {
    USBFSH->HOST_SETUP &= (uint16_t)~USBFS_UH_PRE_PID_EN;
  }
}

static void xact_start(int8_t idx, bool new_ep) {
  hcd_ep_t *ep = &_hcd.ep[idx];
  _hcd.cur_idx  = idx;
  _hcd.last_idx = idx;

  if (new_ep) {
    select_device(ep->daddr);
  }

  sie_wait_idle();

  bool const is_iso = (ep->xfer_type == TUSB_XFER_ISOCHRONOUS);
  uint8_t pid;
  uint8_t tog = ep->toggle ? USBFS_UH_T_TOG : 0; // same bit position as USBFS_UH_R_TOG
  uint8_t *const cursor = ep->buf + ep->xferred_len;
  uint16_t const remain = ep->total_len - ep->xferred_len;

  if (ep->is_setup) {
    USBFSH->HOST_TX_DMA = (uint32_t)ep->setup;
    USBFSH->HOST_TX_LEN = 8;
    pid = USB_PID_SETUP;
    tog = 0;
  } else if (ep->is_out) {
    uint16_t const len = tu_min16(remain, ep->packet_size);
    if (len == 0 || dma_capable(cursor)) {
      USBFSH->HOST_TX_DMA = (uint32_t)(len ? cursor : bounce_buf);
    } else {
      memcpy(bounce_buf, cursor, len);
      USBFSH->HOST_TX_DMA = (uint32_t)bounce_buf;
    }
    USBFSH->HOST_TX_LEN = len;
    pid = USB_PID_OUT;
  } else {
    ep->bounce = !(remain >= ep->packet_size && dma_capable(cursor));
    USBFSH->HOST_RX_DMA = (uint32_t)(ep->bounce ? bounce_buf : cursor);
    USBFSH->HOST_TX_LEN = 0;
    pid = USB_PID_IN;
  }

  if (is_iso) {
    // DATA0, no handshake (same bit position as USBFS_UH_R_RES)
    USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = USBFS_UH_T_RES;
  } else {
    USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = (uint8_t)(USBFS_UH_T_AUTO_TOG | USBFS_UH_R_AUTO_TOG | tog);
  }
  USBFSH->INT_FG       = USBFS_UIF_TRANSFER;
  USBFSH->HOST_EP_PID  = (uint8_t)((pid << 4) | ep->ep_num);
}

// Start the next ready endpoint if the engine is idle. Call with the interrupt masked or
// from the interrupt handler.
static void engine_kick(void) {
  if (_hcd.cur_idx >= 0 || !_hcd.attached || _hcd.in_reset) {
    return;
  }
  int8_t const idx = ep_find_next_ready(_hcd.last_idx);
  if (idx >= 0) {
    xact_start(idx, true);
  }
}

static void engine_next(int8_t cur) {
  int8_t const idx = ep_find_next_ready(cur);
  if (idx >= 0) {
    xact_start(idx, true);
  } else {
    engine_stop();
  }
}

// next packet of a multi-packet transfer, unless an isochronous endpoint is due
static void xact_continue(int8_t idx) {
  int8_t const iso = ep_find_ready_iso();
  if (iso >= 0 && iso != idx) {
    xact_start(iso, true);
  } else {
    xact_start(idx, false);
  }
}

static uint8_t xact_result(void) {
  uint8_t const st = USBFSH->INT_ST;
  if (st & USBFS_UIS_TOG_OK) {
    return XACT_OK;
  }
  switch (st & USBFS_UIS_H_RES_MASK) {
    case USB_PID_NAK:   return XACT_NAK;
    case USB_PID_STALL: return XACT_STALL;
    case USB_PID_NULL:  return XACT_TIMEOUT;
    default:            return XACT_ERROR;
  }
}

static void xfer_complete(int8_t idx, xfer_result_t result, bool in_isr) {
  hcd_ep_t *ep = &_hcd.ep[idx];
  uint8_t const ep_addr = tu_edpt_addr(ep->ep_num, ep->is_out ? TUSB_DIR_OUT : TUSB_DIR_IN);

  ep->pending  = 0;
  ep->is_setup = 0;
  // the next transfer of a periodic endpoint waits for its interval
  bool const periodic = (ep->xfer_type == TUSB_XFER_INTERRUPT || ep->xfer_type == TUSB_XFER_ISOCHRONOUS);
  ep->next_frame = (uint16_t)(_hcd.frame_count + (periodic ? ep->interval : 0));

  hcd_event_xfer_complete(ep->daddr, ep_addr, ep->xferred_len, result, in_isr);
  engine_next(idx);
}

static void handle_iso_done(int8_t idx, bool in_isr) {
  hcd_ep_t *ep = &_hcd.ep[idx];

  if (ep->is_out) {
    ep->xferred_len = ep->total_len;
    xfer_complete(idx, XFER_RESULT_SUCCESS, in_isr);
    return;
  }

  uint8_t const res = USBFSH->INT_ST & USBFS_UIS_H_RES_MASK;
  uint16_t const rx_len = (uint16_t)USBFSH->RX_LEN;
  if ((res != USB_PID_DATA0 && res != USB_PID_DATA1) || rx_len > ep->packet_size) {
    xfer_complete(idx, XFER_RESULT_FAILED, in_isr); // no data or babble
    return;
  }

  uint16_t const len = tu_min16(rx_len, ep->total_len);
  if (ep->bounce && len > 0) {
    memcpy(ep->buf, bounce_buf, len);
  }
  ep->xferred_len = len;
  xfer_complete(idx, XFER_RESULT_SUCCESS, in_isr);
}

static void handle_xfer_done(bool in_isr) {
  int8_t const idx = _hcd.cur_idx;
  USBFSH->HOST_EP_PID = 0;
  if (idx < 0) {
    USBFSH->INT_FG = USBFS_UIF_TRANSFER;
    return;
  }

  hcd_ep_t *ep = &_hcd.ep[idx];
  if (ep->xfer_type == TUSB_XFER_ISOCHRONOUS) {
    handle_iso_done(idx, in_isr);
    return;
  }

  switch (xact_result()) {
    case XACT_OK:
      if (ep->is_setup) {
        ep->xferred_len = 8;
        xfer_complete(idx, XFER_RESULT_SUCCESS, in_isr);
      } else if (ep->is_out) {
        ep->xferred_len += tu_min16(ep->total_len - ep->xferred_len, ep->packet_size);
        ep->toggle = (USBFSH->HOST_TX_CTRL & USBFS_UH_T_TOG) ? 1 : 0;
        if (ep->xferred_len >= ep->total_len) {
          xfer_complete(idx, XFER_RESULT_SUCCESS, in_isr);
        } else {
          xact_continue(idx);
        }
      } else {
        uint16_t const rx_len = (uint16_t)USBFSH->RX_LEN;
        if (rx_len > ep->packet_size) {
          xfer_complete(idx, XFER_RESULT_FAILED, in_isr); // babble
          break;
        }
        uint16_t const len = tu_min16(rx_len, ep->total_len - ep->xferred_len);
        if (ep->bounce && len > 0) {
          memcpy(ep->buf + ep->xferred_len, bounce_buf, len);
        }
        ep->xferred_len += len;
        ep->toggle = (USBFSH->HOST_RX_CTRL & USBFS_UH_R_TOG) ? 1 : 0;
        if (len < ep->packet_size || ep->xferred_len >= ep->total_len) {
          xfer_complete(idx, XFER_RESULT_SUCCESS, in_isr);
        } else {
          xact_continue(idx);
        }
      }
      break;

    case XACT_NAK:
    case XACT_TIMEOUT:
      // retry in the next service frame, usbh times out a control transfer that never ends
      ep->next_frame = (uint16_t)(_hcd.frame_count + ep->interval);
      engine_next(idx);
      break;

    case XACT_STALL:
      xfer_complete(idx, XFER_RESULT_STALLED, in_isr);
      break;

    default:
      xfer_complete(idx, XFER_RESULT_FAILED, in_isr);
      break;
  }
}

//--------------------------------------------------------------------+
// Root port
//--------------------------------------------------------------------+

// Report root port connection changes from the line state: the DETECT edge, and the SOF
// tick for a device that is already present at power-up.
static void root_port_check(uint8_t rhport, bool in_isr) {
  if (_hcd.in_reset) {
    return;
  }

  bool const present = (USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) != 0;
  if (present && !_hcd.attached) {
    _hcd.attached = true;
    hcd_event_device_attach(rhport, in_isr);
  } else if (!present && _hcd.attached) {
    _hcd.attached = false;
    engine_stop();
    for (uint8_t i = 0; i < CFG_TUH_WCH_USBFS_ENDPOINT_MAX; i++) {
      _hcd.ep[i].pending = 0;
    }
    hcd_event_device_remove(rhport, in_isr);
  }
}

static void set_root_low_speed(bool low_speed) {
  if (low_speed) {
    USBFSH->BASE_CTRL |= USBFS_UC_LOW_SPEED;
    USBFSH->HOST_CTRL |= USBFS_UH_LOW_SPEED;
  } else {
    USBFSH->BASE_CTRL &= (uint8_t)~USBFS_UC_LOW_SPEED;
    USBFSH->HOST_CTRL &= (uint8_t)~USBFS_UH_LOW_SPEED;
  }
  USBFSH->HOST_SETUP &= (uint16_t)~USBFS_UH_PRE_PID_EN;
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void)rhport;
  return (USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) != 0;
}

void hcd_port_reset(uint8_t rhport) {
  hcd_int_disable(rhport);
  _hcd.in_reset = true;
  engine_stop();

  USBFSH->DEV_ADDR = (uint8_t)(USBFSH->DEV_ADDR & USBFS_UDA_GP_BIT);
  set_root_low_speed(false);
  USBFSH->HOST_CTRL = (uint8_t)((USBFSH->HOST_CTRL & ~USBFS_UH_PORT_EN) | USBFS_UH_BUS_RESET);
  hcd_int_enable(rhport);
}

void hcd_port_reset_end(uint8_t rhport) {
  USBFSH->HOST_CTRL &= (uint8_t)~USBFS_UH_BUS_RESET;
  tusb_time_delay_ms_api(1); // let the line settle before reading the speed

  hcd_int_disable(rhport);
  if (USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) {
    // D- pulled up in idle: low speed device
    bool const is_low = (USBFSH->MIS_ST & USBFS_UMS_DM_LEVEL) != 0;
    _hcd.root_speed = is_low ? TUSB_SPEED_LOW : TUSB_SPEED_FULL;
    set_root_low_speed(is_low);
    USBFSH->HOST_CTRL |= USBFS_UH_PORT_EN;
    USBFSH->HOST_SETUP |= USBFS_UH_SOF_EN;
  }

  // drop the DETECT edge of the reset itself, root_port_check() reads the line state
  USBFSH->INT_FG = USBFS_UIF_DETECT;
  _hcd.in_reset = false;
  root_port_check(rhport, false);
  hcd_int_enable(rhport);
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void)rhport;
  return _hcd.root_speed;
}

void hcd_device_close(uint8_t rhport, uint8_t daddr) {
  hcd_int_disable(rhport);
  if (_hcd.cur_idx >= 0 && _hcd.ep[_hcd.cur_idx].daddr == daddr) {
    engine_stop();
  }
  for (uint8_t i = 0; i < CFG_TUH_WCH_USBFS_ENDPOINT_MAX; i++) {
    hcd_ep_t *ep = &_hcd.ep[i];
    if (ep->packet_size != 0 && ep->daddr == daddr) {
      if (i == 0) {
        ep->pending = 0; // keep the address 0 control pipe
      } else {
        tu_memclr(ep, sizeof(hcd_ep_t));
      }
    }
  }
  engine_kick();
  hcd_int_enable(rhport);
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rh_init;
  TU_ASSERT(dma_capable(bounce_buf));
  hcd_int_disable(rhport);

  tu_memclr(&_hcd, sizeof(_hcd));
  _hcd.cur_idx    = -1;
  _hcd.root_speed = TUSB_SPEED_FULL;

  // USB clock is set up by board_init()
  USBFSH->BASE_CTRL = USBFS_CTRL_RESET_SIE | USBFS_CTRL_CLR_ALL;
  tusb_time_delay_ms_api(1);
  USBFSH->BASE_CTRL = USBFS_CTRL_HOST_MODE;
  tusb_time_delay_ms_api(1);

  USBFSH->HOST_CTRL    = 0;
  USBFSH->DEV_ADDR     = 0;
  USBFSH->HOST_EP_MOD  = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;
  USBFSH->HOST_RX_DMA  = (uint32_t)bounce_buf;
  USBFSH->HOST_TX_DMA  = (uint32_t)bounce_buf;
  USBFSH->HOST_RX_CTRL = 0;
  USBFSH->HOST_TX_CTRL = 0;
  USBFSH->BASE_CTRL    = USBFS_CTRL_HOST_MODE | USBFS_CTRL_INT_BUSY | USBFS_CTRL_DMA_EN;
  USBFSH->OTG_CR       = USBFS_CR_CHARGE_VBUS;

  // SOF runs before any device attaches: its interrupt is the frame counter and the tick
  // that finds a device already connected at power-up, which makes no DETECT edge.
  USBFSH->HOST_SETUP |= USBFS_UH_SOF_EN;

  USBFSH->INT_FG = 0xFF;
  USBFSH->INT_EN = USBFS_INT_EN_HST_SOF | USBFS_INT_EN_TRANSFER | USBFS_INT_EN_DETECT;
  hcd_int_enable(rhport);
  return true;
}

bool hcd_deinit(uint8_t rhport) {
  hcd_int_disable(rhport);
  USBFSH->INT_EN    = 0;
  USBFSH->BASE_CTRL = USBFS_CTRL_RESET_SIE | USBFS_CTRL_CLR_ALL;
  return true;
}

bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param) {
  (void)rhport;
  (void)cfg_id;
  (void)cfg_param;
  return false;
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void)rhport;
  return _hcd.frame_count;
}

void hcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USBHD_IRQn);
}

void hcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USBHD_IRQn);
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  uint8_t const int_fg = USBFSH->INT_FG;

  if (int_fg & USBFS_UIF_DETECT) {
    USBFSH->INT_FG = USBFS_UIF_DETECT;
    root_port_check(rhport, in_isr);
  }

  if (int_fg & USBFS_UIF_TRANSFER) {
    handle_xfer_done(in_isr); // clears the flag
  }

  if (int_fg & USBFS_UIF_HST_SOF) {
    USBFSH->INT_FG = USBFS_UIF_HST_SOF;
    _hcd.frame_count++;
    root_port_check(rhport, in_isr);
    engine_kick();
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, const tusb_desc_endpoint_t *ep_desc) {
  TU_ASSERT(daddr < 128);
  uint8_t const ep_num = tu_edpt_number(ep_desc->bEndpointAddress);
  uint8_t const dir    = tu_edpt_dir(ep_desc->bEndpointAddress);
  uint8_t const xfer_type = ep_desc->bmAttributes.xfer;
  uint16_t const mps = tu_edpt_packet_size(ep_desc);
  TU_ASSERT(mps > 0);

  // FS isochronous: 2^(bInterval-1) frames, interrupt: bInterval frames
  uint8_t interval = 1;
  if (xfer_type == TUSB_XFER_ISOCHRONOUS) {
    interval = (uint8_t)(1u << (tu_min8(tu_max8(ep_desc->bInterval, 1), 8) - 1));
  } else if (xfer_type == TUSB_XFER_INTERRUPT && ep_desc->bInterval) {
    interval = ep_desc->bInterval;
  }

  hcd_int_disable(rhport);
  hcd_ep_t *ep = ep_find(daddr, ep_num, dir);
  if (ep == NULL) {
    ep = ep_alloc();
  }
  if (ep != NULL) {
    tu_memclr(ep, sizeof(hcd_ep_t));
    ep->daddr       = daddr;
    ep->ep_num      = ep_num & 0x0f;
    ep->is_out      = (dir == TUSB_DIR_OUT) ? 1 : 0;
    ep->xfer_type   = xfer_type;
    ep->packet_size = (xfer_type == TUSB_XFER_ISOCHRONOUS) ? tu_min16(mps, 1023) : tu_max16(tu_min16(mps, MAX_PACKET_SIZE), 8);
    ep->interval    = interval;
    ep->next_frame  = _hcd.frame_count;
  }
  hcd_int_enable(rhport);

  TU_ASSERT(ep != NULL);
  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  hcd_int_disable(rhport);
  hcd_ep_t *ep = ep_find(daddr, tu_edpt_number(ep_addr), tu_edpt_dir(ep_addr));
  if (ep != NULL) {
    if (_hcd.cur_idx >= 0 && &_hcd.ep[_hcd.cur_idx] == ep) {
      engine_stop();
    }
    if (ep == &_hcd.ep[0]) {
      ep->pending = 0;
    } else {
      tu_memclr(ep, sizeof(hcd_ep_t));
    }
    engine_kick();
  }
  hcd_int_enable(rhport);
  return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  hcd_ep_t *ep = ep_find(daddr, 0, TUSB_DIR_OUT);
  TU_ASSERT(ep != NULL);

  hcd_int_disable(rhport);
  if (ep->packet_size == 0) {
    ep->packet_size = 8; // address 0 before its endpoint is opened
    ep->interval    = 1;
  }
  memcpy(ep->setup, setup_packet, 8);
  ep->is_setup    = 1;
  ep->is_out      = 1;
  ep->toggle      = 0;
  ep->buf         = NULL;
  ep->total_len   = 8;
  ep->xferred_len = 0;
  ep->next_frame  = _hcd.frame_count;
  ep->pending     = 1;
  engine_kick();
  hcd_int_enable(rhport);
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  uint8_t const ep_num = tu_edpt_number(ep_addr);
  uint8_t const dir    = tu_edpt_dir(ep_addr);

  hcd_ep_t *ep = ep_find(daddr, ep_num, dir);
  TU_ASSERT(ep != NULL && ep->packet_size != 0);

  // one isochronous packet per transfer; a packet larger than bounce_buf goes straight to buffer
  if (ep->xfer_type == TUSB_XFER_ISOCHRONOUS) {
    TU_ASSERT(buflen <= ep->packet_size || dir == TUSB_DIR_IN);
  }
  if (ep->packet_size > MAX_PACKET_SIZE) {
    TU_ASSERT(dma_capable(buffer) && (dir == TUSB_DIR_OUT || buflen >= ep->packet_size));
  }

  hcd_int_disable(rhport);
  if (ep_num == 0) {
    // control data and status stages start with DATA1
    ep->is_setup = 0;
    ep->is_out   = (dir == TUSB_DIR_OUT) ? 1 : 0;
    ep->toggle   = 1;
  }
  ep->buf         = buffer;
  ep->total_len   = buflen;
  ep->xferred_len = 0;

  // keep the interval since the last transfer, unless it has already passed
  int16_t const wait = (int16_t)(ep->next_frame - _hcd.frame_count);
  if (wait < 0 || wait > ep->interval) {
    ep->next_frame = _hcd.frame_count;
  }
  ep->pending = 1;
  engine_kick();
  hcd_int_enable(rhport);
  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  hcd_int_disable(rhport);
  hcd_ep_t *ep = ep_find(daddr, tu_edpt_number(ep_addr), tu_edpt_dir(ep_addr));
  if (ep != NULL) {
    if (_hcd.cur_idx >= 0 && &_hcd.ep[_hcd.cur_idx] == ep) {
      engine_stop();
      sie_wait_idle();
    }
    ep->pending  = 0;
    ep->is_setup = 0;
    engine_kick();
  }
  hcd_int_enable(rhport);
  return ep != NULL;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void)rhport;
  hcd_ep_t *ep = ep_find(daddr, tu_edpt_number(ep_addr), tu_edpt_dir(ep_addr));
  TU_VERIFY(ep != NULL);
  ep->toggle = 0;
  return true;
}

#endif
