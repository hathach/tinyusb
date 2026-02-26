/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 TinyUSB contributors
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

#ifndef CH58X_USBFS_REG_H
#define CH58X_USBFS_REG_H

#include <stdint.h>

//--------------------------------------------------------------------+
// USB Base Addresses
//--------------------------------------------------------------------+
#define CH58X_USB_BASE   0x40008000u
#define CH58X_USB2_BASE  0x40008400u

//--------------------------------------------------------------------+
// Global Control / Status Registers
//--------------------------------------------------------------------+
#define CH58X_USB_CTRL(base)     (*(volatile uint8_t  *)((base) + 0x00))
#define CH58X_UDEV_CTRL(base)   (*(volatile uint8_t  *)((base) + 0x01))
#define CH58X_USB_INT_EN(base)  (*(volatile uint8_t  *)((base) + 0x02))
#define CH58X_USB_DEV_AD(base)  (*(volatile uint8_t  *)((base) + 0x03))
#define CH58X_USB_MIS_ST(base)  (*(volatile uint8_t  *)((base) + 0x05))
#define CH58X_USB_INT_FG(base)  (*(volatile uint8_t  *)((base) + 0x06))
#define CH58X_USB_INT_ST(base)  (*(volatile uint8_t  *)((base) + 0x07))
#define CH58X_USB_RX_LEN(base)  (*(volatile uint8_t  *)((base) + 0x08))

//--------------------------------------------------------------------+
// Endpoint Mode Registers
//--------------------------------------------------------------------+
#define CH58X_UEP4_1_MOD(base)  (*(volatile uint8_t  *)((base) + 0x0C))
#define CH58X_UEP2_3_MOD(base)  (*(volatile uint8_t  *)((base) + 0x0D))
#define CH58X_UEP567_MOD(base)  (*(volatile uint8_t  *)((base) + 0x0E))

//--------------------------------------------------------------------+
// Endpoint DMA / T_LEN / CTRL register accessors
//
// EP0-EP3 DMA : base + 0x10 + ep*4  (EP4 shares EP0 DMA, no own register)
// EP5-EP7 DMA : base + 0x54 + (ep-5)*4
// EP0-EP4 TLEN: base + 0x20 + ep*4
// EP5-EP7 TLEN: base + 0x64 + (ep-5)*4
// EP0-EP4 CTRL: base + 0x22 + ep*4
// EP5-EP7 CTRL: base + 0x66 + (ep-5)*4
//
// Using computed addresses avoids per-TU copies of static lookup tables.
// EP4 DMA is not accessed directly (it shares EP0's DMA register).
//--------------------------------------------------------------------+

// EP DMA is 16-bit (only low 16 bits of RAM address, high bits implied 0x2000)
#define CH58X_EP_DMA(base, ep)   (*(volatile uint16_t *)((base) + ((ep) <= 3 ? (0x10u + (ep)*4u) : (0x54u + ((ep)-5u)*4u))))
#define CH58X_EP_TLEN(base, ep)  (*(volatile uint8_t  *)((base) + ((ep) <= 4 ? (0x20u + (ep)*4u) : (0x64u + ((ep)-5u)*4u))))
#define CH58X_EP_CTRL(base, ep)  (*(volatile uint8_t  *)((base) + ((ep) <= 4 ? (0x22u + (ep)*4u) : (0x66u + ((ep)-5u)*4u))))

//--------------------------------------------------------------------+
// USB_CTRL (R8_USB_CTRL) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UC_DMA_EN       0x01
#define CH58X_UC_CLR_ALL      0x02
#define CH58X_UC_RESET_SIE    0x04
#define CH58X_UC_INT_BUSY     0x08
#define CH58X_UC_SYS_CTRL     0x10
#define CH58X_UC_DEV_PU_EN    0x20
#define CH58X_UC_LOW_SPEED    0x40
#define CH58X_UC_HOST_MODE    0x80

//--------------------------------------------------------------------+
// UDEV_CTRL (R8_UDEV_CTRL) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UD_PORT_EN      0x01
#define CH58X_UD_GP_BIT       0x02
#define CH58X_UD_LOW_SPEED    0x04
#define CH58X_UD_PD_DIS       0x80

//--------------------------------------------------------------------+
// INT_EN (R8_USB_INT_EN) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UIE_BUS_RST     0x01
#define CH58X_UIE_DETECT      0x01  /* host mode alias */
#define CH58X_UIE_TRANSFER    0x02
#define CH58X_UIE_SUSPEND     0x04
#define CH58X_UIE_HST_SOF     0x08
#define CH58X_UIE_FIFO_OV     0x10

//--------------------------------------------------------------------+
// INT_FG (R8_USB_INT_FG) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UIF_BUS_RST     0x01
#define CH58X_UIF_DETECT      0x01  /* host mode alias */
#define CH58X_UIF_TRANSFER    0x02
#define CH58X_UIF_SUSPEND     0x04
#define CH58X_UIF_HST_SOF     0x08
#define CH58X_UIF_FIFO_OV     0x10
#define CH58X_U_SIE_FREE      0x20
#define CH58X_U_TOG_OK        0x40
#define CH58X_U_IS_NAK        0x80

//--------------------------------------------------------------------+
// INT_ST (R8_USB_INT_ST) parsing
//--------------------------------------------------------------------+
#define CH58X_INT_ST_ENDP(x)    (((x) >> 0) & 0x0F)
#define CH58X_INT_ST_TOKEN(x)   (((x) >> 4) & 0x03)
#define CH58X_UIS_SETUP_ACT     0x80

// Token PID values
#define CH58X_PID_OUT    0
#define CH58X_PID_SOF    1
#define CH58X_PID_IN     2
#define CH58X_PID_SETUP  3

//--------------------------------------------------------------------+
// MIS_ST (R8_USB_MIS_ST) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UMS_DEV_ATTACH  0x01
#define CH58X_UMS_DM_LEVEL    0x02
#define CH58X_UMS_SUSPEND     0x04
#define CH58X_UMS_BUS_RESET   0x08
#define CH58X_UMS_R_FIFO_RDY  0x10
#define CH58X_UMS_SIE_FREE    0x20
#define CH58X_UMS_SOF_ACT     0x40
#define CH58X_UMS_SOF_PRES    0x80

//--------------------------------------------------------------------+
// EP CTRL register bit definitions (merged TX+RX in single 8-bit reg)
//
//  Bit 7: RB_UEP_R_TOG    RX data toggle
//  Bit 6: RB_UEP_T_TOG    TX data toggle
//  Bit 5: (reserved)
//  Bit 4: RB_UEP_AUTO_TOG auto toggle (EP1/2/3 only)
//  Bit 3: R_RES1           RX response high
//  Bit 2: R_RES0           RX response low
//  Bit 1: T_RES1           TX response high
//  Bit 0: T_RES0           TX response low
//--------------------------------------------------------------------+

// TX response bits[1:0]
#define CH58X_EP_T_RES_MASK    0x03
#define CH58X_EP_T_RES_ACK     0x00
#define CH58X_EP_T_RES_TOUT    0x01  /* ISO: no handshake */
#define CH58X_EP_T_RES_NAK     0x02
#define CH58X_EP_T_RES_STALL   0x03

// RX response bits[3:2]
#define CH58X_EP_R_RES_MASK    0x0C
#define CH58X_EP_R_RES_ACK     0x00
#define CH58X_EP_R_RES_TOUT    0x04  /* ISO: no handshake */
#define CH58X_EP_R_RES_NAK     0x08
#define CH58X_EP_R_RES_STALL   0x0C

// Toggle and auto-toggle
#define CH58X_EP_AUTO_TOG      0x10  /* bit 4, shared TX/RX */
#define CH58X_EP_T_TOG         0x40  /* bit 6, TX DATA toggle */
#define CH58X_EP_R_TOG         0x80  /* bit 7, RX DATA toggle */

//--------------------------------------------------------------------+
// EP Mode register (UEP4_1_MOD) bit definitions
//--------------------------------------------------------------------+
// R8_UEP4_1_MOD: EP4 in bits[3:2], EP1 in bits[7:4]
#define CH58X_UEP1_BUF_MOD    0x10
#define CH58X_UEP1_TX_EN      0x40
#define CH58X_UEP1_RX_EN      0x80
#define CH58X_UEP4_TX_EN      0x04
#define CH58X_UEP4_RX_EN      0x08

// R8_UEP2_3_MOD: EP3 in bits[7:4], EP2 in bits[3:0]
#define CH58X_UEP2_BUF_MOD    0x01
#define CH58X_UEP2_TX_EN      0x04
#define CH58X_UEP2_RX_EN      0x08
#define CH58X_UEP3_BUF_MOD    0x10
#define CH58X_UEP3_TX_EN      0x40
#define CH58X_UEP3_RX_EN      0x80

// R8_UEP567_MOD
#define CH58X_UEP5_TX_EN      0x01
#define CH58X_UEP5_RX_EN      0x02
#define CH58X_UEP6_TX_EN      0x04
#define CH58X_UEP6_RX_EN      0x08
#define CH58X_UEP7_TX_EN      0x10
#define CH58X_UEP7_RX_EN      0x20

//--------------------------------------------------------------------+
// Host-mode Register Aliases
// In host mode: EP2 -> RX, EP3 -> TX, EP1_CTRL -> SETUP
//--------------------------------------------------------------------+
#define CH58X_UHOST_CTRL(base)   (*(volatile uint8_t  *)((base) + 0x01))  // = UDEV_CTRL
#define CH58X_UH_EP_MOD(base)    (*(volatile uint8_t  *)((base) + 0x0D))  // = UEP2_3_MOD
#define CH58X_UH_RX_DMA(base)    (*(volatile uint16_t *)((base) + 0x18))  // = UEP2_DMA
#define CH58X_UH_TX_DMA(base)    (*(volatile uint16_t *)((base) + 0x1C))  // = UEP3_DMA
#define CH58X_UH_SETUP(base)     (*(volatile uint8_t  *)((base) + 0x26))  // = UEP1_CTRL
#define CH58X_UH_EP_PID(base)    (*(volatile uint8_t  *)((base) + 0x28))  // = UEP2_T_LEN
#define CH58X_UH_RX_CTRL(base)   (*(volatile uint8_t  *)((base) + 0x2A))  // = UEP2_CTRL
#define CH58X_UH_TX_LEN(base)    (*(volatile uint8_t  *)((base) + 0x2C))  // = UEP3_T_LEN
#define CH58X_UH_TX_CTRL(base)   (*(volatile uint8_t  *)((base) + 0x2E))  // = UEP3_CTRL

//--------------------------------------------------------------------+
// UHOST_CTRL (R8_UHOST_CTRL) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UH_PD_DIS           0x80
#define CH58X_UH_LOW_SPEED        0x04
#define CH58X_UH_BUS_RESET        0x02
#define CH58X_UH_PORT_EN          0x01

//--------------------------------------------------------------------+
// UH_EP_MOD (R8_UH_EP_MOD) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UH_EP_TX_EN         0x40
#define CH58X_UH_EP_TBUF_MOD      0x10
#define CH58X_UH_EP_RX_EN         0x08
#define CH58X_UH_EP_RBUF_MOD      0x01

//--------------------------------------------------------------------+
// UH_SETUP (R8_UH_SETUP) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UH_PRE_PID_EN       0x80
#define CH58X_UH_SOF_EN           0x40

//--------------------------------------------------------------------+
// UH_RX_CTRL (R8_UH_RX_CTRL) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UH_R_TOG            0x80
#define CH58X_UH_R_AUTO_TOG       0x10
#define CH58X_UH_R_RES            0x04

//--------------------------------------------------------------------+
// UH_TX_CTRL (R8_UH_TX_CTRL) bit definitions
//--------------------------------------------------------------------+
#define CH58X_UH_T_TOG            0x40
#define CH58X_UH_T_AUTO_TOG       0x10
#define CH58X_UH_T_RES            0x01

//--------------------------------------------------------------------+
// USB_INT_ST host-mode bits
//--------------------------------------------------------------------+
#define CH58X_UIS_H_RES_MASK      0x0F
#define CH58X_UIS_TOG_OK          0x40

//--------------------------------------------------------------------+
// USB_DEV_AD bits
//--------------------------------------------------------------------+
#define CH58X_UDA_GP_BIT          0x80
#define CH58X_USB_ADDR_MASK       0x7F

//--------------------------------------------------------------------+
// Standard USB PID values (for host token and response)
//--------------------------------------------------------------------+
#define CH58X_USB_PID_OUT         0x01
#define CH58X_USB_PID_IN          0x09
#define CH58X_USB_PID_SOF         0x05
#define CH58X_USB_PID_SETUP       0x0D
#define CH58X_USB_PID_DATA0       0x03
#define CH58X_USB_PID_DATA1       0x0B
#define CH58X_USB_PID_ACK         0x02
#define CH58X_USB_PID_NAK         0x0A
#define CH58X_USB_PID_STALL       0x0E

//--------------------------------------------------------------------+
// PIN_ANALOG_IE register
//--------------------------------------------------------------------+
#define CH58X_PIN_ANALOG_IE       (*(volatile uint16_t *)0x4000101A)
#define CH58X_PIN_USB_DP_PU       0x40
#define CH58X_PIN_USB_IE          0x80
#define CH58X_PIN_USB2_DP_PU      0x10
#define CH58X_PIN_USB2_IE         0x20

//--------------------------------------------------------------------+
// Sleep clock control
//--------------------------------------------------------------------+
#define CH58X_SLP_CLK_OFF1        (*(volatile uint8_t *)0x4000100D)
#define CH58X_SLP_CLK_USB         0x10

#endif /* CH58X_USBFS_REG_H */
