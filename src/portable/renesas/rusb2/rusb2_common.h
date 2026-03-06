/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ha Thach (tinyusb.org)
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
 * This file is part of the TinyUSB stack.
 */
#pragma once

#include "common/tusb_common.h"
#include "rusb2_type.h"

#if TU_CHECK_MCU(OPT_MCU_RX63X, OPT_MCU_RX65X, OPT_MCU_RX72N)
  #include "rusb2_rx.h"
#elif TU_CHECK_MCU(OPT_MCU_RAXXX)
  #include "rusb2_ra.h"

  // Hack for D0FIFO definitions on RA Cortex-M23
  #if defined(RENESAS_CORTEX_M23)
    #define D0FIFO      CFIFO
    #define D0FIFOSEL   CFIFOSEL
    #define D0FIFOSEL_b CFIFOSEL_b
    #define D1FIFOSEL   CFIFOSEL
    #define D1FIFOSEL_b CFIFOSEL_b
    #define D0FIFOCTR   CFIFOCTR
    #define D0FIFOCTR_b CFIFOCTR_b
  #endif

#else
  #error "Unsupported MCU"
#endif


//--------------------------------------------------------------------+
// Common
//--------------------------------------------------------------------+

enum {
  FIFOSEL_BIGEND = (TU_BYTE_ORDER == TU_BIG_ENDIAN ? RUSB2_FIFOSEL_BIGEND : 0)
};
