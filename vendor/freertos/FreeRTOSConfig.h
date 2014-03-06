#ifndef __FREERTOS_CONFIG__H
#define __FREERTOS_CONFIG__H

#include "hal/hal.h"

#if __CORTEX_M == 4
  #include "FreeRTOSConfig_cm4f.h"

#elif __CORTEX_M == 3
  #include "FreeRTOSConfig_cm3.h"

#elif __CORTEX_M == 0
	#include "FreeRTOSConfig_cm0.h"

#else
	#error "not yet supported MCU"
#endif

#endif /* __FREERTOS_CONFIG__H */
