/**************************************************************************/
/*!
    @file     compiler_iar.h
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

/** \file
 *  \brief IAR Compiler
 */

/** \ingroup Group_Compiler
 *  \defgroup Group_IAR IAR ARM
 *  @{
 */

#ifndef _TUSB_COMPILER_IAR_H_
#define _TUSB_COMPILER_IAR_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define ALIGN_OF(x)     __ALIGNOF__(x)

#define ATTR_PACKED_STRUCT(x)     __packed x
#define ATTR_PREPACKED  __packed
#define ATTR_PACKED

#define ATTR_ALIGNED(Bytes)        ATTR_ALIGNED_##Bytes
#define ATTR_ALIGNED_4096          _Pragma("data_alignment=4096")
#define ATTR_ALIGNED_2048          _Pragma("data_alignment=2048")
#define ATTR_ALIGNED_256           _Pragma("data_alignment=256")
#define ATTR_ALIGNED_128           _Pragma("data_alignment=128")
#define ATTR_ALIGNED_64            _Pragma("data_alignment=64")
#define ATTR_ALIGNED_48            _Pragma("data_alignment=48")
#define ATTR_ALIGNED_32            _Pragma("data_alignment=32")
#define ATTR_ALIGNED_4             _Pragma("data_alignment=4")



#ifndef ATTR_ALWAYS_INLINE
/// Generally, functions are not inlined unless optimization is specified. For functions declared inline, this attribute inlines the function even if no optimization level is specified
#define ATTR_ALWAYS_INLINE         error
#endif

#define ATTR_PURE   // TODO IAR pure function attribute
#define ATTR_CONST  // TODO IAR const function attribute
#define ATTR_WEAK                 __weak

#define ATTR_WARN_UNUSED_RESULT
#define ATTR_USED
#define ATTR_UNUSED

#if 0

				/** Indicates to the compiler that the function can not ever return, so that any stack restoring or
				 *  return code may be omitted by the compiler in the resulting binary.
				 */
				#define ATTR_NO_RETURN

				/** Indicates that the specified parameters of the function are pointers which should never be \c NULL.
				 *  When applied as a 1-based comma separated list the compiler will emit a warning if the specified
				 *  parameters are known at compiler time to be \c NULL at the point of calling the function.
				 */
				#define ATTR_NON_NULL_PTR_ARG(...)

				/** Removes any preamble or postamble from the function. When used, the function will not have any
				 *  register or stack saving code. This should be used with caution, and when used the programmer
				 *  is responsible for maintaining stack and register integrity.
				 */
				#define ATTR_NAKED                  __attribute__ ((naked))

				/** Prevents the compiler from considering a specified function for in-lining. When applied, the given
				 *  function will not be in-lined under any circumstances.
				 */
				#define ATTR_NO_INLINE              __attribute__ ((noinline))

				/** Forces the compiler to inline the specified function. When applied, the given function will be
				 *  in-lined under all circumstances.
				 */
				#define PRAGMA_ALWAYS_INLINE          _Pragma("inline=forced")
				#define ATTR_ALWAYS_INLINE

				/** Indicates that the specified function is pure, in that it has no side-effects other than global
				 *  or parameter variable access.
				 */
				#define ATTR_PURE                   __attribute__ ((pure))

				/** Indicates that the specified function is constant, in that it has no side effects other than
				 *  parameter access.
				 */
				#define ATTR_CONST

				/** Marks a given function as deprecated, which produces a warning if the function is called. */
				#define ATTR_DEPRECATED//             __attribute__ ((deprecated))

				/** Marks a function as a weak reference, which can be overridden by other functions with an
				 *  identical name (in which case the weak reference is discarded at link time).
				 */
				#define _PPTOSTR_(x) #x
				#define PRAGMA_WEAK(name, vector) _Pragma(_PPTOSTR_(weak name=vector))
				#define ATTR_WEAK

				/** Marks a function as an alias for another function.
				 *
				 *  \param[in] Func  Name of the function which the given function name should alias.
				 */
				#define ATTR_ALIAS(Func)

				/** Forces the compiler to not automatically zero the given global variable on startup, so that the
				 *  current RAM contents is retained. Under most conditions this value will be random due to the
				 *  behaviour of volatile memory once power is removed, but may be used in some specific circumstances,
				 *  like the passing of values back after a system watchdog reset.
				 */
				#define ATTR_NO_INIT                __attribute__ ((section (".noinit")))
				/** Indicates the minimum alignment in bytes for a variable or struct element.
				 *
				 *  \param[in] Bytes  Minimum number of bytes the item should be aligned to.
				 */
				#define PRAGMA_ALIGN_4096          _Pragma("data_alignment=4096")
				#define PRAGMA_ALIGN_2048          _Pragma("data_alignment=2048")
				#define PRAGMA_ALIGN_256           _Pragma("data_alignment=256")
				#define PRAGMA_ALIGN_128           _Pragma("data_alignment=128")
				#define PRAGMA_ALIGN_64            _Pragma("data_alignment=64")
				#define PRAGMA_ALIGN_48            _Pragma("data_alignment=48")
				#define PRAGMA_ALIGN_32            _Pragma("data_alignment=32")
				#define PRAGMA_ALIGN_4             _Pragma("data_alignment=4")
				#define ATTR_ALIGNED(Bytes)

				//#define ATTR_DEPRECATED				   __attribute__ ((deprecated))

				#define ATTR_ERROR(Message)//			   __attribute__ (( error(Message) ))

				#define ATTR_WARNING(Message)	//		   __attribute__ (( warning(Message) ))

				#define ATTR_IAR_PACKED				   __packed
				#define ATTR_PACKED
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMPILER_IAR_H_ */

/** @} */
