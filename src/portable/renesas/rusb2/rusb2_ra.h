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

#ifdef __cplusplus
}
#endif

#endif /* _RUSB2_RA_H_ */
