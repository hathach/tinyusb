/* The EVT's core_riscv.h includes "CH56xSFR.H" (uppercase extension), which fails on
 * case-sensitive filesystems: forward to the real header. Use an explicit relative path to the
 * SDK copy (not <CH56xSFR.h>): on a case-insensitive filesystem the header search would fold
 * back onto this file, the guard below would swallow it and every register define would vanish. */
#ifndef BSP_CH56XSFR_SHIM_H_
#define BSP_CH56XSFR_SHIM_H_
#include "../../mcu/wch/ch569/EVT/EXAM/SRC/Peripheral/inc/CH56xSFR.h"
#endif
