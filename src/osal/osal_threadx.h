/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#ifndef TUSB_OSAL_THREADX_H_
#define TUSB_OSAL_THREADX_H_

// ThreadX Headers
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint32_t _osal_ms2tick(uint32_t msec) {
  if ( msec == TX_WAIT_FOREVER ) return TX_WAIT_FOREVER;
  if ( msec == 0 ) return 0;

  uint32_t ticks = msec * TX_TIMER_TICKS_PER_SECOND  / 1000;

  // TX_TIMER_TICKS_PER_SECOND is less than 1000 and 1 tick > 1 ms
  // we still need to delay at least 1 tick
  if ( ticks == 0 ) ticks = 1;

  return ticks;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_task_delay(uint32_t msec) {
  tx_thread_sleep(_osal_ms2tick(msec));
}

//--------------------------------------------------------------------+
// Spinlock API
//--------------------------------------------------------------------+
//--------------------------------------------------------------------+
// Spinlock API
//--------------------------------------------------------------------+
typedef struct {
  void (* interrupt_set)(bool);
} osal_spinlock_t;

// For SMP, spinlock must be locked by hardware, cannot just use interrupt
#define OSAL_SPINLOCK_DEF(_name, _int_set) \
  osal_spinlock_t _name = { .interrupt_set = _int_set }

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_init(osal_spinlock_t *ctx) {
  (void) ctx;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_lock(osal_spinlock_t *ctx, bool in_isr) {
 if (!in_isr) {
   ctx->interrupt_set(false);
 }
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_unlock(osal_spinlock_t *ctx, bool in_isr) {
 if (!in_isr) {
   ctx->interrupt_set(true);
 }
}


//--------------------------------------------------------------------+
// Binary Semaphore API (act)
//--------------------------------------------------------------------+
// Note: semaphores are not used in tinyusb for now, and their API has not been tested

typedef TX_SEMAPHORE osal_semaphore_def_t, * osal_semaphore_t;

TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t *semdef) {
  tx_semaphore_create(semdef->semaphore, semdef->name, 0);
  return semdef;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_delete(osal_semaphore_t sem_hdl) {
  (void) sem_hdl;
  return TX_SUCCESS == tx_semaphore_delete(sem_hdl);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr) {
  (void) in_isr;
  return TX_SUCCESS == tx_semaphore_put(sem_hdl);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec) {
  return TX_SUCCESS == tx_semaphore_get(sem_hdl, _osal_ms2tick(msec));
}

TU_ATTR_ALWAYS_INLINE static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl) {
}

//--------------------------------------------------------------------+
// MUTEX API
//--------------------------------------------------------------------+
typedef TX_MUTEX osal_mutex_def_t, *osal_mutex_t;

TU_ATTR_ALWAYS_INLINE static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t *mdef) {
  if (TX_SUCCESS == tx_mutex_create(mdef, mdef->tx_mutex_name, TX_NO_INHERIT)) {
  	return mdef;
  } else {
    return NULL;
  }
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_delete(osal_mutex_t mutex_hdl) {
  (void) mutex_hdl;
  return true; // nothing to do
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_lock(osal_mutex_t mutex_hdl, uint32_t msec) {
  return TX_SUCCESS == tx_mutex_get(mutex_hdl, _osal_ms2tick(msec));
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl) {
  return TX_SUCCESS == tx_mutex_put(mutex_hdl);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

typedef TX_QUEUE osal_queue_def_t, * osal_queue_t;

// _int_set is not used with an RTOS _usbd_qdef

#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type)    \
static _type _name##_buf[_depth];                         \
osal_queue_def_t _name = {                                \
		.tx_queue_name         = #_name,                  \
		.tx_queue_message_size = (sizeof(_type) + 3) / 4, \
		.tx_queue_capacity     = _depth,                  \
		.tx_queue_start        =  _name##_buf }


TU_ATTR_ALWAYS_INLINE static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef) {
  return TX_SUCCESS ==
           tx_queue_create(qdef, qdef->tx_queue_name, qdef->tx_queue_message_size, qdef->tx_queue_start, qdef->tx_queue_capacity * qdef->tx_queue_message_size * 4)
		   ? qdef : 0;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_delete(osal_queue_t qhdl) {
  (void) qhdl;
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec) {
  return 0 == tx_queue_receive(qhdl, data, _osal_ms2tick(msec));
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_send(osal_queue_t qhdl, void *data, bool in_isr) {
  return 0 == tx_queue_send(qhdl, data, in_isr ? TX_NO_WAIT : TX_WAIT_FOREVER);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_empty(osal_queue_t qhdl) {
  ULONG enqueued;
  tx_queue_info_get(qhdl, 0, &enqueued, 0, 0, 0, 0);
  return enqueued == 0;
}

#ifdef __cplusplus
}
#endif

#endif
