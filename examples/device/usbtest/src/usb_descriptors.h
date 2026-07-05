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
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

// Device-capability tier advertised in bcdDevice low byte (0x01TT), read by the
// host script to select which usbtest cases to run:
//   1: bulk source/sink
//   2: + vendor control 0x5b/0x5c (ctrl_out)
//   3: + interrupt source/sink
//   4: + isochronous source/sink
#define USBTEST_TIER  4

// Interrupt/isochronous endpoint max packet sizes, must match the configuration descriptor. The
// CH32 USB IPs have tiny per-endpoint buffers so tier-4's six endpoints don't fit at the usual FS
// sizes: usbfs gives 64 B/ep (iso must drop to 64), and the CH32V20X fsdev port shares one 512 B
// PMA across every endpoint (needs iso 64 AND a small interrupt mps to fit alongside EP0+bulk+iso).
#if CFG_TUSB_MCU == OPT_MCU_CH32V20X && defined(CFG_TUD_WCH_USBIP_FSDEV) && CFG_TUD_WCH_USBIP_FSDEV
  #define USBTEST_INT_EP_MPS  16
  #define USBTEST_ISO_EP_MPS  32  // double-buffered on fsdev: 2x32=64/ep, same 512 B PMA budget
#elif TU_CHECK_MCU(OPT_MCU_CH32V20X, OPT_MCU_CH32V103, OPT_MCU_CH32F20X)
  #define USBTEST_INT_EP_MPS  (TUD_OPT_HIGH_SPEED ? 512 : 64)
  #define USBTEST_ISO_EP_MPS  (TUD_OPT_HIGH_SPEED ? 512 : 64)
#else
  #define USBTEST_INT_EP_MPS  (TUD_OPT_HIGH_SPEED ? 512 : 64)
  #define USBTEST_ISO_EP_MPS  (TUD_OPT_HIGH_SPEED ? 512 : 128)
#endif

#endif /* USB_DESCRIPTORS_H_ */
