/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// Force-included in SYSVIEW builds (family.cmake) so it claims SEGGER_RTT_CONF_H before the vendored
// lib/SEGGER_RTT/Config/SEGGER_RTT_Conf.h, whose generic RISC-V lock reads mstatus with csrr and
// traps (mcause=2) on WCH's QingKe core. The values match that file's defaults.
#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   (3)
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS (3)
#define BUFFER_SIZE_UP                  (1024)
#define BUFFER_SIZE_DOWN                (16)
#define SEGGER_RTT_PRINTF_BUFFER_SIZE   (64u)
#define SEGGER_RTT_MODE_DEFAULT         SEGGER_RTT_MODE_NO_BLOCK_SKIP
#define SEGGER_RTT_MEMCPY_USE_BYTELOOP  0

// QingKe masks interrupts in CSR 0x800; __disable_irq() clears 0x88 (core_riscv.h). SEGGER_RTT locks
// from task and ISR context, so LOCK saves the CSR and UNLOCK restores it instead of re-enabling.
#define SEGGER_RTT_LOCK() {                                       \
    unsigned int _seg_rtt_lock_state;                             \
    __asm volatile("csrr %0, 0x800" : "=r"(_seg_rtt_lock_state)); \
    __asm volatile("csrc 0x800, %0" ::"r"(0x88) : "memory");
#define SEGGER_RTT_UNLOCK()                                                 \
    __asm volatile("csrw 0x800, %0" ::"r"(_seg_rtt_lock_state) : "memory"); \
  }

#endif // SEGGER_RTT_CONF_H
