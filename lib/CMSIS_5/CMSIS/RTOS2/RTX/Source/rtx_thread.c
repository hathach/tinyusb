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
 * Title:       Thread functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  OS Runtime Object Memory Usage
#if ((defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0)))
osRtxObjectMemUsage_t osRtxThreadMemUsage \
__attribute__((section(".data.os.thread.obj"))) =
{ 0U, 0U, 0U };
#endif


//  ==== Helper functions ====

/// Set Thread Flags.
/// \param[in]  thread          thread object.
/// \param[in]  flags           specifies the flags to set.
/// \return thread flags after setting.
static uint32_t ThreadFlagsSet (os_thread_t *thread, uint32_t flags) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask = __get_PRIMASK();
#endif
  uint32_t thread_flags;

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  thread->thread_flags |= flags;
  thread_flags = thread->thread_flags;

  if (primask == 0U) {
    __enable_irq();
  }
#else
  thread_flags = atomic_set32(&thread->thread_flags, flags);
#endif

  return thread_flags;
}

/// Clear Thread Flags.
/// \param[in]  thread          thread object.
/// \param[in]  flags           specifies the flags to clear.
/// \return thread flags before clearing.
static uint32_t ThreadFlagsClear (os_thread_t *thread, uint32_t flags) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask = __get_PRIMASK();
#endif
  uint32_t thread_flags;

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  thread_flags = thread->thread_flags;
  thread->thread_flags &= ~flags;

  if (primask == 0U) {
    __enable_irq();
  }
#else
  thread_flags = atomic_clr32(&thread->thread_flags, flags);
#endif

  return thread_flags;
}

/// Check Thread Flags.
/// \param[in]  thread          thread object.
/// \param[in]  flags           specifies the flags to check.
/// \param[in]  options         specifies flags options (osFlagsXxxx).
/// \return thread flags before clearing or 0 if specified flags have not been set.
static uint32_t ThreadFlagsCheck (os_thread_t *thread, uint32_t flags, uint32_t options) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask;
#endif
  uint32_t thread_flags;

  if ((options & osFlagsNoClear) == 0U) {
#if (EXCLUSIVE_ACCESS == 0)
    primask = __get_PRIMASK();
    __disable_irq();

    thread_flags = thread->thread_flags;
    if ((((options & osFlagsWaitAll) != 0U) && ((thread_flags & flags) != flags)) ||
        (((options & osFlagsWaitAll) == 0U) && ((thread_flags & flags) == 0U))) {
      thread_flags = 0U;
    } else {
      thread->thread_flags &= ~flags;
    }

    if (primask == 0U) {
      __enable_irq();
    }
#else
    if ((options & osFlagsWaitAll) != 0U) {
      thread_flags = atomic_chk32_all(&thread->thread_flags, flags);
    } else {
      thread_flags = atomic_chk32_any(&thread->thread_flags, flags);
    }
#endif
  } else {
    thread_flags = thread->thread_flags;
    if ((((options & osFlagsWaitAll) != 0U) && ((thread_flags & flags) != flags)) ||
        (((options & osFlagsWaitAll) == 0U) && ((thread_flags & flags) == 0U))) {
      thread_flags = 0U;
    }
  }

  return thread_flags;
}


//  ==== Library functions ====

/// Put a Thread into specified Object list sorted by Priority (Highest at Head).
/// \param[in]  object          generic object.
/// \param[in]  thread          thread object.
void osRtxThreadListPut (os_object_t *object, os_thread_t *thread) {
  os_thread_t *prev, *next;
  int32_t      priority;

  priority = thread->priority;

  prev = osRtxThreadObject(object);
  next = prev->thread_next;
  while ((next != NULL) && (next->priority >= priority)) {
    prev = next;
    next = next->thread_next;
  }
  thread->thread_prev = prev;
  thread->thread_next = next;
  prev->thread_next = thread;
  if (next != NULL) {
    next->thread_prev = thread;
  }
}

/// Get a Thread with Highest Priority from specified Object list and remove it.
/// \param[in]  object          generic object.
/// \return thread object.
os_thread_t *osRtxThreadListGet (os_object_t *object) {
  os_thread_t *thread;

  thread = object->thread_list;
  object->thread_list = thread->thread_next;
  if (thread->thread_next != NULL) {
    thread->thread_next->thread_prev = osRtxThreadObject(object);
  }
  thread->thread_prev = NULL;

  return thread;
}

/// Retrieve Thread list root object.
/// \param[in]  thread          thread object.
/// \return root object.
static void *osRtxThreadListRoot (os_thread_t *thread) {
  os_thread_t *thread0;

  thread0 = thread;
  while (thread0->id == osRtxIdThread) {
    thread0 = thread0->thread_prev;
  }
  return thread0;
}

/// Re-sort a Thread in linked Object list by Priority (Highest at Head).
/// \param[in]  thread          thread object.
void osRtxThreadListSort (os_thread_t *thread) {
  os_object_t *object;
  os_thread_t *thread0;

  // Search for object
  thread0 = thread;
  while ((thread0 != NULL) && (thread0->id == osRtxIdThread)) {
    thread0 = thread0->thread_prev;
  }
  object = osRtxObject(thread0);

  if (object != NULL) {
    osRtxThreadListRemove(thread);
    osRtxThreadListPut(object, thread);
  }
}

/// Remove a Thread from linked Object list.
/// \param[in]  thread          thread object.
void osRtxThreadListRemove (os_thread_t *thread) {

  if (thread->thread_prev != NULL) {
    thread->thread_prev->thread_next = thread->thread_next;
    if (thread->thread_next != NULL) {
      thread->thread_next->thread_prev = thread->thread_prev;
    }
    thread->thread_prev = NULL;
  }
}

/// Unlink a Thread from specified linked list.
/// \param[in]  thread          thread object.
static void osRtxThreadListUnlink (os_thread_t **thread_list, os_thread_t *thread) {

  if (thread->thread_next != NULL) {
    thread->thread_next->thread_prev = thread->thread_prev;
  }
  if (thread->thread_prev != NULL) {
    thread->thread_prev->thread_next = thread->thread_next;
    thread->thread_prev = NULL;
  } else {
    *thread_list = thread->thread_next;
  }
}

/// Mark a Thread as Ready and put it into Ready list (sorted by Priority).
/// \param[in]  thread          thread object.
void osRtxThreadReadyPut (os_thread_t *thread) {

  thread->state = osRtxThreadReady;
  osRtxThreadListPut(&osRtxInfo.thread.ready, thread);
}

/// Insert a Thread into the Delay list sorted by Delay (Lowest at Head).
/// \param[in]  thread          thread object.
/// \param[in]  delay           delay value.
static void osRtxThreadDelayInsert (os_thread_t *thread, uint32_t delay) {
  os_thread_t *prev, *next;

  if (delay == osWaitForever) {
    prev = NULL;
    next = osRtxInfo.thread.wait_list;
    while (next != NULL)  {
      prev = next;
      next = next->delay_next;
    }
    thread->delay = delay;
    thread->delay_prev = prev;
    thread->delay_next = NULL;
    if (prev != NULL) {
      prev->delay_next = thread;
    } else {
      osRtxInfo.thread.wait_list = thread;
    }
  } else {
    prev = NULL;
    next = osRtxInfo.thread.delay_list;
    while ((next != NULL) && (next->delay <= delay)) {
      delay -= next->delay;
      prev = next;
      next = next->delay_next;
    }
    thread->delay = delay;
    thread->delay_prev = prev;
    thread->delay_next = next;
    if (prev != NULL) {
      prev->delay_next = thread;
    } else {
      osRtxInfo.thread.delay_list = thread;
    }
    if (next != NULL) {
      next->delay -= delay;
      next->delay_prev = thread;
    }
  }
}

/// Remove a Thread from the Delay list.
/// \param[in]  thread          thread object.
static void osRtxThreadDelayRemove (os_thread_t *thread) {

  if (thread->delay == osWaitForever) {
    if (thread->delay_next != NULL) {
      thread->delay_next->delay_prev = thread->delay_prev;
    }
    if (thread->delay_prev != NULL) {
      thread->delay_prev->delay_next = thread->delay_next;
      thread->delay_prev = NULL;
    } else {
      osRtxInfo.thread.wait_list = thread->delay_next;
    }
  } else {
    if (thread->delay_next != NULL) {
      thread->delay_next->delay += thread->delay;
      thread->delay_next->delay_prev = thread->delay_prev;
    }
    if (thread->delay_prev != NULL) {
      thread->delay_prev->delay_next = thread->delay_next;
      thread->delay_prev = NULL;
    } else {
      osRtxInfo.thread.delay_list = thread->delay_next;
    }
  }
}

/// Process Thread Delay Tick (executed each System Tick).
void osRtxThreadDelayTick (void) {
  os_thread_t *thread;
  os_object_t *object;

  thread = osRtxInfo.thread.delay_list;
  if (thread == NULL) {
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return;
  }

  thread->delay--;

  if (thread->delay == 0U) {
    do {
      switch (thread->state) {
        case osRtxThreadWaitingDelay:
          EvrRtxDelayCompleted(thread);
          break;
        case osRtxThreadWaitingThreadFlags:
          EvrRtxThreadFlagsWaitTimeout(thread);
          break;
        case osRtxThreadWaitingEventFlags:
          object = osRtxObject(osRtxThreadListRoot(thread));
          EvrRtxEventFlagsWaitTimeout(osRtxEventFlagsObject(object));
          break;
        case osRtxThreadWaitingMutex:
          object = osRtxObject(osRtxThreadListRoot(thread));
          osRtxMutexOwnerRestore(osRtxMutexObject(object), thread);
          EvrRtxMutexAcquireTimeout(osRtxMutexObject(object));
          break;
        case osRtxThreadWaitingSemaphore:
          object = osRtxObject(osRtxThreadListRoot(thread));
          EvrRtxSemaphoreAcquireTimeout(osRtxSemaphoreObject(object));
          break;
        case osRtxThreadWaitingMemoryPool:
          object = osRtxObject(osRtxThreadListRoot(thread));
          EvrRtxMemoryPoolAllocTimeout(osRtxMemoryPoolObject(object));
          break;
        case osRtxThreadWaitingMessageGet:
          object = osRtxObject(osRtxThreadListRoot(thread));
          EvrRtxMessageQueueGetTimeout(osRtxMessageQueueObject(object));
          break;
        case osRtxThreadWaitingMessagePut:
          object = osRtxObject(osRtxThreadListRoot(thread));
          EvrRtxMessageQueuePutTimeout(osRtxMessageQueueObject(object));
          break;
        default:
          // Invalid
          break;
      }
      EvrRtxThreadUnblocked(thread, (osRtxThreadRegPtr(thread))[0]);
      osRtxThreadListRemove(thread);
      osRtxThreadReadyPut(thread);
      thread = thread->delay_next;
    } while ((thread != NULL) && (thread->delay == 0U));
    if (thread != NULL) {
      thread->delay_prev = NULL;
    }
    osRtxInfo.thread.delay_list = thread;
  }
}

/// Get pointer to Thread registers (R0..R3)
/// \param[in]  thread          thread object.
/// \return pointer to registers R0-R3.
uint32_t *osRtxThreadRegPtr (const os_thread_t *thread) {
  uint32_t addr = thread->sp + StackOffsetR0(thread->stack_frame);
  //lint -e{923} -e{9078} "cast from unsigned int to pointer"
  return ((uint32_t *)addr);
}

/// Block running Thread execution and register it as Ready to Run.
/// \param[in]  thread          running thread object.
static void osRtxThreadBlock (os_thread_t *thread) {
  os_thread_t *prev, *next;
  int32_t      priority;

  thread->state = osRtxThreadReady;

  priority = thread->priority;

  prev = osRtxThreadObject(&osRtxInfo.thread.ready);
  next = prev->thread_next;

  while ((next != NULL) && (next->priority > priority)) {
    prev = next;
    next = next->thread_next;
  }
  thread->thread_prev = prev;
  thread->thread_next = next;
  prev->thread_next = thread;
  if (next != NULL) {
    next->thread_prev = thread;
  }

  EvrRtxThreadPreempted(thread);
}

/// Switch to specified Thread.
/// \param[in]  thread          thread object.
void osRtxThreadSwitch (os_thread_t *thread) {

  thread->state = osRtxThreadRunning;
  osRtxInfo.thread.run.next = thread;
  osRtxThreadStackCheck();
  EvrRtxThreadSwitched(thread);
}

/// Dispatch specified Thread or Ready Thread with Highest Priority.
/// \param[in]  thread          thread object or NULL.
void osRtxThreadDispatch (os_thread_t *thread) {
  uint8_t      kernel_state;
  os_thread_t *thread_running;
  os_thread_t *thread_ready;

  kernel_state   = osRtxKernelGetState();
  thread_running = osRtxThreadGetRunning();

  if (thread == NULL) {
    thread_ready = osRtxInfo.thread.ready.thread_list;
    if ((kernel_state == osRtxKernelRunning) &&
        (thread_ready != NULL) &&
        (thread_ready->priority > thread_running->priority)) {
      // Preempt running Thread
      osRtxThreadListRemove(thread_ready);
      osRtxThreadBlock(thread_running);
      osRtxThreadSwitch(thread_ready);
    }
  } else {
    if ((kernel_state == osRtxKernelRunning) &&
        (thread->priority > thread_running->priority)) {
      // Preempt running Thread
      osRtxThreadBlock(thread_running);
      osRtxThreadSwitch(thread);
    } else {
      // Put Thread into Ready list
      osRtxThreadReadyPut(thread);
    }
  }
}

/// Exit Thread wait state.
/// \param[in]  thread          thread object.
/// \param[in]  ret_val         return value.
/// \param[in]  dispatch        dispatch flag.
void osRtxThreadWaitExit (os_thread_t *thread, uint32_t ret_val, bool_t dispatch) {
  uint32_t *reg;

  EvrRtxThreadUnblocked(thread, ret_val);

  reg = osRtxThreadRegPtr(thread);
  reg[0] = ret_val;

  osRtxThreadDelayRemove(thread);
  if (dispatch) {
    osRtxThreadDispatch(thread);
  } else {
    osRtxThreadReadyPut(thread);
  }
}

/// Enter Thread wait state.
/// \param[in]  state           new thread state.
/// \param[in]  timeout         timeout.
/// \return true - success, false - failure.
bool_t osRtxThreadWaitEnter (uint8_t state, uint32_t timeout) {
  os_thread_t *thread;

  // Check if Kernel is running
  if (osRtxKernelGetState() != osRtxKernelRunning) {
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return FALSE;
  }

  // Check if any thread is ready
  if (osRtxInfo.thread.ready.thread_list == NULL) {
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return FALSE;
  }

  // Get running thread
  thread = osRtxThreadGetRunning();

  EvrRtxThreadBlocked(thread, timeout);

  thread->state = state;
  osRtxThreadDelayInsert(thread, timeout);
  thread = osRtxThreadListGet(&osRtxInfo.thread.ready);
  osRtxThreadSwitch(thread);

  return TRUE;
}

/// Check current running Thread Stack.
//lint -esym(759,osRtxThreadStackCheck) "Prototype in header"
//lint -esym(765,osRtxThreadStackCheck) "Global scope (can be overridden)"
__WEAK void osRtxThreadStackCheck (void) {
  os_thread_t *thread;

  thread = osRtxThreadGetRunning();
  if (thread != NULL) {
    //lint -e{923} "cast from pointer to unsigned int"
    //lint -e{9079} -e{9087} "cast between pointers to different object types"
    if ((thread->sp <= (uint32_t)thread->stack_mem) ||
        (*((uint32_t *)thread->stack_mem) != osRtxStackMagicWord)) {
      (void)osRtxErrorNotify(osRtxErrorStackUnderflow, thread);
    }
  }
}

#ifdef RTX_TF_M_EXTENSION
/// Get TrustZone Module Identifier of running Thread.
/// \return TrustZone Module Identifier.
uint32_t osRtxTzGetModuleId (void) {
  os_thread_t *thread;
  uint32_t     tz_module;

  thread = osRtxThreadGetRunning();
  if (thread != NULL) {
    tz_module = thread->tz_module;
  } else {
    tz_module = 0U;
  }

  return tz_module;
}
#endif


//  ==== Post ISR processing ====

/// Thread post ISR processing.
/// \param[in]  thread          thread object.
static void osRtxThreadPostProcess (os_thread_t *thread) {
  uint32_t thread_flags;

  // Check if Thread is waiting for Thread Flags
  if (thread->state == osRtxThreadWaitingThreadFlags) {
    thread_flags = ThreadFlagsCheck(thread, thread->wait_flags, thread->flags_options);
    if (thread_flags != 0U) {
      osRtxThreadWaitExit(thread, thread_flags, FALSE);
      EvrRtxThreadFlagsWaitCompleted(thread->wait_flags, thread->flags_options, thread_flags, thread);
    }
  }
}


//  ==== Service Calls ====

/// Create a thread and add it to Active Threads.
/// \note API identical to osThreadNew
static osThreadId_t svcRtxThreadNew (osThreadFunc_t func, void *argument, const osThreadAttr_t *attr) {
  os_thread_t  *thread;
  uint32_t      attr_bits;
  void         *stack_mem;
  uint32_t      stack_size;
  osPriority_t  priority;
  uint8_t       flags;
  const char   *name;
  uint32_t     *ptr;
  uint32_t      n;
#if (DOMAIN_NS == 1)
  TZ_ModuleId_t tz_module;
  TZ_MemoryId_t tz_memory;
#endif

  // Check parameters
  if (func == NULL) {
    EvrRtxThreadError(NULL, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  // Process attributes
  if (attr != NULL) {
    name       = attr->name;
    attr_bits  = attr->attr_bits;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    thread     = attr->cb_mem;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    stack_mem  = attr->stack_mem;
    stack_size = attr->stack_size;
    priority   = attr->priority;
#if (DOMAIN_NS == 1)
    tz_module  = attr->tz_module;
#endif
    if (thread != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)thread & 3U) != 0U) || (attr->cb_size < sizeof(os_thread_t))) {
        EvrRtxThreadError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    } else {
      if (attr->cb_size != 0U) {
        EvrRtxThreadError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
    if (stack_mem != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)stack_mem & 7U) != 0U) || (stack_size == 0U)) {
        EvrRtxThreadError(NULL, osRtxErrorInvalidThreadStack);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
    if (priority == osPriorityNone) {
      priority = osPriorityNormal;
    } else {
      if ((priority < osPriorityIdle) || (priority > osPriorityISR)) {
        EvrRtxThreadError(NULL, osRtxErrorInvalidPriority);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
  } else {
    name       = NULL;
    attr_bits  = 0U;
    thread     = NULL;
    stack_mem  = NULL;
    stack_size = 0U;
    priority   = osPriorityNormal;
#if (DOMAIN_NS == 1)
    tz_module  = 0U;
#endif
  }

  // Check stack size
  if ((stack_size != 0U) && (((stack_size & 7U) != 0U) || (stack_size < (64U + 8U)))) {
    EvrRtxThreadError(NULL, osRtxErrorInvalidThreadStack);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  // Allocate object memory if not provided
  if (thread == NULL) {
    if (osRtxInfo.mpi.thread != NULL) {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      thread = osRtxMemoryPoolAlloc(osRtxInfo.mpi.thread);
    } else {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      thread = osRtxMemoryAlloc(osRtxInfo.mem.common, sizeof(os_thread_t), 1U);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    if (thread != NULL) {
      uint32_t used;
      osRtxThreadMemUsage.cnt_alloc++;
      used = osRtxThreadMemUsage.cnt_alloc - osRtxThreadMemUsage.cnt_free;
      if (osRtxThreadMemUsage.max_used < used) {
        osRtxThreadMemUsage.max_used = used;
      }
    }
#endif
    flags = osRtxFlagSystemObject;
  } else {
    flags = 0U;
  }

  // Allocate stack memory if not provided
  if ((thread != NULL) && (stack_mem == NULL)) {
    if (stack_size == 0U) {
      stack_size = osRtxConfig.thread_stack_size;
      if (osRtxInfo.mpi.stack != NULL) {
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
        stack_mem = osRtxMemoryPoolAlloc(osRtxInfo.mpi.stack);
        if (stack_mem != NULL) {
          flags |= osRtxThreadFlagDefStack;
        }
      } else {
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
        stack_mem = osRtxMemoryAlloc(osRtxInfo.mem.stack, stack_size, 0U);
      }
    } else {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      stack_mem = osRtxMemoryAlloc(osRtxInfo.mem.stack, stack_size, 0U);
    }
    if (stack_mem == NULL) {
      if ((flags & osRtxFlagSystemObject) != 0U) {
        if (osRtxInfo.mpi.thread != NULL) {
          (void)osRtxMemoryPoolFree(osRtxInfo.mpi.thread, thread);
        } else {
          (void)osRtxMemoryFree(osRtxInfo.mem.common, thread);
        }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
        osRtxThreadMemUsage.cnt_free++;
#endif
      }
      thread = NULL;
    }
    flags |= osRtxFlagSystemMemory;
  }

#if (DOMAIN_NS == 1)
  // Allocate secure process stack
  if ((thread != NULL) && (tz_module != 0U)) {
    tz_memory = TZ_AllocModuleContext_S(tz_module);
    if (tz_memory == 0U) {
      EvrRtxThreadError(NULL, osRtxErrorTZ_AllocContext_S);
      if ((flags & osRtxFlagSystemMemory) != 0U) {
        if ((flags & osRtxThreadFlagDefStack) != 0U) {
          (void)osRtxMemoryPoolFree(osRtxInfo.mpi.stack, thread->stack_mem);
        } else {
          (void)osRtxMemoryFree(osRtxInfo.mem.stack, thread->stack_mem);
        }
      }
      if ((flags & osRtxFlagSystemObject) != 0U) {
        if (osRtxInfo.mpi.thread != NULL) {
          (void)osRtxMemoryPoolFree(osRtxInfo.mpi.thread, thread);
        } else {
          (void)osRtxMemoryFree(osRtxInfo.mem.common, thread);
        }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
        osRtxThreadMemUsage.cnt_free++;
#endif
      }
      thread = NULL;
    }
  } else {
    tz_memory = 0U;
  }
#endif

  if (thread != NULL) {
    // Initialize control block
    //lint --e{923}  --e{9078} "cast between pointers and unsigned int"
    //lint --e{9079} --e{9087} "cast between pointers to different object types"
    //lint --e{9074} "conversion between a pointer to function and another type"
    thread->id            = osRtxIdThread;
    thread->state         = osRtxThreadReady;
    thread->flags         = flags;
    thread->attr          = (uint8_t)attr_bits;
    thread->name          = name;
    thread->thread_next   = NULL;
    thread->thread_prev   = NULL;
    thread->delay_next    = NULL;
    thread->delay_prev    = NULL;
    thread->thread_join   = NULL;
    thread->delay         = 0U;
    thread->priority      = (int8_t)priority;
    thread->priority_base = (int8_t)priority;
    thread->stack_frame   = STACK_FRAME_INIT_VAL;
    thread->flags_options = 0U;
    thread->wait_flags    = 0U;
    thread->thread_flags  = 0U;
    thread->mutex_list    = NULL;
    thread->stack_mem     = stack_mem;
    thread->stack_size    = stack_size;
    thread->sp            = (uint32_t)stack_mem + stack_size - 64U;
    thread->thread_addr   = (uint32_t)func;
  #if (DOMAIN_NS == 1)
    thread->tz_memory     = tz_memory;
  #ifdef RTX_TF_M_EXTENSION
    thread->tz_module     = tz_module;
  #endif
  #endif

    // Initialize stack
    //lint --e{613} false detection: "Possible use of null pointer"
    ptr = (uint32_t *)stack_mem;
    ptr[0] = osRtxStackMagicWord;
    if ((osRtxConfig.flags & osRtxConfigStackWatermark) != 0U) {
      for (n = (stack_size/4U) - (16U + 1U); n != 0U; n--) {
         ptr++;
        *ptr = osRtxStackFillPattern;
      }
    }
    ptr = (uint32_t *)thread->sp;
    for (n = 0U; n != 13U; n++) {
      ptr[n] = 0U;                      // R4..R11, R0..R3, R12
    }
    ptr[13] = (uint32_t)osThreadExit;   // LR
    ptr[14] = (uint32_t)func;           // PC
    ptr[15] = xPSR_InitVal(
                (bool_t)((osRtxConfig.flags & osRtxConfigPrivilegedMode) != 0U),
                (bool_t)(((uint32_t)func & 1U) != 0U)
              );                        // xPSR
    ptr[8]  = (uint32_t)argument;       // R0

    // Register post ISR processing function
    osRtxInfo.post_process.thread = osRtxThreadPostProcess;

    EvrRtxThreadCreated(thread, thread->thread_addr, thread->name);
  } else {
    EvrRtxThreadError(NULL, (int32_t)osErrorNoMemory);
  }
  
  if (thread != NULL) {
    osRtxThreadDispatch(thread);
  }

  return thread;
}

/// Get name of a thread.
/// \note API identical to osThreadGetName
static const char *svcRtxThreadGetName (osThreadId_t thread_id) {
  os_thread_t *thread = osRtxThreadId(thread_id);

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadGetName(thread, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  EvrRtxThreadGetName(thread, thread->name);

  return thread->name;
}

/// Return the thread ID of the current running thread.
/// \note API identical to osThreadGetId
static osThreadId_t svcRtxThreadGetId (void) {
  os_thread_t *thread;

  thread = osRtxThreadGetRunning();
  EvrRtxThreadGetId(thread);
  return thread;
}

/// Get current thread state of a thread.
/// \note API identical to osThreadGetState
static osThreadState_t svcRtxThreadGetState (osThreadId_t thread_id) {
  os_thread_t    *thread = osRtxThreadId(thread_id);
  osThreadState_t state;

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadGetState(thread, osThreadError);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osThreadError;
  }

  state = osRtxThreadState(thread);

  EvrRtxThreadGetState(thread, state);

  return state;
}

/// Get stack size of a thread.
/// \note API identical to osThreadGetStackSize
static uint32_t svcRtxThreadGetStackSize (osThreadId_t thread_id) {
  os_thread_t *thread = osRtxThreadId(thread_id);

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadGetStackSize(thread, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxThreadGetStackSize(thread, thread->stack_size);

  return thread->stack_size;
}

/// Get available stack space of a thread based on stack watermark recording during execution.
/// \note API identical to osThreadGetStackSpace
static uint32_t svcRtxThreadGetStackSpace (osThreadId_t thread_id) {
  os_thread_t    *thread = osRtxThreadId(thread_id);
  const uint32_t *stack;
        uint32_t  space;

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadGetStackSpace(thread, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  // Check if stack watermark is not enabled
  if ((osRtxConfig.flags & osRtxConfigStackWatermark) == 0U) {
    EvrRtxThreadGetStackSpace(thread, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  //lint -e{9079} "conversion from pointer to void to pointer to other type"
  stack = thread->stack_mem;
  if (*stack++ == osRtxStackMagicWord) {
    for (space = 4U; space < thread->stack_size; space += 4U) {
      if (*stack++ != osRtxStackFillPattern) {
        break;
      }
    }
  } else {
    space = 0U;
  }

  EvrRtxThreadGetStackSpace(thread, space);

  return space;
}

/// Change priority of a thread.
/// \note API identical to osThreadSetPriority
static osStatus_t svcRtxThreadSetPriority (osThreadId_t thread_id, osPriority_t priority) {
  os_thread_t *thread = osRtxThreadId(thread_id);

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread) ||
      (priority < osPriorityIdle) || (priority > osPriorityISR)) {
    EvrRtxThreadError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check object state
  if (thread->state == osRtxThreadTerminated) {
    EvrRtxThreadError(thread, (int32_t)osErrorResource);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorResource;
  }

  if (thread->priority   != (int8_t)priority) {
    thread->priority      = (int8_t)priority;
    thread->priority_base = (int8_t)priority;
    EvrRtxThreadPriorityUpdated(thread, priority);
    osRtxThreadListSort(thread);
    osRtxThreadDispatch(NULL);
  }

  return osOK;
}

/// Get current priority of a thread.
/// \note API identical to osThreadGetPriority
static osPriority_t svcRtxThreadGetPriority (osThreadId_t thread_id) {
  os_thread_t *thread = osRtxThreadId(thread_id);
  osPriority_t priority;

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadGetPriority(thread, osPriorityError);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osPriorityError;
  }

  // Check object state
  if (thread->state == osRtxThreadTerminated) {
    EvrRtxThreadGetPriority(thread, osPriorityError);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osPriorityError;
  }

  priority = osRtxThreadPriority(thread);

  EvrRtxThreadGetPriority(thread, priority);

  return priority;
}

/// Pass control to next thread that is in state READY.
/// \note API identical to osThreadYield
static osStatus_t svcRtxThreadYield (void) {
  os_thread_t *thread_running;
  os_thread_t *thread_ready;

  if (osRtxKernelGetState() == osRtxKernelRunning) {
    thread_running = osRtxThreadGetRunning();
    thread_ready   = osRtxInfo.thread.ready.thread_list;
    if ((thread_ready != NULL) &&
        (thread_ready->priority == thread_running->priority)) {
      osRtxThreadListRemove(thread_ready);
      osRtxThreadReadyPut(thread_running);
      EvrRtxThreadPreempted(thread_running);
      osRtxThreadSwitch(thread_ready);
    }
  }

  return osOK;
}

/// Suspend execution of a thread.
/// \note API identical to osThreadSuspend
static osStatus_t svcRtxThreadSuspend (osThreadId_t thread_id) {
  os_thread_t *thread = osRtxThreadId(thread_id);
  osStatus_t   status;

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check object state
  switch (thread->state & osRtxThreadStateMask) {
    case osRtxThreadRunning:
      if ((osRtxKernelGetState() != osRtxKernelRunning) ||
          (osRtxInfo.thread.ready.thread_list == NULL)) {
        EvrRtxThreadError(thread, (int32_t)osErrorResource);
        status = osErrorResource;
      } else {
        status = osOK;
      }
      break;
    case osRtxThreadReady:
      osRtxThreadListRemove(thread);
      status = osOK;
      break;
    case osRtxThreadBlocked:
      osRtxThreadListRemove(thread);
      osRtxThreadDelayRemove(thread);
      status = osOK;
      break;
    case osRtxThreadInactive:
    case osRtxThreadTerminated:
    default:
      EvrRtxThreadError(thread, (int32_t)osErrorResource);
      status = osErrorResource;
      break;
  }

  if (status == osOK) {
    EvrRtxThreadSuspended(thread);

    if (thread->state == osRtxThreadRunning) {
      osRtxThreadSwitch(osRtxThreadListGet(&osRtxInfo.thread.ready));
    }

    // Update Thread State and put it into Delay list
    thread->state = osRtxThreadBlocked;
    thread->thread_prev = NULL;
    thread->thread_next = NULL;
    osRtxThreadDelayInsert(thread, osWaitForever);
  }

  return status;
}

/// Resume execution of a thread.
/// \note API identical to osThreadResume
static osStatus_t svcRtxThreadResume (osThreadId_t thread_id) {
  os_thread_t *thread = osRtxThreadId(thread_id);

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check object state
  if ((thread->state & osRtxThreadStateMask) != osRtxThreadBlocked) {
    EvrRtxThreadError(thread, (int32_t)osErrorResource);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorResource;
  }

  EvrRtxThreadResumed(thread);

  // Wakeup Thread
  osRtxThreadListRemove(thread);
  osRtxThreadDelayRemove(thread);
  osRtxThreadDispatch(thread);

  return osOK;
}

/// Free Thread resources.
/// \param[in]  thread          thread object.
static void osRtxThreadFree (os_thread_t *thread) {

  // Mark object as inactive and invalid
  thread->state = osRtxThreadInactive;
  thread->id    = osRtxIdInvalid;

#if (DOMAIN_NS == 1)
  // Free secure process stack
  if (thread->tz_memory != 0U) {
    (void)TZ_FreeModuleContext_S(thread->tz_memory);
  }
#endif

  // Free stack memory
  if ((thread->flags & osRtxFlagSystemMemory) != 0U) {
    if ((thread->flags & osRtxThreadFlagDefStack) != 0U) {
      (void)osRtxMemoryPoolFree(osRtxInfo.mpi.stack, thread->stack_mem);
    } else {
      (void)osRtxMemoryFree(osRtxInfo.mem.stack, thread->stack_mem);
    }
  }

  // Free object memory
  if ((thread->flags & osRtxFlagSystemObject) != 0U) {
    if (osRtxInfo.mpi.thread != NULL) {
      (void)osRtxMemoryPoolFree(osRtxInfo.mpi.thread, thread);
    } else {
      (void)osRtxMemoryFree(osRtxInfo.mem.common, thread);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    osRtxThreadMemUsage.cnt_free++;
#endif
  }
}

/// Detach a thread (thread storage can be reclaimed when thread terminates).
/// \note API identical to osThreadDetach
static osStatus_t svcRtxThreadDetach (osThreadId_t thread_id) {
  os_thread_t *thread = osRtxThreadId(thread_id);

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check object attributes
  if ((thread->attr & osThreadJoinable) == 0U) {
    EvrRtxThreadError(thread, osRtxErrorThreadNotJoinable);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorResource;
  }

  if (thread->state == osRtxThreadTerminated) {
    osRtxThreadListUnlink(&osRtxInfo.thread.terminate_list, thread);
    osRtxThreadFree(thread);
  } else {
    thread->attr &= ~osThreadJoinable;
  }

  EvrRtxThreadDetached(thread);

  return osOK;
}

/// Wait for specified thread to terminate.
/// \note API identical to osThreadJoin
static osStatus_t svcRtxThreadJoin (osThreadId_t thread_id) {
  os_thread_t *thread = osRtxThreadId(thread_id);
  osStatus_t   status;

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check object attributes
  if ((thread->attr & osThreadJoinable) == 0U) {
    EvrRtxThreadError(thread, osRtxErrorThreadNotJoinable);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorResource;
  }

  // Check object state
  if (thread->state == osRtxThreadRunning) {
    EvrRtxThreadError(thread, (int32_t)osErrorResource);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorResource;
  }

  if (thread->state == osRtxThreadTerminated) {
    osRtxThreadListUnlink(&osRtxInfo.thread.terminate_list, thread);
    osRtxThreadFree(thread);
    EvrRtxThreadJoined(thread);
    status = osOK;
  } else {
    // Suspend current Thread
    if (osRtxThreadWaitEnter(osRtxThreadWaitingJoin, osWaitForever)) {
      thread->thread_join = osRtxThreadGetRunning();
      thread->attr &= ~osThreadJoinable;
      EvrRtxThreadJoinPending(thread);
    } else {
      EvrRtxThreadError(thread, (int32_t)osErrorResource);
    }
    status = osErrorResource;
  }

  return status;
}

/// Terminate execution of current running thread.
/// \note API identical to osThreadExit
static void svcRtxThreadExit (void) {
  os_thread_t *thread;

  // Check if switch to next Ready Thread is possible
  if ((osRtxKernelGetState() != osRtxKernelRunning) ||
      (osRtxInfo.thread.ready.thread_list == NULL)) {
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return;
  }

  // Get running thread
  thread = osRtxThreadGetRunning();

  // Release owned Mutexes
  osRtxMutexOwnerRelease(thread->mutex_list);

  // Wakeup Thread waiting to Join
  if (thread->thread_join != NULL) {
    osRtxThreadWaitExit(thread->thread_join, (uint32_t)osOK, FALSE);
    EvrRtxThreadJoined(thread->thread_join);
  }

  // Switch to next Ready Thread
  thread->sp = __get_PSP();
  osRtxThreadSwitch(osRtxThreadListGet(&osRtxInfo.thread.ready));
  osRtxThreadSetRunning(NULL);

  if ((thread->attr & osThreadJoinable) == 0U) {
    osRtxThreadFree(thread);
  } else {
    // Update Thread State and put it into Terminate Thread list
    thread->state = osRtxThreadTerminated;
    thread->thread_prev = NULL;
    thread->thread_next = osRtxInfo.thread.terminate_list;
    if (osRtxInfo.thread.terminate_list != NULL) {
      osRtxInfo.thread.terminate_list->thread_prev = thread;
    }
    osRtxInfo.thread.terminate_list = thread;
  }

  EvrRtxThreadDestroyed(thread);
}

/// Terminate execution of a thread.
/// \note API identical to osThreadTerminate
static osStatus_t svcRtxThreadTerminate (osThreadId_t thread_id) {
  os_thread_t *thread = osRtxThreadId(thread_id);
  osStatus_t   status;

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread)) {
    EvrRtxThreadError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check object state
  switch (thread->state & osRtxThreadStateMask) {
    case osRtxThreadRunning:
      if ((osRtxKernelGetState() != osRtxKernelRunning) ||
          (osRtxInfo.thread.ready.thread_list == NULL)) {
        EvrRtxThreadError(thread, (int32_t)osErrorResource);
        status = osErrorResource;
      } else {
        status = osOK;
      }
      break;
    case osRtxThreadReady:
      osRtxThreadListRemove(thread);
      status = osOK;
      break;
    case osRtxThreadBlocked:
      osRtxThreadListRemove(thread);
      osRtxThreadDelayRemove(thread);
      status = osOK;
      break;
    case osRtxThreadInactive:
    case osRtxThreadTerminated:
    default:
      EvrRtxThreadError(thread, (int32_t)osErrorResource);
      status = osErrorResource;
      break;
  }

  if (status == osOK) {
    // Release owned Mutexes
    osRtxMutexOwnerRelease(thread->mutex_list);

    // Wakeup Thread waiting to Join
    if (thread->thread_join != NULL) {
      osRtxThreadWaitExit(thread->thread_join, (uint32_t)osOK, FALSE);
      EvrRtxThreadJoined(thread->thread_join);
    }

    // Switch to next Ready Thread when terminating running Thread
    if (thread->state == osRtxThreadRunning) {
      thread->sp = __get_PSP();
      osRtxThreadSwitch(osRtxThreadListGet(&osRtxInfo.thread.ready));
      osRtxThreadSetRunning(NULL);
    } else {
      osRtxThreadDispatch(NULL);
    }

    if ((thread->attr & osThreadJoinable) == 0U) {
      osRtxThreadFree(thread);
    } else {
      // Update Thread State and put it into Terminate Thread list
      thread->state = osRtxThreadTerminated;
      thread->thread_prev = NULL;
      thread->thread_next = osRtxInfo.thread.terminate_list;
      if (osRtxInfo.thread.terminate_list != NULL) {
        osRtxInfo.thread.terminate_list->thread_prev = thread;
      }
      osRtxInfo.thread.terminate_list = thread;
    }

    EvrRtxThreadDestroyed(thread);
  }

  return status;
}

/// Get number of active threads.
/// \note API identical to osThreadGetCount
static uint32_t svcRtxThreadGetCount (void) {
  const os_thread_t *thread;
        uint32_t     count;

  // Running Thread
  count = 1U;

  // Ready List
  for (thread = osRtxInfo.thread.ready.thread_list;
       thread != NULL; thread = thread->thread_next) {
    count++;
  }

  // Delay List
  for (thread = osRtxInfo.thread.delay_list;
       thread != NULL; thread = thread->delay_next) {
    count++;
  }

  // Wait List
  for (thread = osRtxInfo.thread.wait_list;
       thread != NULL; thread = thread->delay_next) {
    count++;
  }

  EvrRtxThreadGetCount(count);

  return count;
}

/// Enumerate active threads.
/// \note API identical to osThreadEnumerate
static uint32_t svcRtxThreadEnumerate (osThreadId_t *thread_array, uint32_t array_items) {
  os_thread_t *thread;
  uint32_t     count;

  // Check parameters
  if ((thread_array == NULL) || (array_items == 0U)) {
    EvrRtxThreadEnumerate(thread_array, array_items, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  // Running Thread
  *thread_array = osRtxThreadGetRunning();
   thread_array++;
   count = 1U;

  // Ready List
  for (thread = osRtxInfo.thread.ready.thread_list;
       (thread != NULL) && (count < array_items); thread = thread->thread_next) {
    *thread_array = thread;
     thread_array++;
     count++;
  }

  // Delay List
  for (thread = osRtxInfo.thread.delay_list;
       (thread != NULL) && (count < array_items); thread = thread->delay_next) {
    *thread_array = thread;
     thread_array++;
     count++;
  }

  // Wait List
  for (thread = osRtxInfo.thread.wait_list;
       (thread != NULL) && (count < array_items); thread = thread->delay_next) {
    *thread_array = thread;
     thread_array++;
     count++;
  }

  EvrRtxThreadEnumerate(thread_array - count, array_items, count);

  return count;
}

/// Set the specified Thread Flags of a thread.
/// \note API identical to osThreadFlagsSet
static uint32_t svcRtxThreadFlagsSet (osThreadId_t thread_id, uint32_t flags) {
  os_thread_t *thread = osRtxThreadId(thread_id);
  uint32_t     thread_flags;
  uint32_t     thread_flags0;

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread) ||
      ((flags & ~(((uint32_t)1U << osRtxThreadFlagsLimit) - 1U)) != 0U)) {
    EvrRtxThreadFlagsError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Check object state
  if (thread->state == osRtxThreadTerminated) {
    EvrRtxThreadFlagsError(thread, (int32_t)osErrorResource);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorResource);
  }

  // Set Thread Flags
  thread_flags = ThreadFlagsSet(thread, flags);

  // Check if Thread is waiting for Thread Flags
  if (thread->state == osRtxThreadWaitingThreadFlags) {
    thread_flags0 = ThreadFlagsCheck(thread, thread->wait_flags, thread->flags_options);
    if (thread_flags0 != 0U) {
      if ((thread->flags_options & osFlagsNoClear) == 0U) {
        thread_flags = thread_flags0 & ~thread->wait_flags;
      } else {
        thread_flags = thread_flags0;
      }
      osRtxThreadWaitExit(thread, thread_flags0, TRUE);
      EvrRtxThreadFlagsWaitCompleted(thread->wait_flags, thread->flags_options, thread_flags0, thread);
    }
  }

  EvrRtxThreadFlagsSetDone(thread, thread_flags);

  return thread_flags;
}

/// Clear the specified Thread Flags of current running thread.
/// \note API identical to osThreadFlagsClear
static uint32_t svcRtxThreadFlagsClear (uint32_t flags) {
  os_thread_t *thread;
  uint32_t     thread_flags;

  // Check running thread
  thread = osRtxThreadGetRunning();
  if (thread == NULL) {
    EvrRtxThreadFlagsError(NULL, osRtxErrorKernelNotRunning);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osError);
  }

  // Check parameters
  if ((flags & ~(((uint32_t)1U << osRtxThreadFlagsLimit) - 1U)) != 0U) {
    EvrRtxThreadFlagsError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Clear Thread Flags
  thread_flags = ThreadFlagsClear(thread, flags);

  EvrRtxThreadFlagsClearDone(thread_flags);

  return thread_flags;
}

/// Get the current Thread Flags of current running thread.
/// \note API identical to osThreadFlagsGet
static uint32_t svcRtxThreadFlagsGet (void) {
  const os_thread_t *thread;

  // Check running thread
  thread = osRtxThreadGetRunning();
  if (thread == NULL) {
    EvrRtxThreadFlagsGet(0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxThreadFlagsGet(thread->thread_flags);

  return thread->thread_flags;
}

/// Wait for one or more Thread Flags of the current running thread to become signaled.
/// \note API identical to osThreadFlagsWait
static uint32_t svcRtxThreadFlagsWait (uint32_t flags, uint32_t options, uint32_t timeout) {
  os_thread_t *thread;
  uint32_t     thread_flags;

  // Check running thread
  thread = osRtxThreadGetRunning();
  if (thread == NULL) {
    EvrRtxThreadFlagsError(NULL, osRtxErrorKernelNotRunning);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osError);
  }

  // Check parameters
  if ((flags & ~(((uint32_t)1U << osRtxThreadFlagsLimit) - 1U)) != 0U) {
    EvrRtxThreadFlagsError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Check Thread Flags
  thread_flags = ThreadFlagsCheck(thread, flags, options);
  if (thread_flags != 0U) {
    EvrRtxThreadFlagsWaitCompleted(flags, options, thread_flags, thread);
  } else {
    // Check if timeout is specified
    if (timeout != 0U) {
      // Store waiting flags and options
      EvrRtxThreadFlagsWaitPending(flags, options, timeout);
      thread->wait_flags = flags;
      thread->flags_options = (uint8_t)options;
      // Suspend current Thread
      if (!osRtxThreadWaitEnter(osRtxThreadWaitingThreadFlags, timeout)) {
        EvrRtxThreadFlagsWaitTimeout(thread);
      }
      thread_flags = (uint32_t)osErrorTimeout;
    } else {
      EvrRtxThreadFlagsWaitNotCompleted(flags, options);
      thread_flags = (uint32_t)osErrorResource;
    }
  }
  return thread_flags;
}

//  Service Calls definitions
//lint ++flb "Library Begin" [MISRA Note 11]
SVC0_3 (ThreadNew,           osThreadId_t,    osThreadFunc_t, void *, const osThreadAttr_t *)
SVC0_1 (ThreadGetName,       const char *,    osThreadId_t)
SVC0_0 (ThreadGetId,         osThreadId_t)
SVC0_1 (ThreadGetState,      osThreadState_t, osThreadId_t)
SVC0_1 (ThreadGetStackSize,  uint32_t, osThreadId_t)
SVC0_1 (ThreadGetStackSpace, uint32_t, osThreadId_t)
SVC0_2 (ThreadSetPriority,   osStatus_t,      osThreadId_t, osPriority_t)
SVC0_1 (ThreadGetPriority,   osPriority_t,    osThreadId_t)
SVC0_0 (ThreadYield,         osStatus_t)
SVC0_1 (ThreadSuspend,       osStatus_t,      osThreadId_t)
SVC0_1 (ThreadResume,        osStatus_t,      osThreadId_t)
SVC0_1 (ThreadDetach,        osStatus_t,      osThreadId_t)
SVC0_1 (ThreadJoin,          osStatus_t,      osThreadId_t)
SVC0_0N(ThreadExit,          void)
SVC0_1 (ThreadTerminate,     osStatus_t,      osThreadId_t)
SVC0_0 (ThreadGetCount,      uint32_t)
SVC0_2 (ThreadEnumerate,     uint32_t,        osThreadId_t *, uint32_t)
SVC0_2 (ThreadFlagsSet,      uint32_t,        osThreadId_t, uint32_t)
SVC0_1 (ThreadFlagsClear,    uint32_t,        uint32_t)
SVC0_0 (ThreadFlagsGet,      uint32_t)
SVC0_3 (ThreadFlagsWait,     uint32_t,        uint32_t, uint32_t, uint32_t)
//lint --flb "Library End"


//  ==== ISR Calls ====

/// Set the specified Thread Flags of a thread.
/// \note API identical to osThreadFlagsSet
__STATIC_INLINE
uint32_t isrRtxThreadFlagsSet (osThreadId_t thread_id, uint32_t flags) {
  os_thread_t *thread = osRtxThreadId(thread_id);
  uint32_t     thread_flags;

  // Check parameters
  if ((thread == NULL) || (thread->id != osRtxIdThread) ||
      ((flags & ~(((uint32_t)1U << osRtxThreadFlagsLimit) - 1U)) != 0U)) {
    EvrRtxThreadFlagsError(thread, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorParameter);
  }

  // Check object state
  if (thread->state == osRtxThreadTerminated) {
    EvrRtxThreadFlagsError(thread, (int32_t)osErrorResource);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return ((uint32_t)osErrorResource);
  }

  // Set Thread Flags
  thread_flags = ThreadFlagsSet(thread, flags);

  // Register post ISR processing
  osRtxPostProcess(osRtxObject(thread));

  EvrRtxThreadFlagsSetDone(thread, thread_flags);

  return thread_flags;
}


//  ==== Library functions ====

/// Thread startup (Idle and Timer Thread).
/// \return true - success, false - failure.
bool_t osRtxThreadStartup (void) {
  bool_t ret = TRUE;

  // Create Idle Thread
  osRtxInfo.thread.idle = osRtxThreadId(
    svcRtxThreadNew(osRtxIdleThread, NULL, osRtxConfig.idle_thread_attr)
  );

  // Create Timer Thread
  if (osRtxConfig.timer_mq_mcnt != 0U) {
    osRtxInfo.timer.thread = osRtxThreadId(
      svcRtxThreadNew(osRtxTimerThread, NULL, osRtxConfig.timer_thread_attr)
    );
    if (osRtxInfo.timer.thread == NULL) {
      ret = FALSE;
    }
  }

  return ret;
}


//  ==== Public API ====

/// Create a thread and add it to Active Threads.
osThreadId_t osThreadNew (osThreadFunc_t func, void *argument, const osThreadAttr_t *attr) {
  osThreadId_t thread_id;

  EvrRtxThreadNew(func, argument, attr);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadError(NULL, (int32_t)osErrorISR);
    thread_id = NULL;
  } else {
    thread_id = __svcThreadNew(func, argument, attr);
  }
  return thread_id;
}

/// Get name of a thread.
const char *osThreadGetName (osThreadId_t thread_id) {
  const char *name;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadGetName(thread_id, NULL);
    name = NULL;
  } else {
    name = __svcThreadGetName(thread_id);
  }
  return name;
}

/// Return the thread ID of the current running thread.
osThreadId_t osThreadGetId (void) {
  osThreadId_t thread_id;

  if (IsIrqMode() || IsIrqMasked()) {
    thread_id = svcRtxThreadGetId();
  } else {
    thread_id =  __svcThreadGetId();
  }
  return thread_id;
}

/// Get current thread state of a thread.
osThreadState_t osThreadGetState (osThreadId_t thread_id) {
  osThreadState_t state;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadGetState(thread_id, osThreadError);
    state = osThreadError;
  } else {
    state = __svcThreadGetState(thread_id);
  }
  return state;
}

/// Get stack size of a thread.
uint32_t osThreadGetStackSize (osThreadId_t thread_id) {
  uint32_t stack_size;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadGetStackSize(thread_id, 0U);
    stack_size = 0U;
  } else {
    stack_size = __svcThreadGetStackSize(thread_id);
  }
  return stack_size;
}

/// Get available stack space of a thread based on stack watermark recording during execution.
uint32_t osThreadGetStackSpace (osThreadId_t thread_id) {
  uint32_t stack_space;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadGetStackSpace(thread_id, 0U);
    stack_space = 0U;
  } else {
    stack_space = __svcThreadGetStackSpace(thread_id);
  }
  return stack_space;
}

/// Change priority of a thread.
osStatus_t osThreadSetPriority (osThreadId_t thread_id, osPriority_t priority) {
  osStatus_t status;

  EvrRtxThreadSetPriority(thread_id, priority);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadError(thread_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcThreadSetPriority(thread_id, priority);
  }
  return status;
}

/// Get current priority of a thread.
osPriority_t osThreadGetPriority (osThreadId_t thread_id) {
  osPriority_t priority;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadGetPriority(thread_id, osPriorityError);
    priority = osPriorityError;
  } else {
    priority = __svcThreadGetPriority(thread_id);
  }
  return priority;
}

/// Pass control to next thread that is in state READY.
osStatus_t osThreadYield (void) {
  osStatus_t status;

  EvrRtxThreadYield();
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadError(NULL, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcThreadYield();
  }
  return status;
}

/// Suspend execution of a thread.
osStatus_t osThreadSuspend (osThreadId_t thread_id) {
  osStatus_t status;

  EvrRtxThreadSuspend(thread_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadError(thread_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcThreadSuspend(thread_id);
  }
  return status;
}

/// Resume execution of a thread.
osStatus_t osThreadResume (osThreadId_t thread_id) {
  osStatus_t status;

  EvrRtxThreadResume(thread_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadError(thread_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcThreadResume(thread_id);
  }
  return status;
}

/// Detach a thread (thread storage can be reclaimed when thread terminates).
osStatus_t osThreadDetach (osThreadId_t thread_id) {
  osStatus_t status;

  EvrRtxThreadDetach(thread_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadError(thread_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcThreadDetach(thread_id);
  }
  return status;
}

/// Wait for specified thread to terminate.
osStatus_t osThreadJoin (osThreadId_t thread_id) {
  osStatus_t status;

  EvrRtxThreadJoin(thread_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadError(thread_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcThreadJoin(thread_id);
  }
  return status;
}

/// Terminate execution of current running thread.
__NO_RETURN void osThreadExit (void) {
  EvrRtxThreadExit();
  __svcThreadExit();
  EvrRtxThreadError(NULL, (int32_t)osError);
  for (;;) {}
}

/// Terminate execution of a thread.
osStatus_t osThreadTerminate (osThreadId_t thread_id) {
  osStatus_t status;

  EvrRtxThreadTerminate(thread_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadError(thread_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcThreadTerminate(thread_id);
  }
  return status;
}

/// Get number of active threads.
uint32_t osThreadGetCount (void) {
  uint32_t count;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadGetCount(0U);
    count = 0U;
  } else {
    count = __svcThreadGetCount();
  }
  return count;
}

/// Enumerate active threads.
uint32_t osThreadEnumerate (osThreadId_t *thread_array, uint32_t array_items) {
  uint32_t count;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadEnumerate(thread_array, array_items, 0U);
    count = 0U;
  } else {
    count = __svcThreadEnumerate(thread_array, array_items);
  }
  return count;
}

/// Set the specified Thread Flags of a thread.
uint32_t osThreadFlagsSet (osThreadId_t thread_id, uint32_t flags) {
  uint32_t thread_flags;

  EvrRtxThreadFlagsSet(thread_id, flags);
  if (IsIrqMode() || IsIrqMasked()) {
    thread_flags = isrRtxThreadFlagsSet(thread_id, flags);
  } else {
    thread_flags =  __svcThreadFlagsSet(thread_id, flags);
  }
  return thread_flags;
}

/// Clear the specified Thread Flags of current running thread.
uint32_t osThreadFlagsClear (uint32_t flags) {
  uint32_t thread_flags;

  EvrRtxThreadFlagsClear(flags);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadFlagsError(NULL, (int32_t)osErrorISR);
    thread_flags = (uint32_t)osErrorISR;
  } else {
    thread_flags = __svcThreadFlagsClear(flags);
  }
  return thread_flags;
}

/// Get the current Thread Flags of current running thread.
uint32_t osThreadFlagsGet (void) {
  uint32_t thread_flags;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadFlagsGet(0U);
    thread_flags = 0U;
  } else {
    thread_flags = __svcThreadFlagsGet();
  }
  return thread_flags;
}

/// Wait for one or more Thread Flags of the current running thread to become signaled.
uint32_t osThreadFlagsWait (uint32_t flags, uint32_t options, uint32_t timeout) {
  uint32_t thread_flags;

  EvrRtxThreadFlagsWait(flags, options, timeout);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxThreadFlagsError(NULL, (int32_t)osErrorISR);
    thread_flags = (uint32_t)osErrorISR;
  } else {
    thread_flags = __svcThreadFlagsWait(flags, options, timeout);
  }
  return thread_flags;
}
