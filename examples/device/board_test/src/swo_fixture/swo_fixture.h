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
 *
 */

#ifndef SWO_FIXTURE_H_
#define SWO_FIXTURE_H_

#include <stdint.h>

// Counters of one phase; a debug probe reads swo_fixture_last for the phase it captured
typedef struct {
  uint32_t phase;
  uint32_t tim6_count;   // timer interrupts at SWO_FIXTURE_TIM6_HZ
  uint32_t tim7_count;   // timer interrupts at SWO_FIXTURE_TIM7_HZ, each pends one nested interrupt
  uint32_t nested_count; // nested interrupts, entered inside a TIM7 handler
  uint32_t var_writes;   // writes of swo_fixture_var
  uint32_t var_reads;    // reads of swo_fixture_var
  uint32_t itm_sent;     // ITM stimulus writes made
  uint32_t itm_skipped;  // ITM stimulus writes dropped: ITM or the port disabled, or the FIFO stayed full
} swo_fixture_stats_t;

void swo_fixture_init(void);
void swo_fixture_task(void);

#endif
