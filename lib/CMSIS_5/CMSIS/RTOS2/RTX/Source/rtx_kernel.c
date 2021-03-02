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
 * Title:       Kernel functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  OS Runtime Information
osRtxInfo_t osRtxInfo __attribute__((section(".data.os"))) =
//lint -e{785} "Initialize only OS ID, OS Version and Kernel State"
{ .os_id = osRtxKernelId, .version = osRtxVersionKernel, .kernel.state = osRtxKernelInactive };


//  ==== Helper functions ====

/// Block Kernel (disable: thread switching, time tick, post ISR processing).
static void KernelBlock (void) {

  OS_Tick_Disable();

  osRtxInfo.kernel.blocked = 1U;
  __DSB();

  if (GetPendSV() != 0U) {
    ClrPendSV();
    osRtxInfo.kernel.pendSV = 1U;
  }
}

/// Unblock Kernel
static void KernelUnblock (void) {

  osRtxInfo.kernel.blocked = 0U;
  __DSB();

  if (osRtxInfo.kernel.pendSV != 0U) {
    osRtxInfo.kernel.pendSV = 0U;
    SetPendSV();
  }

  OS_Tick_Enable();
}


//  ==== Service Calls ====

/// Initialize the RTOS Kernel.
/// \note API identical to osKernelInitialize
static osStatus_t svcRtxKernelInitialize (void) {

  if (osRtxInfo.kernel.state == osRtxKernelReady) {
    EvrRtxKernelInitialized();
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osOK;
  }
  if (osRtxInfo.kernel.state != osRtxKernelInactive) {
    EvrRtxKernelError((int32_t)osError);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osError;
  }

#if (DOMAIN_NS == 1)
  // Initialize Secure Process Stack
  if (TZ_InitContextSystem_S() == 0U) {
    EvrRtxKernelError(osRtxErrorTZ_InitContext_S);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osError;
  }
#endif

  // Initialize osRtxInfo
  memset(&osRtxInfo.kernel, 0, sizeof(osRtxInfo) - offsetof(osRtxInfo_t, kernel));

  osRtxInfo.isr_queue.data = osRtxConfig.isr_queue.data;
  osRtxInfo.isr_queue.max  = osRtxConfig.isr_queue.max;

  osRtxInfo.thread.robin.timeout = osRtxConfig.robin_timeout;

  // Initialize Memory Pools (Variable Block Size)
  if (osRtxMemoryInit(osRtxConfig.mem.common_addr, osRtxConfig.mem.common_size) != 0U) {
    osRtxInfo.mem.common = osRtxConfig.mem.common_addr;
  }
  if (osRtxMemoryInit(osRtxConfig.mem.stack_addr, osRtxConfig.mem.stack_size) != 0U) {
    osRtxInfo.mem.stack = osRtxConfig.mem.stack_addr;
  } else {
    osRtxInfo.mem.stack = osRtxInfo.mem.common;
  }
  if (osRtxMemoryInit(osRtxConfig.mem.mp_data_addr, osRtxConfig.mem.mp_data_size) != 0U) {
    osRtxInfo.mem.mp_data = osRtxConfig.mem.mp_data_addr;
  } else {
    osRtxInfo.mem.mp_data = osRtxInfo.mem.common;
  }
  if (osRtxMemoryInit(osRtxConfig.mem.mq_data_addr, osRtxConfig.mem.mq_data_size) != 0U) {
    osRtxInfo.mem.mq_data = osRtxConfig.mem.mq_data_addr;
  } else {
    osRtxInfo.mem.mq_data = osRtxInfo.mem.common;
  }

  // Initialize Memory Pools (Fixed Block Size)
  if (osRtxConfig.mpi.stack != NULL) {
    (void)osRtxMemoryPoolInit(osRtxConfig.mpi.stack,
                              osRtxConfig.mpi.stack->max_blocks,
                              osRtxConfig.mpi.stack->block_size,
                              osRtxConfig.mpi.stack->block_base);
    osRtxInfo.mpi.stack = osRtxConfig.mpi.stack;
  }
  if (osRtxConfig.mpi.thread != NULL) {
    (void)osRtxMemoryPoolInit(osRtxConfig.mpi.thread,
                              osRtxConfig.mpi.thread->max_blocks,
                              osRtxConfig.mpi.thread->block_size,
                              osRtxConfig.mpi.thread->block_base);
    osRtxInfo.mpi.thread = osRtxConfig.mpi.thread;
  }
  if (osRtxConfig.mpi.timer != NULL) {
    (void)osRtxMemoryPoolInit(osRtxConfig.mpi.timer,
                              osRtxConfig.mpi.timer->max_blocks,
                              osRtxConfig.mpi.timer->block_size,
                              osRtxConfig.mpi.timer->block_base);
    osRtxInfo.mpi.timer = osRtxConfig.mpi.timer;
  }
  if (osRtxConfig.mpi.event_flags != NULL) {
    (void)osRtxMemoryPoolInit(osRtxConfig.mpi.event_flags,
                              osRtxConfig.mpi.event_flags->max_blocks,
                              osRtxConfig.mpi.event_flags->block_size,
                              osRtxConfig.mpi.event_flags->block_base);
    osRtxInfo.mpi.event_flags = osRtxConfig.mpi.event_flags;
  }
  if (osRtxConfig.mpi.mutex != NULL) {
    (void)osRtxMemoryPoolInit(osRtxConfig.mpi.mutex,
                              osRtxConfig.mpi.mutex->max_blocks,
                              osRtxConfig.mpi.mutex->block_size,
                              osRtxConfig.mpi.mutex->block_base);
    osRtxInfo.mpi.mutex = osRtxConfig.mpi.mutex;
  }
  if (osRtxConfig.mpi.semaphore != NULL) {
    (void)osRtxMemoryPoolInit(osRtxConfig.mpi.semaphore,
                              osRtxConfig.mpi.semaphore->max_blocks,
                              osRtxConfig.mpi.semaphore->block_size,
                              osRtxConfig.mpi.semaphore->block_base);
    osRtxInfo.mpi.semaphore = osRtxConfig.mpi.semaphore;
  }
  if (osRtxConfig.mpi.memory_pool != NULL) {
    (void)osRtxMemoryPoolInit(osRtxConfig.mpi.memory_pool,
                              osRtxConfig.mpi.memory_pool->max_blocks,
                              osRtxConfig.mpi.memory_pool->block_size,
                              osRtxConfig.mpi.memory_pool->block_base);
    osRtxInfo.mpi.memory_pool = osRtxConfig.mpi.memory_pool;
  }
  if (osRtxConfig.mpi.message_queue != NULL) {
    (void)osRtxMemoryPoolInit(osRtxConfig.mpi.message_queue,
                              osRtxConfig.mpi.message_queue->max_blocks,
                              osRtxConfig.mpi.message_queue->block_size,
                              osRtxConfig.mpi.message_queue->block_base);
    osRtxInfo.mpi.message_queue = osRtxConfig.mpi.message_queue;
  }

  osRtxInfo.kernel.state = osRtxKernelReady;

  EvrRtxKernelInitialized();

  return osOK;
}

///  Get RTOS Kernel Information.
/// \note API identical to osKernelGetInfo
static osStatus_t svcRtxKernelGetInfo (osVersion_t *version, char *id_buf, uint32_t id_size) {
  uint32_t size;

  if (version != NULL) {
    version->api    = osRtxVersionAPI;
    version->kernel = osRtxVersionKernel;
  }

  if ((id_buf != NULL) && (id_size != 0U)) {
    if (id_size > sizeof(osRtxKernelId)) {
      size = sizeof(osRtxKernelId);
    } else {
      size = id_size;
    }
    memcpy(id_buf, osRtxKernelId, size);
  }

  EvrRtxKernelInfoRetrieved(version, id_buf, id_size);

  return osOK;
}

/// Get the current RTOS Kernel state.
/// \note API identical to osKernelGetState
static osKernelState_t svcRtxKernelGetState (void) {
  osKernelState_t state = osRtxKernelState();
  EvrRtxKernelGetState(state);
  return state;
}

/// Start the RTOS Kernel scheduler.
/// \note API identical to osKernelStart
static osStatus_t svcRtxKernelStart (void) {
  os_thread_t *thread;

  if (osRtxInfo.kernel.state != osRtxKernelReady) {
    EvrRtxKernelError(osRtxErrorKernelNotReady);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osError;
  }

  // Thread startup (Idle and Timer Thread)
  if (!osRtxThreadStartup()) {
    EvrRtxKernelError((int32_t)osError);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osError;
  }

  // Setup SVC and PendSV System Service Calls
  SVC_Setup();

  // Setup RTOS Tick
  if (OS_Tick_Setup(osRtxConfig.tick_freq, OS_TICK_HANDLER) != 0) {
    EvrRtxKernelError((int32_t)osError);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osError;
  }
  osRtxInfo.tick_irqn = OS_Tick_GetIRQn();

  // Enable RTOS Tick
  OS_Tick_Enable();

  // Switch to Ready Thread with highest Priority
  thread = osRtxThreadListGet(&osRtxInfo.thread.ready);
  osRtxThreadSwitch(thread);

  if ((osRtxConfig.flags & osRtxConfigPrivilegedMode) != 0U) {
    // Privileged Thread mode & PSP
    __set_CONTROL(0x02U);
  } else {
    // Unprivileged Thread mode & PSP
    __set_CONTROL(0x03U);
  }

  osRtxInfo.kernel.state = osRtxKernelRunning;

  EvrRtxKernelStarted();

  return osOK;
}

/// Lock the RTOS Kernel scheduler.
/// \note API identical to osKernelLock
static int32_t svcRtxKernelLock (void) {
  int32_t lock;

  switch (osRtxInfo.kernel.state) {
    case osRtxKernelRunning:
      osRtxInfo.kernel.state = osRtxKernelLocked;
      EvrRtxKernelLocked(0);
      lock = 0;
      break;
    case osRtxKernelLocked:
      EvrRtxKernelLocked(1);
      lock = 1;
      break;
    default:
      EvrRtxKernelError((int32_t)osError);
      lock = (int32_t)osError;
      break;
  }
  return lock;
}
 
/// Unlock the RTOS Kernel scheduler.
/// \note API identical to osKernelUnlock
static int32_t svcRtxKernelUnlock (void) {
  int32_t lock;

  switch (osRtxInfo.kernel.state) {
    case osRtxKernelRunning:
      EvrRtxKernelUnlocked(0);
      lock = 0;
      break;
    case osRtxKernelLocked:
      osRtxInfo.kernel.state = osRtxKernelRunning;
      EvrRtxKernelUnlocked(1);
      lock = 1;
      break;
    default:
      EvrRtxKernelError((int32_t)osError);
      lock = (int32_t)osError;
      break;
  }
  return lock;
}

/// Restore the RTOS Kernel scheduler lock state.
/// \note API identical to osKernelRestoreLock
static int32_t svcRtxKernelRestoreLock (int32_t lock) {
  int32_t lock_new;

  switch (osRtxInfo.kernel.state) {
    case osRtxKernelRunning:
    case osRtxKernelLocked:
      switch (lock) {
        case 0:
          osRtxInfo.kernel.state = osRtxKernelRunning;
          EvrRtxKernelLockRestored(0);
          lock_new = 0;
          break;
        case 1:
          osRtxInfo.kernel.state = osRtxKernelLocked;
          EvrRtxKernelLockRestored(1);
          lock_new = 1;
          break;
        default:
          EvrRtxKernelError((int32_t)osError);
          lock_new = (int32_t)osError;
          break;
      }
      break;
    default:
      EvrRtxKernelError((int32_t)osError);
      lock_new = (int32_t)osError;
      break;
  }
  return lock_new;
}

/// Suspend the RTOS Kernel scheduler.
/// \note API identical to osKernelSuspend
static uint32_t svcRtxKernelSuspend (void) {
  const os_thread_t *thread;
  const os_timer_t  *timer;
  uint32_t           delay;

  if (osRtxInfo.kernel.state != osRtxKernelRunning) {
    EvrRtxKernelError(osRtxErrorKernelNotRunning);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  KernelBlock();

  delay = osWaitForever;

  // Check Thread Delay list
  thread = osRtxInfo.thread.delay_list;
  if (thread != NULL) {
    delay = thread->delay;
  }

  // Check Active Timer list
  timer = osRtxInfo.timer.list;
  if (timer != NULL) {
    if (timer->tick < delay) {
      delay = timer->tick;
    }
  }

  osRtxInfo.kernel.state = osRtxKernelSuspended;

  EvrRtxKernelSuspended(delay);

  return delay;
}

/// Resume the RTOS Kernel scheduler.
/// \note API identical to osKernelResume
static void svcRtxKernelResume (uint32_t sleep_ticks) {
  os_thread_t *thread;
  os_timer_t  *timer;
  uint32_t     delay;
  uint32_t     ticks;

  if (osRtxInfo.kernel.state != osRtxKernelSuspended) {
    EvrRtxKernelResumed();
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return;
  }

  osRtxInfo.kernel.tick += sleep_ticks;

  // Process Thread Delay list
  thread = osRtxInfo.thread.delay_list;
  if (thread != NULL) {
    delay = sleep_ticks;
    do {
      if (delay >= thread->delay) {
        delay -= thread->delay;
        thread->delay = 1U;
        osRtxThreadDelayTick();
        thread = osRtxInfo.thread.delay_list;
      } else {
        thread->delay -= delay;
        delay = 0U;
      }
    } while ((thread != NULL) && (delay != 0U));
  }

  // Process Active Timer list
  timer = osRtxInfo.timer.list;
  if (timer != NULL) {
    ticks = sleep_ticks;
    do {
      if (ticks >= timer->tick) {
        ticks -= timer->tick;
        timer->tick = 1U;
        osRtxInfo.timer.tick();
        timer = osRtxInfo.timer.list;
      } else {
        timer->tick -= ticks;
        ticks = 0U;
      }
    } while ((timer != NULL) && (ticks != 0U));
  }

  osRtxInfo.kernel.state = osRtxKernelRunning;

  osRtxThreadDispatch(NULL);

  KernelUnblock();

  EvrRtxKernelResumed();
}

/// Get the RTOS kernel tick count.
/// \note API identical to osKernelGetTickCount
static uint32_t svcRtxKernelGetTickCount (void) {
  EvrRtxKernelGetTickCount(osRtxInfo.kernel.tick);
  return osRtxInfo.kernel.tick;
}

/// Get the RTOS kernel tick frequency.
/// \note API identical to osKernelGetTickFreq
static uint32_t svcRtxKernelGetTickFreq (void) {
  EvrRtxKernelGetTickFreq(osRtxConfig.tick_freq);
  return osRtxConfig.tick_freq;
}

/// Get the RTOS kernel system timer count.
/// \note API identical to osKernelGetSysTimerCount
static uint32_t svcRtxKernelGetSysTimerCount (void) {
  uint32_t tick;
  uint32_t count;

  tick  = (uint32_t)osRtxInfo.kernel.tick;
  count = OS_Tick_GetCount();
  if (OS_Tick_GetOverflow() != 0U) {
    count = OS_Tick_GetCount();
    tick++;
  }
  count += tick * OS_Tick_GetInterval();
  EvrRtxKernelGetSysTimerCount(count);
  return count;
}

/// Get the RTOS kernel system timer frequency.
/// \note API identical to osKernelGetSysTimerFreq
static uint32_t svcRtxKernelGetSysTimerFreq (void) {
  uint32_t freq = OS_Tick_GetClock();
  EvrRtxKernelGetSysTimerFreq(freq);
  return freq;
}

//  Service Calls definitions
//lint ++flb "Library Begin" [MISRA Note 11]
SVC0_0 (KernelInitialize,       osStatus_t)
SVC0_3 (KernelGetInfo,          osStatus_t, osVersion_t *, char *, uint32_t)
SVC0_0 (KernelStart,            osStatus_t)
SVC0_0 (KernelLock,             int32_t)
SVC0_0 (KernelUnlock,           int32_t)
SVC0_1 (KernelRestoreLock,      int32_t, int32_t)
SVC0_0 (KernelSuspend,          uint32_t)
SVC0_1N(KernelResume,           void, uint32_t)
SVC0_0 (KernelGetState,         osKernelState_t)
SVC0_0 (KernelGetTickCount,     uint32_t)
SVC0_0 (KernelGetTickFreq,      uint32_t)
SVC0_0 (KernelGetSysTimerCount, uint32_t)
SVC0_0 (KernelGetSysTimerFreq,  uint32_t)
//lint --flb "Library End"


//  ==== Library functions ====

/// RTOS Kernel Pre-Initialization Hook
//lint -esym(759,osRtxKernelPreInit) "Prototype in header"
//lint -esym(765,osRtxKernelPreInit) "Global scope (can be overridden)"
//lint -esym(522,osRtxKernelPreInit) "Can be overridden (do not lack side-effects)"
__WEAK void osRtxKernelPreInit (void) {
}


//  ==== Public API ====

/// Initialize the RTOS Kernel.
osStatus_t osKernelInitialize (void) {
  osStatus_t status;

  osRtxKernelPreInit();
  EvrRtxKernelInitialize();
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxKernelError((int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcKernelInitialize();
  }
  return status;
}

///  Get RTOS Kernel Information.
osStatus_t osKernelGetInfo (osVersion_t *version, char *id_buf, uint32_t id_size) {
  osStatus_t status;

  EvrRtxKernelGetInfo(version, id_buf, id_size);
  if (IsIrqMode() || IsIrqMasked() || IsPrivileged()) {
    status = svcRtxKernelGetInfo(version, id_buf, id_size);
  } else {
    status =  __svcKernelGetInfo(version, id_buf, id_size);
  }
  return status;
}

/// Get the current RTOS Kernel state.
osKernelState_t osKernelGetState (void) {
  osKernelState_t state;

  if (IsIrqMode() || IsIrqMasked() || IsPrivileged()) {
    state = svcRtxKernelGetState();
  } else {
    state =  __svcKernelGetState();
  }
  return state;
}

/// Start the RTOS Kernel scheduler.
osStatus_t osKernelStart (void) {
  osStatus_t status;

  EvrRtxKernelStart();
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxKernelError((int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcKernelStart();
  }
  return status;
}

/// Lock the RTOS Kernel scheduler.
int32_t osKernelLock (void) {
  int32_t lock;

  EvrRtxKernelLock();
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxKernelError((int32_t)osErrorISR);
    lock = (int32_t)osErrorISR;
  } else {
    lock = __svcKernelLock();
  }
  return lock;
}
 
/// Unlock the RTOS Kernel scheduler.
int32_t osKernelUnlock (void) {
  int32_t lock;

  EvrRtxKernelUnlock();
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxKernelError((int32_t)osErrorISR);
    lock = (int32_t)osErrorISR;
  } else {
    lock = __svcKernelUnlock();
  }
  return lock;
}

/// Restore the RTOS Kernel scheduler lock state.
int32_t osKernelRestoreLock (int32_t lock) {
  int32_t lock_new;

  EvrRtxKernelRestoreLock(lock);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxKernelError((int32_t)osErrorISR);
    lock_new = (int32_t)osErrorISR;
  } else {
    lock_new = __svcKernelRestoreLock(lock);
  }
  return lock_new;
}

/// Suspend the RTOS Kernel scheduler.
uint32_t osKernelSuspend (void) {
  uint32_t ticks;

  EvrRtxKernelSuspend();
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxKernelError((int32_t)osErrorISR);
    ticks = 0U;
  } else {
    ticks = __svcKernelSuspend();
  }
  return ticks;
}

/// Resume the RTOS Kernel scheduler.
void osKernelResume (uint32_t sleep_ticks) {

  EvrRtxKernelResume(sleep_ticks);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxKernelError((int32_t)osErrorISR);
  } else {
    __svcKernelResume(sleep_ticks);
  }
}

/// Get the RTOS kernel tick count.
uint32_t osKernelGetTickCount (void) {
  uint32_t count;

  if (IsIrqMode() || IsIrqMasked()) {
    count = svcRtxKernelGetTickCount();
  } else {
    count =  __svcKernelGetTickCount();
  }
  return count;
}

/// Get the RTOS kernel tick frequency.
uint32_t osKernelGetTickFreq (void) {
  uint32_t freq;

  if (IsIrqMode() || IsIrqMasked()) {
    freq = svcRtxKernelGetTickFreq();
  } else {
    freq =  __svcKernelGetTickFreq();
  }
  return freq;
}

/// Get the RTOS kernel system timer count.
uint32_t osKernelGetSysTimerCount (void) {
  uint32_t count;

  if (IsIrqMode() || IsIrqMasked()) {
    count = svcRtxKernelGetSysTimerCount();
  } else {
    count =  __svcKernelGetSysTimerCount();
  }
  return count;
}

/// Get the RTOS kernel system timer frequency.
uint32_t osKernelGetSysTimerFreq (void) {
  uint32_t freq;

  if (IsIrqMode() || IsIrqMasked()) {
    freq = svcRtxKernelGetSysTimerFreq();
  } else {
    freq =  __svcKernelGetSysTimerFreq();
  }
  return freq;
}
