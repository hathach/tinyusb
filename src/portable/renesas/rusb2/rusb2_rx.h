/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 Koji Kitayama
 * SPDX-FileCopyrightText: Portions copyrighted (c) 2021 Roland Winistoerfer
 * SPDX-FileCopyrightText: Copyright (c) 2022 Rafael Silva (@perigoso)
 * SPDX-FileCopyrightText: Copyright (c) 2020 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _RUSB2_RX_H_
#define _RUSB2_RX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "iodefine.h"

#define RUSB2_REG_BASE (0x000A0000)

TU_ATTR_ALWAYS_INLINE static inline rusb2_reg_t* RUSB2_REG(uint8_t rhport) {
  (void) rhport;
  return (rusb2_reg_t *) RUSB2_REG_BASE;
}


#define rusb2_is_highspeed_rhport(_p)  (false)
#define rusb2_is_highspeed_reg(_reg)   (false)

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+


// Start/Stop MSTP TODO implement later
TU_ATTR_ALWAYS_INLINE static inline void rusb2_module_start(uint8_t rhport, bool start) {
  (void) rhport;
  (void) start;
}

TU_ATTR_ALWAYS_INLINE static inline void rusb2_int_enable(uint8_t rhport)
{
  (void) rhport;
#if (CFG_TUSB_MCU == OPT_MCU_RX72N)
  IEN(PERIB, INTB185) = 1;
#else
  IEN(USB0, USBI0) = 1;
#endif
}

TU_ATTR_ALWAYS_INLINE static inline void rusb2_int_disable(uint8_t rhport)
{
  (void) rhport;
#if (CFG_TUSB_MCU == OPT_MCU_RX72N)
  IEN(PERIB, INTB185) = 0;
#else
  IEN(USB0, USBI0) = 0;
#endif
}

// MCU specific PHY init
TU_ATTR_ALWAYS_INLINE static inline void rusb2_phy_init(void)
{
#if (CFG_TUSB_MCU == OPT_MCU_RX72N)
  IR(PERIB, INTB185) = 0;
#else
  IR(USB0, USBI0) = 0;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _RUSB2_RX_H_ */
