/**************************************************************************/
/*!
    @file     verify.h
    @author   hathach

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, hathach (tinyusb.org)
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/
#ifndef VERIFY_H_
#define VERIFY_H_

#include "tusb_option.h"
#include <stdbool.h>
#include <stdint.h>
#include "compiler/compiler.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Compile-time Verify
//--------------------------------------------------------------------+
#if defined __COUNTER__ && __COUNTER__ != __COUNTER__
  #define _VERIFY_COUNTER __COUNTER__
#else
  #define _VERIFY_COUNTER __LINE__
#endif

#define VERIFY_STATIC(const_expr) enum { XSTRING_CONCAT_(static_verify_, _VERIFY_COUNTER) = 1/(!!(const_expr)) }

//--------------------------------------------------------------------+
// VERIFY Helper
//--------------------------------------------------------------------+
#if TUSB_CFG_DEBUG >= 1
//  #define VERIFY_MESS(format, ...) cprintf("[%08ld] %s: %d: verify failed\n", get_millis(), __func__, __LINE__)
  #define VERIFY_MESS(_status)   printf("%s: %d: verify failed, error = %s\n", __PRETTY_FUNCTION__, __LINE__, TUSB_ErrorStr[_status]);
#else
  #define VERIFY_MESS(_status)
#endif

/**
 * Helper to implement optional parameter for VERIFY Macro family
 */
#define GET_3RD_ARG(arg1, arg2, arg3, ...)        arg3
#define GET_4TH_ARG(arg1, arg2, arg3, arg4, ...)  arg4

/*------------------------------------------------------------------*/
/* VERIFY STATUS
 * - VERIFY_STS_1ARGS : return status of condition if failed
 * - VERIFY_STS_2ARGS : return provided status code if failed
 *------------------------------------------------------------------*/
#define VERIFY_STS_1ARGS(sts)             \
    do {                                  \
      uint32_t _status = (uint32_t)(sts); \
      if ( 0 != _status ) { VERIFY_MESS(_status) return _status; } \
    } while(0)

#define VERIFY_STS_2ARGS(sts, _error)     \
    do {                                  \
      uint32_t _status = (uint32_t)(sts); \
      if ( 0 != _status ) { VERIFY_MESS(_status) return _error; }\
    } while(0)

/**
 * Check if status is success (zero), otherwise return
 * - status value if called with 1 parameter e.g VERIFY_STATUS(status)
 * - 2 parameter if called with 2 parameters e.g VERIFY_STATUS(status, errorcode)
 */
#define VERIFY_STATUS(...)  GET_3RD_ARG(__VA_ARGS__, VERIFY_STS_2ARGS, VERIFY_STS_1ARGS)(__VA_ARGS__)

/*------------------------------------------------------------------*/
/* VERIFY STATUS WITH HANDLER
 * - VERIFY_STS_HDLR_2ARGS : execute handler, return status if failed
 * - VERIFY_STS_HDLR_3ARGS : execute handler, return provided error if failed
 *------------------------------------------------------------------*/
#define VERIFY_STS_HDLR_2ARGS(sts, _handler)          \
    do {                                              \
      uint32_t _status = (uint32_t)(sts);             \
      if ( 0 != _status ) { VERIFY_MESS(_status) _handler; return _status; }\
    } while(0)

#define VERIFY_STS_HDLR_3ARGS(sts, _handler, _error)  \
    do {                                              \
      uint32_t _status = (uint32_t)(sts);             \
      if ( 0 != _status ) { VERIFY_MESS(_status) _handler; return _error; }\
    } while(0)

#define VERIFY_STATUS_HDLR(...)  GET_4TH_ARG(__VA_ARGS__, VERIFY_STS_HDLR_3ARGS, VERIFY_STS_HDLR_2ARGS)(__VA_ARGS__)

/*------------------------------------------------------------------*/
/* VERIFY
 * - VERIFY_1ARGS : return false if failed
 * - VERIFY_2ARGS : return provided value if failed
 *------------------------------------------------------------------*/
#define VERIFY_1ARGS(cond)            do { if (!(cond)) { hal_debugger_breakpoint(); return false; } } while(0)
#define VERIFY_2ARGS(cond, _error)    do { if (!(cond)) { hal_debugger_breakpoint(); return _error;} } while(0)

/**
 * Check if condition is success (true), otherwise return
 * - false value if called with 1 parameter e.g VERIFY(condition)
 * - 2 parameter if called with 2 parameters e.g VERIFY(condition, errorcode)
 */
#define VERIFY(...)  GET_3RD_ARG(__VA_ARGS__, VERIFY_2ARGS, VERIFY_1ARGS)(__VA_ARGS__)

/*------------------------------------------------------------------*/
/* VERIFY WITH HANDLER
 * - VERIFY_HDLR_2ARGS : execute handler, return false if failed
 * - VERIFY_HDLR_3ARGS : execute handler, return provided error if failed
 *------------------------------------------------------------------*/
#define VERIFY_HDLR_2ARGS(cond, _handler)         \
    do { if ( !(cond) ) { _handler; return false; } } while(0)

#define VERIFY_HDLR_3ARGS(cond, _handler, _error) \
    do { if ( !(cond) ) { _handler; return _error; } } while(0)

#define VERIFY_HDLR(...) GET_4TH_ARG(__VA_ARGS__, VERIFY_HDLR_3ARGS, VERIFY_HDLR_2ARGS)(__VA_ARGS__)

#ifdef __cplusplus
 }
#endif

#endif /* VERIFY_H_ */
