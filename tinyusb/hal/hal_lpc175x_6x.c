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

#include "common/common.h"
#include "hal.h"

#if TUSB_CFG_MCU == MCU_LPC175X_6X

#ifdef __CC_ARM
#pragma diag_suppress 66 // Suppress Keil warnings #66-D: enumeration value is out of "int" range
#endif

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
tusb_error_t hal_init(void)
{
  enum {
    USBCLK_DEVCIE = 0x12,     // AHB + Device
    USBCLK_HOST   = 0x19,     // AHB + Host + OTG (!)

    PCONP_PCUSB   = BIT_(31)
  };

  LPC_SC->PCONP |= PCONP_PCUSB; // enable USB Peripherals

  //------------- user manual 11.13 usb device controller initialization -------------//
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) { .Portnum = 0, .Pinnum = 29, .Funcnum = 1} ); // P0.29 as D+
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) { .Portnum = 0, .Pinnum = 30, .Funcnum = 1} ); // P0.30 as D-

#if MODE_HOST_SUPPORTED
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) { .Portnum = 1, .Pinnum = 22, .Funcnum = 2} ); // P1.22 as USB_PWRD
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) { .Portnum = 1, .Pinnum = 19, .Funcnum = 2} ); // P1.19 as USB_PPWR

  LPC_USB->USBClkCtrl = USBCLK_HOST;
  while ((LPC_USB->USBClkSt & USBCLK_HOST) != USBCLK_HOST);
  LPC_USB->OTGStCtrl = 0x3;
#endif

#if MODE_DEVICE_SUPPORTED
  LPC_PINCON->PINSEL4 = bit_set_range(LPC_PINCON->PINSEL4, 18, 19, BIN8(01)); // P2_9 as USB Connect

  // P1_30 as VBUS, ignore if it is already in VBUS mode
  if ( !(!BIT_TEST_(LPC_PINCON->PINSEL3, 28) && BIT_TEST_(LPC_PINCON->PINSEL3, 29)) )
  {
    // some board like lpcxpresso1769 does not connect VBUS signal to pin P1_30, this allow those board to overwrite
    // by always pulling P1_30 to high
    PINSEL_ConfigPin( &(PINSEL_CFG_Type) {
      .Portnum = 1, .Pinnum = 30,
      .Funcnum = 2, .Pinmode = PINSEL_PINMODE_PULLDOWN} );
  }

  LPC_USB->USBClkCtrl = USBCLK_DEVCIE;
  while ((LPC_USB->USBClkSt & USBCLK_DEVCIE) != USBCLK_DEVCIE);
#endif

  return TUSB_ERROR_NONE;
}

void USB_IRQHandler(void)
{
  tusb_isr(0);
}

#endif
