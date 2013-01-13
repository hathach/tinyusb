/*
 * binary.h
 *
 *  Created on: Jan 11, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
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

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_BINARY_H_
#define _TUSB_BINARY_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>

/// n-th Bit
#define BIT_(n) (1 << (n))

/// set n-th bit of x to 1
#define BIT_SET_(x, n) ( (x) | BIT_(n) )

/// clear n-th bit of x
#define BIT_CLR_(x, n) ( (x) & (~BIT_(n)) )

#if defined(__GNUC__)

#define BIN8(x) (0b##x)
#define BIN16   BIN8
#define BIN32   BIN8

#else

//  internal macro of B8, B16, B32
#define _B8__(x) ((x&0x0000000FLU)?1:0) \
                +((x&0x000000F0LU)?2:0) \
                +((x&0x00000F00LU)?4:0) \
                +((x&0x0000F000LU)?8:0) \
                +((x&0x000F0000LU)?16:0) \
                +((x&0x00F00000LU)?32:0) \
                +((x&0x0F000000LU)?64:0) \
                +((x&0xF0000000LU)?128:0)

#define BIN8(d) ((uint8_t)_B8__(0x##d##LU))
#define BIN16(dmsb,dlsb) (((uint16_t)B8(dmsb)<<8) + B8(dlsb))
#define BIN32(dmsb,db2,db3,dlsb) \
            (((uint32_t)B8(dmsb)<<24) \
            + ((uint32_t)B8(db2)<<16) \
            + ((uint32_t)B8(db3)<<8) \
            + B8(dlsb))
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_BINARY_H_ */

/** @} */
