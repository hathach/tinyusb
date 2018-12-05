/*
 * @brief LPC11u6xx USART1/2/3/4 driver
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

#ifndef __UART_N_11U6X_H_
#define __UART_N_11U6X_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ring_buffer.h"

/** @defgroup UART_11U6X_2 CHIP: LPC11u6x USART N Driver (UARTS 1/2/3/4)
 * @ingroup CHIP_11U6X_Drivers
 * This driver only works with USARTs 1-4. Do not mix UART0_* and UARTN_*
 * macro definitions across UUSART 0 and USART 1-4 drivers.
 * @{
 */

/**
 * @brief UART register block structure
 */
typedef struct {
	__IO uint32_t  CFG;				/*!< Configuration register */
	__IO uint32_t  CTRL;			/*!< Control register */
	__IO uint32_t  STAT;			/*!< Status register */
	__IO uint32_t  INTENSET;		/*!< Interrupt Enable read and set register */
	__O  uint32_t  INTENCLR;		/*!< Interrupt Enable clear register */
	__I  uint32_t  RXDATA;			/*!< Receive Data register */
	__I  uint32_t  RXDATA_STAT;		/*!< Receive Data with status register */
	__IO uint32_t  TXDATA;			/*!< Transmit data register */
	__IO uint32_t  BRG;				/*!< Baud Rate Generator register */
	__IO uint32_t  INTSTAT;			/*!< Interrupt status register */
	__IO uint32_t  OSR;				/*!< Oversample selection register for asynchronous communication */
	__IO uint32_t  ADDR;			/*!< Address register for automatic address matching */
} LPC_USARTN_T;

/**
 * @brief UART CFG register definitions
 */
#define UARTN_CFG_ENABLE         (0x01 << 0)
#define UARTN_CFG_DATALEN_7      (0x00 << 2)	/*!< UART 7 bit length mode */
#define UARTN_CFG_DATALEN_8      (0x01 << 2)	/*!< UART 8 bit length mode */
#define UARTN_CFG_DATALEN_9      (0x02 << 2)	/*!< UART 9 bit length mode */
#define UARTN_CFG_PARITY_NONE    (0x00 << 4)	/*!< No parity */
#define UARTN_CFG_PARITY_EVEN    (0x02 << 4)	/*!< Even parity */
#define UARTN_CFG_PARITY_ODD     (0x03 << 4)	/*!< Odd parity */
#define UARTN_CFG_STOPLEN_1      (0x00 << 6)	/*!< UART One Stop Bit Select */
#define UARTN_CFG_STOPLEN_2      (0x01 << 6)	/*!< UART Two Stop Bits Select */
#define UARTN_MODE_32K           (0x01 << 7)	/*!< Selects the 32 kHz clock from the RTC oscillator as the clock source to the BRG */
#define UARTN_CFG_CTSEN          (0x01 << 9)	/*!< CTS enable bit */
#define UARTN_CFG_SYNCEN         (0x01 << 11)	/*!< Synchronous mode enable bit */
#define UARTN_CFG_CLKPOL         (0x01 << 12)	/*!< Un_RXD rising edge sample enable bit */
#define UARTN_CFG_SYNCMST        (0x01 << 14)	/*!< Select master mode (synchronous mode) enable bit */
#define UARTN_CFG_LOOP           (0x01 << 15)	/*!< Loopback mode enable bit */

/**
 * @brief UART CTRL register definitions
 */
#define UARTN_CTRL_TXBRKEN       (0x01 << 1)		/*!< Continuous break enable bit */
#define UARTN_CTRL_ADDRDET       (0x01 << 2)		/*!< Address detect mode enable bit */
#define UARTN_CTRL_TXDIS         (0x01 << 6)		/*!< Transmit disable bit */
#define UARTN_CTRL_CC            (0x01 << 8)		/*!< Continuous Clock mode enable bit */
#define UARTN_CTRL_CLRCC         (0x01 << 9)		/*!< Clear Continuous Clock bit */

/**
 * @brief UART STAT register definitions
 */
#define UARTN_STAT_RXRDY         (0x01 << 0)			/*!< Receiver ready */
#define UARTN_STAT_RXIDLE        (0x01 << 1)			/*!< Receiver idle */
#define UARTN_STAT_TXRDY         (0x01 << 2)			/*!< Transmitter ready for data */
#define UARTN_STAT_TXIDLE        (0x01 << 3)			/*!< Transmitter idle */
#define UARTN_STAT_CTS           (0x01 << 4)			/*!< Status of CTS signal */
#define UARTN_STAT_DELTACTS      (0x01 << 5)			/*!< Change in CTS state */
#define UARTN_STAT_TXDISINT      (0x01 << 6)			/*!< Transmitter disabled */
#define UARTN_STAT_OVERRUNINT    (0x01 << 8)			/*!< Overrun Error interrupt flag. */
#define UARTN_STAT_RXBRK         (0x01 << 10)		/*!< Received break */
#define UARTN_STAT_DELTARXBRK    (0x01 << 11)		/*!< Change in receive break detection */
#define UARTN_STAT_START         (0x01 << 12)		/*!< Start detected */
#define UARTN_STAT_FRM_ERRINT    (0x01 << 13)		/*!< Framing Error interrupt flag */
#define UARTN_STAT_PAR_ERRINT    (0x01 << 14)		/*!< Parity Error interrupt flag */
#define UARTN_STAT_RXNOISEINT    (0x01 << 15)		/*!< Received Noise interrupt flag */

/**
 * @brief UART INTENSET/INTENCLR register definitions
 */
#define UARTN_INTEN_RXRDY        (0x01 << 0)			/*!< Receive Ready interrupt */
#define UARTN_INTEN_TXRDY        (0x01 << 2)			/*!< Transmit Ready interrupt */
#define UARTN_INTEN_DELTACTS     (0x01 << 5)			/*!< Change in CTS state interrupt */
#define UARTN_INTEN_TXDIS        (0x01 << 6)			/*!< Transmitter disable interrupt */
#define UARTN_INTEN_OVERRUN      (0x01 << 8)			/*!< Overrun error interrupt */
#define UARTN_INTEN_DELTARXBRK   (0x01 << 11)		/*!< Change in receiver break detection interrupt */
#define UARTN_INTEN_START        (0x01 << 12)		/*!< Start detect interrupt */
#define UARTN_INTEN_FRAMERR      (0x01 << 13)		/*!< Frame error interrupt */
#define UARTN_INTEN_PARITYERR    (0x01 << 14)		/*!< Parity error interrupt */
#define UARTN_INTEN_RXNOISE      (0x01 << 15)		/*!< Received noise interrupt */

/**
 * @brief	Enable the UART
 * @param	pUART		: Pointer to selected UARTx peripheral
 * @return	Nothing
 */
STATIC INLINE void Chip_UARTN_Enable(LPC_USARTN_T *pUART)
{
	pUART->CFG |= UARTN_CFG_ENABLE;
}

/**
 * @brief	Disable the UART
 * @param	pUART	: Pointer to selected UARTx peripheral
 * @return	Nothing
 */
STATIC INLINE void Chip_UARTN_Disable(LPC_USARTN_T *pUART)
{
	pUART->CFG &= ~UARTN_CFG_ENABLE;
}

/**
 * @brief	Enable transmission on UART TxD pin
 * @param	pUART	: Pointer to selected pUART peripheral
 * @return Nothing
 */
STATIC INLINE void Chip_UARTN_TXEnable(LPC_USARTN_T *pUART)
{
	pUART->CTRL &= ~UARTN_CTRL_TXDIS;
}

/**
 * @brief	Disable transmission on UART TxD pin
 * @param	pUART	: Pointer to selected pUART peripheral
 * @return Nothing
 */
STATIC INLINE void Chip_UARTN_TXDisable(LPC_USARTN_T *pUART)
{
	pUART->CTRL |= UARTN_CTRL_TXDIS;
}

/**
 * @brief	Transmit a single data byte through the UART peripheral
 * @param	pUART	: Pointer to selected UART peripheral
 * @param	data	: Byte to transmit
 * @return	Nothing
 * @note	This function attempts to place a byte into the UART transmit
 *			holding register regard regardless of UART state.
 */
STATIC INLINE void Chip_UARTN_SendByte(LPC_USARTN_T *pUART, uint8_t data)
{
	pUART->TXDATA = (uint32_t) data;
}

/**
 * @brief	Read a single byte data from the UART peripheral
 * @param	pUART	: Pointer to selected UART peripheral
 * @return	A single byte of data read
 * @note	This function reads a byte from the UART receive FIFO or
 *			receive hold register regard regardless of UART state. The
 *			FIFO status should be read first prior to using this function
 */
STATIC INLINE uint32_t Chip_UARTN_ReadByte(LPC_USARTN_T *pUART)
{
	/* Strip off undefined reserved bits, keep 9 lower bits */
	return (uint32_t) (pUART->RXDATA & 0x000001FF);
}

/**
 * @brief	Enable UART interrupts
 * @param	pUART	: Pointer to selected UART peripheral
 * @param	intMask	: OR'ed Interrupts to enable
 * @return	Nothing
 * @note	Use an OR'ed value of UARTN_INTEN_* definitions with this function
 *			to enable specific UART interrupts.
 */
STATIC INLINE void Chip_UARTN_IntEnable(LPC_USARTN_T *pUART, uint32_t intMask)
{
	pUART->INTENSET = intMask;
}

/**
 * @brief	Disable UART interrupts
 * @param	pUART	: Pointer to selected UART peripheral
 * @param	intMask	: OR'ed Interrupts to disable
 * @return	Nothing
 * @note	Use an OR'ed value of UARTN_INTEN_* definitions with this function
 *			to disable specific UART interrupts.
 */
STATIC INLINE void Chip_UARTN_IntDisable(LPC_USARTN_T *pUART, uint32_t intMask)
{
	pUART->INTENCLR = intMask;
}

/**
 * @brief	Returns UART interrupts that are enabled
 * @param	pUART	: Pointer to selected UART peripheral
 * @return	Returns the enabled UART interrupts
 * @note	Use an OR'ed value of UARTN_INTEN_* definitions with this function
 *			to determine which interrupts are enabled. You can check
 *			for multiple enabled bits if needed.
 */
STATIC INLINE uint32_t Chip_UARTN_GetIntsEnabled(LPC_USARTN_T *pUART)
{
	return pUART->INTENSET;
}

/**
 * @brief	Get UART interrupt status
 * @param	pUART	: The base of UART peripheral on the chip
 * @return	The Interrupt status register of UART
 * @note	Multiple interrupts may be pending. Mask the return value
 *			with one or more UARTN_INTEN_* definitions to determine
 *			pending interrupts.
 */
STATIC INLINE uint32_t Chip_UARTN_GetIntStatus(LPC_USARTN_T *pUART)
{
	return pUART->INTSTAT;
}

/**
 * @brief	Configure data width, parity and stop bits
 * @param	pUART	: Pointer to selected pUART peripheral
 * @param	config	: UART configuration, OR'ed values of select UARTN_CFG_* defines
 * @return	Nothing
 * @note	Select OR'ed config options for the UART from the UARTN_CFG_PARITY_*,
 *			UARTN_CFG_STOPLEN_*, and UARTN_CFG_DATALEN_* definitions. For example,
 *			a configuration of 8 data bits, 1 stop bit, and even (enabled) parity would be
 *			(UARTN_CFG_DATALEN_8 | UARTN_CFG_STOPLEN_1 | UARTN_CFG_PARITY_EVEN). Will not
 *			alter other bits in the CFG register.
 */
STATIC INLINE void Chip_UARTN_ConfigData(LPC_USARTN_T *pUART, uint32_t config)
{
	uint32_t reg;

	reg = pUART->CFG & ~((0x3 << 2) | (0x3 << 4) | (0x1 << 6));
	pUART->CFG = reg | config;
}

/**
 * @brief	Get the UART status register
 * @param	pUART	: Pointer to selected UARTx peripheral
 * @return	UART status register
 * @note	Multiple statuses may be pending. Mask the return value
 *			with one or more UARTN_STAT_* definitions to determine
 *			statuses.
 */
STATIC INLINE uint32_t Chip_UARTN_GetStatus(LPC_USARTN_T *pUART)
{
	return pUART->STAT;
}

/**
 * @brief	Clear the UART status register
 * @param	pUART	: Pointer to selected UARTx peripheral
 * @param	stsMask	: OR'ed statuses to disable
 * @return	Nothing
 * @note	Multiple interrupts may be pending. Mask the return value
 *			with one or more UARTN_INTEN_* definitions to determine
 *			pending interrupts.
 */
STATIC INLINE void Chip_UARTN_ClearStatus(LPC_USARTN_T *pUART, uint32_t stsMask)
{
	pUART->STAT = stsMask;
}

/**
 * @brief	Initialize the UART peripheral
 * @param	pUART	: The base of UART peripheral on the chip
 * @return	Nothing
 */
void Chip_UARTN_Init(LPC_USARTN_T *pUART);

/**
 * @brief	Deinitialize the UART peripheral
 * @param	pUART	: The base of UART peripheral on the chip
 * @return	Nothing
 */
void Chip_UARTN_DeInit(LPC_USARTN_T *pUART);

/**
 * @brief	Transmit a byte array through the UART peripheral (non-blocking)
 * @param	pUART		: Pointer to selected UART peripheral
 * @param	data		: Pointer to bytes to transmit
 * @param	numBytes	: Number of bytes to transmit
 * @return	The actual number of bytes placed into the FIFO
 * @note	This function places data into the transmit FIFO until either
 *			all the data is in the FIFO or the FIFO is full. This function
 *			will not block in the FIFO is full. The actual number of bytes
 *			placed into the FIFO is returned. This function ignores errors.
 */
int Chip_UARTN_Send(LPC_USARTN_T *pUART, const void *data, int numBytes);

/**
 * @brief	Read data through the UART peripheral (non-blocking)
 * @param	pUART		: Pointer to selected UART peripheral
 * @param	data		: Pointer to bytes array to fill
 * @param	numBytes	: Size of the passed data array
 * @return	The actual number of bytes read
 * @note	This function reads data from the receive FIFO until either
 *			all the data has been read or the passed buffer is completely full.
 *			This function will not block. This function ignores errors.
 */
int Chip_UARTN_Read(LPC_USARTN_T *pUART, void *data, int numBytes);

/**
 * @brief	Set baud rate for UART
 * @param	pUART	: The base of UART peripheral on the chip
 * @param	baudrate: Baud rate to be set
 * @return	Nothing
 */
void Chip_UARTN_SetBaud(LPC_USARTN_T *pUART, uint32_t baudrate);

/**
 * @brief	Set baud rate for UART using RTC32K oscillator
 * @param	pUART	: The base of UART peripheral on the chip
 * @param	baudrate: Baud rate to be set
 * @return	Nothing
 * @note	Since the baud rate is divided from the 32KHz oscillator,
 *			this function should only be used with baud rates less
 *			than or equal to 9600 baud. Don't expect any accuracy.
 */
void Chip_UARTN_SetBaudWithRTC32K(LPC_USARTN_T *pUART, uint32_t baudrate);

/**
 * @brief	Transmit a byte array through the UART peripheral (blocking)
 * @param	pUART		: Pointer to selected UART peripheral
 * @param	data		: Pointer to data to transmit
 * @param	numBytes	: Number of bytes to transmit
 * @return	The number of bytes transmitted
 * @note	This function will send or place all bytes into the transmit
 *			FIFO. This function will block until the last bytes are in the FIFO.
 */
int Chip_UARTN_SendBlocking(LPC_USARTN_T *pUART, const void *data, int numBytes);

/**
 * @brief	Read data through the UART peripheral (blocking)
 * @param	pUART		: Pointer to selected UART peripheral
 * @param	data		: Pointer to data array to fill
 * @param	numBytes	: Size of the passed data array
 * @return	The size of the dat array
 * @note	This function reads data from the receive FIFO until the passed
 *			buffer is completely full. The function will block until full.
 *			This function ignores errors.
 */
int Chip_UARTN_ReadBlocking(LPC_USARTN_T *pUART, void *data, int numBytes);

/**
 * @brief	UART receive-only interrupt handler for ring buffers
 * @param	pUART	: Pointer to selected UART peripheral
 * @param	pRB		: Pointer to ring buffer structure to use
 * @return	Nothing
 * @note	If ring buffer support is desired for the receive side
 *			of data transfer, the UART interrupt should call this
 *			function for a receive based interrupt status.
 */
void Chip_UARTN_RXIntHandlerRB(LPC_USARTN_T *pUART, RINGBUFF_T *pRB);

/**
 * @brief	UART transmit-only interrupt handler for ring buffers
 * @param	pUART	: Pointer to selected UART peripheral
 * @param	pRB		: Pointer to ring buffer structure to use
 * @return	Nothing
 * @note	If ring buffer support is desired for the transmit side
 *			of data transfer, the UART interrupt should call this
 *			function for a transmit based interrupt status.
 */
void Chip_UARTN_TXIntHandlerRB(LPC_USARTN_T *pUART, RINGBUFF_T *pRB);

/**
 * @brief	Populate a transmit ring buffer and start UART transmit
 * @param	pUART	: Pointer to selected UART peripheral
 * @param	pRB		: Pointer to ring buffer structure to use
 * @param	data	: Pointer to buffer to move to ring buffer
 * @param	count	: Number of bytes to move
 * @return	The number of bytes placed into the ring buffer
 * @note	Will move the data into the TX ring buffer and start the
 *			transfer. If the number of bytes returned is less than the
 *			number of bytes to send, the ring buffer is considered full.
 */
uint32_t Chip_UARTN_SendRB(LPC_USARTN_T *pUART, RINGBUFF_T *pRB, const void *data, int count);

/**
 * @brief	Copy data from a receive ring buffer
 * @param	pUART	: Pointer to selected UART peripheral
 * @param	pRB		: Pointer to ring buffer structure to use
 * @param	data	: Pointer to buffer to fill from ring buffer
 * @param	bytes	: Size of the passed buffer in bytes
 * @return	The number of bytes placed into the ring buffer
 * @note	Will move the data from the RX ring buffer up to the
 *			the maximum passed buffer size. Returns 0 if there is
 *			no data in the ring buffer.
 */
int Chip_UARTN_ReadRB(LPC_USARTN_T *pUART, RINGBUFF_T *pRB, void *data, int bytes);

/**
 * @brief	UART receive/transmit interrupt handler for ring buffers
 * @param	pUART	: Pointer to selected UART peripheral
 * @param	pRXRB	: Pointer to transmit ring buffer
 * @param	pTXRB	: Pointer to receive ring buffer
 * @return	Nothing
 * @note	This provides a basic implementation of the UART IRQ
 *			handler for support of a ring buffer implementation for
 *			transmit and receive.
 */
void Chip_UARTN_IRQRBHandler(LPC_USARTN_T *pUART, RINGBUFF_T *pRXRB, RINGBUFF_T *pTXRB);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __UART_N_11U6X_H_ */
