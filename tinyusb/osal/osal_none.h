/**************************************************************************/
/*!
    @file     osal_none.h
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
 * \defgroup Group_OSNone None OS
 *  @{ */

#ifndef _TUSB_OSAL_NONE_H_
#define _TUSB_OSAL_NONE_H_

#include "osal_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TICK API
//--------------------------------------------------------------------+
uint32_t tusb_tick_get(void);
#define osal_tick_get tusb_tick_get

//--------------------------------------------------------------------+
// TASK API
// NOTES: Each blocking OSAL_NONE services such as semaphore wait,
// queue receive embedded return statement, therefore local variable
// retain value before/after such services needed to declare as static
// OSAL_TASK_LOOP
// {
//   OSAL_TASK_LOOP_BEGIN
//
//   task body statements
//
//   OSAL_TASK_LOOP_ENG
// }
//--------------------------------------------------------------------+
#define OSAL_TASK_DEF(code, stack_depth, prio)
#define OSAL_TASK_REF
#define osal_task_create(x) TUSB_ERROR_NONE

#define OSAL_TASK_FUNCTION(task_func, p_para)   tusb_error_t task_func(void * p_para)

#define TASK_RESTART \
  state = 0

#define OSAL_TASK_LOOP_BEGIN \
  ATTR_UNUSED static uint32_t timeout = 0;\
  static uint16_t state = 0;\
  (void) timeout; /* timemout can possible unsued */ \
  switch(state) { \
    case 0: { \

#define OSAL_TASK_LOOP_END \
  default:\
    TASK_RESTART;\
  }}\
  return TUSB_ERROR_NONE;


#define osal_task_delay(msec) \
  do {\
    timeout = osal_tick_get();\
    state = __LINE__; case __LINE__:\
      if ( timeout + osal_tick_from_msec(msec) > osal_tick_get() ) /* time out */ \
        return TUSB_ERROR_OSAL_WAITING;\
  }while(0)

//--------------------------------------------------------------------+
// SUBTASK (a sub function that uses OS blocking services & called by a task
//--------------------------------------------------------------------+
#define OSAL_SUBTASK_INVOKED_AND_WAIT(subtask, status) \
    do {\
      state = __LINE__; case __LINE__:\
      {\
        status = subtask; /* invoke sub task */\
        if (TUSB_ERROR_OSAL_WAITING == status) /* sub task not finished -> continue waiting */\
          return TUSB_ERROR_OSAL_WAITING;\
      }\
    }while(0)

#define OSAL_SUBTASK_BEGIN  OSAL_TASK_LOOP_BEGIN
#define OSAL_SUBTASK_END    OSAL_TASK_LOOP_END

//------------- Sub Task Assert -------------//
#define SUBTASK_EXIT(error)  \
    do {\
      TASK_RESTART; return error;\
    }while(0)

#define _SUBTASK_ASSERT_ERROR_HANDLER(error, func_call) \
  func_call; TASK_RESTART; return error

#define SUBTASK_ASSERT_STATUS(sts) \
    ASSERT_DEFINE_WITH_HANDLER(_SUBTASK_ASSERT_ERROR_HANDLER, , tusb_error_t status = (tusb_error_t)(sts),\
                               TUSB_ERROR_NONE == status, status, "%s", TUSB_ErrorStr[status])

#define SUBTASK_ASSERT_STATUS_WITH_HANDLER(sts, func_call) \
    ASSERT_DEFINE_WITH_HANDLER(_SUBTASK_ASSERT_ERROR_HANDLER, func_call, tusb_error_t status = (tusb_error_t)(sts),\
                               TUSB_ERROR_NONE == status, status, "%s", TUSB_ErrorStr[status])

// TODO allow to specify error return
#define SUBTASK_ASSERT(condition)  \
    ASSERT_DEFINE_WITH_HANDLER(_SUBTASK_ASSERT_ERROR_HANDLER, , , \
                               (condition), TUSB_ERROR_OSAL_TASK_FAILED, "%s", "evaluated to false")
// TODO remove assert with handler by catching error in enum main task
#define SUBTASK_ASSERT_WITH_HANDLER(condition, func_call) \
    ASSERT_DEFINE_WITH_HANDLER(_SUBTASK_ASSERT_ERROR_HANDLER, func_call, ,\
                               condition, TUSB_ERROR_OSAL_TASK_FAILED, "%s", "evaluated to false")

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef volatile uint8_t osal_semaphore_t;
typedef osal_semaphore_t * osal_semaphore_handle_t;

#define OSAL_SEM_DEF(name) osal_semaphore_t name
#define OSAL_SEM_REF(name) &name

static inline osal_semaphore_handle_t osal_semaphore_create(osal_semaphore_t * p_sem) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline osal_semaphore_handle_t osal_semaphore_create(osal_semaphore_t * p_sem)
{
  (*p_sem) = 0; // TODO consider to have initial count parameter
  return (osal_semaphore_handle_t) p_sem;
}

static inline  tusb_error_t osal_semaphore_post(osal_semaphore_handle_t sem_hdl) ATTR_ALWAYS_INLINE;
static inline  tusb_error_t osal_semaphore_post(osal_semaphore_handle_t sem_hdl)
{
  (*sem_hdl)++;
  return TUSB_ERROR_NONE;
}

static inline void osal_semaphore_reset(osal_semaphore_handle_t sem_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_semaphore_reset(osal_semaphore_handle_t sem_hdl)
{
  (*sem_hdl) = 0;
}

#define osal_semaphore_wait(sem_hdl, msec, p_error) \
  do {\
    timeout = osal_tick_get();\
    state = __LINE__; case __LINE__:\
    if( *(sem_hdl) == 0 ) {\
      if ( ( ((uint32_t) (msec)) != OSAL_TIMEOUT_WAIT_FOREVER) && (timeout + osal_tick_from_msec(msec) <= osal_tick_get()) ) /* time out */ \
        *(p_error) = TUSB_ERROR_OSAL_TIMEOUT;\
      else\
        return TUSB_ERROR_OSAL_WAITING;\
    } else{\
      (*(sem_hdl))--; /*TODO mutex hal_interrupt_disable consideration*/\
      *(p_error) = TUSB_ERROR_NONE;\
    }\
  }while(0)

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef osal_semaphore_t        osal_mutex_t;
typedef osal_semaphore_handle_t osal_mutex_handle_t;

#define OSAL_MUTEX_DEF(name) osal_mutex_t name
#define OSAL_MUTEX_REF(name) &name

static inline osal_mutex_handle_t osal_mutex_create(osal_mutex_t * p_mutex) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline osal_mutex_handle_t osal_mutex_create(osal_mutex_t * p_mutex)
{
  (*p_mutex) = 1;
  return (osal_mutex_handle_t) p_mutex;
}

static inline  tusb_error_t osal_mutex_release(osal_mutex_handle_t mutex_hdl) ATTR_ALWAYS_INLINE;
static inline  tusb_error_t osal_mutex_release(osal_mutex_handle_t mutex_hdl)
{
  (*mutex_hdl) = 1; // mutex is a binary semaphore

  return TUSB_ERROR_NONE;
}

static inline void osal_mutex_reset(osal_mutex_handle_t mutex_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_mutex_reset(osal_mutex_handle_t mutex_hdl)
{
  (*mutex_hdl) = 1;
}

#define osal_mutex_wait osal_semaphore_wait

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
typedef struct{
           uint8_t* const buffer    ; ///< buffer pointer
           uint8_t  const depth     ; ///< max items
           uint8_t  const item_size ; ///< size of each item
  volatile uint8_t count            ; ///< number of items in queue
  volatile uint8_t wr_idx           ; ///< write pointer
  volatile uint8_t rd_idx           ; ///< read pointer
} osal_queue_t;

typedef osal_queue_t * osal_queue_handle_t;

// use to declare a queue, within the scope of tinyusb, should only use primitive type only
#define OSAL_QUEUE_DEF(name, queue_depth, type)\
  STATIC_ASSERT(queue_depth < 256, "OSAL Queue only support up to 255 depth");\
  type name##_buffer[queue_depth];\
  osal_queue_t name = {\
      .buffer    = (uint8_t*) name##_buffer,\
      .depth     = queue_depth,\
      .item_size = sizeof(type)\
  }

#define OSAL_QUEUE_REF(name)  (&name)

static inline osal_queue_handle_t osal_queue_create(osal_queue_t * const p_queue) ATTR_WARN_UNUSED_RESULT ATTR_ALWAYS_INLINE;
static inline osal_queue_handle_t osal_queue_create(osal_queue_t * const p_queue)
{
  p_queue->count = p_queue->wr_idx = p_queue->rd_idx = 0;
  return (osal_queue_handle_t) p_queue;
}
tusb_error_t osal_queue_send(osal_queue_handle_t const queue_hdl, void const * data);
static inline void osal_queue_flush(osal_queue_handle_t const queue_hdl) ATTR_ALWAYS_INLINE;
static inline void osal_queue_flush(osal_queue_handle_t const queue_hdl)
{
  queue_hdl->count = queue_hdl->rd_idx = queue_hdl->wr_idx = 0;
}

#define osal_queue_receive(queue_hdl, p_data, msec, p_error) \
  do {\
    timeout = osal_tick_get();\
    state = __LINE__; case __LINE__:\
    if( queue_hdl->count == 0 ) {\
      if ( (msec != OSAL_TIMEOUT_WAIT_FOREVER) && ( timeout + osal_tick_from_msec(msec) <= osal_tick_get() )) /* time out */ \
        *(p_error) = TUSB_ERROR_OSAL_TIMEOUT;\
      else\
        return TUSB_ERROR_OSAL_WAITING;\
    } else{\
      /*TODO mutex lock hal_interrupt_disable */\
      memcpy(p_data, queue_hdl->buffer + (queue_hdl->rd_idx * queue_hdl->item_size), queue_hdl->item_size);\
      queue_hdl->rd_idx = (queue_hdl->rd_idx + 1) % queue_hdl->depth;\
      queue_hdl->count--;\
      /*TODO mutex unlock hal_interrupt_enable */\
      *(p_error) = TUSB_ERROR_NONE;\
    }\
  }while(0)

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_NONE_H_ */

/** @} */
