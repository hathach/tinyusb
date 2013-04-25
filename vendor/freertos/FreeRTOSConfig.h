#ifndef __FREERTOS_CONFIG__H
#define __FREERTOS_CONFIG__H

#ifdef CORE_M4
#include "lpc43xx_m4_FreeRTOSConfig.h"
#elif defined(CORE_M0)
	#include "lpc43xx_m0_FreeRTOSConfig.h"
#else
	#error "For LPC43XX one of CORE_M0 or CORE_M4 must be defined!"
#endif /* ifdef CORE_M4 */

# endif /* __FREERTOS_CONFIG__H */
