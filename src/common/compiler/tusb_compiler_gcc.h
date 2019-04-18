/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

/** \ingroup Group_Compiler
 *  \defgroup Group_GCC GNU GCC
 *  @{ */

#ifndef _TUSB_COMPILER_GCC_H_
#define _TUSB_COMPILER_GCC_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define ALIGN_OF(x)                __alignof__(x)

// Specifies a minimum alignment for the variable or structure field, measured in bytes
#define ATTR_ALIGNED(Bytes)        __attribute__ ((aligned(Bytes)))

// Place variable in a specific section
#define ATTR_SECTION(sec_name)      __attribute__ (( section(#sec_name) ))

// Packed struct/variable into smallest possible size
#define ATTR_PACKED                __attribute__ ((packed))
#define ATTR_PREPACKED

// The deprecated attribute results in a warning if the function is used.
#define ATTR_DEPRECATED(mess)      __attribute__ ((deprecated(mess)))

// The weak attribute causes the declaration to be emitted as a weak symbol rather than a global.
#define ATTR_WEAK                  __attribute__ ((weak))

// Function/Variable is meant to be possibly unused (thus no warning)
#define ATTR_UNUSED                __attribute__ ((unused))

// TODO mcu specific
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __n2be(x)       __builtin_bswap32(x) ///< built-in function to convert 32-bit from native to Big Endian
#define __be2n(x)       __n2be(x) ///< built-in function to convert 32-bit from Big Endian to native

#define __n2be_16(u16)  __builtin_bswap16(u16)
#define __be2n_16(u16)  __n2be_16(u16)
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMPILER_GCC_H_ */

/// @}
