/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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
