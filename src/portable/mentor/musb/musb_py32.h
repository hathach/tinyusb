/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
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

#ifndef TUSB_MUSB_PY32_H_
#define TUSB_MUSB_PY32_H_

#include "py32f0xx.h"

#ifdef __cplusplus
extern "C" {
#endif

// PY32 does not expose generic MUSB shared FIFO allocation registers; this
// selects the existing TX_MODE path required for IN endpoints.
#define MUSB_CFG_SHARED_FIFO 1
#define MUSB_CFG_DYNAMIC_FIFO 0
#define MUSB_INTR_EP_TX_RX_SWAP 1

static const uintptr_t MUSB_BASES[] = { USBD_BASE };
static const IRQn_Type musb_irqs[] = { USB_IRQn };

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_phy_init(uint8_t rhport) {
  musb_regs_t* musb_regs = MUSB_REGS(rhport);

  musb_regs->index = 0;
  musb_regs->faddr = 0;
  musb_regs->intr_usben = MUSB_IE_RESET | MUSB_IE_RESUME | MUSB_IE_SUSPND;
  musb_regs->intr_txen = TU_BIT(0);
  musb_regs->intr_rxen = 0;
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_enable(uint8_t rhport) {
  NVIC_EnableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_disable(uint8_t rhport) {
  NVIC_DisableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_clear(uint8_t rhport) {
  NVIC_ClearPendingIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE static inline unsigned musb_dcd_get_int_enable(uint8_t rhport) {
  return NVIC_GetEnableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_handler_enter(uint8_t rhport) {
  (void) rhport;
}

#ifdef __cplusplus
}
#endif

#endif
