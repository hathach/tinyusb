/*
 * SPDX-FileCopyrightText: Copyright (c) 2021 Rafael Silva (@perigoso)
 * SPDX-FileCopyrightText: Copyright (c) 2021, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _DWC2_EFM32_H_
#define _DWC2_EFM32_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "em_device.h"

// EFM32 has custom control register before DWC registers
#define DWC2_REG_BASE       (USB_BASE + offsetof(USB_TypeDef, GOTGCTL))
#define DWC2_EP_MAX         7

static const dwc2_controller_t _dwc2_controller[] =
{
  { .reg_base = DWC2_REG_BASE, .irqnum = USB_IRQn, .ep_count = DWC2_EP_MAX, .otg_dfifo_depth = 512 }
};

// MCU specific to enable dwc2 clock/power before any access to register
TU_ATTR_ALWAYS_INLINE static inline void dwc2_clock_init(uint8_t rhport, tusb_role_t role) {
  (void) rhport;
  (void) role;
}

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
//  uint32_t count = SystemCoreClock / 1000;
//  while ( count-- ) __NOP();
}

// MCU specific PHY init, called BEFORE core reset
static inline void dwc2_phy_init(dwc2_regs_t * dwc2, uint8_t hs_phy_type)
{
  (void) dwc2;
  (void) hs_phy_type;

  // Enable PHY
  USB->ROUTE = USB_ROUTE_PHYPEN;
}

// MCU specific PHY deinit, disable PHY power
static inline void dwc2_phy_deinit(dwc2_regs_t * dwc2, uint8_t hs_phy_type) {
  (void) dwc2;
  (void) hs_phy_type;
  // Disable PHY pin
  USB->ROUTE = 0;
}

// MCU specific PHY update, it is called AFTER init() and core reset
static inline void dwc2_phy_update(dwc2_regs_t * dwc2, uint8_t hs_phy_type)
{
  (void) dwc2;
  (void) hs_phy_type;

  // EFM32 Manual: turn around must be 5 (reset & default value)
  // dwc2->gusbcfg = (dwc2->gusbcfg & ~GUSBCFG_TRDT_Msk) | (5u << GUSBCFG_TRDT_Pos);
}

#ifdef __cplusplus
}
#endif

#endif
