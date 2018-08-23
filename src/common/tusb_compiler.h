/**************************************************************************/
/*!
    @file     compiler.h
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
#if defined(__ICCARM__) || (__STDC_VERSION__ >= 201112L )
  #include <assert.h>
  #define TU_VERIFY_STATIC   static_assert
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
