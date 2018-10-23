/**************************************************************************/
/*!
    @file     osal_mynewt.h
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
*/
/**************************************************************************/

#ifndef OSAL_MYNEWT_H_
#define OSAL_MYNEWT_H_

#include "os/os.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
#define OSAL_TASK_DEF(_name, _str, _func, _prio, _stack_sz) \
  static os_stack_t _name##_##buf[_stack_sz]; \
  osal_task_def_t _name = { .func = _func, .prio = _prio, .stack_sz = _stack_sz, .buf = _name##_##buf, .strname = _str };

typedef struct
{
  struct os_task mynewt_task;
  osal_task_func_t func;

  uint16_t prio;
  uint16_t stack_sz;
  void*    buf;
  const char* strname;
}osal_task_def_t;

static inline bool osal_task_create(osal_task_def_t* taskdef)
{
  return OS_OK == os_task_init(&taskdef->mynewt_task, taskdef->strname, taskdef->func, NULL, taskdef->prio, OS_WAIT_FOREVER,
                               (os_stack_t*) taskdef->buf, taskdef->stack_sz);
}

static inline void osal_task_delay(uint32_t msec)
{
  os_time_delay( os_time_ms_to_ticks32(msec) );
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
#define OSAL_QUEUE_DEF(_name, _depth, _type) \
  static _type _name##_##buf[_depth];\
  static struct os_event* _name##_##evbuf[_depth];\
  osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf, .evbuf =  _name##_##evbuf};\

typedef struct
{
  uint16_t depth;
  uint16_t item_sz;
  void*    buf;
  void*    evbuf;

  struct os_mempool mpool;
  struct os_mempool epool;

  struct os_eventq  evq;
}osal_queue_def_t;

typedef osal_queue_def_t* osal_queue_t;

static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  if ( OS_OK != os_mempool_init(&qdef->mpool, qdef->depth, qdef->item_sz, qdef->buf, "usbd queue") ) return NULL;
  if ( OS_OK != os_mempool_init(&qdef->epool, qdef->depth, sizeof(struct os_event), qdef->evbuf, "usbd evqueue") ) return NULL;

  os_eventq_init(&qdef->evq);
  return (osal_queue_t) qdef;
}

static inline void osal_queue_receive (osal_queue_t const queue_hdl, void *p_data, uint32_t msec, tusb_error_t *p_error)
{
  (void) msec;
  struct os_event* ev;

  if ( msec == 0 )
  {
    ev = os_eventq_get_no_wait(&queue_hdl->evq);
    if ( !ev )
    {
      *p_error = TUSB_ERROR_OSAL_TIMEOUT;
      return;
    }
  }else
  {
    ev = os_eventq_get(&queue_hdl->evq);
  }

  memcpy(p_data, ev->ev_arg, queue_hdl->item_sz); // copy message
  os_memblock_put(&queue_hdl->mpool, ev->ev_arg); // put back mem block
  os_memblock_put(&queue_hdl->epool, ev);         // put back ev block

  *p_error = TUSB_ERROR_NONE;
}

#define osal_queue_send_isr   osal_queue_send

static inline bool osal_queue_send(osal_queue_t const queue_hdl, void const * data)
{
  // get a block from mem pool for data
  void* ptr = os_memblock_get(&queue_hdl->mpool);
  if (!ptr) return false;
  memcpy(ptr, data, queue_hdl->item_sz);

  // get a block from event pool to put into queue
  struct os_event* ev = (struct os_event*) os_memblock_get(&queue_hdl->epool);
  if (!ev)
  {
    os_memblock_put(&queue_hdl->mpool, ptr);
    return false;
  }
  tu_memclr(ev, sizeof(struct os_event));
  ev->ev_arg = ptr;

  os_eventq_put(&queue_hdl->evq, ev);

  return true;
}

static inline void osal_queue_flush(osal_queue_t const queue_hdl)
{

}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef struct os_sem  osal_semaphore_def_t;
typedef struct os_sem* osal_semaphore_t;

static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  return (os_sem_init(semdef, 0) == OS_OK) ? (osal_semaphore_t) semdef : NULL;
}

#define osal_semaphore_post_isr   osal_semaphore_post

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl)
{
  return os_sem_release(sem_hdl) == OS_OK;
}

static inline void osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec, tusb_error_t *p_error)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? OS_TIMEOUT_NEVER : os_time_ms_to_ticks32(msec);
  (*p_error) = ( (os_sem_pend(sem_hdl, ticks) == OS_OK) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT );
}

static inline void osal_semaphore_reset_isr(osal_semaphore_t const sem_hdl)
{
//  xSemaphoreTakeFromISR(sem_hdl, NULL);
}

#if 0
//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef struct os_mutex osal_mutex_t;

#define osal_mutex_create(x) xSemaphoreCreateMutex()

static inline bool osal_mutex_release(osal_mutex_t mutex_hdl)
{
  return xSemaphoreGive(mutex_hdl);
}

static inline void osal_mutex_wait(osal_mutex_t mutex_hdl, uint32_t msec, tusb_error_t *p_error)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(msec);
  (*p_error) = (xSemaphoreTake(mutex_hdl, ticks) ? TUSB_ERROR_NONE : TUSB_ERROR_OSAL_TIMEOUT);
}
#endif


#ifdef __cplusplus
 }
#endif

#endif /* OSAL_MYNEWT_H_ */
