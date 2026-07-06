/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */
#ifndef TUSB_OHCI_NXP_H
#define TUSB_OHCI_NXP_H

#if TU_CHECK_MCU(OPT_MCU_LPC175X_6X, OPT_MCU_LPC177X_8X, OPT_MCU_LPC40XX)

#include "chip.h"
#define OHCI_REG   ((ohci_registers_t *) LPC_USB_BASE)

void hcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USB_IRQn);
}

void hcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USB_IRQn);
}

static void ohci_phy_init(uint8_t rhport) {
  (void) rhport;
}

#else

#include "fsl_device_registers.h"

// for LPC55 USB0 controller
#define OHCI_REG  ((ohci_registers_t *) USBFSH_BASE)

static void ohci_phy_init(uint8_t rhport) {
  (void) rhport;
}

void hcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USB0_IRQn);
}

void hcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USB0_IRQn);
}
#endif

#endif
