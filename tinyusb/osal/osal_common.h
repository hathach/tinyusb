/**************************************************************************/
/*!
    @file     osal_common.h
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

/** \ingroup group_osal
 *  @{ */

#ifndef _TUSB_OSAL_COMMON_H_
#define _TUSB_OSAL_COMMON_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/common.h"

#ifdef __CC_ARM
#pragma diag_suppress 66 // Suppress Keil warnings #66-D: enumeration value is out of "int" range
#endif

enum
{
  OSAL_TIMEOUT_NOTIMEOUT    = 0,  // for use within ISR,  return immediately
  OSAL_TIMEOUT_NORMAL       = 10*5, // default is 10 msec, FIXME [CMSIS-RTX] easily timeout with 10 msec
  OSAL_TIMEOUT_WAIT_FOREVER = 0xFFFFFFFF
};

#ifdef __CC_ARM
#pragma diag_default 66 // return Keil 66 to normal severity
#endif

static inline uint32_t osal_tick_from_msec(uint32_t msec) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint32_t osal_tick_from_msec(uint32_t msec)
{
  return  (msec * TUSB_CFG_TICKS_HZ)/1000;
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_COMMON_H_ */

/** @} */
