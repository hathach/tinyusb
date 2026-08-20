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

// Known-erratum quirk flags advertised in bcdDevice bits 4-7, read by the host script to
// skip cases the silicon cannot pass at SuperSpeed:
//   0x10: EP0 OUT data stages whose wLength % 4 == 1 are intermittently dropped (CH569 silicon
//         erratum, see the dcd_ch56x_usb30.c header; WCH's own binary USB3 stack fails
//         identically). Strike rate is host-timing dependent: an onboard AMD xHCI ran 15/15
//         clean (the quirk was briefly retired on that evidence), a Renesas uPD720201 fails
//         most ctrl_out runs (wire-confirmed: transaction error, full 489-byte residual)
//   0x20: a halted endpoint answers exactly one STALL TP per halt and cannot be re-armed to
//         repeat it (CH569, and the CH32H417 despite its RB_EP_TX/RX_HALT: the first
//         solicitation of a halted bulk endpoint gets a clean STALL TP, every later one gets no
//         response at all — the xHC retries, then reports a transaction error, so usbtest's
//         verify_still_halted sees -EPROTO instead of -EPIPE). On the H417 this was traced on
//         hardware: the halted endpoint raises no interrupt and changes no register across the
//         whole halted window, and neither re-asserting HALT, toggling it 1->0->1, clearing
//         FC_ST, requesting an ERDY, zeroing the endpoint sequence number, nor the vendor demo's
//         own `UEP_TX_CR |= HALT` (armed chain left in place) re-arms it — skip the ep-halt case
//   0x40: a SETUP arriving while a transfer completion is still unserviced gets no handshake at
//         all, so the host retries three times and fails the transfer with -EPROTO. The CH32H417
//         USBHS control register has no auto-busy bit: the CH569's USBHS has RB_USB_INT_BUSY,
//         "automatic responding busy for device mode ... during interrupt flag UIF_TRANSFER
//         valid" (CH56xSFR.h), which makes that same window merely NAK so the host retries
//         successfully -- and the CH569 passes 30/30 at high speed on identical usbd. RM 25.2.1.1
//         lists no equivalent bit for the H417 USBHS, though its USBFS block does have
//         USBFS_UC_INT_BUSY, so the feature exists on the part but not on this controller.
//         Wire-measured at ~34% of SETUPs dropped, rising to 63% under back-to-back control
//         traffic, and no software change reaches it: the drop happens before the ISR can clear
//         the completion. Ordinary class traffic survives on host retries (11/12 HIL examples
//         pass, CDC 5.8/6.5 MB/s, MSC 25/10 MB/s); only usbtest's control-stress cases fail.
#if TU_CHECK_MCU(OPT_MCU_CH32H417) && defined(CFG_TUD_WCH_USBIP_USBHS) && CFG_TUD_WCH_USBIP_USBHS
  #define USBTEST_QUIRKS  0x40
#elif TU_CHECK_MCU(OPT_MCU_CH569) && defined(CFG_TUD_WCH_USBIP_USB30) && CFG_TUD_WCH_USBIP_USB30
  #define USBTEST_QUIRKS  0x30
#elif TU_CHECK_MCU(OPT_MCU_CH32H417) && defined(CFG_TUD_WCH_USBIP_USB30) && CFG_TUD_WCH_USBIP_USB30
  #define USBTEST_QUIRKS  0x20
#else
  #define USBTEST_QUIRKS  0
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

// CH32H417 USBSS: the ISO-mode TX chain engine emits fixed 1024-byte DPs regardless of the
// armed CHAIN_LEN (hw-observed: with iso mps 512 a strict host — uPD720201 — flags every
// serviced interval as Babble Detected with 0 bytes accepted; the vendor's only USBSS iso
// demo, UVC, also runs mps 1024). Declare what the silicon sends. Other SS parts (CH569)
// keep the high-speed size, which their engines respect on the wire.
#if TU_CHECK_MCU(OPT_MCU_CH32H417)
  #define USBTEST_ISO_EP_MPS_SS  1024
#else
  #define USBTEST_ISO_EP_MPS_SS  USBTEST_ISO_EP_MPS_HS
#endif

// Compile-time capability maximum: sizes the source buffers / vendor epbufs for the largest
// packet the build can negotiate. Runtime write lengths follow tud_speed_get() (see main.c) —
// a high-speed build enumerated at full speed submits the _FS lengths.
#define USBTEST_INT_EP_MPS  (TUD_OPT_HIGH_SPEED ? USBTEST_INT_EP_MPS_HS : USBTEST_INT_EP_MPS_FS)
#define USBTEST_ISO_EP_MPS  (TUD_OPT_SUPER_SPEED ? USBTEST_ISO_EP_MPS_SS : \
                             (TUD_OPT_HIGH_SPEED ? USBTEST_ISO_EP_MPS_HS : USBTEST_ISO_EP_MPS_FS))

#endif /* USB_DESCRIPTORS_H_ */
