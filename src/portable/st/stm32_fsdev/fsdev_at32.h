/*
* The MIT License (MIT)
 *
 * Copyright (c) 2024, hathach (tinyusb.org)
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
#ifndef TUSB_FSDEV_AT32_H
#define TUSB_FSDEV_AT32_H

#include "common/tusb_compiler.h"

#if CFG_TUSB_MCU == OPT_MCU_AT32F403A_407
  #include "at32f403a_407.h"

#elif CFG_TUSB_MCU == OPT_MCU_AT32F413
  #include "at32f413.h"

#endif

#define FSDEV_PMA_SIZE (512u)
#define FSDEV_USE_SBUF_ISO 0
#define FSDEV_REG_BASE  (APB1PERIPH_BASE + 0x00005C00UL)
#define FSDEV_PMA_BASE  (APB1PERIPH_BASE + 0x00006000UL)

#ifndef CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP
  #define CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP 0
#endif

/****************************  ISTR interrupt events  *************************/
#define USB_ISTR_CTR                         ((uint16_t)0x8000U)               /*!< Correct TRansfer (clear-only bit) */
#define USB_ISTR_PMAOVR                      ((uint16_t)0x4000U)               /*!< DMA OVeR/underrun (clear-only bit) */
#define USB_ISTR_ERR                         ((uint16_t)0x2000U)               /*!< ERRor (clear-only bit) */
#define USB_ISTR_WKUP                        ((uint16_t)0x1000U)               /*!< WaKe UP (clear-only bit) */
#define USB_ISTR_SUSP                        ((uint16_t)0x0800U)               /*!< SUSPend (clear-only bit) */
#define USB_ISTR_RESET                       ((uint16_t)0x0400U)               /*!< RESET (clear-only bit) */
#define USB_ISTR_SOF                         ((uint16_t)0x0200U)               /*!< Start Of Frame (clear-only bit) */
#define USB_ISTR_ESOF                        ((uint16_t)0x0100U)               /*!< Expected Start Of Frame (clear-only bit) */
#define USB_ISTR_DIR                         ((uint16_t)0x0010U)               /*!< DIRection of transaction (read-only bit)  */
#define USB_ISTR_EP_ID                       ((uint16_t)0x000FU)               /*!< EndPoint IDentifier (read-only bit)  */

/* Legacy defines */
#define USB_ISTR_PMAOVRM USB_ISTR_PMAOVR

#define USB_CLR_CTR                          (~USB_ISTR_CTR)             /*!< clear Correct TRansfer bit */
#define USB_CLR_PMAOVR                       (~USB_ISTR_PMAOVR)          /*!< clear DMA OVeR/underrun bit*/
#define USB_CLR_ERR                          (~USB_ISTR_ERR)             /*!< clear ERRor bit */
#define USB_CLR_WKUP                         (~USB_ISTR_WKUP)            /*!< clear WaKe UP bit */
#define USB_CLR_SUSP                         (~USB_ISTR_SUSP)            /*!< clear SUSPend bit */
#define USB_CLR_RESET                        (~USB_ISTR_RESET)           /*!< clear RESET bit */
#define USB_CLR_SOF                          (~USB_ISTR_SOF)             /*!< clear Start Of Frame bit */
#define USB_CLR_ESOF                         (~USB_ISTR_ESOF)            /*!< clear Expected Start Of Frame bit */

/* Legacy defines */
#define USB_CLR_PMAOVRM USB_CLR_PMAOVR

/*************************  CNTR control register bits definitions  ***********/
#define USB_CNTR_CTRM                        ((uint16_t)0x8000U)               /*!< Correct TRansfer Mask */
#define USB_CNTR_PMAOVR                      ((uint16_t)0x4000U)               /*!< DMA OVeR/underrun Mask */
#define USB_CNTR_ERRM                        ((uint16_t)0x2000U)               /*!< ERRor Mask */
#define USB_CNTR_WKUPM                       ((uint16_t)0x1000U)               /*!< WaKe UP Mask */
#define USB_CNTR_SUSPM                       ((uint16_t)0x0800U)               /*!< SUSPend Mask */
#define USB_CNTR_RESETM                      ((uint16_t)0x0400U)               /*!< RESET Mask   */
#define USB_CNTR_SOFM                        ((uint16_t)0x0200U)               /*!< Start Of Frame Mask */
#define USB_CNTR_ESOFM                       ((uint16_t)0x0100U)               /*!< Expected Start Of Frame Mask */
#define USB_CNTR_RESUME                      ((uint16_t)0x0010U)               /*!< RESUME request */
#define USB_CNTR_FSUSP                       ((uint16_t)0x0008U)               /*!< Force SUSPend */
#define USB_CNTR_LPMODE                      ((uint16_t)0x0004U)               /*!< Low-power MODE */
#define USB_CNTR_PDWN                        ((uint16_t)0x0002U)               /*!< Power DoWN */
#define USB_CNTR_FRES                        ((uint16_t)0x0001U)               /*!< Force USB RESet */

/* Legacy defines */
#define USB_CNTR_PMAOVRM USB_CNTR_PMAOVR
#define USB_CNTR_LP_MODE USB_CNTR_LPMODE

/********************  FNR Frame Number Register bit definitions   ************/
#define USB_FNR_RXDP                         ((uint16_t)0x8000U)               /*!< status of D+ data line */
#define USB_FNR_RXDM                         ((uint16_t)0x4000U)               /*!< status of D- data line */
#define USB_FNR_LCK                          ((uint16_t)0x2000U)               /*!< LoCKed */
#define USB_FNR_LSOF                         ((uint16_t)0x1800U)               /*!< Lost SOF */
#define USB_FNR_FN                           ((uint16_t)0x07FFU)               /*!< Frame Number */

/********************  DADDR Device ADDRess bit definitions    ****************/
#define USB_DADDR_EF                         ((uint8_t)0x80U)                  /*!< USB device address Enable Function */
#define USB_DADDR_ADD                        ((uint8_t)0x7FU)                  /*!< USB device address */

/******************************  Endpoint register    *************************/
#define USB_EP0R                             USB_BASE                    /*!< endpoint 0 register address */
#define USB_EP1R                             (USB_BASE + 0x04U)           /*!< endpoint 1 register address */
#define USB_EP2R                             (USB_BASE + 0x08U)           /*!< endpoint 2 register address */
#define USB_EP3R                             (USB_BASE + 0x0CU)           /*!< endpoint 3 register address */
#define USB_EP4R                             (USB_BASE + 0x10U)           /*!< endpoint 4 register address */
#define USB_EP5R                             (USB_BASE + 0x14U)           /*!< endpoint 5 register address */
#define USB_EP6R                             (USB_BASE + 0x18U)           /*!< endpoint 6 register address */
#define USB_EP7R                             (USB_BASE + 0x1CU)           /*!< endpoint 7 register address */
/* bit positions */
#define USB_EP_CTR_RX                        ((uint16_t)0x8000U)               /*!<  EndPoint Correct TRansfer RX */
#define USB_EP_DTOG_RX                       ((uint16_t)0x4000U)               /*!<  EndPoint Data TOGGLE RX */
#define USB_EPRX_STAT                        ((uint16_t)0x3000U)               /*!<  EndPoint RX STATus bit field */
#define USB_EP_SETUP                         ((uint16_t)0x0800U)               /*!<  EndPoint SETUP */
#define USB_EP_T_FIELD                       ((uint16_t)0x0600U)               /*!<  EndPoint TYPE */
#define USB_EP_KIND                          ((uint16_t)0x0100U)               /*!<  EndPoint KIND */
#define USB_EP_CTR_TX                        ((uint16_t)0x0080U)               /*!<  EndPoint Correct TRansfer TX */
#define USB_EP_DTOG_TX                       ((uint16_t)0x0040U)               /*!<  EndPoint Data TOGGLE TX */
#define USB_EPTX_STAT                        ((uint16_t)0x0030U)               /*!<  EndPoint TX STATus bit field */
#define USB_EPADDR_FIELD                     ((uint16_t)0x000FU)               /*!<  EndPoint ADDRess FIELD */

/* EndPoint REGister MASK (no toggle fields) */
#define USB_EPREG_MASK     (USB_EP_CTR_RX|USB_EP_SETUP|USB_EP_T_FIELD|USB_EP_KIND|USB_EP_CTR_TX|USB_EPADDR_FIELD)
                                                                               /*!< EP_TYPE[1:0] EndPoint TYPE */
#define USB_EP_TYPE_MASK                     ((uint16_t)0x0600U)               /*!< EndPoint TYPE Mask */
#define USB_EP_BULK                          ((uint16_t)0x0000U)               /*!< EndPoint BULK */
#define USB_EP_CONTROL                       ((uint16_t)0x0200U)               /*!< EndPoint CONTROL */
#define USB_EP_ISOCHRONOUS                   ((uint16_t)0x0400U)               /*!< EndPoint ISOCHRONOUS */
#define USB_EP_INTERRUPT                     ((uint16_t)0x0600U)               /*!< EndPoint INTERRUPT */
#define USB_EP_T_MASK                        ((uint16_t) ~USB_EP_T_FIELD & USB_EPREG_MASK)

#define USB_EPKIND_MASK                      ((uint16_t) ~USB_EP_KIND & USB_EPREG_MASK)            /*!< EP_KIND EndPoint KIND */
                                                                               /*!< STAT_TX[1:0] STATus for TX transfer */
#define USB_EP_TX_DIS                        ((uint16_t)0x0000U)               /*!< EndPoint TX DISabled */
#define USB_EP_TX_STALL                      ((uint16_t)0x0010U)               /*!< EndPoint TX STALLed */
#define USB_EP_TX_NAK                        ((uint16_t)0x0020U)               /*!< EndPoint TX NAKed */
#define USB_EP_TX_VALID                      ((uint16_t)0x0030U)               /*!< EndPoint TX VALID */
#define USB_EPTX_DTOG1                       ((uint16_t)0x0010U)               /*!< EndPoint TX Data TOGgle bit1 */
#define USB_EPTX_DTOG2                       ((uint16_t)0x0020U)               /*!< EndPoint TX Data TOGgle bit2 */
#define USB_EPTX_DTOGMASK  (USB_EPTX_STAT|USB_EPREG_MASK)
                                                                               /*!< STAT_RX[1:0] STATus for RX transfer */
#define USB_EP_RX_DIS                        ((uint16_t)0x0000U)               /*!< EndPoint RX DISabled */
#define USB_EP_RX_STALL                      ((uint16_t)0x1000U)               /*!< EndPoint RX STALLed */
#define USB_EP_RX_NAK                        ((uint16_t)0x2000U)               /*!< EndPoint RX NAKed */
#define USB_EP_RX_VALID                      ((uint16_t)0x3000U)               /*!< EndPoint RX VALID */
#define USB_EPRX_DTOG1                       ((uint16_t)0x1000U)               /*!< EndPoint RX Data TOGgle bit1 */
#define USB_EPRX_DTOG2                       ((uint16_t)0x2000U)               /*!< EndPoint RX Data TOGgle bit1 */
#define USB_EPRX_DTOGMASK  (USB_EPRX_STAT|USB_EPREG_MASK)

#include "fsdev_type.h"

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

#if (CFG_TUSB_MCU == OPT_MCU_AT32F403A_407) || (CFG_TUSB_MCU == OPT_MCU_AT32F413)
static const IRQn_Type fsdev_irq[] = {
  USBFS_H_CAN1_TX_IRQn,
  USBFS_L_CAN1_RX0_IRQn,
  USBFSWakeUp_IRQn
};
enum { FSDEV_IRQ_NUM = TU_ARRAY_SIZE(fsdev_irq) };

#else
  #error "Unsupported MCU"
#endif

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;
  #if (CFG_TUSB_MCU == OPT_MCU_AT32F403A_407) || (CFG_TUSB_MCU == OPT_MCU_AT32F413)
  // AT32F403A/407 devices allow to remap the USB interrupt vectors from
  // shared USB/CAN IRQs to separate CAN and USB IRQs.
  // This dynamically checks if this remap is active to enable the right IRQs.
  if (CRM->intmap_bit.usbintmap) {
    NVIC_EnableIRQ(USBFS_MAPH_IRQn);
    NVIC_EnableIRQ(USBFS_MAPL_IRQn);
    NVIC_EnableIRQ(USBFSWakeUp_IRQn);
  } else
  #endif
  {
    for(uint8_t i=0; i < FSDEV_IRQ_NUM; i++) {
      NVIC_EnableIRQ(fsdev_irq[i]);
    }
  }
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;
  #if (CFG_TUSB_MCU == OPT_MCU_AT32F403A_407) || (CFG_TUSB_MCU == OPT_MCU_AT32F413)
  // AT32F403A/407 devices allow to remap the USB interrupt vectors from
  // shared USB/CAN IRQs to separate CAN and USB IRQs.
  // This dynamically checks if this remap is active to enable the right IRQs.
  if (CRM->intmap_bit.usbintmap) {
    NVIC_DisableIRQ(USBFS_MAPH_IRQn);
    NVIC_DisableIRQ(USBFS_MAPL_IRQn);
    NVIC_DisableIRQ(USBFSWakeUp_IRQn);
  } else
  #endif
  {
    for(uint8_t i=0; i < FSDEV_IRQ_NUM; i++) {
      NVIC_DisableIRQ(fsdev_irq[i]);
    }
  }
}

void dcd_disconnect(uint8_t rhport) {
  (void) rhport;
  /* disable usb phy */
  FSDEV_REG->CNTR |= USB_CNTR_PDWN;
  /* D+ 1.5k pull-up disable, USB->cfg_bit.puo = TRUE; */
  *(uint32_t *)(FSDEV_REG_BASE+0x60) |= (1u<<1);
}

void dcd_connect(uint8_t rhport) {
  (void) rhport;
  /* enable usb phy */
  FSDEV_REG->CNTR &= ~USB_CNTR_PDWN;
  /* Dp 1.5k pull-up enable, USB->cfg_bit.puo = 0; */
  *(uint32_t *)(FSDEV_REG_BASE+0x60) &= ~(1u<<1);
}

#endif
