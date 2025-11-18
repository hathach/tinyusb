/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ha Thach (tinyusb.org)
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
