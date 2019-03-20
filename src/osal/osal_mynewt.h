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

#ifndef OSAL_MYNEWT_H_
#define OSAL_MYNEWT_H_

#include "os/os.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
static inline void osal_task_delay(uint32_t msec)
{
  os_time_delay( os_time_ms_to_ticks32(msec) );
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_role, _name, _depth, _type) \
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

static inline bool osal_queue_send(osal_queue_t const queue_hdl, void const * data, bool in_isr)
{
  (void) in_isr;

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

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  (void) in_isr;
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
