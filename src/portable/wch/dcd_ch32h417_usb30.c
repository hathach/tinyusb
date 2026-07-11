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

// USB3.0 SuperSpeed device driver for the WCH CH32H417/CH32H416 USBSS controller
// (@0x40034000), documented in CH32H417RM-EN chapter 27. The LINK layer is the same IP as the
// CH569 (see dcd_ch56x_usb30.c) but the endpoint engine is a reworked chain-DMA design:
// hardware sequence numbers and ERDY, per-endpoint halt (RB_EP_TX/RX_HALT), per-chain
// completion flags. Software drives the LTSSM through the USBSS_LINK interrupt.
//
// With CFG_TUD_WCH_USB30_FALLBACK this dcd owns both controllers: TIM12 times SuperSpeed
// training and hands rhport 0 to the USB2 driver (ch32h417_usb2_* in dcd_ch32h417_usbhs.c)
// when the host has no SuperSpeed port.

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USB30_H417) && \
    defined(CFG_TUD_WCH_USBIP_USB30) && CFG_TUD_WCH_USBIP_USB30 == 1

#include "device/dcd.h"

// implementation lands with the SPEED=super bring-up stage

#endif
