/****************************************************************************
 *   $Id:: nmi.c 7227 2011-04-27 20:20:38Z usb01267                         $
 *   Project: NXP LPC13Uxx NMI interrupt example
 *
 *   Description:
 *     This file contains NMI interrupt handler code example.
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
#include "nmi.h"

#if NMI_ENABLED
volatile uint32_t NMI_Counter[MAX_NMI_NUM];

/*****************************************************************************
** Function name:		NMI_Handler
**
** Descriptions:		NMI interrupt handler
** parameters:		None			 
** 						
** Returned value:	None
** 
*****************************************************************************/
void NMI_Handler( void )
{
  uint32_t regVal;

  regVal = LPC_SYSCON->NMISRC;
  regVal &=	~0x80000000;
  if ( regVal < MAX_NMI_NUM )
  {
    if ( regVal == CT16B0_IRQn )
	{
	  /* Use TIMER16_0_IRQHandler as example for real application. */ 	
	  LPC_CT16B0->IR = 0xFF;	/* Clear timer16_0 interrupt */
	}
	else if ( regVal == CT16B1_IRQn )
	{
	  /* Use TIMER16_1_IRQHandler as example for real application. */	
	  LPC_CT16B1->IR = 0xFF;	/* Clear timer16_1 interrupt */
	}
    else if ( regVal == CT32B0_IRQn )
	{
	  /* Use TIMER32_0_IRQHandler as example for real application. */ 	
	  LPC_CT32B0->IR = 0xFF;	/* Clear timer32_0 interrupt */
	}
	else if ( regVal == CT32B1_IRQn )
	{
	  /* Use TIMER32_0_IRQHandler as example for real application. */ 	
	  LPC_CT32B1->IR = 0xFF;	/* Clear timer32_1 interrupt */
	}
	NMI_Counter[regVal]++; 
  }
  return;
}

/*****************************************************************************
** Function name:		NMI_Init
**
** Descriptions:		NMI initialization
** parameters:			NMI number			 
** 						
** Returned value:		None
** 
*****************************************************************************/
void NMI_Init( uint32_t NMI_num )
{
  uint32_t i;

  for ( i = 0; i < MAX_NMI_NUM; i++ )
  {
    NMI_Counter[i] = 0x0;
  }
  LPC_SYSCON->NMISRC = 0x80000000|NMI_num;
  return;
}

#endif
