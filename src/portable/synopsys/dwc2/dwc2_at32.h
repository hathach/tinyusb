/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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


#ifndef DWC2_AT32_H_
#define DWC2_AT32_H_

#include <at32f415.h>

#ifdef __cplusplus
 extern "C" {
#endif

#define DWC2_REG_BASE       0x50000000UL
#define DWC2_EP_MAX         4

static const dwc2_controller_t _dwc2_controller[] =
{
  { .reg_base = DWC2_REG_BASE, .irqnum = OTGFS1_IRQn, .ep_count = DWC2_EP_MAX, .ep_fifo_size = 1280 }
};

TU_ATTR_ALWAYS_INLINE
static inline void dwc2_dcd_int_enable(uint8_t rhport)
{
  NVIC_EnableIRQ(_dwc2_controller[rhport].irqnum);
}

TU_ATTR_ALWAYS_INLINE
static inline void dwc2_dcd_int_disable (uint8_t rhport)
{
  NVIC_DisableIRQ(_dwc2_controller[rhport].irqnum);
}

static inline void dwc2_remote_wakeup_delay(void)
{
  // try to delay for 1 ms
  uint32_t count = system_core_clock / 1000;
  while ( count-- ) __asm volatile ("nop");
}

// MCU specific PHY init, called BEFORE core reset
static inline void dwc2_phy_init(dwc2_regs_t * dwc2, uint8_t hs_phy_type)
{
  (void) dwc2;
  (void) hs_phy_type;

  // nothing to do
}

// MCU specific PHY update, it is called AFTER init() and core reset
static inline void dwc2_phy_update(dwc2_regs_t * dwc2, uint8_t hs_phy_type)
{
  (void) dwc2;
  (void) hs_phy_type;

  dwc2->stm32_gccfg |= STM32_GCCFG_PWRDWN | STM32_GCCFG_DCDEN | STM32_GCCFG_PDEN;
}

#ifdef __cplusplus
}
#endif

#endif /* DWC2_GD32_H_ */
