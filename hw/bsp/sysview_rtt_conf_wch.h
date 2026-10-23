/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// Shared by every WCH family in a SYSVIEW build. Force-included ahead of every source file (see
// each family.cmake) so it wins the "#ifndef SEGGER_RTT_CONF_H" include-guard race against the
// vendored lib/SEGGER_RTT/Config/SEGGER_RTT_Conf.h -- that file is not to be edited (get_deps.py
// can overwrite it wholesale) and its generic RISC-V SEGGER_RTT_LOCK()/UNLOCK() read/write the
// standard `mstatus` CSR via `csrr`, which traps illegal instruction (mcause=2) on WCH's QingKe
// core -- observed on ch32v307 at the very first instruction of SEGGER_RTT_AllocUpBuffer's lock.
// Claiming the guard here means the vendored file's body (including that lock) never runs; the
// values below match its defaults exactly (also mirrored by lib/SEGGER_RTT/RTT/SEGGER_RTT.c's own
// ifndef-guarded fallbacks, except SEGGER_RTT_MAX_NUM_UP_BUFFERS, which
// SEGGER_SYSVIEW_ConfDefaults.h requires directly with no fallback of its own).
//
// Only the lock differs per family. WCH's own core_riscv.h interrupt helpers use the custom "fast
// interrupt" CSR 0x800 instead of mstatus, and only ever WRITE it -- unconditionally, never
// restoring a caller's prior state. SEGGER_RTT.c calls LOCK/UNLOCK from both task and ISR context
// with no other serialization, so an unconditional UNLOCK force-reenables interrupts mid-ISR the
// moment a SystemView event fires from inside one. Fixed the same way SEGGER's own generic RISC-V
// lock does it: brace-scoped save/restore -- LOCK reads the CSR back first (a plain `csrr`,
// side-effect-free on any implemented RISC-V CSR, unlike a write) into a block-local that UNLOCK
// restores verbatim; LOCK opens the block, UNLOCK closes it. A family whose vendor masks
// interrupts differently defines SEGGER_RTT_WCH_MASK_IRQ() before including this file.
#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   (3)
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS (3)
#define BUFFER_SIZE_UP                  (1024)
#define BUFFER_SIZE_DOWN                (16)
#define SEGGER_RTT_PRINTF_BUFFER_SIZE   (64u)
#define SEGGER_RTT_MODE_DEFAULT         SEGGER_RTT_MODE_NO_BLOCK_SKIP
#define SEGGER_RTT_MEMCPY_USE_BYTELOOP  0

#ifndef SEGGER_RTT_WCH_MASK_IRQ
// ch32v20x/v30x: __disable_irq() clears the fixed 0x88 mask (core_riscv.h)
#define SEGGER_RTT_WCH_MASK_IRQ() __asm volatile("csrc 0x800, %0" ::"r"(0x88) : "memory")
#endif

#define SEGGER_RTT_LOCK() {                                       \
    unsigned int _seg_rtt_lock_state;                             \
    __asm volatile("csrr %0, 0x800" : "=r"(_seg_rtt_lock_state)); \
    SEGGER_RTT_WCH_MASK_IRQ();
#define SEGGER_RTT_UNLOCK()                                                 \
    __asm volatile("csrw 0x800, %0" ::"r"(_seg_rtt_lock_state) : "memory"); \
  }

#endif // SEGGER_RTT_CONF_H
