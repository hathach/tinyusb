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
#ifndef TUSB_FSDEV_APM32_H
#define TUSB_FSDEV_APM32_H

#include "common/tusb_compiler.h"

#if CFG_TUSB_MCU == OPT_MCU_APM32F0XX
  #include "apm32f0xx.h"
#endif

#define FSDEV_USE_SBUF_ISO 0
#define FSDEV_REG_BASE  ((uint32_t)(USBD_BASE))
#define FSDEV_PMA_BASE  ((uint32_t)(USBD_BASE + 0x400UL))

#ifndef CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP
  #define CFG_TUD_FSDEV_DOUBLE_BUFFERED_ISO_EP 0
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static const IRQn_Type fsdev_irq[] = {
  USBD_IRQn
};
enum { FSDEV_IRQ_NUM = TU_ARRAY_SIZE(fsdev_irq) };

TU_ATTR_ALWAYS_INLINE static inline void fsdev_int_enable(uint8_t rhport) {
  (void)rhport;
  for (uint8_t i = 0; i < FSDEV_IRQ_NUM; i++) {
    NVIC_EnableIRQ(fsdev_irq[i]);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_int_disable(uint8_t rhport) {
  (void)rhport;
  for (uint8_t i = 0; i < FSDEV_IRQ_NUM; i++) {
    NVIC_DisableIRQ(fsdev_irq[i]);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_disconnect(uint8_t rhport) {
  (void) rhport;
  FSDEV_REG->CNTR |= U_CNTR_PDWN;
  FSDEV_REG->BCDR &= ~U_BCDR_DPPU;
}

TU_ATTR_ALWAYS_INLINE static inline void fsdev_connect(uint8_t rhport) {
  (void) rhport;
  FSDEV_REG->CNTR &= ~U_CNTR_PDWN;
  FSDEV_REG->BCDR |= U_BCDR_DPPU;
}

#endif
