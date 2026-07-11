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

#ifndef USB_CH32H417_USBHS_REG_H
#define USB_CH32H417_USBHS_REG_H

// USBHSD_TypeDef (device block @0x40030000) and USBHS_* bit macros come from the SDK.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "ch32h417.h"
#include "ch32h417_usb.h"
#pragma GCC diagnostic pop

// The per-endpoint registers are named fields in USBHSD_TypeDef, not arrays: reach EPn from EP0
// by pointer arithmetic. Verify the strides against the SDK struct so a header change is caught.
TU_VERIFY_STATIC(offsetof(USBHSD_TypeDef, UEP1_RX_DMA) - offsetof(USBHSD_TypeDef, UEP0_DMA) == 4, "RX DMA stride");
TU_VERIFY_STATIC(offsetof(USBHSD_TypeDef, UEP1_TX_DMA) - offsetof(USBHSD_TypeDef, UEP1_RX_DMA) == 7 * 4, "TX DMA block");
TU_VERIFY_STATIC(offsetof(USBHSD_TypeDef, UEP1_MAX_LEN) - offsetof(USBHSD_TypeDef, UEP0_MAX_LEN) == 4, "MAX_LEN stride");
TU_VERIFY_STATIC(offsetof(USBHSD_TypeDef, UEP1_RX_LEN) - offsetof(USBHSD_TypeDef, UEP0_RX_LEN) == 4, "RX_LEN stride");
TU_VERIFY_STATIC(offsetof(USBHSD_TypeDef, UEP1_TX_LEN) - offsetof(USBHSD_TypeDef, UEP0_TX_LEN) == 4, "TX_LEN/CTRL stride");
TU_VERIFY_STATIC(offsetof(USBHSD_TypeDef, UEP0_TX_CTRL) - offsetof(USBHSD_TypeDef, UEP0_TX_LEN) == 2, "TX_CTRL offset");
TU_VERIFY_STATIC(offsetof(USBHSD_TypeDef, UEP0_RX_CTRL) - offsetof(USBHSD_TypeDef, UEP0_TX_LEN) == 3, "RX_CTRL offset");

// EP0..EP7 with the TX group (TX_LEN u16, TX_CTRL u8, RX_CTRL u8) at 4-byte stride, the RX/TX DMA
// pointers in their own contiguous blocks, and per-EP MAX_LEN / RX_LEN at 4-byte stride. Index
// from each field's own base pointer (matching type) so no cast increases alignment.
#define EP_TX_LEN(ep)      (*((volatile uint16_t*)&USBHSD->UEP0_TX_LEN + 2u * (ep)))
#define EP_TX_CTRL(ep)     (*((volatile uint8_t*)&USBHSD->UEP0_TX_CTRL + 4u * (ep)))
#define EP_RX_CTRL(ep)     (*((volatile uint8_t*)&USBHSD->UEP0_RX_CTRL + 4u * (ep)))
#define EP_MAX_LEN(ep)     (*((volatile uint32_t*)&USBHSD->UEP0_MAX_LEN + (ep)))
#define EP_RX_LEN(ep)      (*((volatile uint16_t*)&USBHSD->UEP0_RX_LEN + 2u * (ep)))
#define EP_RX_DMA(ep)      (*((volatile uint32_t*)&USBHSD->UEP1_RX_DMA + ((ep) - 1)))
#define EP_TX_DMA(ep)      (*((volatile uint32_t*)&USBHSD->UEP1_TX_DMA + ((ep) - 1)))

#endif
