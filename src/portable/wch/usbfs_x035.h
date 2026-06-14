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

#ifndef USBFS_X035_H_
#define USBFS_X035_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <ch32x035_usb.h>

#define USBOTG_FS  USBFSD
#define USBHD_IRQn USBFS_IRQn

// CH32X035 has EP0-EP7, but EP4 is special:
// it shares UEP0_DMA with a fixed buffer layout, and it has no hardware auto-toggle so the
// this flags turns on driver toggling it in software
#define CH32_USBFS_SPECIAL_EP4 1

#define USBFS_EP_T_TOG       USBFS_UEP_T_TOG
#define USBFS_EP_T_AUTO_TOG  USBFS_UEP_T_AUTO_TOG
#define USBFS_EP_T_RES_MASK  USBFS_UEP_T_RES_MASK
#define USBFS_EP_T_RES_ACK   USBFS_UEP_T_RES_ACK
#define USBFS_EP_T_RES_NYET  USBFS_UEP_T_RES_NONE
#define USBFS_EP_T_RES_NAK   USBFS_UEP_T_RES_NAK
#define USBFS_EP_T_RES_STALL USBFS_UEP_T_RES_STALL

#define USBFS_EP_R_TOG       USBFS_UEP_R_TOG
#define USBFS_EP_R_AUTO_TOG  USBFS_UEP_R_AUTO_TOG
#define USBFS_EP_R_RES_MASK  USBFS_UEP_R_RES_MASK
#define USBFS_EP_R_RES_ACK   USBFS_UEP_R_RES_ACK
#define USBFS_EP_R_RES_NYET  USBFS_UEP_R_RES_NONE
#define USBFS_EP_R_RES_NAK   USBFS_UEP_R_RES_NAK
#define USBFS_EP_R_RES_STALL USBFS_UEP_R_RES_STALL

static inline volatile uint32_t *ch32_usbfs_ep_dma_reg(uint8_t ep) {
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

// There's a gap between EP4 and EP5 registers
#define EP_DMA(ep)     (*ch32_usbfs_ep_dma_reg(ep))
#define EP_TX_LEN(ep)  ((&USBOTG_FS->UEP0_TX_LEN)[2 * (ep) + ((ep) > 4 ? 24 : 0)])
#define EP_TX_CTRL(ep) ((&USBOTG_FS->UEP0_CTRL_H)[2 * (ep) + ((ep) > 4 ? 24 : 0)])
#define EP_RX_CTRL(ep) EP_TX_CTRL(ep)

// MASK are bits to preserve when writing one direction
#define EP_TX_CTRL_MASK (USBFS_EP_R_TOG | USBFS_EP_R_AUTO_TOG | USBFS_EP_R_RES_MASK)
#define EP_RX_CTRL_MASK (USBFS_EP_T_TOG | USBFS_EP_T_AUTO_TOG | USBFS_EP_T_RES_MASK)

#define EP_TX_CTRL_WRITE(ep, value) \
  (EP_TX_CTRL(ep) = (EP_TX_CTRL(ep) & EP_TX_CTRL_MASK) | (value))
#define EP_RX_CTRL_WRITE(ep, value) \
  (EP_RX_CTRL(ep) = (EP_RX_CTRL(ep) & EP_RX_CTRL_MASK) | (value))

#ifdef __cplusplus
}
#endif

#endif
