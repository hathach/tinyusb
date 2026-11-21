/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */

// Force-included wherever SEGGER_RTT.c is built, SYSVIEW or LOGGER=rtt (FAMILY_RTT_CONF in family_support.cmake/.mk),
// so it claims SEGGER_RTT_CONF_H before the vendored lib/SEGGER_RTT/Config/SEGGER_RTT_Conf.h, whose generic RISC-V
// lock reads mstatus and traps (mcause=2): the QingKe startup mrets to main in U-mode. The values match that file's
// defaults.
#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#ifndef SEGGER_RTT_MAX_NUM_UP_BUFFERS
  #define SEGGER_RTT_MAX_NUM_UP_BUFFERS   (3)
#endif

#ifndef SEGGER_RTT_MAX_NUM_DOWN_BUFFERS
  #define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS (3)
#endif

#ifndef BUFFER_SIZE_UP
  #define BUFFER_SIZE_UP                  (1024)
#endif

#ifndef BUFFER_SIZE_DOWN
  #define BUFFER_SIZE_DOWN                (16)
#endif

#ifndef SEGGER_RTT_PRINTF_BUFFER_SIZE
  #define SEGGER_RTT_PRINTF_BUFFER_SIZE   (64u)
#endif

#ifndef SEGGER_RTT_MODE_DEFAULT
  #define SEGGER_RTT_MODE_DEFAULT         SEGGER_RTT_MODE_NO_BLOCK_SKIP
#endif

#ifndef SEGGER_RTT_MEMCPY_USE_BYTELOOP
  #define SEGGER_RTT_MEMCPY_USE_BYTELOOP  0
#endif

// QingKe V3V masks interrupts in CSR 0x800 (gintenr, the U-mode view of mstatus MIE/MPIE, writable with 0xBC0 bit5
// set as the startup does; QingKe V3 manual §3.2, §8.3) with fence.i after it, as the SDK's __disable_irq() does
// (EVT/EXAM/SRC/Core/core_riscv.h). SEGGER_RTT locks from task and ISR context, so LOCK saves the CSR and UNLOCK
// restores it instead of re-enabling.
#define SEGGER_RTT_LOCK() {                                       \
    unsigned int _seg_rtt_lock_state;                             \
    __asm volatile("csrr %0, 0x800" : "=r"(_seg_rtt_lock_state)); \
    __asm volatile("csrc 0x800, %0\n fence.i" ::"r"(0x88) : "memory");
#define SEGGER_RTT_UNLOCK()                                                 \
    __asm volatile("csrw 0x800, %0" ::"r"(_seg_rtt_lock_state) : "memory"); \
  }

#endif // SEGGER_RTT_CONF_H
