/****************************************************************************
 *   $Id:: uart.c 7125 2011-04-15 00:22:12Z usb01267                        $
 *   Project: NXP LPC13Uxx UART example
 *
 *   Description:
 *     This file contains UART code example which include UART
 *     initialization, UART interrupt handler, and related APIs for
 *     UART access.
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
#include "type.h"
#include "uart.h"

volatile uint32_t UARTStatus;
volatile uint8_t  UARTTxEmpty = 1;
volatile uint8_t  UARTBuffer[BUFSIZE];
volatile uint32_t UARTCount = 0;

#if AUTOBAUD_ENABLE
volatile uint32_t UARTAutoBaud = 0, AutoBaudTimeout = 0;
#endif

/*****************************************************************************
** Function name:		USART_IRQHandler
**
** Descriptions:		USART interrupt handler
**
** parameters:			None
** Returned value:		None
**
*****************************************************************************/
void USART_IRQHandler(void)
{
  uint8_t IIRValue, LSRValue;
  uint8_t Dummy = Dummy;

  IIRValue = LPC_USART->IIR;

  IIRValue >>= 1;			/* skip pending bit in IIR */
  IIRValue &= 0x07;			/* check bit 1~3, interrupt identification */
  if (IIRValue == IIR_RLS)		/* Receive Line Status */
  {
    LSRValue = LPC_USART->LSR;
    /* Receive Line Status */
    if (LSRValue & (LSR_OE | LSR_PE | LSR_FE | LSR_RXFE | LSR_BI))
    {
      /* There are errors or break interrupt */
      /* Read LSR will clear the interrupt */
      UARTStatus = LSRValue;
      Dummy = LPC_USART->RBR;	/* Dummy read on RX to clear
								interrupt, then bail out */
      return;
    }
    if (LSRValue & LSR_RDR)	/* Receive Data Ready */
    {
      /* If no error on RLS, normal ready, save into the data buffer. */
      /* Note: read RBR will clear the interrupt */
      UARTBuffer[UARTCount++] = LPC_USART->RBR;
      if (UARTCount == BUFSIZE)
      {
        UARTCount = 0;		/* buffer overflow */
      }
    }
  }
  else if (IIRValue == IIR_RDA)	/* Receive Data Available */
  {
    /* Receive Data Available */
    UARTBuffer[UARTCount++] = LPC_USART->RBR;
    if (UARTCount == BUFSIZE)
    {
      UARTCount = 0;		/* buffer overflow */
    }
  }
  else if (IIRValue == IIR_CTI)	/* Character timeout indicator */
  {
    /* Character Time-out indicator */
    UARTStatus |= 0x100;		/* Bit 9 as the CTI error */
  }
  else if (IIRValue == IIR_THRE)	/* THRE, transmit holding register empty */
  {
    /* THRE interrupt */
    LSRValue = LPC_USART->LSR;		/* Check status in the LSR to see if
								valid data in U0THR or not */
    if (LSRValue & LSR_THRE)
    {
      UARTTxEmpty = 1;
    }
    else
    {
      UARTTxEmpty = 0;
    }
  }
#if AUTOBAUD_ENABLE
  if (LPC_USART->IIR & IIR_ABEO) /* End of Auto baud */
  {
	LPC_USART->IER &= ~IIR_ABEO;
	/* clear bit ABEOInt in the IIR by set ABEOIntClr in the ACR register */
	LPC_USART->ACR |= IIR_ABEO;
	UARTAutoBaud = 1;
  }
  else if (LPC_USART->IIR & IIR_ABTO)/* Auto baud time out */
  {
	LPC_USART->IER &= ~IIR_ABTO;
	AutoBaudTimeout = 1;
	/* clear bit ABTOInt in the IIR by set ABTOIntClr in the ACR register */
	LPC_USART->ACR |= IIR_ABTO;
  }
#endif
  return;
}

#if MODEM_TEST
/*****************************************************************************
** Function name:		ModemInit
**
** Descriptions:		Initialize UART0 port as modem, setup pin select.
**
** parameters:			None
** Returned value:		None
**
*****************************************************************************/
void ModemInit( void )
{

  LPC_IOCON->PIO0_7 &= ~0x07;     /* UART I/O config */
  LPC_IOCON->PIO0_7 |= 0x01;      /* UART CTS */
  LPC_IOCON->PIO0_17 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO0_17 |= 0x01;     /* UART RTS */
#if 1
  LPC_IOCON->PIO1_13 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_13 |= 0x01;     /* UART DTR */
  LPC_IOCON->PIO1_14 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_14 |= 0x01;     /* UART DSR */
  LPC_IOCON->PIO1_15 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_15 |= 0x01;     /* UART DCD */
  LPC_IOCON->PIO1_16 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_16 |= 0x01;     /* UART RI */

#else
  LPC_IOCON->PIO1_19 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_19 |= 0x01;     /* UART DTR */
  LPC_IOCON->PIO1_20 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_20 |= 0x01;     /* UART DSR */
  LPC_IOCON->PIO1_21 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_21 |= 0x01;     /* UART DCD */
  LPC_IOCON->PIO1_22 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_22 |= 0x01;     /* UART RI */
#endif
  LPC_USART->MCR = 0xC0;          /* Enable Auto RTS and Auto CTS. */
  return;
}
#endif

/***********************************************************************
 *
 * Function: uart_set_divisors
 *
 * Purpose: Determines best dividers to get a target clock rate
 *
 * Processing:
 *     See function.
 *
 * Parameters:
 *     UARTClk    : UART clock
 *     baudrate   : Desired UART baud rate
 *
 * Outputs:
 *	  baudrate : Sets the estimated buadrate value in DLL, DLM, and FDR.
 *
 * Returns: Error status.
 *
 * Notes: None
 *
 **********************************************************************/
uint32_t uart_set_divisors(uint32_t UARTClk, uint32_t baudrate)
{
  uint32_t uClk;
  uint32_t calcBaudrate = 0;
  uint32_t temp = 0;

  uint32_t mulFracDiv, dividerAddFracDiv;
  uint32_t diviser = 0 ;
  uint32_t mulFracDivOptimal = 1;
  uint32_t dividerAddOptimal = 0;
  uint32_t diviserOptimal = 0;

  uint32_t relativeError = 0;
  uint32_t relativeOptimalError = 100000;

  /* get UART block clock */
  uClk = UARTClk >> 4; /* div by 16 */
  /* In the Uart IP block, baud rate is calculated using FDR and DLL-DLM registers
   * The formula is :
   * BaudRate= uClk * (mulFracDiv/(mulFracDiv+dividerAddFracDiv) / (16 * (DLL)
   * It involves floating point calculations. That's the reason the formulae are adjusted with
   * Multiply and divide method.*/
  /* The value of mulFracDiv and dividerAddFracDiv should comply to the following expressions:
   * 0 < mulFracDiv <= 15, 0 <= dividerAddFracDiv <= 15 */
  for (mulFracDiv = 1; mulFracDiv <= 15; mulFracDiv++)
  {
    for (dividerAddFracDiv = 0; dividerAddFracDiv <= 15; dividerAddFracDiv++)
    {
      temp = (mulFracDiv * uClk) / ((mulFracDiv + dividerAddFracDiv));
      diviser = temp / baudrate;
      if ((temp % baudrate) > (baudrate / 2))
        diviser++;

      if (diviser > 2 && diviser < 65536)
      {
        calcBaudrate = temp / diviser;

        if (calcBaudrate <= baudrate)
          relativeError = baudrate - calcBaudrate;
        else
          relativeError = calcBaudrate - baudrate;

        if ((relativeError < relativeOptimalError))
        {
          mulFracDivOptimal = mulFracDiv ;
          dividerAddOptimal = dividerAddFracDiv;
          diviserOptimal = diviser;
          relativeOptimalError = relativeError;
          if (relativeError == 0)
            break;
        }
      } /* End of if */
    } /* end of inner for loop */
    if (relativeError == 0)
      break;
  } /* end of outer for loop  */

  if (relativeOptimalError < (baudrate / 30))
  {
    /* Set the `Divisor Latch Access Bit` and enable so the DLL/DLM access*/
    /* Initialise the `Divisor latch LSB` and `Divisor latch MSB` registers */
    LPC_USART->DLM = (diviserOptimal >> 8) & 0xFF;
    LPC_USART->DLL = diviserOptimal & 0xFF;

    /* Initialise the Fractional Divider Register */
    LPC_USART->FDR = ((mulFracDivOptimal & 0xF) << 4) | (dividerAddOptimal & 0xF);
    return( TRUE );
  }
  return ( FALSE );
}

/*****************************************************************************
** Function name:		UARTInit
**
** Descriptions:		Initialize UART0 port, setup pin select,
**				clock, parity, stop bits, FIFO, etc.
**
** parameters:			UART baudrate
** Returned value:		None
**
*****************************************************************************/
void UARTInit(uint32_t baudrate)
{
#if !AUTOBAUD_ENABLE
  uint32_t Fdiv;
#endif
  volatile uint32_t regVal;

  UARTTxEmpty = 1;
  UARTCount = 0;

  NVIC_DisableIRQ(USART_IRQn);
  /* Select only one location from below. */
#if 1
  LPC_IOCON->PIO0_18 &= ~0x07;    /*  UART I/O config */
  LPC_IOCON->PIO0_18 |= 0x01;     /* UART RXD */
  LPC_IOCON->PIO0_19 &= ~0x07;
  LPC_IOCON->PIO0_19 |= 0x01;     /* UART TXD */
#endif
#if 0
  LPC_IOCON->PIO1_14 &= ~0x07;    /*  UART I/O config */
  LPC_IOCON->PIO1_14 |= 0x03;     /* UART RXD */
  LPC_IOCON->PIO1_13 &= ~0x07;
  LPC_IOCON->PIO1_13 |= 0x03;     /* UART TXD */
#endif
#if 0
  LPC_IOCON->PIO1_17 &= ~0x07;    /*  UART I/O config */
  LPC_IOCON->PIO1_17 |= 0x02;     /* UART RXD */
  LPC_IOCON->PIO1_18 &= ~0x07;
  LPC_IOCON->PIO1_18 |= 0x02;     /* UART TXD */
#endif
#if 0
  LPC_IOCON->PIO1_26 &= ~0x07;    /*  UART I/O config */
  LPC_IOCON->PIO1_26 |= 0x02;     /* UART RXD */
  LPC_IOCON->PIO1_27 &= ~0x07;
  LPC_IOCON->PIO1_27 |= 0x02;     /* UART TXD */
#endif

  /* Enable UART clock */
  LPC_SYSCON->SYSAHBCLKCTRL |= (1<<12);
  LPC_SYSCON->UARTCLKDIV = 0x1;     /* divided by 1 */

  LPC_USART->LCR = 0x83;            /* 8 bits, no Parity, 1 Stop bit */
#if !AUTOBAUD_ENABLE
#if FDR_CALIBRATION
	if ( uart_set_divisors(SystemCoreClock/LPC_SYSCON->UARTCLKDIV, baudrate) != TRUE )
	{
      Fdiv = ((SystemCoreClock/LPC_SYSCON->UARTCLKDIV)/16)/baudrate ;	/*baud rate */
      LPC_USART->DLM = Fdiv / 256;
      LPC_USART->DLL = Fdiv % 256;
	  LPC_USART->FDR = 0x10;		/* Default */
	}
#else
    Fdiv = ((SystemCoreClock/LPC_SYSCON->UARTCLKDIV)/16)/baudrate ;	/*baud rate */
    LPC_USART->DLM = Fdiv / 256;
    LPC_USART->DLL = Fdiv % 256;
	LPC_USART->FDR = 0x10;		/* Default */
#endif
#endif
  LPC_USART->LCR = 0x03;		/* DLAB = 0 */
  LPC_USART->FCR = 0x07;		/* Enable and reset TX and RX FIFO. */

  /* Read to clear the line status. */
  regVal = LPC_USART->LSR;

  /* Ensure a clean start, no data in either TX or RX FIFO. */
  while (( LPC_USART->LSR & (LSR_THRE|LSR_TEMT)) != (LSR_THRE|LSR_TEMT) );
  while ( LPC_USART->LSR & LSR_RDR )
  {
	regVal = LPC_USART->RBR;	/* Dump data from RX FIFO */
  }

  /* Enable the UART Interrupt */
  NVIC_EnableIRQ(USART_IRQn);

#if TX_INTERRUPT
  LPC_USART->IER = IER_RBR | IER_THRE | IER_RLS;	/* Enable UART interrupt */
#else
  LPC_USART->IER = IER_RBR | IER_RLS;	/* Enable UART interrupt */
#endif
#if AUTOBAUD_ENABLE
    LPC_USART->IER |= IER_ABEO | IER_ABTO;
#endif
  return;
}

/*****************************************************************************
** Function name:		UARTSend
**
** Descriptions:		Send a block of data to the UART 0 port based
**				on the data length
**
** parameters:		buffer pointer, and data length
** Returned value:	None
**
*****************************************************************************/
void UARTSend(uint8_t *BufferPtr, uint32_t Length)
{

  while ( Length != 0 )
  {
	  /* THRE status, contain valid data */
#if !TX_INTERRUPT
	  while ( !(LPC_USART->LSR & LSR_THRE) );
	  LPC_USART->THR = *BufferPtr;
#else
	  /* Below flag is set inside the interrupt handler when THRE occurs. */
      while ( !(UARTTxEmpty & 0x01) );
	  LPC_USART->THR = *BufferPtr;
      UARTTxEmpty = 0;	/* not empty in the THR until it shifts out */
#endif
      BufferPtr++;
      Length--;
  }
  return;
}

/*****************************************************************************
** Function name:		print_string
**
** Descriptions:		print out string on the terminal
**
** parameters:			pointer to the string end with NULL char.
** Returned value:		none.
**
*****************************************************************************/
void print_string( uint8_t *str_ptr )
{
  while(*str_ptr != 0x00)
  {
    while((LPC_USART->LSR & 0x60) != 0x60);
    LPC_USART->THR = *str_ptr;
    str_ptr++;
  }
  return;
}

/*****************************************************************************
** Function name:		get_key
**
** Descriptions:		Get a character from the terminal
**
** parameters:			None
** Returned value:		character, zero is none.
**
*****************************************************************************/
uint8_t get_key( void )
{
  uint8_t dummy;

  while ( !(LPC_USART->LSR & 0x01) );
  dummy = LPC_USART->RBR;
  if ((dummy>=65) && (dummy<=90))
  {
	/* convert capital to non-capital character, A2a, B2b, C2c. */
	dummy +=32;
  }
  /* echo */
  LPC_USART->THR = dummy;
  return(dummy);
}

/******************************************************************************
**                            End Of File
******************************************************************************/
