#ifndef __FREERTOS_CONFIG__H
#define __FREERTOS_CONFIG__H

#include "hal/hal.h"

#if __CORTEX_M == 4
#include "lpc43xx_m4_FreeRTOSConfig.h"
#elif __CORTEX_M == 0
	#include "lpc43xx_m0_FreeRTOSConfig.h"
#elif __CORTEX_M == 3
  #include "FreeRTOSConfig_lpc175x_6x.h"
#else
	#error "not yet supported MCU"
#endif /* ifdef CORE_M4 */

# endif /* __FREERTOS_CONFIG__H */
