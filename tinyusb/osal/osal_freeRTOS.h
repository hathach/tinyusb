/*
 * osal_freeRTOS.h
 *
 *  Created on: Feb 2, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
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

#ifndef _TUSB_OSAL_FREERTOS_H_
#define _TUSB_OSAL_FREERTOS_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "osal_common.h"

//------------- FreeRTOS Headers -------------//
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

//--------------------------------------------------------------------+
// TICK API
//--------------------------------------------------------------------+
#define osal_tick_get xTaskGetTickCount

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
#define OSAL_TASK_LOOP_BEGIN \
  while(1) {

#define OSAL_TASK_LOOP_END \
  }

#define TASK_ASSERT(condition)
#define TASK_ASSERT_STATUS(sts)

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
#define OSAL_SEM_DEF(name)
typedef xSemaphoreHandle osal_semaphore_handle_t;

// create FreeRTOS binary semaphore with zero as init value TODO: omit semaphore take from vSemaphoreCreateBinary API, should double checks this
#define osal_semaphore_create(x) \
  xQueueGenericCreate( ( unsigned portBASE_TYPE ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE )

static inline  tusb_error_t osal_semaphore_post(osal_semaphore_handle_t const sem_hdl) ATTR_ALWAYS_INLINE;
static inline  tusb_error_t osal_semaphore_post(osal_semaphore_handle_t const sem_hdl)
{
  portBASE_TYPE taskWaken;
  return (xSemaphoreGiveFromISR(sem_hdl, &taskWaken) == pdTRUE) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_SEMAPHORE_FAILED;
}

static inline void osal_semaphore_wait(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error) ATTR_ALWAYS_INLINE;
static inline void osal_semaphore_wait(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error)
{
  (*p_error) = ( xSemaphoreTake(sem_hdl, osal_tick_from_msec(msec)) == pdPASS ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
typedef struct{
  uint8_t const depth        ; ///< buffer size
} osal_queue_t;

typedef xQueueHandle osal_queue_handle_t;

#define OSAL_QUEUE_DEF(name, queue_depth, type)\
  osal_queue_t name = {\
      .depth   = queue_depth\
  }

#define osal_queue_create(p_queue) \
  xQueueCreate((p_queue)->depth, sizeof(uint32_t))

static inline void osal_queue_receive (osal_queue_handle_t const queue_hdl, uint32_t *p_data, uint32_t msec, tusb_error_t *p_error) ATTR_ALWAYS_INLINE;
static inline void osal_queue_receive (osal_queue_handle_t const queue_hdl, uint32_t *p_data, uint32_t msec, tusb_error_t *p_error)
{
  (*p_error) = ( xQueueReceive(queue_hdl, p_data, osal_tick_from_msec(msec)) == pdPASS ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT;
}

static inline tusb_error_t osal_queue_send(osal_queue_handle_t const queue_hdl, uint32_t data) ATTR_ALWAYS_INLINE;
static inline tusb_error_t osal_queue_send(osal_queue_handle_t const queue_hdl, uint32_t data)
{
  portBASE_TYPE taskWaken;
  return ( xQueueSendFromISR(queue_hdl, &data, &taskWaken) == pdTRUE ) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_QUEUE_FAILED;
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_FREERTOS_H_ */

/** @} */
