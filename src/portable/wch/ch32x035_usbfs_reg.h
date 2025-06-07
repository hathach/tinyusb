/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Matthew Tran
 * Copyright (c) 2024 hathach
 * Copyright (c) 2024 TinyUSB Community (for CH32X035 modifications)
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

// TODO: This is a SKELETON header file for CH32X035 USB Full-Speed (USBFS) peripheral registers.
// It needs to be populated with actual register definitions, base addresses, and bit fields
// specific to the CH32X035 series, based on its official reference manual.
// The existing definitions below are largely from other CH32 series (like V103, V307)
// and MUST BE VERIFIED AND UPDATED.

#ifndef USB_CH32X035_USBFS_REG_H
#define USB_CH32X035_USBFS_REG_H

// TODO: Remove or verify these GCC pragmas if not needed or if causing issues with chosen compiler for CH32X035
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

// TODO: CH32X035 - Define the USBOTG_FS_TypeDef structure according to the CH32X035 reference manual.
// The structure below is an example from CH32V103 and is likely DIFFERENT for CH32X035.
/*
typedef struct
{
  __IO uint8_t  BASE_CTRL;
  __IO uint8_t  UDEV_CTRL;
  __IO uint8_t  INT_EN;
  __IO uint8_t  DEV_ADDR;
  __IO uint8_t  Reserve0;
  __IO uint8_t  MIS_ST;
  __IO uint8_t  INT_FG;
  __IO uint8_t  INT_ST;
  __IO uint32_t RX_LEN;
  __IO uint8_t  UEP4_1_MOD;
  __IO uint8_t  UEP2_3_MOD;
  __IO uint8_t  UEP5_6_MOD;
  __IO uint8_t  UEP7_MOD;
  __IO uint32_t UEP0_DMA;
  __IO uint32_t UEP1_DMA;
  // ... other endpoint DMA registers ...
  __IO uint32_t UEP7_DMA;
  __IO uint16_t UEP0_TX_LEN;
  __IO uint8_t  UEP0_TX_CTRL;
  __IO uint8_t  UEP0_RX_CTRL;
  // ... other endpoint TX/RX control and length registers ...
  __IO uint16_t UEP7_TX_LEN;
  __IO uint8_t  UEP7_TX_CTRL;
  __IO uint8_t  UEP7_RX_CTRL;
  __IO uint32_t Reserve1;
  __IO uint32_t OTG_CR;     // May not exist or be different in CH32X035
  __IO uint32_t OTG_SR;     // May not exist or be different in CH32X035
} USBOTG_FS_TypeDef;
*/

// TODO: CH32X035 - Define the correct base address for the USBFS peripheral.
// The address 0x40023400 is from CH32V103 and is LIKELY INCORRECT for CH32X035.
// #define USBOTG_FS ((USBOTG_FS_TypeDef *) 0x40023400_CH32X035_USBFS_BASE_ADDRESS_TODO)


// TODO: Verify ALL register bit definitions below for CH32X035. These are generic names
// but their values or existence might differ.

// CTRL Register Bits
#define USBFS_CTRL_DMA_EN    (1 << 0) // TODO: Verify bit position and meaning
#define USBFS_CTRL_CLR_ALL   (1 << 1) // TODO: Verify bit position and meaning
#define USBFS_CTRL_RESET_SIE (1 << 2) // TODO: Verify bit position and meaning
#define USBFS_CTRL_INT_BUSY  (1 << 3) // TODO: Verify bit position and meaning
#define USBFS_CTRL_SYS_CTRL  (1 << 4) // TODO: Verify bit position and meaning (e.g., UC_HOST_MODE in some WCH chips)
#define USBFS_CTRL_DEV_PUEN  (1 << 5) // TODO: Verify bit position and meaning (pull-up enable)
#define USBFS_CTRL_LOW_SPEED (1 << 6) // TODO: Verify bit position and meaning
#define USBFS_CTRL_HOST_MODE (1 << 7) // TODO: Verify bit position and meaning (may be part of SYS_CTRL)

// INT_EN Register Bits (Interrupt Enable)
#define USBFS_INT_EN_BUS_RST  (1 << 0) // TODO: Verify bit position (for device mode: reset)
#define USBFS_INT_EN_DETECT   (1 << 0) // TODO: Verify bit position (for host mode: connect/disconnect)
#define USBFS_INT_EN_TRANSFER (1 << 1) // TODO: Verify bit position (transfer complete)
#define USBFS_INT_EN_SUSPEND  (1 << 2) // TODO: Verify bit position (suspend event)
#define USBFS_INT_EN_HST_SOF  (1 << 3) // TODO: Verify bit position (for host mode: SOF sent)
#define USBFS_INT_EN_FIFO_OV  (1 << 4) // TODO: Verify bit position (FIFO overflow)
#define USBFS_INT_EN_DEV_NAK  (1 << 6) // TODO: Verify bit position (for device mode: NAK sent)
#define USBFS_INT_EN_DEV_SOF  (1 << 7) // TODO: Verify bit position (for device mode: SOF received)

// INT_FG Register Bits (Interrupt Flag)
#define USBFS_INT_FG_BUS_RST  (1 << 0) // TODO: Verify bit position
#define USBFS_INT_FG_DETECT   (1 << 0) // TODO: Verify bit position
#define USBFS_INT_FG_TRANSFER (1 << 1) // TODO: Verify bit position
#define USBFS_INT_FG_SUSPEND  (1 << 2) // TODO: Verify bit position
#define USBFS_INT_FG_HST_SOF  (1 << 3) // TODO: Verify bit position
#define USBFS_INT_FG_FIFO_OV  (1 << 4) // TODO: Verify bit position
#define USBFS_INT_FG_SIE_FREE (1 << 5) // TODO: Verify bit position (CH32V10x specific?)
#define USBFS_INT_FG_TOG_OK   (1 << 6) // TODO: Verify bit position (Toggle OK - CH32V10x specific?)
#define USBFS_INT_FG_IS_NAK   (1 << 7) // TODO: Verify bit position (NAK transaction flag)

// INT_ST Register Bits (Interrupt Status)
#define USBFS_INT_ST_MASK_UIS_ENDP(x)  (((x) >> 0) & 0x0F) // TODO: Verify mask and shift for endpoint number
#define USBFS_INT_ST_MASK_UIS_TOKEN(x) (((x) >> 4) & 0x03) // TODO: Verify mask and shift for token PID

// UDEV_CTRL Register Bits (USB Device Control)
#define USBFS_UDEV_CTRL_PORT_EN   (1 << 0) // TODO: Verify bit position (USB port enable)
#define USBFS_UDEV_CTRL_GP_BIT    (1 << 1) // TODO: Verify bit position (General purpose bit, CH32V10x specific?)
#define USBFS_UDEV_CTRL_LOW_SPEED (1 << 2) // TODO: Verify bit position (Force low speed)
#define USBFS_UDEV_CTRL_DM_PIN    (1 << 4) // TODO: Verify bit position (DM pin status)
#define USBFS_UDEV_CTRL_DP_PIN    (1 << 5) // TODO: Verify bit position (DP pin status)
#define USBFS_UDEV_CTRL_PD_DIS    (1 << 7) // TODO: Verify bit position (Pull-down disable, often means pull-up enable for D+)


// Endpoint TX_CTRL Register Bits
#define USBFS_EP_T_RES_MASK (3 << 0) // TODO: Verify mask for transaction response
#define USBFS_EP_T_TOG      (1 << 2) // TODO: Verify bit for current toggle status (read-only?)
#define USBFS_EP_T_AUTO_TOG (1 << 3) // TODO: Verify bit for hardware auto toggle

#define USBFS_EP_T_RES_ACK   (0 << 0) // TODO: Verify value for ACK
#define USBFS_EP_T_RES_NYET  (1 << 0) // TODO: Verify value for NYET
#define USBFS_EP_T_RES_NAK   (2 << 0) // TODO: Verify value for NAK
#define USBFS_EP_T_RES_STALL (3 << 0) // TODO: Verify value for STALL

// Endpoint RX_CTRL Register Bits
#define USBFS_EP_R_RES_MASK (3 << 0) // TODO: Verify mask for transaction response
#define USBFS_EP_R_TOG      (1 << 2) // TODO: Verify bit for current toggle status (read-only?)
#define USBFS_EP_R_AUTO_TOG (1 << 3) // TODO: Verify bit for hardware auto toggle

#define USBFS_EP_R_RES_ACK   (0 << 0) // TODO: Verify value for ACK
#define USBFS_EP_R_RES_NYET  (1 << 0) // TODO: Verify value for NYET (usually not for RX)
#define USBFS_EP_R_RES_NAK   (2 << 0) // TODO: Verify value for NAK
#define USBFS_EP_R_RES_STALL (3 << 0) // TODO: Verify value for STALL

// Token PID values (These are standard USB PIDs, likely unchanged)
#define PID_OUT   0
#define PID_SOF   1
#define PID_IN    2
#define PID_SETUP 3

// TODO: Add any other CH32X035 specific registers or bit fields.
// For example, endpoint configuration registers (UEPn_CONFIG),
// endpoint buffer DMA address registers (UEPn_DMA),
// endpoint max packet size registers, etc.

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif // USB_CH32X035_USBFS_REG_H
