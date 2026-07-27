/*
 * SPDX-FileCopyrightText: Copyright (c) 2021, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _CI_HS_LPC18_43_H_
#define _CI_HS_LPC18_43_H_

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

// LPCOpen for 18xx & 43xx
#include "chip.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static const ci_hs_controller_t _ci_controller[] =
{
  { .reg_base = LPC_USB0_BASE, .irqnum = USB0_IRQn },
  { .reg_base = LPC_USB1_BASE, .irqnum = USB1_IRQn }
};

#define CI_HS_REG(_port)        ((ci_hs_regs_t*) _ci_controller[_port].reg_base)

#define CI_DCD_INT_ENABLE(_p)   NVIC_EnableIRQ ((IRQn_Type)_ci_controller[_p].irqnum)
#define CI_DCD_INT_DISABLE(_p)  NVIC_DisableIRQ((IRQn_Type)_ci_controller[_p].irqnum)

#define CI_HCD_INT_ENABLE(_p)   NVIC_EnableIRQ ((IRQn_Type)_ci_controller[_p].irqnum)
#define CI_HCD_INT_DISABLE(_p)  NVIC_DisableIRQ((IRQn_Type)_ci_controller[_p].irqnum)

enum {
  CI_HS_LPC18_43_SBUSCFG_OFFSET        = 0x90u,
  CI_HS_LPC18_43_AHBBRST_INCR16_UNSPEC = 0x07u,
};

TU_ATTR_ALWAYS_INLINE static inline void ci_hs_lpc18_43_set_ahb_burst(uint8_t rhport) {
  // USB0 SBUSCFG is at offset 0x90. NXP recommends AHBBRST=0x7:
  // INCR16 with non-multiple transfers decomposed into smaller unspecified bursts.
  if (rhport == 0) {
    volatile uint32_t *sbuscfg = (volatile uint32_t *)(_ci_controller[rhport].reg_base + CI_HS_LPC18_43_SBUSCFG_OFFSET);
    *sbuscfg = CI_HS_LPC18_43_AHBBRST_INCR16_UNSPEC;
  }
}

#endif
