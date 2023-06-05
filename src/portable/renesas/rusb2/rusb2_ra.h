/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Rafael Silva (@perigoso)
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

#ifndef _RUSB2_RA_H_
#define _RUSB2_RA_H_

#ifdef __cplusplus
extern "C" {
#endif

/* renesas fsp api */
#include "bsp_api.h"

extern IRQn_Type _usb_fs_irqn;
extern IRQn_Type _usb_hs_irqn;

#if !defined(CFG_TUSB_RHPORT0_MODE) && !defined(CFG_TUSB_RHPORT1_MODE)
// fallback
#define CFG_TUSB_RHPORT0_MODE   ( CFG_TUD_ENABLED ? OPT_MODE_DEVICE : OPT_MODE_HOST )
#define CFG_TUSB_RHPORT1_MODE   0
#endif

#if defined(__ICCARM__)
  #define __builtin_ctz(x)             __iar_builtin_CLZ(__iar_builtin_RBIT(x))
#endif

TU_ATTR_ALWAYS_INLINE static inline void rusb2_int_enable(uint8_t rhport)
{
#ifdef CFG_TUSB_RHPORT1_MODE
#if (CFG_TUSB_RHPORT1_MODE != 0)
  if (rhport == 1) {
    NVIC_EnableIRQ(_usb_hs_irqn);
  }
#endif
#endif

#ifdef CFG_TUSB_RHPORT0_MODE
#if  (CFG_TUSB_RHPORT0_MODE != 0)
  if (rhport == 0) {
    NVIC_EnableIRQ(_usb_fs_irqn);
  }
#endif
#endif
}

TU_ATTR_ALWAYS_INLINE static inline void rusb2_int_disable(uint8_t rhport)
{
#ifdef CFG_TUSB_RHPORT1_MODE
#if (CFG_TUSB_RHPORT1_MODE != 0)
  if (rhport == 1) {
    NVIC_DisableIRQ(_usb_hs_irqn);
  }
#endif
#endif

#ifdef CFG_TUSB_RHPORT0_MODE
#if (CFG_TUSB_RHPORT0_MODE != 0)
  if (rhport == 0) {
    NVIC_DisableIRQ(_usb_fs_irqn);
  }
#endif
#endif
}

// MCU specific PHY init
TU_ATTR_ALWAYS_INLINE static inline void rusb2_phy_init(void)
{
}

#ifdef __cplusplus
}
#endif

#endif /* _RUSB2_RA_H_ */
