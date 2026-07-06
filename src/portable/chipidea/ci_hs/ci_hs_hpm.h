/*
 * SPDX-FileCopyrightText: Copyright (c) 2021, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _CI_HS_HPM_H_
#define _CI_HS_HPM_H_

#include "ci_hs_type.h"
#include "hpm_soc.h"
#include "hpm_interrupt.h"
#include "hpm_usb_drv.h"

static const ci_hs_controller_t _ci_controller[] =
{
    { .reg_base = HPM_USB0_BASE, .irqnum = IRQn_USB0},
    #ifdef HPM_USB1_BASE
    { .reg_base = HPM_USB1_BASE, .irqnum = IRQn_USB1},
    #endif
};

#define CI_HS_REG(_port)        ((ci_hs_regs_t*) _ci_controller[_port].reg_base)

//------------- DCD -------------//
#define CI_DCD_INT_ENABLE(_p)   intc_m_enable_irq (_ci_controller[_p].irqnum)
#define CI_DCD_INT_DISABLE(_p)  intc_m_disable_irq(_ci_controller[_p].irqnum)

//------------- HCD -------------//
#define CI_HCD_INT_ENABLE(_p)   intc_m_enable_irq (_ci_controller[_p].irqnum)
#define CI_HCD_INT_DISABLE(_p)  intc_m_disable_irq(_ci_controller[_p].irqnum)


#endif
