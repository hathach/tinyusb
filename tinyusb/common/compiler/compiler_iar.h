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

// built-in function to convert 32-bit Big-Endian to Little-Endian
#define __be2le   __REV
#define __le2be   __be2le

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMPILER_IAR_H_ */

/** @} */
