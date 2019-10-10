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

// Segger embOS porting layer
// 2019-10-08 Matt Page @ Scannex.com

#ifndef _TUSB_OSAL_EMBOS_H_
#define _TUSB_OSAL_EMBOS_H_

#include "RTOS.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
static inline void osal_task_delay(uint32_t msec)
{
  // TODO: Convert milliseconds to RTOS ticks (most BSP are millisecond based, but not all!)
  OS_Delay(msec);
}

//--------------------------------------------------------------------+
// Interrupt API
//--------------------------------------------------------------------+
static inline void osal_enter_interrupt(void)
{
  OS_EnterInterrupt();
}

static inline void osal_leave_interrupt(void)
{
  OS_LeaveInterrupt();
}

//--------------------------------------------------------------------+
// Binary Semaphore API
//--------------------------------------------------------------------+
typedef OS_CSEMA              osal_semaphore_def_t;
typedef osal_semaphore_def_t* osal_semaphore_t;

static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  OS_CreateCSema(semdef, 0);
  return semdef;
}

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  (void) in_isr;
  OS_SignalCSema(sem_hdl);
  return true;
}

static inline bool osal_semaphore_wait (osal_semaphore_t sem_hdl, uint32_t msec)
{
  (void) msec;

  if (msec == OSAL_TIMEOUT_WAIT_FOREVER) {
    OS_WaitCSema(sem_hdl);
    return true;
  } else {
    return OS_WaitCSemaTimed(sem_hdl, msec);
  }
}

static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl)
{
  OS_SetCSemaValue(sem_hdl, 0);
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
// Within tinyusb, mutex is never used in ISR context
//--------------------------------------------------------------------+
typedef OS_RSEMA osal_mutex_def_t;
typedef OS_RSEMA* osal_mutex_t;

static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
  OS_CreateRSema(mdef);
  return mdef;
}

static inline bool osal_mutex_lock (osal_mutex_t mutex_hdl, uint32_t msec)
{
  if (msec == OSAL_TIMEOUT_WAIT_FOREVER) {
    OS_Use(mutex_hdl);
    return true;
  }
  else {
    return OS_UseTimed(mutex_hdl, msec);
  }
}

static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
  OS_Unuse(mutex_hdl);
  return true;
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_role, _name, _depth, _type) \
  static _type _name##_##buf[_depth];\
  osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf };

typedef struct
{
  uint16_t depth;
  uint16_t item_sz;
  void*    buf;

  OS_MAILBOX mb;
}osal_queue_def_t;

typedef OS_MAILBOX* osal_queue_t;

static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  OS_MAILBOX *mbp = &qdef->mb;
  OS_CreateMB(mbp, qdef->item_sz, qdef->depth, qdef->buf);
  return mbp;
}

static inline bool osal_queue_receive(osal_queue_t const queue_hdl, void* data)
{
  OS_GetMail(queue_hdl, data);
  return true;
}

static inline bool osal_queue_send(osal_queue_t const queue_hdl, void const * data, bool in_isr)
{
  if (in_isr) return (0 == OS_PutMailCond(queue_hdl, data));
  OS_PutMail(queue_hdl, data);
  return true;
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_NONE_H_ */
