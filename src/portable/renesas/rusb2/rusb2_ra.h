/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 Rafael Silva (@perigoso)
 * SPDX-FileCopyrightText: Copyright (c) 2022 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _RUSB2_RA_H_
#define _RUSB2_RA_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic ignored "-Wundef"

// extra push due to https://github.com/renesas/fsp/pull/278
#pragma GCC diagnostic push
#endif

/* renesas fsp api */
#include "bsp_api.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// IAR does not have __builtin_ctz
#if defined(__ICCARM__)
  #define __builtin_ctz(x)   __iar_builtin_CLZ(__iar_builtin_RBIT(x))
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

typedef struct {
  uint32_t reg_base;
  int32_t irqnum;
}rusb2_controller_t;

#if defined(BSP_MCU_GROUP_RA6M5) || defined(BSP_MCU_GROUP_RA6M3) || (BSP_CFG_MCU_PART_SERIES == 8)
  #define RUSB2_SUPPORT_HIGHSPEED
  #define RUSB2_CONTROLLER_COUNT 2

  #define rusb2_is_highspeed_rhport(_p)  (_p == 1)
  #define rusb2_is_highspeed_reg(_reg)   (_reg == RUSB2_REG(1))

  // UTMI PHY reference clock is the main oscillator: PHYSET.CLKSEL must match the board XTAL
  // before the PHY PLL is released (RA6M5 UM R01UH0891 29.2.17: 00=12 MHz, 10=20 MHz,
  // 11=24 MHz reset default). EK-RA6M5 runs 24 MHz (default works); EK-RA8M1 runs 20 MHz and
  // never locks/chirps on the default. A board with a non-standard USB clocking scheme can
  // pre-define RUSB2_PHYSET_CLKSEL_VALUE to override this selection.
  #ifndef RUSB2_PHYSET_CLKSEL_VALUE
    #if   BSP_CFG_XTAL_HZ == 12000000
      #define RUSB2_PHYSET_CLKSEL_VALUE 0u
    #elif BSP_CFG_XTAL_HZ == 20000000
      #define RUSB2_PHYSET_CLKSEL_VALUE 2u
    #elif BSP_CFG_XTAL_HZ == 24000000
      #define RUSB2_PHYSET_CLKSEL_VALUE 3u
    #else
      #error "USBHS UTMI PHY: no PHYSET.CLKSEL encoding for this BSP_CFG_XTAL_HZ; define RUSB2_PHYSET_CLKSEL_VALUE"
    #endif
  #endif
#else
  #define RUSB2_CONTROLLER_COUNT 1

  #define rusb2_is_highspeed_rhport(_p)  (false)
  #define rusb2_is_highspeed_reg(_reg)   (false)
#endif

extern rusb2_controller_t rusb2_controller[];
#define RUSB2_REG(_p)      ((rusb2_reg_t*) rusb2_controller[_p].reg_base)

//--------------------------------------------------------------------+
// RUSB2 API
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline void rusb2_module_start(uint8_t rhport, bool start) {
  uint32_t const mask = 1U << (11+rhport);
  if (start) {
    R_MSTP->MSTPCRB &= ~mask;
  }else {
    R_MSTP->MSTPCRB |= mask;
  }
}

TU_ATTR_ALWAYS_INLINE static inline void rusb2_int_enable(uint8_t rhport) {
  NVIC_EnableIRQ(rusb2_controller[rhport].irqnum);
}

TU_ATTR_ALWAYS_INLINE static inline void rusb2_int_disable(uint8_t rhport) {
  NVIC_DisableIRQ(rusb2_controller[rhport].irqnum);
}

// MCU specific PHY init
TU_ATTR_ALWAYS_INLINE static inline void rusb2_phy_init(void) {
}

#ifdef RUSB2_SUPPORT_HIGHSPEED
// UTMI PHY power-up per the FSP reference sequence (r_usb_preg_access.c), shared by dcd_init and
// hcd_init: program CLKSEL to the board XTAL while the PHY is powered down (DIRPD=1), 1 us,
// release DIRPD, 1 ms, release PLLRESET, then wait for PLL lock. Changing CLKSEL as the PHY
// powers up gets mis-sampled (EK-RA8M1, 20 MHz).
static inline void rusb2_utmi_phy_powerup(rusb2_reg_t* rusb) {
  uint16_t physet = rusb->PHYSET | RUSB2_PHYSET_DIRPD_Msk;
  rusb->PHYSET = physet;
  #ifdef RUSB2_PHYSET_CLKSEL_VALUE
  physet = (uint16_t) ((physet & ~RUSB2_PHYSET_CLKSEL_Msk) |
                       (RUSB2_PHYSET_CLKSEL_VALUE << RUSB2_PHYSET_CLKSEL_Pos));
  rusb->PHYSET = physet;
  #endif
  R_BSP_SoftwareDelay((uint32_t) 1, BSP_DELAY_UNITS_MICROSECONDS);
  physet &= (uint16_t) ~RUSB2_PHYSET_DIRPD_Msk;
  rusb->PHYSET = physet;
  R_BSP_SoftwareDelay((uint32_t) 1, BSP_DELAY_UNITS_MILLISECONDS);
  rusb->PHYSET_b.PLLRESET = 0;

  // set UTMI to operating mode and wait for PLL lock confirmation
  rusb->LPSTS_b.SUSPENDM = 1;
  while (!rusb->PLLSTA_b.PLLLOCK) {}
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RUSB2_RA_H_ */
