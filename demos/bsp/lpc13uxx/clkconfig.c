/****************************************************************************
 *   $Id:: clkconfig.c 6874 2011-03-22 01:58:31Z usb00423                   $
 *   Project: NXP LPC13Uxx Clock Configuration example
 *
 *   Description:
 *     This file contains clock configuration code example which include 
 *     watchdog setup and debug clock out setup.
 *
 ****************************************************************************
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
* Permission to use, copy, modify, and distribute this software and its
* documentation is hereby granted, under NXP Semiconductors'
* relevant copyright in the software, without fee, provided that it
* is used in conjunction with NXP Semiconductors microcontrollers.  This
* copyright, permission, and disclaimer notice must appear in all copies of
* this code.
****************************************************************************/
#include "LPC13Uxx.h"
#include "clkconfig.h"


/*****************************************************************************
** Function name:		WDT_CLK_Setup
**
** Descriptions:		Configure WDT clock.
** parameters:			clock source: irc_osc(0), main_clk(1), wdt_osc(2).			 
** 						
** Returned value:		None
** 
*****************************************************************************/
void WDT_CLK_Setup ( uint32_t clksrc )
{
  /* Freq = 0.5Mhz, div_sel is 0x1F, divided by 64. WDT_OSC should be 7.8125khz */
  LPC_SYSCON->WDTOSCCTRL = (0x1<<5)|0x1F;
  LPC_SYSCON->PDRUNCFG &= ~(0x1<<6);    /* Let WDT clock run */

  /* Enables clock for WDT */
  LPC_SYSCON->SYSAHBCLKCTRL |= (1<<15);
  LPC_WWDT->CLKSEL = clksrc;        /* Select clock source */
  return;
}

/*****************************************************************************
** Function name:		CLKOUT_Setup
**
** Descriptions:		Configure CLKOUT for reference clock check.
** parameters:			clock source: irc_osc(0), sys_osc(1), wdt_osc(2),
**						main_clk(3).			 
** 						
** Returned value:		None
** 
*****************************************************************************/
void CLKOUT_Setup ( uint32_t clksrc )
{
  /* debug PLL after configuration. */
  LPC_SYSCON->CLKOUTSEL = clksrc;	/* Select Main clock */
  LPC_SYSCON->CLKOUTDIV = 1;			/* Divided by 1 */
  return;
}

