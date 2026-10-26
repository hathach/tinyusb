// OPT_OS_CUSTOM for the PVS-Studio compile-only configurations: osal_none's single-context
// primitives plus the millisecond clock a scheduler port provides.
#ifndef TUSB_OS_CUSTOM_H_
#define TUSB_OS_CUSTOM_H_

#include "osal/osal_none.h"

uint32_t osal_time_millis(void);

#endif
