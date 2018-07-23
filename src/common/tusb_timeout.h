/**************************************************************************/
/*!
    @file     tusb_timeout.h
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

/** \ingroup Group_Common Common Files
 *  \defgroup Group_TimeoutTimer timeout timer
 *  @{ */


#ifndef _TUSB_TIMEOUT_H_
#define _TUSB_TIMEOUT_H_

#include "tusb_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t start;
  uint32_t interval;
}tu_timeout_t;

extern uint32_t tusb_hal_millis(void);

static inline void tu_timeout_set(tu_timeout_t* tt, uint32_t msec)
{
  tt->interval = msec;
  tt->start    = tusb_hal_millis();
}

static inline bool tu_timeout_expired(tu_timeout_t* tt)
{
  return ( tusb_hal_millis() - tt->start ) >= tt->interval;
}

// For used with periodic event to prevent drift
static inline void tu_timeout_reset(tu_timeout_t* tt)
{
  tt->start += tt->interval;
}

static inline void tu_timeout_restart(tu_timeout_t* tt)
{
  tt->start = tusb_hal_millis();
}


static inline void tu_timeout_wait(uint32_t msec)
{
  tu_timeout_t tt;
  tu_timeout_set(&tt, msec);

  // blocking delay
  while ( !tu_timeout_expired(&tt) ) { }
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TIMEOUT_H_ */

/** @} */
