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

/** \ingroup Group_Common
 *  \defgroup Group_Compiler Compiler
 *  \brief Group_Compiler brief
 *  @{ */

#ifndef _TUSB_COMPILER_H_
#define _TUSB_COMPILER_H_

#define STRING_(x)            #x                   ///< stringify without expand
#define XSTRING_(x)           STRING_(x)           ///< expand then stringify
#define STRING_CONCAT_(a, b)  a##b                 ///< concat without expand
#define XSTRING_CONCAT_(a, b) STRING_CONCAT_(a, b) ///< expand then concat

#if defined __COUNTER__ && __COUNTER__ != __COUNTER__
  #define _TU_COUNTER_ __COUNTER__
#else
  #define _TU_COUNTER_ __LINE__
#endif

//--------------------------------------------------------------------+
// Compile-time Assert (use TU_VERIFY_STATIC to avoid name conflict)
//--------------------------------------------------------------------+
#if __STDC_VERSION__ >= 201112L
  #define TU_VERIFY_STATIC   _Static_assert
#else
  #define TU_VERIFY_STATIC(const_expr, _mess) enum { XSTRING_CONCAT_(_verify_static_, _TU_COUNTER_) = 1/(!!(const_expr)) }
#endif

// allow debugger to watch any module-wide variables anywhere
#if CFG_TUSB_DEBUG
#define STATIC_VAR
#else
#define STATIC_VAR static
#endif


#if defined(__GNUC__)
  #include "compiler/tusb_compiler_gcc.h"
#elif defined __ICCARM__
  #include "compiler/tusb_compiler_iar.h"
#endif

#endif /* _TUSB_COMPILER_H_ */

/// @}
