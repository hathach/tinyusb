/*
 * @brief LPC11xx UART chip driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2014
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

/* Autobaud status flag */
STATIC volatile FlagStatus ABsyncSts = RESET;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Returns clock for the peripheral block */
STATIC CHIP_SYSCTL_CLOCK_T Chip_UART_GetClockIndex(LPC_USART_T *pUART)
{
	CHIP_SYSCTL_CLOCK_T clkUART;

	if (pUART == LPC_UART1) {
		clkUART = SYSCTL_CLOCK_UART1;
	}
	else if (pUART == LPC_UART2) {
		clkUART = SYSCTL_CLOCK_UART2;
	}
	else if (pUART == LPC_UART3) {
		clkUART = SYSCTL_CLOCK_UART3;
	}
#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
	else if (pUART == LPC_UART4) {
		clkUART = SYSCTL_CLOCK_UART4;
	}
#endif
	else {
		clkUART = SYSCTL_CLOCK_UART0;
	}

	return clkUART;
}

/* UART Autobaud command interrupt handler */
STATIC void Chip_UART_ABIntHandler(LPC_USART_T *pUART)
{
	/* Handle End Of Autobaud interrupt */
	if((Chip_UART_ReadIntIDReg(pUART) & UART_IIR_ABEO_INT) != 0) {
        Chip_UART_SetAutoBaudReg(pUART, UART_ACR_ABEOINT_CLR); 
		Chip_UART_IntDisable(pUART, UART_IER_ABEOINT);
	    if (ABsyncSts == RESET) {
	        ABsyncSts = SET;
        }
	}
	
    /* Handle Autobaud Timeout interrupt */
	if((Chip_UART_ReadIntIDReg(pUART) & UART_IIR_ABTO_INT) != 0) {
        Chip_UART_SetAutoBaudReg(pUART, UART_ACR_ABTOINT_CLR); 
		Chip_UART_IntDisable(pUART, UART_IER_ABTOINT);
	}
}

#if defined(CHIP_LPC175X_6X)
/* Returns clock ID for the peripheral block */
STATIC CHIP_SYSCTL_PCLK_T Chip_UART_GetClkIndex(LPC_USART_T *pUART)
{
	CHIP_SYSCTL_PCLK_T clkUART;

	if (pUART == LPC_UART1) {
		clkUART = SYSCTL_PCLK_UART1;
	}
	else if (pUART == LPC_UART2) {
		clkUART = SYSCTL_PCLK_UART2;
	}
	else if (pUART == LPC_UART3) {
		clkUART = SYSCTL_PCLK_UART3;
	}
	else {
		clkUART = SYSCTL_PCLK_UART0;
	}

	return clkUART;
}
#endif

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Initializes the pUART peripheral */
void Chip_UART_Init(LPC_USART_T *pUART)
{
    uint32_t tmp;

	(void) tmp;
	
	/* Enable UART clocking. UART base clock(s) must already be enabled */
	Chip_Clock_EnablePeriphClock(Chip_UART_GetClockIndex(pUART));

	/* Enable FIFOs by default, reset them */
	Chip_UART_SetupFIFOS(pUART, (UART_FCR_FIFO_EN | UART_FCR_RX_RS | UART_FCR_TX_RS));
    
    /* Disable Tx */
    Chip_UART_TXDisable(pUART);
	
    /* Disable interrupts */
	pUART->IER = 0;
	/* Set LCR to default state */
	pUART->LCR = 0;
	/* Set ACR to default state */
	pUART->ACR = 0;
    /* Set RS485 control to default state */
	pUART->RS485CTRL = 0;
	/* Set RS485 delay timer to default state */
	pUART->RS485DLY = 0;
	/* Set RS485 addr match to default state */
	pUART->RS485ADRMATCH = 0;
	
    /* Clear MCR */
    if (pUART == LPC_UART1) {
		/* Set Modem Control to default state */
		pUART->MCR = 0;
		/*Dummy Reading to Clear Status */
		tmp = pUART->MSR;
	}

	/* Default 8N1, with DLAB disabled */
	Chip_UART_ConfigData(pUART, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT | UART_LCR_PARITY_DIS));

	/* Disable fractional divider */
	pUART->FDR = 0x10;
}

/* De-initializes the pUART peripheral */
void Chip_UART_DeInit(LPC_USART_T *pUART)
{
    /* Disable Tx */
    Chip_UART_TXDisable(pUART);

    /* Disable clock */
	Chip_Clock_DisablePeriphClock(Chip_UART_GetClockIndex(pUART));
}

/* Enable transmission on UART TxD pin */
void Chip_UART_TXEnable(LPC_USART_T *pUART)
{
#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
    if(pUART == LPC_UART4) {
        pUART->TER2 = UART_TER2_TXEN;
    }
    else {
#endif
        pUART->TER1 = UART_TER1_TXEN;
#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
    }
#endif
}

/* Disable transmission on UART TxD pin */
void Chip_UART_TXDisable(LPC_USART_T *pUART)
{
#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
    if(pUART == LPC_UART4) {
        pUART->TER2 = 0;
    }
    else {
#endif
        pUART->TER1 = 0;
#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
    }
#endif
}

/* Transmit a byte array through the UART peripheral (non-blocking) */
int Chip_UART_Send(LPC_USART_T *pUART, const void *data, int numBytes)
{
	int sent = 0;
	uint8_t *p8 = (uint8_t *) data;

	/* Send until the transmit FIFO is full or out of bytes */
	while ((sent < numBytes) &&
		   ((Chip_UART_ReadLineStatus(pUART) & UART_LSR_THRE) != 0)) {
		Chip_UART_SendByte(pUART, *p8);
		p8++;
		sent++;
	}

	return sent;
}

/* Check whether if UART is busy or not */
FlagStatus Chip_UART_CheckBusy(LPC_USART_T *pUART)
{
	if (pUART->LSR & UART_LSR_TEMT) {
		return RESET;
	}
	else {
		return SET;
	}
}

/* Transmit a byte array through the UART peripheral (blocking) */
int Chip_UART_SendBlocking(LPC_USART_T *pUART, const void *data, int numBytes)
{
	int pass, sent = 0;
	uint8_t *p8 = (uint8_t *) data;

	while (numBytes > 0) {
		pass = Chip_UART_Send(pUART, p8, numBytes);
		numBytes -= pass;
		sent += pass;
		p8 += pass;
	}

	return sent;
}

/* Read data through the UART peripheral (non-blocking) */
int Chip_UART_Read(LPC_USART_T *pUART, void *data, int numBytes)
{
	int readBytes = 0;
	uint8_t *p8 = (uint8_t *) data;

	/* Send until the transmit FIFO is full or out of bytes */
	while ((readBytes < numBytes) &&
		   ((Chip_UART_ReadLineStatus(pUART) & UART_LSR_RDR) != 0)) {
		*p8 = Chip_UART_ReadByte(pUART);
		p8++;
		readBytes++;
	}

	return readBytes;
}

/* Read data through the UART peripheral (blocking) */
int Chip_UART_ReadBlocking(LPC_USART_T *pUART, void *data, int numBytes)
{
	int pass, readBytes = 0;
	uint8_t *p8 = (uint8_t *) data;

	while (readBytes < numBytes) {
		pass = Chip_UART_Read(pUART, p8, numBytes);
		numBytes -= pass;
		readBytes += pass;
		p8 += pass;
	}

	return readBytes;
}

/* Determines and sets best dividers to get a target bit rate */
uint32_t Chip_UART_SetBaud(LPC_USART_T *pUART, uint32_t baudrate)
{
	uint32_t div, divh, divl, clkin;

	/* Determine UART clock in rate without FDR */
#if defined(CHIP_LPC175X_6X)
	clkin = Chip_Clock_GetPeripheralClockRate(Chip_UART_GetClkIndex(pUART));
#else
	clkin = Chip_Clock_GetPeripheralClockRate();
#endif
	div = clkin / (baudrate * 16);

	/* High and low halves of the divider */
	divh = div / 256;
	divl = div - (divh * 256);

	Chip_UART_EnableDivisorAccess(pUART);
	Chip_UART_SetDivisorLatches(pUART, divl, divh);
	Chip_UART_DisableDivisorAccess(pUART);

	/* Fractional FDR already setup for 1 in UART init */

	return clkin / div;
}

/* UART receive-only interrupt handler for ring buffers */
void Chip_UART_RXIntHandlerRB(LPC_USART_T *pUART, RINGBUFF_T *pRB)
{
	/* New data will be ignored if data not popped in time */
	while (Chip_UART_ReadLineStatus(pUART) & UART_LSR_RDR) {
		uint8_t ch = Chip_UART_ReadByte(pUART);
		RingBuffer_Insert(pRB, &ch);
	}
}

/* UART transmit-only interrupt handler for ring buffers */
void Chip_UART_TXIntHandlerRB(LPC_USART_T *pUART, RINGBUFF_T *pRB)
{
	uint8_t ch;

	/* Fill FIFO until full or until TX ring buffer is empty */
	while ((Chip_UART_ReadLineStatus(pUART) & UART_LSR_THRE) != 0 &&
		   RingBuffer_Pop(pRB, &ch)) {
		Chip_UART_SendByte(pUART, ch);
	}
}

/* Populate a transmit ring buffer and start UART transmit */
uint32_t Chip_UART_SendRB(LPC_USART_T *pUART, RINGBUFF_T *pRB, const void *data, int bytes)
{
	uint32_t ret;
	uint8_t *p8 = (uint8_t *) data;

	/* Don't let UART transmit ring buffer change in the UART IRQ handler */
	Chip_UART_IntDisable(pUART, UART_IER_THREINT);

	/* Move as much data as possible into transmit ring buffer */
	ret = RingBuffer_InsertMult(pRB, p8, bytes);
	Chip_UART_TXIntHandlerRB(pUART, pRB);

	/* Add additional data to transmit ring buffer if possible */
	ret += RingBuffer_InsertMult(pRB, (p8 + ret), (bytes - ret));

	/* Enable UART transmit interrupt */
	Chip_UART_IntEnable(pUART, UART_IER_THREINT);

	return ret;
}

/* Copy data from a receive ring buffer */
int Chip_UART_ReadRB(LPC_USART_T *pUART, RINGBUFF_T *pRB, void *data, int bytes)
{
	(void) pUART;

	return RingBuffer_PopMult(pRB, (uint8_t *) data, bytes);
}

/* UART receive/transmit interrupt handler for ring buffers */
void Chip_UART_IRQRBHandler(LPC_USART_T *pUART, RINGBUFF_T *pRXRB, RINGBUFF_T *pTXRB)
{
	/* Handle transmit interrupt if enabled */
	if (pUART->IER & UART_IER_THREINT) {
		Chip_UART_TXIntHandlerRB(pUART, pTXRB);

		/* Disable transmit interrupt if the ring buffer is empty */
		if (RingBuffer_IsEmpty(pTXRB)) {
			Chip_UART_IntDisable(pUART, UART_IER_THREINT);
		}
	}

	/* Handle receive interrupt */
	Chip_UART_RXIntHandlerRB(pUART, pRXRB);

    /* Handle Autobaud interrupts */
    Chip_UART_ABIntHandler(pUART);
}

/* Determines and sets best dividers to get a target baud rate */
uint32_t Chip_UART_SetBaudFDR(LPC_USART_T *pUART, uint32_t baudrate)

{
	uint32_t uClk;
	uint32_t actualRate = 0, d, m, bestd, bestm, tmp;
	uint32_t current_error, best_error;
	uint64_t best_divisor, divisor;
	uint32_t recalcbaud;

	/* Get Clock rate */
#if defined(CHIP_LPC175X_6X)
	uClk = Chip_Clock_GetPeripheralClockRate(Chip_UART_GetClkIndex(pUART));
#else
	uClk = Chip_Clock_GetPeripheralClockRate();
#endif
    
	/* In the Uart IP block, baud rate is calculated using FDR and DLL-DLM registers
	 * The formula is :
	 * BaudRate= uClk * (mulFracDiv/(mulFracDiv+dividerAddFracDiv) / (16 * (DLL)
	 * It involves floating point calculations. That's the reason the formulae are adjusted with
	 * Multiply and divide method.*/
	/* The value of mulFracDiv and dividerAddFracDiv should comply to the following expressions:
	 * 0 < mulFracDiv <= 15, 0 <= dividerAddFracDiv <= 15 */
	best_error = 0xFFFFFFFF;/* Worst case */
	bestd = 0;
	bestm = 0;
	best_divisor = 0;
	for (m = 1; m <= 15; m++) {
		for (d = 0; d < m; d++) {

			/*   The result here is a fixed point number.  The integer portion is in the upper 32 bits.
			 * The fractional portion is in the lower 32 bits.
			 */
			divisor = ((uint64_t) uClk << 28) * m / (baudrate * (m + d));

			/*   The fractional portion is the error. */
			current_error = divisor & 0xFFFFFFFF;

			/*   Snag the integer portion of the divisor. */
			tmp = divisor >> 32;

			/*   If closer to the next divisor... */
			if (current_error > ((uint32_t) 1 << 31)) {

				/* Increment to the next divisor... */
				tmp++;

				/* Now the error is the distance to the next divisor... */
				current_error = -current_error;
			}

			/*   Can't use a divisor that's less than 1 or more than 65535. */
			if ((tmp < 1) || (tmp > 65535)) {
				/* Out of range */
				continue;
			}

			/*   Also, if fractional divider is enabled can't use a divisor that is less than 3. */
			if ((d != 0) && (tmp < 3)) {
				/* Out of range */
				continue;
			}

			/*   Do we have a new best? */
			if (current_error < best_error) {
				best_error = current_error;
				best_divisor = tmp;
				bestd = d;
				bestm = m;

				/*   If error is 0, that's perfect.  We're done. */
				if (best_error == 0) {
					break;
				}
			}
		}	/* for (d) */

		/*   If error is 0, that's perfect.  We're done. */
		if (best_error == 0) {
			break;
		}
	}	/* for (m) */

	if (best_divisor == 0) {
		/* can not find best match */
		return 0;
	}

	recalcbaud = (uClk >> 4) * bestm / (best_divisor * (bestm + bestd));

	/* reuse best_error to evaluate baud error */
	if (baudrate > recalcbaud) {
		best_error = baudrate - recalcbaud;
	}
	else {
		best_error = recalcbaud - baudrate;
	}

	best_error = (best_error * 100) / baudrate;

    /* Update UART registers */
	Chip_UART_EnableDivisorAccess(pUART);
	Chip_UART_SetDivisorLatches(pUART, UART_LOAD_DLL(best_divisor), UART_LOAD_DLM(best_divisor));
	Chip_UART_DisableDivisorAccess(pUART);

	/* Set best fractional divider */
	pUART->FDR = (UART_FDR_MULVAL(bestm) | UART_FDR_DIVADDVAL(bestd));

	/* Return actual baud rate */
	actualRate = recalcbaud;

	return actualRate;
}

/* UART interrupt service routine */
FlagStatus Chip_UART_GetABEOStatus(LPC_USART_T *pUART)
{
	(void) pUART;
	return ABsyncSts;
}

/* Start/Stop Auto Baudrate activity */
void Chip_UART_ABCmd(LPC_USART_T *pUART, uint32_t mode, bool autorestart, FunctionalState NewState)
{
    uint32_t tmp = 0;

	if (NewState == ENABLE) {
		/* Clear DLL and DLM value */
		pUART->LCR |= UART_LCR_DLAB_EN;
		pUART->DLL = 0;
		pUART->DLM = 0;
		pUART->LCR &= ~UART_LCR_DLAB_EN;

		/* FDR value must be reset to default value */
		pUART->FDR = 0x10;

		if (mode == UART_ACR_MODE1) {
			tmp = UART_ACR_START | UART_ACR_MODE;
		}
		else {
			tmp = UART_ACR_START;
		}

		if (autorestart == true) {
			tmp |= UART_ACR_AUTO_RESTART;
		}
		pUART->ACR = tmp;
	}
	else {
		pUART->ACR = 0;
	}
}

