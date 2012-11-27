/*
 * compiler_gcc.h
 *
 *  Created on: Nov 26, 2012
 *      Author: hathach (thachha@live.com)
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (thachha@live.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#ifndef _TUSB_COMPILER_GCC_H_
#define _TUSB_COMPILER_GCC_H_

#define ATTR_ALIGNED(Bytes) 		__attribute__ ((aligned(Bytes)))
#define ATTR_PACKED 						__attribute__ ((packed))

#define ATTR_DEPRECATED 				__attribute__ ((deprecated))
#define ATTR_ERROR(Message) 		__attribute__ ((error(Message)))
#define ATTR_WARNING(Message) 	__attribute__ ((warning(Message)))

#define ATTR_WEAK 							__attribute__ ((weak))
#define ATTR_ALIAS(Func)				__attribute__ ((alias(#Func)))

#define ATTR_NO_RETURN					__attribute__ ((noreturn))
#define ATTR_WARN_UNUSED_RESULT __attribute__ ((warn_unused_result))
#define ATTR_ALWAYS_INLINE			__attribute__ ((always_inline))

#endif /* _TUSB_COMPILER_GCC_H_ */
