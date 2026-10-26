/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _CI_FS_KINETIS_H
#define _CI_FS_KINETIS_H

#include "fsl_device_registers.h"

//static const ci_fs_controller_t _ci_controller[] = {
//    {.reg_base = USB0_BASE, .irqnum = USB0_IRQn}
//};

#define CI_FS_REG(_port)        ((ci_fs_regs_t*) USB0_BASE)
#define CI_REG                  CI_FS_REG(0)

void dcd_int_enable(uint8_t rhport) {
  (void) rhport;
  NVIC_EnableIRQ(USB0_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
  (void) rhport;
  NVIC_DisableIRQ(USB0_IRQn);
}

#endif
