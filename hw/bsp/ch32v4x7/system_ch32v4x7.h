/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// System header pulled in by the SDK's ch32v4x7.h. The EVT ships it per-project; the
// implementation compiled by this BSP is the EVT GPIO demo's system_ch32v4x7.c.

#ifndef SYSTEM_CH32V4X7_H_
#define SYSTEM_CH32V4X7_H_

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t SystemClock;     // SYSCLK
extern uint32_t HCLKClock;       // HCLK
extern uint32_t SystemCoreClock; // core clock

extern void SystemInit(void);
extern void SystemCoreClockUpdate(void);

#ifdef __cplusplus
}
#endif

#endif
