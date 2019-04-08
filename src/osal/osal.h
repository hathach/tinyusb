/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_OSAL_H_
#define _TUSB_OSAL_H_

#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup group_osal
 *  @{ */

#include "common/tusb_common.h"

enum
{
  OSAL_TIMEOUT_NOTIMEOUT    = 0,      // return immediately
  OSAL_TIMEOUT_NORMAL       = 10,     // default timeout
  OSAL_TIMEOUT_WAIT_FOREVER = 0xFFFFFFFFUL
};

#define OSAL_TIMEOUT_CONTROL_XFER  OSAL_TIMEOUT_WAIT_FOREVER

typedef void (*osal_task_func_t)( void * );

//--------------------------------------------------------------------+
// OSAL Porting API
//--------------------------------------------------------------------+
#if 0
void osal_task_delay(uint32_t msec);

//------------- Semaphore -------------//
osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef);
bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr);
bool osal_semaphore_wait (osal_semaphore_t sem_hdl, uint32_t msec);

void osal_semaphore_reset(osal_semaphore_t sem_hdl); // TODO removed

//------------- Mutex -------------//
osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef);
bool osal_mutex_lock (osal_mutex_t sem_hdl, uint32_t msec);
bool osal_mutex_unlock(osal_mutex_t mutex_hdl);

//------------- Queue -------------//
osal_queue_t osal_queue_create(osal_queue_def_t* qdef);
bool osal_queue_receive(osal_queue_t const qhdl, void* data);
bool osal_queue_send(osal_queue_t const qhdl, void const * data, bool in_isr);
#endif

#if CFG_TUSB_OS == OPT_OS_NONE
  #include "osal_none.h"
#else
  #if CFG_TUSB_OS == OPT_OS_FREERTOS
    #include "osal_freertos.h"
  #elif CFG_TUSB_OS == OPT_OS_MYNEWT
    #include "osal_mynewt.h"
  #else
    #error CFG_TUSB_OS is not defined or OS is not supported yet
  #endif

  // TODO remove subtask related macros later

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
