/**************************************************************************/
/*!
    @file     osal_freeRTOS.h
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
 * @{
 *  \defgroup Group_FreeRTOS  FreeRTOS
 *  @{ */

#ifndef _TUSB_OSAL_FREERTOS_H_
#define _TUSB_OSAL_FREERTOS_H_

#include "osal_common.h"

//------------- FreeRTOS Headers -------------//
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// TICK API
//--------------------------------------------------------------------+
#define osal_tick_get xTaskGetTickCount

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
#define OSAL_TASK_FUNCTION portTASK_FUNCTION

typedef struct {
  char const * name;
  pdTASK_CODE code;
  unsigned portSHORT stack_depth;
  unsigned portBASE_TYPE prio;
} osal_task_t;

#define OSAL_TASK_DEF(task_code, task_stack_depth, task_prio) \
  osal_task_t osal_task_def_##task_code = {\
      .name        = #task_code       , \
      .code        = task_code        , \
      .stack_depth = task_stack_depth , \
      .prio        = task_prio          \
  }

#define OSAL_TASK_REF(name)   (&osal_task_def_##name)

static inline tusb_error_t osal_task_create(osal_task_t *task) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline tusb_error_t osal_task_create(osal_task_t *task)
{
  return pdPASS == xTaskCreate(task->code, (signed portCHAR const *) task->name, task->stack_depth, NULL, task->prio, NULL) ?
    TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TASK_CREATE_FAILED;
}

static inline void osal_task_delay(uint32_t msec) ATTR_ALWAYS_INLINE;
static inline void osal_task_delay(uint32_t msec)
{
  vTaskDelay( (TUSB_CFG_TICKS_HZ * msec) / 1000 );
}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
#define OSAL_SEM_DEF(name)
#define OSAL_SEM_REF(name)
typedef xSemaphoreHandle osal_semaphore_handle_t;

// create FreeRTOS binary semaphore with zero as init value TODO: omit semaphore take from vSemaphoreCreateBinary API, should double checks this
#define osal_semaphore_create(x) xSemaphoreCreateBinary()

// TODO add timeout (with instant return from ISR option) for semaphore post & queue send
static inline  tusb_error_t osal_semaphore_post(osal_semaphore_handle_t const sem_hdl) ATTR_ALWAYS_INLINE;
static inline  tusb_error_t osal_semaphore_post(osal_semaphore_handle_t const sem_hdl)
{
  return (xSemaphoreGive(sem_hdl) == pdTRUE) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_SEMAPHORE_FAILED;
}

static inline void osal_semaphore_wait(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error) ATTR_ALWAYS_INLINE;
static inline void osal_semaphore_wait(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? portMAX_DELAY : osal_tick_from_msec(msec);
  (*p_error) = ( xSemaphoreTake(sem_hdl, ticks) == pdPASS ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

static inline void osal_semaphore_reset(osal_semaphore_handle_t const sem_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_semaphore_reset(osal_semaphore_handle_t const sem_hdl)
{
  (void) xSemaphoreTake(sem_hdl, 0);
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
#define OSAL_MUTEX_DEF  OSAL_SEM_DEF
#define OSAL_MUTEX_REF  OSAL_SEM_REF
typedef xSemaphoreHandle osal_mutex_handle_t;

#define osal_mutex_create(x) xSemaphoreCreateMutex()

static inline  tusb_error_t osal_mutex_release(osal_mutex_handle_t const mutex_hdl) ATTR_ALWAYS_INLINE;
static inline  tusb_error_t osal_mutex_release(osal_mutex_handle_t const mutex_hdl)
{
  return (xSemaphoreGive(mutex_hdl) == pdPASS) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_MUTEX_FAILED;
}

static inline void osal_mutex_wait(osal_mutex_handle_t const mutex_hdl, uint32_t msec, tusb_error_t *p_error) ATTR_ALWAYS_INLINE;
static inline void osal_mutex_wait(osal_mutex_handle_t const mutex_hdl, uint32_t msec, tusb_error_t *p_error)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? portMAX_DELAY : osal_tick_from_msec(msec);
  (*p_error) = ( xSemaphoreTake(mutex_hdl, ticks) == pdPASS ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

static inline void osal_mutex_reset(osal_mutex_handle_t const mutex_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_mutex_reset(osal_mutex_handle_t const mutex_hdl)
{
  xSemaphoreGive(mutex_hdl);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
typedef struct{
  uint8_t const depth;     ///< buffer size
  uint8_t const item_size; ///< size of each item
} osal_queue_t;

typedef xQueueHandle osal_queue_handle_t;

#define OSAL_QUEUE_DEF(name, queue_depth, type)\
  osal_queue_t name = {\
      .depth     = queue_depth,\
      .item_size = sizeof(type)\
  }

#define OSAL_QUEUE_REF(name)    (&name)

#define osal_queue_create(p_queue) xQueueCreate((p_queue)->depth, (p_queue)->item_size)

static inline void osal_queue_receive (osal_queue_handle_t const queue_hdl, void *p_data, uint32_t msec, tusb_error_t *p_error) ATTR_ALWAYS_INLINE;
static inline void osal_queue_receive (osal_queue_handle_t const queue_hdl, void *p_data, uint32_t msec, tusb_error_t *p_error)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? portMAX_DELAY : osal_tick_from_msec(msec);
  (*p_error) = ( xQueueReceive(queue_hdl, p_data, ticks) == pdPASS ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

static inline tusb_error_t osal_queue_send(osal_queue_handle_t const queue_hdl, void const * data) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline tusb_error_t osal_queue_send(osal_queue_handle_t const queue_hdl, void const * data)
{
  return ( xQueueSend(queue_hdl, data, 0) == pdTRUE ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_QUEUE_FAILED;
}

static inline void osal_queue_flush(osal_queue_handle_t const queue_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_queue_flush(osal_queue_handle_t const queue_hdl)
{
  xQueueReset(queue_hdl);
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_FREERTOS_H_ */

/** @} */
/** @} */

