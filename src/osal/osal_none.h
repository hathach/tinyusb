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

#include "tusb_hal.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
// Virtually do nothing in osal none
//--------------------------------------------------------------------+
#define OSAL_TASK_DEF(_name, _str, _func, _prio, _stack_sz)  osal_task_def_t _name;
typedef uint8_t osal_task_def_t;

static inline bool osal_task_create(osal_task_def_t* taskdef)
{
  (void) taskdef;
  return true;
}

static inline void osal_task_delay(uint32_t msec)
{
  uint32_t start = tusb_hal_millis();
  while ( ( tusb_hal_millis() - start ) < msec ) {}
}

//--------------------------------------------------------------------+
// Binary Semaphore API
//--------------------------------------------------------------------+
typedef struct
{
  volatile uint16_t count;
}osal_semaphore_def_t;

typedef osal_semaphore_def_t* osal_semaphore_t;

static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  semdef->count = 0;
  return semdef;
}

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  (void) in_isr;
  sem_hdl->count++;
  return true;
}

static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl)
{
  sem_hdl->count = 0;
}

// TODO blocking for now
static inline bool osal_semaphore_wait (osal_semaphore_t sem_hdl, uint32_t msec)
{
  (void) msec;

  while (sem_hdl->count == 0) { }
  sem_hdl->count--;

  return true;
}

//--------------------------------------------------------------------+
// MUTEX API
// Within tinyusb, mutex is never used in ISR context
//--------------------------------------------------------------------+
typedef osal_semaphore_def_t osal_mutex_def_t;
typedef osal_semaphore_t osal_mutex_t;

static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
  mdef->count = 1;
  return mdef;
}

#define osal_mutex_unlock(_mutex_hdl)   osal_semaphore_post(_mutex_hdl, false)
#define osal_mutex_lock                 osal_semaphore_wait

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
#include "common/tusb_fifo.h"

// extern to avoid including dcd.h and hcd.h
#if TUSB_OPT_DEVICE_ENABLED
extern void dcd_int_disable(uint8_t rhport);
extern void dcd_int_enable(uint8_t rhport);
#endif

#if MODE_HOST_SUPPORTED
extern void hcd_int_disable(uint8_t rhport);
extern void hcd_int_enable(uint8_t rhport);
#endif

typedef struct
{
    uint8_t role; // device or host
    tu_fifo_t ff;
}osal_queue_def_t;

typedef osal_queue_def_t* osal_queue_t;

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_role, _name, _depth, _type) \
  uint8_t _name##_buf[_depth*sizeof(_type)];        \
  osal_queue_def_t _name = {                        \
    .role = _role,                                  \
    .ff = {                                         \
      .buffer       = _name##_buf,                  \
      .depth        = _depth,                       \
      .item_size    = sizeof(_type),                \
      .overwritable = false,                        \
    }\
  }

// lock queue by disable usb isr
static inline void _osal_q_lock(osal_queue_t qhdl)
{
#if TUSB_OPT_DEVICE_ENABLED
  if (qhdl->role == OPT_MODE_DEVICE) dcd_int_disable(TUD_OPT_RHPORT);
#endif

#if MODE_HOST_SUPPORTED
  if (qhdl->role == OPT_MODE_HOST) hcd_int_disable(TUH_OPT_RHPORT);
#endif
}

// unlock queue
static inline void _osal_q_unlock(osal_queue_t qhdl)
{
#if TUSB_OPT_DEVICE_ENABLED
  if (qhdl->role == OPT_MODE_DEVICE) dcd_int_enable(TUD_OPT_RHPORT);
#endif

#if MODE_HOST_SUPPORTED
  if (qhdl->role == OPT_MODE_HOST) hcd_int_enable(TUH_OPT_RHPORT);
#endif
}

static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  tu_fifo_clear(&qdef->ff);
  return (osal_queue_t) qdef;
}

static inline bool osal_queue_send(osal_queue_t const qhdl, void const * data, bool in_isr)
{
  if (!in_isr) {
    _osal_q_lock(qhdl);
  }

  bool success = tu_fifo_write(&qhdl->ff, data);

  if (!in_isr) {
    _osal_q_unlock(qhdl);
  }

  return success;
}

static inline void osal_queue_reset(osal_queue_t const qhdl)
{
  // tusb_hal_int_disable_all();
  tu_fifo_clear(&qhdl->ff);
  // tusb_hal_int_enable_all();
}

// non blocking
static inline bool osal_queue_receive(osal_queue_t const qhdl, void* data)
{
  _osal_q_lock(qhdl);
  bool success = tu_fifo_read(&qhdl->ff, data);
  _osal_q_unlock(qhdl);

  return success;
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_NONE_H_ */

/** @} */
