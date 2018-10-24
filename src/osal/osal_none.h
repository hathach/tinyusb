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

#include "common/tusb_fifo.h"

#ifdef __cplusplus
 extern "C" {
#endif

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
#define OSAL_TASK_DEF(_name, _str, _func, _prio, _stack_sz)  osal_task_def_t _name;

typedef uint8_t osal_task_def_t;

static inline bool osal_task_create(osal_task_def_t* taskdef)
{
  (void) taskdef;
  return true;
}

#define TASK_RESTART                             \
  _state = 0

#define osal_task_delay(msec)                    \
  do {                                           \
    _timeout = tusb_hal_millis();                \
    _state = __LINE__; case __LINE__:            \
      if ( _timeout + msec > tusb_hal_millis() ) \
        return TUSB_ERROR_OSAL_WAITING;          \
  }while(0)

//--------------------------------------------------------------------+
// SUBTASK (a sub function that uses OS blocking services & called by a task
//--------------------------------------------------------------------+
#define OSAL_SUBTASK_BEGIN                       \
  static uint16_t _state = 0;                    \
  ATTR_UNUSED static uint32_t _timeout = 0;      \
  (void) _timeout;                               \
  switch(_state) {                               \
    case 0: {

#define OSAL_SUBTASK_END                         \
  default: TASK_RESTART; break;                  \
  }}                                             \
  return TUSB_ERROR_NONE;

#define STASK_INVOKE(_subtask, _status)                                         \
  do {                                                                          \
    _state = __LINE__; case __LINE__:                                           \
    {                                                                           \
      (_status) = _subtask; /* invoke sub task */                               \
      if (TUSB_ERROR_OSAL_WAITING == (_status)) return TUSB_ERROR_OSAL_WAITING; \
    }                                                                           \
  }while(0)

//------------- Sub Task Assert -------------//
#define STASK_RETURN(error)     do { TASK_RESTART; return error; } while(0)

#define STASK_ASSERT_ERR(_err)                TU_VERIFY_ERR_HDLR(_err, TU_BREAKPOINT(); TASK_RESTART, TUSB_ERROR_FAILED)
#define STASK_ASSERT_ERR_HDLR(_err, _func)    TU_VERIFY_ERR_HDLR(_err, TU_BREAKPOINT(); _func; TASK_RESTART, TUSB_ERROR_FAILED )

#define STASK_ASSERT(_cond)                   TU_VERIFY_HDLR(_cond, TU_BREAKPOINT(); TASK_RESTART, TUSB_ERROR_FAILED)
#define STASK_ASSERT_HDLR(_cond, _func)       TU_VERIFY_HDLR(_cond, TU_BREAKPOINT(); _func; TASK_RESTART, TUSB_ERROR_FAILED)

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
#define OSAL_QUEUE_DEF(_name, _depth, _type)    TU_FIFO_DEF(_name, _depth, _type, false)

typedef tu_fifo_t  osal_queue_def_t;
typedef tu_fifo_t* osal_queue_t;

static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  tu_fifo_clear(qdef);
  return (osal_queue_t) qdef;
}

static inline bool osal_queue_send(osal_queue_t const queue_hdl, void const * data, bool in_isr)
{
  (void) in_isr;
  return tu_fifo_write( (tu_fifo_t*) queue_hdl, data);
}

static inline void osal_queue_flush(osal_queue_t const queue_hdl)
{
  queue_hdl->count = queue_hdl->rd_idx = queue_hdl->wr_idx = 0;
}

#define osal_queue_receive(queue_hdl, p_data, msec, p_error)                                \
  do {                                                                                      \
    _timeout = tusb_hal_millis();                                                           \
    _state = __LINE__; case __LINE__:                                                       \
    if( queue_hdl->count == 0 ) {                                                           \
      if ( (msec != OSAL_TIMEOUT_WAIT_FOREVER) && ( _timeout + msec <= tusb_hal_millis()) ) \
        *(p_error) = TUSB_ERROR_OSAL_TIMEOUT;                                               \
      else                                                                                  \
        return TUSB_ERROR_OSAL_WAITING;                                                     \
    } else{                                                                                 \
      /*tusb_hal_int_disable_all();*/                                                       \
      tu_fifo_read(queue_hdl, p_data);                                                         \
      /*tusb_hal_int_enable_all();*/                                                            \
      *(p_error) = TUSB_ERROR_NONE;                                                         \
    }                                                                                       \
  }while(0)


//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef struct
{
  volatile uint16_t count;
           uint16_t max_count;
}osal_semaphore_def_t;

typedef osal_semaphore_def_t* osal_semaphore_t;


static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  semdef->count     = 0;
  semdef->max_count = 1;
  return semdef;
}

static inline  bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  (void) in_isr;
  if (sem_hdl->count < sem_hdl->max_count ) sem_hdl->count++;
  return true;
}

static inline void osal_semaphore_reset_isr(osal_semaphore_t sem_hdl)
{
  sem_hdl->count = 0;
}

#define osal_semaphore_wait(sem_hdl, msec, p_error)                                        \
  do {                                                                                     \
    _timeout = tusb_hal_millis();                                                          \
    _state = __LINE__; case __LINE__:                                                      \
    if( sem_hdl->count == 0 ) {                                                            \
      if ( (msec != OSAL_TIMEOUT_WAIT_FOREVER) && (_timeout + msec <= tusb_hal_millis()) ) \
        *(p_error) = TUSB_ERROR_OSAL_TIMEOUT;                                              \
      else                                                                                 \
        return TUSB_ERROR_OSAL_WAITING;                                                    \
    } else{                                                                                \
      /*tusb_hal_int_disable_all();*/                                                          \
      sem_hdl->count--;                                                                    \
      /*tusb_hal_int_enable_all();*/                                                           \
      *(p_error) = TUSB_ERROR_NONE;                                                        \
    }                                                                                      \
  }while(0)

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
#if 0
typedef osal_semaphore_t osal_mutex_t;

static inline osal_mutex_t osal_mutex_create(void)
{
  return osal_semaphore_create(1, 0);
}

static inline bool osal_mutex_release(osal_mutex_t mutex_hdl)
{
  return osal_semaphore_post(mutex_hdl);
}

#define osal_mutex_wait osal_semaphore_wait
#endif


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_NONE_H_ */

/** @} */
