/**************************************************************************/
/*!
    @file     hal_lpc43xx.c
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
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "common/common.h"
#include "hal.h"

#if TUSB_CFG_MCU == MCU_LPC43XX

#include "lpc43xx_cgu.h"
#include "lpc43xx_scu.h"

enum {
  LPC43XX_USBMODE_DEVICE = 2,
  LPC43XX_USBMODE_HOST   = 3
};

enum {
  LPC43XX_USBMODE_VBUS_LOW  = 0,
  LPC43XX_USBMODE_VBUS_HIGH = 1
};

static tusb_error_t hal_controller_reset(uint8_t coreid)
{ // TODO timeout expired to prevent trap
  volatile uint32_t * p_reg_usbcmd;

  p_reg_usbcmd = (coreid ? &LPC_USB1->USBCMD_D : &LPC_USB0->USBCMD_D);
// NXP chip powered with non-host mode --> sts bit is not correctly reflected
  (*p_reg_usbcmd) |= BIT_(1);

//  timeout_timer_t timeout;
//  timeout_set(&timeout, 2); // should not take longer the time to stop controller
  while( ((*p_reg_usbcmd) & BIT_(1)) /*&& !timeout_expired(&timeout)*/) {}
//
//  return timeout_expired(&timeout) ? TUSB_ERROR_OSAL_TIMEOUT : TUSB_ERROR_NONE;
  return TUSB_ERROR_NONE;
}

tusb_error_t hal_init(void)
{
  LPC_CREG->CREG0 &= ~(1<<5); /* Turn on the phy */

  //------------- USB0 -------------//
#if TUSB_CFG_CONTROLLER_0_MODE
  CGU_EnableEntity(CGU_CLKSRC_PLL0, DISABLE); /* Disable PLL first */
  ASSERT_INT( CGU_ERROR_SUCCESS, CGU_SetPLL0(), TUSB_ERROR_FAILED); /* the usb core require output clock = 480MHz */
  CGU_EntityConnect(CGU_CLKSRC_XTAL_OSC, CGU_CLKSRC_PLL0);
  CGU_EnableEntity(CGU_CLKSRC_PLL0, ENABLE);   /* Enable PLL after all setting is done */

  // reset controller & set role
  ASSERT_STATUS( hal_controller_reset(0) );

  #if TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_HOST
    LPC_USB0->USBMODE_H = LPC43XX_USBMODE_HOST | (LPC43XX_USBMODE_VBUS_HIGH << 5);
  #else // TODO OTG
    LPC_USB0->USBMODE_D = LPC43XX_USBMODE_DEVICE;
    LPC_USB0->OTGSC = (1<<3) | (1<<0) /*| (1<<16)| (1<<24)| (1<<25)| (1<<26)| (1<<27)| (1<<28)| (1<<29)| (1<<30)*/;
    #if TUSB_CFG_DEVICE_FULLSPEED // TODO for easy testing
      LPC_USB0->PORTSC1_D |= (1<<24); // force full speed
    #endif
  #endif
#endif

  //------------- USB1 -------------//
#if TUSB_CFG_CONTROLLER_1_MODE
  // Host require to config P2_5, TODO confirm whether device mode require P2_5 or not
  scu_pinmux(0x2, 5, MD_PLN | MD_EZI | MD_ZI, FUNC2);	// USB1_VBUS monitor presence, must be high for bus reset occur

  /* connect CLK_USB1 to 60 MHz clock */
  CGU_EntityConnect(CGU_CLKSRC_PLL1, CGU_BASE_USB1); /* FIXME Run base BASE_USB1_CLK clock from PLL1 (assume PLL1 is 60 MHz, no division required) */
  LPC_SCU->SFSUSB = (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_HOST) ? 0x16 : 0x12; // enable USB1 with on-chip FS PHY

  ASSERT_STATUS( hal_controller_reset(1) );

  #if TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_HOST
    LPC_USB1->USBMODE_H = LPC43XX_USBMODE_HOST | (LPC43XX_USBMODE_VBUS_HIGH << 5);
  #else // TODO OTG
    LPC_USB1->USBMODE_D = LPC43XX_USBMODE_DEVICE;
  #endif

  LPC_USB1->PORTSC1_D |= (1<<24); // TODO abstract, force port to fullspeed
#endif

  return TUSB_ERROR_NONE;
}

#if TUSB_CFG_CONTROLLER_0_MODE
void USB0_IRQHandler(void)
{
  tusb_isr(0);
}
#endif

#if TUSB_CFG_CONTROLLER_1_MODE
void USB1_IRQHandler(void)
{
  tusb_isr(1);
}
#endif

#endif
