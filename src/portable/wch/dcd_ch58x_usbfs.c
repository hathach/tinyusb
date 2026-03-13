/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 TinyUSB contributors
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

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_CH58X) && CFG_TUD_WCH_USBIP_USBFS

#include "device/dcd.h"
#include "ch58x_usbfs_reg.h"

//--------------------------------------------------------------------+
// Configuration
//--------------------------------------------------------------------+
#define EP_MAX       8
#define EP_BUF_SIZE  64

//--------------------------------------------------------------------+
// USB base address selection by rhport
//--------------------------------------------------------------------+
static inline uint32_t get_usb_base(uint8_t rhport) {
  return (rhport == 0) ? CH58X_USB_BASE : CH58X_USB2_BASE;
}

//--------------------------------------------------------------------+
// Register access helpers using base address
//--------------------------------------------------------------------+
#define USB_CTRL(base)     CH58X_USB_CTRL(base)
#define USB_UDEV_CTRL(base) CH58X_UDEV_CTRL(base)
#define USB_INT_EN(base)   CH58X_USB_INT_EN(base)
#define USB_DEV_AD(base)   CH58X_USB_DEV_AD(base)
#define USB_MIS_ST(base)   CH58X_USB_MIS_ST(base)
#define USB_INT_FG(base)   CH58X_USB_INT_FG(base)
#define USB_INT_ST(base)   CH58X_USB_INT_ST(base)
#define USB_RX_LEN(base)   CH58X_USB_RX_LEN(base)

#define EP_DMA(base, ep)   CH58X_EP_DMA(base, ep)
#define EP_TLEN(base, ep)  CH58X_EP_TLEN(base, ep)
#define EP_CTRL(base, ep)  CH58X_EP_CTRL(base, ep)

//--------------------------------------------------------------------+
// Inline helpers for merged EP CTRL register
//--------------------------------------------------------------------+
static inline void ep_set_tx_response(uint32_t base, uint8_t ep, uint8_t resp) {
  uint8_t ctrl = EP_CTRL(base, ep);
  ctrl = (ctrl & ~CH58X_EP_T_RES_MASK) | resp;
  EP_CTRL(base, ep) = ctrl;
}

static inline void ep_set_rx_response(uint32_t base, uint8_t ep, uint8_t resp) {
  uint8_t ctrl = EP_CTRL(base, ep);
  ctrl = (ctrl & ~CH58X_EP_R_RES_MASK) | resp;
  EP_CTRL(base, ep) = ctrl;
}

static inline void ep_set_both_response(uint32_t base, uint8_t ep, uint8_t tx_resp, uint8_t rx_resp) {
  uint8_t ctrl = EP_CTRL(base, ep);
  ctrl = (ctrl & ~(CH58X_EP_T_RES_MASK | CH58X_EP_R_RES_MASK)) | tx_resp | rx_resp;
  EP_CTRL(base, ep) = ctrl;
}

//--------------------------------------------------------------------+
// Private data structures
//--------------------------------------------------------------------+
typedef struct {
  bool valid;
  uint8_t* buffer;
  size_t len;
  size_t processed_len;
  size_t max_size;
} xfer_ctl_t;

typedef struct {
  uint32_t usb_base;
  bool ep0_tog;
  volatile uint8_t setup_pending;     // SETUP arrived while EP0 completion pending in queue
  volatile bool ep0_completion_pending; // EP0 XFER_COMPLETE in event queue, not yet processed
  uint8_t pending_addr;               // Address to set in ISR after Set Address status ZLP
  bool isochronous[EP_MAX][2]; // [ep][dir]
  xfer_ctl_t xfer[EP_MAX][2];  // [ep][dir]

  // EP0 + EP4 shared buffer: EP0 OUT(64) + EP4 OUT(64) + EP4 IN(64)
  TU_ATTR_ALIGNED(4) uint8_t ep0_buffer[EP_BUF_SIZE + EP_BUF_SIZE + EP_BUF_SIZE];

  // EP1-EP3: OUT(64) + IN(64)
  TU_ATTR_ALIGNED(4) uint8_t ep1_buffer[2][EP_BUF_SIZE];
  TU_ATTR_ALIGNED(4) uint8_t ep2_buffer[2][EP_BUF_SIZE];
  TU_ATTR_ALIGNED(4) uint8_t ep3_buffer[2][EP_BUF_SIZE];

  // EP5-EP7: OUT(64) + IN(64)
  TU_ATTR_ALIGNED(4) uint8_t ep5_buffer[2][EP_BUF_SIZE];
  TU_ATTR_ALIGNED(4) uint8_t ep6_buffer[2][EP_BUF_SIZE];
  TU_ATTR_ALIGNED(4) uint8_t ep7_buffer[2][EP_BUF_SIZE];
} dcd_data_t;

// Per-port data (support up to 2 USB ports)
static dcd_data_t _dcd_data[2];

//--------------------------------------------------------------------+
// Buffer address helpers
//--------------------------------------------------------------------+
static uint8_t* ep_out_buffer(dcd_data_t* d, uint8_t ep) {
  switch (ep) {
    case 0: return &d->ep0_buffer[0];
    case 1: return d->ep1_buffer[0];
    case 2: return d->ep2_buffer[0];
    case 3: return d->ep3_buffer[0];
    case 4: return &d->ep0_buffer[EP_BUF_SIZE];
    case 5: return d->ep5_buffer[0];
    case 6: return d->ep6_buffer[0];
    case 7: return d->ep7_buffer[0];
    default: return NULL;
  }
}

static uint8_t* ep_in_buffer(dcd_data_t* d, uint8_t ep) {
  switch (ep) {
    case 0: return &d->ep0_buffer[0]; // EP0 IN uses same buffer as OUT
    case 1: return d->ep1_buffer[1];
    case 2: return d->ep2_buffer[1];
    case 3: return d->ep3_buffer[1];
    case 4: return &d->ep0_buffer[2 * EP_BUF_SIZE];
    case 5: return d->ep5_buffer[1];
    case 6: return d->ep6_buffer[1];
    case 7: return d->ep7_buffer[1];
    default: return NULL;
  }
}

// DMA base pointer (EP4 shares with EP0)
static uint8_t* ep_dma_buffer(dcd_data_t* d, uint8_t ep) {
  switch (ep) {
    case 0: case 4: return &d->ep0_buffer[0];
    case 1: return d->ep1_buffer[0];
    case 2: return d->ep2_buffer[0];
    case 3: return d->ep3_buffer[0];
    case 5: return d->ep5_buffer[0];
    case 6: return d->ep6_buffer[0];
    case 7: return d->ep7_buffer[0];
    default: return NULL;
  }
}

// AUTO_TOG is not used (EP1-3/5-7 support it). When clear-stall resets T_TOG/
// R_TOG to DATA0, the hardware internal toggle doesn't sync, causing mismatch
// and bus resets. Use manual toggle (EP_CTRL ^= TOG) in ISR instead.

//--------------------------------------------------------------------+
// Private transfer helpers
//--------------------------------------------------------------------+
static void update_in(uint8_t rhport, uint8_t ep, bool force, bool in_isr) {
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = d->usb_base;
  xfer_ctl_t* xfer = &d->xfer[ep][TUSB_DIR_IN];

  if (xfer->valid) {
    if (force || xfer->len) {
      size_t len = TU_MIN(xfer->max_size, xfer->len);

      // Copy data to IN buffer
      uint8_t* buf = ep_in_buffer(d, ep);
      if (len > 0 && xfer->buffer != NULL) {
        memcpy(buf, xfer->buffer, len);
      }

      xfer->buffer += len;
      xfer->len -= len;
      xfer->processed_len += len;

      EP_TLEN(base, ep) = (uint8_t) len;

      if (ep == 0) {
        // EP0: manual toggle
        uint8_t ctrl = EP_CTRL(base, 0);
        ctrl = (ctrl & ~(CH58X_EP_T_RES_MASK | CH58X_EP_T_TOG));
        ctrl |= CH58X_EP_T_RES_ACK;
        if (d->ep0_tog) ctrl |= CH58X_EP_T_TOG;
        EP_CTRL(base, 0) = ctrl;
        d->ep0_tog = !d->ep0_tog;
      } else if (d->isochronous[ep][TUSB_DIR_IN]) {
        ep_set_tx_response(base, ep, CH58X_EP_T_RES_TOUT);
      } else {
        ep_set_tx_response(base, ep, CH58X_EP_T_RES_ACK);
      }
    } else {
      // Transfer complete
      xfer->valid = false;
      ep_set_tx_response(base, ep, CH58X_EP_T_RES_NAK);
      if (ep == 0) d->ep0_completion_pending = true;
      dcd_event_xfer_complete(rhport, ep | TUSB_DIR_IN_MASK,
                              xfer->processed_len, XFER_RESULT_SUCCESS, in_isr);
    }
  }
}

static void update_out(uint8_t rhport, uint8_t ep, size_t rx_len, bool in_isr) {
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = d->usb_base;
  xfer_ctl_t* xfer = &d->xfer[ep][TUSB_DIR_OUT];

  if (xfer->valid) {
    size_t len = TU_MIN(xfer->max_size, TU_MIN(xfer->len, rx_len));

    // Copy from OUT buffer
    if (len > 0 && xfer->buffer != NULL) {
      uint8_t* buf = ep_out_buffer(d, ep);
      memcpy(xfer->buffer, buf, len);
    }

    xfer->buffer += len;
    xfer->len -= len;
    xfer->processed_len += len;

    if (xfer->len == 0 || len < xfer->max_size) {
      xfer->valid = false;
      // NAK to prevent hardware from accepting next OUT before a new xfer is queued
      ep_set_rx_response(base, ep, CH58X_EP_R_RES_NAK);
      if (ep == 0) d->ep0_completion_pending = true;
      dcd_event_xfer_complete(rhport, ep, xfer->processed_len,
                              XFER_RESULT_SUCCESS, in_isr);
    } else if (ep == 0) {
      // EP0 multi-packet: ensure ACK for next packet
      ep_set_rx_response(base, 0, CH58X_EP_R_RES_ACK);
    }
  }
}

//--------------------------------------------------------------------+
// DCD API Implementation
//--------------------------------------------------------------------+

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = get_usb_base(rhport);
  d->usb_base = base;

  // Clear state
  tu_memclr(d->xfer, sizeof(d->xfer));
  tu_memclr(d->isochronous, sizeof(d->isochronous));
  d->ep0_tog = true;
  d->setup_pending = 0;
  d->ep0_completion_pending = false;
  d->pending_addr = 0;

  // Reset USB control register first (SDK pattern)
  USB_CTRL(base) = 0x00;

  // Init control registers
  USB_CTRL(base) = CH58X_UC_DEV_PU_EN | CH58X_UC_INT_BUSY | CH58X_UC_DMA_EN;
  USB_UDEV_CTRL(base) = CH58X_UD_PD_DIS | CH58X_UD_PORT_EN;
  USB_DEV_AD(base) = 0x00;

  // Clear all interrupt flags, then enable interrupts
  USB_INT_FG(base) = 0xFF;
  USB_INT_EN(base) = CH58X_UIE_BUS_RST | CH58X_UIE_TRANSFER | CH58X_UIE_SUSPEND;

  // EP0 setup (also sets EP4 DMA since they share)
  EP_DMA(base, 0) = (uint16_t)(uint32_t) &d->ep0_buffer[0];
  EP_TLEN(base, 0) = 0;
  EP_CTRL(base, 0) = CH58X_EP_R_RES_ACK | CH58X_EP_T_RES_NAK;

  // Enable all endpoints TX+RX
  CH58X_UEP4_1_MOD(base) = CH58X_UEP1_RX_EN | CH58X_UEP1_TX_EN |
                             CH58X_UEP4_RX_EN | CH58X_UEP4_TX_EN;
  CH58X_UEP2_3_MOD(base) = CH58X_UEP2_RX_EN | CH58X_UEP2_TX_EN |
                             CH58X_UEP3_RX_EN | CH58X_UEP3_TX_EN;
  CH58X_UEP567_MOD(base) = CH58X_UEP5_RX_EN | CH58X_UEP5_TX_EN |
                             CH58X_UEP6_RX_EN | CH58X_UEP6_TX_EN |
                             CH58X_UEP7_RX_EN | CH58X_UEP7_TX_EN;

  // EP1-3: DMA + manual toggle + NAK both directions
  for (uint8_t ep = 1; ep <= 3; ep++) {
    EP_DMA(base, ep) = (uint16_t)(uint32_t) ep_dma_buffer(d, ep);
    EP_TLEN(base, ep) = 0;
    EP_CTRL(base, ep) = CH58X_EP_R_RES_NAK | CH58X_EP_T_RES_NAK;
  }

  // EP4: no independent DMA, no auto-toggle
  EP_TLEN(base, 4) = 0;
  EP_CTRL(base, 4) = CH58X_EP_R_RES_NAK | CH58X_EP_T_RES_NAK;

  // EP5-7: DMA + manual toggle
  for (uint8_t ep = 5; ep <= 7; ep++) {
    EP_DMA(base, ep) = (uint16_t)(uint32_t) ep_dma_buffer(d, ep);
    EP_TLEN(base, ep) = 0;
    EP_CTRL(base, ep) = CH58X_EP_R_RES_NAK | CH58X_EP_T_RES_NAK;
  }

  // Set EP0 max size
  d->xfer[0][TUSB_DIR_OUT].max_size = EP_BUF_SIZE;
  d->xfer[0][TUSB_DIR_IN].max_size = EP_BUF_SIZE;

  dcd_connect(rhport);
  return true;
}

void dcd_int_handler(uint8_t rhport) {
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = d->usb_base;
  uint8_t status = USB_INT_FG(base);

  if (status & CH58X_UIF_TRANSFER) {
    uint8_t int_st = USB_INT_ST(base);
    uint8_t ep = CH58X_INT_ST_ENDP(int_st);
    uint8_t token = CH58X_INT_ST_TOKEN(int_st);

    // Process regular token before SETUP to avoid losing it
    if (token != CH58X_PID_SETUP) {
      switch (token) {
        case CH58X_PID_OUT: {
          uint8_t rx_len = USB_RX_LEN(base);
          if (ep == 0) {
            // EP0: manual toggle, always process
            EP_CTRL(base, 0) ^= CH58X_EP_R_TOG;
            update_out(rhport, 0, rx_len, true);
          } else if (int_st & CH58X_UIS_TOG_OK) {
            // Toggle OK: manual toggle and process
            EP_CTRL(base, ep) ^= CH58X_EP_R_TOG;
            update_out(rhport, ep, rx_len, true);
          }
          // else: toggle mismatch, discard
          break;
        }

        case CH58X_PID_IN: {
          // Apply pending Set Address immediately after status ZLP
          if (ep == 0 && d->pending_addr) {
            USB_DEV_AD(base) = (USB_DEV_AD(base) & CH58X_UDA_GP_BIT) |
                               (d->pending_addr & CH58X_USB_ADDR_MASK);
            d->pending_addr = 0;
          }
          if (ep != 0) {
            // Manual toggle for all non-EP0 endpoints
            EP_CTRL(base, ep) ^= CH58X_EP_T_TOG;
          }
          update_in(rhport, ep, false, true);
          break;
        }

        default:
          break;
      }
      USB_INT_FG(base) = CH58X_UIF_TRANSFER;
    }

    // SETUP_ACT is checked separately — it persists even if token field changed
    if (int_st & CH58X_UIS_SETUP_ACT) {
      // Reset toggles to DATA1, NAK both directions until stack is ready
      EP_CTRL(base, 0) = CH58X_EP_R_TOG | CH58X_EP_T_TOG |
                         CH58X_EP_R_RES_NAK | CH58X_EP_T_RES_NAK;
      d->ep0_tog = true;

      d->pending_addr = 0;

      // Mark stale EP0 completion so dcd_edpt_xfer can skip it
      d->setup_pending = d->ep0_completion_pending ? 1 : 0;
      d->ep0_completion_pending = false;
      d->xfer[0][TUSB_DIR_OUT].valid = false;
      d->xfer[0][TUSB_DIR_IN].valid = false;

      dcd_event_setup_received(rhport, ep_out_buffer(d, 0), true);
      USB_INT_FG(base) = CH58X_UIF_TRANSFER;
    }
  }

  // Process bus reset: reset all endpoints immediately (matching WCH SDK pattern)
  if (status & CH58X_UIF_BUS_RST) {
    d->ep0_tog = true;
    d->setup_pending = 0;
    d->ep0_completion_pending = false;
    d->pending_addr = 0;

    // Reset EP0: ACK for RX (ready for SETUP), NAK for TX
    EP_CTRL(base, 0) = CH58X_EP_R_RES_ACK | CH58X_EP_T_RES_NAK;
    EP_TLEN(base, 0) = 0;
    d->xfer[0][TUSB_DIR_OUT].max_size = EP_BUF_SIZE;
    d->xfer[0][TUSB_DIR_IN].max_size = EP_BUF_SIZE;

    // Reset EP1-7: NAK both directions, invalidate pending transfers
    for (uint8_t ep = 1; ep < EP_MAX; ep++) {
      d->xfer[ep][TUSB_DIR_IN].valid = false;
      d->xfer[ep][TUSB_DIR_OUT].valid = false;
      EP_CTRL(base, ep) = CH58X_EP_R_RES_NAK | CH58X_EP_T_RES_NAK;
      EP_TLEN(base, ep) = 0;
    }

    USB_DEV_AD(base) = 0x00;

    tusb_speed_t speed = (USB_CTRL(base) & CH58X_UC_LOW_SPEED) ?
                          TUSB_SPEED_LOW : TUSB_SPEED_FULL;
    dcd_event_bus_reset(rhport, speed, true);

    USB_INT_FG(base) = CH58X_UIF_BUS_RST;
  }

  // Process suspend/resume
  if (status & CH58X_UIF_SUSPEND) {
    dcd_event_bus_signal(rhport,
      (USB_MIS_ST(base) & CH58X_UMS_SUSPEND) ? DCD_EVENT_SUSPEND : DCD_EVENT_RESUME,
      true);
    USB_INT_FG(base) = CH58X_UIF_SUSPEND;
  }
}

void dcd_int_enable(uint8_t rhport) {
  uint8_t irqn = (rhport == 0) ? CH58X_USB_IRQn : CH58X_USB2_IRQn;
  CH58X_PFIC_IENR[irqn / 32] = (1u << (irqn % 32));
}

void dcd_int_disable(uint8_t rhport) {
  uint8_t irqn = (rhport == 0) ? CH58X_USB_IRQn : CH58X_USB2_IRQn;
  CH58X_PFIC_IRER[irqn / 32] = (1u << (irqn % 32));
  __asm volatile ("fence.i");
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  // Defer to ISR: apply address right after status ZLP is ACK'd
  _dcd_data[rhport].pending_addr = dev_addr;
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false);
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void) rhport;
  // TODO: not supported
}

void dcd_connect(uint8_t rhport) {
  uint32_t base = get_usb_base(rhport);
  USB_CTRL(base) |= CH58X_UC_DEV_PU_EN;
}

void dcd_disconnect(uint8_t rhport) {
  uint32_t base = get_usb_base(rhport);
  USB_CTRL(base) &= ~CH58X_UC_DEV_PU_EN;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void) rhport;
  (void) en;
}

void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const* request) {
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = d->usb_base;

  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_ADDRESS) {
    // Safety net: re-apply address in case ISR path was skipped
    USB_DEV_AD(base) = (USB_DEV_AD(base) & CH58X_UDA_GP_BIT) |
                       ((uint8_t)request->wValue & CH58X_USB_ADDR_MASK);
  }

  dcd_int_disable(rhport);
  d->ep0_completion_pending = false;
  if (d->setup_pending) {
    // SETUP already arrived — don't override its NAK with ACK
    d->setup_pending = 0;
  } else {
    ep_set_both_response(base, 0, CH58X_EP_T_RES_NAK, CH58X_EP_R_RES_ACK);
  }
  dcd_int_enable(rhport);
}

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const* desc_ep) {
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = d->usb_base;
  uint8_t ep = tu_edpt_number(desc_ep->bEndpointAddress);
  uint8_t dir = tu_edpt_dir(desc_ep->bEndpointAddress);
  TU_ASSERT(ep < EP_MAX);

  d->isochronous[ep][dir] = (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  uint16_t max_size = tu_edpt_packet_size(desc_ep);
  if (max_size > EP_BUF_SIZE) {
    max_size = EP_BUF_SIZE;
  }
  d->xfer[ep][dir].max_size = max_size;

  if (ep != 0) {
    dcd_int_disable(rhport);
    uint8_t ctrl = EP_CTRL(base, ep);
    if (dir == TUSB_DIR_OUT) {
      // Clear RX toggle to DATA0 per USB spec
      ctrl &= ~CH58X_EP_R_TOG;
      if (d->isochronous[ep][TUSB_DIR_OUT]) {
        ctrl = (ctrl & ~CH58X_EP_R_RES_MASK) | CH58X_EP_R_RES_TOUT;
      } else {
        // Start with NAK; dcd_edpt_xfer will set ACK when a transfer is submitted
        ctrl = (ctrl & ~CH58X_EP_R_RES_MASK) | CH58X_EP_R_RES_NAK;
      }
    } else {
      // Clear TX toggle to DATA0 per USB spec
      ctrl &= ~CH58X_EP_T_TOG;
      EP_TLEN(base, ep) = 0;
      ctrl = (ctrl & ~CH58X_EP_T_RES_MASK) | CH58X_EP_T_RES_NAK;
    }
    EP_CTRL(base, ep) = ctrl;
    dcd_int_enable(rhport);
  }
  return true;
}

void dcd_edpt_close_all(uint8_t rhport) {
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = d->usb_base;

  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    d->xfer[ep][TUSB_DIR_IN].valid = false;
    d->xfer[ep][TUSB_DIR_OUT].valid = false;
    d->isochronous[ep][TUSB_DIR_OUT] = false;
    d->isochronous[ep][TUSB_DIR_IN] = false;
    EP_CTRL(base, ep) = CH58X_EP_R_RES_NAK | CH58X_EP_T_RES_NAK;
  }
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  (void) rhport;
  (void) ep_addr;
  (void) largest_packet_size;
  return false;
}

bool dcd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t* desc_ep) {
  (void) rhport;
  (void) desc_ep;
  return false;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer,
                   uint16_t total_bytes, bool is_isr) {
  uint8_t ep = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);

  dcd_data_t* d = &_dcd_data[rhport];
  xfer_ctl_t* xfer = &d->xfer[ep][dir];

  dcd_int_disable(rhport);

  // Skip stale EP0 arm if a new SETUP has already arrived
  if (ep == 0) {
    d->ep0_completion_pending = false;
    if (d->setup_pending) {
      d->setup_pending = 0;
      dcd_int_enable(rhport);
      return true;
    }
  }

  xfer->valid = true;
  xfer->buffer = buffer;
  xfer->len = total_bytes;
  xfer->processed_len = 0;

  if (dir == TUSB_DIR_IN) {
    update_in(rhport, ep, true, is_isr);
  } else {
    if (d->isochronous[ep][TUSB_DIR_OUT]) {
      ep_set_rx_response(d->usb_base, ep, CH58X_EP_R_RES_TOUT);
    } else {
      ep_set_rx_response(d->usb_base, ep, CH58X_EP_R_RES_ACK);
    }
  }
  dcd_int_enable(rhport);
  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = d->usb_base;
  uint8_t ep = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);

  dcd_int_disable(rhport);
  if (ep == 0) {
    // EP0: stall both directions
    EP_CTRL(base, 0) = CH58X_EP_R_RES_STALL | CH58X_EP_T_RES_STALL |
                        CH58X_EP_R_TOG | CH58X_EP_T_TOG;
  } else {
    if (dir == TUSB_DIR_OUT) {
      ep_set_rx_response(base, ep, CH58X_EP_R_RES_STALL);
    } else {
      ep_set_tx_response(base, ep, CH58X_EP_T_RES_STALL);
    }
  }
  dcd_int_enable(rhport);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  dcd_data_t* d = &_dcd_data[rhport];
  uint32_t base = d->usb_base;
  uint8_t ep = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);

  dcd_int_disable(rhport);
  if (ep == 0) {
    if (dir == TUSB_DIR_OUT) {
      ep_set_rx_response(base, 0, CH58X_EP_R_RES_ACK);
    }
  } else {
    uint8_t ctrl = EP_CTRL(base, ep);
    if (dir == TUSB_DIR_OUT) {
      ctrl &= ~(CH58X_EP_R_RES_MASK | CH58X_EP_R_TOG);
      ctrl |= CH58X_EP_R_RES_ACK;
    } else {
      ctrl &= ~(CH58X_EP_T_RES_MASK | CH58X_EP_T_TOG);
      ctrl |= CH58X_EP_T_RES_NAK;
    }
    EP_CTRL(base, ep) = ctrl;
  }
  dcd_int_enable(rhport);
}

#endif /* CFG_TUD_ENABLED && TUP_USBIP_WCH_CH58X */
