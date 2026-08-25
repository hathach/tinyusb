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
// Default is the full tier 4; a board whose DCD cannot serve a tier lowers it here
// (BOARD_<NAME> is defined by both build systems) and the host battery follows.
#ifndef USBTEST_TIER
  #if defined(BOARD_RA2A1_EK)
    // RA2A1's RUSB2 instance has no isochronous pipe (other RA parts have pipes 1-2)
    #define USBTEST_TIER  3
  #else
    #define USBTEST_TIER  4
  #endif
#endif

// Interrupt/isochronous endpoint max packet sizes, must match the configuration descriptor.
// TUD_OPT_HIGH_SPEED is a compile-time capability flag, NOT the live bus speed, so the full-speed
// config descriptor (and the OTHER_SPEED descriptor served to a HS host) must use full-speed-legal
// sizes regardless of it: interrupt <= 64 B, isochronous <= 1023 B (and both iso EPs must fit the
// 1023 B/frame FS periodic budget). Hence separate _FS / _HS descriptor sizes; the plain macro
// below is the compile-time capability maximum that sizes the source buffers (runtime write
// lengths follow the negotiated speed via tud_speed_get(), see main.c).
//
// The CH32 USB IPs have tiny per-endpoint buffers so tier-4's six endpoints don't fit at the usual
// FS sizes: usbfs gives 64 B/ep (iso must drop to 64), and the CH32V20X fsdev port shares one 512 B
// PMA across every endpoint (needs iso 32 AND a small interrupt mps to fit alongside EP0+bulk+iso).
#if CFG_TUSB_MCU == OPT_MCU_CH32V20X && defined(CFG_TUD_WCH_USBIP_FSDEV) && CFG_TUD_WCH_USBIP_FSDEV
  #define USBTEST_INT_EP_MPS_FS  16
  #define USBTEST_ISO_EP_MPS_FS  32  // double-buffered on fsdev: 2x32=64/ep, same 512 B PMA budget
#elif TU_CHECK_MCU(OPT_MCU_CH32V20X, OPT_MCU_CH32V103, OPT_MCU_CH32F20X, OPT_MCU_CH32V307, OPT_MCU_CH583)
  // WCH USBFS parts cap every endpoint (except EP3 IN) at 64 B. For the CH32V307 this applies to its
  // full-speed (usbfs) port; its high-speed (usbhs) port uses the _HS sizes below via desc_hs.
  #define USBTEST_INT_EP_MPS_FS  64
  #define USBTEST_ISO_EP_MPS_FS  64
#else
  #define USBTEST_INT_EP_MPS_FS  64
  #define USBTEST_ISO_EP_MPS_FS  128
#endif
// RUSB2 (Renesas RA) interrupt pipes 6-9 have a fixed 64-byte single buffer at any speed
// (RA6M5 UM R01UH0891 sec 29.1: "Pipes 6 to 9: Interrupt transfer with 64-byte single buffer").
#if TU_CHECK_MCU(OPT_MCU_RAXXX)
  #define USBTEST_INT_EP_MPS_HS  64
#else
  #define USBTEST_INT_EP_MPS_HS  512
#endif
#define USBTEST_ISO_EP_MPS_HS  512

// Compile-time capability maximum: sizes the source buffers / vendor epbufs for the largest
// packet the build can negotiate. Runtime write lengths follow tud_speed_get() (see main.c) —
// a high-speed build enumerated at full speed submits the _FS lengths.
#define USBTEST_INT_EP_MPS  (TUD_OPT_HIGH_SPEED ? USBTEST_INT_EP_MPS_HS : USBTEST_INT_EP_MPS_FS)
#define USBTEST_ISO_EP_MPS  (TUD_OPT_HIGH_SPEED ? USBTEST_ISO_EP_MPS_HS : USBTEST_ISO_EP_MPS_FS)

#endif /* USB_DESCRIPTORS_H_ */
