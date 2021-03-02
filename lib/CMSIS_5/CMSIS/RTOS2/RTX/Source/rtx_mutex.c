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
 * Title:       Mutex functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  OS Runtime Object Memory Usage
#if ((defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0)))
osRtxObjectMemUsage_t osRtxMutexMemUsage \
__attribute__((section(".data.os.mutex.obj"))) =
{ 0U, 0U, 0U };
#endif


//  ==== Library functions ====

/// Release Mutex list when owner Thread terminates.
/// \param[in]  mutex_list      mutex list.
void osRtxMutexOwnerRelease (os_mutex_t *mutex_list) {
  os_mutex_t  *mutex;
  os_mutex_t  *mutex_next;
  os_thread_t *thread;

  mutex = mutex_list;
  while (mutex != NULL) {
    mutex_next = mutex->owner_next;
    // Check if Mutex is Robust
    if ((mutex->attr & osMutexRobust) != 0U) {
      // Clear Lock counter
      mutex->lock = 0U;
      EvrRtxMutexReleased(mutex, 0U);
      // Check if Thread is waiting for a Mutex
      if (mutex->thread_list != NULL) {
        // Wakeup waiting Thread with highest Priority
        thread = osRtxThreadListGet(osRtxObject(mutex));
        osRtxThreadWaitExit(thread, (uint32_t)osOK, FALSE);
        // Thread is the new Mutex owner
        mutex->owner_thread = thread;
        mutex->owner_prev   = NULL;
        mutex->owner_next   = thread->mutex_list;
        if (thread->mutex_list != NULL) {
          thread->mutex_list->owner_prev = mutex;
        }
        thread->mutex_list = mutex;
        mutex->lock = 1U;
        EvrRtxMutexAcquired(mutex, 1U);
      }
    }
    mutex = mutex_next;
  }
}

/// Restore Mutex owner Thread priority.
/// \param[in]  mutex           mutex object.
/// \param[in]  thread_wakeup   thread wakeup object.
void osRtxMutexOwnerRestore (const os_mutex_t *mutex, const os_thread_t *thread_wakeup) {
  const os_mutex_t  *mutex0;
        os_thread_t *thread;
        os_thread_t *thread0;
        int8_t       priority;

  // Restore owner Thread priority
  if ((mutex->attr & osMutexPrioInherit) != 0U) {
    thread   = mutex->owner_thread;
    priority = thread->priority_base;
    mutex0   = thread->mutex_list;
    // Check Mutexes owned by Thread
    do {
      // Check Threads waiting for Mutex
      thread0 = mutex0->thread_list;
      if (thread0 == thread_wakeup) {
        // Skip thread that is waken-up
        thread0 = thread0->thread_next;
      }
      if ((thread0 != NULL) && (thread0->priority > priority)) {
        // Higher priority Thread is waiting for Mutex
        priority = thread0->priority;
      }
      mutex0 = mutex0->owner_next;
    } while (mutex0 != NULL);
    if (thread->priority != priority) {
      thread->priority = priority;
      osRtxThreadListSort(thread);
    }
  }
}


//  ==== Service Calls ====

/// Create and Initialize a Mutex object.
/// \note API identical to osMutexNew
static osMutexId_t svcRtxMutexNew (const osMutexAttr_t *attr) {
  os_mutex_t *mutex;
  uint32_t    attr_bits;
  uint8_t     flags;
  const char *name;

  // Process attributes
  if (attr != NULL) {
    name      = attr->name;
    attr_bits = attr->attr_bits;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    mutex     = attr->cb_mem;
    if (mutex != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)mutex & 3U) != 0U) || (attr->cb_size < sizeof(os_mutex_t))) {
        EvrRtxMutexError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    } else {
      if (attr->cb_size != 0U) {
        EvrRtxMutexError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
  } else {
    name      = NULL;
    attr_bits = 0U;
    mutex     = NULL;
  }

  // Allocate object memory if not provided
  if (mutex == NULL) {
    if (osRtxInfo.mpi.mutex != NULL) {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      mutex = osRtxMemoryPoolAlloc(osRtxInfo.mpi.mutex);
    } else {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      mutex = osRtxMemoryAlloc(osRtxInfo.mem.common, sizeof(os_mutex_t), 1U);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    if (mutex != NULL) {
      uint32_t used;
      osRtxMutexMemUsage.cnt_alloc++;
      used = osRtxMutexMemUsage.cnt_alloc - osRtxMutexMemUsage.cnt_free;
      if (osRtxMutexMemUsage.max_used < used) {
        osRtxMutexMemUsage.max_used = used;
      }
    }
#endif
    flags = osRtxFlagSystemObject;
  } else {
    flags = 0U;
  }

  if (mutex != NULL) {
    // Initialize control block
    mutex->id           = osRtxIdMutex;
    mutex->flags        = flags;
    mutex->attr         = (uint8_t)attr_bits;
    mutex->name         = name;
    mutex->thread_list  = NULL;
    mutex->owner_thread = NULL;
    mutex->owner_prev   = NULL;
    mutex->owner_next   = NULL;
    mutex->lock         = 0U;

    EvrRtxMutexCreated(mutex, mutex->name);
  } else {
    EvrRtxMutexError(NULL, (int32_t)osErrorNoMemory);
  }

  return mutex;
}

/// Get name of a Mutex object.
/// \note API identical to osMutexGetName
static const char *svcRtxMutexGetName (osMutexId_t mutex_id) {
  os_mutex_t *mutex = osRtxMutexId(mutex_id);

  // Check parameters
  if ((mutex == NULL) || (mutex->id != osRtxIdMutex)) {
    EvrRtxMutexGetName(mutex, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  EvrRtxMutexGetName(mutex, mutex->name);

  return mutex->name;
}

/// Acquire a Mutex or timeout if it is locked.
/// \note API identical to osMutexAcquire
static osStatus_t svcRtxMutexAcquire (osMutexId_t mutex_id, uint32_t timeout) {
  os_mutex_t  *mutex = osRtxMutexId(mutex_id);
  os_thread_t *thread;
  osStatus_t   status;

  // Check running thread
  thread = osRtxThreadGetRunning();
  if (thread == NULL) {
    EvrRtxMutexError(mutex, osRtxErrorKernelNotRunning);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osError;
  }

  // Check parameters
  if ((mutex == NULL) || (mutex->id != osRtxIdMutex)) {
    EvrRtxMutexError(mutex, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check if Mutex is not locked
  if (mutex->lock == 0U) {
    // Acquire Mutex
    mutex->owner_thread = thread;
    mutex->owner_prev   = NULL;
    mutex->owner_next   = thread->mutex_list;
    if (thread->mutex_list != NULL) {
      thread->mutex_list->owner_prev = mutex;
    }
    thread->mutex_list = mutex;
    mutex->lock = 1U;
    EvrRtxMutexAcquired(mutex, mutex->lock);
    status = osOK;
  } else {
    // Check if Mutex is recursive and running Thread is the owner
    if (((mutex->attr & osMutexRecursive) != 0U) && (mutex->owner_thread == thread)) {
      // Try to increment lock counter
      if (mutex->lock == osRtxMutexLockLimit) {
        EvrRtxMutexError(mutex, osRtxErrorMutexLockLimit);
        status = osErrorResource;
      } else {
        mutex->lock++;
        EvrRtxMutexAcquired(mutex, mutex->lock);
        status = osOK;
      }
    } else {
      // Check if timeout is specified
      if (timeout != 0U) {
        // Check if Priority inheritance protocol is enabled
        if ((mutex->attr & osMutexPrioInherit) != 0U) {
          // Raise priority of owner Thread if lower than priority of running Thread
          if (mutex->owner_thread->priority < thread->priority) {
            mutex->owner_thread->priority = thread->priority;
            osRtxThreadListSort(mutex->owner_thread);
          }
        }
        EvrRtxMutexAcquirePending(mutex, timeout);
        // Suspend current Thread
        if (osRtxThreadWaitEnter(osRtxThreadWaitingMutex, timeout)) {
          osRtxThreadListPut(osRtxObject(mutex), thread);
        } else {
          EvrRtxMutexAcquireTimeout(mutex);
        }
        status = osErrorTimeout;
      } else {
        EvrRtxMutexNotAcquired(mutex);
        status = osErrorResource;
      }
    }
  }

  return status;
}

/// Release a Mutex that was acquired by osMutexAcquire.
/// \note API identical to osMutexRelease
static osStatus_t svcRtxMutexRelease (osMutexId_t mutex_id) {
        os_mutex_t  *mutex = osRtxMutexId(mutex_id);
  const os_mutex_t  *mutex0;
        os_thread_t *thread;
        int8_t       priority;

  // Check running thread
  thread = osRtxThreadGetRunning();
  if (thread == NULL) {
    EvrRtxMutexError(mutex, osRtxErrorKernelNotRunning);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osError;
  }

  // Check parameters
  if ((mutex == NULL) || (mutex->id != osRtxIdMutex)) {
    EvrRtxMutexError(mutex, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check if Mutex is not locked
  if (mutex->lock == 0U) {
    EvrRtxMutexError(mutex, osRtxErrorMutexNotLocked);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorResource;
  }

  // Check if running Thread is not the owner
  if (mutex->owner_thread != thread) {
    EvrRtxMutexError(mutex, osRtxErrorMutexNotOwned);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorResource;
  }

  // Decrement Lock counter
  mutex->lock--;
  EvrRtxMutexReleased(mutex, mutex->lock);

  // Check Lock counter
  if (mutex->lock == 0U) {

    // Remove Mutex from Thread owner list
    if (mutex->owner_next != NULL) {
      mutex->owner_next->owner_prev = mutex->owner_prev;
    }
    if (mutex->owner_prev != NULL) {
      mutex->owner_prev->owner_next = mutex->owner_next;
    } else {
      thread->mutex_list = mutex->owner_next;
    }

    // Restore running Thread priority
    if ((mutex->attr & osMutexPrioInherit) != 0U) {
      priority = thread->priority_base;
      mutex0   = thread->mutex_list;
      // Check mutexes owned by running Thread
      while (mutex0 != NULL) {
        if ((mutex0->thread_list != NULL) && (mutex0->thread_list->priority > priority)) {
          // Higher priority Thread is waiting for Mutex
          priority = mutex0->thread_list->priority;
        }
        mutex0 = mutex0->owner_next;
      }
      thread->priority = priority;
    }

    // Check if Thread is waiting for a Mutex
    if (mutex->thread_list != NULL) {
      // Wakeup waiting Thread with highest Priority
      thread = osRtxThreadListGet(osRtxObject(mutex));
      osRtxThreadWaitExit(thread, (uint32_t)osOK, FALSE);
      // Thread is the new Mutex owner
      mutex->owner_thread = thread;
      mutex->owner_prev   = NULL;
      mutex->owner_next   = thread->mutex_list;
      if (thread->mutex_list != NULL) {
        thread->mutex_list->owner_prev = mutex;
      }
      thread->mutex_list = mutex;
      mutex->lock = 1U;
      EvrRtxMutexAcquired(mutex, 1U);
    }

    osRtxThreadDispatch(NULL);
  }

  return osOK;
}

/// Get Thread which owns a Mutex object.
/// \note API identical to osMutexGetOwner
static osThreadId_t svcRtxMutexGetOwner (osMutexId_t mutex_id) {
  os_mutex_t *mutex = osRtxMutexId(mutex_id);

  // Check parameters
  if ((mutex == NULL) || (mutex->id != osRtxIdMutex)) {
    EvrRtxMutexGetOwner(mutex, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  // Check if Mutex is not locked
  if (mutex->lock == 0U) {
    EvrRtxMutexGetOwner(mutex, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  EvrRtxMutexGetOwner(mutex, mutex->owner_thread);

  return mutex->owner_thread;
}

/// Delete a Mutex object.
/// \note API identical to osMutexDelete
static osStatus_t svcRtxMutexDelete (osMutexId_t mutex_id) {
        os_mutex_t  *mutex = osRtxMutexId(mutex_id);
  const os_mutex_t  *mutex0;
        os_thread_t *thread;
        int8_t       priority;

  // Check parameters
  if ((mutex == NULL) || (mutex->id != osRtxIdMutex)) {
    EvrRtxMutexError(mutex, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check if Mutex is locked
  if (mutex->lock != 0U) {

    thread = mutex->owner_thread;

    // Remove Mutex from Thread owner list
    if (mutex->owner_next != NULL) {
      mutex->owner_next->owner_prev = mutex->owner_prev;
    }
    if (mutex->owner_prev != NULL) {
      mutex->owner_prev->owner_next = mutex->owner_next;
    } else {
      thread->mutex_list = mutex->owner_next;
    }

    // Restore owner Thread priority
    if ((mutex->attr & osMutexPrioInherit) != 0U) {
      priority = thread->priority_base;
      mutex0   = thread->mutex_list;
      // Check Mutexes owned by Thread
      while (mutex0 != NULL) {
        if ((mutex0->thread_list != NULL) && (mutex0->thread_list->priority > priority)) {
          // Higher priority Thread is waiting for Mutex
          priority = mutex0->thread_list->priority;
        }
        mutex0 = mutex0->owner_next;
      }
      if (thread->priority != priority) {
        thread->priority = priority;
        osRtxThreadListSort(thread);
      }
    }

    // Unblock waiting threads
    while (mutex->thread_list != NULL) {
      thread = osRtxThreadListGet(osRtxObject(mutex));
      osRtxThreadWaitExit(thread, (uint32_t)osErrorResource, FALSE);
    }

    osRtxThreadDispatch(NULL);
  }

  // Mark object as invalid
  mutex->id = osRtxIdInvalid;

  // Free object memory
  if ((mutex->flags & osRtxFlagSystemObject) != 0U) {
    if (osRtxInfo.mpi.mutex != NULL) {
      (void)osRtxMemoryPoolFree(osRtxInfo.mpi.mutex, mutex);
    } else {
      (void)osRtxMemoryFree(osRtxInfo.mem.common, mutex);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    osRtxMutexMemUsage.cnt_free++;
#endif
  }

  EvrRtxMutexDestroyed(mutex);

  return osOK;
}

//  Service Calls definitions
//lint ++flb "Library Begin" [MISRA Note 11]
SVC0_1(MutexNew,      osMutexId_t,  const osMutexAttr_t *)
SVC0_1(MutexGetName,  const char *, osMutexId_t)
SVC0_2(MutexAcquire,  osStatus_t,   osMutexId_t, uint32_t)
SVC0_1(MutexRelease,  osStatus_t,   osMutexId_t)
SVC0_1(MutexGetOwner, osThreadId_t, osMutexId_t)
SVC0_1(MutexDelete,   osStatus_t,   osMutexId_t)
//lint --flb "Library End"


//  ==== Public API ====

/// Create and Initialize a Mutex object.
osMutexId_t osMutexNew (const osMutexAttr_t *attr) {
  osMutexId_t mutex_id;

  EvrRtxMutexNew(attr);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMutexError(NULL, (int32_t)osErrorISR);
    mutex_id = NULL;
  } else {
    mutex_id = __svcMutexNew(attr);
  }
  return mutex_id;
}

/// Get name of a Mutex object.
const char *osMutexGetName (osMutexId_t mutex_id) {
  const char *name;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMutexGetName(mutex_id, NULL);
    name = NULL;
  } else {
    name = __svcMutexGetName(mutex_id);
  }
  return name;
}

/// Acquire a Mutex or timeout if it is locked.
osStatus_t osMutexAcquire (osMutexId_t mutex_id, uint32_t timeout) {
  osStatus_t status;

  EvrRtxMutexAcquire(mutex_id, timeout);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMutexError(mutex_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcMutexAcquire(mutex_id, timeout);
  }
  return status;
}

/// Release a Mutex that was acquired by \ref osMutexAcquire.
osStatus_t osMutexRelease (osMutexId_t mutex_id) {
  osStatus_t status;

  EvrRtxMutexRelease(mutex_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMutexError(mutex_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcMutexRelease(mutex_id);
  }
  return status;
}

/// Get Thread which owns a Mutex object.
osThreadId_t osMutexGetOwner (osMutexId_t mutex_id) {
  osThreadId_t thread;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMutexGetOwner(mutex_id, NULL);
    thread = NULL;
  } else {
    thread = __svcMutexGetOwner(mutex_id);
  }
  return thread;
}

/// Delete a Mutex object.
osStatus_t osMutexDelete (osMutexId_t mutex_id) {
  osStatus_t status;

  EvrRtxMutexDelete(mutex_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMutexError(mutex_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcMutexDelete(mutex_id);
  }
  return status;
}
