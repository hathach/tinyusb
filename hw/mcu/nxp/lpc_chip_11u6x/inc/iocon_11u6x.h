/*
 * @brief LPC11u6x IOCON driver
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

#ifndef __IOCON_11U6X_H_
#define __IOCON_11U6X_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup IOCON_11U6X CHIP: LPC11u6x IO Control driver
 * @ingroup CHIP_11U6X_Drivers
 * @{
 */

/**
 * @brief LPC11u6x IO Configuration Unit register block structure
 */
typedef struct {			/*!< LPC11U6X IOCON Structure */
	__IO uint32_t  PIO0[24];
	__IO uint32_t  PIO1[32];
	__I  uint32_t  reserved[4];
	__IO uint32_t  PIO2A[2];/* PIO2_0/1 only */
	__I  uint32_t  reserved1[1];
	__IO uint32_t  PIO2B[22];	/* PIO2_2 to PIO_2_23 */
} LPC_IOCON_T;

/** 
 * @brief Array of IOCON pin definitions passed to Chip_IOCON_SetPinMuxing() must be in this format 
 */ 
typedef struct {
	uint32_t port : 8;			/* Pin port */
	uint32_t pin : 8;			/* Pin number */
	uint32_t modefunc : 16;		/* Function and mode */
} PINMUX_GRP_T;

/**
 * IOCON function and mode selection definitions
 * See the User Manual for specific modes and functions supported by the
 * various LPC11u6x pins.
 */
#define IOCON_FUNC0             0x0				/*!< Selects pin function 0 */
#define IOCON_FUNC1             0x1				/*!< Selects pin function 1 */
#define IOCON_FUNC2             0x2				/*!< Selects pin function 2 */
#define IOCON_FUNC3             0x3				/*!< Selects pin function 3 */
#define IOCON_FUNC4             0x4				/*!< Selects pin function 4 */
#define IOCON_FUNC5             0x5				/*!< Selects pin function 5 */
#define IOCON_FUNC6             0x6				/*!< Selects pin function 6 */
#define IOCON_FUNC7             0x7				/*!< Selects pin function 7 */
#define IOCON_MODE_INACT        (0x0 << 3)		/*!< No addition pin function */
#define IOCON_MODE_PULLDOWN     (0x1 << 3)		/*!< Selects pull-down function */
#define IOCON_MODE_PULLUP       (0x2 << 3)		/*!< Selects pull-up function */
#define IOCON_MODE_REPEATER     (0x3 << 3)		/*!< Selects pin repeater function */
#define IOCON_HYS_EN            (0x1 << 5)		/*!< Enables hysteresis */
#define IOCON_INV_EN            (0x1 << 6)		/*!< Enables invert function on input */
#define IOCON_ADMODE_EN         (0x0 << 7)		/*!< Enables analog input function (analog pins only) */
#define IOCON_DIGMODE_EN        (0x1 << 7)		/*!< Enables digital function (analog pins only) */
#define IOCON_SFI2C_EN          (0x0 << 8)		/*!< I2C standard mode/fast-mode */
#define IOCON_STDI2C_EN         (0x1 << 8)		/*!< I2C standard I/O functionality */
#define IOCON_FASTI2C_EN        (0x2 << 8)		/*!< I2C Fast-mode Plus */
#define IOCON_FILT_DIS          (0x1 << 8)		/*!< Disables noise pulses filtering (10nS glitch filter) */
#define IOCON_OPENDRAIN_EN      (0x1 << 10)		/*!< Enables open-drain function */
#define IOCON_S_MODE_0CLK       (0x0 << 11)		/*!< Bypass input filter */
#define IOCON_S_MODE_1CLK       (0x1 << 11)		/*!< Input pulses shorter than 1 filter clock are rejected */
#define IOCON_S_MODE_2CLK       (0x2 << 11)		/*!< Input pulses shorter than 2 filter clock2 are rejected */
#define IOCON_S_MODE_3CLK       (0x3 << 11)		/*!< Input pulses shorter than 3 filter clock2 are rejected */
#define IOCON_S_MODE(clks)      ((clks) << 11)	/*!< Select clocks for digital input filter mode */
#define IOCON_CLKDIV(div)       ((div) << 13)	/*!< Select peripheral clock divider for input filter sampling clock, 0 -6 */

/**
 * @brief	Sets I/O Control pin mux
 * @param	pIOCON		: The base of IOCON peripheral on the chip
 * @param	port		: GPIO port to mux
 * @param	pin			: GPIO pin to mux
 * @param	modefunc	: OR'ed values or type IOCON_*
 * @return	Nothing
 */
void Chip_IOCON_PinMuxSet(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin, uint32_t modefunc);

/**
 * @brief	I/O Control pin mux
 * @param	pIOCON	: The base of IOCON peripheral on the chip
 * @param	port	: GPIO port to mux
 * @param	pin		: GPIO pin to mux
 * @param	mode	: OR'ed values or type IOCON_*
 * @param	func	: Pin function, value of type IOCON_FUNC?
 * @return	Nothing
 */
STATIC INLINE void Chip_IOCON_PinMux(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin, uint16_t mode, uint8_t func)
{
	Chip_IOCON_PinMuxSet(pIOCON, port, pin, (uint32_t) (mode | func));
}

/**
 * @brief	Set all I/O Control pin muxing
 * @param	pIOCON	    : The base of IOCON peripheral on the chip
 * @param	pinArray    : Pointer to array of pin mux selections
 * @param	arrayLength : Number of entries in pinArray
 * @return	Nothing
 */
void Chip_IOCON_SetPinMuxing(LPC_IOCON_T *pIOCON, const PINMUX_GRP_T* pinArray, uint32_t arrayLength);

/**
 * @brief	Returns the value of I/O Control pin mux
 * @param	pIOCON		: The base of IOCON peripheral on the chip
 * @param	port		: GPIO port to mux
 * @param	pin			: GPIO pin to mux
 * @return	Value of IOCON register
 */
uint32_t Chip_IOCON_GetPinMux(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __IOCON_11U6X_H_ */
