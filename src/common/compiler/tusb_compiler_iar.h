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

#ifndef _TUSB_COMPILER_IAR_H_
#define _TUSB_COMPILER_IAR_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define ALIGN_OF(x)               __ALIGNOF__(x)
#define ATTR_ALIGNED(bytes)       _Pragma(XSTRING_(data_alignment=##bytes))
//#define ATTR_SECTION(section)      _Pragma((#section))
#define ATTR_PREPACKED            __packed
#define ATTR_PACKED
#define ATTR_DEPRECATED(mess)
#define ATTR_WEAK                 __weak
#define ATTR_UNUSED

// built-in function to convert 32-bit Big-Endian to Little-Endian
//#if __LITTLE_ENDIAN__
#define __be2n   __REV
#define __n2be   __be2n

#define __n2be_16(u16)  ((uint16_t) __REV16(u16))
#define __be2n_16(u16)  __n2be_16(u16)

#error "IAR won't work due to '__packed' placement before struct"

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMPILER_IAR_H_ */

