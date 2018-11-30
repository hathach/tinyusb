/****************************************************************************
 *   $Id:: timer32.c 6951 2011-03-23 22:11:07Z usb00423                     $
 *   Project: NXP LPC13Uxx 32-bit timer example
 *
 *   Description:
 *     This file contains 32-bit timer code example which include timer 
 *     initialization, timer interrupt handler, and related APIs for 
 *     timer setup.
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
#include "timer32.h"
#include "nmi.h"

volatile uint32_t timer32_0_counter[4] = {0,0,0,0};
volatile uint32_t timer32_1_counter[4] = {0,0,0,0};
volatile uint32_t timer32_0_capture[4] = {0,0,0,0};
volatile uint32_t timer32_1_capture[4] = {0,0,0,0};
volatile uint32_t timer32_0_period = 0;
volatile uint32_t timer32_1_period = 0;

/*****************************************************************************
** Function name:		delay32Ms
**
** Descriptions:		Start the timer delay in milo seconds
**						until elapsed
**
** parameters:			timer number, Delay value in milo second			 
** 						
** Returned value:		None
** 
*****************************************************************************/
void delay32Ms(uint8_t timer_num, uint32_t delayInMs)
{
  if (timer_num == 0)
  {
    /* setup timer #0 for delay */
    LPC_CT32B0->TCR = 0x02;		/* reset timer */
    LPC_CT32B0->PR  = 0x00;		/* set prescaler to zero */
    LPC_CT32B0->MR0 = delayInMs * (SystemCoreClock / 1000);
    LPC_CT32B0->IR  = 0xff;		/* reset all interrrupts */
    LPC_CT32B0->MCR = 0x04;		/* stop timer on match */
    LPC_CT32B0->TCR = 0x01;		/* start timer */
  
    /* wait until delay time has elapsed */
    while (LPC_CT32B0->TCR & 0x01);
  }
  else if (timer_num == 1)
  {
    /* setup timer #1 for delay */
    LPC_CT32B1->TCR = 0x02;		/* reset timer */
    LPC_CT32B1->PR  = 0x00;		/* set prescaler to zero */
    LPC_CT32B1->MR0 = delayInMs * (SystemCoreClock / 1000);
    LPC_CT32B1->IR  = 0xff;		/* reset all interrrupts */
    LPC_CT32B1->MCR = 0x04;		/* stop timer on match */
    LPC_CT32B1->TCR = 0x01;		/* start timer */
  
    /* wait until delay time has elapsed */
    while (LPC_CT32B1->TCR & 0x01);
  }
  return;
}

/******************************************************************************
** Function name:		CT32B0_IRQHandler
**
** Descriptions:		Timer/CounterX and captureX interrupt handler
**
** parameters:			None
** Returned value:		None
** 
******************************************************************************/
void CT32B0_IRQHandler(void)
{
  if ( LPC_CT32B0->IR & (0x01<<0) )
  {  
	LPC_CT32B0->IR = 0x1<<0;			/* clear interrupt flag */
	timer32_0_counter[0]++;
  }
  if ( LPC_CT32B0->IR & (0x01<<1) )
  {  
	LPC_CT32B0->IR = 0x1<<1;			/* clear interrupt flag */
	timer32_0_counter[1]++;
  }
  if ( LPC_CT32B0->IR & (0x01<<2) )
  {  
	LPC_CT32B0->IR = 0x1<<2;			/* clear interrupt flag */
	timer32_0_counter[2]++;
  }
  if ( LPC_CT32B0->IR & (0x01<<3) )
  {  
	LPC_CT32B0->IR = 0x1<<3;			/* clear interrupt flag */
	timer32_0_counter[3]++;
  }
  if ( LPC_CT32B0->IR & (0x1<<4) )
  {  
	LPC_CT32B0->IR = 0x1<<4;			/* clear interrupt flag */
	timer32_0_capture[0]++;
  }
  if ( LPC_CT32B0->IR & (0x1<<5) )
  {  
	LPC_CT32B0->IR = 0x1<<5;			/* clear interrupt flag */
	timer32_0_capture[1]++;
  }
  if ( LPC_CT32B0->IR & (0x1<<6) )
  {  
	LPC_CT32B0->IR = 0x1<<6;			/* clear interrupt flag */
	timer32_0_capture[2]++;
  }
  if ( LPC_CT32B0->IR & (0x1<<7) )
  {  
	LPC_CT32B0->IR = 0x1<<7;			/* clear interrupt flag */
	timer32_0_capture[3]++;
  }
  return;
}

/******************************************************************************
** Function name:		CT32B1_IRQHandler
**
** Descriptions:		Timer/CounterX and captureX interrupt handler
**
** parameters:			None
** Returned value:		None
** 
******************************************************************************/
void CT32B1_IRQHandler(void)
{
  if ( LPC_CT32B1->IR & (0x01<<0) )
  {  
	LPC_CT32B1->IR = 0x1<<0;			/* clear interrupt flag */
	timer32_1_counter[0]++;
  }
  if ( LPC_CT32B1->IR & (0x01<<1) )
  {  
	LPC_CT32B1->IR = 0x1<<1;			/* clear interrupt flag */
	timer32_1_counter[1]++;
  }
  if ( LPC_CT32B1->IR & (0x01<<2) )
  {  
	LPC_CT32B1->IR = 0x1<<2;			/* clear interrupt flag */
	timer32_1_counter[2]++;
  }
  if ( LPC_CT32B1->IR & (0x01<<3) )
  {  
	LPC_CT32B1->IR = 0x1<<3;			/* clear interrupt flag */
	timer32_1_counter[3]++;
  }
  if ( LPC_CT32B1->IR & (0x1<<4) )
  {  
	LPC_CT32B1->IR = 0x1<<4;			/* clear interrupt flag */
	timer32_1_capture[0]++;
  }
  if ( LPC_CT32B1->IR & (0x1<<5) )
  {  
	LPC_CT32B1->IR = 0x1<<5;			/* clear interrupt flag */
	timer32_1_capture[1]++;
  }
  if ( LPC_CT32B1->IR & (0x1<<6) )
  {  
	LPC_CT32B1->IR = 0x1<<6;			/* clear interrupt flag */
	timer32_1_capture[2]++;
  }
  if ( LPC_CT32B1->IR & (0x1<<7) )
  {  
	LPC_CT32B1->IR = 0x1<<7;			/* clear interrupt flag */
	timer32_1_capture[3]++;
  }
  return;
}

/******************************************************************************
** Function name:		enable_timer
**
** Descriptions:		Enable timer
**
** parameters:			timer number: 0 or 1
** Returned value:		None
** 
******************************************************************************/
void enable_timer32(uint8_t timer_num)
{
  if ( timer_num == 0 )
  {
    LPC_CT32B0->TCR = 1;
  }
  else
  {
    LPC_CT32B1->TCR = 1;
  }
  return;
}

/******************************************************************************
** Function name:		disable_timer
**
** Descriptions:		Disable timer
**
** parameters:			timer number: 0 or 1
** Returned value:		None
** 
******************************************************************************/
void disable_timer32(uint8_t timer_num)
{
  if ( timer_num == 0 )
  {
    LPC_CT32B0->TCR = 0;
  }
  else
  {
    LPC_CT32B1->TCR = 0;
  }
  return;
}

/******************************************************************************
** Function name:		reset_timer
**
** Descriptions:		Reset timer
**
** parameters:			timer number: 0 or 1
** Returned value:		None
** 
******************************************************************************/
void reset_timer32(uint8_t timer_num)
{
  uint32_t regVal;

  if ( timer_num == 0 )
  {
    regVal = LPC_CT32B0->TCR;
    regVal |= 0x02;
    LPC_CT32B0->TCR = regVal;
  }
  else
  {
    regVal = LPC_CT32B1->TCR;
    regVal |= 0x02;
    LPC_CT32B1->TCR = regVal;
  }
  return;
}

/******************************************************************************
** Function name:		set_timer_capture
**
** Descriptions:		Set timer capture based on location
**
** parameters:			timer number: 0~1, location 0~2
** Returned value:		None
** 
******************************************************************************/
void set_timer32_capture(uint8_t timer_num, uint8_t location )
{
  if ( timer_num == 0 )
  {
	/*  Timer0_32 I/O config */
	if ( location == 0 )
	{
	  LPC_IOCON->PIO1_28          &= ~0x07;
	  LPC_IOCON->PIO1_28          |= 0x01;		/* Timer0_32 CAP0 */
	  LPC_IOCON->PIO1_29          &= ~0x07;
	  LPC_IOCON->PIO1_29          |= 0x02;		/* Timer0_32 CAP2 */
	}
	else if ( location == 1 )
	{
	  LPC_IOCON->PIO0_17          &= ~0x07;
	  LPC_IOCON->PIO0_17          |= 0x02;		/* Timer0_32 CAP0 */
	}
	else
	{
	  while ( 1 );				/* Fatal location number error */
	}
  }
  else
  {
	/*  Timer1_32 I/O config */
	if ( location == 0 )
	{
	  LPC_IOCON->PIO1_4           &= ~0x07;		/*  Timer1_32 I/O config */
	  LPC_IOCON->PIO1_4           |= 0x01;		/* Timer1_32 CAP0 */
	  LPC_IOCON->PIO1_5           &= ~0x07;
	  LPC_IOCON->PIO1_5           |= 0x01;		/* Timer1_32 CAP1 */
	}
	else if ( location == 1 )
	{
	  LPC_IOCON->TMS_PIO0_12        &= ~0x07;
	  LPC_IOCON->TMS_PIO0_12        |= 0x03;		/* Timer1_32 CAP0 */
	}
	else
	{
	  while ( 1 );				/* Fatal location number error */
	}
  }	
  return;
}

/******************************************************************************
** Function name:		set_timer_match
**
** Descriptions:		Set timer match based on location
**
** parameters:			timer number: 0~1, location 0~2
** Returned value:		None
** 
******************************************************************************/
void set_timer32_match(uint8_t timer_num, uint8_t match_enable, uint8_t location)
{
  if ( timer_num == 0 )
  {
	if ( match_enable & 0x01 )
	{
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO0_18           &= ~0x07;	
		LPC_IOCON->PIO0_18           |= 0x02;		/* Timer0_32 MAT0 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO1_24           &= ~0x07;
		LPC_IOCON->PIO1_24           |= 0x01;		/* Timer0_32 MAT0 */
	  }
	}
	if ( match_enable & 0x02 )
	{
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO0_19           &= ~0x07;
		LPC_IOCON->PIO0_19           |= 0x02;		/* Timer0_32 MAT1 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO1_25           &= ~0x07;
		LPC_IOCON->PIO1_25           |= 0x01;		/* Timer0_32 MAT1 */
	  }
	}
	if ( match_enable & 0x04 )
	{
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO0_1            &= ~0x07;
		LPC_IOCON->PIO0_1            |= 0x02;		/* Timer0_32 MAT2 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO1_26           &= ~0x07;
		LPC_IOCON->PIO1_26           |= 0x01;		/* Timer0_32 MAT2 */
	  }
	}
	if ( match_enable & 0x08 )
	{
	  if ( location == 0 )
	  {
		LPC_IOCON->TDI_PIO0_11       &= ~0x07;
		LPC_IOCON->TDI_PIO0_11       |= 0x03;		/* Timer0_32 MAT3 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO1_27           &= ~0x07;
		LPC_IOCON->PIO1_27           |= 0x01;		/* Timer0_32 MAT3 */
	  }
	}
  }
  else if ( timer_num == 1 )
  {
	if ( match_enable & 0x01 )
	{
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO1_0            &= ~0x07;
		LPC_IOCON->PIO1_0            |= 0x01;		/* Timer1_32 MAT0 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->TDO_PIO0_13       &= ~0x07;
		LPC_IOCON->TDO_PIO0_13       |= 0x03;		/* Timer1_32 MAT0 */
	  }
	}
	if ( match_enable & 0x02 )
	{
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO1_1            &= ~0x07;
		LPC_IOCON->PIO1_1            |= 0x01;		/* Timer1_32 MAT1 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->TRST_PIO0_14      &= ~0x07;
		LPC_IOCON->TRST_PIO0_14      |= 0x03;		/* Timer1_32 MAT1 */
	  }
	}
	if ( match_enable & 0x04 )
	{
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO1_2            &= ~0x07;
		LPC_IOCON->PIO1_2            |= 0x01;		/* Timer1_32 MAT2 */
	  }
	  else if ( location == 1 )
	  {
#if __SWD_DISABLED
		LPC_IOCON->SWDIO_PIO0_15     &= ~0x07;
		LPC_IOCON->SWDIO_PIO0_15     |= 0x03;		/* Timer1_32 MAT2 */
#endif
	  }
	}
	if ( match_enable & 0x08 )
	{
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO1_3            &= ~0x07;
		LPC_IOCON->PIO1_3            |= 0x01;		/* Timer1_32 MAT3 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO0_16           &= ~0x07;
		LPC_IOCON->PIO0_16           |= 0x02;		/* Timer1_32 MAT3 */
	  }
	}
  }
  return;		
}

/******************************************************************************
** Function name:		init_timer
**
** Descriptions:		Initialize timer, set timer interval, reset timer,
**				install timer interrupt handler
**
** parameters:		timer number and timer interval
** Returned value:	None
** 
******************************************************************************/
void init_timer32(uint8_t timer_num, uint32_t TimerInterval) 
{
  uint32_t i;

  if ( timer_num == 0 )
  {
    /* Some of the I/O pins need to be clearfully planned if
    you use below module because JTAG and TIMER CAP/MAT pins are muxed. */
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<9);

    LPC_CT32B0->MR0 = TimerInterval;
#if TIMER_MATCH
  	for ( i = 0; i < 4; i++ )
	{
      timer32_0_counter[i] = 0;
	}
    set_timer32_match(timer_num, 0x0F, 0);
	LPC_CT32B0->EMR &= ~(0xFF<<4);
	LPC_CT32B0->EMR |= ((0x3<<4)|(0x3<<6)|(0x3<<8)|(0x3<<10));	/* MR0/1/2/3 Toggle */
#else
  	for ( i = 0; i < 4; i++ )
	{
      timer32_0_capture[i] = 0;
	}
	set_timer32_capture(timer_num, 0 );
	/* Capture 0 on rising edge, interrupt enable. */
	LPC_CT32B0->CCR = (0x5<<0)|(0x5<<6);
#endif
 	LPC_CT32B0->MCR = 3;			/* Interrupt and Reset on MR0 */

    /* Enable the TIMER0 Interrupt */
#if NMI_ENABLED
    NVIC_DisableIRQ( CT32B0_IRQn );
	NMI_Init( CT32B0_IRQn );
#else
    NVIC_EnableIRQ(CT32B0_IRQn);
#endif
  }
  else if ( timer_num == 1 )
  {
    /* Some of the I/O pins need to be clearfully planned if
    you use below module because JTAG and TIMER CAP/MAT pins are muxed. */
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<10);

    LPC_CT32B1->MR0 = TimerInterval;
#if TIMER_MATCH
    for ( i = 0; i < 4; i++ )
	{
      timer32_1_counter[i] = 0;
	}
	set_timer32_match(timer_num, 0x0F, 0);
	LPC_CT32B1->EMR &= ~(0xFF<<4);
	LPC_CT32B1->EMR |= ((0x3<<4)|(0x3<<6)|(0x3<<8)|(0x3<<10));	/* MR0/1/2 Toggle */
#else
  	for ( i = 0; i < 4; i++ )
	{
      timer32_1_capture[i] = 0;
	}
	set_timer32_capture(timer_num, 0 );
	/* Capture 0 on rising edge, interrupt enable. */
	LPC_CT32B1->CCR = (0x5<<0)|(0x5<<3);
#endif
    LPC_CT32B1->MCR = 3;			/* Interrupt and Reset on MR0 */

    /* Enable the TIMER1 Interrupt */
#if NMI_ENABLED
    NVIC_DisableIRQ( CT32B1_IRQn );
	NMI_Init( CT32B1_IRQn );
#else
    NVIC_EnableIRQ(CT32B1_IRQn);
#endif
  }
  return;
}
/******************************************************************************
** Function name:		init_timer32PWM
**
** Descriptions:		Initialize timer as PWM
**
** parameters:			timer number, period and match enable:
**						match_enable[0] = PWM for MAT0 
**						match_enable[1] = PWM for MAT1
**						match_enable[2] = PWM for MAT2
** Returned value:		None
** 
******************************************************************************/
void init_timer32PWM(uint8_t timer_num, uint32_t period, uint8_t match_enable)
{
  disable_timer32(timer_num);
  if (timer_num == 1)
  {
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<10);

    /* Setup the external match register */
    LPC_CT32B1->EMR = (1<<EMC3)|(1<<EMC2)|(2<<EMC1)|(1<<EMC0)|MATCH3|(match_enable);

    /* Setup the outputs */
    /* If match0 is enabled, set the output, use location 0 for test. */
	set_timer32_match( timer_num, match_enable, 0 );

    /* Enable the selected PWMs and enable Match3 */
    LPC_CT32B1->PWMC = MATCH3|(match_enable);
 
    /* Setup the match registers */
    /* set the period value to a global variable */
    timer32_1_period = period;
    LPC_CT32B1->MR3 = timer32_1_period;
    LPC_CT32B1->MR0 = timer32_1_period/2;
    LPC_CT32B1->MR1 = timer32_1_period/2;
    LPC_CT32B1->MR2 = timer32_1_period/2;
    LPC_CT32B1->MCR = 1<<10;				/* Reset on MR3 */
  }
  else
  {
    /* Some of the I/O pins need to be clearfully planned if
    you use below module because JTAG and TIMER CAP/MAT pins are muxed. */
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<9);

    /* Setup the external match register */
    LPC_CT32B0->EMR = (1<<EMC3)|(2<<EMC2)|(1<<EMC1)|(1<<EMC0)|MATCH3|(match_enable);
 
    /* Setup the outputs */
    /* If match0 is enabled, set the output, use location 0 for test. */
	set_timer32_match( timer_num, match_enable, 0 );

    /* Enable the selected PWMs and enable Match3 */
    LPC_CT32B0->PWMC = MATCH3|(match_enable);

    /* Setup the match registers */
    /* set the period value to a global variable */
    timer32_0_period = period;
    LPC_CT32B0->MR3 = timer32_0_period;
    LPC_CT32B0->MR0 = timer32_0_period/2;
    LPC_CT32B0->MR1 = timer32_0_period/2;
    LPC_CT32B0->MR2 = timer32_0_period/2;
    LPC_CT32B0->MCR = 1<<10;				/* Reset on MR3 */
  }
}

/******************************************************************************
** Function name:		pwm32_setMatch
**
** Descriptions:		Set the pwm32 match values
**
** parameters:			timer number, match numner and the value
**
** Returned value:		None
** 
******************************************************************************/
void setMatch_timer32PWM (uint8_t timer_num, uint8_t match_nr, uint32_t value)
{
  if (timer_num)
  {
    switch (match_nr)
    {
      case 0:
        LPC_CT32B1->MR0 = value;
      break;
      case 1: 
        LPC_CT32B1->MR1 = value;
      break;
      case 2:
        LPC_CT32B1->MR2 = value;
      break;
      case 3: 
        LPC_CT32B1->MR3 = value;
      break;
      default:
      break;
    }	
  }
  else 
  {
    switch (match_nr)
    {
      case 0:
        LPC_CT32B0->MR0 = value;
      break;
      case 1: 
        LPC_CT32B0->MR1 = value;
      break;
      case 2:
        LPC_CT32B0->MR2 = value;
      break;
      case 3: 
        LPC_CT32B0->MR3 = value;
      break;
      default:
      break;
    }	
  }
}

/******************************************************************************
**                            End Of File
******************************************************************************/
