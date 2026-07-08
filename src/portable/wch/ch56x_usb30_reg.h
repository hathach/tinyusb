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

// CH565/CH569 USB3.0 SuperSpeed device controller (USBSS @0x40008000) bit definitions.
// WCH does not document these registers; the values below were established by the hydrausb3
// project (wch-ch56x-bsp / wch-ch56x-lib, Apache-2.0, reverse engineered from WCH's binary
// USB3 library) and cross-checked against the register-compatible LINK layer of the newer
// CH32H417 (officially documented in CH32H417RM Chapter 27, whose R32_LINK_* block matches
// the CH569 byte for byte). The endpoint engine is CH569-specific.

#ifndef TUSB_CH56X_USB30_REG_H_
#define TUSB_CH56X_USB30_REG_H_

#include <stddef.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <CH56xSFR.h>
#include <core_riscv.h>
#pragma GCC diagnostic pop

// Guard against dep header drift: offsets verified against hydrausb3 CH56xSFR.h and the
// CH32H417RM LINK-layer register map
TU_VERIFY_STATIC(offsetof(USBSS_TypeDef, LINK_INT_FLAG) == 0x0C, "usbss layout");
TU_VERIFY_STATIC(offsetof(USBSS_TypeDef, USB_CONTROL) == 0x20, "usbss layout");
TU_VERIFY_STATIC(offsetof(USBSS_TypeDef, USB_FC_CTRL) == 0x44, "usbss layout");
TU_VERIFY_STATIC(offsetof(USBSS_TypeDef, UEP_CFG) == 0x70, "usbss layout");
TU_VERIFY_STATIC(offsetof(USBSS_TypeDef, UEP0_DMA) == 0x74, "usbss layout");
TU_VERIFY_STATIC(offsetof(USBSS_TypeDef, UEP2_RX_DMA) == 0x90, "usbss layout");
TU_VERIFY_STATIC(offsetof(USBSS_TypeDef, UEP7_TX_CTRL) == 0xEC, "usbss layout");

//--------------------------------------------------------------------+
// LINK layer (official bit names per CH32H417RM 27.2.6, same IP)
//--------------------------------------------------------------------+

// R32_LINK_CFG
#define USBSS_LINK_CFG_RX_TERM_EN   (1u << 1)  // RX termination enable (device present)
#define USBSS_LINK_CFG_PIPE_RESET   (1u << 3)
#define USBSS_LINK_CFG_LFPS_RX_PD   (1u << 5)
#define USBSS_LINK_CFG_RX_EQ_EN     (1u << 6)
#define USBSS_LINK_CFG_TX_DEEMPH    (1u << 8)
#define USBSS_LINK_CFG_INIT         0x140u     // RX_EQ_EN | TX_DEEMPH (init value)

// R32_LINK_CTRL
#define USBSS_LINK_CTRL_PM_MASK     0x3u       // power mode: 0=U0 1=U1 2=U2 3=U3/off
#define USBSS_LINK_CTRL_GO_DISABLED (1u << 4)
#define USBSS_LINK_CTRL_TX_WARM_RST (1u << 8)
#define USBSS_LINK_CTRL_TX_UX_EXIT  (1u << 9)
#define USBSS_LINK_CTRL_POLLING_EN  (1u << 12)
#define USBSS_LINK_CTRL_TX_HOT_RST  (1u << 16)

// R32_LINK_STATUS
#define USBSS_LINK_STATUS_PRESENT   (1u << 0)  // RX terminations detected (partner present)
#define USBSS_LINK_STATUS_RX_WARM   (1u << 1)  // receiving warm reset
#define USBSS_LINK_STATUS_BUSY      (1u << 2)  // link busy; poll 0 before/after config

// R32_LINK_INT_CTRL / R32_LINK_INT_FLAG (write 1 to clear)
#define USBSS_LINK_IF_RDY           (1u << 0)  // link trained to U0 (polling handshake done)
#define USBSS_LINK_IF_RECOV         (1u << 1)
#define USBSS_LINK_IF_INACT         (1u << 2)
#define USBSS_LINK_IF_DISABLE       (1u << 3)  // LTSSM entered disabled (e.g. no SS host)
#define USBSS_LINK_IF_GO_U3         (1u << 4)
#define USBSS_LINK_IF_GO_U2         (1u << 5)
#define USBSS_LINK_IF_GO_U1         (1u << 6)
#define USBSS_LINK_IF_GO_U0         (1u << 7)
#define USBSS_LINK_IF_U3_WAKE       (1u << 8)
#define USBSS_LINK_IF_UX_REJECT     (1u << 9)
#define USBSS_LINK_IF_TERM_PRESENT  (1u << 10) // far-end terminations appeared/changed
#define USBSS_LINK_IF_TXEQ          (1u << 11) // TX equalization done (polling)
#define USBSS_LINK_IF_UX_EXIT       (1u << 12)
#define USBSS_LINK_IF_WARM_RESET    (1u << 13)
#define USBSS_LINK_IF_U3_WAKEUP     (1u << 14)
#define USBSS_LINK_IF_HOT_RESET     (1u << 15)
#define USBSS_LINK_IF_RX_DET        (1u << 20)
#define USBSS_LINK_INT_EN_INIT      0x10bc7du  // RDY|INACT|DISABLE|GO_U3..U1|TERM|TXEQ|UX_EXIT|WARM|HOT|RX_DET

//--------------------------------------------------------------------+
// USB control / status (CH569 endpoint engine, reverse engineered)
//--------------------------------------------------------------------+

// USB_CONTROL: bit1 = clear all, bit2 = force reset, bit31 = hot-reset ack, [30:24] = device address
#define USBSS_USB_CTRL_ALL_CLR      (1u << 1)
#define USBSS_USB_CTRL_FORCE_RST    (1u << 2)
#define USBSS_USB_CTRL_HOT_RST_ACK  (1u << 31)
#define USBSS_USB_CTRL_INIT         0x30021u   // DMA/SIE enable + setup receive (undocumented)
#define USBSS_DEV_ADDR_SHIFT        24

// USB_STATUS
#define USBSS_USB_STATUS_EP_EVT     (1u << 0)  // endpoint transfer event
#define USBSS_USB_STATUS_LMP_EVT    (1u << 1)  // LMP received
#define USBSS_USB_STATUS_ITP_EVT    (1u << 3)  // isochronous timestamp packet
#define USBSS_USB_STATUS_CLEAR_INIT 0x13u      // W1C all pending at init
#define usbss_status_ep_num(st)     (((st) >> 8) & 7u)
#define usbss_status_ep_is_in(st)   (((st) >> 12) & 1u)

// UEP_CFG endpoint enables
#define USBSS_EP_R_EN(ep)           (1u << (ep))
#define USBSS_EP_T_EN(ep)           (1u << (8 + (ep)))
// Isochronous endpoint mode (per WCH's USB30 device blob, USB30_ISO_Setendp disassembly)
#define USBSS_EP_ISO_RX(ep)         (1u << (16 + (ep)))
#define USBSS_EP_ISO_TX(ep)         (1u << (24 + (ep)))

//--------------------------------------------------------------------+
// Endpoint TX/RX control word fields
//--------------------------------------------------------------------+
// TX: [10:0] length of last packet, [20:16] NUMP, [25:21] packet sequence,
//     [27:26] response (0=NRDY 1=ACK 2=STALL), [28] LPF (last packet flag / end of burst)
// RX: [15:0] length of last received packet, [20:16] NUMP, [25:21] sequence,
//     [27:26] response, [29] status-stage/partial-packet flag, [30] SETUP received

#define USBSS_EP_TX_LEN_MASK        0x7FFu
#define USBSS_EP_RX_LEN_MASK        0xFFFFu
#define USBSS_EP_NUMP_SHIFT         16
#define USBSS_EP_NUMP_MASK          (0x1Fu << 16)
#define USBSS_EP_SEQ_MASK           0x03E00000u
#define USBSS_EP_RES_NRDY           (0u << 26)
#define USBSS_EP_RES_ACK            (1u << 26)
#define USBSS_EP_RES_STALL          (2u << 26)
#define USBSS_EP_LPF                (1u << 28)
#define USBSS_EP_RX_STATUS_STAGE    (1u << 29)  // EP0: status stage arrived; EPn: partial last packet
#define USBSS_EP_RX_SETUP           (1u << 30)  // EP0: SETUP packet arrived

// EP0 arm literals (from the reference implementation)
#define USBSS_UEP0_TX_ARM           0x14010000u // LPF | ACK | NUMP=1 (+ length)
#define USBSS_UEP0_RX_ARM           0x04010000u // ACK | NUMP=1
#define USBSS_UEP0_STALL            0x08010000u // STALL | NUMP=1

// EP0..7 control word accessors (0x10 byte stride)
#define USBSS_RX_CTRL(ep)           ((&USBSS->UEP0_RX_CTRL)[(ep) * 4])
#define USBSS_TX_CTRL(ep)           ((&USBSS->UEP0_TX_CTRL)[(ep) * 4])

// Per-endpoint DMA address registers. EP2's RX/TX register order is swapped vs the others
TU_ATTR_ALWAYS_INLINE static inline volatile uint32_t *usbss_tx_dma(uint8_t ep) {
  return (ep == 2) ? &USBSS->UEP2_TX_DMA : (&USBSS->UEP1_TX_DMA + (ep - 1) * 4);
}
TU_ATTR_ALWAYS_INLINE static inline volatile uint32_t *usbss_rx_dma(uint8_t ep) {
  return (ep == 2) ? &USBSS->UEP2_RX_DMA : (&USBSS->UEP1_RX_DMA + (ep - 1) * 4);
}

// USB_FC_CTRL: send ERDY for an endpoint so the host (re)issues the transaction
TU_ATTR_ALWAYS_INLINE static inline void usbss_send_erdy(uint8_t ep, bool dir_in, uint8_t nump) {
  USBSS->USB_FC_CTRL = ((uint32_t)(ep & 0xFu) << 2) | ((uint32_t)nump << 6) | (dir_in ? 3u : 1u);
}

//--------------------------------------------------------------------+
// LMP (link management packets), answered in software
//--------------------------------------------------------------------+
#define USBSS_LMP_SUBTYPE_MASK      0x1E0u
#define USBSS_LMP_PORT_CFG          0x0A0u     // received port configuration (subtype 5)
#define USBSS_LMP_PORT_CFG_RES      0x2C0u     // port configuration response (LINK_SPEED | subtype 6)
// PORT_CAPABILITY sent after link ready: DATA0 = LINK_SPEED|PORT_CAP, DATA1 = UP_STREAM|NUM_HP_BUF
#define USBSS_LMP_TX_PORT_CAP_DATA0 0x280u
#define USBSS_LMP_TX_PORT_CAP_DATA1 ((2u << 16) | 4u)

#endif // TUSB_CH56X_USB30_REG_H_
