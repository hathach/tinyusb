/**************************************************************************/
/*!
    @file     tusb_compiler_gcc.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	  This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup Group_Compiler
 *  \defgroup Group_GCC GNU GCC
 *  @{ */

#ifndef _TUSB_COMPILER_GCC_H_
#define _TUSB_COMPILER_GCC_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define ALIGN_OF(x) __alignof__(x)

/// This attribute specifies a minimum alignment for the variable or structure field, measured in bytes
#define ATTR_ALIGNED(Bytes)        __attribute__ ((aligned(Bytes)))

/// Place variable in a specific section
#define ATTR_SECTION(sec_name)      __attribute__ (( section(#sec_name) ))

/// The packed attribute specifies that a variable or structure field should have
/// the smallest possible alignment—one byte for a variable, and one bit for a field.
#define ATTR_PACKED                __attribute__ ((packed))
#define ATTR_PREPACKED

/// This attribute inlines the function even if no optimization level is specified
#define ATTR_ALWAYS_INLINE         __attribute__ ((always_inline))

/// The deprecated attribute results in a warning if the function is used anywhere in the source file.
/// This is useful when identifying functions that are expected to be removed in a future version of a program.
#define ATTR_DEPRECATED(mess)      __attribute__ ((deprecated(mess)))

/// The weak attribute causes the declaration to be emitted as a weak symbol rather than a global.
/// This is primarily useful in defining library functions that can be overridden in user code
#define ATTR_WEAK                  __attribute__ ((weak))

/// Warn if a caller of the function with this attribute does not use its return value.
#define ATTR_WARN_UNUSED_RESULT    __attribute__ ((warn_unused_result))

/// Function is meant to be possibly unused. GCC does not produce a warning for this function.
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
