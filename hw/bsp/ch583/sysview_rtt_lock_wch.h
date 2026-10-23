/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// CH58x masks interrupts differently from ch32v20x/v30x: PFIC_DisableAllIRQ()
// (hw/mcu/wch/ch583/EVT/EXAM/SRC/RVMSIS/core_riscv.h:99-100) writes CSR 0x800 absolutely
// (csrw 0x80, not a set/clear of 0x88) and follows it with fence.i. Both are kept as-is here;
// only the vendor's unconditional unlock is replaced, by the shared header's save/restore.
#define SEGGER_RTT_WCH_MASK_IRQ()                            \
  do {                                                       \
    __asm volatile("csrw 0x800, %0" ::"r"(0x80) : "memory"); \
    __asm volatile("fence.i");                               \
  } while (0)

#include "../sysview_rtt_conf_wch.h"
