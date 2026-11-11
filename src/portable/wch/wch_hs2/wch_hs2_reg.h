/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef WCH_HS2_REG_H
#define WCH_HS2_REG_H

#include <stddef.h>
#include <stdint.h>

#include "common/tusb_compiler.h"

// Device register block of the WCH HS2 controller (CH32H417, CH32V4x7, CH32V205, CH32X3x5,
// CH584-587). Layout and bits from CH32V407RM Rev 1.3 section 22.2.1; SDK-independent because
// vendor SDKs name this block differently per chip (USBHSD_TypeDef vs R8_USB2_* on CH585).
typedef struct {
  volatile uint16_t tx_len;  // UEPn_T_LEN
  volatile uint8_t  tx_ctrl; // UEPn_TX_CTRL
  volatile uint8_t  rx_ctrl; // UEPn_RX_CTRL
} wch_hs2_ep_ctrl_t;

typedef struct {
  volatile uint16_t rx_len;  // UEPn_RX_LEN
  volatile uint16_t rx_size; // UEPn_RX_SIZE (reserved for EP0)
} wch_hs2_ep_rx_t;

typedef struct {
  volatile uint8_t  CONTROL;         // 0x00
  volatile uint8_t  BASE_MODE;       // 0x01
  volatile uint8_t  INT_EN;          // 0x02
  volatile uint8_t  DEV_AD;          // 0x03
  volatile uint8_t  WAKE_CTRL;       // 0x04
  volatile uint8_t  TEST_MODE;       // 0x05
  volatile uint16_t LPM_DATA;        // 0x06
  volatile uint8_t  INT_FG;          // 0x08
  volatile uint8_t  INT_ST;          // 0x09
  volatile uint8_t  MIS_ST;          // 0x0A
  uint8_t           reserved0;       // 0x0B
  volatile uint16_t FRAME_NO;        // 0x0C
  volatile uint16_t BUS;             // 0x0E
  volatile uint16_t UEP_TX_EN;       // 0x10
  volatile uint16_t UEP_RX_EN;       // 0x12
  volatile uint16_t UEP_T_TOG_AUTO;  // 0x14
  volatile uint16_t UEP_R_TOG_AUTO;  // 0x16
  volatile uint8_t  UEP_T_BURST;     // 0x18
  volatile uint8_t  UEP_T_BURST_MODE;// 0x19
  volatile uint8_t  UEP_R_BURST;     // 0x1A
  volatile uint8_t  UEP_R_RES_MODE;  // 0x1B
  volatile uint32_t UEP_AF_MODE;     // 0x1C
  volatile uint32_t UEP0_DMA;        // 0x20
  volatile uint32_t UEP_RX_DMA[7];   // 0x24 EP1..EP7
  volatile uint32_t UEP_TX_DMA[7];   // 0x40 EP1..EP7
  volatile uint32_t UEP_MAX_LEN[8];  // 0x5C EP0..EP7
  wch_hs2_ep_rx_t   UEP_RX[8];       // 0x7C EP0..EP7
  wch_hs2_ep_ctrl_t UEP_CTRL[8];     // 0x9C EP0..EP7
  volatile uint16_t UEP_T_ISO;       // 0xBC
  volatile uint16_t UEP_R_ISO;       // 0xBE
} wch_hs2_dev_t;

#define WCH_HS2_OFFSET_CHECK(_field, _ofs) \
  TU_VERIFY_STATIC(offsetof(wch_hs2_dev_t, _field) == (_ofs), "wch_hs2 " #_field " offset")

WCH_HS2_OFFSET_CHECK(LPM_DATA, 0x06);
WCH_HS2_OFFSET_CHECK(INT_FG, 0x08);
WCH_HS2_OFFSET_CHECK(FRAME_NO, 0x0C);
WCH_HS2_OFFSET_CHECK(UEP_TX_EN, 0x10);
WCH_HS2_OFFSET_CHECK(UEP_AF_MODE, 0x1C);
WCH_HS2_OFFSET_CHECK(UEP0_DMA, 0x20);
WCH_HS2_OFFSET_CHECK(UEP_RX_DMA, 0x24);
WCH_HS2_OFFSET_CHECK(UEP_TX_DMA, 0x40);
WCH_HS2_OFFSET_CHECK(UEP_MAX_LEN, 0x5C);
WCH_HS2_OFFSET_CHECK(UEP_RX, 0x7C);
WCH_HS2_OFFSET_CHECK(UEP_CTRL, 0x9C);
WCH_HS2_OFFSET_CHECK(UEP_T_ISO, 0xBC);
TU_VERIFY_STATIC(sizeof(wch_hs2_dev_t) == 0xC0, "wch_hs2 device block size");

// R8_USB_CTRL
#define WCH_HS2_UD_RST_LINK     0x01
#define WCH_HS2_UD_RST_SIE      0x02
#define WCH_HS2_UD_CLR_ALL      0x04
#define WCH_HS2_UD_PHY_SUSPENDM 0x08
#define WCH_HS2_UD_DMA_EN       0x10
#define WCH_HS2_UD_DEV_EN       0x20
#define WCH_HS2_UD_LPM_EN       0x80

// R8_USB_BASE_MODE
#define WCH_HS2_UD_SPEED_FULL 0x00
#define WCH_HS2_UD_SPEED_HIGH 0x01

// R8_USB_INT_EN
#define WCH_HS2_UDIE_BUS_RST   0x01
#define WCH_HS2_UDIE_SUSPEND   0x02
#define WCH_HS2_UDIE_BUS_SLEEP 0x04
#define WCH_HS2_UDIE_LPM_ACT   0x08
#define WCH_HS2_UDIE_TRANSFER  0x10
#define WCH_HS2_UDIE_SOF_ACT   0x20
#define WCH_HS2_UDIE_LINK_RDY  0x40

// R8_USB_INT_FG: write 1 to clear, except TRANSFER (RTX_ACT, cleared by the endpoint DONE bits)
#define WCH_HS2_UDIF_BUS_RST  0x01
#define WCH_HS2_UDIF_SUSPEND  0x02
#define WCH_HS2_UDIF_TRANSFER 0x10
#define WCH_HS2_UDIF_RX_SOF   0x20
#define WCH_HS2_UDIF_LINK_RDY 0x40

// R8_USB_MIS_ST
#define WCH_HS2_UDMS_READY   0x01
#define WCH_HS2_UDMS_SUSPEND 0x02
#define WCH_HS2_UDMS_HS_MOD  0x80

// R8_USB_WAKE_CTRL
#define WCH_HS2_UD_REMOTE_WKUP 0x01

// R16_USB_FRAME_NO
#define WCH_HS2_UD_FRAME_NO 0x07FF

// UEPn_TX_CTRL: DONE is RW0 (write 0 clears)
#define WCH_HS2_UEP_T_RES_MASK  0x03
#define WCH_HS2_UEP_T_RES_NAK   0x00
#define WCH_HS2_UEP_T_RES_STALL 0x01
#define WCH_HS2_UEP_T_RES_ACK   0x02
#define WCH_HS2_UEP_T_TOG_MASK  0x0C
#define WCH_HS2_UEP_T_TOG_DATA0 0x00
#define WCH_HS2_UEP_T_TOG_DATA1 0x04
#define WCH_HS2_UEP_T_DONE      0x80

// UEPn_RX_CTRL: DONE is RW0. EP1-7 toggle is bits 3:2; EP0 toggle is bit 2 only and bit 3 is the
// read-only SETUP flag (RM 22.2.1.32 / 22.2.1.35).
#define WCH_HS2_UEP_R_RES_MASK  0x03
#define WCH_HS2_UEP_R_RES_NAK   0x00
#define WCH_HS2_UEP_R_RES_STALL 0x01
#define WCH_HS2_UEP_R_RES_ACK   0x02
#define WCH_HS2_UEP_R_TOG_MASK  0x0C
#define WCH_HS2_UEP0_R_TOG_MASK 0x04
#define WCH_HS2_UEP_R_TOG_DATA0 0x00
#define WCH_HS2_UEP_R_TOG_DATA1 0x04
#define WCH_HS2_UEP_R_SETUP_IS  0x08
#define WCH_HS2_UEP_R_TOG_MATCH 0x10
#define WCH_HS2_UEP_R_DONE      0x80

#endif
