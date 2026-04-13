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
 *
 * Modification
 * - Gabriel Koppenstein add nRF54LM20 support
 */
#ifndef TUSB_DWC2_NRF_H
#define TUSB_DWC2_NRF_H

// NRF is device only without OTG support
#include "nrf.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif

#include <soc/nrfx_coredep.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#define DWC2_EP_MAX 16

// Use the auto-resolving peripheral pointer (respects TrustZone secure/non-secure mapping)
#if defined(NRF54LM20A_ENGA_XXAA)
  #define DWC2_REG_BASE ((uintptr_t)NRF_USBHSCORE)
#else
  #define DWC2_REG_BASE ((uintptr_t)NRF_USBHSCORE0)
#endif

static const dwc2_controller_t _dwc2_controller[] = {
  {.reg_base = DWC2_REG_BASE, .irqnum = USBHS_IRQn, .ep_count = 16, .otg_dfifo_depth = 3072},
};

// MCU specific to enable dwc2 clock/power before any access to register
TU_ATTR_ALWAYS_INLINE static inline void dwc2_clock_init(uint8_t rhport, tusb_role_t role) {
  (void) rhport;
  (void) role;

  #if defined(NRF54LM20A_ENGA_XXAA)
  // Start the USB voltage regulator
  NRF_VREGUSB->TASKS_START = VREGUSB_TASKS_START_TASKS_START_Trigger;

  // Based on Zephyr usbhs_enable_core() in drivers/usb/udc/udc_dwc2_vendor_quirks.h
  // Step 1: Power up core only (PHY not yet)
  NRF_USBHS->ENABLE = USBHS_ENABLE_CORE_Msk;

  // Step 2: Override ID=Device (bit 31), and temporarily override VBUSVALID
  NRF_USBHS->PHY.OVERRIDEVALUES = (USBHS_PHY_OVERRIDEVALUES_ID_Device << USBHS_PHY_OVERRIDEVALUES_ID_Pos);
  NRF_USBHS->PHY.INPUTOVERRIDE = USBHS_PHY_INPUTOVERRIDE_ID_Msk | USBHS_PHY_INPUTOVERRIDE_VBUSVALID_Msk;

  // Step 3: Release PHY power-on reset by enabling PHY
  NRF_USBHS->ENABLE = USBHS_ENABLE_PHY_Msk | USBHS_ENABLE_CORE_Msk;

  // Step 4: Wait 45us for PHY clock to start
  nrfx_coredep_delay_us(45);

  // Step 5: Release DWC2 reset
  NRF_USBHS->TASKS_START = USBHS_TASKS_START_TASKS_START_Trigger;

  // Step 6: Wait for clock to start to avoid hang on too early register read
  nrfx_coredep_delay_us(2);

  // Step 7: Clear VBUSVALID override (keep ID=Device override)
  // DWC2 is now in Non-Driving opmode; D+ pull-up will activate when DWC2 clears DCTL SftDiscon
  NRF_USBHS->PHY.INPUTOVERRIDE = USBHS_PHY_INPUTOVERRIDE_ID_Msk;

  // Barrier: USBHS wrapper (0x5005A000) and USBHSCORE (0x50020000) are separate
  // peripheral blocks. Ensure the ENABLE/TASKS_START writes have propagated from
  // the Cortex-M33 write buffer to hardware before anyone reads DWC2 core regs.
  __DSB();
  #endif
}

TU_ATTR_ALWAYS_INLINE static inline void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled) {
  (void)rhport;
  (void)role;
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
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_init(dwc2_regs_t *dwc2, uint8_t hs_phy_type) {
  (void)dwc2;
  (void)hs_phy_type;
}

// MCU specific PHY deinit, disable PHY power
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_deinit(dwc2_regs_t *dwc2, uint8_t hs_phy_type) {
  (void)dwc2;
  (void)hs_phy_type;
}

// MCU specific PHY update, it is called AFTER init() and core reset
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_update(dwc2_regs_t *dwc2, uint8_t hs_phy_type) {
  (void)dwc2;
  (void)hs_phy_type;
}

// nRF54 Cortex-M33 has no D-cache, provide no-op stubs for DMA mode
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_clean(const void *addr, uint32_t data_size) {
  (void)addr;
  (void)data_size;
  return true;
}
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_invalidate(const void *addr, uint32_t data_size) {
  (void)addr;
  (void)data_size;
  return true;
}
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_clean_invalidate(const void *addr, uint32_t data_size) {
  (void)addr;
  (void)data_size;
  return true;
}

#endif
