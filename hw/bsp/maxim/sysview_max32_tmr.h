/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// SEGGER_SYSVIEW timestamp for the MAX32 parts, whose Cortex-M4 DWT has NO cycle counter:
// DWT_CTRL.NOCYCCNT=1 (read live on max32666fthr, 2026-08-11 -- Maxim implemented only the
// watchpoint comparators), so SystemView's ARMv7-M default of reading 0xE0001004 returns a
// frozen 0 and every duration decodes as zero. family.cmake therefore builds SystemView with
// SEGGER_SYSVIEW_CORE=SEGGER_SYSVIEW_CORE_OTHER, routing timestamps to the hooks below.
//
// Source: a TMR in Continuous mode at prescaler 1 (f_PCLK = f_SYS_CLK/2 = 48 MHz, UG6971
// Eq. 15-1). The 1 MHz "microseconds" contract the M0/RISC-V families follow is unreachable
// here -- the prescaler is powers-of-two only -- so the frequency is reported alongside via
// SEGGER_SYSVIEW_X_GetTimestampFreq() (see CFG_TUSB_SYSVIEW_TIMESTAMP_BSP in tusb_sysview.c).
// UG6971 15.2 guarantees TMRn_CNT "is always readable, even while the timer is enabled and
// counting". With CMP=0xFFFFFFFF the hardware wrap reloads CNT to 0x0000_0001, not 0
// (UG6971 15.5.1): modulus 2^32-1, i.e. one 20.8 ns slip per ~89.5 s wrap -- ~0.23 ppb, far
// below crystal tolerance.
//
// The instance is SYSVIEW_MAX32_TMR (default TMR0, present on every MAX32 part). TinyUSB
// examples use no TMR at all and the FreeRTOS builds tick on SysTick (lib/FreeRTOS-Kernel's
// ARM_CM4F port -- MSDK's TMR-based tickless demos are never compiled here), so any instance
// is free; override with -DSYSVIEW_MAX32_TMR=<0..5> if an application claims TMR0.
//
// Include once from family.c inside its `#if CFG_TUD_SYSVIEW || CFG_TUH_SYSVIEW` guard:
// SEGGER_SYSVIEW_X_GetTimestamp*() below are non-static linker-visible hooks (SEGGER calls
// them by name), so a second include in the same TU is a redefinition error.
#ifndef TUSB_BSP_SYSVIEW_MAX32_TMR_H_
#define TUSB_BSP_SYSVIEW_MAX32_TMR_H_

#include "tmr_regs.h" // mxc_device.h pulls in no peripheral register maps; these two are
#include "gcr_regs.h" // on the include path from the family's CMSIS/Device Include dir

#ifndef SYSVIEW_MAX32_TMR
#define SYSVIEW_MAX32_TMR 0
#endif

static inline void sysview_max32_tmr_start(void) {
  mxc_tmr_regs_t* tmr = MXC_TMR_GET_TMR(SYSVIEW_MAX32_TMR);
  // PERCKCN0 timer disable bits are contiguous from TIMER0D (gcr_regs.h); reset default is
  // 0 (enabled) -- cleared defensively in case a bootloader or app gated it.
  MXC_GCR->perckcn0 &= ~(MXC_F_GCR_PERCKCN0_TIMER0D << SYSVIEW_MAX32_TMR);
  tmr->cn  = 0;                                // disable while configuring; prescaler bits stay 0 = /1
  tmr->cmp = 0xFFFFFFFFu;                      // free-run over the full range
  tmr->cnt = 1u;                               // the value hardware reloads at every wrap
  tmr->cn  = MXC_S_TMR_CN_TMODE_CONTINUOUS | MXC_F_TMR_CN_TEN;
}

uint32_t SEGGER_SYSVIEW_X_GetTimestamp(void) {
  return MXC_TMR_GET_TMR(SYSVIEW_MAX32_TMR)->cnt; // free-running PCLK (48 MHz) ticks
}

uint32_t SEGGER_SYSVIEW_X_GetTimestampFreq(void) {
  return PeripheralClock; // system_max32665.h: SystemCoreClock / 2 -- tracks reclocking
}

#endif /* TUSB_BSP_SYSVIEW_MAX32_TMR_H_ */
