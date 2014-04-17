/**************************************************************************/
/*!
    @file     compiler_gcc.h
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

/// Normally, the compiler places the objects it generates in sections like data or bss & function in text. Sometimes, however, you need additional sections, or you need certain particular variables to appear in special sections, for example to map to special hardware. The section attribute specifies that a variable (or function) lives in a particular section
#define ATTR_SECTION(sec_name)      __attribute__ (( section(#sec_name) ))

/// If this attribute is used on a function declaration and a call to such a function is not eliminated through dead code elimination or other optimizations, an error that includes message is diagnosed. This is useful for compile-time checking
#define ATTR_ERROR(Message)        __attribute__ ((error(Message)))

/// If this attribute is used on a function declaration and a call to such a function is not eliminated through dead code elimination or other optimizations, a warning that includes message is diagnosed. This is useful for compile-time checking
#define ATTR_WARNING(Message)      __attribute__ ((warning(Message)))

/** \defgroup Group_VariableAttr Variable Attributes
 *  @{ */

/// This attribute specifies a minimum alignment for the variable or structure field, measured in bytes
#define ATTR_ALIGNED(Bytes)        __attribute__ ((aligned(Bytes)))

/// The packed attribute specifies that a variable or structure field should have the smallest possible alignment—one byte for a variable, and one bit for a field, unless you specify a larger value with the aligned attribute
#define ATTR_PACKED                __attribute__ ((packed))

#define ATTR_PREPACKED

#define ATTR_PACKED_STRUCT(x)     x __attribute__ ((packed))
/** @} */

/** \defgroup Group_FuncAttr Function Attributes
 *  @{ */

#ifndef ATTR_ALWAYS_INLINE
/// Generally, functions are not inlined unless optimization is specified. For functions declared inline, this attribute inlines the function even if no optimization level is specified
#define ATTR_ALWAYS_INLINE         __attribute__ ((always_inline))
#endif

/// The nonnull attribute specifies that some function parameters should be non-null pointers. f the compiler determines that a null pointer is passed in an argument slot marked as non-null, and the -Wnonnull option is enabled, a warning is issued. All pointer arguments are marked as non-null
#define ATTR_NON_NULL              __attribute__ ((nonull))

/// Many functions have no effects except the return value and their return value depends only on the parameters and/or global variables. Such a function can be subject to common subexpression elimination and loop optimization just as an arithmetic operator would be. These functions should be declared with the attribute pure
#define ATTR_PURE                  __attribute__ ((pure))

/// \brief Many functions do not examine any values except their arguments, and have no effects except the return value. Basically this is just slightly more strict class than the pure attribute below, since function is not allowed to read global memory.
/// Note that a function that has pointer arguments and examines the data pointed to must not be declared const. Likewise, a function that calls a non-const function usually must not be const. It does not make sense for a const function to return void
#define ATTR_CONST                 __attribute__ ((const))

/// The deprecated attribute results in a warning if the function is used anywhere in the source file. This is useful when identifying functions that are expected to be removed in a future version of a program. The warning also includes the location of the declaration of the deprecated function, to enable users to easily find further information about why the function is deprecated, or what they should do instead. Note that the warnings only occurs for uses
#define ATTR_DEPRECATED            __attribute__ ((deprecated))

/// Same as the deprecated attribute with optional message in the warning
#define ATTR_DEPRECATED_MESS(mess) __attribute__ ((deprecated(mess)))

/// The weak attribute causes the declaration to be emitted as a weak symbol rather than a global. This is primarily useful in defining library functions that can be overridden in user code
#define ATTR_WEAK                  __attribute__ ((weak))

/// The alias attribute causes the declaration to be emitted as an alias for another symbol, which must be specified
#define ATTR_ALIAS(func)           __attribute__ ((alias(#func)))

/// The weakref attribute marks a declaration as a weak reference. It is equivalent with weak + alias attribute, but require function is static
#define ATTR_WEAKREF(func)         __attribute__ ((weakref(#func)))

/// The warn_unused_result attribute causes a warning to be emitted if a caller of the function with this attribute does not use its return value. This is useful for functions where not checking the result is either a security problem or always a bug
#define ATTR_WARN_UNUSED_RESULT    __attribute__ ((warn_unused_result))

/// This attribute, attached to a function, means that code must be emitted for the function even if it appears that the function is not referenced. This is useful, for example, when the function is referenced only in inline assembly.
#define ATTR_USED                  __attribute__ ((used))

/// This attribute, attached to a function, means that the function is meant to be possibly unused. GCC does not produce a warning for this function.
#define ATTR_UNUSED                __attribute__ ((unused))

/** @} */

/** \defgroup Group_BuiltinFunc Built-in Functions
*  @{ */

// TODO mcu specific
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __n2be(x)   __REV(x) ///< built-in function to convert 32-bit from native to Big Endian
#define __be2n(x)   __n2be(x) ///< built-in function to convert 32-bit from Big Endian to native

#define __n2be_16(u16)  ((uint16_t) __REV16(u16))
#define __be2n_16(u16)  __n2be_16(u16)
#endif

/** @} */

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMPILER_GCC_H_ */

/// @}
