/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// System header pulled in by the SDK's ch32h417.h. The EVT ships this file per-project; the
// implementation compiled by this BSP is the EVT GPIO demo's V3F system_ch32h417.c.

#ifndef SYSTEM_CH32H417_H_
#define SYSTEM_CH32H417_H_

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t SystemClock;     // SYSCLK
extern uint32_t HCLKClock;       // HB bus / V3F core clock
extern uint32_t SystemCoreClock; // clock of the core this code runs on

extern void SystemInit(void);
extern void SystemAndCoreClockUpdate(void);

#ifdef __cplusplus
}
#endif

#endif
