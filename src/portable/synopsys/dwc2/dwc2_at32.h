/*
 * SPDX-FileCopyrightText: Copyright (c) 2021, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */


#ifndef DWC2_AT32_H_
#define DWC2_AT32_H_

#define DWC2_EP_MAX TUP_DCD_ENDPOINT_MAX

#if CFG_TUSB_MCU == OPT_MCU_AT32F415
  #include <at32f415.h>
  #define OTG1_DFIFO_DEPTH           320
  #define OTG1_IRQn                OTGFS1_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
#elif CFG_TUSB_MCU == OPT_MCU_AT32F435_437
  #include <at32f435_437.h>
  #define OTG1_DFIFO_DEPTH           320
  #define OTG2_DFIFO_DEPTH           320
  #define OTG1_IRQn                OTGFS1_IRQn
  #define OTG2_IRQn                OTGFS2_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
  #define DWC2_OTG2_REG_BASE       0x40040000UL
#elif CFG_TUSB_MCU == OPT_MCU_AT32F423
  #include <at32f423.h>
  #define OTG1_DFIFO_DEPTH           320
  #define OTG1_IRQn                OTGFS1_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
#elif CFG_TUSB_MCU == OPT_MCU_AT32F402_405
  #include <at32f402_405.h>
  #define OTG1_DFIFO_DEPTH           320
  #define OTG2_DFIFO_DEPTH           1024
  #define OTG1_IRQn                OTGFS1_IRQn
  #define OTG2_IRQn                OTGHS_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
  #define DWC2_OTG2_REG_BASE       0x40040000UL //OTGHS
#elif CFG_TUSB_MCU == OPT_MCU_AT32F425
  #include <at32f425.h>
  #define OTG1_DFIFO_DEPTH           320
  #define OTG1_IRQn                OTGFS1_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
#elif CFG_TUSB_MCU == OPT_MCU_AT32F45X
  #include <at32f45x.h>
  #define OTG1_DFIFO_DEPTH           320
  #define OTG1_IRQn                OTGFS1_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
#endif

#ifdef __cplusplus
extern "C" {
#endif

static const dwc2_controller_t _dwc2_controller[] = {
    {.reg_base = DWC2_OTG1_REG_BASE, .irqnum = OTG1_IRQn, .ep_count = DWC2_EP_MAX, .otg_dfifo_depth = OTG1_DFIFO_DEPTH},
#if defined DWC2_OTG2_REG_BASE
    {.reg_base = DWC2_OTG2_REG_BASE, .irqnum = OTG2_IRQn, .ep_count = DWC2_EP_MAX, .otg_dfifo_depth = OTG2_DFIFO_DEPTH}
#endif
};

// MCU specific to enable dwc2 clock/power before any access to register
TU_ATTR_ALWAYS_INLINE static inline void dwc2_clock_init(uint8_t rhport, tusb_role_t role) {
  (void) rhport;
  (void) role;
}

TU_ATTR_ALWAYS_INLINE static inline void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled) {
  (void) role;
  const IRQn_Type irqn = (IRQn_Type) _dwc2_controller[rhport].irqnum;
  if (enabled) {
    NVIC_EnableIRQ(irqn);
  } else {
    NVIC_DisableIRQ(irqn);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void dwc2_dcd_int_enable(uint8_t rhport) {
  NVIC_EnableIRQ(_dwc2_controller[rhport].irqnum);
}

TU_ATTR_ALWAYS_INLINE static inline void dwc2_dcd_int_disable(uint8_t rhport) {
  NVIC_DisableIRQ(_dwc2_controller[rhport].irqnum);
}

TU_ATTR_ALWAYS_INLINE static inline void dwc2_remote_wakeup_delay(void) {
  // try to delay for 1 ms
  uint32_t count = system_core_clock / 1000;
  while (count--) __asm volatile("nop");
}

// MCU specific PHY init, called BEFORE core reset
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_init(dwc2_regs_t *dwc2, uint8_t hs_phy_type) {
  (void) dwc2;
  // Enable on-chip HS PHY
  if (hs_phy_type == GHWCFG2_HSPHY_UTMI || hs_phy_type == GHWCFG2_HSPHY_UTMI_ULPI) {
  } else if (hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED) {
  }
}

// MCU specific PHY deinit, disable PHY power
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_deinit(dwc2_regs_t *dwc2, uint8_t hs_phy_type) {
  (void) hs_phy_type;
  dwc2->stm32_gccfg &= ~(STM32_GCCFG_PWRDWN | STM32_GCCFG_DCDEN | STM32_GCCFG_PDEN);
}

// MCU specific PHY update, it is called AFTER init() and core reset
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_update(dwc2_regs_t *dwc2, uint8_t hs_phy_type) {
  (void) dwc2;
  (void) hs_phy_type;

  dwc2->stm32_gccfg |= STM32_GCCFG_PWRDWN | STM32_GCCFG_DCDEN | STM32_GCCFG_PDEN;
}

#ifdef __cplusplus
}
#endif

#endif /* DWC2_AT32_H_ */
