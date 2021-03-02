/*
 * Copyright (c) 2013-2019 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------------
 *
 * Project:     CMSIS-RTOS RTX
 * Title:       Timer functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  OS Runtime Object Memory Usage
#if ((defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0)))
osRtxObjectMemUsage_t osRtxTimerMemUsage \
__attribute__((section(".data.os.timer.obj"))) =
{ 0U, 0U, 0U };
#endif


//  ==== Helper functions ====

/// Insert Timer into the Timer List sorted by Time.
/// \param[in]  timer           timer object.
/// \param[in]  tick            timer tick.
static void TimerInsert (os_timer_t *timer, uint32_t tick) {
  os_timer_t *prev, *next;

  prev = NULL;
  next = osRtxInfo.timer.list;
  while ((next != NULL) && (next->tick <= tick)) {
    tick -= next->tick;
    prev  = next;
    next  = next->next;
  }
  timer->tick = tick;
  timer->prev = prev;
  timer->next = next;
  if (next != NULL) {
    next->tick -= timer->tick;
    next->prev  = timer;
  }
  if (prev != NULL) {
    prev->next = timer;
  } else {
    osRtxInfo.timer.list = timer;
  }
}

/// Remove Timer from the Timer List.
/// \param[in]  timer           timer object.
static void TimerRemove (const os_timer_t *timer) {

  if (timer->next != NULL) {
    timer->next->tick += timer->tick;
    timer->next->prev  = timer->prev;
  }
  if (timer->prev != NULL) {
    timer->prev->next  = timer->next;
  } else {
    osRtxInfo.timer.list = timer->next;
  }
}

/// Unlink Timer from the Timer List Head.
/// \param[in]  timer           timer object.
static void TimerUnlink (const os_timer_t *timer) {

  if (timer->next != NULL) {
    timer->next->prev = timer->prev;
  }
  osRtxInfo.timer.list = timer->next;
}


//  ==== Library functions ====

/// Timer Tick (called each SysTick).
static void osRtxTimerTick (void) {
  os_timer_t *timer;
  osStatus_t  status;

  timer = osRtxInfo.timer.list;
  if (timer == NULL) {
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return;
  }

  timer->tick--;
  while ((timer != NULL) && (timer->tick == 0U)) {
    TimerUnlink(timer);
    status = osMessageQueuePut(osRtxInfo.timer.mq, &timer->finfo, 0U, 0U);
    if (status != osOK) {
      (void)osRtxErrorNotify(osRtxErrorTimerQueueOverflow, timer);
    }
    if (timer->type == osRtxTimerPeriodic) {
      TimerInsert(timer, timer->load);
    } else {
      timer->state = osRtxTimerStopped;
    }
    timer = osRtxInfo.timer.list;
  }
}

/// Timer Thread
__WEAK __NO_RETURN void osRtxTimerThread (void *argument) {
  os_timer_finfo_t finfo;
  osStatus_t       status;
  (void)           argument;

  osRtxInfo.timer.mq = osRtxMessageQueueId(
    osMessageQueueNew(osRtxConfig.timer_mq_mcnt, sizeof(os_timer_finfo_t), osRtxConfig.timer_mq_attr)
  );
  osRtxInfo.timer.tick = osRtxTimerTick;

  for (;;) {
    //lint -e{934} "Taking address of near auto variable"
    status = osMessageQueueGet(osRtxInfo.timer.mq, &finfo, NULL, osWaitForever);
    if (status == osOK) {
      EvrRtxTimerCallback(finfo.func, finfo.arg);
      (finfo.func)(finfo.arg);
    }
  }
}

//  ==== Service Calls ====

/// Create and Initialize a timer.
/// \note API identical to osTimerNew
static osTimerId_t svcRtxTimerNew (osTimerFunc_t func, osTimerType_t type, void *argument, const osTimerAttr_t *attr) {
  os_timer_t *timer;
  uint8_t     flags;
  const char *name;

  // Check parameters
  if ((func == NULL) || ((type != osTimerOnce) && (type != osTimerPeriodic))) {
    EvrRtxTimerError(NULL, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  // Process attributes
  if (attr != NULL) {
    name  = attr->name;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    timer = attr->cb_mem;
    if (timer != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)timer & 3U) != 0U) || (attr->cb_size < sizeof(os_timer_t))) {
        EvrRtxTimerError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    } else {
      if (attr->cb_size != 0U) {
        EvrRtxTimerError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
  } else {
    name  = NULL;
    timer = NULL;
  }

  // Allocate object memory if not provided
  if (timer == NULL) {
    if (osRtxInfo.mpi.timer != NULL) {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      timer = osRtxMemoryPoolAlloc(osRtxInfo.mpi.timer);
    } else {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      timer = osRtxMemoryAlloc(osRtxInfo.mem.common, sizeof(os_timer_t), 1U);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    if (timer != NULL) {
      uint32_t used;
      osRtxTimerMemUsage.cnt_alloc++;
      used = osRtxTimerMemUsage.cnt_alloc - osRtxTimerMemUsage.cnt_free;
      if (osRtxTimerMemUsage.max_used < used) {
        osRtxTimerMemUsage.max_used = used;
      }
    }
#endif
    flags = osRtxFlagSystemObject;
  } else {
    flags = 0U;
  }

  if (timer != NULL) {
    // Initialize control block
    timer->id         = osRtxIdTimer;
    timer->state      = osRtxTimerStopped;
    timer->flags      = flags;
    timer->type       = (uint8_t)type;
    timer->name       = name;
    timer->prev       = NULL;
    timer->next       = NULL;
    timer->tick       = 0U;
    timer->load       = 0U;
    timer->finfo.func = func;
    timer->finfo.arg  = argument;

    EvrRtxTimerCreated(timer, timer->name);
  } else {
    EvrRtxTimerError(NULL, (int32_t)osErrorNoMemory);
  }

  return timer;
}

/// Get name of a timer.
/// \note API identical to osTimerGetName
static const char *svcRtxTimerGetName (osTimerId_t timer_id) {
  os_timer_t *timer = osRtxTimerId(timer_id);

  // Check parameters
  if ((timer == NULL) || (timer->id != osRtxIdTimer)) {
    EvrRtxTimerGetName(timer, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  EvrRtxTimerGetName(timer, timer->name);

  return timer->name;
}

/// Start or restart a timer.
/// \note API identical to osTimerStart
static osStatus_t svcRtxTimerStart (osTimerId_t timer_id, uint32_t ticks) {
  os_timer_t *timer = osRtxTimerId(timer_id);

  // Check parameters
  if ((timer == NULL) || (timer->id != osRtxIdTimer) || (ticks == 0U)) {
    EvrRtxTimerError(timer, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  if (timer->state == osRtxTimerRunning) {
    TimerRemove(timer);
  } else {
    if (osRtxInfo.timer.tick == NULL) {
      EvrRtxTimerError(timer, (int32_t)osErrorResource);
      //lint -e{904} "Return statement before end of function" [MISRA Note 1]
      return osErrorResource;
    } else {
      timer->state = osRtxTimerRunning;
      timer->load  = ticks;
    }
  }

  TimerInsert(timer, ticks);

  EvrRtxTimerStarted(timer);

  return osOK;
}

/// Stop a timer.
/// \note API identical to osTimerStop
static osStatus_t svcRtxTimerStop (osTimerId_t timer_id) {
  os_timer_t *timer = osRtxTimerId(timer_id);

  // Check parameters
  if ((timer == NULL) || (timer->id != osRtxIdTimer)) {
    EvrRtxTimerError(timer, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check object state
  if (timer->state != osRtxTimerRunning) {
    EvrRtxTimerError(timer, (int32_t)osErrorResource);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorResource;
  }

  timer->state = osRtxTimerStopped;

  TimerRemove(timer);

  EvrRtxTimerStopped(timer);

  return osOK;
}

/// Check if a timer is running.
/// \note API identical to osTimerIsRunning
static uint32_t svcRtxTimerIsRunning (osTimerId_t timer_id) {
  os_timer_t *timer = osRtxTimerId(timer_id);
  uint32_t    is_running;

  // Check parameters
  if ((timer == NULL) || (timer->id != osRtxIdTimer)) {
    EvrRtxTimerIsRunning(timer, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  if (timer->state == osRtxTimerRunning) {
    EvrRtxTimerIsRunning(timer, 1U);
    is_running = 1U;
  } else {
    EvrRtxTimerIsRunning(timer, 0U);
    is_running = 0;
  }

  return is_running;
}

/// Delete a timer.
/// \note API identical to osTimerDelete
static osStatus_t svcRtxTimerDelete (osTimerId_t timer_id) {
  os_timer_t *timer = osRtxTimerId(timer_id);

  // Check parameters
  if ((timer == NULL) || (timer->id != osRtxIdTimer)) {
    EvrRtxTimerError(timer, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  if (timer->state == osRtxTimerRunning) {
    TimerRemove(timer);
  }

  // Mark object as inactive and invalid
  timer->state = osRtxTimerInactive;
  timer->id    = osRtxIdInvalid;

  // Free object memory
  if ((timer->flags & osRtxFlagSystemObject) != 0U) {
    if (osRtxInfo.mpi.timer != NULL) {
      (void)osRtxMemoryPoolFree(osRtxInfo.mpi.timer, timer);
    } else {
      (void)osRtxMemoryFree(osRtxInfo.mem.common, timer);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    osRtxTimerMemUsage.cnt_free++;
#endif
  }

  EvrRtxTimerDestroyed(timer);

  return osOK;
}

//  Service Calls definitions
//lint ++flb "Library Begin" [MISRA Note 11]
SVC0_4(TimerNew,       osTimerId_t,  osTimerFunc_t, osTimerType_t, void *, const osTimerAttr_t *)
SVC0_1(TimerGetName,   const char *, osTimerId_t)
SVC0_2(TimerStart,     osStatus_t,   osTimerId_t, uint32_t)
SVC0_1(TimerStop,      osStatus_t,   osTimerId_t)
SVC0_1(TimerIsRunning, uint32_t,     osTimerId_t)
SVC0_1(TimerDelete,    osStatus_t,   osTimerId_t)
//lint --flb "Library End"


//  ==== Public API ====

/// Create and Initialize a timer.
osTimerId_t osTimerNew (osTimerFunc_t func, osTimerType_t type, void *argument, const osTimerAttr_t *attr) {
  osTimerId_t timer_id;

  EvrRtxTimerNew(func, type, argument, attr);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxTimerError(NULL, (int32_t)osErrorISR);
    timer_id = NULL;
  } else {
    timer_id = __svcTimerNew(func, type, argument, attr);
  }
  return timer_id;
}

/// Get name of a timer.
const char *osTimerGetName (osTimerId_t timer_id) {
  const char *name;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxTimerGetName(timer_id, NULL);
    name = NULL;
  } else {
    name = __svcTimerGetName(timer_id);
  }
  return name;
}

/// Start or restart a timer.
osStatus_t osTimerStart (osTimerId_t timer_id, uint32_t ticks) {
  osStatus_t status;

  EvrRtxTimerStart(timer_id, ticks);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxTimerError(timer_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcTimerStart(timer_id, ticks);
  }
  return status;
}

/// Stop a timer.
osStatus_t osTimerStop (osTimerId_t timer_id) {
  osStatus_t status;

  EvrRtxTimerStop(timer_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxTimerError(timer_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcTimerStop(timer_id);
  }
  return status;
}

/// Check if a timer is running.
uint32_t osTimerIsRunning (osTimerId_t timer_id) {
  uint32_t is_running;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxTimerIsRunning(timer_id, 0U);
    is_running = 0U;
  } else {
    is_running = __svcTimerIsRunning(timer_id);
  }
  return is_running;
}

/// Delete a timer.
osStatus_t osTimerDelete (osTimerId_t timer_id) {
  osStatus_t status;

  EvrRtxTimerDelete(timer_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxTimerError(timer_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcTimerDelete(timer_id);
  }
  return status;
}
