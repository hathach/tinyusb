/*
 * @brief LPC11u6xx USART0 driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licenser disclaim any and
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

/* Counts instances of UART3 and 4 init calls for shared clock handling */
static uint8_t uart_3_4_cnt;

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Return UART clock ID from the UART register address */
static CHIP_SYSCTL_CLOCK_T getUARTClockID(LPC_USARTN_T *pUART)
{
	if (pUART == LPC_USART1) {
		return SYSCTL_CLOCK_USART1;
	}
	else if (pUART == LPC_USART2) {
		return SYSCTL_CLOCK_USART2;
	}

	return SYSCTL_CLOCK_USART3_4;
}

/* UART clock enable */
static void Chip_UARTN_EnableClock(LPC_USARTN_T *pUART)
{
	CHIP_SYSCTL_CLOCK_T clk = getUARTClockID(pUART);

	/* Special handling for shared UART 3/4 clock */
	if (clk == SYSCTL_CLOCK_USART3_4) {
		/* Does not handle unbalanced Init() and DeInit() calls */
		uart_3_4_cnt++;
	}

	Chip_Clock_EnablePeriphClock(clk);
}

/* UART clock disable */
static void Chip_UARTN_DisableClock(LPC_USARTN_T *pUART)
{
	CHIP_SYSCTL_CLOCK_T clk = getUARTClockID(pUART);

	/* Special handling for shared UART 3/4 clock */
	if (clk != SYSCTL_CLOCK_USART3_4) {
		Chip_Clock_DisablePeriphClock(clk);
	}
	else {
		/* Does not handle unbalanced Init() and DeInit() calls */
		uart_3_4_cnt--;
		if (uart_3_4_cnt == 0) {
			Chip_Clock_DisablePeriphClock(clk);
		}
	}
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Initialize the UART peripheral */
void Chip_UARTN_Init(LPC_USARTN_T *pUART)
{
	CHIP_SYSCTL_PERIPH_RESET_T resetID;

	/* Enable USART clock */
	Chip_UARTN_EnableClock(pUART);

	/* Select UART reset */
	if (pUART == LPC_USART1) {
		resetID = RESET_USART1;
	}
	else if (pUART == LPC_USART2) {
		resetID = RESET_USART2;
	}
	else if (pUART == LPC_USART3) {
		resetID = RESET_USART3;
	}
	else {
		resetID = RESET_USART4;
	}

	Chip_SYSCTL_PeriphReset(resetID);
}

/* Initialize the UART peripheral */
void Chip_UARTN_DeInit(LPC_USARTN_T *pUART)
{
	/* Disable USART clock */
	Chip_UARTN_DisableClock(pUART);
}

/* Transmit a byte array through the UART peripheral (non-blocking) */
int Chip_UARTN_Send(LPC_USARTN_T *pUART, const void *data, int numBytes)
{
	int sent = 0;
	uint8_t *p8 = (uint8_t *) data;

	/* Send until the transmit FIFO is full or out of bytes */
	while ((sent < numBytes) &&
		   ((Chip_UARTN_GetStatus(pUART) & UARTN_STAT_TXRDY) != 0)) {
		Chip_UARTN_SendByte(pUART, *p8);
		p8++;
		sent++;
	}

	return sent;
}

/* Transmit a byte array through the UART peripheral (blocking) */
int Chip_UARTN_SendBlocking(LPC_USARTN_T *pUART, const void *data, int numBytes)
{
	int pass, sent = 0;
	uint8_t *p8 = (uint8_t *) data;

	while (numBytes > 0) {
		pass = Chip_UARTN_Send(pUART, p8, numBytes);
		numBytes -= pass;
		sent += pass;
		p8 += pass;
	}

	return sent;
}

/* Read data through the UART peripheral (non-blocking) */
int Chip_UARTN_Read(LPC_USARTN_T *pUART, void *data, int numBytes)
{
	int readBytes = 0;
	uint8_t *p8 = (uint8_t *) data;

	/* Send until the transmit FIFO is full or out of bytes */
	while ((readBytes < numBytes) &&
		   ((Chip_UARTN_GetStatus(pUART) & UARTN_STAT_RXRDY) != 0)) {
		*p8 = Chip_UARTN_ReadByte(pUART);
		p8++;
		readBytes++;
	}

	return readBytes;
}

/* Read data through the UART peripheral (blocking) */
int Chip_UARTN_ReadBlocking(LPC_USARTN_T *pUART, void *data, int numBytes)
{
	int pass, readBytes = 0;
	uint8_t *p8 = (uint8_t *) data;

	while (readBytes < numBytes) {
		pass = Chip_UARTN_Read(pUART, p8, numBytes);
		numBytes -= pass;
		readBytes += pass;
		p8 += pass;
	}

	return readBytes;
}

/* Set baud rate for UART */
void Chip_UARTN_SetBaud(LPC_USARTN_T *pUART, uint32_t baudrate)
{
	uint32_t baudRateGenerator;
	baudRateGenerator = Chip_Clock_GetUSARTNBaseClockRate() / (16 * baudrate);
	pUART->BRG = baudRateGenerator - 1;	/* baud rate */
}

/* Set baud rate for UART using RTC32K oscillator */
void Chip_UARTN_SetBaudWithRTC32K(LPC_USARTN_T *pUART, uint32_t baudrate)
{
	/* Simple integer divide. 9600 is maximum baud rate. */
	pUART->BRG = (9600 / baudrate) - 1;

	pUART->CFG |= UARTN_MODE_32K;
}

/* UART receive-only interrupt handler for ring buffers */
void Chip_UARTN_RXIntHandlerRB(LPC_USARTN_T *pUART, RINGBUFF_T *pRB)
{
	/* New data will be ignored if data not popped in time */
	while ((Chip_UARTN_GetStatus(pUART) & UARTN_STAT_RXRDY) != 0) {
		uint8_t ch = Chip_UARTN_ReadByte(pUART);
		RingBuffer_Insert(pRB, &ch);
	}
}

/* UART transmit-only interrupt handler for ring buffers */
void Chip_UARTN_TXIntHandlerRB(LPC_USARTN_T *pUART, RINGBUFF_T *pRB)
{
	uint8_t ch;

	/* Fill FIFO until full or until TX ring buffer is empty */
	while (((Chip_UARTN_GetStatus(pUART) & UARTN_STAT_TXRDY) != 0) &&
		   RingBuffer_Pop(pRB, &ch)) {
		Chip_UARTN_SendByte(pUART, ch);
	}
}

/* Populate a transmit ring buffer and start UART transmit */
uint32_t Chip_UARTN_SendRB(LPC_USARTN_T *pUART, RINGBUFF_T *pRB, const void *data, int count)
{
	uint32_t ret;
	uint8_t *p8 = (uint8_t *) data;

	/* Don't let UART transmit ring buffer change in the UART IRQ handler */
	Chip_UARTN_IntDisable(pUART, UARTN_INTEN_TXRDY);

	/* Move as much data as possible into transmit ring buffer */
	ret = RingBuffer_InsertMult(pRB, p8, count);
	Chip_UARTN_TXIntHandlerRB(pUART, pRB);

	/* Add additional data to transmit ring buffer if possible */
	ret += RingBuffer_InsertMult(pRB, (p8 + ret), (count - ret));

	/* Enable UART transmit interrupt */
	Chip_UARTN_IntEnable(pUART, UARTN_INTEN_TXRDY);

	return ret;
}

/* Copy data from a receive ring buffer */
int Chip_UARTN_ReadRB(LPC_USARTN_T *pUART, RINGBUFF_T *pRB, void *data, int bytes)
{
	(void) pUART;

	return RingBuffer_PopMult(pRB, (uint8_t *) data, bytes);
}

/* UART receive/transmit interrupt handler for ring buffers */
void Chip_UARTN_IRQRBHandler(LPC_USARTN_T *pUART, RINGBUFF_T *pRXRB, RINGBUFF_T *pTXRB)
{
	/* Handle transmit interrupt if enabled */
	if ((Chip_UARTN_GetStatus(pUART) & UARTN_STAT_TXRDY) != 0) {
		Chip_UARTN_TXIntHandlerRB(pUART, pTXRB);

		/* Disable transmit interrupt if the ring buffer is empty */
		if (RingBuffer_IsEmpty(pTXRB)) {
			Chip_UARTN_IntDisable(pUART, UARTN_INTEN_TXRDY);
		}
	}

	/* Handle receive interrupt */
	Chip_UARTN_RXIntHandlerRB(pUART, pRXRB);
}
