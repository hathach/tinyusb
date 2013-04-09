/*
 * osal.h
 *
 *  Created on: Jan 18, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.org)
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

#ifndef _TUSB_OSAL_H_
#define _TUSB_OSAL_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "tusb_option.h"

#define TUSB_OS_NONE     1
#define TUSB_OS_CMSIS    2
#define TUSB_OS_FREERTOS 3
#define TUSB_OS_UCOS     4

#ifndef _TEST_

#if TUSB_CFG_OS == TUSB_OS_NONE
  #include "osal_none.h"
#else
  #error TUSB_CFG_OS is not defined or OS is not supported yet
#endif

#else // OSAL API for cmock

#include "osal_common.h"

//------------- Tick -------------//
uint32_t osal_tick_get(void);

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
typedef uint32_t osal_task_t;
tusb_error_t osal_task_create(osal_task_t *task);

#define OSAL_TASK_DEF(name, code, stack_depth, prio) \
    osal_task_t name

#define OSAL_TASK_DECLARE(task_name) \
    void task_name(void)

#define OSAL_TASK_LOOP_BEGIN
#define OSAL_TASK_LOOP_END

//------------- Sub Task -------------//
#define OSAL_SUBTASK_INVOKED_AND_WAIT(subtask) TASK_ASSERT_STATUS(subtask)

#define TASK_RESTART

#define OSAL_SUBTASK_BEGIN
#define OSAL_SUBTASK_END \
  return TUSB_ERROR_NONE;

//------------- Task Assert -------------//
#define _TASK_ASSERT_ERROR_HANDLER(error, func_call)\
    func_call; TASK_RESTART; return error

#define TASK_ASSERT_STATUS(sts) \
    ASSERT_DEFINE_WITH_HANDLER(_TASK_ASSERT_ERROR_HANDLER, , tusb_error_t status = (tusb_error_t)(sts),\
                               TUSB_ERROR_NONE == status, (void) 0, "%s", TUSB_ErrorStr[status])

#define TASK_ASSERT_STATUS_WITH_HANDLER(sts, func_call) \
    ASSERT_DEFINE_WITH_HANDLER(_TASK_ASSERT_ERROR_HANDLER, func_call, tusb_error_t status = (tusb_error_t)(sts),\
                               TUSB_ERROR_NONE == status, (void) 0, "%s", TUSB_ErrorStr[status])

#define TASK_ASSERT(condition) \
    ASSERT_DEFINE_WITH_HANDLER(_TASK_ASSERT_ERROR_HANDLER, , ,\
                               condition, (void) 0, "%s", "evaluated to false")

#define TASK_ASSERT_WITH_HANDLER(condition, func_call) \
    ASSERT_DEFINE_WITH_HANDLER(_TASK_ASSERT_ERROR_HANDLER, func_call, ,\
                               condition, (void) 0, "%s", "evaluated to false")

//------------- Sub Task Assert (like Task but return error) -------------//
#define _SUBTASK_ASSERT_ERROR_HANDLER(error, func_call)\
    func_call; return error

#define SUBTASK_ASSERT_STATUS(sts) ASSERT_STATUS(sts)

#define SUBTASK_ASSERT_STATUS_WITH_HANDLER(sts, func_call) \
    ASSERT_DEFINE_WITH_HANDLER(_SUBTASK_ASSERT_ERROR_HANDLER, func_call, tusb_error_t status = (tusb_error_t)(sts),\
                               TUSB_ERROR_NONE == status, status, "%s", TUSB_ErrorStr[status])

#define SUBTASK_ASSERT(condition)  ASSERT(condition, TUSB_ERROR_OSAL_TASK_FAILED)

#define SUBTASK_ASSERT_WITH_HANDLER(condition, func_call) \
    ASSERT_DEFINE_WITH_HANDLER(_SUBTASK_ASSERT_ERROR_HANDLER, func_call, ,\
                               condition, TUSB_ERROR_OSAL_TASK_FAILED, "%s", "evaluated to false")


//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef volatile uint8_t osal_semaphore_t;
typedef osal_semaphore_t * osal_semaphore_handle_t;

#define OSAL_SEM_DEF(name)\
  osal_semaphore_t name

#define OSAL_SEM_REF(name)\
  &name

osal_semaphore_handle_t osal_semaphore_create(osal_semaphore_t * const sem);
void osal_semaphore_wait(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error);
tusb_error_t osal_semaphore_post(osal_semaphore_handle_t const sem_hdl);
void osal_semaphore_reset(osal_semaphore_handle_t const sem_hdl);

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
typedef struct{
           uint32_t * const buffer     ; ///< buffer pointer
           uint8_t const depth        ; ///< buffer size
  volatile uint8_t count          ; ///< bytes in fifo
  volatile uint8_t wr_idx       ; ///< write pointer
  volatile uint8_t rd_idx       ; ///< read pointer
} osal_queue_t;

typedef osal_queue_t * osal_queue_handle_t;

#define OSAL_QUEUE_DEF(name, queue_depth, type) \
  osal_queue_t name

osal_queue_handle_t  osal_queue_create  (osal_queue_t *p_queue);
void                 osal_queue_receive (osal_queue_handle_t const queue_hdl, uint32_t *p_data, uint32_t msec, tusb_error_t *p_error);
tusb_error_t         osal_queue_send    (osal_queue_handle_t const queue_hdl, const void * data);
void osal_queue_flush(osal_queue_handle_t const queue_hdl);

//--------------------------------------------------------------------+
// TICK API
//--------------------------------------------------------------------+
uint32_t osal_tick_get(void);
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_H_ */

/** @} */
