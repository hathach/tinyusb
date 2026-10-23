/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// Shared SEGGER_SYSVIEW_X_GetTimestamp() timebase for the STM32 families whose Cortex-M0/M0+ core
// has no DWT cycle counter (stm32f0, stm32g0, stm32u0): a free-running TIM2, the only 32-bit
// general-purpose timer on each of these series, clocked to 1 MHz so it wraps every ~71 minutes
// instead of the ~65 ms a 16-bit timer would give. SysTick is deliberately left alone in every
// family -- it backs board_millis()/the RTOS tick.
//
// Include this once, near the top of each family.c (after the vendor HAL / device header, so
// TIM2 and SystemCoreClock are already visible), inside the family's own
// `#if (CFG_TUD_SYSVIEW || CFG_TUH_SYSVIEW) && defined(TIM2)` guard -- a part with no TIM2 (e.g.
// STM32F070) then falls through to the intended "SYSVIEW not ported" link error instead of a
// hard compile error naming a register that does not exist on that part. It provides both
// sysview_stm32_tim2_start() (call from board_init(), under the same guard) and the
// SEGGER_SYSVIEW_X_GetTimestamp() definition itself, so one include line does both jobs.
//
// TIM2->SR is cleared after the TIM_EGR_UG update -- kept from stm32f0/stm32g0, even though
// nothing here reads SR; stm32u0 had silently dropped it, and clearing the flag this cheaply
// avoids depending on that omission staying harmless if TIM2 is ever touched for anything else.
//
// Include-once-per-family, not general-purpose: SEGGER_SYSVIEW_X_GetTimestamp() below is a
// non-static linker-visible hook (SEGGER calls it by name), so a second include in the same TU
// is a redefinition error and a second .c in the same family including it is a duplicate symbol
// at link. The guard below only protects against the former.
#ifndef TUSB_BSP_SYSVIEW_STM32_TIM2_H_
#define TUSB_BSP_SYSVIEW_STM32_TIM2_H_

static inline void sysview_stm32_tim2_start(void) {
  __HAL_RCC_TIM2_CLK_ENABLE();
  TIM2->PSC = SystemCoreClock / 1000000u - 1u; // CK_CNT = fCK_PSC / (PSC + 1) = 1 MHz
  TIM2->ARR = 0xFFFFFFFFu;                     // 32-bit counter: free-run, wrap only at full range
  TIM2->EGR = TIM_EGR_UG;                      // latch PSC/ARR into the shadow regs, clears CNT
  TIM2->SR  = 0;                               // drop the update flag UG just raised
  TIM2->CR1 = TIM_CR1_CEN;
}

uint32_t SEGGER_SYSVIEW_X_GetTimestamp(void) {
  return TIM2->CNT; // free-running 1 MHz, started by sysview_stm32_tim2_start() above
}

#endif /* TUSB_BSP_SYSVIEW_STM32_TIM2_H_ */
