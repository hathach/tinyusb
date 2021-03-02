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
 * Title:       Event Flags functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  OS Runtime Object Memory Usage
#if ((defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0)))
osRtxObjectMemUsage_t osRtxEventFlagsMemUsage \
__attribute__((section(".data.os.evflags.obj"))) =
{ 0U, 0U, 0U };
#endif


//  ==== Helper functions ====

/// Set Event Flags.
/// \param[in]  ef              event flags object.
/// \param[in]  flags           specifies the flags to set.
/// \return event flags after setting.
static uint32_t EventFlagsSet (os_event_flags_t *ef, uint32_t flags) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask = __get_PRIMASK();
#endif
  uint32_t event_flags;

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  ef->event_flags |= flags;
  event_flags = ef->event_flags;

  if (primask == 0U) {
    __enable_irq();
  }
#else
  event_flags = atomic_set32(&ef->event_flags, flags);
#endif

  return event_flags;
}

/// Clear Event Flags.
/// \param[in]  ef              event flags object.
/// \param[in]  flags           specifies the flags to clear.
/// \return event flags before clearing.
static uint32_t EventFlagsClear (os_event_flags_t *ef, uint32_t flags) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask = __get_PRIMASK();
#endif
  uint32_t event_flags;

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  event_flags = ef->event_flags;
  ef->event_flags &= ~flags;

  if (primask == 0U) {
    __enable_irq();
  }
#else
  event_flags = atomic_clr32(&ef->event_flags, flags);
#endif

  return event_flags;
}

/// Check Event Flags.
/// \param[in]  ef              event flags object.
/// \param[in]  flags           specifies the flags to check.
/// \param[in]  options         specifies flags options (osFlagsXxxx).
/// \return event flags before clearing or 0 if specified flags have not been set.
static uint32_t EventFlagsCheck (os_event_flags_t *ef, uint32_t flags, uint32_t options) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask;
#endif
  uint32_t event_flags;

  if ((options & osFlagsNoClear) == 0U) {
#if (EXCLUSIVE_ACCESS == 0)
    primask = __get_PRIMASK();
    __disable_irq();

    event_flags = ef->event_flags;
    if ((((options & osFlagsWaitAll) != 0U) && ((event_flags & flags) != flags)) ||
        (((options & osFlagsWaitAll) == 0U) && ((event_flags & flags) == 0U))) {
      event_flags = 0U;
    } else {
      ef->event_flags &= ~flags;
    }

    if (primask == 0U) {
      __enable_irq();
    }
#else
    if ((options & osFlagsWaitAll) != 0U) {
      event_flags = atomic_chk32_all(&ef->event_flags, flags);
    } else {
      event_flags = atomic_chk32_any(&ef->event_flags, flags);
    }
#endif
  } else {
    event_flags = ef->event_flags;
    if ((((options & osFlagsWaitAll) != 0U) && ((event_flags & flags) != flags)) ||
        (((options & osFlagsWaitAll) == 0U) && ((event_flags & flags) == 0U))) {
      event_flags = 0U;
    }
  }

  return event_flags;
}


//  ==== Post ISR processing ====

/// Event Flags post ISR processing.
/// \param[in]  ef              event flags object.
static void osRtxEventFlagsPostProcess (os_event_flags_t *ef) {
  os_thread_t *thread;
  os_thread_t *thread_next;
  uint32_t     event_flags;

  // Check if Threads are waiting for Event Flags
  thread = ef->thread_list;
  while (thread != NULL) {
    thread_next = thread->thread_next;
    event_flags = EventFlagsCheck(ef, thread->wait_flags, thread->flags_options);
    if (event_flags != 0U) {
      osRtxThreadListRemove(thread);
      osRtxThreadWaitExit(thread, event_flags, FALSE);
      EvrRtxEventFlagsWaitCompleted(ef, thread->wait_flags, thread->flags_options, event_flags);
    }
    thread = thread_next;
  }
}


//  ==== Service Calls ====

/// Create and Initialize an Event Flags object.
/// \note API identical to osEventFlagsNew
static osEventFlagsId_t svcRtxEventFlagsNew (const osEventFlagsAttr_t *attr) {
  os_event_flags_t *ef;
  uint8_t           flags;
  const char       *name;

  // Process attributes
  if (attr != NULL) {
    name = attr->name;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    ef   = attr->cb_mem;
    if (ef != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)ef & 3U) != 0U) || (attr->cb_size < sizeof(os_event_flags_t))) {
        EvrRtxEventFlagsError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    } else {
      if (attr->cb_size != 0U) {
        EvrRtxEventFlagsError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
  } else {
    name = NULL;
    ef   = NULL;
  }

  // Allocate object memory if not provided
  if (ef == NULL) {
    if (osRtxInfo.mpi.event_flags != NULL) {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      ef = osRtxMemoryPoolAlloc(osRtxInfo.mpi.event_flags);
    } else {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      ef = osRtxMemoryAlloc(osRtxInfo.mem.common, sizeof(os_event_flags_t), 1U);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    if (ef != NULL) {
      uint32_t used;
      osRtxEventFlagsMemUsage.cnt_alloc++;
      used = osRtxEventFlagsMemUsage.cnt_alloc - osRtxEventFlagsMemUsage.cnt_free;
      if (osRtxEventFlagsMemUsage.max_used < used) {
        osRtxEventFlagsMemUsage.max_used = used;
      }
    }
#endif
    flags = osRtxFlagSystemObject;
  } else {
    flags = 0U;
  }

  if (ef != NULL) {
    // Initialize control block
    ef->id          = osRtxIdEventFlags;
    ef->flags       = flags;
    ef->name        = name;
    ef->thread_list = NULL;
    ef->event_flags = 0U;

    // Register post ISR processing function
    osRtxInfo.post_process.event_flags = osRtxEventFlagsPostProcess;

    EvrRtxEventFlagsCreated(ef, ef->name);
  } else {
    EvrRtxEventFlagsError(NULL, (int32_t)osErrorNoMemory);
  }

  return ef;
}

/// Get name of an Event Flags object.
/// \note API identical to osEventFlagsGetName
static const char *svcRtxEventFlagsGetName (osEventFlagsId_t ef_id) {
  os_event_flags_t *ef = osRtxEventFlagsId(ef_id);

  // Check parameters
  if ((ef == NULL) || (ef->id != osRtxIdEventFlags)) {
    EvrRtxEventFlagsGetName(ef, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  EvrRtxEventFlagsGetName(ef, ef->name);

  return ef->name;
}

/// Set the specified Event Flags.
/// \note API identical to osEventFlagsSet
static uint32_t svcRtxEventFlagsSet (osEventFlagsId_t ef_id, uint32_t flags) {
  os_event_flags_t *ef = osRtxEventFlagsId(ef_id);
  os_thread_t      *thread;
  os_thread_t      *thread_next;
  uint32_t          event_flags;
  uint32_t          event_flags0;

  // Check parameters
  if ((ef == NULL) || (ef->id != osRtxIdEventFlags) ||
      ((flags & ~(((uint32_t)1U << osRtxEventFlagsLimit) - 1U)) != 0U)) {
    EvrRtxEventFlagsError(ef, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Set Event Flags
  event_flags = EventFlagsSet(ef, flags);

  // Check if Threads are waiting for Event Flags
  thread = ef->thread_list;
  while (thread != NULL) {
    thread_next = thread->thread_next;
    event_flags0 = EventFlagsCheck(ef, thread->wait_flags, thread->flags_options);
    if (event_flags0 != 0U) {
      if ((thread->flags_options & osFlagsNoClear) == 0U) {
        event_flags = event_flags0 & ~thread->wait_flags;
      } else {
        event_flags = event_flags0;
      }
      osRtxThreadListRemove(thread);
      osRtxThreadWaitExit(thread, event_flags0, FALSE);
      EvrRtxEventFlagsWaitCompleted(ef, thread->wait_flags, thread->flags_options, event_flags0);
    }
    thread = thread_next;
  }
  osRtxThreadDispatch(NULL);

  EvrRtxEventFlagsSetDone(ef, event_flags);

  return event_flags;
}

/// Clear the specified Event Flags.
/// \note API identical to osEventFlagsClear
static uint32_t svcRtxEventFlagsClear (osEventFlagsId_t ef_id, uint32_t flags) {
  os_event_flags_t *ef = osRtxEventFlagsId(ef_id);
  uint32_t          event_flags;

  // Check parameters
  if ((ef == NULL) || (ef->id != osRtxIdEventFlags) ||
      ((flags & ~(((uint32_t)1U << osRtxEventFlagsLimit) - 1U)) != 0U)) {
    EvrRtxEventFlagsError(ef, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Clear Event Flags
  event_flags = EventFlagsClear(ef, flags);

  EvrRtxEventFlagsClearDone(ef, event_flags);
  
  return event_flags;
}

/// Get the current Event Flags.
/// \note API identical to osEventFlagsGet
static uint32_t svcRtxEventFlagsGet (osEventFlagsId_t ef_id) {
  os_event_flags_t *ef = osRtxEventFlagsId(ef_id);

  // Check parameters
  if ((ef == NULL) || (ef->id != osRtxIdEventFlags)) {
    EvrRtxEventFlagsGet(ef, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxEventFlagsGet(ef, ef->event_flags);

  return ef->event_flags;
}

/// Wait for one or more Event Flags to become signaled.
/// \note API identical to osEventFlagsWait
static uint32_t svcRtxEventFlagsWait (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout) {
  os_event_flags_t *ef = osRtxEventFlagsId(ef_id);
  os_thread_t      *thread;
  uint32_t          event_flags;

  // Check parameters
  if ((ef == NULL) || (ef->id != osRtxIdEventFlags) ||
      ((flags & ~(((uint32_t)1U << osRtxEventFlagsLimit) - 1U)) != 0U)) {
    EvrRtxEventFlagsError(ef, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Check Event Flags
  event_flags = EventFlagsCheck(ef, flags, options);
  if (event_flags != 0U) {
    EvrRtxEventFlagsWaitCompleted(ef, flags, options, event_flags);
  } else {
    // Check if timeout is specified
    if (timeout != 0U) {
      EvrRtxEventFlagsWaitPending(ef, flags, options, timeout);
      // Suspend current Thread
      if (osRtxThreadWaitEnter(osRtxThreadWaitingEventFlags, timeout)) {
        thread = osRtxThreadGetRunning();
        osRtxThreadListPut(osRtxObject(ef), thread);
        // Store waiting flags and options
        thread->wait_flags = flags;
        thread->flags_options = (uint8_t)options;
      } else {
        EvrRtxEventFlagsWaitTimeout(ef);
      }
      event_flags = (uint32_t)osErrorTimeout;
    } else {
      EvrRtxEventFlagsWaitNotCompleted(ef, flags, options);
      event_flags = (uint32_t)osErrorResource;
    }
  }

  return event_flags;
}

/// Delete an Event Flags object.
/// \note API identical to osEventFlagsDelete
static osStatus_t svcRtxEventFlagsDelete (osEventFlagsId_t ef_id) {
  os_event_flags_t *ef = osRtxEventFlagsId(ef_id);
  os_thread_t      *thread;

  // Check parameters
  if ((ef == NULL) || (ef->id != osRtxIdEventFlags)) {
    EvrRtxEventFlagsError(ef, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Unblock waiting threads
  if (ef->thread_list != NULL) {
    do {
      thread = osRtxThreadListGet(osRtxObject(ef));
      osRtxThreadWaitExit(thread, (uint32_t)osErrorResource, FALSE);
    } while (ef->thread_list != NULL);
    osRtxThreadDispatch(NULL);
  }

  // Mark object as invalid
  ef->id = osRtxIdInvalid;

  // Free object memory
  if ((ef->flags & osRtxFlagSystemObject) != 0U) {
    if (osRtxInfo.mpi.event_flags != NULL) {
      (void)osRtxMemoryPoolFree(osRtxInfo.mpi.event_flags, ef);
    } else {
      (void)osRtxMemoryFree(osRtxInfo.mem.common, ef);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    osRtxEventFlagsMemUsage.cnt_free++;
#endif
  }

  EvrRtxEventFlagsDestroyed(ef);

  return osOK;
}

//  Service Calls definitions
//lint ++flb "Library Begin" [MISRA Note 11]
SVC0_1(EventFlagsNew,     osEventFlagsId_t, const osEventFlagsAttr_t *)
SVC0_1(EventFlagsGetName, const char *,     osEventFlagsId_t)
SVC0_2(EventFlagsSet,     uint32_t,         osEventFlagsId_t, uint32_t)
SVC0_2(EventFlagsClear,   uint32_t,         osEventFlagsId_t, uint32_t)
SVC0_1(EventFlagsGet,     uint32_t,         osEventFlagsId_t)
SVC0_4(EventFlagsWait,    uint32_t,         osEventFlagsId_t, uint32_t, uint32_t, uint32_t)
SVC0_1(EventFlagsDelete,  osStatus_t,       osEventFlagsId_t)
//lint --flb "Library End"


//  ==== ISR Calls ====

/// Set the specified Event Flags.
/// \note API identical to osEventFlagsSet
__STATIC_INLINE
uint32_t isrRtxEventFlagsSet (osEventFlagsId_t ef_id, uint32_t flags) {
  os_event_flags_t *ef = osRtxEventFlagsId(ef_id);
  uint32_t          event_flags;

  // Check parameters
  if ((ef == NULL) || (ef->id != osRtxIdEventFlags) ||
      ((flags & ~(((uint32_t)1U << osRtxEventFlagsLimit) - 1U)) != 0U)) {
    EvrRtxEventFlagsError(ef, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Set Event Flags
  event_flags = EventFlagsSet(ef, flags);

  // Register post ISR processing
  osRtxPostProcess(osRtxObject(ef));

  EvrRtxEventFlagsSetDone(ef, event_flags);

  return event_flags;
}

/// Wait for one or more Event Flags to become signaled.
/// \note API identical to osEventFlagsWait
__STATIC_INLINE
uint32_t isrRtxEventFlagsWait (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout) {
  os_event_flags_t *ef = osRtxEventFlagsId(ef_id);
  uint32_t          event_flags;

  // Check parameters
  if ((ef == NULL) || (ef->id != osRtxIdEventFlags) || (timeout != 0U) ||
      ((flags & ~(((uint32_t)1U << osRtxEventFlagsLimit) - 1U)) != 0U)) {
    EvrRtxEventFlagsError(ef, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Check Event Flags
  event_flags = EventFlagsCheck(ef, flags, options);
  if (event_flags != 0U) {
    EvrRtxEventFlagsWaitCompleted(ef, flags, options, event_flags);
  } else {
    EvrRtxEventFlagsWaitNotCompleted(ef, flags, options);
    event_flags = (uint32_t)osErrorResource;
  }

  return event_flags;
}


//  ==== Public API ====

/// Create and Initialize an Event Flags object.
osEventFlagsId_t osEventFlagsNew (const osEventFlagsAttr_t *attr) {
  osEventFlagsId_t ef_id;

  EvrRtxEventFlagsNew(attr);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxEventFlagsError(NULL, (int32_t)osErrorISR);
    ef_id = NULL;
  } else {
    ef_id = __svcEventFlagsNew(attr);
  }
  return ef_id;
}

/// Get name of an Event Flags object.
const char *osEventFlagsGetName (osEventFlagsId_t ef_id) {
  const char *name;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxEventFlagsGetName(ef_id, NULL);
    name = NULL;
  } else {
    name = __svcEventFlagsGetName(ef_id);
  }
  return name;
}

/// Set the specified Event Flags.
uint32_t osEventFlagsSet (osEventFlagsId_t ef_id, uint32_t flags) {
  uint32_t event_flags;

  EvrRtxEventFlagsSet(ef_id, flags);
  if (IsIrqMode() || IsIrqMasked()) {
    event_flags = isrRtxEventFlagsSet(ef_id, flags);
  } else {
    event_flags =  __svcEventFlagsSet(ef_id, flags);
  }
  return event_flags;
}

/// Clear the specified Event Flags.
uint32_t osEventFlagsClear (osEventFlagsId_t ef_id, uint32_t flags) {
  uint32_t event_flags;

  EvrRtxEventFlagsClear(ef_id, flags);
  if (IsIrqMode() || IsIrqMasked()) {
    event_flags = svcRtxEventFlagsClear(ef_id, flags);
  } else {
    event_flags =  __svcEventFlagsClear(ef_id, flags);
  }
  return event_flags;
}

/// Get the current Event Flags.
uint32_t osEventFlagsGet (osEventFlagsId_t ef_id) {
  uint32_t event_flags;

  if (IsIrqMode() || IsIrqMasked()) {
    event_flags = svcRtxEventFlagsGet(ef_id);
  } else {
    event_flags =  __svcEventFlagsGet(ef_id);
  }
  return event_flags;
}

/// Wait for one or more Event Flags to become signaled.
uint32_t osEventFlagsWait (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout) {
  uint32_t event_flags;

  EvrRtxEventFlagsWait(ef_id, flags, options, timeout);
  if (IsIrqMode() || IsIrqMasked()) {
    event_flags = isrRtxEventFlagsWait(ef_id, flags, options, timeout);
  } else {
    event_flags =  __svcEventFlagsWait(ef_id, flags, options, timeout);
  }
  return event_flags;
}

/// Delete an Event Flags object.
osStatus_t osEventFlagsDelete (osEventFlagsId_t ef_id) {
  osStatus_t status;

  EvrRtxEventFlagsDelete(ef_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxEventFlagsError(ef_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcEventFlagsDelete(ef_id);
  }
  return status;
}
