/**************************************************************************/
/*!
    @file     common.h
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
 *  \defgroup Group_CommonH common.h
 *  @{ */

#ifndef _TUSB_COMMON_H_
#define _TUSB_COMMON_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDES
//--------------------------------------------------------------------+

//------------- Standard Header -------------//
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

//------------- TUSB Option Header -------------//
#include "tusb_option.h"

//------------- General Header -------------//
#include "compiler/compiler.h"
#include "assertion.h"
#include "binary.h"
#include "tusb_errors.h"

//------------- TUSB Header -------------//
#include "tusb_types.h"
#include "std_descriptors.h"
#include "std_request.h"

//--------------------------------------------------------------------+
// MACROS
//--------------------------------------------------------------------+
#define MAX_OF(a, b)  ( (a) > (b) ? (a) : (b) )
#define MIN_OF(a, b)  ( (a) < (b) ? (a) : (b) )

#define U16_HIGH_U8(u16) ((uint8_t) (((u16) >> 8) & 0x00ff))
#define U16_LOW_U8(u16)  ((uint8_t) ((u16)       & 0x00ff))
#define U16_TO_U8S_BE(u16)  U16_HIGH_U8(u16), U16_LOW_U8(u16)
#define U16_TO_U8S_LE(u16)  U16_LOW_U8(u16), U16_HIGH_U8(u16)

#define U32_B1_U8(u32) ((uint8_t) (((u32) >> 24) & 0x000000ff)) // MSB
#define U32_B2_U8(u32) ((uint8_t) (((u32) >> 16) & 0x000000ff))
#define U32_B3_U8(u32) ((uint8_t) (((u32) >>  8) & 0x000000ff))
#define U32_B4_U8(u32) ((uint8_t) ((u32)        & 0x000000ff)) // LSB

#define U32_TO_U8S_BE(u32) U32_B1_U8(u32), U32_B2_U8(u32), U32_B3_U8(u32), U32_B4_U8(u32)
#define U32_TO_U8S_LE(u32) U32_B4_U8(u32), U32_B3_U8(u32), U32_B2_U8(u32), U32_B1_U8(u32)

//------------- Endian Conversion -------------//
#define ENDIAN_BE(u32) \
    (uint32_t) ( (((u32) & 0xFF) << 24) | (((u32) & 0xFF00) << 8) | (((u32) >> 8) & 0xFF00) | (((u32) >> 24) & 0xFF) )

#define ENDIAN_BE16(le16) ((uint16_t) ((U16_LOW_U8(le16) << 8) | U16_HIGH_U8(le16)) )

#ifndef __n2be_16
#define __n2be_16(u16)  ((uint16_t) ((U16_LOW_U8(u16) << 8) | U16_HIGH_U8(u16)) )
#define __be2n_16(u16)  __n2be_16(u16)
#endif

//--------------------------------------------------------------------+
// INLINE FUNCTION
//--------------------------------------------------------------------+
#define memclr_(buffer, size)  memset((buffer), 0, (size))


static inline uint8_t const * descriptor_next(uint8_t const p_desc[]) ATTR_ALWAYS_INLINE ATTR_PURE;
static inline uint8_t const * descriptor_next(uint8_t const p_desc[])
{
  return p_desc + p_desc[DESCRIPTOR_OFFSET_LENGTH];
}

static inline uint8_t descriptor_typeof(uint8_t const p_desc[]) ATTR_ALWAYS_INLINE ATTR_PURE;
static inline uint8_t descriptor_typeof(uint8_t const p_desc[])
{
  return p_desc[DESCRIPTOR_OFFSET_TYPE];
}

//------------- Conversion -------------//
/// form an uint32_t from 4 x uint8_t
static inline uint32_t u32_from_u8(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t u32_from_u8(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
  return ( ((uint32_t) b1) << 24) + ( ((uint32_t) b2) << 16) + ( ((uint32_t) b3) << 8) + b4;
}

static inline uint8_t u16_high_u8(uint16_t u16) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t u16_high_u8(uint16_t u16)
{
  return (uint8_t) ( ((uint16_t) (u16 >> 8)) & 0x00ff);
}

static inline uint8_t u16_low_u8(uint16_t u16) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t u16_low_u8(uint16_t u16)
{
  return (uint8_t) (u16 & 0x00ff);
}

static inline uint16_t u16_le2be(uint16_t u16) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint16_t u16_le2be(uint16_t u16)
{
  return ((uint16_t)(u16_low_u8(u16) << 8)) | u16_high_u8(u16);
}

//------------- Min -------------//
static inline uint8_t min8_of(uint8_t x, uint8_t y) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint8_t min8_of(uint8_t x, uint8_t y)
{
  return (x < y) ? x : y;
}

static inline uint16_t min16_of(uint16_t x, uint16_t y) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint16_t min16_of(uint16_t x, uint16_t y)
{
  return (x < y) ? x : y;
}

static inline uint32_t min32_of(uint32_t x, uint32_t y) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t min32_of(uint32_t x, uint32_t y)
{
  return (x < y) ? x : y;
}

//------------- Max -------------//
static inline uint32_t max32_of(uint32_t x, uint32_t y) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t max32_of(uint32_t x, uint32_t y)
{
  return (x > y) ? x : y;
}

static inline uint16_t max16_of(uint16_t x, uint16_t y) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint16_t max16_of(uint16_t x, uint16_t y)
{
  return (x > y) ? x : y;
}

//------------- Align -------------//
static inline uint32_t align32 (uint32_t value) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t align32 (uint32_t value)
{
	return (value & 0xFFFFFFE0UL);
}

static inline uint32_t align16 (uint32_t value) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t align16 (uint32_t value)
{
	return (value & 0xFFFFFFF0UL);
}

static inline uint32_t align_n (uint32_t alignment, uint32_t value) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t align_n (uint32_t alignment, uint32_t value)
{
	return value & ((uint32_t) ~(alignment-1));
}

static inline uint32_t align4k (uint32_t value) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t align4k (uint32_t value)
{
	return (value & 0xFFFFF000UL);
}

static inline uint32_t offset4k(uint32_t value) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t offset4k(uint32_t value)
{
	return (value & 0xFFFUL);
}

//------------- Mathematics -------------//
static inline uint32_t abs_of(int32_t value) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint32_t abs_of(int32_t value)
{
  return (value < 0) ? (-value) : value;
}


/// inclusive range checking
static inline bool is_in_range(uint32_t lower, uint32_t value, uint32_t upper) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline bool is_in_range(uint32_t lower, uint32_t value, uint32_t upper)
{
  return (lower <= value) && (value <= upper);
}

/// exclusive range checking
static inline bool is_in_range_exclusive(uint32_t lower, uint32_t value, uint32_t upper) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline bool is_in_range_exclusive(uint32_t lower, uint32_t value, uint32_t upper)
{
  return (lower < value) && (value < upper);
}

// TODO use clz
static inline uint8_t log2_of(uint32_t value) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint8_t log2_of(uint32_t value)
{
  uint8_t result = 0; // log2 of a value is its MSB's position

  while (value >>= 1)
  {
    result++;
  }
  return result;
}

// return the number of set bits in value
static inline uint8_t cardinality_of(uint32_t value) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint8_t cardinality_of(uint32_t value)
{
  // Brian Kernighan's method goes through as many iterations as there are set bits. So if we have a 32-bit word with only
  // the high bit set, then it will only go once through the loop
  // Published in 1988, the C Programming Language 2nd Ed. (by Brian W. Kernighan and Dennis M. Ritchie)
  // mentions this in exercise 2-9. On April 19, 2006 Don Knuth pointed out to me that this method
  // "was first published by Peter Wegner in CACM 3 (1960), 322. (Also discovered independently by Derrick Lehmer and
  // published in 1964 in a book edited by Beckenbach.)"
  uint8_t count;
  for (count = 0; value; count++)
  {
    value &= value - 1; // clear the least significant bit set
  }

  return count;
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMMON_H_ */

/**  @} */
