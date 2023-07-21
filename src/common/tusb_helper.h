/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#ifndef _TUSB_HELPER_H_
#define _TUSB_HELPER_H_

//--------------------------------------------------------------------+
// Macros Helper
//--------------------------------------------------------------------+
#define TU_ARRAY_SIZE(_arr)   ( sizeof(_arr) / sizeof(_arr[0]) )
#define TU_MIN(_x, _y)        ( ( (_x) < (_y) ) ? (_x) : (_y) )
#define TU_MAX(_x, _y)        ( ( (_x) > (_y) ) ? (_x) : (_y) )

#define TU_U16(_high, _low)   ((uint16_t) (((_high) << 8) | (_low)))
#define TU_U16_HIGH(_u16)     ((uint8_t) (((_u16) >> 8) & 0x00ff))
#define TU_U16_LOW(_u16)      ((uint8_t) ((_u16)       & 0x00ff))
#define U16_TO_U8S_BE(_u16)   TU_U16_HIGH(_u16), TU_U16_LOW(_u16)
#define U16_TO_U8S_LE(_u16)   TU_U16_LOW(_u16), TU_U16_HIGH(_u16)

#define TU_U32_BYTE3(_u32)    ((uint8_t) ((((uint32_t) _u32) >> 24) & 0x000000ff)) // MSB
#define TU_U32_BYTE2(_u32)    ((uint8_t) ((((uint32_t) _u32) >> 16) & 0x000000ff))
#define TU_U32_BYTE1(_u32)    ((uint8_t) ((((uint32_t) _u32) >>  8) & 0x000000ff))
#define TU_U32_BYTE0(_u32)    ((uint8_t) (((uint32_t)  _u32)        & 0x000000ff)) // LSB

#define U32_TO_U8S_BE(_u32)   TU_U32_BYTE3(_u32), TU_U32_BYTE2(_u32), TU_U32_BYTE1(_u32), TU_U32_BYTE0(_u32)
#define U32_TO_U8S_LE(_u32)   TU_U32_BYTE0(_u32), TU_U32_BYTE1(_u32), TU_U32_BYTE2(_u32), TU_U32_BYTE3(_u32)

#define TU_BIT(n)             (1UL << (n))
#define TU_GENMASK(h, l)      ( (UINT32_MAX << (l)) & (UINT32_MAX >> (31 - (h))) )

#endif /* _TUSB_HELPER_H_ */
