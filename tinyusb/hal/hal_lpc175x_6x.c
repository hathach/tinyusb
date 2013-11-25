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

#include "tusb_option.h"

#if TUSB_CFG_MCU == MCU_LPC175X_6X

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hal.h"

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
tusb_error_t hal_init(void)
{
  LPC_SC->PCONP |= CLKPWR_PCONP_PCUSB;                	/* USB PCLK -> enable USB Per.*/

  //------------- user manual 11.13 usb device controller initialization -------------//
  LPC_PINCON->PINSEL1 &= ~((3<<26)|(3<<28));  /* P0.29 D+, P0.30 D- */
  LPC_PINCON->PINSEL1 |=  ((1<<26)|(1<<28));  /* PINSEL1 26.27, 28.29  = 01 */

//  LPC_PINCON->PINSEL3 &= ~(3<<6); TODO HOST
//  LPC_PINCON->PINSEL3 |= (2<<6);

  //------------- Device -------------//
  LPC_PINCON->PINSEL4 &= ~(3 << 18);
  LPC_PINCON->PINSEL4 |= (1 << 18); // P2_9 as USB Connect

  // P1_30 as VBUS, ignore if it is already in VBUS mode
  if ( !(!BIT_TEST_(LPC_PINCON->PINSEL3, 28) && BIT_TEST_(LPC_PINCON->PINSEL3, 29)) )
  {
    // some board like lpcxpresso1769 does not connect VBUS signal to pin P1_30, this allow those board to overwrite
    // by always pulling P1_30 to high
    PINSEL_ConfigPin( &(PINSEL_CFG_Type) {
      .Portnum = 1, .Pinnum = 30,
      .Funcnum = 2, .Pinmode = PINSEL_PINMODE_PULLDOWN} );
  }

  LPC_USB->USBClkCtrl = 0x12;                 /* Dev, PortSel, AHB clock enable */
  while ((LPC_USB->USBClkSt & 0x12) != 0x12);

  return TUSB_ERROR_NONE;
}

void USB_IRQHandler(void)
{
  tusb_isr(0);
}

#endif
