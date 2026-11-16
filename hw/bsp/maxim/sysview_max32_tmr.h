/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// SEGGER_SYSVIEW timestamp for the MAX32 parts, from a TMR instead of the DWT cycle counter:
// MAX3266x's DWT reads NOCYCCNT=1 (observed on max32666fthr), so SystemView's ARMv7-M default
// read of 0xE0001004 returns 0; MAX32650 and MAX32690's user guides do not document their DWT.
// family.cmake routes SEGGER_SYSVIEW_GET_TIMESTAMP to SEGGER_SYSVIEW_X_GetTimestamp below.
//
// The TMR runs in Continuous mode at prescaler 1 on PCLK = SYSCLK/2 (UG6971 Eq. 15-1, MAX32650
// UG p41, MAX32690 UG 4.2.2). The prescaler is powers-of-two only, so 1 MHz is unreachable and
// this overrides tusb.c's weak SEGGER_SYSVIEW_X_GetTimestampFreq() with the timer's rate.
// TMRn_CNT is readable while counting (UG6971 15.2). With CMP=0xFFFFFFFF a wrap reloads CNT to 1,
// not 0 (UG6971 15.5.1, MAX32690 UG 23.7.2): one tick lost per 2^32-1.
//
// SYSVIEW_MAX32_TMR selects the instance, default TMR0: the examples use no TMR and FreeRTOS
// ticks on SysTick. Override with -DSYSVIEW_MAX32_TMR=<0..5> (<0..3> on MAX32690).
//
// Include once, from family.c inside its `#if CFG_TUSB_SYSVIEW_ENABLED` guard: the
// SEGGER_SYSVIEW_X_GetTimestamp*() hooks below are non-static, called by name from SEGGER.
#ifndef TUSB_BSP_SYSVIEW_MAX32_TMR_H_
#define TUSB_BSP_SYSVIEW_MAX32_TMR_H_

#include "tmr_regs.h" // mxc_device.h pulls in no peripheral register maps; these two are
#include "gcr_regs.h" // on the include path from the family's CMSIS/Device Include dir

#ifndef SYSVIEW_MAX32_TMR
#define SYSVIEW_MAX32_TMR 0
#endif

#if SYSVIEW_MAX32_TMR < 0 || SYSVIEW_MAX32_TMR > 5
#error "SYSVIEW_MAX32_TMR: no such timer, use 0..5 (0..3 on MAX32690)"
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
