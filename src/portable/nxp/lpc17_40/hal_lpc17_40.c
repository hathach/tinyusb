/**************************************************************************/
/*!
    @file     hal_lpc175x_6x.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "common/tusb_common.h"

#if (CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC40XX)

#include "chip.h"

void tusb_hal_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USB_IRQn);
}

void tusb_hal_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USB_IRQn);
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
bool tusb_hal_init(void)
{
  enum {
    USBCLK_DEVCIE = 0x12,     // AHB + Device
    USBCLK_HOST   = 0x19,     // AHB + Host + OTG (!)
  };

  Chip_USB_Init();

#if MODE_HOST_SUPPORTED
  // TODO move pin config to BSP
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) { .Portnum = 1, .Pinnum = 22, .Funcnum = 2} ); // P1.22 as USB_PWRD
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) { .Portnum = 1, .Pinnum = 19, .Funcnum = 2} ); // P1.19 as USB_PPWR

  // Enable host
  LPC_USB->USBClkCtrl = USBCLK_HOST;
  while ((LPC_USB->USBClkSt & USBCLK_HOST) != USBCLK_HOST);
  LPC_USB->OTGClkSt = 0x3;
#endif

#if TUSB_OPT_DEVICE_ENABLED
  // Enable Device
  LPC_USB->USBClkCtrl = USBCLK_DEVCIE;
  while ((LPC_USB->USBClkSt & USBCLK_DEVCIE) != USBCLK_DEVCIE);
#endif

  return true;
}

void USB_IRQHandler(void)
{
  extern void hal_dcd_isr(uint8_t rhport);

  #if MODE_HOST_SUPPORTED
    hal_hcd_isr(0);
  #endif

  #if TUSB_OPT_DEVICE_ENABLED
    hal_dcd_isr(0);
  #endif
}

#endif
