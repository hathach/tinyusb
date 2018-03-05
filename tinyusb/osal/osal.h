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

/** \defgroup group_supported_os Supported RTOS
 * \brief \ref TUSB_CFG_OS must be defined to one of these
 *  @{ */
#define TUSB_OS_NONE       1 ///< No RTOS is used
#define TUSB_OS_FREERTOS   2 ///< FreeRTOS is used
/** @} */

#include "tusb_option.h"
#include "common/common.h"


#if TUSB_CFG_OS == TUSB_OS_NONE
  #include "osal_none.h"

#else
   #if TUSB_CFG_OS == TUSB_OS_FREERTOS
    #include "osal_freeRTOS.h"
  #else
    #error TUSB_CFG_OS is not defined or OS is not supported yet
  #endif

  #define OSAL_TASK_BEGIN while(1) {
  #define OSAL_TASK_END   }

  //------------- Sub Task -------------//
  #define OSAL_SUBTASK_BEGIN
  #define OSAL_SUBTASK_END      return TUSB_ERROR_NONE;

  #define SUBTASK_RETURN(error)   return error;
  #define OSAL_SUBTASK_INVOKED_AND_WAIT(subtask, status) status = subtask

  //------------- Sub Task Assert -------------//
  #define SUBTASK_ASSERT_STATUS(sts) VERIFY_STATUS(sts)
  #define SUBTASK_ASSERT(condition)  VERIFY(condition, TUSB_ERROR_OSAL_TASK_FAILED)

  #define SUBTASK_ASSERT_STATUS_WITH_HANDLER(sts, func_call)  VERIFY_STATUS_HDLR(sts, func_call)
  #define SUBTASK_ASSERT_WITH_HANDLER(condition, func_call)   VERIFY_HDLR(condition, func_call)
#endif

#ifdef __cplusplus
 }
#endif

/** @} */

#endif /* _TUSB_OSAL_H_ */
