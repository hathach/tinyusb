/****************************************************************************
 *   $Id:: timer16.c 6950 2011-03-23 22:09:44Z usb00423                     $
 *   Project: NXP LPC13Uxx 16-bit timer example
 *
 *   Description:
 *     This file contains 16-bit timer code example which include timer 
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
#include "timer16.h"
#include "nmi.h"

volatile uint32_t timer16_0_counter[4] = {0,0,0,0};
volatile uint32_t timer16_1_counter[4] = {0,0,0,0};
volatile uint32_t timer16_0_capture[4] = {0,0,0,0};
volatile uint32_t timer16_1_capture[4] = {0,0,0,0};
volatile uint32_t timer16_0_period = 0;
volatile uint32_t timer16_1_period = 0;

/*****************************************************************************
** Function name:		delayMs
**
** Descriptions:		Start the timer delay in milo seconds
**						until elapsed
**
** parameters:			timer number, Delay value in milo second			 
** 						
** Returned value:		None
** 
*****************************************************************************/
void delayMs(uint8_t timer_num, uint32_t delayInMs)
{
  if (timer_num == 0)
  {
    /*
    * setup timer #0 for delay
    */
    LPC_CT16B0->TCR = 0x02;		/* reset timer */
    LPC_CT16B0->PR  = 0x00;		/* set prescaler to zero */
    LPC_CT16B0->MR0 = delayInMs * (SystemCoreClock / 1000);
    LPC_CT16B0->IR  = 0xff;		/* reset all interrrupts */
    LPC_CT16B0->MCR = 0x04;		/* stop timer on match */
    LPC_CT16B0->TCR = 0x01;		/* start timer */
    /* wait until delay time has elapsed */
    while (LPC_CT16B0->TCR & 0x01);
  }
  else if (timer_num == 1)
  {
    /*
    * setup timer #1 for delay
    */
    LPC_CT16B1->TCR = 0x02;		/* reset timer */
    LPC_CT16B1->PR  = 0x00;		/* set prescaler to zero */
    LPC_CT16B1->MR0 = delayInMs * (SystemCoreClock / 1000);
    LPC_CT16B1->IR  = 0xff;		/* reset all interrrupts */
    LPC_CT16B1->MCR = 0x04;		/* stop timer on match */
    LPC_CT16B1->TCR = 0x01;		/* start timer */
    /* wait until delay time has elapsed */
    while (LPC_CT16B1->TCR & 0x01);
  }
  return;
}

/******************************************************************************
** Function name:		CT16B0_IRQHandler
**
** Descriptions:		Timer/CounterX and CaptureX interrupt handler
**
** parameters:			None
** Returned value:		None
** 
******************************************************************************/
void CT16B0_IRQHandler(void)
{
  if ( LPC_CT16B0->IR & (0x1<<0) )
  {
	LPC_CT16B0->IR = 0x1<<0;			/* clear interrupt flag */
	timer16_0_counter[0]++;
  }
  if ( LPC_CT16B0->IR & (0x1<<1) )
  {
	LPC_CT16B0->IR = 0x1<<1;			/* clear interrupt flag */
	timer16_0_counter[1]++;
  }
  if ( LPC_CT16B0->IR & (0x1<<2) )
  {
	LPC_CT16B0->IR = 0x1<<2;			/* clear interrupt flag */
	timer16_0_counter[2]++;
  }
  if ( LPC_CT16B0->IR & (0x1<<3) )
  {
	LPC_CT16B0->IR = 0x1<<3;			/* clear interrupt flag */
	timer16_0_counter[3]++;
  }
  if ( LPC_CT16B0->IR & (0x1<<4) )
  {
	LPC_CT16B0->IR = 0x1<<4;		/* clear interrupt flag */
	timer16_0_capture[0]++;
  }
  if ( LPC_CT16B0->IR & (0x1<<5) )
  {
	LPC_CT16B0->IR = 0x1<<5;		/* clear interrupt flag */
	timer16_0_capture[1]++;
  }
  if ( LPC_CT16B0->IR & (0x1<<6) )
  {
	LPC_CT16B0->IR = 0x1<<6;		/* clear interrupt flag */
	timer16_0_capture[2]++;
  }
  if ( LPC_CT16B0->IR & (0x1<<7) )
  {
	LPC_CT16B0->IR = 0x1<<7;		/* clear interrupt flag */
	timer16_0_capture[3]++;
  }
  return;
}

/******************************************************************************
** Function name:		CT16B1_IRQHandler
**
** Descriptions:		Timer/CounterX and CaptureX interrupt handler
**
** parameters:			None
** Returned value:		None
** 
******************************************************************************/
void CT16B1_IRQHandler(void)
{
  if ( LPC_CT16B1->IR & (0x1<<0) )
  {  
	LPC_CT16B1->IR = 0x1<<0;			/* clear interrupt flag */
	timer16_1_counter[0]++;
  }
  if ( LPC_CT16B1->IR & (0x1<<1) )
  {  
	LPC_CT16B1->IR = 0x1<<1;			/* clear interrupt flag */
	timer16_1_counter[1]++;
  }
  if ( LPC_CT16B1->IR & (0x1<<2) )
  {  
	LPC_CT16B1->IR = 0x1<<2;			/* clear interrupt flag */
	timer16_1_counter[2]++;
  }
  if ( LPC_CT16B1->IR & (0x1<<3) )
  {  
	LPC_CT16B1->IR = 0x1<<3;			/* clear interrupt flag */
	timer16_1_counter[3]++;
  }
  if ( LPC_CT16B1->IR & (0x1<<4) )
  {
	LPC_CT16B1->IR = 0x1<<4;		/* clear interrupt flag */
	timer16_1_capture[0]++;
  }
  if ( LPC_CT16B1->IR & (0x1<<5) )
  {
	LPC_CT16B1->IR = 0x1<<5;		/* clear interrupt flag */
	timer16_1_capture[1]++;
  }
  if ( LPC_CT16B1->IR & (0x1<<6) )
  {
	LPC_CT16B1->IR = 0x1<<6;		/* clear interrupt flag */
	timer16_1_capture[2]++;
  }
  if ( LPC_CT16B1->IR & (0x1<<7) )
  {
	LPC_CT16B1->IR = 0x1<<7;		/* clear interrupt flag */
	timer16_1_capture[3]++;
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
void enable_timer16(uint8_t timer_num)
{
  if ( timer_num == 0 )
  {
    LPC_CT16B0->TCR = 1;
  }
  else
  {
    LPC_CT16B1->TCR = 1;
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
void disable_timer16(uint8_t timer_num)
{
  if ( timer_num == 0 )
  {
    LPC_CT16B0->TCR = 0;
  }
  else
  {
    LPC_CT16B1->TCR = 0;
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
void reset_timer16(uint8_t timer_num)
{
  uint32_t regVal;

  if ( timer_num == 0 )
  {
    regVal = LPC_CT16B0->TCR;
    regVal |= 0x02;
    LPC_CT16B0->TCR = regVal;
  }
  else
  {
    regVal = LPC_CT16B1->TCR;
    regVal |= 0x02;
    LPC_CT16B1->TCR = regVal;
  }
  return;
}

/******************************************************************************
** Function name:		Set_timer_capture
**
** Descriptions:		set timer capture based on LOC number.
**
** parameters:			timer number and location number
** Returned value:		None
** 
******************************************************************************/
void set_timer16_capture(uint8_t timer_num, uint8_t location )
{
  if ( timer_num == 0 )
  {
	/*  Timer0_16 I/O config */
	if ( location == 0 )
	{
	  LPC_IOCON->PIO1_16           &= ~0x07;
	  LPC_IOCON->PIO1_16           |= 0x02;		/* Timer0_16 CAP0 */
	  LPC_IOCON->PIO1_17           &= ~0x07;
	  LPC_IOCON->PIO1_17           |= 0x01;		/* Timer0_16 CAP2 */
	}
	else if ( location == 1 )
	{
	  LPC_IOCON->PIO0_2            &= ~0x07;
	  LPC_IOCON->PIO0_2            |= 0x02;		/* Timer0_16 CAP0 */
	}
	else
	{
	  while ( 1 );				/* Fatal location number error */
	}
  }
  else
  {
	/*  Timer1_16 I/O config */
	if ( location == 0 )
	{
	  LPC_IOCON->PIO0_20           &= ~0x07;	/*  Timer1_16 I/O config */
	  LPC_IOCON->PIO0_20           |= 0x01;		/* Timer1_16 CAP0 */
	  LPC_IOCON->PIO1_18           &= ~0x07;
	  LPC_IOCON->PIO1_18           |= 0x01;		/* Timer1_16 CAP1 */
	}
	else
	{
	  while ( 1 );				/* Fatal location number error */
	}
  }	
  return;
}

/******************************************************************************
** Function name:		Set_timer_match
**
** Descriptions:		set timer match based on LOC number.
**
** parameters:			timer number, match enable, and location number 
** Returned value:		None
** 
******************************************************************************/
void set_timer16_match(uint8_t timer_num, uint8_t match_enable, uint8_t location)
{
  if ( timer_num == 0 )
  {
	if ( match_enable & 0x01 )
	{
	  /*  Timer0_16 I/O config */
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO0_8           &= ~0x07;	
		LPC_IOCON->PIO0_8           |= 0x02;		/* Timer0_16 MAT0 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO1_13           &= ~0x07;
		LPC_IOCON->PIO1_13           |= 0x02;		/* Timer0_16 MAT0 */
	  }
	}
	if ( match_enable & 0x02 )
	{
	  /*  Timer0_16 I/O config */
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO0_9           &= ~0x07;
		LPC_IOCON->PIO0_9           |= 0x02;		/* Timer0_16 MAT1 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO1_14           &= ~0x07;
		LPC_IOCON->PIO1_14           |= 0x02;		/* Timer0_16 MAT1 */
	  }
	}
	if ( match_enable & 0x04 )
	{
	  /*  Timer0_16 I/O config */
	  if ( location == 0 )
	  {
#ifdef __SWD_DISABLED
		LPC_IOCON->SWCLK_PIO0_10    &= ~0x07;
		LPC_IOCON->SWCLK_PIO0_10    |= 0x03;		/* Timer0_16 MAT2 */
#endif	
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO1_15           &= ~0x07;
		LPC_IOCON->PIO1_15           |= 0x02;		/* Timer0_16 MAT2 */
	  }
	}
  }
  else if ( timer_num == 1 )
  {
	if ( match_enable & 0x01 )
	{
	  /*  Timer1_16 I/O config */
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO0_21           &= ~0x07;
		LPC_IOCON->PIO0_21           |= 0x01;		/* Timer1_16 MAT0 */
	  }
	}
	if ( match_enable & 0x02 )
	{
	  /*  Timer1_16 I/O config */
	  if ( location == 0 )
	  {
		LPC_IOCON->PIO0_22           &= ~0x07;
		LPC_IOCON->PIO0_22           |= 0x02;		/* Timer1_16 MAT1 */
	  }
	  else if ( location == 1 )
	  {
		LPC_IOCON->PIO1_23           &= ~0x07;
		LPC_IOCON->PIO1_23           |= 0x01;		/* Timer1_16 MAT1 */
	  }
	}
  }
  return;		
}

/******************************************************************************
** Function name:		init_timer
**
** Descriptions:		Initialize timer, set timer interval, reset timer,
**						install timer interrupt handler
**
** parameters:			timer number and timer interval
** Returned value:		None
** 
******************************************************************************/
void init_timer16(uint8_t timer_num, uint32_t TimerInterval) 
{
  uint32_t i;

  if ( timer_num == 0 )
  {
    /* Some of the I/O pins need to be clearfully planned if
    you use below module because JTAG and TIMER CAP/MAT pins are muxed. */
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<7);

    LPC_CT16B0->MR0 = TimerInterval;
    LPC_CT16B0->MR1 = TimerInterval;
#if TIMER_MATCH
    for ( i = 0; i < 4; i++ )
    {
      timer16_0_counter[i] = 0;
    }
    set_timer16_match(timer_num, 0x07, 0);
    LPC_CT16B0->EMR &= ~(0xFF<<4);
    LPC_CT16B0->EMR |= ((0x3<<4)|(0x3<<6)|(0x3<<8));
#else
    for ( i = 0; i < 4; i++ )
    {
      timer16_0_capture[i] = 0;
    }
    set_timer16_capture(timer_num, 0);
    /* Capture 0 and 2 on rising edge, interrupt enable. */
    LPC_CT16B0->CCR = (0x5<<0)|(0x5<<6);
#endif
    LPC_CT16B0->MCR = (0x3<<0)|(0x3<<3);  /* Interrupt and Reset on MR0 and MR1 */
		
    /* Enable the TIMER0 Interrupt */
#if NMI_ENABLED
    NVIC_DisableIRQ(CT16B0_IRQn);
    NMI_Init( CT16B0_IRQn );
#else
    NVIC_EnableIRQ(CT16B0_IRQn);
#endif
  }
  else if ( timer_num == 1 )
  {
    /* Some of the I/O pins need to be clearfully planned if
    you use below module because JTAG and TIMER CAP/MAT pins are muxed. */
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<8);
    LPC_CT16B1->MR0 = TimerInterval;
    LPC_CT16B1->MR1 = TimerInterval;
#if TIMER_MATCH
    for ( i = 0; i < 4; i++ )
    {
      timer16_1_counter[i] = 0;
    }
    set_timer16_match(timer_num, 0x07, 0);
    LPC_CT16B1->EMR &= ~(0xFF<<4);
    LPC_CT16B1->EMR |= ((0x3<<4)|(0x3<<6)|(0x3<<8));
#else
    for ( i = 0; i < 4; i++ )
    {
      timer16_1_capture[i] = 0;
    }
    set_timer16_capture(timer_num, 0);
    /* Capture 0 and 1 on rising edge, interrupt enable. */
    LPC_CT16B1->CCR = (0x5<<0)|(0x5<<3);
#endif
    LPC_CT16B1->MCR = (0x3<<0)|(0x3<<3);  /* Interrupt and Reset on MR0 and MR1 */

    /* Enable the TIMER1 Interrupt */
#if NMI_ENABLED
    NVIC_DisableIRQ(CT16B1_IRQn);
    NMI_Init( CT16B1_IRQn );
#else
    NVIC_EnableIRQ(CT16B1_IRQn);
#endif
  }
  return;
}

/******************************************************************************
** Function name:		init_timer16PWM
**
** Descriptions:		Initialize timer as PWM
**
** parameters:			timer number, period and match enable:
**						match_enable[0] = PWM for MAT0 
**						match_enable[1] = PWM for MAT1
**						match_enable[2] = PWM for MAT2
**			
** Returned value:	None
** 
******************************************************************************/
void init_timer16PWM(uint8_t timer_num, uint32_t period, uint8_t match_enable, uint8_t cap_enabled)
{	
  disable_timer16(timer_num);

  if (timer_num == 1)
  {
    /* Some of the I/O pins need to be clearfully planned if
    you use below module because JTAG and TIMER CAP/MAT pins are muxed. */
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<8);
		
    /* Setup the external match register */
    LPC_CT16B1->EMR = (1<<EMC3)|(1<<EMC2)|(1<<EMC1)|(2<<EMC0)|(1<<3)|(match_enable);
		
    /* Setup the outputs */
    /* If match0 is enabled, set the output */
    set_timer16_match(timer_num, match_enable, 0 );
		
    /* Enable the selected PWMs and enable Match3 */
    LPC_CT16B1->PWMC = (1<<3)|(match_enable);
		
    /* Setup the match registers */
    /* set the period value to a global variable */
    timer16_1_period 	= period;
    LPC_CT16B1->MR3 	= timer16_1_period;
    LPC_CT16B1->MR0	= timer16_1_period/2;
    LPC_CT16B1->MR1	= timer16_1_period/2;
    LPC_CT16B1->MR2	= timer16_1_period/2;
		
    /* Set match control register */
    LPC_CT16B1->MCR = 1<<10;// | 1<<9;				/* Reset on MR3 */
		
    if (cap_enabled)
    {
	  /* Use location 0 for test. */
	  set_timer16_capture( timer_num, 0 );
      LPC_CT16B1->IR = 0xF;							/* clear interrupt flag */
			
      /* Set the capture control register */
      LPC_CT16B1->CCR = 7;
			
    }
    /* Enable the TIMER1 Interrupt */
    NVIC_EnableIRQ(CT16B1_IRQn);
  }
  else
  {
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<7);
		
    /* Setup the external match register */
    LPC_CT16B0->EMR = (1<<EMC3)|(1<<EMC2)|(1<<EMC1)|(1<<EMC0)|(1<<3)|(match_enable);
		
    /* Setup the outputs */
    /* If match0 is enabled, set the output */
    set_timer16_match(timer_num, match_enable, 0 );
			
    /* Enable the selected PWMs and enable Match3 */
    LPC_CT16B0->PWMC = (1<<3)|(match_enable);
		
    /* Setup the match registers */
    /* set the period value to a global variable */
    timer16_0_period = period;
    LPC_CT16B0->MR3 = timer16_0_period;
    LPC_CT16B0->MR0 = timer16_0_period/2;
    LPC_CT16B0->MR1 = timer16_0_period/2;
    LPC_CT16B0->MR2 = timer16_0_period/2;
		
    /* Set the match control register */
    LPC_CT16B0->MCR = 1<<10;				/* Reset on MR3 */
		
    /* Enable the TIMER1 Interrupt */
    NVIC_EnableIRQ(CT16B0_IRQn);
  }
}

/******************************************************************************
** Function name:		pwm16_setMatch
**
** Descriptions:		Set the pwm16 match values
**
** parameters:			timer number, match numner and the value
**
** Returned value:		None
** 
******************************************************************************/
void setMatch_timer16PWM (uint8_t timer_num, uint8_t match_nr, uint32_t value)
{
  if (timer_num)
  {
    switch (match_nr)
    {
      case 0:
        LPC_CT16B1->MR0 = value;
      break;
      case 1: 
        LPC_CT16B1->MR1 = value;
      break;
      case 2:
        LPC_CT16B1->MR2 = value;
      break;
      case 3: 
        LPC_CT16B1->MR3 = value;
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
        LPC_CT16B0->MR0 = value;
      break;
      case 1: 
        LPC_CT16B0->MR1 = value;
      break;
      case 2:
        LPC_CT16B0->MR2 = value;
      break;
      case 3: 
        LPC_CT16B0->MR3 = value;
      break;
      default:
      break;
    }	
  }
}

/******************************************************************************
**                            End Of File
******************************************************************************/
