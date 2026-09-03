/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// SEGGER_SYSVIEW_X_GetTimestamp(), shared by the three ported WCH bare-metal families
// (ch32v20x, ch32v30x, ch583). Each family.c includes this from inside its own
// `#if (CFG_TUD_SYSVIEW || CFG_TUH_SYSVIEW) && CFG_TUSB_OS == OPT_OS_NONE` block -- the only
// place system_ticks/SysTick (defined a few lines above the include site, inside the matching
// `#if CFG_TUSB_OS == OPT_OS_NONE` block) are available -- after defining SYSVIEW_WCH_CLOCK_HZ()
// to its clock source: ch32v20x/ch32v30x use the cached SystemCoreClock global; ch583 has no
// such cache and must call GetSysClock() (a register read + divide, see
// hw/mcu/wch/ch583/EVT/EXAM/SRC/StdPeriphDriver/CH58x_sys.c) -- the same value SysTick_Config()
// itself was given.
//
// RISC-V has no DWT cycle counter, so SystemView takes its timestamp from a hardware timer
// (contract in src/common/tusb_sysview.c: free-running microseconds). The QingKe SysTick is a
// peripheral, not a core register: its CNT counts HCLK cycles and is reloaded to 0 by hardware
// every millisecond (CMP = clock/1000 - 1, see each family's SysTick_Config). Composing the
// millisecond tick with CNT costs no extra peripheral and leaves the 1 ms tick untouched.
//
// Hardware reloads CNT to 0 the instant it matches CMP -- independently of when SysTick_Handler
// actually runs and increments system_ticks. Callers here run under SEGGER_RTT_LOCK() (interrupts
// masked), so a wrap can sit pending for the whole locked section: CNT reads back near 0 while
// system_ticks is still the old value in BOTH reads below, which the stable-read retry can't see
// (system_ticks genuinely hasn't changed yet). Left uncompensated, that is an up-to-1ms backwards
// step versus an event recorded moments earlier at ms*1000 + ~999us. SysTick->SR bit 0 (CNTIF)
// latches on that same hardware wrap regardless of whether the ISR has serviced it yet, so it is
// read alongside cnt and used to compensate the still-pending millisecond. A monotonic clamp is
// kept as belt-and-braces -- callers are serialized by the RTT lock, so the static is race-free
// on this family.
//
// Include-once-per-family, not general-purpose: SEGGER_SYSVIEW_X_GetTimestamp() below is a
// non-static linker-visible hook (SEGGER calls it by name) -- a second include in the same TU
// is a redefinition error, a second .c in the same family including it is a duplicate symbol at
// link. The #ifndef SYSVIEW_WCH_CLOCK_HZ check below only catches "forgot to configure before
// including", not a repeat include; TUSB_BSP_SYSVIEW_WCH_TIMESTAMP_H_ below is the real guard.
#ifndef TUSB_BSP_SYSVIEW_WCH_TIMESTAMP_H_
#define TUSB_BSP_SYSVIEW_WCH_TIMESTAMP_H_

#ifndef SYSVIEW_WCH_CLOCK_HZ
  #error "define SYSVIEW_WCH_CLOCK_HZ() to the family's clock source before including this file"
#endif

uint32_t SEGGER_SYSVIEW_X_GetTimestamp(void) {
  // W11: SYSVIEW_WCH_CLOCK_HZ() is a peripheral-register read + divide (ch583's GetSysClock())
  // or at least a divide on the cached SystemCoreClock (ch32v20x/v30x) -- redone on every
  // recorded event inside this interrupt-masked (SEGGER_RTT_LOCK()) window. The clock never
  // changes after board_init, so latch it once instead. Benign race on the first call (callers
  // are serialized by the RTT lock anyway, so this is theoretical): two callers computing this
  // concurrently would both compute and store the same value.
  static uint32_t cycles_per_us;
  if (cycles_per_us == 0) {
    cycles_per_us = SYSVIEW_WCH_CLOCK_HZ() / 1000000u;
  }
  uint32_t ms, cnt, pend;
  do { // re-read the tick: CNT wraps to 0 exactly when system_ticks increments
    ms = system_ticks;
    // SR before CNT: a wrap landing between the two reads must not pair a
    // pre-wrap CNT with a post-wrap pending flag (that adds a phantom ~1 ms)
    pend = SysTick->SR & 1u;
    cnt = (uint32_t) SysTick->CNT;
  } while (ms != system_ticks || pend != (SysTick->SR & 1u));
  uint32_t t = (ms + (pend ? 1u : 0u)) * 1000u + (cycles_per_us ? cnt / cycles_per_us : 0);
  static uint32_t last;
  if ((int32_t) (t - last) < 0) { t = last; }
  last = t;
  return t;
}

#undef SYSVIEW_WCH_CLOCK_HZ

#endif /* TUSB_BSP_SYSVIEW_WCH_TIMESTAMP_H_ */
