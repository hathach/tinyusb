/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (tinyusb.org)
 * Copyright (c) 2023 IngHK Heiko Kuester
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

#ifndef _TUSB_CDC_DEBUG_H_
#define _TUSB_CDC_DEBUG_H_

#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

// logging of line coding
#define TU_LOG_LINE_CODING(PRE_TEXT,LINE_CODING)                          \
  TU_LOG_DRV(PRE_TEXT "Line Coding %luBd %u%c%s\r\n",                     \
  LINE_CODING.bit_rate,                                                   \
  LINE_CODING.data_bits,                                                  \
  LINE_CODING.parity == CDC_LINE_CODING_PARITY_NONE ? 'N' :               \
    LINE_CODING.parity == CDC_LINE_CODING_PARITY_ODD ? 'O' :              \
      LINE_CODING.parity == CDC_LINE_CODING_PARITY_EVEN ? 'E' :           \
        LINE_CODING.parity == CDC_LINE_CODING_PARITY_MARK ? 'M' :         \
          LINE_CODING.parity == CDC_LINE_CODING_PARITY_SPACE ? 'S' : '?', \
  LINE_CODING.stop_bits == CDC_LINE_CODING_STOP_BITS_1 ? "1" :            \
    LINE_CODING.stop_bits == CDC_LINE_CODING_STOP_BITS_1_5 ? "1.5" :      \
      LINE_CODING.stop_bits == CDC_LINE_CODING_STOP_BITS_2 ? "2" : "?")

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_DEBUG_H_ */
