/**********************************************************************
* $Id$		lpc_debug.c			2011-11-20
*//**
* @file		lpc_debug.c
* @brief	LWIP debug re-direction
* @version	1.0
* @date		20. Nov. 2011
* @author	NXP MCU SW Application Team
* 
* Copyright(C) 2011, NXP Semiconductor
* All rights reserved.
*
***********************************************************************
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
**********************************************************************/

#include "lwip/opt.h"

/** @ingroup lwip_lpc_debug
 * @{
 */

#ifdef LWIP_DEBUG 

/** \brief  Displays an error message on assertion

    This function will display an error message on an assertion
	to the debug output.

	\param[in]    msg   Error message to display
	\param[in]    line  Line number in file with error
	\param[in]    file  Filename with error
 */
void assert_printf(char *msg, int line, char *file)
{
	if (msg) {
		LWIP_DEBUGF(LWIP_DBG_ON, ("%s:%d in file %s\n", msg, line, file));
		while (1) {
			/* Fast LED flash */
			led_set(0);
			msDelay(100);
			led_set(1);
			msDelay(100);
		}
	}
}

#endif /* LWIP_DEBUG */

/**
 * @}
 */

/* --------------------------------- End Of File ------------------------------ */
