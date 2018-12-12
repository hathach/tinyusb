/*
 * @brief LPC11u6x USART0 chip driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "chip.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Initializes the pUART peripheral */
void Chip_UART0_Init(LPC_USART0_T *pUART)
{
	/* A USART 0 divider of 1 is used with this driver */
	Chip_Clock_SetUSART0ClockDiv(1);

	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_UART0);

	/* Enable FIFOs by default, reset them */
	Chip_UART0_SetupFIFOS(pUART, (UART0_FCR_FIFO_EN | UART0_FCR_RX_RS | UART0_FCR_TX_RS));

	/* Disable fractional divider */
	pUART->FDR = 0x10;
}

/* De-initializes the pUART peripheral */
void Chip_UART0_DeInit(LPC_USART0_T *pUART)
{
  (void) pUART;
	Chip_Clock_DisablePeriphClock(SYSCTL_CLOCK_UART0);
}

/* Transmit a byte array through the UART peripheral (non-blocking) */
int Chip_UART0_Send(LPC_USART0_T *pUART, const void *data, int numBytes)
{
	int sent = 0;
	uint8_t *p8 = (uint8_t *) data;

	/* Send until the transmit FIFO is full or out of bytes */
	while ((sent < numBytes) &&
		   ((Chip_UART0_ReadLineStatus(pUART) & UART0_LSR_THRE) != 0)) {
		Chip_UART0_SendByte(pUART, *p8);
		p8++;
		sent++;
	}

	return sent;
}

/* Transmit a byte array through the UART peripheral (blocking) */
int Chip_UART0_SendBlocking(LPC_USART0_T *pUART, const void *data, int numBytes)
{
	int pass, sent = 0;
	uint8_t *p8 = (uint8_t *) data;

	while (numBytes > 0) {
		pass = Chip_UART0_Send(pUART, p8, numBytes);
		numBytes -= pass;
		sent += pass;
		p8 += pass;
	}

	return sent;
}

/* Read data through the UART peripheral (non-blocking) */
int Chip_UART0_Read(LPC_USART0_T *pUART, void *data, int numBytes)
{
	int readBytes = 0;
	uint8_t *p8 = (uint8_t *) data;

	/* Send until the transmit FIFO is full or out of bytes */
	while ((readBytes < numBytes) &&
		   ((Chip_UART0_ReadLineStatus(pUART) & UART0_LSR_RDR) != 0)) {
		*p8 = Chip_UART0_ReadByte(pUART);
		p8++;
		readBytes++;
	}

	return readBytes;
}

/* Read data through the UART peripheral (blocking) */
int Chip_UART0_ReadBlocking(LPC_USART0_T *pUART, void *data, int numBytes)
{
	int pass, readBytes = 0;
	uint8_t *p8 = (uint8_t *) data;

	while (numBytes > 0) {
		pass = Chip_UART0_Read(pUART, p8, numBytes);
		numBytes -= pass;
		readBytes += pass;
		p8 += pass;
	}

	return readBytes;
}

/* Determines and sets best dividers to get a target bit rate */
uint32_t Chip_UART0_SetBaud(LPC_USART0_T *pUART, uint32_t baudrate)
{
	uint32_t div, divh, divl, clkin;

	/* USART clock input divider of 1 */
	Chip_Clock_SetUSART0ClockDiv(1);

	/* Determine UART clock in rate without FDR */
	clkin = Chip_Clock_GetMainClockRate();
	div = clkin / (baudrate * 16);

	/* High and low halves of the divider */
	divh = div / 256;
	divl = div - (divh * 256);

	Chip_UART0_EnableDivisorAccess(pUART);
	Chip_UART0_SetDivisorLatches(pUART, divl, divh);
	Chip_UART0_DisableDivisorAccess(pUART);

	/* Fractional FDR already setup for 1 in UART init */

	return clkin / (div * 16);
}

/* UART receive-only interrupt handler for ring buffers */
void Chip_UART0_RXIntHandlerRB(LPC_USART0_T *pUART, RINGBUFF_T *pRB)
{
	/* New data will be ignored if data not popped in time */
	while (Chip_UART0_ReadLineStatus(pUART) & UART0_LSR_RDR) {
		uint8_t ch = Chip_UART0_ReadByte(pUART);
		RingBuffer_Insert(pRB, &ch);
	}
}

/* UART transmit-only interrupt handler for ring buffers */
void Chip_UART0_TXIntHandlerRB(LPC_USART0_T *pUART, RINGBUFF_T *pRB)
{
	uint8_t ch;

	/* Fill FIFO until full or until TX ring buffer is empty */
	while ((Chip_UART0_ReadLineStatus(pUART) & UART0_LSR_THRE) != 0 &&
		   RingBuffer_Pop(pRB, &ch)) {
		Chip_UART0_SendByte(pUART, ch);
	}
}

/* Populate a transmit ring buffer and start UART transmit */
uint32_t Chip_UART0_SendRB(LPC_USART0_T *pUART, RINGBUFF_T *pRB, const void *data, int bytes)
{
	uint32_t ret;
	uint8_t *p8 = (uint8_t *) data;

	/* Don't let UART transmit ring buffer change in the UART IRQ handler */
	Chip_UART0_IntDisable(pUART, UART0_IER_THREINT);

	/* Move as much data as possible into transmit ring buffer */
	ret = RingBuffer_InsertMult(pRB, p8, bytes);
	Chip_UART0_TXIntHandlerRB(pUART, pRB);

	/* Add additional data to transmit ring buffer if possible */
	ret += RingBuffer_InsertMult(pRB, (p8 + ret), (bytes - ret));

	/* Enable UART transmit interrupt */
	Chip_UART0_IntEnable(pUART, UART0_IER_THREINT);

	return ret;
}

/* Copy data from a receive ring buffer */
int Chip_UART0_ReadRB(LPC_USART0_T *pUART, RINGBUFF_T *pRB, void *data, int bytes)
{
	(void) pUART;

	return RingBuffer_PopMult(pRB, (uint8_t *) data, bytes);
}

/* UART receive/transmit interrupt handler for ring buffers */
void Chip_UART0_IRQRBHandler(LPC_USART0_T *pUART, RINGBUFF_T *pRXRB, RINGBUFF_T *pTXRB)
{
	/* Handle transmit interrupt if enabled */
	if (pUART->IER & UART0_IER_THREINT) {
		Chip_UART0_TXIntHandlerRB(pUART, pTXRB);

		/* Disable transmit interrupt if the ring buffer is empty */
		if (RingBuffer_IsEmpty(pTXRB)) {
			Chip_UART0_IntDisable(pUART, UART0_IER_THREINT);
		}
	}

	/* Handle receive interrupt */
	Chip_UART0_RXIntHandlerRB(pUART, pRXRB);
}

/* Determines and sets best dividers to get a target baud rate */
uint32_t Chip_UART0_SetBaudFDR(LPC_USART0_T *pUART, uint32_t baudrate)

{
	uint32_t uClk;
	uint32_t dval, mval;
	uint32_t dl;
	uint32_t rate16 = 16 * baudrate;
	uint32_t actualRate = 0;

	/* Get Clock rate */
	uClk = Chip_Clock_GetMainClockRate();

	/* The fractional is calculated as (PCLK  % (16 * Baudrate)) / (16 * Baudrate)
	 * Let's make it to be the ratio DivVal / MulVal
	 */
	dval = uClk % rate16;

	/* The PCLK / (16 * Baudrate) is fractional
	 * => dval = pclk % rate16
	 * mval = rate16;
	 * now mormalize the ratio
	 * dval / mval = 1 / new_mval
	 * new_mval = mval / dval
	 * new_dval = 1
	 */
	if (dval > 0) {
		mval = rate16 / dval;
		dval = 1;

		/* In case mval still bigger then 4 bits
		 * no adjustment require
		 */
		if (mval > 12) {
			dval = 0;
		}
	}
	dval &= 0xf;
	mval &= 0xf;
	dl = uClk / (rate16 + rate16 * dval / mval);

	/* Update UART registers */
	Chip_UART0_EnableDivisorAccess(pUART);
	Chip_UART0_SetDivisorLatches(pUART, UART0_LOAD_DLL(dl), UART0_LOAD_DLM(dl));
	Chip_UART0_DisableDivisorAccess(pUART);

	/* Set best fractional divider */
	pUART->FDR = (UART0_FDR_MULVAL(mval) | UART0_FDR_DIVADDVAL(dval));

	/* Return actual baud rate */
	actualRate = uClk / (16 * dl + 16 * dl * dval / mval);
	return actualRate;
}
