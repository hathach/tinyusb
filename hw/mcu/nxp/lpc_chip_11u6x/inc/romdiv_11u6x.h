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

#ifndef __ROMDIV_11U6X_H_
#define __ROMDIV_11U6X_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup ROMAPI_DIV_11U6X ROM divider API declarations
 * @ingroup ROMAPI_11U6X
 * The ROM-based 32-bit integer division routines are available for all LPC11U6x.
 * These routines are performance optimized and reduce application code space. As
 * part of chip library these routines overload “/” and “%” operators in C.
 * @{
 */

/**
 * @brief Structure containing signed integer divider return data.
 */
typedef struct {
	int quot;			/*!< Quotient */
	int rem;			/*!< Reminder */
} IDIV_RETURN_T;

/**
 * @brief Structure containing unsigned integer divider return data.
 */
typedef struct {
	unsigned quot;		/*!< Quotient */
	unsigned rem;		/*!< Reminder */
} UIDIV_RETURN_T;

/**
 * @brief ROM divider API Structure.
 */
typedef struct {
	int (*sidiv)(int numerator, int denominator);							/*!< Signed integer division */
	unsigned (*uidiv)(unsigned numerator, unsigned denominator);			/*!< Unsigned integer division */
	IDIV_RETURN_T (*sidivmod)(int numerator, int denominator);				/*!< Signed integer division with remainder */
	UIDIV_RETURN_T (*uidivmod)(unsigned numerator, unsigned denominator);	/*!< Unsigned integer division with remainder */
} ROM_DIV_API_T;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __ROMDIV_11U6X_H_ */
