/*
 * @brief LPC17xx/40xx IOCON registers and control functions
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

#ifndef __IOCON_17XX_40XX_H_
#define __IOCON_17XX_40XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup IOCON_17XX_40XX CHIP: LPC17xx/40xx I/O configuration driver
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

/** 
 * @brief Array of IOCON pin definitions passed to Chip_IOCON_SetPinMuxing() must be in this format 
 */ 
typedef struct {
	uint32_t pingrp:3;		/* Pin group */
	uint32_t pinnum:5;		/* Pin number */
	uint32_t modefunc:24;	/* Function and mode. */
} PINMUX_GRP_T;

/**
 * @brief IOCON register block
 */
typedef struct {
#if defined(CHIP_LPC175X_6X)
	__IO uint32_t PINSEL[11];
	uint32_t RESERVED0[5];
	__IO uint32_t PINMODE[10];
	__IO uint32_t PINMODE_OD[5];
	__IO uint32_t I2CPADCFG;
#else
	__IO uint32_t p[5][32];
#endif
} LPC_IOCON_T;

/**
 * IOCON function and mode selection definitions
 * See the User Manual for specific modes and functions supoprted by the
 * various LPC11xx devices. Functionality can vary per device.
 */
#define IOCON_FUNC0             0x0				/*!< Selects pin function 0 */
#define IOCON_FUNC1             0x1				/*!< Selects pin function 1 */
#define IOCON_FUNC2             0x2				/*!< Selects pin function 2 */
#define IOCON_FUNC3             0x3				/*!< Selects pin function 3 */
#if defined(CHIP_LPC175X_6X)
#define IOCON_MODE_INACT        (0x2 << 2)		/*!< No addition pin function */
#define IOCON_MODE_PULLDOWN     (0x3 << 2)		/*!< Selects pull-down function */
#define IOCON_MODE_PULLUP       (0x0 << 2)		/*!< Selects pull-up function */
#define IOCON_MODE_REPEATER     (0x1 << 2)		/*!< Selects pin repeater function */
#else
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
#define IOCON_FILT_DIS          (0x1 << 8)		/*!< Disables noise pulses filtering (10nS glitch filter) */
#define IOCON_HS_DIS            (0x1 << 8)		/*!< I2C glitch filter and slew rate disabled */
#define IOCON_HIDRIVE_EN        (0x1 << 9)		/*!< Sink current is 20 mA */
#define IOCON_FASTSLEW_EN       (0x1 << 9)		/*!< Enables fast slew */
#define IOCON_OPENDRAIN_EN      (0x1 << 10)		/*!< Enables open-drain function */
#define IOCON_DAC_EN            (0x1 << 16)		/*!< Enables DAC function */
#endif

/**
 * IOCON function and mode selection definitions (old)
 * For backwards compatibility.
 */
#define FUNC0					0x0				/** Function 0  */
#define FUNC1					0x1				/** Function 1  */
#define FUNC2					0x2				/** Function 2	*/
#define FUNC3					0x3				/** Function 3	*/
#if defined(CHIP_LPC175X_6X)
#define MD_PLN					(0x2)
#define MD_PDN					(0x3)
#define MD_PUP					(0x0)
#define MD_BUK					(0x1)

#else
#define MD_PLN					(0x0 << 3)
#define MD_PDN					(0x1 << 3)
#define MD_PUP					(0x2 << 3)
#define MD_BUK					(0x3 << 3)
#define MD_HYS_ENA				(0x1 << 5)		/*!< Macro to enable hysteresis- use with Chip_IOCON_PinMux */
#define MD_HYS_DIS				(0x0 << 5)		/*!< Macro to disable hysteresis- use with Chip_IOCON_PinMux */
#define MD_IINV_ENA				(0x1 << 6)		/*!< Macro to enable input inversion- use with Chip_IOCON_PinMux */
#define MD_IINV_DIS				(0x0 << 6)		/*!< Macro to disable input inversion- use with Chip_IOCON_PinMux */
#define MD_OD_ENA				(0x1 << 10)		/*!< Macro to enable simulated open drain mode- use with Chip_IOCON_PinMux */
#define MD_OD_DIS				(0x0 << 10)		/*!< Macro to disable simulated open drain mode- use with Chip_IOCON_PinMux */
#define MD_HS_ENA				(0x0 << 8)		/*!< Macro to enable I2C 50ns glitch filter and slew rate control- use with Chip_IOCON_PinMux */
#define MD_HS_DIS				(0x1 << 8)		/*!< Macro to disable I2C 50ns glitch filter and slew rate control- use with Chip_IOCON_PinMux */
#define MD_ANA_ENA				(0x0 << 7)		/*!< Macro to enable analog mode (ADC)- use with Chip_IOCON_PinMux */
#define MD_ANA_DIS				(0x1 << 7)		/*!< Macro to disable analog mode (ADC)- use with Chip_IOCON_PinMux */
#define MD_FILT_ENA				(0x0 << 8)		/*!< Macro to enable input filter- use with Chip_IOCON_PinMux */
#define MD_FILT_DIS				(0x1 << 8)		/*!< Macro to disable input filter- use with Chip_IOCON_PinMux */
#define MD_DAC_ENA				(0x1 << 16)		/*!< Macro to enable DAC- use with Chip_IOCON_PinMux */
#define MD_DAC_DIS				(0x0 << 16)		/*!< Macro to disable DAC- use with Chip_IOCON_PinMux */
#define MD_STD_SLEW_RATE		(0x0 << 9)		/*!< Macro to enable standard mode, slew rate control is enabled - use with Chip_IOCON_PinMux */
#define MD_FAST_SLEW_RATE		(0x1 << 9)		/*!< Macro to enable fast mode, slew rate control is disabled - use with Chip_IOCON_PinMux */
#define MD_HD_ENA				(0x1 << 9)		/*!< Macro to enable high drive output- use with Chip_IOCON_PinMux */
#define MD_HD_DIS				(0x0 << 9)		/*!< Macro to disable high drive output- use with Chip_IOCON_PinMux */
#define FUNC4					0x4				/** Function 4  */
#define FUNC5					0x5				/** Function 5  */
#define FUNC6					0x6				/** Function 6	*/
#define FUNC7					0x7				/** Function 7	*/
#endif /* defined(CHIP_LPC175X_6X)*/

/**
 * @brief	Initialize the IOCON peripheral
 * @param	pIOCON	: The base of IOCON peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_IOCON_Init(LPC_IOCON_T *pIOCON)
{
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_GPIO);
}

#if defined(CHIP_LPC175X_6X)
/* PINSEL and PINMODE register index calculation.*/
#define IOCON_REG_INDEX(port, pin)		(2 * port + (pin / 16))
/* Bit position calculation in PINSEL and PINMODE register.*/
#define IOCON_BIT_INDEX(pin)			((pin % 16) * 2)

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
 * @brief	Setup pin modes and function
 * @param	pIOCON		: The base of IOCON peripheral on the chip
 * @param	port 		: port number
 * @param	pin			: gpio pin number
 * @param	mode		: OR'ed values or type IOCON_*
 * @param	func		: Pin function, value of type IOCON_FUNC0 to IOCON_FUNC3
 * @return	Nothing
 */
void Chip_IOCON_PinMux(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin, uint32_t mode, uint8_t func);

/**
 * @brief	Enable open drain mode
 * @param	pIOCON		: The base of IOCON peripheral on the chip
 * @param	port 		: port number
 * @param	pin			: gpio pin number
 * @return	Nothing
 */
STATIC INLINE void Chip_IOCON_EnableOD(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin)
{
	pIOCON->PINMODE_OD[port] |= (0x01UL << pin);
}

/**
 * @brief	Disable open drain mode
 * @param	pIOCON		: The base of IOCON peripheral on the chip
 * @param	port 		: port number
 * @param	pin			: gpio pin number
 * @return	Nothing
 */
STATIC INLINE void Chip_IOCON_DisableOD(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin)
{
	pIOCON->PINMODE_OD[port] &= ~(0x01UL << pin);
}

/**
 * @brief I2C pin configuration definitions 
 */
typedef enum {
	I2CPADCFG_STD_MODE = 0x00,                  /*!< Standard I2C mode */
	I2CPADCFG_FAST_MODE = I2CPADCFG_STD_MODE,   /*!< Fast mode */
	I2CPADCFG_FAST_MODE_PLUS = 0x05,            /*!< Fast mode plus */
	I2CPADCFG_NON_I2C = 0x0A,                  /*!< For non-I2C use*/
} IOCON_I2CPINS_CONFIG;

/**
 * @brief Configure I2C pad pins (P0.27 and P0.28)
 * @param	pIOCON		: The base of IOCON peripheral on the chip
 * @param	cfg 		: pin configurations
 * @return	Nothing
 */
STATIC INLINE void Chip_IOCON_SetI2CPad(LPC_IOCON_T *pIOCON, IOCON_I2CPINS_CONFIG cfg)
{
	pIOCON->I2CPADCFG = cfg;
}

#else
/**
 * @brief	Sets I/O Control pin mux
 * @param	pIOCON		: The base of IOCON peripheral on the chip
 * @param	port		: GPIO port to mux
 * @param	pin			: GPIO pin to mux
 * @param	modefunc	: OR'ed values or type IOCON_*
 * @return	Nothing
 */
STATIC INLINE void Chip_IOCON_PinMuxSet(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin, uint32_t modefunc)
{
	pIOCON->p[port][pin] = modefunc;
}

/**
 * @brief	Setup pin modes and function
 * @param	pIOCON		: The base of IOCON peripheral on the chip
 * @param	port 		: port number
 * @param	pin			: gpio pin number
 * @param	mode		: OR'ed values or type IOCON_*
 * @param	func		: Pin function, value of type IOCON_FUNC0 to IOCON_FUNC7
 * @return	Nothing
 */
STATIC INLINE void Chip_IOCON_PinMux(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin, uint32_t mode, uint8_t func)
{
	Chip_IOCON_PinMuxSet(pIOCON, port, pin, (mode | func));
}
#endif /* defined(CHIP_LPC175X_6X) */

/**
 * @brief	Set all I/O Control pin muxing
 * @param	pIOCON	    : The base of IOCON peripheral on the chip
 * @param	pinArray    : Pointer to array of pin mux selections
 * @param	arrayLength : Number of entries in pinArray
 * @return	Nothing
 */
void Chip_IOCON_SetPinMuxing(LPC_IOCON_T *pIOCON, const PINMUX_GRP_T* pinArray, uint32_t arrayLength);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __IOCON_17XX_40XX_H_ */
