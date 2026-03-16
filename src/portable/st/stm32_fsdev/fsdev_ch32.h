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
/** <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  */

#ifndef TUSB_FSDEV_CH32_H
#define TUSB_FSDEV_CH32_H

#include "common/tusb_compiler.h"

// https://github.com/openwch/ch32v307/pull/90
// https://github.com/openwch/ch32v20x/pull/12
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

#if CFG_TUSB_MCU == OPT_MCU_CH32F20X
  #include <ch32f20x.h>
#elif CFG_TUSB_MCU == OPT_MCU_CH32V20X
  #include <ch32v20x.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
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

#if CFG_TUSB_MCU == OPT_MCU_CH32V20X
static const IRQn_Type fsdev_irq[] = {
  USB_HP_CAN1_TX_IRQn,
  USB_LP_CAN1_RX0_IRQn,
  USBWakeUp_IRQn
};
enum { FSDEV_IRQ_NUM = TU_ARRAY_SIZE(fsdev_irq) };
#else
  #error "Unsupported MCU"
#endif

TU_ATTR_ALWAYS_INLINE static inline void fsdev_int_enable(uint8_t rhport) {
  (void)rhport;
  for(uint8_t i=0; i < FSDEV_IRQ_NUM; i++) {
    NVIC_EnableIRQ(fsdev_irq[i]);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_int_disable(uint8_t rhport) {
  (void)rhport;
  for(uint8_t i=0; i < FSDEV_IRQ_NUM; i++) {
    NVIC_DisableIRQ(fsdev_irq[i]);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_disconnect(uint8_t rhport) {
  (void) rhport;
  EXTEN->EXTEN_CTR &= ~EXTEN_USBD_PU_EN;
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_connect(uint8_t rhport) {
  (void) rhport;
  EXTEN->EXTEN_CTR |= EXTEN_USBD_PU_EN;
}

#endif
