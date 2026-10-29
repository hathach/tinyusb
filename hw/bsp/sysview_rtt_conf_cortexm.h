/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// Force-included in SYSVIEW builds (family_support.cmake). On ARMv7-M/ARMv8-M mainline the vendored
// SEGGER_RTT_Conf.h locks with BASEPRI, which never masks a priority-0 IRQ, and most BSPs leave the USB IRQ
// at its reset priority 0: a USB ISR then records into a packet the interrupted code is still building
// (#4294). UM08027 requires the lock to mask every interrupt that records ("in case of doubt disable all
// interrupts"), so take PRIMASK on every Cortex-M: the vendored ARMv6-M/ARMv8-M baseline lock already does but
// its asm lacks a "memory" clobber, and ARMv8.1-M gets no vendored lock at all. Privileged code only (CPS is
// ignored unprivileged); BASEPRI is left untouched.
#if defined(__GNUC__) && (defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || \
    defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__) || defined(__ARM_ARCH_8_1M_MAIN__))
  #include "SEGGER_RTT_Conf.h"
  #undef SEGGER_RTT_LOCK
  #undef SEGGER_RTT_UNLOCK
  // CPSID serializes the raise; restoring the saved state keeps a nested or already-masked caller masked
  #define SEGGER_RTT_LOCK() {                                                             \
      unsigned int _SEGGER_RTT__LockState;                                                \
      __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(_SEGGER_RTT__LockState) :: "memory");
  #define SEGGER_RTT_UNLOCK()                                                             \
      __asm volatile("msr primask, %0" :: "r"(_SEGGER_RTT__LockState) : "memory");        \
    }
#endif
