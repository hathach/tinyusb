/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ha Thach (tinyusb.org)
 * Copyright (c) 2026, Gabriel Koppenstein
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
#ifndef TUSB_DWC2_NRF_H
#define TUSB_DWC2_NRF_H

#include "nrf.h"

#define DWC2_EP_MAX 16

// Use the auto-resolving peripheral pointer (respects TrustZone secure/non-secure mapping)
#if defined(NRF54LM20A_ENGA_XXAA)
  #define _DWC2_NRF_REG_BASE ((uintptr_t) NRF_USBHSCORE)
#else
  #define _DWC2_NRF_REG_BASE ((uintptr_t) NRF_USBHSCORE0)
#endif

static const dwc2_controller_t _dwc2_controller[] = {
  { .reg_base = _DWC2_NRF_REG_BASE, .irqnum = USBHS_IRQn, .ep_count = 16, .ep_fifo_size = 12160 },
};

TU_ATTR_ALWAYS_INLINE static inline void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled) {
  (void) rhport;
  (void) role;
  if (enabled) {
    NVIC_EnableIRQ(USBHS_IRQn);
  } else {
    NVIC_DisableIRQ(USBHS_IRQn);
  }
}

#define dwc2_dcd_int_enable(_rhport)  dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, true)
#define dwc2_dcd_int_disable(_rhport) dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, false)

TU_ATTR_ALWAYS_INLINE static inline void dwc2_remote_wakeup_delay(void) {
}

// MCU specific PHY init, called BEFORE core reset
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_init(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  (void)dwc2;
  (void)hs_phy_type;
}

// MCU specific PHY deinit, disable PHY power
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_deinit(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  (void)dwc2;
  (void)hs_phy_type;
}

// MCU specific PHY update, it is called AFTER init() and core reset
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_update(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  (void)dwc2;
  (void)hs_phy_type;
}

// nRF54 Cortex-M33 has no D-cache, provide no-op stubs for DMA mode
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_clean(const void* addr, uint32_t data_size) {
  (void)addr; (void)data_size; return true;
}
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_invalidate(const void* addr, uint32_t data_size) {
  (void)addr; (void)data_size; return true;
}
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_clean_invalidate(const void* addr, uint32_t data_size) {
  (void)addr; (void)data_size; return true;
}

#endif
