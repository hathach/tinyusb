/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 TinyUSB contributors
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

#ifndef TUSB_DWC2_IFX_H
#define TUSB_DWC2_IFX_H

// Infineon PSOC Edge E84 (PSE84) High-Speed USB
// The USBHS block is a Synopsys DWC2 OTG core wrapped by an Infineon subsystem
// (SS) register group that handles clocking and PHY bring-up. The bring-up
// sequence mirrors Zephyr's drivers/usb/udc/udc_dwc2_infineon_usbhs.h quirk.

// We deliberately avoid the PDL umbrella <cy_pdl.h>: it pulls in every PDL
// driver (cy_axidmac.h, cy_scb_uart.h, cy_sd_host.h, ...) whose inline register
// accessors emit -Wcast-qual/-Wsign-compare warnings into every translation
// unit that includes this header. We only need:
//  - the device header: USBHS_Type, USBHS_BASE, USBHS_SS_* masks, IRQ enum
//  - cy_sysclk.h: Cy_SysClk_PeriGroup* clock helpers
// cy_sysclk.h does NOT include the noisy driver headers.
#include <cy_device_headers.h>
#include <cy_sysclk.h>

#define DWC2_EP_MAX 9

// DWC2 OTG core base. On the secure (S) m33 partition this is 0x54900000.
#define DWC2_REG_BASE ((uintptr_t) USBHS_BASE)

// USBHS peripheral-group clock parameters (from the PSE84 device tree / datasheet:
// mmio-peri-inst, mmio-peri-group, mmio-reg-bit-pos, mmio-peri-hf-src, mmio-peri-div).
#define IFX_USBHS_PERI_INST    1
#define IFX_USBHS_PERI_GROUP   3
#define IFX_USBHS_REG_BIT_POS  5
#define IFX_USBHS_PERI_HF_SRC  1
#define IFX_USBHS_PERI_DIV     3

// otg_dfifo_depth = GHWCFG3.DfifoDepth (bits [31:16]) in 32-bit words.
// Read live from this core: GHWCFG3 = 0x1A4F80E8 -> 0x1A4F = 6735 words.
static const dwc2_controller_t _dwc2_controller[] = {
  {
    .reg_base = DWC2_REG_BASE,
    .irqnum = usbhs_interrupt_usbhsctrl_IRQn,
    .ep_count = DWC2_EP_MAX,
    .ep_in_count = 9, // num-in-eps
    .otg_dfifo_depth = 6735,
  },
};

// Enable the Infineon USB subsystem (SS) wrapper so the DWC2 core register
// block becomes accessible. Must run before any core register read (gsnpsid,
// ghwcfg, ...). Idempotent. Mirrors usbhs_ifx_phy_enable() in Zephyr's
// drivers/usb/udc/udc_dwc2_infineon_usbhs.h.
TU_ATTR_ALWAYS_INLINE static inline void ifx_usbhs_ss_enable(USBHS_Type* wrapper, tusb_role_t role) {
  // SUBSYSTEM_CTL.USB_MODE selects the PHY role: 1 = host, 0 = device. Host
  // mode engages the port pull-downs; device mode asserts the D+ pull-up. It
  // must match the core role or an idle host port floats D+ high and reports a
  // spurious full-speed connect.
  if (role == TUSB_ROLE_HOST) {
    wrapper->SS.SUBSYSTEM_CTL |= USBHS_SS_SUBSYSTEM_CTL_USB_MODE_Msk;
  } else {
    wrapper->SS.SUBSYSTEM_CTL &= ~USBHS_SS_SUBSYSTEM_CTL_USB_MODE_Msk;
  }
  wrapper->SS.SUBSYSTEM_CTL |= USBHS_SS_SUBSYSTEM_CTL_AHB_MASTER_SYNC_Msk;
  wrapper->SS.SUBSYSTEM_CTL &= ~USBHS_SS_SUBSYSTEM_CTL_USB_CTRL_SCALEDOWN_MODE_Msk;
  // SUBSYSTEM_CTL must be enabled prior to configuring other subsystem registers
  wrapper->SS.SUBSYSTEM_CTL |= USBHS_SS_SUBSYSTEM_CTL_SS_ENABLE_Msk;

  wrapper->SS.PHY_FUNC_CTL_2 |= USBHS_SS_PHY_FUNC_CTL_2_EFUSE_SEL_Msk;
  wrapper->SS.PHY_FUNC_CTL_2 |= USBHS_SS_PHY_FUNC_CTL_2_RES_TUNING_SEL_Msk;

  __DSB();
}

// MCU specific to enable dwc2 clock before any access to register
TU_ATTR_ALWAYS_INLINE static inline void dwc2_clock_init(uint8_t rhport, tusb_role_t role) {
  // Enable and divide the USBHS peripheral-group clock (Infineon SS clock tree).
  Cy_SysClk_PeriGroupSlaveInit(IFX_USBHS_PERI_INST, IFX_USBHS_PERI_GROUP,
                               IFX_USBHS_REG_BIT_POS, IFX_USBHS_PERI_HF_SRC);
  Cy_SysClk_PeriGroupSetDivider((IFX_USBHS_PERI_INST << 8) | IFX_USBHS_PERI_GROUP,
                                IFX_USBHS_PERI_DIV);
  __DSB();

  // Enable the SS subsystem now so the DWC2 core registers are accessible when
  // the common dcd init reads gsnpsid/ghwcfg (which happens before dwc2_phy_init).
  ifx_usbhs_ss_enable((USBHS_Type*) _dwc2_controller[rhport].reg_base, role);

  // After SS_ENABLE the DWC2 core's AHB-slave (CSR) interface needs a short
  // settling time before it returns valid data. The common dcd init reads
  // gsnpsid right away (check_dwc2) and TU_ASSERTs on a bad value, so poll the
  // constant core ID register here until it reports a valid DWC2 signature.
  // Bounded so a genuinely absent/clock-less core does not hang init forever
  // (check_dwc2 will then fail exactly as it would without this loop).
  dwc2_regs_t* const dwc2 = (dwc2_regs_t*) _dwc2_controller[rhport].reg_base;
  for (uint32_t i = 0; i < 100000u; i++) {
    const uint32_t id = dwc2->gsnpsid & TU_GENMASK(31, 16);
    if (id == DWC2_OTG_ID || id == DWC2_FS_IOT_ID || id == DWC2_HS_IOT_ID) {
      break;
    }
  }
}

TU_ATTR_ALWAYS_INLINE static inline void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled) {
  (void) rhport;
  (void) role;
  if (enabled) {
    NVIC_EnableIRQ(usbhs_interrupt_usbhsctrl_IRQn);
  } else {
    NVIC_DisableIRQ(usbhs_interrupt_usbhsctrl_IRQn);
  }
}

#define dwc2_dcd_int_enable(_rhport)  dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, true)
#define dwc2_dcd_int_disable(_rhport) dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, false)

TU_ATTR_ALWAYS_INLINE static inline void dwc2_remote_wakeup_delay(void) {
}

// MCU specific PHY init, called BEFORE core reset. Enables the Infineon USB
// subsystem (SS) so the DWC2 core registers become accessible. Already enabled
// in dwc2_clock_init; re-asserted here (idempotent) to match the documented
// PHY bring-up sequence.
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_init(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  (void) hs_phy_type;
#if CFG_TUH_ENABLED
  ifx_usbhs_ss_enable((USBHS_Type*) dwc2, TUSB_ROLE_HOST);
#else
  ifx_usbhs_ss_enable((USBHS_Type*) dwc2, TUSB_ROLE_DEVICE);
#endif
}

// MCU specific PHY deinit, disable subsystem/PHY power
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_deinit(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  (void) hs_phy_type;
  USBHS_Type* wrapper = (USBHS_Type*) dwc2;
  wrapper->SS.SUBSYSTEM_CTL &= ~USBHS_SS_SUBSYSTEM_CTL_SS_ENABLE_Msk;
}

// MCU specific PHY update, called AFTER init() and core reset
TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_update(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  (void) dwc2;
  (void) hs_phy_type;
}

// DMA cache maintenance: the PSE84 Cortex-M33 has no data cache
// (__DCACHE_PRESENT == 0), so no cache maintenance is required. These no-op
// stubs are therefore always correct on this core.
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_clean(const void* addr, uint32_t data_size) {
  (void) addr;
  (void) data_size;
  return true;
}
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_invalidate(const void* addr, uint32_t data_size) {
  (void) addr;
  (void) data_size;
  return true;
}
TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_clean_invalidate(const void* addr, uint32_t data_size) {
  (void) addr;
  (void) data_size;
  return true;
}

#endif
