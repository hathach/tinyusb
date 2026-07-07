/* The EVT's core_riscv.h includes "CH56xSFR.H" (uppercase extension), which fails on
 * case-sensitive filesystems: forward to the real header. */
#include "CH56xSFR.h"
