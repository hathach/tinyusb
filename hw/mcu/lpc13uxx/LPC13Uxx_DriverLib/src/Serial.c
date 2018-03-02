/******************************************************************************/
/* SERIAL.C: Low Level Serial Routines                                        */
/******************************************************************************/
/* This file is part of the uVision/ARM development tools.                    */
/* Copyright (c) 2005-2006 Keil Software. All rights reserved.                */
/* This software may only be used under the terms of a valid, current,        */
/* end user licence from KEIL for a compatible version of KEIL software       */
/* development tools. Nothing else gives you the right to use this software.  */
/******************************************************************************/

#include "LPC13Uxx.h"                     /* LPC13Uxx definitions              */
#include "uart.h"

#define CR     0x0D

/* implementation of putchar (also used by printf function to output data)    */
int sendchar (int ch)  {                 /* Write character to Serial Port    */


  if (ch == '\n')  {
    while (!(LPC_USART->LSR & 0x20));
    LPC_USART->THR = CR;                          /* output CR */
  }
  while (!(LPC_USART->LSR & 0x20));
  return (LPC_USART->THR = ch);
}


int getkey (void)  {                     /* Read character from Serial Port   */

  while (!(LPC_USART->LSR & 0x01));
  return (LPC_USART->RBR);
}
