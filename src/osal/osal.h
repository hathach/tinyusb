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

  #define OSAL_TASK_BEGIN
  #define OSAL_TASK_END

#else
  /* RTOS Porting API
   *
   * uint32_t tusb_hal_millis(void)
   *
   * Task
   *    osal_task_def_t
   *    bool osal_task_create(osal_task_def_t* taskdef)
   *    void osal_task_delay(uint32_t msec)
   *
   * Queue
   *     osal_queue_def_t, osal_queue_t
   *     osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
   *     osal_queue_receive (osal_queue_t const queue_hdl, void *p_data, uint32_t msec, uint32_t *p_error)
   *     bool osal_queue_send(osal_queue_t const queue_hdl, void const * data, bool in_isr)
   *     osal_queue_reset()
   *
   * Semaphore
   *    osal_semaphore_def_t, osal_semaphore_t
   *    osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
   *    bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
   *    void osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec, uint32_t *p_error)
   *    void osal_semaphore_reset(osal_semaphore_t const sem_hdl)
   *
   * Mutex
   *    osal_mutex_t
   *    osal_mutex_create(osal_mutex_def_t* mdef)
   *    bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
   *    void osal_mutex_lock(osal_mutex_t mutex_hdl, uint32_t msec, uint32_t *p_error)
   */

  #if CFG_TUSB_OS == OPT_OS_FREERTOS
    #include "osal_freertos.h"
  #elif CFG_TUSB_OS == OPT_OS_MYNEWT
    #include "osal_mynewt.h"
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
