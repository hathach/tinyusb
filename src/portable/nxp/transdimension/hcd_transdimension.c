/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include "tusb_option.h"

// NXP Trans-Dimension USB IP implement EHCI for host functionality

#if TUSB_OPT_HOST_ENABLED && \
    (CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX)

#if CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  #include "fsl_device_registers.h"
#else
  // LPCOpen for 18xx & 43xx
  #include "chip.h"
#endif

typedef struct
{
  uint32_t regs_addr;     // registers base
  const IRQn_Type irqnum; // IRQ number
}hcd_controller_t;

#if CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  static const hcd_controller_t _hcd_controller[] =
  {
    // RT1010 and RT1020 only has 1 USB controller
    #if FSL_FEATURE_SOC_USBHS_COUNT == 1
      { .regs_addr = (uint32_t) &USB->USBCMD , .irqnum = USB_OTG1_IRQn }
    #else
      { .regs_addr = (uint32_t) &USB1->USBCMD, .irqnum = USB_OTG1_IRQn },
      { .regs_addr = (uint32_t) &USB2->USBCMD, .irqnum = USB_OTG2_IRQn }
    #endif
  };

#else
  static const hcd_controller_t _hcd_controller[] =
  {
    { .regs_addr = (uint32_t) &LPC_USB0->USBCMD_H, .irqnum = USB0_IRQn },
    { .regs_addr = (uint32_t) &LPC_USB1->USBCMD_H, .irqnum = USB1_IRQn }
  };
#endif


void hcd_int_enable(uint8_t rhport)
{
  NVIC_EnableIRQ(_hcd_controller[rhport].irqnum);
}

void hcd_int_disable(uint8_t rhport)
{
  NVIC_DisableIRQ(_hcd_controller[rhport].irqnum);
}

uint32_t hcd_ehci_register_addr(uint8_t rhport)
{
  return _hcd_controller[rhport].regs_addr;
}

#endif
