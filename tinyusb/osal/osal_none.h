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
#include "common/fifo.h"

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
//   OSAL_TASK_BEGIN
//
//   task body statements
//
//   OSAL_TASK_LOOP_ENG
// }
//
// NOTE: no switch statement is allowed in Task and subtask
//--------------------------------------------------------------------+
typedef void (*osal_func_t)(void *param);
typedef void* osal_task_t;

static inline osal_task_t osal_task_create(osal_func_t code, const char* name, uint32_t stack_size, void* param, uint32_t prio)
{
  (void) code; (void) name; (void) stack_size; (void) param; (void) prio;
  return (osal_task_t) 1;
}

#define TASK_RESTART \
  state = 0

#define OSAL_TASK_BEGIN \
  ATTR_UNUSED static uint32_t timeout = 0;\
  static uint16_t state = 0;\
  (void) timeout; /* timemout can possible unsued */ \
  switch(state) { \
    case 0: { \

#define OSAL_TASK_END \
  default:\
    TASK_RESTART;\
  }}\
  return;


#define osal_task_delay(msec) \
  do {\
    timeout = osal_tick_get();\
    state = __LINE__; case __LINE__:\
      if ( timeout + osal_tick_from_msec(msec) > osal_tick_get() ) \
        return TUSB_ERROR_OSAL_WAITING;\
  }while(0)

//--------------------------------------------------------------------+
// SUBTASK (a sub function that uses OS blocking services & called by a task
//--------------------------------------------------------------------+
#define OSAL_SUBTASK_BEGIN  OSAL_TASK_BEGIN
#define OSAL_SUBTASK_END \
  default:\
    TASK_RESTART;\
  }}\
  return TUSB_ERROR_NONE;

#define OSAL_SUBTASK_INVOKED_AND_WAIT(subtask, status) \
    do {\
      state = __LINE__; case __LINE__:\
      {\
        status = subtask; /* invoke sub task */\
        if (TUSB_ERROR_OSAL_WAITING == status) /* sub task not finished -> continue waiting */\
          return TUSB_ERROR_OSAL_WAITING;\
      }\
    }while(0)

//------------- Sub Task Assert -------------//
#define SUBTASK_EXIT(error) \
    do { TASK_RESTART; return error; } while(0)

#define _SUBTASK_ASSERT_ERROR_HANDLER(error, func_call) \
    do { func_call; TASK_RESTART; return error; } while(0)


#define SUBTASK_ASSERT_STATUS(sts)                          VERIFY_STATUS_HDLR(sts, TASK_RESTART)
#define SUBTASK_ASSERT_STATUS_WITH_HANDLER(sts, func_call)  VERIFY_STATUS_HDLR(sts, func_call; TASK_RESTART )

#define SUBTASK_ASSERT(condition)                           VERIFY_HDLR(condition, TASK_RESTART)
// TODO remove assert with handler by catching error in enum main task
#define SUBTASK_ASSERT_WITH_HANDLER(condition, func_call)  VERIFY_HDLR(condition, func_call; TASK_RESTART)

/*
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

*/



//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
typedef fifo_t* osal_queue_t;

static inline osal_queue_t osal_queue_create(uint32_t depth, uint32_t item_size)
{
  fifo_t* ff   = (fifo_t* ) tu_malloc( sizeof(fifo_t) );
  uint8_t* buf = (uint8_t*) tu_malloc( depth*item_size );

  VERIFY( ff && buf, NULL);

  *ff = (fifo_t) {
    .buffer = buf, .depth = depth, .item_size = item_size,
    .count = 0, .wr_idx =0, .rd_idx = 0, .overwritable = false
  };

  return (osal_queue_t) ff;
}


static inline bool osal_queue_send(osal_queue_t const queue_hdl, void const * data)
{
  return fifo_write( (fifo_t*) queue_hdl, data);
}

static inline void osal_queue_flush(osal_queue_t const queue_hdl)
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
      /*TODO mutex lock hal_usb_int_disable */\
      memcpy(p_data, queue_hdl->buffer + (queue_hdl->rd_idx * queue_hdl->item_size), queue_hdl->item_size);\
      queue_hdl->rd_idx = (queue_hdl->rd_idx + 1) % queue_hdl->depth;\
      queue_hdl->count--;\
      /*TODO mutex unlock hal_usb_int_enable */\
      *(p_error) = TUSB_ERROR_NONE;\
    }\
  }while(0)


//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef struct
{
  volatile uint16_t count;
           uint16_t max_count;
}osal_semaphore_data_t;

typedef osal_semaphore_data_t* osal_semaphore_t;


static inline osal_semaphore_t osal_semaphore_create(uint32_t max_count, uint32_t init)
{
  osal_semaphore_data_t* sem_data = (osal_semaphore_data_t*) tu_malloc( sizeof(osal_semaphore_data_t));
  VERIFY(sem_data, NULL);

  sem_data->count     = init;
  sem_data->max_count = max_count;

  return sem_data;
}

static inline  bool osal_semaphore_post(osal_semaphore_t sem_hdl)
{
  if (sem_hdl->count < sem_hdl->max_count ) sem_hdl->count++;
  return true;
}

static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl)
{
  sem_hdl->count = 0;
}

#define osal_semaphore_wait(sem_hdl, msec, p_error) \
  do {\
    timeout = osal_tick_get();\
    state = __LINE__; case __LINE__:\
    if( sem_hdl->count == 0 ) {\
      if ( ( ((uint32_t) (msec)) != OSAL_TIMEOUT_WAIT_FOREVER) && (timeout + osal_tick_from_msec(msec) <= osal_tick_get()) ) /* time out */ \
        *(p_error) = TUSB_ERROR_OSAL_TIMEOUT;\
      else\
        return TUSB_ERROR_OSAL_WAITING;\
    } else{\
      if (sem_hdl->count) sem_hdl->count--; /*TODO mutex hal_usb_int_disable consideration*/\
      *(p_error) = TUSB_ERROR_NONE;\
    }\
  }while(0)

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef osal_semaphore_t osal_mutex_t;

static inline osal_mutex_t osal_mutex_create(void)
{
  return osal_semaphore_create(1, 0);
}

static inline bool osal_mutex_release(osal_mutex_t mutex_hdl)
{
  return osal_semaphore_post(mutex_hdl);
}

// TOOD remove
static inline void osal_mutex_reset(osal_mutex_t mutex_hdl)
{
  osal_semaphore_reset(mutex_hdl);
}

#define osal_mutex_wait osal_semaphore_wait



#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_NONE_H_ */

/** @} */
