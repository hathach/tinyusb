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

#endif
