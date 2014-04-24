/**************************************************************************/
/*!
    @file     osal_cmsis_rtx.h
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup group_osal
 *  @{
 *  \defgroup Group_CMSISRTX CMSIS-RTOS RTX
 *  @{ */

#ifndef _TUSB_OSAL_CMSIS_RTX_H_
#define _TUSB_OSAL_CMSIS_RTX_H_

#include "osal_common.h"
#include "cmsis_os.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TICK API
//--------------------------------------------------------------------+
#define osal_tick_get osKernelSysTick

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
#define OSAL_TASK_FUNCTION(task_func, p_para) void task_func(void const * p_para)

typedef osThreadDef_t osal_task_t;

#define OSAL_TASK_DEF(task_code, task_stack_depth, task_prio) \
  osThreadDef(task_code, (osPriority) task_prio, 1, task_stack_depth*4) // stack depth is in bytes

#define OSAL_TASK_REF(task_name)    osThread(task_name)

static inline tusb_error_t osal_task_create(const osal_task_t *task) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline tusb_error_t osal_task_create(const osal_task_t *task)
{
  return ( osThreadCreate(task, NULL) != NULL ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TASK_CREATE_FAILED;
}

#define osal_task_delay(msec)   osDelay(msec)

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef struct {
  uint32_t sem_cb[2];
}osal_semaphore_t;

typedef osSemaphoreId osal_semaphore_handle_t;

#define OSAL_SEM_DEF(name)  osal_semaphore_t name
#define OSAL_SEM_REF(name)  (&name)

static inline osal_semaphore_handle_t osal_semaphore_create(osal_semaphore_t * p_sem) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline osal_semaphore_handle_t osal_semaphore_create(osal_semaphore_t * p_sem)
{
  return osSemaphoreCreate( &(osSemaphoreDef_t) { p_sem }, 0 );
}

// TODO add timeout (with instant return from ISR option) for semaphore post & queue send
static inline  tusb_error_t osal_semaphore_post(osal_semaphore_handle_t const sem_hdl) ATTR_ALWAYS_INLINE;
static inline  tusb_error_t osal_semaphore_post(osal_semaphore_handle_t const sem_hdl)
{
  return (osSemaphoreRelease(sem_hdl) == osOK) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_SEMAPHORE_FAILED;
}

static inline void osal_semaphore_wait(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error) ATTR_ALWAYS_INLINE;
static inline void osal_semaphore_wait(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error)
{
  (*p_error) = ( osSemaphoreWait(sem_hdl, msec) > 0 ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

static inline void osal_semaphore_reset(osal_semaphore_handle_t const sem_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_semaphore_reset(osal_semaphore_handle_t const sem_hdl)
{
  osSemaphoreWait(sem_hdl, 0); // instant return without putting caller to suspend
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef struct {
  uint32_t mutex_cb[3];
}osal_mutex_t;

typedef osMutexId osal_mutex_handle_t;

#define OSAL_MUTEX_DEF(name)  osal_mutex_t name
#define OSAL_MUTEX_REF(name)  (&name)

static inline osal_mutex_handle_t osal_mutex_create(osal_mutex_t * p_mutex) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline osal_mutex_handle_t osal_mutex_create(osal_mutex_t * p_mutex)
{
  return osMutexCreate( &(osMutexDef_t) {p_mutex} );
}

static inline  tusb_error_t osal_mutex_release(osal_mutex_handle_t const mutex_hdl) ATTR_ALWAYS_INLINE;
static inline  tusb_error_t osal_mutex_release(osal_mutex_handle_t const mutex_hdl)
{
  return (osMutexRelease(mutex_hdl) == osOK) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_MUTEX_FAILED;
}

static inline void osal_mutex_wait(osal_mutex_handle_t const mutex_hdl, uint32_t msec, tusb_error_t *p_error) ATTR_ALWAYS_INLINE;
static inline void osal_mutex_wait(osal_mutex_handle_t const mutex_hdl, uint32_t msec, tusb_error_t *p_error)
{
  (*p_error) = ( osMutexWait(mutex_hdl, msec) == osOK ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

static inline void osal_mutex_reset(osal_mutex_handle_t const mutex_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_mutex_reset(osal_mutex_handle_t const mutex_hdl)
{
  osMutexRelease(mutex_hdl);
}

//--------------------------------------------------------------------+
// QUEUE API (for RTX: osMailQId is osMessageQDef_t.pool)
//--------------------------------------------------------------------+
typedef osMailQDef_t osal_queue_t;
typedef osal_queue_t * osal_queue_handle_t;

#define OSAL_QUEUE_DEF(name, queue_depth, type)\
  osMailQDef(name, queue_depth, type);

#define OSAL_QUEUE_REF(name)  ((osal_queue_t*) osMailQ(name))

static inline osal_queue_handle_t osal_queue_create(osal_queue_t * const p_queue) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline osal_queue_handle_t osal_queue_create(osal_queue_t * const p_queue)
{
  return (NULL != osMailCreate( (osMailQDef_t const *) p_queue, NULL)) ?  p_queue : NULL;
}

static inline void osal_queue_receive (osal_queue_handle_t const queue_hdl, void *p_data, uint32_t msec, tusb_error_t *p_error) ATTR_ALWAYS_INLINE;
static inline void osal_queue_receive (osal_queue_handle_t const queue_hdl, void *p_data, uint32_t msec, tusb_error_t *p_error)
{
  osEvent evt = osMailGet(queue_hdl->pool, msec);

  if (evt.status != osEventMail)
  {
    (*p_error) = TUSB_ERROR_OSAL_TIMEOUT;
  }else
  {
    memcpy(p_data, evt.value.p, queue_hdl->item_sz);
    osMailFree(queue_hdl->pool, evt.value.p);
    (*p_error) = TUSB_ERROR_NONE;
  }
}

static inline tusb_error_t osal_queue_send(osal_queue_handle_t const queue_hdl, void const * data) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline tusb_error_t osal_queue_send(osal_queue_handle_t const queue_hdl, void const * data)
{
  void *p_buf = osMailAlloc(queue_hdl->pool, 0); // instantly return in case of sending within ISR (mostly used)
  if (p_buf == NULL) return TUSB_ERROR_OSAL_QUEUE_FAILED;

  memcpy(p_buf, data, queue_hdl->item_sz);
  osMailPut(queue_hdl->pool, p_buf);
  return TUSB_ERROR_NONE;
}

static inline void osal_queue_flush(osal_queue_handle_t const queue_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_queue_flush(osal_queue_handle_t const queue_hdl)
{
  osEvent  evt;

  while( (evt = osMailGet(queue_hdl->pool, 0) ).status == osEventMail )
  {
    osMailFree(queue_hdl->pool, evt.value.p);
  }
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_CMSIS_RTX_H_ */

/** @} */
/** @} */
