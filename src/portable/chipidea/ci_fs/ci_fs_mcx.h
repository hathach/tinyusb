/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _CI_FS_MCX_H
#define _CI_FS_MCX_H

#include "fsl_device_registers.h"

#if CFG_TUSB_MCU == OPT_MCU_MCXN9
  #define CI_FS_REG(_port)  ((ci_fs_regs_t*) USBFS0_BASE)
  #define CIFS_IRQN 				USB0_FS_IRQn

#elif CFG_TUSB_MCU == OPT_MCU_MCXA15
  #define CI_FS_REG(_port)  ((ci_fs_regs_t*) USB0_BASE)
  #define CIFS_IRQN         USB0_IRQn

#else
  #error "MCU is not supported"
#endif

#define CI_REG              CI_FS_REG(0)

void dcd_int_enable(uint8_t rhport) {
  (void) rhport;
  NVIC_EnableIRQ(CIFS_IRQN);
}

void dcd_int_disable(uint8_t rhport) {
  (void) rhport;
  NVIC_DisableIRQ(CIFS_IRQN);
}

#endif
