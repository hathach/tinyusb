/**************************************************************************/
/*!
    @file     binary.h
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
 *  \defgroup Group_Binary Binary
 *  @{ */

#ifndef _TUSB_BINARY_H_
#define _TUSB_BINARY_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "tusb_compiler.h"

//------------- Bit manipulation -------------//
#define TU_BIT(n) (1U << (n))                                ///< n-th Bit
#define TU_BIT_SET(x, n) ( (x) | TU_BIT(n) )                   ///< set n-th bit of x to 1
#define TU_BIT_CLEAR(x, n) ( (x) & (~TU_BIT(n)) )                ///< clear n-th bit of x
#define TU_BIT_TEST(x, n) ( ((x) & TU_BIT(n)) ? true : false ) ///< check if n-th bit of x is 1

static inline uint32_t tu_bit_set(uint32_t value, uint8_t n)
{
  return value | TU_BIT(n);
}

static inline uint32_t tu_bit_clear(uint32_t value, uint8_t n)
{
  return value & (~TU_BIT(n));
}

static inline bool tu_bit_test(uint32_t value, uint8_t n)
{
  return (value & TU_BIT(n)) ? true : false;
}

///< create a mask with n-bit lsb set to 1
static inline uint32_t tu_bit_mask(uint8_t n)
{
  return (n < 32) ? ( TU_BIT(n) - 1 ) : UINT32_MAX;
}

//------------- Binary Constant -------------//
#if defined(__GNUC__) && !defined(__CC_ARM)

#define TU_BIN8(x)               ((uint8_t)  (0b##x))
#define TU_BIN16(b1, b2)         ((uint16_t) (0b##b1##b2))
#define TU_BIN32(b1, b2, b3, b4) ((uint32_t) (0b##b1##b2##b3##b4))

#else

//  internal macro of B8, B16, B32
#define _B8__(x) (((x&0x0000000FUL)?1:0) \
                +((x&0x000000F0UL)?2:0) \
                +((x&0x00000F00UL)?4:0) \
                +((x&0x0000F000UL)?8:0) \
                +((x&0x000F0000UL)?16:0) \
                +((x&0x00F00000UL)?32:0) \
                +((x&0x0F000000UL)?64:0) \
                +((x&0xF0000000UL)?128:0))

#define TU_BIN8(d) ((uint8_t) _B8__(0x##d##UL))
#define TU_BIN16(dmsb,dlsb) (((uint16_t)TU_BIN8(dmsb)<<8) + TU_BIN8(dlsb))
#define TU_BIN32(dmsb,db2,db3,dlsb) \
            (((uint32_t)TU_BIN8(dmsb)<<24) \
            + ((uint32_t)TU_BIN8(db2)<<16) \
            + ((uint32_t)TU_BIN8(db3)<<8) \
            + TU_BIN8(dlsb))
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_BINARY_H_ */

/** @} */
