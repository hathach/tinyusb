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

#ifndef USB_CH32H417_USB30_REG_H
#define USB_CH32H417_USB30_REG_H

// USBSSD_TypeDef (@0x40034000), LINK_*/USBSS_* bit macros come from the SDK. The LINK-layer bit
// names/semantics are documented in CH32H417RM-EN chapter 27 (the same LINK IP is used, less its
// register documentation, by the CH569).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "ch32h417.h"
#include "ch32h417_usb.h"
#include "ch32h417_tim.h" // TIM12 fallback backstop timer (TIM_IT_Update, TIM_CEN)
#pragma GCC diagnostic pop

// PHY configuration back-door (undocumented; values transcribed from the WCH EVT USBSS demo)
#define USBSS_PHY_CFG_CR   (*(volatile uint32_t*)0x400341F8)
#define USBSS_PHY_CFG_DAT  (*(volatile uint32_t*)0x400341FC)
#define USBSS_PHY_MISC_REG (*(volatile uint32_t*)0x5003C018)

// Link Management Packet (LMP) subtype / header fields (device = upstream port)
#define LMP_HP             0
#define LMP_SUBTYPE_MASK   (0xFu << 5)
#define LMP_SET_LINK_FUNC  (0x1u << 5)
#define LMP_U2_INACT_TOUT  (0x2u << 5)
#define LMP_PORT_CAP       (0x4u << 5)
#define LMP_PORT_CFG       (0x5u << 5)
#define LMP_PORT_CFG_RES   (0x6u << 5)
#define LMP_LINK_SPEED     (1u << 9)
#define LMP_NUM_HP_BUF     (4u << 0)
#define LMP_UP_STREAM      (2u << 16)

// Offset sanity checks against the SDK struct
TU_VERIFY_STATIC(offsetof(USBSSD_TypeDef, USB_CONTROL) == 0x70, "USB_CONTROL");
TU_VERIFY_STATIC(offsetof(USBSSD_TypeDef, USB_STATUS) == 0x74, "USB_STATUS");
TU_VERIFY_STATIC(offsetof(USBSSD_TypeDef, UEP0_TX_CTRL) == 0x84, "UEP0_TX_CTRL");
TU_VERIFY_STATIC(offsetof(USBSSD_TypeDef, EP1_TX) == 0xC0, "EP1_TX bank");

// The per-endpoint TX and RX register banks alternate in the struct (EP1_TX, EP1_RX, EP2_TX,
// EP2_RX, ...), each 16 bytes, so EPn's bank is 2 * (n-1) entries past EP1's.
TU_ATTR_ALWAYS_INLINE static inline volatile USBSS_EP_TX_TypeDef* usbss_ep_tx(uint8_t ep_num) {
  return (volatile USBSS_EP_TX_TypeDef*)&USBSSD->EP1_TX + 2u * (ep_num - 1u);
}
TU_ATTR_ALWAYS_INLINE static inline volatile USBSS_EP_RX_TypeDef* usbss_ep_rx(uint8_t ep_num) {
  return (volatile USBSS_EP_RX_TypeDef*)&USBSSD->EP1_RX + 2u * (ep_num - 1u);
}

// USB_STATUS fields: which endpoint/direction raised the transfer interrupt
#define USBSS_STATUS_EP_NUM(st) (((st) & USBSS_EP_ID_MASK) >> 8)
#define USBSS_STATUS_EP_IN(st)  (((st) & USBSS_EP_DIR_MASK) != 0)

#endif
