/*
 * The MIT License (MIT)
 *
 * Copyright(c) N Conrad
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_FSDEV_STM32_H
#define TUSB_FSDEV_STM32_H

#if CFG_TUSB_MCU == OPT_MCU_STM32C0
  #include "stm32c0xx.h"
  #define FSDEV_HAS_SBUF_ISO 1

#elif CFG_TUSB_MCU == OPT_MCU_STM32F0
  #include "stm32f0xx.h"
  #define FSDEV_HAS_SBUF_ISO 0
  // F0x2 models are crystal-less
  // All have internal D+ pull-up
  // 070RB:    2 x 16 bits/word memory     LPM Support, BCD Support
  // PMA dedicated to USB (no sharing with CAN)

#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  #include "stm32f1xx.h"
  #define FSDEV_HAS_SBUF_ISO 0
  // NO internal Pull-ups
  //         *B, and *C:    2 x 16 bits/word

#elif CFG_TUSB_MCU == OPT_MCU_STM32F3
  #include "stm32f3xx.h"
  #define FSDEV_HAS_SBUF_ISO 0
  // NO internal Pull-ups. PMA dedicated to USB (no sharing with CAN)
  // xB, and xC: 512 bytes
  // x6, x8, xD, and xE: 1024 bytes + LPM Support. When CAN clock is enabled, USB can use the first 768 bytes ONLY.

#elif CFG_TUSB_MCU == OPT_MCU_STM32G0
  #include "stm32g0xx.h"
  #define FSDEV_HAS_SBUF_ISO 1

#elif CFG_TUSB_MCU == OPT_MCU_STM32G4
  #include "stm32g4xx.h"
  #define FSDEV_HAS_SBUF_ISO 0

#elif CFG_TUSB_MCU == OPT_MCU_STM32H5
  #include "stm32h5xx.h"
  #define FSDEV_HAS_SBUF_ISO 1

#elif CFG_TUSB_MCU == OPT_MCU_STM32L0
  #include "stm32l0xx.h"
  #define FSDEV_HAS_SBUF_ISO 0

#elif CFG_TUSB_MCU == OPT_MCU_STM32L1
  #include "stm32l1xx.h"
  #define FSDEV_HAS_SBUF_ISO 0

#elif CFG_TUSB_MCU == OPT_MCU_STM32L4
  #include "stm32l4xx.h"
  #define FSDEV_HAS_SBUF_ISO 0

#elif CFG_TUSB_MCU == OPT_MCU_STM32L5
  #include "stm32l5xx.h"
  #define FSDEV_HAS_SBUF_ISO 0

  #ifndef USB_PMAADDR
    #define USB_PMAADDR (USB_BASE + (USB_PMAADDR_NS - USB_BASE_NS))
  #endif

#elif CFG_TUSB_MCU == OPT_MCU_STM32U0
  #include "stm32u0xx.h"
  #define FSDEV_HAS_SBUF_ISO 1

#elif CFG_TUSB_MCU == OPT_MCU_STM32U3
  #include "stm32u3xx.h"
  #define FSDEV_HAS_SBUF_ISO 1

#elif CFG_TUSB_MCU == OPT_MCU_STM32U5
  #include "stm32u5xx.h"
  #define FSDEV_HAS_SBUF_ISO 1

#elif CFG_TUSB_MCU == OPT_MCU_STM32WB
  #include "stm32wbxx.h"
  #define FSDEV_HAS_SBUF_ISO 0

#else
  #error You are using an untested or unimplemented STM32 variant. Please update the driver.
#endif

//--------------------------------------------------------------------+
// Register and PMA Base Address
//--------------------------------------------------------------------+
#ifndef FSDEV_REG_BASE
#if defined(USB_BASE)
  #define FSDEV_REG_BASE USB_BASE
#elif defined(USB_DRD_BASE)
  #define FSDEV_REG_BASE USB_DRD_BASE
#elif defined(USB_DRD_FS_BASE)
  #define FSDEV_REG_BASE USB_DRD_FS_BASE
#else
  #error "FSDEV_REG_BASE not defined"
#endif
#endif

#ifndef FSDEV_PMA_BASE
#if defined(USB_PMAADDR)
  #define FSDEV_PMA_BASE USB_PMAADDR
#elif defined(USB_DRD_PMAADDR)
  #define FSDEV_PMA_BASE USB_DRD_PMAADDR
#else
  #error "FSDEV_PMA_BASE not defined"
#endif
#endif

#ifndef FSDEV_HAS_SBUF_ISO
  #error "FSDEV_HAS_SBUF_ISO not defined"
#endif

#ifndef CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP
  // Default configuration for double-buffered isochronous endpoints:
  // - Enable double buffering on devices with >1KB Packet Memory Area (PMA)
  //   to improve isochronous transfer reliability and performance
  // - Disable on devices with limited PMA to conserve memory space
  #if CFG_TUSB_FSDEV_PMA_SIZE > 1024u
    #define CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP 1
  #else
    #define CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP 0
  #endif
#endif

#if FSDEV_HAS_SBUF_ISO != 0 && CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP == 0
  // SBUF_ISO configuration:
  // - Some STM32 devices have special hardware support for single-buffered isochronous endpoints
  // - When SBUF_ISO bit is available and double buffering is disabled:
  //   Enable SBUF_ISO to optimize endpoint register usage (one half of endpoint pair register)
  #define FSDEV_USE_SBUF_ISO 1
#else
  // When either:
  // - Hardware doesn't support SBUF_ISO feature, or
  // - Double buffering is enabled for isochronous endpoints
  // We must use the entire endpoint pair register
  #define FSDEV_USE_SBUF_ISO 0
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

#if TU_CHECK_MCU(OPT_MCU_STM32L1) && !defined(USBWakeUp_IRQn)
  #define USBWakeUp_IRQn USB_FS_WKUP_IRQn
#endif

static const IRQn_Type fsdev_irq[] = {
  #if TU_CHECK_MCU(OPT_MCU_STM32F0, OPT_MCU_STM32L0, OPT_MCU_STM32L4, OPT_MCU_STM32U5)
    USB_IRQn,
  #elif TU_CHECK_MCU(OPT_MCU_STM32L5, OPT_MCU_STM32U3)
    USB_FS_IRQn,
  #elif TU_CHECK_MCU(OPT_MCU_STM32C0, OPT_MCU_STM32H5, OPT_MCU_STM32U0)
    USB_DRD_FS_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32G0
    #ifdef STM32G0B0xx
    USB_IRQn,
    #else
    USB_UCPD1_2_IRQn,
    #endif
  #elif CFG_TUSB_MCU == OPT_MCU_STM32F1
    USB_HP_CAN1_TX_IRQn,
    USB_LP_CAN1_RX0_IRQn,
    USBWakeUp_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32F3
    USB_HP_CAN_TX_IRQn,
    USB_LP_CAN_RX0_IRQn,
    USBWakeUp_IRQn,
  #elif TU_CHECK_MCU(OPT_MCU_STM32G4, OPT_MCU_STM32L1)
    USB_HP_IRQn,
    USB_LP_IRQn,
    USBWakeUp_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32WB
    USB_HP_IRQn,
    USB_LP_IRQn,
  #else
    #error Unknown arch in USB driver
  #endif
};
enum { FSDEV_IRQ_NUM = TU_ARRAY_SIZE(fsdev_irq) };

TU_ATTR_ALWAYS_INLINE static inline void fsdev_int_enable(uint8_t rhport) {
  (void)rhport;

  // forces write to RAM before allowing ISR to execute
  __DSB(); __ISB();

  #if CFG_TUSB_MCU == OPT_MCU_STM32F3 && defined(SYSCFG_CFGR1_USB_IT_RMP)
  // Some STM32F302/F303 devices allow to remap the USB interrupt vectors from
  // shared USB/CAN IRQs to separate CAN and USB IRQs.
  // This dynamically checks if this remap is active to enable the right IRQs.
  if (SYSCFG->CFGR1 & SYSCFG_CFGR1_USB_IT_RMP) {
    NVIC_EnableIRQ(USB_HP_IRQn);
    NVIC_EnableIRQ(USB_LP_IRQn);
    NVIC_EnableIRQ(USBWakeUp_RMP_IRQn);
  } else
  #endif
  {
    for (uint8_t i = 0; i < FSDEV_IRQ_NUM; i++) {
      NVIC_EnableIRQ(fsdev_irq[i]);
    }
  }
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_int_disable(uint8_t rhport) {
  (void)rhport;

  #if CFG_TUSB_MCU == OPT_MCU_STM32F3 && defined(SYSCFG_CFGR1_USB_IT_RMP)
  // Some STM32F302/F303 devices allow to remap the USB interrupt vectors from
  // shared USB/CAN IRQs to separate CAN and USB IRQs.
  // This dynamically checks if this remap is active to enable the right IRQs.
  if (SYSCFG->CFGR1 & SYSCFG_CFGR1_USB_IT_RMP) {
    NVIC_DisableIRQ(USB_HP_IRQn);
    NVIC_DisableIRQ(USB_LP_IRQn);
    NVIC_DisableIRQ(USBWakeUp_RMP_IRQn);
  } else
  #endif
  {
    for (uint8_t i = 0; i < FSDEV_IRQ_NUM; i++) {
      NVIC_DisableIRQ(fsdev_irq[i]);
    }
  }

  // CMSIS has a membar after disabling interrupts
}

//--------------------------------------------------------------------+
// Connect / Disconnect
//--------------------------------------------------------------------+

#if defined(USB_BCDR_DPPU)

TU_ATTR_ALWAYS_INLINE static inline void fsdev_disconnect(uint8_t rhport) {
  (void)rhport;
  FSDEV_REG->BCDR &= ~U_BCDR_DPPU;
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_connect(uint8_t rhport) {
  (void)rhport;
  FSDEV_REG->BCDR |= U_BCDR_DPPU;
}

#elif defined(SYSCFG_PMC_USB_PU) // works e.g. on STM32L151

TU_ATTR_ALWAYS_INLINE static inline void fsdev_disconnect(uint8_t rhport) {
  (void)rhport;
  SYSCFG->PMC &= ~(SYSCFG_PMC_USB_PU);
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_connect(uint8_t rhport) {
  (void)rhport;
  SYSCFG->PMC |= SYSCFG_PMC_USB_PU;
}
#endif

#endif /* TUSB_FSDEV_STM32_H */
