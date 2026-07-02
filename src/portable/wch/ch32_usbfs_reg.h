/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Matthew Tran
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ha Thach
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef USB_CH32_USBFS_REG_H
#define USB_CH32_USBFS_REG_H

// https://github.com/openwch/ch32v307/pull/90
// https://github.com/openwch/ch32v20x/pull/12
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

#if CFG_TUSB_MCU == OPT_MCU_CH32F20X
  #include <ch32f20x.h>
#elif CFG_TUSB_MCU == OPT_MCU_CH32V103
  #include <ch32v10x.h>
  // Newer-IP layout (separate UEPn_TX_CTRL/UEPn_RX_CTRL). The older IP (CH32V103) has a single
  // combined control register at the UEPn_TX_CTRL offset, with UEPn_RX_CTRL reserved; the union
  // exposes that same byte as UEPn_CTRL. Offsets are byte offsets from the peripheral base.
  // TODO unify into a single struct shared by all WCH USBFS parts.
  typedef struct
  {
    __IO uint8_t  BASE_CTRL;        // 0x00
    __IO uint8_t  UDEV_CTRL;        // 0x01
    __IO uint8_t  INT_EN;           // 0x02
    __IO uint8_t  DEV_ADDR;         // 0x03
    __IO uint8_t  Reserve0;         // 0x04
    __IO uint8_t  MIS_ST;           // 0x05
    __IO uint8_t  INT_FG;           // 0x06
    __IO uint8_t  INT_ST;           // 0x07
    __IO uint32_t RX_LEN;           // 0x08
    __IO uint8_t  UEP4_1_MOD;       // 0x0C
    __IO uint8_t  UEP2_3_MOD;       // 0x0D
    __IO uint8_t  UEP5_6_MOD;       // 0x0E
    __IO uint8_t  UEP7_MOD;         // 0x0F
    __IO uint32_t UEP0_DMA;         // 0x10
    __IO uint32_t UEP1_DMA;         // 0x14
    __IO uint32_t UEP2_DMA;         // 0x18
    __IO uint32_t UEP3_DMA;         // 0x1C
    __IO uint32_t UEP4_DMA;         // 0x20
    __IO uint32_t UEP5_DMA;         // 0x24
    __IO uint32_t UEP6_DMA;         // 0x28
    __IO uint32_t UEP7_DMA;         // 0x2C
    __IO uint16_t UEP0_TX_LEN;      // 0x30
    union {
      __IO uint8_t UEP0_TX_CTRL;
      __IO uint8_t UEP0_CTRL;
    };                              // 0x32 (TX_CTRL: IN | CTRL: combined)
    __IO uint8_t  UEP0_RX_CTRL;     // 0x33 (OUT ctrl; reserved on combined IP)
    __IO uint16_t UEP1_TX_LEN;      // 0x34
    union {
      __IO uint8_t UEP1_TX_CTRL;
      __IO uint8_t UEP1_CTRL;
    };                              // 0x36
    __IO uint8_t  UEP1_RX_CTRL;     // 0x37
    __IO uint16_t UEP2_TX_LEN;      // 0x38
    union {
      __IO uint8_t UEP2_TX_CTRL;
      __IO uint8_t UEP2_CTRL;
    };                              // 0x3A
    __IO uint8_t  UEP2_RX_CTRL;     // 0x3B
    __IO uint16_t UEP3_TX_LEN;      // 0x3C
    union {
      __IO uint8_t UEP3_TX_CTRL;
      __IO uint8_t UEP3_CTRL;
    };                              // 0x3E
    __IO uint8_t  UEP3_RX_CTRL;     // 0x3F
    __IO uint16_t UEP4_TX_LEN;      // 0x40
    union {
      __IO uint8_t UEP4_TX_CTRL;
      __IO uint8_t UEP4_CTRL;
    };                              // 0x42
    __IO uint8_t  UEP4_RX_CTRL;     // 0x43
    __IO uint16_t UEP5_TX_LEN;      // 0x44
    union {
      __IO uint8_t UEP5_TX_CTRL;
      __IO uint8_t UEP5_CTRL;
    };                              // 0x46
    __IO uint8_t  UEP5_RX_CTRL;     // 0x47
    __IO uint16_t UEP6_TX_LEN;      // 0x48
    union {
      __IO uint8_t UEP6_TX_CTRL;
      __IO uint8_t UEP6_CTRL;
    };                              // 0x4A
    __IO uint8_t  UEP6_RX_CTRL;     // 0x4B
    __IO uint16_t UEP7_TX_LEN;      // 0x4C
    union {
      __IO uint8_t UEP7_TX_CTRL;
      __IO uint8_t UEP7_CTRL;
    };                              // 0x4E
    __IO uint8_t  UEP7_RX_CTRL;     // 0x4F
    __IO uint32_t Reserve1;         // 0x50
    __IO uint32_t OTG_CR;           // 0x54
    __IO uint32_t OTG_SR;           // 0x58
  } USBOTG_FS_TypeDef;

  #define USBOTG_FS ((USBOTG_FS_TypeDef *) 0x40023400)

  // CH32V103 has the older USBFS IP: a single combined control register per endpoint
  // (UEPn_CTRL) instead of separate TX_CTRL/RX_CTRL bytes. The struct's UEPn_TX_CTRL field
  // aliases that combined register (same address); UEPn_RX_CTRL maps to unused padding.
  #define CH32_USBFS_EP_CTRL_COMBINED 1
#elif CFG_TUSB_MCU == OPT_MCU_CH32V20X
  #include <ch32v20x.h>
#elif CFG_TUSB_MCU == OPT_MCU_CH32V307
  #include <ch32v30x.h>
  #define USBHD_IRQn OTG_FS_IRQn
#elif CFG_TUSB_MCU == OPT_MCU_CH583
  #include "CH58x_common.h"
  // CH582/583 USBFS device controller: same combined per-endpoint control register as
  // CH32V103 (IN response bits[1:0], OUT response bits[3:2]) but a different register map -
  // the EP control/length block sits lower (EP0_CTRL @ +0x22), EP5-7 are split out, EP4
  // shares EP0's DMA buffer, and EP5/6/7 mode bits live in one UEP567_MOD. The control/status
  // block matches CH32. Two FS controllers exist (USB @ 0x40008000, USB2 @ 0x40008400); the
  // device uses USB0. Full register map per CH583/582 datasheet Table 17-2; the parameterized
  // EP_* macros below index off these named fields.
  #define CH58X_USBFS_BASE  0x40008000u
  // Per-endpoint register slots, 4-byte stride each; the EP_* macros index arrays of these.
  typedef struct {
    __IO uint16_t DMA;              // R16_UEPn_DMA: endpoint n buffer start address
    __IO uint16_t reserved;
  } ch58x_ep_dma_t;
  typedef struct {
    __IO uint8_t T_LEN;           // R8_UEPn_T_LEN (+0): transmit length
    __IO uint8_t reserved0;
    __IO uint8_t CTRL;            // R8_UEPn_CTRL  (+2): endpoint control
    __IO uint8_t reserved1;
  } ch58x_ep_ctrl_t;
  typedef struct {
    __IO uint8_t  BASE_CTRL;        // 0x00 R8_USB_CTRL
    __IO uint8_t  UDEV_CTRL;        // 0x01 R8_UDEV_CTRL
    __IO uint8_t  INT_EN;           // 0x02 R8_USB_INT_EN
    __IO uint8_t  DEV_ADDR;         // 0x03 R8_USB_DEV_AD
    __IO uint8_t  Reserve0;         // 0x04
    __IO uint8_t  MIS_ST;           // 0x05 R8_USB_MIS_ST
    __IO uint8_t  INT_FG;           // 0x06 R8_USB_INT_FG
    __IO uint8_t  INT_ST;           // 0x07 R8_USB_INT_ST
    __IO uint8_t  RX_LEN;           // 0x08 R8_USB_RX_LEN (8-bit on CH58X)
    __IO uint8_t  Reserve1[3];      // 0x09..0x0B
    __IO uint8_t  UEP4_1_MOD;       // 0x0C R8_UEP4_1_MOD
    __IO uint8_t  UEP2_3_MOD;       // 0x0D R8_UEP2_3_MOD
    __IO uint8_t  UEP567_MOD;       // 0x0E R8_UEP567_MOD
    __IO uint8_t    Reserve2;       // 0x0F
    ch58x_ep_dma_t  EP_DMA_0_3[4];      // 0x10 EP0-3 DMA  (EP4 has no DMA reg; it shares EP0's, index 0)
    ch58x_ep_ctrl_t EP_CTRL_0_4[5];     // 0x20 EP0-4 length/control
    __IO uint8_t    Reserve3[0x54u - 0x34u]; // 0x34..0x53
    ch58x_ep_dma_t  EP_DMA_5_7[3];      // 0x54 EP5-7 DMA
    __IO uint8_t    Reserve4[0x64u - 0x60u]; // 0x60..0x63
    ch58x_ep_ctrl_t EP_CTRL_5_7[3];     // 0x64 EP5-7 length/control
  } USBOTG_FS_TypeDef;
  #define USBOTG_FS  ((USBOTG_FS_TypeDef *) CH58X_USBFS_BASE)

  // 4-byte slot stride + these block offsets pin every EP register to its datasheet address.
  TU_VERIFY_STATIC(sizeof(ch58x_ep_dma_t)  == 4, "CH58x EP DMA slot must be 4 bytes");
  TU_VERIFY_STATIC(sizeof(ch58x_ep_ctrl_t) == 4, "CH58x EP ctrl slot must be 4 bytes");
  TU_VERIFY_STATIC(offsetof(USBOTG_FS_TypeDef, EP_DMA_0_3)  == 0x10, "CH58x EP_DMA_0_3 @0x10");
  TU_VERIFY_STATIC(offsetof(USBOTG_FS_TypeDef, EP_CTRL_0_4) == 0x20, "CH58x EP_CTRL_0_4 @0x20");
  TU_VERIFY_STATIC(offsetof(USBOTG_FS_TypeDef, EP_DMA_5_7)  == 0x54, "CH58x EP_DMA_5_7 @0x54");
  TU_VERIFY_STATIC(offsetof(USBOTG_FS_TypeDef, EP_CTRL_5_7) == 0x64, "CH58x EP_CTRL_5_7 @0x64");

  #define CH32_USBFS_EP_CTRL_COMBINED 1
  // CH58x's hardware AUTO_TOG does not stay in sync (notably across clear-stall and multi-packet
  // bulk transfers), causing data-toggle mismatch and bus resets. Drive the toggle manually in
  // the ISR instead. CH32V103/V20x/V307 keep AUTO_TOG (this macro is undefined for them).
  #define CH32_USBFS_EP_MANUAL_TOG    1
  // CH58x EP4 has no DMA register of its own: it overlays EP0's DMA region as
  // EP0[0:63] + EP4_OUT[64:127] + EP4_IN[128:191], so EP0 needs a 192-byte buffer.
  #define CH32_USBFS_EP4_SHARES_EP0   1
  #define USBHD_IRQn USB_IRQn
  #ifndef NVIC_EnableIRQ
    #define NVIC_EnableIRQ(n)  PFIC_EnableIRQ(n)
    #define NVIC_DisableIRQ(n) PFIC_DisableIRQ(n)
  #endif
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// CTRL
#define USBFS_CTRL_DMA_EN    (1 << 0)
#define USBFS_CTRL_CLR_ALL   (1 << 1)
#define USBFS_CTRL_RESET_SIE (1 << 2)
#define USBFS_CTRL_INT_BUSY  (1 << 3)
#define USBFS_CTRL_SYS_CTRL  (1 << 4)
#define USBFS_CTRL_DEV_PUEN  (1 << 5)
#define USBFS_CTRL_LOW_SPEED (1 << 6)
#define USBFS_CTRL_HOST_MODE (1 << 7)

// INT_EN
#define USBFS_INT_EN_BUS_RST  (1 << 0)
#define USBFS_INT_EN_DETECT   (1 << 0)
#define USBFS_INT_EN_TRANSFER (1 << 1)
#define USBFS_INT_EN_SUSPEND  (1 << 2)
#define USBFS_INT_EN_HST_SOF  (1 << 3)
#define USBFS_INT_EN_FIFO_OV  (1 << 4)
#define USBFS_INT_EN_DEV_NAK  (1 << 6)
#define USBFS_INT_EN_DEV_SOF  (1 << 7)

// INT_FG
#define USBFS_INT_FG_BUS_RST  (1 << 0)
#define USBFS_INT_FG_DETECT   (1 << 0)
#define USBFS_INT_FG_TRANSFER (1 << 1)
#define USBFS_INT_FG_SUSPEND  (1 << 2)
#define USBFS_INT_FG_HST_SOF  (1 << 3)
#define USBFS_INT_FG_FIFO_OV  (1 << 4)
#define USBFS_INT_FG_SIE_FREE (1 << 5)
#define USBFS_INT_FG_TOG_OK   (1 << 6)
#define USBFS_INT_FG_IS_NAK   (1 << 7)

// MIS_ST: the SUSPEND interrupt fires on both suspend and resume; this bit (R8_USB_MIS_ST) is 1
// while the bus is suspended and 0 once it has resumed, so it tells the two apart.
#define USBFS_MIS_ST_SUSPEND  (1 << 2)

// INT_ST
#define USBFS_INT_ST_MASK_UIS_ENDP(x)  (((x) >> 0) & 0x0F)
#define USBFS_INT_ST_MASK_UIS_TOKEN(x) (((x) >> 4) & 0x03)
#define USBFS_INT_ST_TOG_OK            (1 << 6)  // received packet's data toggle matched expectation

// UDEV_CTRL
#define USBFS_UDEV_CTRL_PORT_EN   (1 << 0)
#define USBFS_UDEV_CTRL_GP_BIT    (1 << 1)
#define USBFS_UDEV_CTRL_LOW_SPEED (1 << 2)
#define USBFS_UDEV_CTRL_DM_PIN    (1 << 4)
#define USBFS_UDEV_CTRL_DP_PIN    (1 << 5)
#define USBFS_UDEV_CTRL_PD_DIS    (1 << 7)

// TX_CTRL
#define USBFS_EP_T_RES_MASK (3 << 0)
#define USBFS_EP_T_TOG      (1 << 2)
#define USBFS_EP_T_AUTO_TOG (1 << 3)

#define USBFS_EP_T_RES_ACK   (0 << 0)
#define USBFS_EP_T_RES_NYET  (1 << 0)
#define USBFS_EP_T_RES_NAK   (2 << 0)
#define USBFS_EP_T_RES_STALL (3 << 0)

// RX_CTRL
#define USBFS_EP_R_RES_MASK (3 << 0)
#define USBFS_EP_R_TOG      (1 << 2)
#define USBFS_EP_R_AUTO_TOG (1 << 3)

#define USBFS_EP_R_RES_ACK   (0 << 0)
#define USBFS_EP_R_RES_NYET  (1 << 0)
#define USBFS_EP_R_RES_NAK   (2 << 0)
#define USBFS_EP_R_RES_STALL (3 << 0)

#ifdef CH32_USBFS_EP_CTRL_COMBINED
// Combined per-endpoint control register (older IP, e.g. CH32V103): IN response in
// bits [1:0], OUT response in bits [3:2], shared auto-toggle, separate IN/OUT toggle.
#define USBFS_EPC_T_RES_MASK  0x03
#define USBFS_EPC_R_RES_MASK  0x0C
#define USBFS_EPC_R_RES_SHIFT 2
#define USBFS_EPC_AUTO_TOG    0x10
#define USBFS_EPC_T_TOG       0x40
#define USBFS_EPC_R_TOG       0x80
#endif

// token PID
#define PID_OUT   0
#define PID_SOF   1
#define PID_IN    2
#define PID_SETUP 3

#endif // USB_CH32_USBFS_REG_H
