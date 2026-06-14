/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Matthew Tran
 * Copyright (c) 2024 hathach
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
    __IO uint32_t UEP2_DMA;
    __IO uint32_t UEP3_DMA;
    __IO uint32_t UEP4_DMA;
    __IO uint32_t UEP5_DMA;
    __IO uint32_t UEP6_DMA;
    __IO uint32_t UEP7_DMA;
    __IO uint16_t UEP0_TX_LEN;
    __IO uint8_t  UEP0_TX_CTRL;
    __IO uint8_t  UEP0_RX_CTRL;
    __IO uint16_t UEP1_TX_LEN;
    __IO uint8_t  UEP1_TX_CTRL;
    __IO uint8_t  UEP1_RX_CTRL;
    __IO uint16_t UEP2_TX_LEN;
    __IO uint8_t  UEP2_TX_CTRL;
    __IO uint8_t  UEP2_RX_CTRL;
    __IO uint16_t UEP3_TX_LEN;
    __IO uint8_t  UEP3_TX_CTRL;
    __IO uint8_t  UEP3_RX_CTRL;
    __IO uint16_t UEP4_TX_LEN;
    __IO uint8_t  UEP4_TX_CTRL;
    __IO uint8_t  UEP4_RX_CTRL;
    __IO uint16_t UEP5_TX_LEN;
    __IO uint8_t  UEP5_TX_CTRL;
    __IO uint8_t  UEP5_RX_CTRL;
    __IO uint16_t UEP6_TX_LEN;
    __IO uint8_t  UEP6_TX_CTRL;
    __IO uint8_t  UEP6_RX_CTRL;
    __IO uint16_t UEP7_TX_LEN;
    __IO uint8_t  UEP7_TX_CTRL;
    __IO uint8_t  UEP7_RX_CTRL;
    __IO uint32_t Reserve1;
    __IO uint32_t OTG_CR;
    __IO uint32_t OTG_SR;
  } USBOTG_FS_TypeDef;

  #define USBOTG_FS ((USBOTG_FS_TypeDef *) 0x40023400)
#elif CFG_TUSB_MCU == OPT_MCU_CH32V20X
  #include <ch32v20x.h>
  #include <ch32v20x_usb.h>
#elif CFG_TUSB_MCU == OPT_MCU_CH32V307
  #include <ch32v30x.h>
  #include <ch32v30x_usb.h>
  #define USBHD_IRQn OTG_FS_IRQn
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// we only speak up to full-speed here, i.e. 64 bytes per packet
#define MAX_PACKET_SIZE 64



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

// INT_ST
#define USBFS_INT_ST_MASK_UIS_ENDP(x)  (((x) >> 0) & 0x0F)
#define USBFS_INT_ST_MASK_UIS_TOKEN(x) (((x) >> 4) & 0x03)

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

// token PID
#define PID_OUT   0
#define PID_SOF   1
#define PID_IN    2
#define PID_SETUP 3

// USB host defines taken from ch32v20x_usb.h
#define USBFS_UH_EP_TX_EN 0x40
#define USBFS_UH_EP_RX_EN 0x08

#define USBFS_UIF_DETECT 0x01
#define USBFS_UMS_DEV_ATTACH 0x01

#define USBFS_UDA_GP_BIT 0x80
#define USBFS_USB_ADDR_MASK 0x7F

#define USBFS_UH_LOW_SPEED 0x04
#define USBFS_UH_PRE_PID_EN 0x0400

#define USBFS_UH_BUS_RESET 0x02
#define USBFS_UH_PORT_EN 0x01

#define USBFS_UMS_DM_LEVEL 0x02
#define USB_LOW_SPEED 0x00
#define USB_FULL_SPEED 0x01
#define USBFS_UC_LOW_SPEED 0x40

#define USBFS_UH_SOF_EN 0x0004

/* USB PID */
#ifndef USB_PID_SETUP
  #define USB_PID_NULL 0x00
  #define USB_PID_SOF 0x05
  #define USB_PID_SETUP 0x0D
  #define USB_PID_IN 0x09
  #define USB_PID_OUT 0x01
  #define USB_PID_NYET 0x06
  #define USB_PID_ACK 0x02
  #define USB_PID_NAK 0x0A
  #define USB_PID_STALL 0x0E
  #define USB_PID_DATA0 0x03
  #define USB_PID_DATA1 0x0B
  #define USB_PID_PRE 0x0C
#endif

#define USBFS_UIF_HST_SOF 0x08
#define USBFS_UIF_TRANSFER 0x02

/* R8_USB_MIS_ST */
#define USBFS_UMS_SOF_PRES 0x80
#define USBFS_UMS_SOF_ACT 0x40
#define USBFS_UMS_SIE_FREE 0x20
#define USBFS_UMS_R_FIFO_RDY 0x10
#define USBFS_UMS_BUS_RESET 0x08
#define USBFS_UMS_SUSPEND 0x04
#define USBFS_UMS_DM_LEVEL 0x02
#define USBFS_UMS_DEV_ATTACH 0x01

#define USBFS_UIS_IS_NAK 0x80    // RO, indicate current USB transfer is NAK received for USB device mode
#define USBFS_UIS_TOG_OK 0x40    // RO, indicate current USB transfer toggle is OK
#define USBFS_UIS_TOKEN_MASK 0x30// RO, bit mask of current token PID code received for USB device mode
#define USBFS_UIS_TOKEN_OUT 0x00
#define USBFS_UIS_TOKEN_SOF 0x10
#define USBFS_UIS_TOKEN_IN 0x20
#define USBFS_UIS_TOKEN_SETUP 0x30

#define USBFS_UH_ENDP_MASK 0x0F
#define USBFS_UIS_H_RES_MASK 0x0F// RO, bit mask of current transfer handshake response for USB host mode:
#define USBFS_UH_TOKEN_MASK 0xF0


/* R8_UHOST_CTRL */
#define USBFS_UH_PD_DIS 0x80   // disable USB UDP/UDM pulldown resistance: 0=enable pulldown, 1=disable
#define USBFS_UH_DP_PIN 0x20   // ReadOnly: indicate current UDP pin level
#define USBFS_UH_DM_PIN 0x10   // ReadOnly: indicate current UDM pin level
#define USBFS_UH_LOW_SPEED 0x04// enable USB port low speed: 0=full speed, 1=low speed
#define USBFS_UH_BUS_RESET 0x02// control USB bus reset: 0=normal, 1=force bus reset
#define USBFS_UH_PORT_EN 0x01  // enable USB port: 0=disable, 1=enable port, automatic disabled if USB device detached

/* R32_UH_EP_MOD */
#define USBFS_UH_EP_TX_EN 0x40   // enable USB host OUT endpoint transmittal
#define USBFS_UH_EP_TBUF_MOD 0x10// buffer mode of USB host OUT endpoint
// bUH_EP_TX_EN & bUH_EP_TBUF_MOD: USB host OUT endpoint buffer mode, buffer start address is UH_TX_DMA
//   0 x:  disable endpoint and disable buffer
//   1 0:  64 bytes buffer for transmittal (OUT endpoint)
//   1 1:  dual 64 bytes buffer by toggle bit bUH_T_TOG selection for transmittal (OUT endpoint), total=128bytes
#define USBFS_UH_EP_RX_EN 0x08   // enable USB host IN endpoint receiving
#define USBFS_UH_EP_RBUF_MOD 0x01// buffer mode of USB host IN endpoint
// bUH_EP_RX_EN & bUH_EP_RBUF_MOD: USB host IN endpoint buffer mode, buffer start address is UH_RX_DMA
//   0 x:  disable endpoint and disable buffer
//   1 0:  64 bytes buffer for receiving (IN endpoint)
//   1 1:  dual 64 bytes buffer by toggle bit bUH_R_TOG selection for receiving (IN endpoint), total=128bytes

/* R16_UH_SETUP */
#define USBFS_UH_PRE_PID_EN 0x0400// USB host PRE PID enable for low speed device via hub
#define USBFS_UH_SOF_EN 0x0004    // USB host automatic SOF enable

/* R8_UH_EP_PID */
#define USBFS_UH_TOKEN_MASK 0xF0// bit mask of token PID for USB host transfer
#define USBFS_UH_ENDP_MASK 0x0F // bit mask of endpoint number for USB host transfer

/* R8_UH_RX_CTRL */
#define USBFS_UH_R_AUTO_TOG 0x08// enable automatic toggle after successful transfer completion: 0=manual toggle, 1=automatic toggle
#define USBFS_UH_R_TOG 0x04     // expected data toggle flag of host receiving (IN): 0=DATA0, 1=DATA1
#define USBFS_UH_R_RES 0x01     // prepared handshake response type for host receiving (IN): 0=ACK (ready), 1=no response, time out to device, for isochronous transactions

/* R8_UH_TX_CTRL */
#define USBFS_UH_T_AUTO_TOG 0x08// enable automatic toggle after successful transfer completion: 0=manual toggle, 1=automatic toggle
#define USBFS_UH_T_TOG 0x04     // prepared data toggle flag of host transmittal (SETUP/OUT): 0=DATA0, 1=DATA1
#define USBFS_UH_T_RES 0x01     // expected handshake response type for host transmittal (SETUP/OUT): 0=ACK (ready), 1=no response, time out from device, for isochronous transactions

/* R8_USB_MIS_ST */
#define USBFS_UMS_SOF_PRES 0x80
#define USBFS_UMS_SOF_ACT 0x40
#define USBFS_UMS_SIE_FREE 0x20
#define USBFS_UMS_R_FIFO_RDY 0x10
#define USBFS_UMS_BUS_RESET 0x08
#define USBFS_UMS_SUSPEND 0x04
#define USBFS_UMS_DM_LEVEL 0x02
#define USBFS_UMS_DEV_ATTACH 0x01

#endif// USB_CH32_USBFS_REG_H
