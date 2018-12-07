/*
 * @brief LPC11u6x Pin Interrupt and Pattern Match Registers and driver
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

#ifndef __PININT_11U6X_H_
#define __PININT_11U6X_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup PININT_11U6X CHIP: LPC11u6x Pin Interrupt and Pattern Match driver
 * @ingroup CHIP_11U6X_Drivers
 * @{
 */

/**
 * @brief LPC11u6x Pin Interrupt and Pattern Match register block structure
 */
typedef struct {			/*!< PIN_INT Structure */
	__IO uint32_t ISEL;		/*!< Pin Interrupt Mode register */
	__IO uint32_t IENR;		/*!< Pin Interrupt Enable (Rising) register */
	__IO uint32_t SIENR;	/*!< Set Pin Interrupt Enable (Rising) register */
	__IO uint32_t CIENR;	/*!< Clear Pin Interrupt Enable (Rising) register */
	__IO uint32_t IENF;		/*!< Pin Interrupt Enable Falling Edge / Active Level register */
	__IO uint32_t SIENF;	/*!< Set Pin Interrupt Enable Falling Edge / Active Level register */
	__IO uint32_t CIENF;	/*!< Clear Pin Interrupt Enable Falling Edge / Active Level address */
	__IO uint32_t RISE;		/*!< Pin Interrupt Rising Edge register */
	__IO uint32_t FALL;		/*!< Pin Interrupt Falling Edge register */
	__IO uint32_t IST;		/*!< Pin Interrupt Status register */
	__IO uint32_t PMCTRL;	/*!< GPIO pattern match interrupt control register          */
	__IO uint32_t PMSRC;	/*!< GPIO pattern match interrupt bit-slice source register */
	__IO uint32_t PMCFG;	/*!< GPIO pattern match interrupt bit slice configuration register */
} LPC_PIN_INT_T;

/**
 * LPC11u6x Pin Interrupt and Pattern match engine register
 * bit fields and macros
 */
/* PININT interrupt control register */
#define PININT_PMCTRL_PMATCH_SEL (1 << 0)
#define PININT_PMCTRL_RXEV_ENA   (1 << 1)

/* PININT Bit slice source register bits */
#define PININT_SRC_BITSOURCE_START  8
#define PININT_SRC_BITSOURCE_MASK   7

/* PININT Bit slice configuration register bits */
#define PININT_SRC_BITCFG_START  8
#define PININT_SRC_BITCFG_MASK   7

/**
 * LPC11u6x Pin Interrupt channel values
 */
#define PININTCH0         (1 << 0)
#define PININTCH1         (1 << 1)
#define PININTCH2         (1 << 2)
#define PININTCH3         (1 << 3)
#define PININTCH4         (1 << 4)
#define PININTCH5         (1 << 5)
#define PININTCH6         (1 << 6)
#define PININTCH7         (1 << 7)
#define PININTCH(ch)      (1 << (ch))

/**
 * LPC11u6x Pin Matching Interrupt bit slice enum values
 */
typedef enum Chip_PININT_BITSLICE {
	PININTBITSLICE0 = 0,	/*!< PININT Bit slice 0 */
	PININTBITSLICE1 = 1,	/*!< PININT Bit slice 1 */
	PININTBITSLICE2 = 2,	/*!< PININT Bit slice 2 */
	PININTBITSLICE3 = 3,	/*!< PININT Bit slice 3 */
	PININTBITSLICE4 = 4,	/*!< PININT Bit slice 4 */
	PININTBITSLICE5 = 5,	/*!< PININT Bit slice 5 */
	PININTBITSLICE6 = 6,	/*!< PININT Bit slice 6 */
	PININTBITSLICE7 = 7	/*!< PININT Bit slice 7 */
} Chip_PININT_BITSLICE_T;

/**
 * LPC11u6x Pin Matching Interrupt bit slice configuration enum values
 */
typedef enum Chip_PININT_BITSLICE_CFG {
	PININT_PATTERNCONST1           = 0x0,	/*!< Contributes to product term match */
	PININT_PATTERNRISING           = 0x1,	/*!< Rising edge */
	PININT_PATTERNFALLING          = 0x2,	/*!< Falling edge */
	PININT_PATTERNRISINGRFALLING   = 0x3,	/*!< Rising or Falling edge */
	PININT_PATTERNHIGH             = 0x4,	/*!< High level */
	PININT_PATTERNLOW              = 0x5,	/*!< Low level */
	PININT_PATTERCONST0            = 0x6,	/*!< Never contributes for match */
	PININT_PATTEREVENT             = 0x7	/*!< Match occurs on event */
} Chip_PININT_BITSLICE_CFG_T;

/**
 * @brief	Initialize Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	Nothing
 * @note	This function should be used after the Chip_GPIO_Init() function.
 */
STATIC INLINE void Chip_PININT_Init(LPC_PIN_INT_T *pPININT) { (void) pPININT; }

/**
 * @brief	De-Initialize Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_DeInit(LPC_PIN_INT_T *pPININT) {  (void) pPININT; }

/**
 * @brief	Configure the pins as edge sensitive in Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pins (ORed value of PININTCH*)
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_SetPinModeEdge(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->ISEL &= ~pins;
}

/**
 * @brief	Configure the pins as level sensitive in Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pins (ORed value of PININTCH*)
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_SetPinModeLevel(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->ISEL |= pins;
}

/**
 * @brief	Return current PININT rising edge or high level interrupt enable state
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	A bifield containing the high edge/level interrupt enables for each
 * interrupt. Bit 0 = PININT0, 1 = PININT1, etc.
 * For each bit, a 0 means the high edge/level interrupt is disabled, while a 1
 * means it's enabled.
 */
STATIC INLINE uint32_t Chip_PININT_GetHighEnabled(LPC_PIN_INT_T *pPININT)
{
	return pPININT->IENR;
}

/**
 * @brief	Enable high edge/level PININT interrupts for pins
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pins to enable (ORed value of PININTCH*)
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_EnableIntHigh(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->SIENR = pins;
}

/**
 * @brief	Disable high edge/level PININT interrupts for pins
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pins to disable (ORed value of PININTCH*)
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_DisableIntHigh(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->CIENR = pins;
}

/**
 * @brief	Return current PININT falling edge or low level interrupt enable state
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	A bifield containing the low edge/level interrupt enables for each
 * interrupt. Bit 0 = PININT0, 1 = PININT1, etc.
 * For each bit, a 0 means the low edge/level interrupt is disabled, while a 1
 * means it's enabled.
 */
STATIC INLINE uint32_t Chip_PININT_GetLowEnabled(LPC_PIN_INT_T *pPININT)
{
	return pPININT->IENF;
}

/**
 * @brief	Enable low edge/level PININT interrupts for pins
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pins to enable (ORed value of PININTCH*)
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_EnableIntLow(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->SIENF = pins;
}

/**
 * @brief	Disable low edge/level PININT interrupts for pins
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pins to disable (ORed value of PININTCH*)
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_DisableIntLow(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->CIENF = pins;
}

/**
 * @brief	Return pin states that have a detected latched high edge (RISE) state
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	PININT states (bit n = high) with a latched rise state detected
 */
STATIC INLINE uint32_t Chip_PININT_GetRiseStates(LPC_PIN_INT_T *pPININT)
{
	return pPININT->RISE;
}

/**
 * @brief	Clears pin states that had a latched high edge (RISE) state
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pins with latched states to clear
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_ClearRiseStates(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->RISE = pins;
}

/**
 * @brief	Return pin states that have a detected latched falling edge (FALL) state
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	PININT states (bit n = high) with a latched rise state detected
 */
STATIC INLINE uint32_t Chip_PININT_GetFallStates(LPC_PIN_INT_T *pPININT)
{
	return pPININT->FALL;
}

/**
 * @brief	Clears pin states that had a latched falling edge (FALL) state
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pins with latched states to clear
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_ClearFallStates(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->FALL = pins;
}

/**
 * @brief	Get interrupt status from Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	Interrupt status (bit n for PININTn = high means interrupt ie pending)
 */
STATIC INLINE uint32_t Chip_PININT_GetIntStatus(LPC_PIN_INT_T *pPININT)
{
	return pPININT->IST;
}

/**
 * @brief	Clear interrupt status in Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	pins	: Pin interrupts to clear (ORed value of PININTCH*)
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_ClearIntStatus(LPC_PIN_INT_T *pPININT, uint32_t pins)
{
	pPININT->IST = pins;
}

/**
 * @brief	Set source for pattern match in Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @param	chan	: PININT channel number (From 0 to 7)
 * @param	slice	: PININT slice number
 * @return	Nothing
 */
void Chip_PININT_SetPatternMatchSrc(LPC_PIN_INT_T *pPININT, uint8_t chan, Chip_PININT_BITSLICE_T slice);

/**
 * @brief	Configure the pattern matcch in Pin interrupt block
 * @param	pPININT		: The base address of Pin interrupt block
 * @param	slice		: PININT slice number
 * @param	slice_cfg	: PININT slice configuration value (enum Chip_PININT_BITSLICE_CFG_T)
 * @param	end_point	: If true, current slice is final component
 * @return	Nothing
 */
void Chip_PININT_SetPatternMatchConfig(LPC_PIN_INT_T *pPININT, Chip_PININT_BITSLICE_T slice,
									   Chip_PININT_BITSLICE_CFG_T slice_cfg, bool end_point);

/**
 * @brief	Enable pattern match interrupts in Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_EnablePatternMatch(LPC_PIN_INT_T *pPININT)
{
	pPININT->PMCTRL |= PININT_PMCTRL_PMATCH_SEL;
}

/**
 * @brief	Disable pattern match interrupts in Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_DisablePatternMatch(LPC_PIN_INT_T *pPININT)
{
	pPININT->PMCTRL &= ~PININT_PMCTRL_PMATCH_SEL;
}

/**
 * @brief	Enable RXEV output in Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_EnablePatternMatchRxEv(LPC_PIN_INT_T *pPININT)
{
	pPININT->PMCTRL |= PININT_PMCTRL_RXEV_ENA;
}

/**
 * @brief	Disable RXEV output in Pin interrupt block
 * @param	pPININT	: The base address of Pin interrupt block
 * @return	Nothing
 */
STATIC INLINE void Chip_PININT_DisablePatternMatchRxEv(LPC_PIN_INT_T *pPININT)
{
	pPININT->PMCTRL &= ~PININT_PMCTRL_RXEV_ENA;
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __PININT_11U6X_H_ */
