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

  // Struct-based EP register access (uniform layout). CH58X has a different register map and
  // defines EP_DMA/EP_TX_LEN/EP_CTRL itself in ch32_usbfs_reg.h.
  #if CFG_TUSB_MCU == OPT_MCU_CH583
    // CH58X EP registers split into a low block (EP0-4) and a high block (EP5-7). Walk from each
    // block's first slot by the 4-byte slot stride (pointer arithmetic off slot 0, so the unused
    // ternary branch's index can't trip -Warray-bounds). EP4 has no DMA register of its own (it
    // shares EP0's, slot 0) and is never written (see ep_shares_ep0_dma()).
    #define EP_TX_LEN(ep) (*((ep) <= 4u ? &USBOTG_FS->EP_CTRL_0_4[0].T_LEN + (ep) * 4u \
                                        : &USBOTG_FS->EP_CTRL_5_7[0].T_LEN + ((ep) - 5u) * 4u))
    #define EP_CTRL(ep)   (*((ep) <= 4u ? &USBOTG_FS->EP_CTRL_0_4[0].CTRL  + (ep) * 4u \
                                        : &USBOTG_FS->EP_CTRL_5_7[0].CTRL  + ((ep) - 5u) * 4u))
    #define EP_DMA(ep)    (*((ep) <= 3u ? &USBOTG_FS->EP_DMA_0_3[0].DMA + (ep) * 2u \
                             : (ep) == 4u ? &USBOTG_FS->EP_DMA_0_3[0].DMA \
                                          : &USBOTG_FS->EP_DMA_5_7[0].DMA + ((ep) - 5u) * 2u))
  #else
    #define EP_DMA(ep)     ((&USBOTG_FS->UEP0_DMA)[ep])
    #define EP_TX_LEN(ep)  ((&USBOTG_FS->UEP0_TX_LEN)[2 * ep])
    #define EP_TX_CTRL(ep) ((&USBOTG_FS->UEP0_TX_CTRL)[4 * ep])
    #define EP_RX_CTRL(ep) ((&USBOTG_FS->UEP0_RX_CTRL)[4 * ep])
  #endif

// Endpoint control register access. The newer USBFS IP (CH32V20x/V307/X035) has separate
// TX_CTRL and RX_CTRL bytes per endpoint; the older IP (CH32V103) has a single combined
// UEPn_CTRL register. These helpers hide the difference so the rest of the driver is shared.
// Values use the newer-IP encoding (USBFS_EP_T_*/USBFS_EP_R_*); the combined path remaps them.
#ifdef CH32_USBFS_EP_CTRL_COMBINED
  #ifndef EP_CTRL // parts with a custom register map (CH58X) define EP_CTRL directly in reg.h
  #define EP_CTRL(ep) EP_TX_CTRL(ep) // UEPn_TX_CTRL field aliases the combined UEPn_CTRL register
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

// Hardware auto data-toggle flag. Parts whose AUTO_TOG is reliable OR it into the EP setup so the
// controller flips DATA0/DATA1 itself; CH58x (CH32_USBFS_EP_MANUAL_TOG) leaves it clear and the
// ISR flips the toggle bit after each packet instead.
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
  // CH58X buffers laid out by hand so EP0/EP4 don't burn two unused buffer[] slots. EP0 and EP4
  // share one contiguous 192-byte DMA region (EP4 has no DMA register of its own):
  // EP0 [0:63] (half-duplex OUT+IN) + EP4 OUT [64:127] + EP4 IN [128:191]. Every other endpoint
  // (incl. EP3, which is bulk-only here — CH58X has no isochronous support) gets a plain 128-byte
  // OUT+IN buffer, so no oversized EP3 buffer is needed.
  TU_ATTR_ALIGNED(4) uint8_t ep0_ep4_buffer[3 * 64];
  TU_ATTR_ALIGNED(4) uint8_t ep1_buffer[2][64];
  TU_ATTR_ALIGNED(4) uint8_t ep2_buffer[2][64];
  TU_ATTR_ALIGNED(4) uint8_t ep3_buffer[2][64];
  TU_ATTR_ALIGNED(4) uint8_t ep5_buffer[2][64];
  TU_ATTR_ALIGNED(4) uint8_t ep6_buffer[2][64];
  TU_ATTR_ALIGNED(4) uint8_t ep7_buffer[2][64];
#else
  TU_ATTR_ALIGNED(4) uint8_t buffer[EP_MAX][2][64];
  // EP3 IN gets an enlarged buffer for full-speed isochronous (packets up to 1023 B).
  TU_ATTR_ALIGNED(4) struct {
    // OUT transfers >64 bytes will overwrite queued IN data!
    uint8_t out[64];
    uint8_t in[1023];
    uint8_t pad;
  } ep3_buffer;
#endif
} data;

// DMA / copy buffer pointers per endpoint. The WCH USBFS buffer holds OUT (RX) at offset 0 and
// IN (TX) at +64; EP0 is half-duplex and reuses its OUT chunk for IN; EP3 has an enlarged IN
// buffer for throughput. On CH58X, EP0/EP4 share ep0_ep4_buffer and the regular endpoints use
// their own named buffer (see the struct above).
#ifdef CH32_USBFS_EP4_SHARES_EP0
// OUT base of the regular CH58X endpoints (EP1/2/3/5/6/7; EP0/EP4 share ep0_ep4_buffer).
static inline uint8_t* ch58x_ep_buffer(uint8_t ep) {
  switch (ep) {
    case 1:  return data.ep1_buffer[0];
    case 2:  return data.ep2_buffer[0];
    case 3:  return data.ep3_buffer[0];
    case 5:  return data.ep5_buffer[0];
    case 6:  return data.ep6_buffer[0];
    default: return data.ep7_buffer[0]; // ep == 7
  }
}
#endif

static inline uint32_t ep_dma_addr(uint8_t ep) {
#ifdef CH32_USBFS_EP4_SHARES_EP0
  if (ep == 0 || ep == 4) { return (uint32_t) &data.ep0_ep4_buffer[0]; } // EP4 shares EP0's DMA
  return (uint32_t) ch58x_ep_buffer(ep);
#else
  if (ep == 3) { return (uint32_t) &data.ep3_buffer.out[0]; }
  return (uint32_t) &data.buffer[ep][0];
#endif
}

static inline uint8_t* ep_out_buf(uint8_t ep) {
#ifdef CH32_USBFS_EP4_SHARES_EP0
  if (ep == 0) { return &data.ep0_ep4_buffer[0]; }
  if (ep == 4) { return &data.ep0_ep4_buffer[64]; }
  return ch58x_ep_buffer(ep);
#else
  if (ep == 3) { return data.ep3_buffer.out; }
  return data.buffer[ep][TUSB_DIR_OUT];
#endif
}

static inline uint8_t* ep_in_buf(uint8_t ep) {
#ifdef CH32_USBFS_EP4_SHARES_EP0
  if (ep == 0) { return &data.ep0_ep4_buffer[0]; } // EP0 half-duplex: IN reuses OUT chunk
  if (ep == 4) { return &data.ep0_ep4_buffer[128]; }
  return ch58x_ep_buffer(ep) + 64; // IN at +64 within the endpoint's 128-byte buffer
#else
  if (ep == 0) { return data.buffer[0][TUSB_DIR_OUT]; } // EP0 half-duplex: IN reuses OUT chunk
  if (ep == 3) { return data.ep3_buffer.in; }
  return data.buffer[ep][TUSB_DIR_IN];
#endif
}

// EP4 on CH58X has no DMA register (shares EP0's); skip its EP_DMA() write.
static inline bool ep_shares_ep0_dma(uint8_t ep) {
#ifdef CH32_USBFS_EP4_SHARES_EP0
  return ep == 4;
#else
  (void) ep;
  return false;
#endif
}

/* private helpers */
static void update_in(uint8_t rhport, uint8_t ep, bool force) {
  struct usb_xfer *xfer = &data.xfer[ep][TUSB_DIR_IN];
  if (xfer->valid) {
    if (force || xfer->len) {
      size_t len = TU_MIN(xfer->max_size, xfer->len);
#if CFG_TUSB_MCU == OPT_MCU_CH583
      // Every CH58x endpoint buffer is 64 bytes. Isochronous (which would push max_size up to 1023)
      // is refused in dcd_edpt_iso_alloc(), but some classes (e.g. video) ignore that result, so cap
      // the copy here to guarantee we never write past the buffer into a neighbouring endpoint's.
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
#if CFG_TUSB_MCU == OPT_MCU_CH583
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
#if CFG_TUSB_MCU == OPT_MCU_CH583
  // CH58X: a single mode register enables EP5/6/7 RX+TX (different bit layout than CH32).
  USBOTG_FS->UEP567_MOD = RB_UEP5_RX_EN | RB_UEP5_TX_EN | RB_UEP6_RX_EN | RB_UEP6_TX_EN |
                          RB_UEP7_RX_EN | RB_UEP7_TX_EN;
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
        // Drop an OUT packet whose data toggle doesn't match what we expect -- a host retransmit
        // after a lost ACK, or a host that doesn't alternate DATA0/DATA1. The hardware auto-toggle
        // does not reject these on its own, so the check is needed on every variant. EP0 keeps its
        // own toggle via the SETUP/status flow and is exempt.
        if (ep != 0 && !(int_st & USBFS_INT_ST_TOG_OK)) { break; }
#ifdef CH32_USBFS_EP_MANUAL_TOG
        // CH58x has no hardware auto-toggle: advance the expected RX toggle after each accepted packet
        // (EP0 included -- it also has no auto-toggle and a control-OUT data stage can span packets).
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
        // A new SETUP supersedes any control transfer still in flight; drop its stale EP0 state so a
        // spurious EP0 IN/OUT can't run update_in()/update_out() against the previous request.
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
  return true;
#endif
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  (void)rhport;
  uint8_t ep  = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);

  struct usb_xfer *xfer = &data.xfer[ep][dir];
  // Keep the IRQ masked across the whole arming sequence: update_in()/ep_rx_set_response() do a
  // read-modify-write of the (combined) EP control register, which the ISR also RMWs to flip the
  // manual data toggle; re-enabling before they run lets a transfer IRQ clobber that toggle.
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
