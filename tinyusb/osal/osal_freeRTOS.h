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
static inline bool osal_task_create(osal_func_t code, const char* name, uint32_t stack_size, void* param, uint32_t prio, osal_task_t* task_hdl)
{
  return xTaskCreate(code, (const signed char*) name, stack_size, param, prio, task_hdl);
}

static inline void osal_task_delay(uint32_t msec)
{
  vTaskDelay( (TUSB_CFG_TICKS_HZ * msec) / 1000 );
}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef xSemaphoreHandle osal_semaphore_t;

// create FreeRTOS binary semaphore with zero as init value TODO: omit semaphore take from vSemaphoreCreateBinary API, should double checks this
//#define osal_semaphore_create(x) xSemaphoreCreateBinary()

static inline osal_semaphore_t osal_semaphore_create(uint32_t max, uint32_t init)
{
  return xSemaphoreCreateCounting(max, init);
}

// TODO add timeout (with instant return from ISR option) for semaphore post & queue send
static inline  tusb_error_t osal_semaphore_post(osal_semaphore_t sem_hdl)
{
  return (xSemaphoreGive(sem_hdl) == pdTRUE) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_SEMAPHORE_FAILED;
}

static inline void osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec, tusb_error_t *p_error)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? portMAX_DELAY : osal_tick_from_msec(msec);
  (*p_error) = ( xSemaphoreTake(sem_hdl, ticks) == pdPASS ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

static inline void osal_semaphore_reset(osal_semaphore_t const sem_hdl)
{
  (void) xSemaphoreTake(sem_hdl, 0);
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef xSemaphoreHandle osal_mutex_t;

#define osal_mutex_create(x) xSemaphoreCreateMutex()

static inline  tusb_error_t osal_mutex_release(osal_mutex_t mutex_hdl)
{
  return osal_semaphore_post(mutex_hdl);
}

static inline void osal_mutex_wait(osal_mutex_t mutex_hdl, uint32_t msec, tusb_error_t *p_error)
{
  osal_semaphore_wait(mutex_hdl, msec, p_error);
}

// TOOD remove
static inline void osal_mutex_reset(osal_mutex_t mutex_hdl)
{
  xSemaphoreGive(mutex_hdl);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
typedef xQueueHandle osal_queue_t;

static inline osal_queue_t osal_queue_create(uint32_t depth, uint32_t item_size)
{
  return xQueueCreate(depth, item_size);
}

static inline void osal_queue_receive (osal_queue_t const queue_hdl, void *p_data, uint32_t msec, tusb_error_t *p_error)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? portMAX_DELAY : osal_tick_from_msec(msec);

  portBASE_TYPE result = (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) ?  xQueueReceiveFromISR(queue_hdl, p_data, NULL) : xQueueReceive(queue_hdl, p_data, ticks);
  (*p_error) = ( result == pdPASS ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

static inline bool osal_queue_send(osal_queue_t const queue_hdl, void const * data)
{
  if (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)
  {
    return xQueueSendToFrontFromISR(queue_hdl, data, NULL);
  }else
  {
    return xQueueSendToFront(queue_hdl, data, 0);
  }
}

static inline void osal_queue_flush(osal_queue_t const queue_hdl)
{
  xQueueReset(queue_hdl);
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_FREERTOS_H_ */

/** @} */
/** @} */

