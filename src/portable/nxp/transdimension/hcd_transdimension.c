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

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#if CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  #include "fsl_device_registers.h"
#else
  // LPCOpen for 18xx & 43xx
  #include "chip.h"
#endif

#include "common/tusb_common.h"
#include "common_transdimension.h"
#include "portable/ehci/ehci_api.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// TODO can be merged with dcd_controller_t
typedef struct
{
  uint32_t regs_base;     // registers base
  const IRQn_Type irqnum; // IRQ number
}hcd_controller_t;

#if CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  static const hcd_controller_t _hcd_controller[] =
  {
    // RT1010 and RT1020 only has 1 USB controller
    #if FSL_FEATURE_SOC_USBHS_COUNT == 1
      { .regs_base = USB_BASE , .irqnum = USB_OTG1_IRQn }
    #else
      { .regs_base = USB1_BASE, .irqnum = USB_OTG1_IRQn },
      { .regs_base = USB2_BASE, .irqnum = USB_OTG2_IRQn }
    #endif
  };

#else
  static const hcd_controller_t _hcd_controller[] =
  {
    { .regs_base = LPC_USB0_BASE, .irqnum = USB0_IRQn },
    { .regs_base = LPC_USB1_BASE, .irqnum = USB1_IRQn }
  };
#endif

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

bool hcd_init(uint8_t rhport)
{
  hcd_registers_t* hcd_reg = (hcd_registers_t*) _hcd_controller[rhport].regs_base;

  // Reset controller
  hcd_reg->USBCMD |= USBCMD_RESET;
  while( hcd_reg->USBCMD & USBCMD_RESET ) {}

  // Set mode to device, must be set immediately after reset
#if CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_LPC43XX
  // LPC18XX/43XX need to set VBUS Power Select to HIGH
  // RHPORT1 is fullspeed only (need external PHY for Highspeed)
  hcd_reg->USBMODE = USBMODE_CM_HOST | USBMODE_VBUS_POWER_SELECT;
  if (rhport == 1) hcd_reg->PORTSC1 |= PORTSC1_FORCE_FULL_SPEED;
#else
  hcd_reg->USBMODE = USBMODE_CM_HOST;
#endif

  // FIXME force full speed, still have issue with Highspeed enumeration
  hcd_reg->PORTSC1 |= PORTSC1_FORCE_FULL_SPEED;

  return ehci_init(rhport, (uint32_t) &hcd_reg->CAPLENGTH, (uint32_t) &hcd_reg->USBCMD);
}

void hcd_int_enable(uint8_t rhport)
{
  NVIC_EnableIRQ(_hcd_controller[rhport].irqnum);
}

void hcd_int_disable(uint8_t rhport)
{
  NVIC_DisableIRQ(_hcd_controller[rhport].irqnum);
}

#endif
