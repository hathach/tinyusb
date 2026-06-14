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

#ifndef USBFS_GENERIC_H_
#define USBFS_GENERIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CH32_USBFS_SPECIAL_EP4 0

// TX_CTRL
#define USBFS_EP_T_RES_MASK  (3u << 0)
#define USBFS_EP_T_TOG       (1u << 2)
#define USBFS_EP_T_AUTO_TOG  (1u << 3)

#define USBFS_EP_T_RES_ACK   (0u << 0)
#define USBFS_EP_T_RES_NYET  (1u << 0)
#define USBFS_EP_T_RES_NAK   (2u << 0)
#define USBFS_EP_T_RES_STALL (3u << 0)

// RX_CTRL
#define USBFS_EP_R_RES_MASK  (3u << 0)
#define USBFS_EP_R_TOG       (1u << 2)
#define USBFS_EP_R_AUTO_TOG  (1u << 3)

#define USBFS_EP_R_RES_ACK   (0u << 0)
#define USBFS_EP_R_RES_NYET  (1u << 0)
#define USBFS_EP_R_RES_NAK   (2u << 0)
#define USBFS_EP_R_RES_STALL (3u << 0)

#define EP_DMA(ep)     ((&USBOTG_FS->UEP0_DMA)[ep])
#define EP_TX_LEN(ep)  ((&USBOTG_FS->UEP0_TX_LEN)[2 * (ep)])
#define EP_TX_CTRL(ep) ((&USBOTG_FS->UEP0_TX_CTRL)[4 * (ep)])
#define EP_RX_CTRL(ep) ((&USBOTG_FS->UEP0_RX_CTRL)[4 * (ep)])

#define EP_TX_CTRL_WRITE(ep, value) (EP_TX_CTRL(ep) = (value))
#define EP_RX_CTRL_WRITE(ep, value) (EP_RX_CTRL(ep) = (value))

#ifdef __cplusplus
}
#endif

#endif
