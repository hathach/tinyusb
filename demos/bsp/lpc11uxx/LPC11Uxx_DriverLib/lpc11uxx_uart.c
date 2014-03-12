/*****************************************************************************
 *   uart.c:  UART API file for NXP LPC11xx Family Microprocessors
 *
 *   Copyright(C) 2008, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   2009.12.07  ver 1.00    Preliminary version, first Release
 *
******************************************************************************/

#include <stdint.h>
#include "LPC11Uxx.h"
#include "lpc11uxx_uart.h"

volatile uint32_t UARTStatus;
volatile uint8_t  UARTTxEmpty = 1;
volatile uint8_t  UARTBuffer[BUFSIZE];
volatile uint32_t UARTCount = 0;

/*****************************************************************************
** Function name:		UART_IRQHandler
**
** Descriptions:		UART interrupt handler
**
** parameters:			None
** Returned value:		None
**
*****************************************************************************/
void UART_IRQHandler(void)
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
  LPC_IOCON->PIO2_0 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO2_0 |= 0x01;     /* UART DTR */
  LPC_IOCON->PIO0_7 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO0_7 |= 0x01;     /* UART CTS */
  LPC_IOCON->PIO1_5 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO1_5 |= 0x01;     /* UART RTS */
#if 1
  LPC_IOCON->DSR_LOC	= 0;
  LPC_IOCON->PIO2_1 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO2_1 |= 0x01;     /* UART DSR */

  LPC_IOCON->DCD_LOC	= 0;
  LPC_IOCON->PIO2_2 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO2_2 |= 0x01;     /* UART DCD */

  LPC_IOCON->RI_LOC	= 0;
  LPC_IOCON->PIO2_3 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO2_3 |= 0x01;     /* UART RI */

#else
  LPC_IOCON->DSR_LOC = 1;
  LPC_IOCON->PIO3_1 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO3_1 |= 0x01;     /* UART DSR */

  LPC_IOCON->DCD_LOC = 1;
  LPC_IOCON->PIO3_2 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO3_2 |= 0x01;     /* UART DCD */

  LPC_IOCON->RI_LOC = 1;
  LPC_IOCON->PIO3_3 &= ~0x07;    /* UART I/O config */
  LPC_IOCON->PIO3_3 |= 0x01;     /* UART RI */
#endif
  LPC_USART->MCR = 0xC0;          /* Enable Auto RTS and Auto CTS. */
  return;
}
#endif

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
  uint32_t Fdiv;
  uint32_t regVal;

  UARTTxEmpty = 1;
  UARTCount = 0;

  NVIC_DisableIRQ(UART_IRQn);

  LPC_IOCON->PIO0_18 &= ~0x07;    /*  UART I/O config */
  LPC_IOCON->PIO0_18 |= 0x01;     /* UART RXD */
  LPC_IOCON->PIO0_19 &= ~0x07;
  LPC_IOCON->PIO0_19 |= 0x01;     /* UART TXD */
  /* Enable UART clock */
  LPC_SYSCON->SYSAHBCLKCTRL |= (1<<12);
  LPC_SYSCON->UARTCLKDIV = 0x1;     /* divided by 1 */

  LPC_USART->LCR = 0x83;            /* 8 bits, no Parity, 1 Stop bit */
  regVal = LPC_SYSCON->UARTCLKDIV;

  Fdiv = (((SystemCoreClock*LPC_SYSCON->SYSAHBCLKDIV)/regVal)/16)/baudrate ;	/*baud rate */

  LPC_USART->DLM = Fdiv / 256;
  LPC_USART->DLL = Fdiv % 256;
  LPC_USART->LCR = 0x03;		/* DLAB = 0 */
  LPC_USART->FCR = 0x07;		/* Enable and reset TX and RX FIFO. */

  /* Read to clear the line status. */
  regVal = LPC_USART->LSR;

  /* Ensure a clean start, no data in either TX or RX FIFO. */
// CodeRed - added parentheses around comparison in operand of &
  while (( LPC_USART->LSR & (LSR_THRE|LSR_TEMT)) != (LSR_THRE|LSR_TEMT) );
  while ( LPC_USART->LSR & LSR_RDR )
  {
	regVal = LPC_USART->RBR;	/* Dump data from RX FIFO */
  }

  /* Enable the UART Interrupt */
  NVIC_EnableIRQ(UART_IRQn);

#if CONFIG_UART_ENABLE_INTERRUPT==1
#if CONFIG_UART_ENABLE_TX_INTERRUPT==1
  LPC_USART->IER = IER_RBR | IER_THRE | IER_RLS;	/* Enable UART interrupt */
#else
  LPC_USART->IER = IER_RBR | IER_RLS;	/* Enable UART interrupt */
#endif
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
#if CONFIG_UART_ENABLE_TX_INTERRUPT==1
	  /* Below flag is set inside the interrupt handler when THRE occurs. */
      while ( !(UARTTxEmpty & 0x01) );
	  LPC_USART->THR = *BufferPtr;
      UARTTxEmpty = 0;	/* not empty in the THR until it shifts out */
#else
	  while ( !(LPC_USART->LSR & LSR_THRE) );
	  LPC_USART->THR = *BufferPtr;
#endif
      BufferPtr++;
      Length--;
  }
  return;
}

/******************************************************************************
**                            End Of File
******************************************************************************/
