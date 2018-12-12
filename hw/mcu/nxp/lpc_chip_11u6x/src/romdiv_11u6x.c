/*
 * @brief Routines to overload "/" and "%" operator in C using ROM  based divider library
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

#if !defined( __GNUC__ )

/* Redirector of signed 32 bit integer divider to ROM routine */
int __aeabi_idiv(int numerator, int denominator)
{
	ROM_DIV_API_T const *pROMDiv = LPC_ROM_API->divApiBase;
	return pROMDiv->sidiv(numerator, denominator);
}

/* Redirector of unsigned 32 bit integer divider to ROM routine */
unsigned __aeabi_uidiv(unsigned numerator, unsigned denominator)
{
	ROM_DIV_API_T const *pROMDiv = LPC_ROM_API->divApiBase;
	return pROMDiv->uidiv(numerator, denominator);
}

/* Redirector of signed 32 bit integer divider with reminder to ROM routine */
__value_in_regs IDIV_RETURN_T __aeabi_idivmod(int numerator, int denominator)
{
	ROM_DIV_API_T const *pROMDiv = LPC_ROM_API->divApiBase;
	return pROMDiv->sidivmod(numerator, denominator);
}

/* Redirector of unsigned 32 bit integer divider with reminder to ROM routine */
__value_in_regs UIDIV_RETURN_T __aeabi_uidivmod(unsigned numerator, unsigned denominator)
{
	ROM_DIV_API_T const *pROMDiv = LPC_ROM_API->divApiBase;
	return pROMDiv->uidivmod(numerator, denominator);
}

#endif /* !defined( __GNUC__ ) */
