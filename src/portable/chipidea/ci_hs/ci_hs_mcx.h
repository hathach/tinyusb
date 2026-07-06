/*
 * SPDX-FileCopyrightText: Copyright (c) 2021, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _CI_HS_MCX_H_
#define _CI_HS_MCX_H_

#include "fsl_device_registers.h"

// NOTE: MCX N9 has 2 different USB Controller
// - USB0 is KHCI FullSpeed
// - USB1 is ChipIdea HighSpeed, therefore rhport = 1 is actually index 0

static const ci_hs_controller_t _ci_controller[] = {
    {.reg_base = USBHS1__USBC_BASE, .irqnum = USB1_HS_IRQn}
};

TU_ATTR_ALWAYS_INLINE static inline ci_hs_regs_t* CI_HS_REG(uint8_t port) {
  (void) port;
  return ((ci_hs_regs_t*) _ci_controller[0].reg_base);
}

#define CI_DCD_INT_ENABLE(_p)   do { (void) _p; NVIC_EnableIRQ (_ci_controller[0].irqnum); } while (0)
#define CI_DCD_INT_DISABLE(_p)  do { (void) _p; NVIC_DisableIRQ(_ci_controller[0].irqnum); } while (0)

#define CI_HCD_INT_ENABLE(_p)   NVIC_EnableIRQ (_ci_controller[_p].irqnum)
#define CI_HCD_INT_DISABLE(_p)  NVIC_DisableIRQ(_ci_controller[_p].irqnum)


#endif
