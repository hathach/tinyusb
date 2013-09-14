/*
 * @brief LWIP debug re-direction
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
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

#include "lwip/opt.h"

/** @defgroup NET_LWIP_DEBUG LWIP debug re-direction
 * @ingroup NET_LWIP
 * Support functions for debug output for LWIP
 * @{
 */

#ifdef LWIP_DEBUG

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

/* Displays an error message on assertion */
void assert_printf(char *msg, int line, char *file)
{
	if (msg) {
		LWIP_DEBUGF(LWIP_DBG_ON, ("%s:%d in file %s\n", msg, line, file));
	}
	while (1) {}
}

#else
/* LWIP optimized assertion loop (no LWIP_DEBUG) */
void assert_loop(void)
{
	while (1) {}
}

 #endif /* LWIP_DEBUG */

/**
 * @}
 */
