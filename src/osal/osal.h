/**************************************************************************/
/*!
    @file     osal.h
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

#ifndef _TUSB_OSAL_H_
#define _TUSB_OSAL_H_

#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup group_osal
 *  @{ */

#include "tusb_option.h"
#include "common/tusb_common.h"

enum
{
  OSAL_TIMEOUT_NOTIMEOUT    = 0,      // return immediately
  OSAL_TIMEOUT_NORMAL       = 10,     // default timeout
  OSAL_TIMEOUT_WAIT_FOREVER = 0xFFFFFFFFUL
};

#define OSAL_TIMEOUT_CONTROL_XFER  OSAL_TIMEOUT_WAIT_FOREVER

typedef void (*osal_task_func_t)( void * );

#if CFG_TUSB_OS == OPT_OS_NONE
  #include "osal_none.h"

#else
   #if CFG_TUSB_OS == OPT_OS_FREERTOS
    #include "osal_freeRTOS.h"
  #else
    #error CFG_TUSB_OS is not defined or OS is not supported yet
  #endif

  #define OSAL_TASK_BEGIN while(1) {
  #define OSAL_TASK_END   }

  //------------- Sub Task -------------//
  #define OSAL_SUBTASK_BEGIN
  #define OSAL_SUBTASK_END                    return TUSB_ERROR_NONE;

  #define STASK_RETURN(_error)                return _error;
  #define STASK_INVOKE(_subtask, _status)     (_status) = _subtask

  //------------- Sub Task Assert -------------//
  #define STASK_ASSERT_ERR(_err)              TU_VERIFY_ERR(_err)
  #define STASK_ASSERT_ERR_HDLR(_err, _func)  TU_VERIFY_ERR_HDLR(_err, _func)

  #define STASK_ASSERT(_cond)                 TU_VERIFY(_cond, TUSB_ERROR_OSAL_TASK_FAILED)
  #define STASK_ASSERT_HDLR(_cond, _func)     TU_VERIFY_HDLR(_cond, _func)
#endif

#ifdef __cplusplus
 }
#endif

/** @} */

#endif /* _TUSB_OSAL_H_ */
