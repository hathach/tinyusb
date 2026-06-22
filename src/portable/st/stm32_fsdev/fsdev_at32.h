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

#define FSDEV_USE_SBUF_ISO 0
#define FSDEV_REG_BASE  (APB1PERIPH_BASE + 0x00005C00UL)
#define FSDEV_PMA_BASE  (APB1PERIPH_BASE + 0x00006000UL)

#ifndef CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP
  #define CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP 0
#endif

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

TU_ATTR_ALWAYS_INLINE static inline void fsdev_int_enable(uint8_t rhport) {
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

TU_ATTR_ALWAYS_INLINE static inline void fsdev_int_disable(uint8_t rhport) {
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

TU_ATTR_ALWAYS_INLINE static inline void fsdev_disconnect(uint8_t rhport) {
  (void) rhport;
  /* disable usb phy */
  *(volatile uint32_t*)(FSDEV_REG_BASE + 0x40) |= U_CNTR_PDWN;
  /* D+ 1.5k pull-up disable, USB->cfg_bit.puo = TRUE; */
  *(volatile uint32_t *)(FSDEV_REG_BASE+0x60) |= (1u<<1);
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_connect(uint8_t rhport) {
  (void) rhport;
  /* enable usb phy */
  *(volatile uint32_t*)(FSDEV_REG_BASE + 0x40) &= ~U_CNTR_PDWN;
  /* Dp 1.5k pull-up enable, USB->cfg_bit.puo = 0; */
  *(volatile uint32_t *)(FSDEV_REG_BASE+0x60) &= ~(1u<<1);
}

#endif
