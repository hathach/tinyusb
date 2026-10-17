/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Geehy Semiconductor
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

#ifndef DWC2_APM32_H_
#define DWC2_APM32_H_

#ifdef __cplusplus
extern "C" {
#endif

#if CFG_TUSB_MCU == OPT_MCU_APM32F107
  #include "apm32f10x.h"

  #define EP_MAX_FS       4
  #define DFIFO_DEPTH_FS  320

  #ifndef USB_OTG_FS_PERIPH_BASE
    #define USB_OTG_FS_PERIPH_BASE  0x50000000UL
  #endif
#endif

// OTG HS always has higher number of endpoints than FS
#ifdef USB_OTG_HS_PERIPH_BASE
  #define DWC2_EP_MAX   EP_MAX_HS
#else
  #define DWC2_EP_MAX   EP_MAX_FS
#endif

// On APM32 for consistency we associate
// - Port0 to OTG_FS
static const dwc2_controller_t _dwc2_controller[] = {
    #ifdef USB_OTG_FS_PERIPH_BASE
    { .reg_base = USB_OTG_FS_PERIPH_BASE, .irqnum = OTG_FS_IRQn, .ep_count = EP_MAX_FS, .ep_in_count = EP_MAX_FS, .otg_dfifo_depth = DFIFO_DEPTH_FS },
    #endif

    #ifdef USB_OTG_HS_PERIPH_BASE
    { .reg_base = USB_OTG_HS_PERIPH_BASE, .irqnum = OTG_HS_IRQn, .ep_count = EP_MAX_HS, .otg_dfifo_depth = DFIFO_DEPTH_HS },
    #endif
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// SystemCoreClock is already included by family header
// extern uint32_t SystemCoreClock;

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

#define dwc2_dcd_int_enable(_rhport)  dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, true)
#define dwc2_dcd_int_disable(_rhport) dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, false)


TU_ATTR_ALWAYS_INLINE static inline void dwc2_remote_wakeup_delay(void) {
  // try to delay for 1 ms
  uint32_t count = SystemCoreClock / 1000;
  while (count--) {
    __NOP();
  }
}

#ifndef APM32_GCCFG_PWEN
  #define APM32_GCCFG_PWEN_Pos           (16U)
  #define APM32_GCCFG_PWEN_Msk           (0x1UL << APM32_GCCFG_PWEN_Pos)
  #define APM32_GCCFG_PWEN               APM32_GCCFG_PWEN_Msk
#endif

// MCU specific PHY init, called BEFORE core reset
static inline void dwc2_phy_init(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  if (hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED) {
    // Enable on-chip FS PHY (PWEN = Power Enable)
    dwc2->stm32_gccfg |= APM32_GCCFG_PWEN;
  } else {
    // Disable FS PHY
    dwc2->stm32_gccfg &= ~APM32_GCCFG_PWEN;
  }
}

// MCU specific PHY deinit, disable PHY power
static inline void dwc2_phy_deinit(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  if (hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED) {
    // Disable on-chip FS PHY
    dwc2->stm32_gccfg &= ~APM32_GCCFG_PWEN;
  }
}

// MCU specific PHY update, it is called AFTER init() and core reset
static inline void dwc2_phy_update(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  // used to set turnaround time for fullspeed, nothing to do in highspeed mode
  if (hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED) {
    // Turnaround timeout depends on the AHB clock dictated by APM32 Reference Manual
    uint32_t turnaround;

    if (SystemCoreClock >= 32000000u) {
      turnaround = 0x6u;
    } else if (SystemCoreClock >= 27500000u) {
      turnaround = 0x7u;
    } else if (SystemCoreClock >= 24000000u) {
      turnaround = 0x8u;
    } else if (SystemCoreClock >= 21800000u) {
      turnaround = 0x9u;
    }
    else if (SystemCoreClock >= 20000000u) {
      turnaround = 0xAu;
    }
    else if (SystemCoreClock >= 18500000u) {
      turnaround = 0xBu;
    }
    else if (SystemCoreClock >= 17200000u) {
      turnaround = 0xCu;
    }
    else if (SystemCoreClock >= 16000000u) {
      turnaround = 0xDu;
    }
    else if (SystemCoreClock >= 15000000u) {
      turnaround = 0xEu;
    }
    else {
      turnaround = 0xFu;
    }

    dwc2->gusbcfg = (dwc2->gusbcfg & ~GUSBCFG_TRDT_Msk) | (turnaround << GUSBCFG_TRDT_Pos);
  }
}

//------------- GCCFG configuration -------------//
static inline void dwc2_stm32_gccfg_cfg(dwc2_regs_t* dwc2, bool vbus_sensing, bool is_host) {
  if (is_host) {
    vbus_sensing = false;
  }

  uint32_t gccfg = dwc2->stm32_gccfg;
  if (dwc2->guid < 0x2000) {
    // PWEN (bit 16) = Power Enable
    // ADVBSEN (bit 18) = Advanced VBUS Sensing
    // BDVBSEN (bit 19) = Basic VBUS Sensing
    // VBSDIS (bit 21) = VBUS Disable
    if (is_host) {
      gccfg &= ~((1UL << 18) | (1UL << 19) | (1UL << 21));
    } else {
      if (vbus_sensing) {
        // Enable VBUS sensing: BDVBSEN = 1, VBSDIS = 0
        gccfg |= (1UL << 19);  // BDVBSEN
        gccfg &= ~(1UL << 21); // VBSDIS
      } else {
        // Disable VBUS sensing: VBSDIS = 1
        gccfg |= (1UL << 21);  // VBSDIS
        gccfg &= ~(1UL << 19); // BDVBSEN
      }
    }
  }

  dwc2->stm32_gccfg = gccfg;
}

#ifdef __cplusplus
}
#endif

#endif
