/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// SEGGER_SYSVIEW timestamp for the MAX32 parts. MAX3266x's Cortex-M4 DWT has NO cycle counter:
// DWT_CTRL.NOCYCCNT=1 (read live on max32666fthr, 2026-08-11 -- Maxim implemented only the
// watchpoint comparators), so SystemView's ARMv7-M default of reading 0xE0001004 returns a
// frozen 0 and every duration decodes as zero. MAX32650 and MAX32690's user guides do not
// document their DWT, so they use a TMR too. family.cmake therefore defines
// SEGGER_SYSVIEW_GET_TIMESTAMP as SEGGER_SYSVIEW_X_GetTimestamp, routing timestamps to the hook below.
//
// Source: a TMR in Continuous mode at prescaler 1, counting PCLK = SYSCLK/2 (UG6971 Eq. 15-1,
// MAX32650 UG p41, MAX32690 UG 4.2.2). The 1 MHz "microseconds" contract the M0/RISC-V
// families follow is unreachable here -- the prescaler is powers-of-two only -- so this
// overrides tusb.c's weak SEGGER_SYSVIEW_X_GetTimestampFreq() with the timer's rate.
// UG6971 15.2 guarantees TMRn_CNT "is always readable, even while the timer is enabled and
// counting". With CMP=0xFFFFFFFF the hardware wrap reloads CNT to 0x0000_0001, not 0
// (UG6971 15.5.1, MAX32690 UG 23.7.2): modulus 2^32-1, one tick lost per wrap -- on MAX3266x
// one 20.8 ns slip per ~89.5 s, ~0.23 ppb, far below crystal tolerance.
//
// The instance is SYSVIEW_MAX32_TMR (default TMR0, present on every MAX32 part). TinyUSB
// examples use no TMR at all and the FreeRTOS builds tick on SysTick (lib/FreeRTOS-Kernel's
// ARM_CM4F port -- MSDK's TMR-based tickless demos are never compiled here), so any instance
// is free; override with -DSYSVIEW_MAX32_TMR=<0..5> (<0..3> on MAX32690) if an application
// claims TMR0.
//
// Include once from family.c inside its `#if CFG_TUSB_SYSVIEW_ENABLED` guard:
// SEGGER_SYSVIEW_X_GetTimestamp*() below are non-static linker-visible hooks (SEGGER calls
// them by name), so a second include in the same TU is a redefinition error.
#ifndef TUSB_BSP_SYSVIEW_MAX32_TMR_H_
#define TUSB_BSP_SYSVIEW_MAX32_TMR_H_

#include "tmr_regs.h" // mxc_device.h pulls in no peripheral register maps; these two are
#include "gcr_regs.h" // on the include path from the family's CMSIS/Device Include dir

#ifndef SYSVIEW_MAX32_TMR
#define SYSVIEW_MAX32_TMR 0
#endif

#if defined(MAX32690)
// MAX32690: TMR0-3 are dual 16-bit timers run here cascaded as one 32-bit counter on PCLK
// (UG MAX32690 Table 23-1, 23.3); their clock-select, clock-ready and write-done handshakes
// (23.4) are MSDK's driver's, which family.cmake compiles for this part. TMR4/5 are low-power
// timers without PCLK.
#include "tmr.h"
#if SYSVIEW_MAX32_TMR > 3
#error "SYSVIEW_MAX32_TMR: MAX32690's TMR4/5 have no PCLK source, use 0..3"
#endif

static inline void sysview_max32_tmr_start(void) {
  mxc_tmr_cfg_t cfg = {.pres = TMR_PRES_1, .mode = TMR_MODE_CONTINUOUS, .bitMode = TMR_BIT_MODE_32,
                       .clock = MXC_TMR_APB_CLK, .cmp_cnt = 0xFFFFFFFFu, .pol = 0};
  MXC_TMR_Init(MXC_TMR_GET_TMR(SYSVIEW_MAX32_TMR), &cfg, false);
  MXC_TMR_Start(MXC_TMR_GET_TMR(SYSVIEW_MAX32_TMR));
}
#else
static inline void sysview_max32_tmr_start(void) {
  mxc_tmr_regs_t* tmr = MXC_TMR_GET_TMR(SYSVIEW_MAX32_TMR);
#if defined(MAX32650)
  // PCLK_DIS0 timer0..5 disable bits are contiguous from bit 15 (UG MAX32650 Table 3-13)
  MXC_GCR->pclk_dis0 &= ~(MXC_F_GCR_PCLK_DIS0_TIMER0 << SYSVIEW_MAX32_TMR);
#else
  // PERCKCN0 timer disable bits are contiguous from TIMER0D (gcr_regs.h) and reset to 1, clock
  // disabled (UG6971 Table 4-59)
  MXC_GCR->perckcn0 &= ~(MXC_F_GCR_PERCKCN0_TIMER0D << SYSVIEW_MAX32_TMR);
#endif
  tmr->cn  = 0;                                // disable while configuring; prescaler bits stay 0 = /1
  tmr->cmp = 0xFFFFFFFFu;                      // free-run over the full range
  tmr->cnt = 1u;                               // the value hardware reloads at every wrap
  tmr->cn  = MXC_S_TMR_CN_TMODE_CONTINUOUS | MXC_F_TMR_CN_TEN;
}
#endif

uint32_t SEGGER_SYSVIEW_X_GetTimestamp(void) {
  return MXC_TMR_GET_TMR(SYSVIEW_MAX32_TMR)->cnt; // free-running PCLK ticks
}

uint32_t SEGGER_SYSVIEW_X_GetTimestampFreq(void) {
  return PeripheralClock; // system_<part>.h: SystemCoreClock / 2 -- tracks reclocking
}

#endif /* TUSB_BSP_SYSVIEW_MAX32_TMR_H_ */
