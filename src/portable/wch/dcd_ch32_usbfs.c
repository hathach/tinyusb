/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Matthew Tran
 * Copyright (c) 2024 hathach
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

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USBFS) && CFG_TUD_WCH_USBIP_USBFS

  #include "device/dcd.h"
  #include "ch32_usbfs_reg.h"

  /* private defines */
  #define EP_MAX         (8)

  #if CFG_TUSB_MCU == OPT_MCU_CH583
    // EP0-4 and EP5-7 are split, and EP4 has no DMA register
    #define EP_TX_LEN(ep) (*((ep) <= 4u ? &USBOTG_FS->EP_CTRL_0_4[0].T_LEN + (ep) * 4u \
                                        : &USBOTG_FS->EP_CTRL_5_7[0].T_LEN + ((ep) - 5u) * 4u))
    #define EP_CTRL(ep)   (*((ep) <= 4u ? &USBOTG_FS->EP_CTRL_0_4[0].CTRL  + (ep) * 4u \
                                        : &USBOTG_FS->EP_CTRL_5_7[0].CTRL  + ((ep) - 5u) * 4u))
    #define EP_DMA(ep)    (*((ep) <= 3u ? &USBOTG_FS->EP_DMA_0_3[0].DMA + (ep) * 2u \
                             : (ep) == 4u ? &USBOTG_FS->EP_DMA_0_3[0].DMA \
                                          : &USBOTG_FS->EP_DMA_5_7[0].DMA + ((ep) - 5u) * 2u))
  #elif CFG_TUSB_MCU == OPT_MCU_CH32X035
    // EP0-4 and EP5-7 are split, and EP4 has no DMA register
    static inline volatile uint32_t* ch32_usbfs_ep_dma_reg(uint8_t ep) {
      switch (ep) {
        case 0: return &USBOTG_FS->UEP0_DMA;
        case 1: return &USBOTG_FS->UEP1_DMA;
        case 2: return &USBOTG_FS->UEP2_DMA;
        case 3: return &USBOTG_FS->UEP3_DMA;
        case 4: return &USBOTG_FS->UEP0_DMA;
        case 5: return &USBOTG_FS->UEP5_DMA;
        case 6: return &USBOTG_FS->UEP6_DMA;
        default: return &USBOTG_FS->UEP7_DMA;
      }
    }

    // There's a gap between EP4 and EP5 registers.
    #define EP_DMA(ep)     (*ch32_usbfs_ep_dma_reg(ep))
    #define EP_TX_LEN(ep)  ((&USBOTG_FS->UEP0_TX_LEN)[2 * (ep) + ((ep) > 4 ? 24 : 0)])
    #define EP_CTRL(ep)    ((&USBOTG_FS->UEP0_CTRL_H)[2 * (ep) + ((ep) > 4 ? 24 : 0)])
  #else
    #define EP_DMA(ep)     ((&USBOTG_FS->UEP0_DMA)[ep])
    #define EP_TX_LEN(ep)  ((&USBOTG_FS->UEP0_TX_LEN)[2 * ep])
    #define EP_TX_CTRL(ep) ((&USBOTG_FS->UEP0_TX_CTRL)[4 * ep])
    #define EP_RX_CTRL(ep) ((&USBOTG_FS->UEP0_RX_CTRL)[4 * ep])
  #endif

// Map generic TX/RX control values to split or combined endpoint control registers.
// Combined: V103/CH58X/X035
// Split: V20x/V307
#ifdef CH32_USBFS_EP_CTRL_COMBINED
  #ifndef EP_CTRL
    #define EP_CTRL(ep) EP_TX_CTRL(ep)
  #endif

  static inline uint8_t ep_tx_to_comb(uint8_t v) {
    uint8_t c = v & USBFS_EP_T_RES_MASK; // IN response: bits [1:0] in both encodings
    if (v & USBFS_EP_T_TOG)      { c |= USBFS_EPC_T_TOG; }
    if (v & USBFS_EP_T_AUTO_TOG) { c |= USBFS_EPC_AUTO_TOG; }
    return c;
  }
  static inline uint8_t ep_rx_to_comb(uint8_t v) {
    uint8_t c = (uint8_t) ((v & USBFS_EP_R_RES_MASK) << USBFS_EPC_R_RES_SHIFT); // OUT response -> bits [3:2]
    if (v & USBFS_EP_R_TOG)      { c |= USBFS_EPC_R_TOG; }
    if (v & USBFS_EP_R_AUTO_TOG) { c |= USBFS_EPC_AUTO_TOG; }
    return c;
  }
  // Set IN side (response/toggle/auto-tog), preserving the OUT response + OUT toggle.
  static inline void ep_tx_ctrl_set(uint8_t ep, uint8_t v) {
    EP_CTRL(ep) = (uint8_t) ((EP_CTRL(ep) & (USBFS_EPC_R_RES_MASK | USBFS_EPC_R_TOG)) | ep_tx_to_comb(v));
  }
  // Set OUT side, preserving the IN response + IN toggle.
  static inline void ep_rx_ctrl_set(uint8_t ep, uint8_t v) {
    EP_CTRL(ep) = (uint8_t) ((EP_CTRL(ep) & (USBFS_EPC_T_RES_MASK | USBFS_EPC_T_TOG)) | ep_rx_to_comb(v));
  }
  static inline void ep_tx_set_response(uint8_t ep, uint8_t res) {
    EP_CTRL(ep) = (uint8_t) ((EP_CTRL(ep) & ~USBFS_EPC_T_RES_MASK) | (res & USBFS_EP_T_RES_MASK));
  }
  static inline void ep_rx_set_response(uint8_t ep, uint8_t res) {
    EP_CTRL(ep) = (uint8_t) ((EP_CTRL(ep) & ~USBFS_EPC_R_RES_MASK) | ((res & USBFS_EP_R_RES_MASK) << USBFS_EPC_R_RES_SHIFT));
  }
  #define EP0_SETUP_RX_TOG USBFS_EP_R_TOG // combined IP: data/status stage after SETUP is DATA1
#else
  static inline void ep_tx_ctrl_set(uint8_t ep, uint8_t v) { EP_TX_CTRL(ep) = v; }
  static inline void ep_rx_ctrl_set(uint8_t ep, uint8_t v) { EP_RX_CTRL(ep) = v; }
  static inline void ep_tx_set_response(uint8_t ep, uint8_t res) {
    EP_TX_CTRL(ep) = (uint8_t) ((EP_TX_CTRL(ep) & ~USBFS_EP_T_RES_MASK) | res);
  }
  static inline void ep_rx_set_response(uint8_t ep, uint8_t res) {
    EP_RX_CTRL(ep) = (uint8_t) ((EP_RX_CTRL(ep) & ~USBFS_EP_R_RES_MASK) | res);
  }
  #define EP0_SETUP_RX_TOG 0
#endif

// Clear AUTO_TOG on parts that need ISR-driven data toggles.
#ifdef CH32_USBFS_EP_MANUAL_TOG
  #define EP_T_AUTO_TOG 0
  #define EP_R_AUTO_TOG 0
#else
  #define EP_T_AUTO_TOG USBFS_EP_T_AUTO_TOG
  #define EP_R_AUTO_TOG USBFS_EP_R_AUTO_TOG
#endif

/* private data */
struct usb_xfer {
  bool     valid;
  uint8_t *buffer;
  size_t   len;
  size_t   processed_len;
  size_t   max_size;
};

static struct {
  bool            ep0_tog;
  bool            isochronous[EP_MAX];
  struct usb_xfer xfer[EP_MAX][2];
#ifdef CH32_USBFS_EP4_SHARES_EP0
  // EP0/EP4 share one DMA region: EP0, EP4 OUT, EP4 IN.
  // Other endpoints get one OUT+IN pair.
  TU_ATTR_ALIGNED(4) union {
    uint8_t ep0_ep4_buffer[3 * 64];
    struct {
      uint8_t ep0_buffer[64];
      uint8_t ep4_buffer[2][64];
    };
  };
  TU_ATTR_ALIGNED(4) uint8_t buffer[6][2][64];
#else
  TU_ATTR_ALIGNED(4) uint8_t buffer[EP_MAX - 1][2][64];
  // EP3 IN gets an enlarged buffer for full-speed isochronous (packets up to 1023 B).
  TU_ATTR_ALIGNED(4) struct {
    // OUT transfers >64 bytes will overwrite queued IN data!
    uint8_t out[64];
    uint8_t in[1023];
    uint8_t pad;
  } ep3_buffer;
#endif
} data;

#ifdef CH32_USBFS_EP4_SHARES_EP0
static inline uint8_t* ep_buffer(uint8_t ep, uint8_t dir) {
  switch (ep) {
    case 1:  return data.buffer[0][dir];
    case 2:  return data.buffer[1][dir];
    case 3:  return data.buffer[2][dir];
    case 4:  return data.ep4_buffer[dir];
    case 5:  return data.buffer[3][dir];
    case 6:  return data.buffer[4][dir];
    default: return data.buffer[5][dir]; // ep == 7
  }
}

static inline uint32_t ep_dma_addr(uint8_t ep) {
  if (ep == 0 || ep == 4) { return (uint32_t) data.ep0_ep4_buffer; } // EP4 shares EP0's DMA
  return (uint32_t) ep_buffer(ep, TUSB_DIR_OUT);
}

static inline uint8_t* ep_out_buf(uint8_t ep) {
  if (ep == 0) { return data.ep0_buffer; }
  return ep_buffer(ep, TUSB_DIR_OUT);
}

static inline uint8_t* ep_in_buf(uint8_t ep) {
  if (ep == 0) { return data.ep0_buffer; } // EP0 half-duplex: IN reuses OUT chunk
  return ep_buffer(ep, TUSB_DIR_IN);
}

// EP4 shares EP0's DMA register.
static inline bool ep_shares_ep0_dma(uint8_t ep) {
  return ep == 4;
}
#else
static inline uint8_t* ep_buffer(uint8_t ep, uint8_t dir) {
  if (ep > 3) { return data.buffer[ep - 1][dir]; }
  return data.buffer[ep][dir];
}

static inline uint32_t ep_dma_addr(uint8_t ep) {
  if (ep == 3) { return (uint32_t) &data.ep3_buffer.out[0]; }
  return (uint32_t) ep_buffer(ep, TUSB_DIR_OUT);
}

static inline uint8_t* ep_out_buf(uint8_t ep) {
  if (ep == 3) { return data.ep3_buffer.out; }
  return ep_buffer(ep, TUSB_DIR_OUT);
}

static inline uint8_t* ep_in_buf(uint8_t ep) {
  if (ep == 0) { return ep_buffer(0, TUSB_DIR_OUT); } // EP0 half-duplex: IN reuses OUT chunk
  if (ep == 3) { return data.ep3_buffer.in; }
  return ep_buffer(ep, TUSB_DIR_IN);
}

static inline bool ep_shares_ep0_dma(uint8_t ep) {
  (void) ep;
  return false;
}
#endif

/* private helpers */
static void update_in(uint8_t rhport, uint8_t ep, bool force) {
  struct usb_xfer *xfer = &data.xfer[ep][TUSB_DIR_IN];
  if (xfer->valid) {
    if (force || xfer->len) {
      size_t len = TU_MIN(xfer->max_size, xfer->len);
#if CFG_TUSB_MCU == OPT_MCU_CH583 || CFG_TUSB_MCU == OPT_MCU_CH32X035
      // These variants only have 64-byte endpoint buffers. Cap the copy here to guarantee we never
      // write past the buffer into a neighbouring endpoint's.
      len = TU_MIN(len, 64u);
#endif
      memcpy(ep_in_buf(ep), xfer->buffer, len);
      xfer->buffer += len;
      xfer->len -= len;
      xfer->processed_len += len;

      EP_TX_LEN(ep) = len;
      if (ep == 0) {
        ep_tx_ctrl_set(0, USBFS_EP_T_RES_ACK | (data.ep0_tog ? USBFS_EP_T_TOG : 0));
        data.ep0_tog  = !data.ep0_tog;
      } else if (data.isochronous[ep]) {
        ep_tx_set_response(ep, USBFS_EP_T_RES_NYET);
      } else {
        ep_tx_set_response(ep, USBFS_EP_T_RES_ACK);
      }
    } else {
      xfer->valid = false;
      if (ep == 0) {
        ep_tx_ctrl_set(0, USBFS_EP_T_RES_NAK | (data.ep0_tog ? USBFS_EP_T_TOG : 0));
      } else if (!data.isochronous[ep]) {
        ep_tx_set_response(ep, USBFS_EP_T_RES_NAK);
      }
      dcd_event_xfer_complete(rhport, ep | TUSB_DIR_IN_MASK, xfer->processed_len, XFER_RESULT_SUCCESS, true);
    }
  }
}

static void update_out(uint8_t rhport, uint8_t ep, size_t rx_len) {
  struct usb_xfer *xfer = &data.xfer[ep][TUSB_DIR_OUT];
  if (xfer->valid) {
    size_t len = TU_MIN(xfer->max_size, TU_MIN(xfer->len, rx_len));
#if CFG_TUSB_MCU == OPT_MCU_CH583 || CFG_TUSB_MCU == OPT_MCU_CH32X035
    len = TU_MIN(len, 64u); // cap to the 64-byte EP buffer (see update_in)
#endif
    memcpy(xfer->buffer, ep_out_buf(ep), len);
    xfer->buffer += len;
    xfer->len -= len;
    xfer->processed_len += len;

    if (xfer->len == 0 || len < xfer->max_size) {
      xfer->valid = false;
      dcd_event_xfer_complete(rhport, ep, xfer->processed_len, XFER_RESULT_SUCCESS, true);
    }

    if (ep == 0) {
      ep_rx_set_response(0, USBFS_EP_R_RES_NAK);
    } else {
      uint8_t rx_res =
        data.isochronous[ep] ? USBFS_EP_R_RES_NYET : (xfer->valid ? USBFS_EP_R_RES_ACK : USBFS_EP_R_RES_NAK);
      ep_rx_set_response(ep, rx_res);
    }
  }
}

static void reset_ep_ctrls(void) {
  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    if (!ep_shares_ep0_dma(ep)) { EP_DMA(ep) = ep_dma_addr(ep); }
    EP_TX_LEN(ep)  = 0;
    ep_tx_ctrl_set(ep, EP_T_AUTO_TOG | USBFS_EP_T_RES_NYET);
    ep_rx_ctrl_set(ep, EP_R_AUTO_TOG | USBFS_EP_R_RES_NYET);
  }
}

/* public functions */
bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rh_init;
  // init registers
  USBOTG_FS->BASE_CTRL = USBFS_CTRL_SYS_CTRL | USBFS_CTRL_INT_BUSY | USBFS_CTRL_DMA_EN;
  USBOTG_FS->UDEV_CTRL = USBFS_UDEV_CTRL_PD_DIS | USBFS_UDEV_CTRL_PORT_EN;
  USBOTG_FS->DEV_ADDR  = 0x00;

  USBOTG_FS->INT_FG = 0xFF;
  USBOTG_FS->INT_EN = USBFS_INT_EN_BUS_RST | USBFS_INT_EN_TRANSFER | USBFS_INT_EN_SUSPEND;

  // setup endpoint 0 (also backs EP4's buffer on CH58X via the shared DMA region)
  EP_DMA(0)     = ep_dma_addr(0);
  EP_TX_LEN(0)  = 0;
  ep_tx_ctrl_set(0, USBFS_EP_T_RES_NAK);
  ep_rx_ctrl_set(0, USBFS_EP_R_RES_ACK);

  // enable other endpoints but NAK everything
  USBOTG_FS->UEP4_1_MOD = 0xCC;
  USBOTG_FS->UEP2_3_MOD = 0xCC;
#if CFG_TUSB_MCU == OPT_MCU_CH583 || CFG_TUSB_MCU == OPT_MCU_CH32X035
  USBOTG_FS->UEP567_MOD = 0x3F;
#else
  USBOTG_FS->UEP5_6_MOD = 0xCC;
  USBOTG_FS->UEP7_MOD   = 0x0C;
#endif

  reset_ep_ctrls();

  dcd_connect(rhport);

  return true;
}

void dcd_int_handler(uint8_t rhport) {
  (void)rhport;
  uint8_t status = USBOTG_FS->INT_FG;
  if (status & USBFS_INT_FG_TRANSFER) {
    uint8_t  int_st   = USBOTG_FS->INT_ST;
    uint8_t  ep       = USBFS_INT_ST_MASK_UIS_ENDP(int_st);
    uint8_t  token    = USBFS_INT_ST_MASK_UIS_TOKEN(int_st);
    uint16_t rx_len   = USBOTG_FS->RX_LEN;

    switch (token) {
      case PID_OUT: {
        // Drop unexpected DATA0/DATA1 packets; EP0 has its own control-flow toggle.
        if (ep != 0 && !(int_st & USBFS_INT_ST_TOG_OK)) { break; }
#ifdef CH32_USBFS_EP_MANUAL_TOG
        // Manual toggle: advance RX after each accepted packet.
        EP_CTRL(ep) ^= USBFS_EPC_R_TOG;
#endif
        update_out(rhport, ep, rx_len);
        break;
      }

      case PID_IN:
#ifdef CH32_USBFS_EP_MANUAL_TOG
        // Manual toggle: flip the TX toggle after each ACK'd IN packet (EP0 manages its own).
        if (ep != 0) { EP_CTRL(ep) ^= USBFS_EPC_T_TOG; }
#endif
        update_in(rhport, ep, false);
        break;

      case PID_SETUP:
        // setup clears stall
        ep_tx_ctrl_set(0, USBFS_EP_T_RES_NAK);
        data.ep0_tog  = true;
        // A new SETUP cancels any stale EP0 transfer.
        data.xfer[0][TUSB_DIR_OUT].valid = false;
        data.xfer[0][TUSB_DIR_IN].valid  = false;

        uint8_t *ep0_out = ep_out_buf(0);
        const tusb_control_request_t *setup = (const tusb_control_request_t *)ep0_out;
        // EP0_SETUP_RX_TOG arms the data/status stage at DATA1 on the combined-control IP
        ep_rx_ctrl_set(0, ((setup->wLength == 0) ? USBFS_EP_R_RES_ACK : USBFS_EP_R_RES_NAK) | EP0_SETUP_RX_TOG);

        dcd_event_setup_received(rhport, ep0_out, true);
        break;
    }

    USBOTG_FS->INT_FG = USBFS_INT_FG_TRANSFER;
  } else if (status & USBFS_INT_FG_BUS_RST) {
    data.ep0_tog                        = true;
    data.xfer[0][TUSB_DIR_OUT].max_size = 64;
    data.xfer[0][TUSB_DIR_IN].max_size  = 64;

    // dcd_event_bus_reset(rhport, (USBOTG_FS->BASE_CTRL & USBFS_CTRL_LOW_SPEED) ? TUSB_SPEED_LOW : TUSB_SPEED_FULL,
    // true);
    dcd_event_bus_reset(rhport, (USBOTG_FS->UDEV_CTRL & USBFS_UDEV_CTRL_LOW_SPEED) ? TUSB_SPEED_LOW : TUSB_SPEED_FULL,
                        true);

    USBOTG_FS->DEV_ADDR = 0x00;
    ep_rx_ctrl_set(0, USBFS_EP_R_RES_ACK);

    reset_ep_ctrls();

    USBOTG_FS->INT_FG = USBFS_INT_FG_BUS_RST;
  } else if (status & USBFS_INT_FG_SUSPEND) {
#if CFG_TUSB_MCU == OPT_MCU_CH583
    // CH58x raises this single interrupt for both suspend and resume; MIS_ST's suspend bit tells
    // them apart (set while suspended, clear once resumed) so tud_resume_cb() actually fires.
    dcd_event_t event = {.rhport = rhport,
                         .event_id = (USBOTG_FS->MIS_ST & USBFS_MIS_ST_SUSPEND) ? DCD_EVENT_SUSPEND : DCD_EVENT_RESUME};
#else
    dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_SUSPEND};
#endif
    dcd_event_handler(&event, true);
    USBOTG_FS->INT_FG = USBFS_INT_FG_SUSPEND;
  }
}

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USBHD_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USBHD_IRQn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false); // zlp status response
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void)rhport;
  // TODO optional
}

void dcd_connect(uint8_t rhport) {
  (void)rhport;
  USBOTG_FS->BASE_CTRL |= USBFS_CTRL_DEV_PUEN;
}

void dcd_disconnect(uint8_t rhport) {
  (void)rhport;
  USBOTG_FS->BASE_CTRL &= ~USBFS_CTRL_DEV_PUEN;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void)rhport;
  (void)en;

  // TODO implement later
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;
  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD && request->bRequest == TUSB_REQ_SET_ADDRESS) {
#if CFG_TUSB_MCU == OPT_MCU_CH583
    // On CH58x R8_USB_DEV_AD bit 7 is a user general-purpose flag; only bits [6:0] are the address.
    USBOTG_FS->DEV_ADDR = (uint8_t)((USBOTG_FS->DEV_ADDR & 0x80u) | (request->wValue & 0x7Fu));
#else
    USBOTG_FS->DEV_ADDR = (uint8_t)request->wValue;
#endif
  }
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
  (void)rhport;
  uint8_t ep  = tu_edpt_number(desc_ep->bEndpointAddress);
  uint8_t dir = tu_edpt_dir(desc_ep->bEndpointAddress);
  TU_ASSERT(ep < EP_MAX);

  data.xfer[ep][dir].max_size = tu_edpt_packet_size(desc_ep);

  if (ep != 0) {
    // Opening clears the toggle to DATA0 (ep_*_ctrl_set writes the toggle bit clear since v has no
    // R/T_TOG); with manual toggle EP_*_AUTO_TOG is 0 so the ISR owns subsequent toggling.
    if (dir == TUSB_DIR_OUT) {
      ep_rx_ctrl_set(ep, EP_R_AUTO_TOG | USBFS_EP_R_RES_NAK);
    } else {
      ep_tx_ctrl_set(ep, EP_T_AUTO_TOG | USBFS_EP_T_RES_NAK);
    }
  }
  return true;
}

void dcd_edpt_close_all(uint8_t rhport) {
  (void)rhport;
  // TODO optional
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  (void)rhport;
  (void)ep_addr;
  (void)largest_packet_size;
#if CFG_TUSB_MCU == OPT_MCU_CH583
  // No isochronous support on CH58x: its 8-bit T_LEN caps a packet at 255B and the endpoints use
  // plain 64-byte buffers, so accepting an iso max_size (up to 1023) would let update_in()/
  // update_out() run off the end of the buffer into neighbouring ones. Refuse it outright.
  return false;
#else
  #if CFG_TUSB_MCU == OPT_MCU_CH32X035
  if (largest_packet_size > 64u) { return false; }
  #endif
  uint8_t ep  = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);

  data.isochronous[ep]        = true;
  data.xfer[ep][dir].max_size = largest_packet_size;
  return true;
#endif
}

bool dcd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
  (void)rhport;
  (void)desc_ep;
#if CFG_TUSB_MCU == OPT_MCU_CH583
  return false; // CH58x has no isochronous support (see dcd_edpt_iso_alloc)
#else
  #if CFG_TUSB_MCU == OPT_MCU_CH32X035
  if (tu_edpt_packet_size(desc_ep) > 64u) { return false; }
  #endif
  return true;
#endif
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  (void)rhport;
  uint8_t ep  = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);

  struct usb_xfer *xfer = &data.xfer[ep][dir];
  // Keep IRQ masked while arming; EP control RMWs also happen in the ISR.
  dcd_int_disable(rhport);
  xfer->valid         = true;
  xfer->buffer        = buffer;
  xfer->len           = total_bytes;
  xfer->processed_len = 0;

  if (dir == TUSB_DIR_IN) {
    update_in(rhport, ep, true);
  } else {
    uint8_t rx_res = data.isochronous[ep] ? USBFS_EP_R_RES_NYET : USBFS_EP_R_RES_ACK;
    ep_rx_set_response(ep, rx_res);
  }
  dcd_int_enable(rhport);
  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  uint8_t ep  = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);
  if (ep == 0) {
    if (dir == TUSB_DIR_OUT) {
      ep_rx_ctrl_set(0, USBFS_EP_R_RES_STALL);
    } else {
      EP_TX_LEN(0)  = 0;
      ep_tx_ctrl_set(0, USBFS_EP_T_RES_STALL);
    }
  } else {
    if (dir == TUSB_DIR_OUT) {
      ep_rx_set_response(ep, USBFS_EP_R_RES_STALL);
    } else {
      ep_tx_set_response(ep, USBFS_EP_T_RES_STALL);
    }
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  uint8_t ep  = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);
  if (ep == 0) {
    if (dir == TUSB_DIR_OUT) {
      ep_rx_ctrl_set(0, USBFS_EP_R_RES_ACK);
    }
  } else {
    // clear-stall resets the toggle to DATA0 (USB spec); manual-toggle parts then re-sync via ISR
    if (dir == TUSB_DIR_OUT) {
      ep_rx_ctrl_set(ep, EP_R_AUTO_TOG | USBFS_EP_R_RES_NAK);
    } else {
      ep_tx_ctrl_set(ep, EP_T_AUTO_TOG | USBFS_EP_T_RES_NAK);
    }
  }
}
#endif
