/*
 * @brief LPC17xx/40xx GPIO driver
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

#ifndef __GPIOINT_17XX_40XX_H_
#define __GPIOINT_17XX_40XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup GPIOINT_17XX_40XX CHIP: LPC17xx/40xx GPIO Interrupt driver
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

/**
 * @brief GPIO Interupt registers for Portn
 */
typedef struct {
	__I  uint32_t STATR;		/*!< GPIO Interrupt Status Register for Rising edge */
	__I  uint32_t STATF;		/*!< GPIO Interrupt Status Register for Falling edge */
	__O  uint32_t CLR;			/*!< GPIO Interrupt Clear  Register */
	__IO uint32_t ENR;			/*!< GPIO Interrupt Enable Register 0 for Rising edge */
	__IO uint32_t ENF;			/*!< GPIO Interrupt Enable Register 0 for Falling edge */
} GPIOINT_PORT_T;

/**
 * @brief GPIO Interrupt register block structure
 */
typedef struct {
	__I  uint32_t STATUS;		/*!< GPIO overall Interrupt Status Register */
	GPIOINT_PORT_T IO0;			/*!< GPIO Interrupt Registers for Port 0 */
	uint32_t RESERVED0[3];
	GPIOINT_PORT_T IO2;			/*!< GPIO Interrupt Registers for Port 2 */
} LPC_GPIOINT_T;

/**
 * @brief	GPIO interrupt capable ports
 */
typedef enum {
	GPIOINT_PORT0,             /*!< GPIO PORT 0 */
	GPIOINT_PORT2 = 2          /*!< GPIO PORT 2 */
}LPC_GPIOINT_PORT_T;

/**
 * @brief	Initialize GPIO interrupt block
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @return	Nothing
 * @note	This function enables the clock to IOCON, GPIO and GPIOINT
 * 			peripheral blocks.
 */
STATIC INLINE void Chip_GPIOINT_Init(LPC_GPIOINT_T *pGPIOINT)
{
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_GPIO);
}

/**
 * @brief	De-Initialize GPIO Interrupt block
 * @param	pGPIOINT	: The base of GPIO interrupt peripheral on the chip
 * @return	Nothing
 * @note	This function disables the clock to IOCON, GPIO and GPIOINT
 * 			peripheral blocks This function should not be called
 * 			if IOCON or GPIO needs to be used after calling this function.
 */
STATIC INLINE void Chip_GPIOINT_DeInit(LPC_GPIOINT_T *pGPIOINT)
{
	Chip_Clock_DisablePeriphClock(SYSCTL_CLOCK_GPIO);
}

/**
 * @brief	Enable interrupts on falling edge of given @a pins
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @param	port		: GPIOINT port (GPIOINT_PORT0 or GPIOINT_PORT2)
 * @param	pins		: Pins set to 1 will have falling edge interrupt enabled,
 * 						  Pins set to 0 will have falling edge interrupt disabled
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIOINT_SetIntFalling(LPC_GPIOINT_T *pGPIOINT, LPC_GPIOINT_PORT_T port, uint32_t pins)
{
	if (port == GPIOINT_PORT0) {
		pGPIOINT->IO0.ENF = pins;
	} else {
		pGPIOINT->IO2.ENF = pins;
	}
}

/**
 * @brief	Enable interrupts on rising edge of given @a pins
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @param	port		: GPIOINT port (GPIOINT_PORT0 or GPIOINT_PORT2)
 * @param	pins		: Pins set to 1 will have rising edge interrupt enabled,
 * 						  Pins set to 0 will have rising edge interrupt disabled
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIOINT_SetIntRising(LPC_GPIOINT_T *pGPIOINT, LPC_GPIOINT_PORT_T port, uint32_t pins)
{
	if (port == GPIOINT_PORT0) {
		pGPIOINT->IO0.ENR = pins;
	} else {
		pGPIOINT->IO2.ENR = pins;
	}
}

/**
 * @brief	Get the pins that has falling edge interrupt enabled
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @param	port		: GPIOINT port (GPIOINT_PORT0 or GPIOINT_PORT2)
 * @return	Pins that are configured for Falling edge interrupt enabled
 */
STATIC INLINE uint32_t Chip_GPIOINT_GetIntFalling(LPC_GPIOINT_T *pGPIOINT, LPC_GPIOINT_PORT_T port)
{
	if (port == GPIOINT_PORT0) {
		return pGPIOINT->IO0.ENF;
	} else {
		return pGPIOINT->IO2.ENF;
	}
}

/**
 * @brief	Get pins that has rising edge interrupt enabled
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @param	port		: GPIOINT port (GPIOINT_PORT0 or GPIOINT_PORT2)
 * @return	Pins that are configured for rising edge interrupt enabled
 */
STATIC INLINE uint32_t Chip_GPIOINT_GetIntRising(LPC_GPIOINT_T *pGPIOINT, LPC_GPIOINT_PORT_T port)
{
	if (port == GPIOINT_PORT0) {
		return pGPIOINT->IO0.ENR;
	} else {
		return pGPIOINT->IO2.ENR;
	}
}

/**
 * @brief	Get status of the pins for falling edge
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @param	port		: GPIOINT port (GPIOINT_PORT0 or GPIOINT_PORT2)
 * @return	Pins that has detected falling edge
 */
STATIC INLINE uint32_t Chip_GPIOINT_GetStatusFalling(LPC_GPIOINT_T *pGPIOINT, LPC_GPIOINT_PORT_T port)
{
	if (port == GPIOINT_PORT0) {
		return pGPIOINT->IO0.STATF;
	} else {
		return pGPIOINT->IO2.STATF;
	}
}

/**
 * @brief	Get status of the pins for rising edge
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @param	port		: GPIOINT port (GPIOINT_PORT0 or GPIOINT_PORT2)
 * @return	Pins that has detected rising edge
 */
STATIC INLINE uint32_t Chip_GPIOINT_GetStatusRising(LPC_GPIOINT_T *pGPIOINT, LPC_GPIOINT_PORT_T port)
{
	if (port == GPIOINT_PORT0) {
		return pGPIOINT->IO0.STATR;
	} else {
		return pGPIOINT->IO2.STATR;
	}
}

/**
 * @brief	Clear the falling and rising edge interrupt for given @a pins
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @param	port		: GPIOINT port (GPIOINT_PORT0 or GPIOINT_PORT2)
 * @param	pins		: Pins to clear the interrupts for
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT_T *pGPIOINT, LPC_GPIOINT_PORT_T port, uint32_t pins)
{
	if (port == GPIOINT_PORT0) {
		pGPIOINT->IO0.CLR = pins;
	} else {
		pGPIOINT->IO2.CLR = pins;
	}
}

/**
 * @brief	Checks if an interrupt is pending on a given port
 * @param	pGPIOINT	: The base address of GPIO interrupt block
 * @param	port		: GPIOINT port (GPIOINT_PORT0 or GPIOINT_PORT2)
 * @return	true if any pin in given port has a pending interrupt
 */
STATIC INLINE bool Chip_GPIOINT_IsIntPending(LPC_GPIOINT_T *pGPIOINT, LPC_GPIOINT_PORT_T port)
{
	return ((pGPIOINT->STATUS & (1 << (int)port)) != 0);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __GPIOINT_17XX_40XX_H_ */
