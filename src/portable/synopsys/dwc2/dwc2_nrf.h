/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ha Thach (tinyusb.org)
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

static const dwc2_controller_t _dwc2_controller[] = {
  { .reg_base = NRF_USBHSCORE0_NS_BASE, .irqnum = USBHS_IRQn, .ep_count = 16, .ep_fifo_size = 12288 },
};

TU_ATTR_ALWAYS_INLINE static inline void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled) {
  (void) rhport;
  (void) role;
  (void) enabled;
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

// MCU specific PHY update, it is called AFTER init() and core reset
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_update(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  (void)dwc2;
  (void)hs_phy_type;
}

#endif
