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
 * Title:       RTX Event Recorder definitions
 *
 * -----------------------------------------------------------------------------
 */

#ifndef RTX_EVR_H_
#define RTX_EVR_H_

#include "cmsis_os2.h"                  // CMSIS RTOS API
#include "RTX_Config.h"                 // RTX Configuration
#include "rtx_os.h"                     // RTX OS definitions

// Initial Thread configuration covered also Thread Flags and Generic Wait
#ifndef   OS_EVR_THFLAGS
#define   OS_EVR_THFLAGS        OS_EVR_THREAD
#endif
#ifndef   OS_EVR_WAIT
#define   OS_EVR_WAIT           OS_EVR_THREAD
#endif

#ifdef   _RTE_
#include "RTE_Components.h"
#endif

#ifdef    RTE_Compiler_EventRecorder

//lint -emacro((835,845),EventID) [MISRA Note 13]

#include "EventRecorder.h"
#include "EventRecorderConf.h"

#if ((defined(OS_EVR_INIT) && (OS_EVR_INIT != 0)) || (EVENT_TIMESTAMP_SOURCE == 2))
#ifndef EVR_RTX_KERNEL_GET_STATE_DISABLE
#define EVR_RTX_KERNEL_GET_STATE_DISABLE
#endif
#endif

#if (EVENT_TIMESTAMP_SOURCE == 2)
#ifndef EVR_RTX_KERNEL_GET_SYS_TIMER_COUNT_DISABLE
#define EVR_RTX_KERNEL_GET_SYS_TIMER_COUNT_DISABLE
#endif
#ifndef EVR_RTX_KERNEL_GET_SYS_TIMER_FREQ_DISABLE
#define EVR_RTX_KERNEL_GET_SYS_TIMER_FREQ_DISABLE
#endif
#endif

/// RTOS component number
#define EvtRtxMemoryNo                  (0xF0U)
#define EvtRtxKernelNo                  (0xF1U)
#define EvtRtxThreadNo                  (0xF2U)
#define EvtRtxThreadFlagsNo             (0xF4U)
#define EvtRtxWaitNo                    (0xF3U)
#define EvtRtxTimerNo                   (0xF6U)
#define EvtRtxEventFlagsNo              (0xF5U)
#define EvtRtxMutexNo                   (0xF7U)
#define EvtRtxSemaphoreNo               (0xF8U)
#define EvtRtxMemoryPoolNo              (0xF9U)
#define EvtRtxMessageQueueNo            (0xFAU)

#endif  // RTE_Compiler_EventRecorder


/// Extended Status codes
#define osRtxErrorKernelNotReady        (-7)
#define osRtxErrorKernelNotRunning      (-8)
#define osRtxErrorInvalidControlBlock   (-9)
#define osRtxErrorInvalidDataMemory     (-10)
#define osRtxErrorInvalidThreadStack    (-11)
#define osRtxErrorInvalidPriority       (-12)
#define osRtxErrorThreadNotJoinable     (-13)
#define osRtxErrorMutexNotOwned         (-14)
#define osRtxErrorMutexNotLocked        (-15)
#define osRtxErrorMutexLockLimit        (-16)
#define osRtxErrorSemaphoreCountLimit   (-17)
#define osRtxErrorTZ_InitContext_S      (-18)
#define osRtxErrorTZ_AllocContext_S     (-19)
#define osRtxErrorTZ_FreeContext_S      (-20)
#define osRtxErrorTZ_LoadContext_S      (-21)
#define osRtxErrorTZ_SaveContext_S      (-22)


//  ==== Memory Events ====

/**
  \brief  Event on memory initialization (Op)
  \param[in]  mem           pointer to memory pool.
  \param[in]  size          size of a memory pool in bytes.
  \param[in]  result        execution status: 1 - success, 0 - failure.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_INIT_DISABLE))
extern void EvrRtxMemoryInit (void *mem, uint32_t size, uint32_t result);
#else
#define EvrRtxMemoryInit(mem, size, result)
#endif

/**
  \brief  Event on memory allocate (Op)
  \param[in]  mem           pointer to memory pool.
  \param[in]  size          size of a memory block in bytes.
  \param[in]  type          memory block type: 0 - generic, 1 - control block.
  \param[in]  block         pointer to allocated memory block or NULL in case of no memory is available.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_ALLOC_DISABLE))
extern void EvrRtxMemoryAlloc (void *mem, uint32_t size, uint32_t type, void *block);
#else
#define EvrRtxMemoryAlloc(mem, size, type, block)
#endif

/**
  \brief  Event on memory free (Op)
  \param[in]  mem           pointer to memory pool.
  \param[in]  block         memory block to be returned to the memory pool.
  \param[in]  result        execution status: 1 - success, 0 - failure.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_FREE_DISABLE))
extern void EvrRtxMemoryFree (void *mem, void *block, uint32_t result);
#else
#define EvrRtxMemoryFree(mem, block, result)
#endif

/**
  \brief  Event on memory block initialization (Op)
  \param[in]  mp_info       memory pool info.
  \param[in]  block_count   maximum number of memory blocks in memory pool.
  \param[in]  block_size    size of a memory block in bytes.
  \param[in]  block_mem     pointer to memory for block storage.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_BLOCK_INIT_DISABLE))
extern void EvrRtxMemoryBlockInit (osRtxMpInfo_t *mp_info, uint32_t block_count, uint32_t block_size, void *block_mem);
#else
#define EvrRtxMemoryBlockInit(mp_info, block_count, block_size, block_mem)
#endif

/**
  \brief  Event on memory block alloc (Op)
  \param[in]  mp_info       memory pool info.
  \param[in]  block         address of the allocated memory block or NULL in case of no memory is available.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_BLOCK_ALLOC_DISABLE))
extern void EvrRtxMemoryBlockAlloc (osRtxMpInfo_t *mp_info, void *block);
#else
#define EvrRtxMemoryBlockAlloc(mp_info, block)
#endif

/**
  \brief  Event on memory block free (Op)
  \param[in]  mp_info       memory pool info.
  \param[in]  block         address of the allocated memory block to be returned to the memory pool.
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_BLOCK_FREE_DISABLE))
extern void EvrRtxMemoryBlockFree (osRtxMpInfo_t *mp_info, void *block, int32_t status);
#else
#define EvrRtxMemoryBlockFree(mp_info, block, status)
#endif


//  ==== Kernel Events ====

/**
  \brief  Event on RTOS kernel error (Error)
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_ERROR_DISABLE))
extern void EvrRtxKernelError (int32_t status);
#else
#define EvrRtxKernelError(status)
#endif

/**
  \brief  Event on RTOS kernel initialize (API)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_INITIALIZE_DISABLE))
extern void EvrRtxKernelInitialize (void);
#else
#define EvrRtxKernelInitialize()
#endif

/**
  \brief  Event on successful RTOS kernel initialize (Op)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_INITIALIZED_DISABLE))
extern void EvrRtxKernelInitialized (void);
#else
#define EvrRtxKernelInitialized()
#endif

/**
  \brief  Event on RTOS kernel information retrieve (API)
  \param[in]  version       pointer to buffer for retrieving version information.
  \param[in]  id_buf        pointer to buffer for retrieving kernel identification string.
  \param[in]  id_size       size of buffer for kernel identification string.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_INFO_DISABLE))
extern void EvrRtxKernelGetInfo (osVersion_t *version, char *id_buf, uint32_t id_size);
#else
#define EvrRtxKernelGetInfo(version, id_buf, id_size)
#endif

/**
  \brief  Event on successful RTOS kernel information retrieve (Op)
  \param[in]  version       pointer to buffer for retrieving version information.
  \param[in]  id_buf        pointer to buffer for retrieving kernel identification string.
  \param[in]  id_size       size of buffer for kernel identification string.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_INFO_RETRIEVED_DISABLE))
extern void EvrRtxKernelInfoRetrieved (const osVersion_t *version, const char *id_buf, uint32_t id_size);
#else
#define EvrRtxKernelInfoRetrieved(version, id_buf, id_size)
#endif

/**
  \brief  Event on current RTOS Kernel state retrieve (API)
  \param[in]  state         current RTOS Kernel state.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_STATE_DISABLE))
extern void EvrRtxKernelGetState (osKernelState_t state);
#else
#define EvrRtxKernelGetState(state)
#endif

/**
  \brief  Event on RTOS Kernel scheduler start (API)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_START_DISABLE))
extern void EvrRtxKernelStart (void);
#else
#define EvrRtxKernelStart()
#endif

/**
  \brief  Event on successful RTOS Kernel scheduler start (Op)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_STARTED_DISABLE))
extern void EvrRtxKernelStarted (void);
#else
#define EvrRtxKernelStarted()
#endif

/**
  \brief  Event on RTOS Kernel scheduler lock (API)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_LOCK_DISABLE))
extern void EvrRtxKernelLock (void);
#else
#define EvrRtxKernelLock()
#endif

/**
  \brief  Event on successful RTOS Kernel scheduler lock (Op)
  \param[in]  lock          previous lock state (1 - locked, 0 - not locked).
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_LOCKED_DISABLE))
extern void EvrRtxKernelLocked (int32_t lock);
#else
#define EvrRtxKernelLocked(lock)
#endif

/**
  \brief  Event on RTOS Kernel scheduler unlock (API)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_UNLOCK_DISABLE))
extern void EvrRtxKernelUnlock (void);
#else
#define EvrRtxKernelUnlock()
#endif

/**
  \brief  Event on successful RTOS Kernel scheduler unlock (Op)
  \param[in]  lock          previous lock state (1 - locked, 0 - not locked).
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_UNLOCKED_DISABLE))
extern void EvrRtxKernelUnlocked (int32_t lock);
#else
#define EvrRtxKernelUnlocked(lock)
#endif

/**
  \brief  Event on RTOS Kernel scheduler lock state restore (API)
  \param[in]  lock          lock state obtained by \ref osKernelLock or \ref osKernelUnlock.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_RESTORE_LOCK_DISABLE))
extern void EvrRtxKernelRestoreLock (int32_t lock);
#else
#define EvrRtxKernelRestoreLock(lock)
#endif

/**
  \brief  Event on successful RTOS Kernel scheduler lock state restore (Op)
  \param[in]  lock          new lock state (1 - locked, 0 - not locked).
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_LOCK_RESTORED_DISABLE))
extern void EvrRtxKernelLockRestored (int32_t lock);
#else
#define EvrRtxKernelLockRestored(lock)
#endif

/**
  \brief  Event on RTOS Kernel scheduler suspend (API)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_SUSPEND_DISABLE))
extern void EvrRtxKernelSuspend (void);
#else
#define EvrRtxKernelSuspend()
#endif

/**
  \brief  Event on successful RTOS Kernel scheduler suspend (Op)
  \param[in]  sleep_ticks   time in ticks, for how long the system can sleep or power-down.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_SUSPENDED_DISABLE))
extern void EvrRtxKernelSuspended (uint32_t sleep_ticks);
#else
#define EvrRtxKernelSuspended(sleep_ticks)
#endif

/**
  \brief  Event on RTOS Kernel scheduler resume (API)
  \param[in]  sleep_ticks   time in ticks, for how long the system was in sleep or power-down mode.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_RESUME_DISABLE))
extern void EvrRtxKernelResume (uint32_t sleep_ticks);
#else
#define EvrRtxKernelResume(sleep_ticks)
#endif

/**
  \brief  Event on successful RTOS Kernel scheduler resume (Op)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_RESUMED_DISABLE))
extern void EvrRtxKernelResumed (void);
#else
#define EvrRtxKernelResumed()
#endif

/**
  \brief  Event on RTOS kernel tick count retrieve (API)
  \param[in]  count         RTOS kernel current tick count.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_TICK_COUNT_DISABLE))
extern void EvrRtxKernelGetTickCount (uint32_t count);
#else
#define EvrRtxKernelGetTickCount(count)
#endif

/**
  \brief  Event on RTOS kernel tick frequency retrieve (API)
  \param[in]  freq          frequency of the kernel tick.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_TICK_FREQ_DISABLE))
extern void EvrRtxKernelGetTickFreq (uint32_t freq);
#else
#define EvrRtxKernelGetTickFreq(freq)
#endif

/**
  \brief  Event on RTOS kernel system timer count retrieve (API)
  \param[in]  count         RTOS kernel current system timer count as 32-bit value.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_SYS_TIMER_COUNT_DISABLE))
extern void EvrRtxKernelGetSysTimerCount (uint32_t count);
#else
#define EvrRtxKernelGetSysTimerCount(count)
#endif

/**
  \brief  Event on RTOS kernel system timer frequency retrieve (API)
  \param[in]  freq          frequency of the system timer.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_SYS_TIMER_FREQ_DISABLE))
extern void EvrRtxKernelGetSysTimerFreq (uint32_t freq);
#else
#define EvrRtxKernelGetSysTimerFreq(freq)
#endif


//  ==== Thread Events ====

/**
  \brief  Event on thread error (Error)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId or NULL when ID is unknown.
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_ERROR_DISABLE))
extern void EvrRtxThreadError (osThreadId_t thread_id, int32_t status);
#else
#define EvrRtxThreadError(thread_id, status)
#endif

/**
  \brief  Event on thread create and intialize (API)
  \param[in]  func          thread function.
  \param[in]  argument      pointer that is passed to the thread function as start argument.
  \param[in]  attr          thread attributes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_NEW_DISABLE))
extern void EvrRtxThreadNew (osThreadFunc_t func, void *argument, const osThreadAttr_t *attr);
#else
#define EvrRtxThreadNew(func, argument, attr)
#endif

/**
  \brief  Event on successful thread create (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  thread_addr   thread entry address.
  \param[in]  name          pointer to thread object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_CREATED_DISABLE))
extern void EvrRtxThreadCreated (osThreadId_t thread_id, uint32_t thread_addr, const char *name);
#else
#define EvrRtxThreadCreated(thread_id, thread_addr, name)
#endif

/**
  \brief  Event on thread name retrieve (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  name          pointer to thread object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_NAME_DISABLE))
extern void EvrRtxThreadGetName (osThreadId_t thread_id, const char *name);
#else
#define EvrRtxThreadGetName(thread_id, name)
#endif

/**
  \brief  Event on current running thread ID retrieve (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_ID_DISABLE))
extern void EvrRtxThreadGetId (osThreadId_t thread_id);
#else
#define EvrRtxThreadGetId(thread_id)
#endif

/**
  \brief  Event on thread state retrieve (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  state         current thread state of the specified thread.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_STATE_DISABLE))
extern void EvrRtxThreadGetState (osThreadId_t thread_id, osThreadState_t state);
#else
#define EvrRtxThreadGetState(thread_id, state)
#endif

/**
  \brief  Event on thread stack size retrieve (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  stack_size    stack size in bytes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_STACK_SIZE_DISABLE))
extern void EvrRtxThreadGetStackSize (osThreadId_t thread_id, uint32_t stack_size);
#else
#define EvrRtxThreadGetStackSize(thread_id, stack_size)
#endif

/**
  \brief  Event on available stack space retrieve (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  stack_space   remaining stack space in bytes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_STACK_SPACE_DISABLE))
extern void EvrRtxThreadGetStackSpace (osThreadId_t thread_id, uint32_t stack_space);
#else
#define EvrRtxThreadGetStackSpace(thread_id, stack_space)
#endif

/**
  \brief  Event on thread priority set (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  priority      new priority value for the thread function.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_SET_PRIORITY_DISABLE))
extern void EvrRtxThreadSetPriority (osThreadId_t thread_id, osPriority_t priority);
#else
#define EvrRtxThreadSetPriority(thread_id, priority)
#endif

/**
  \brief  Event on thread priority updated (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  priority      new priority value for the thread function.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_PRIORITY_UPDATED_DISABLE))
extern void EvrRtxThreadPriorityUpdated (osThreadId_t thread_id, osPriority_t priority);
#else
#define EvrRtxThreadPriorityUpdated(thread_id, priority)
#endif

/**
  \brief  Event on thread priority retrieve (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  priority      current priority value of the specified thread.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_PRIORITY_DISABLE))
extern void EvrRtxThreadGetPriority (osThreadId_t thread_id, osPriority_t priority);
#else
#define EvrRtxThreadGetPriority(thread_id, priority)
#endif

/**
  \brief  Event on thread yield (API)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_YIELD_DISABLE))
extern void EvrRtxThreadYield (void);
#else
#define EvrRtxThreadYield()
#endif

/**
  \brief  Event on thread suspend (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_SUSPEND_DISABLE))
extern void EvrRtxThreadSuspend (osThreadId_t thread_id);
#else
#define EvrRtxThreadSuspend(thread_id)
#endif

/**
  \brief  Event on successful thread suspend (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_SUSPENDED_DISABLE))
extern void EvrRtxThreadSuspended (osThreadId_t thread_id);
#else
#define EvrRtxThreadSuspended(thread_id)
#endif

/**
  \brief  Event on thread resume (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_RESUME_DISABLE))
extern void EvrRtxThreadResume (osThreadId_t thread_id);
#else
#define EvrRtxThreadResume(thread_id)
#endif

/**
  \brief  Event on successful thread resume (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_RESUMED_DISABLE))
extern void EvrRtxThreadResumed (osThreadId_t thread_id);
#else
#define EvrRtxThreadResumed(thread_id)
#endif

/**
  \brief  Event on thread detach (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_DETACH_DISABLE))
extern void EvrRtxThreadDetach (osThreadId_t thread_id);
#else
#define EvrRtxThreadDetach(thread_id)
#endif

/**
  \brief  Event on successful thread detach (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_DETACHED_DISABLE))
extern void EvrRtxThreadDetached (osThreadId_t thread_id);
#else
#define EvrRtxThreadDetached(thread_id)
#endif

/**
  \brief  Event on thread join (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_JOIN_DISABLE))
extern void EvrRtxThreadJoin (osThreadId_t thread_id);
#else
#define EvrRtxThreadJoin(thread_id)
#endif

/**
  \brief  Event on pending thread join (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_JOIN_PENDING_DISABLE))
extern void EvrRtxThreadJoinPending (osThreadId_t thread_id);
#else
#define EvrRtxThreadJoinPending(thread_id)
#endif

/**
  \brief  Event on successful thread join (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_JOINED_DISABLE))
extern void EvrRtxThreadJoined (osThreadId_t thread_id);
#else
#define EvrRtxThreadJoined(thread_id)
#endif

/**
  \brief  Event on thread execution block (Detail)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_BLOCKED_DISABLE))
extern void EvrRtxThreadBlocked (osThreadId_t thread_id, uint32_t timeout);
#else
#define EvrRtxThreadBlocked(thread_id, timeout)
#endif

/**
  \brief  Event on thread execution unblock (Detail)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  ret_val       extended execution status of the thread.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_UNBLOCKED_DISABLE))
extern void EvrRtxThreadUnblocked (osThreadId_t thread_id, uint32_t ret_val);
#else
#define EvrRtxThreadUnblocked(thread_id, ret_val)
#endif

/**
  \brief  Event on running thread pre-emption (Detail)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_PREEMPTED_DISABLE))
extern void EvrRtxThreadPreempted (osThreadId_t thread_id);
#else
#define EvrRtxThreadPreempted(thread_id)
#endif

/**
  \brief  Event on running thread switch (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_SWITCHED_DISABLE))
extern void EvrRtxThreadSwitched (osThreadId_t thread_id);
#else
#define EvrRtxThreadSwitched(thread_id)
#endif

/**
  \brief  Event on thread exit (API)
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_EXIT_DISABLE))
extern void EvrRtxThreadExit (void);
#else
#define EvrRtxThreadExit()
#endif

/**
  \brief  Event on thread terminate (API)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_TERMINATE_DISABLE))
extern void EvrRtxThreadTerminate (osThreadId_t thread_id);
#else
#define EvrRtxThreadTerminate(thread_id)
#endif

/**
  \brief  Event on successful thread terminate (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_DESTROYED_DISABLE))
extern void EvrRtxThreadDestroyed (osThreadId_t thread_id);
#else
#define EvrRtxThreadDestroyed(thread_id)
#endif

/**
  \brief  Event on active thread count retrieve (API)
  \param[in]  count         number of active threads.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_COUNT_DISABLE))
extern void EvrRtxThreadGetCount (uint32_t count);
#else
#define EvrRtxThreadGetCount(count)
#endif

/**
  \brief  Event on active threads enumerate (API)
  \param[in]  thread_array  pointer to array for retrieving thread IDs.
  \param[in]  array_items   maximum number of items in array for retrieving thread IDs.
  \param[in]  count         number of enumerated threads.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_ENUMERATE_DISABLE))
extern void EvrRtxThreadEnumerate (osThreadId_t *thread_array, uint32_t array_items, uint32_t count);
#else
#define EvrRtxThreadEnumerate(thread_array, array_items, count)
#endif


//  ==== Thread Flags Events ====

/**
  \brief  Event on thread flags error (Error)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId or NULL when ID is unknown.
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_ERROR_DISABLE))
extern void EvrRtxThreadFlagsError (osThreadId_t thread_id, int32_t status);
#else
#define EvrRtxThreadFlagsError(thread_id, status)
#endif

/**
  \brief  Event on thread flags set (API)
  \param[in]   thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]   flags         flags of the thread that shall be set.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_SET_DISABLE))
extern void EvrRtxThreadFlagsSet (osThreadId_t thread_id, uint32_t flags);
#else
#define EvrRtxThreadFlagsSet(thread_id, flags)
#endif

/**
  \brief  Event on successful thread flags set (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
  \param[in]  thread_flags  thread flags after setting.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_SET_DONE_DISABLE))
extern void EvrRtxThreadFlagsSetDone (osThreadId_t thread_id, uint32_t thread_flags);
#else
#define EvrRtxThreadFlagsSetDone(thread_id, thread_flags)
#endif

/**
  \brief  Event on thread flags clear (API)
  \param[in]  flags         flags of the thread that shall be cleared.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_CLEAR_DISABLE))
extern void EvrRtxThreadFlagsClear (uint32_t flags);
#else
#define EvrRtxThreadFlagsClear(flags)
#endif

/**
  \brief  Event on successful thread flags clear (Op)
  \param[in]  thread_flags  thread flags before clearing.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_CLEAR_DONE_DISABLE))
extern void EvrRtxThreadFlagsClearDone (uint32_t thread_flags);
#else
#define EvrRtxThreadFlagsClearDone(thread_flags)
#endif

/**
  \brief  Event on thread flags retrieve (API)
  \param[in]  thread_flags  current thread flags.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_GET_DISABLE))
extern void EvrRtxThreadFlagsGet (uint32_t thread_flags);
#else
#define EvrRtxThreadFlagsGet(thread_flags)
#endif

/**
  \brief  Event on wait for thread flags (API)
  \param[in]  flags         flags to wait for.
  \param[in]  options       flags options (osFlagsXxxx).
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_DISABLE))
extern void EvrRtxThreadFlagsWait (uint32_t flags, uint32_t options, uint32_t timeout);
#else
#define EvrRtxThreadFlagsWait(flags, options, timeout)
#endif

/**
  \brief  Event on pending wait for thread flags (Op)
  \param[in]  flags         flags to wait for.
  \param[in]  options       flags options (osFlagsXxxx).
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_PENDING_DISABLE))
extern void EvrRtxThreadFlagsWaitPending (uint32_t flags, uint32_t options, uint32_t timeout);
#else
#define EvrRtxThreadFlagsWaitPending(flags, options, timeout)
#endif

/**
  \brief  Event on wait timeout for thread flags (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_TIMEOUT_DISABLE))
extern void EvrRtxThreadFlagsWaitTimeout (osThreadId_t thread_id);
#else
#define EvrRtxThreadFlagsWaitTimeout(thread_id)
#endif

/**
  \brief  Event on successful wait for thread flags (Op)
  \param[in]  flags         flags to wait for.
  \param[in]  options       flags options (osFlagsXxxx).
  \param[in]  thread_flags  thread flags before clearing.
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_COMPLETED_DISABLE))
extern void EvrRtxThreadFlagsWaitCompleted (uint32_t flags, uint32_t options, uint32_t thread_flags, osThreadId_t thread_id);
#else
#define EvrRtxThreadFlagsWaitCompleted(flags, options, thread_flags, thread_id)
#endif

/**
  \brief  Event on unsuccessful wait for thread flags (Op)
  \param[in]  flags         flags to wait for.
  \param[in]  options       flags options (osFlagsXxxx).
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_NOT_COMPLETED_DISABLE))
extern void EvrRtxThreadFlagsWaitNotCompleted (uint32_t flags, uint32_t options);
#else
#define EvrRtxThreadFlagsWaitNotCompleted(flags, options)
#endif


//  ==== Generic Wait Events ====

/**
  \brief  Event on delay error (Error)
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_ERROR_DISABLE))
extern void EvrRtxDelayError (int32_t status);
#else
#define EvrRtxDelayError(status)
#endif

/**
  \brief  Event on delay for specified time (API)
  \param[in]  ticks         \ref CMSIS_RTOS_TimeOutValue "time ticks" value.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_DISABLE))
extern void EvrRtxDelay (uint32_t ticks);
#else
#define EvrRtxDelay(ticks)
#endif

/**
  \brief  Event on delay until specified time (API)
  \param[in]  ticks         absolute time in ticks.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_UNTIL_DISABLE))
extern void EvrRtxDelayUntil (uint32_t ticks);
#else
#define EvrRtxDelayUntil(ticks)
#endif

/**
  \brief  Event on delay started (Op)
  \param[in]  ticks         \ref CMSIS_RTOS_TimeOutValue "time ticks" value.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_STARTED_DISABLE))
extern void EvrRtxDelayStarted (uint32_t ticks);
#else
#define EvrRtxDelayStarted(ticks)
#endif

/**
  \brief  Event on delay until specified time started (Op)
  \param[in]  ticks         \ref CMSIS_RTOS_TimeOutValue "time ticks" value.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_UNTIL_STARTED_DISABLE))
extern void EvrRtxDelayUntilStarted (uint32_t ticks);
#else
#define EvrRtxDelayUntilStarted(ticks)
#endif

/**
  \brief  Event on delay completed (Op)
  \param[in]  thread_id     thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_COMPLETED_DISABLE))
extern void EvrRtxDelayCompleted (osThreadId_t thread_id);
#else
#define EvrRtxDelayCompleted(thread_id)
#endif


//  ==== Timer Events ====

/**
  \brief  Event on timer error (Error)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew or NULL when ID is unknown.
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_ERROR_DISABLE))
extern void EvrRtxTimerError (osTimerId_t timer_id, int32_t status);
#else
#define EvrRtxTimerError(timer_id, status)
#endif

/**
  \brief  Event on timer callback call (Op)
  \param[in]  func          start address of a timer call back function.
  \param[in]  argument      argument to the timer call back function.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_CALLBACK_DISABLE))
extern void EvrRtxTimerCallback (osTimerFunc_t func, void *argument);
#else
#define EvrRtxTimerCallback(func, argument)
#endif

/**
  \brief  Event on timer create and initialize (API)
  \param[in]  func          start address of a timer call back function.
  \param[in]  type          osTimerOnce for one-shot or osTimerPeriodic for periodic behavior.
  \param[in]  argument      argument to the timer call back function.
  \param[in]  attr          timer attributes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_NEW_DISABLE))
extern void EvrRtxTimerNew (osTimerFunc_t func, osTimerType_t type, void *argument, const osTimerAttr_t *attr);
#else
#define EvrRtxTimerNew(func, type, argument, attr)
#endif

/**
  \brief  Event on successful timer create (Op)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
  \param[in]  name          pointer to timer object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_CREATED_DISABLE))
extern void EvrRtxTimerCreated (osTimerId_t timer_id, const char *name);
#else
#define EvrRtxTimerCreated(timer_id, name)
#endif

/**
  \brief  Event on timer name retrieve (API)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
  \param[in]  name          pointer to timer object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_GET_NAME_DISABLE))
extern void EvrRtxTimerGetName (osTimerId_t timer_id, const char *name);
#else
#define EvrRtxTimerGetName(timer_id, name)
#endif

/**
  \brief  Event on timer start (API)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
  \param[in]  ticks         \ref CMSIS_RTOS_TimeOutValue "time ticks" value of the timer.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_START_DISABLE))
extern void EvrRtxTimerStart (osTimerId_t timer_id, uint32_t ticks);
#else
#define EvrRtxTimerStart(timer_id, ticks)
#endif

/**
  \brief  Event on successful timer start (Op)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_STARTED_DISABLE))
extern void EvrRtxTimerStarted (osTimerId_t timer_id);
#else
#define EvrRtxTimerStarted(timer_id)
#endif

/**
  \brief  Event on timer stop (API)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_STOP_DISABLE))
extern void EvrRtxTimerStop (osTimerId_t timer_id);
#else
#define EvrRtxTimerStop(timer_id)
#endif

/**
  \brief  Event on successful timer stop (Op)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_STOPPED_DISABLE))
extern void EvrRtxTimerStopped (osTimerId_t timer_id);
#else
#define EvrRtxTimerStopped(timer_id)
#endif

/**
  \brief  Event on timer running state check (API)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
  \param[in]  running       running state: 0 not running, 1 running.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_IS_RUNNING_DISABLE))
extern void EvrRtxTimerIsRunning (osTimerId_t timer_id, uint32_t running);
#else
#define EvrRtxTimerIsRunning(timer_id, running)
#endif

/**
  \brief  Event on timer delete (API)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_DELETE_DISABLE))
extern void EvrRtxTimerDelete (osTimerId_t timer_id);
#else
#define EvrRtxTimerDelete(timer_id)
#endif

/**
  \brief  Event on successful timer delete (Op)
  \param[in]  timer_id      timer ID obtained by \ref osTimerNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_DESTROYED_DISABLE))
extern void EvrRtxTimerDestroyed (osTimerId_t timer_id);
#else
#define EvrRtxTimerDestroyed(timer_id)
#endif


//  ==== Event Flags Events ====

/**
  \brief  Event on event flags error (Error)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew or NULL when ID is unknown.
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_ERROR_DISABLE))
extern void EvrRtxEventFlagsError (osEventFlagsId_t ef_id, int32_t status);
#else
#define EvrRtxEventFlagsError(ef_id, status)
#endif

/**
  \brief  Event on event flags create and initialize (API)
  \param[in]  attr          event flags attributes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_NEW_DISABLE))
extern void EvrRtxEventFlagsNew (const osEventFlagsAttr_t *attr);
#else
#define EvrRtxEventFlagsNew(attr)
#endif

/**
  \brief  Event on successful event flags create (Op)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  name          pointer to event flags object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_CREATED_DISABLE))
extern void EvrRtxEventFlagsCreated (osEventFlagsId_t ef_id, const char *name);
#else
#define EvrRtxEventFlagsCreated(ef_id, name)
#endif

/**
  \brief  Event on event flags name retrieve (API)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  name          pointer to event flags object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_GET_NAME_DISABLE))
extern void EvrRtxEventFlagsGetName (osEventFlagsId_t ef_id, const char *name);
#else
#define EvrRtxEventFlagsGetName(ef_id, name)
#endif

/**
  \brief  Event on event flags set (API)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  flags         flags that shall be set.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_SET_DISABLE))
extern void EvrRtxEventFlagsSet (osEventFlagsId_t ef_id, uint32_t flags);
#else
#define EvrRtxEventFlagsSet(ef_id, flags)
#endif

/**
  \brief  Event on successful event flags set (Op)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  event_flags   event flags after setting.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_SET_DONE_DISABLE))
extern void EvrRtxEventFlagsSetDone (osEventFlagsId_t ef_id, uint32_t event_flags);
#else
#define EvrRtxEventFlagsSetDone(ef_id, event_flags)
#endif

/**
  \brief  Event on event flags clear (API)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  flags         flags that shall be cleared.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_CLEAR_DISABLE))
extern void EvrRtxEventFlagsClear (osEventFlagsId_t ef_id, uint32_t flags);
#else
#define EvrRtxEventFlagsClear(ef_id, flags)
#endif

/**
  \brief  Event on successful event flags clear (Op)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  event_flags   event flags before clearing.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_CLEAR_DONE_DISABLE))
extern void EvrRtxEventFlagsClearDone (osEventFlagsId_t ef_id, uint32_t event_flags);
#else
#define EvrRtxEventFlagsClearDone(ef_id, event_flags)
#endif

/**
  \brief  Event on event flags retrieve (API)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  event_flags   current event flags.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_GET_DISABLE))
extern void EvrRtxEventFlagsGet (osEventFlagsId_t ef_id, uint32_t event_flags);
#else
#define EvrRtxEventFlagsGet(ef_id, event_flags)
#endif

/**
  \brief  Event on wait for event flags (API)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  flags         flags to wait for.
  \param[in]  options       flags options (osFlagsXxxx).
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_DISABLE))
extern void EvrRtxEventFlagsWait (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout);
#else
#define EvrRtxEventFlagsWait(ef_id, flags, options, timeout)
#endif

/**
  \brief  Event on pending wait for event flags (Op)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  flags         flags to wait for.
  \param[in]  options       flags options (osFlagsXxxx).
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_PENDING_DISABLE))
extern void EvrRtxEventFlagsWaitPending (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout);
#else
#define EvrRtxEventFlagsWaitPending(ef_id, flags, options, timeout)
#endif

/**
  \brief  Event on wait timeout for event flags (Op)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_TIMEOUT_DISABLE))
extern void EvrRtxEventFlagsWaitTimeout (osEventFlagsId_t ef_id);
#else
#define EvrRtxEventFlagsWaitTimeout(ef_id)
#endif

/**
  \brief  Event on successful wait for event flags (Op)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  flags         flags to wait for.
  \param[in]  options       flags options (osFlagsXxxx).
  \param[in]  event_flags   event flags before clearing or 0 if specified flags have not been set.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_COMPLETED_DISABLE))
extern void EvrRtxEventFlagsWaitCompleted (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t event_flags);
#else
#define EvrRtxEventFlagsWaitCompleted(ef_id, flags, options, event_flags)
#endif

/**
  \brief  Event on unsuccessful wait for event flags (Op)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
  \param[in]  flags         flags to wait for.
  \param[in]  options       flags options (osFlagsXxxx).
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_NOT_COMPLETED_DISABLE))
extern void EvrRtxEventFlagsWaitNotCompleted (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options);
#else
#define EvrRtxEventFlagsWaitNotCompleted(ef_id, flags, options)
#endif

/**
  \brief  Event on event flags delete (API)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_DELETE_DISABLE))
extern void EvrRtxEventFlagsDelete (osEventFlagsId_t ef_id);
#else
#define EvrRtxEventFlagsDelete(ef_id)
#endif

/**
  \brief  Event on successful event flags delete (Op)
  \param[in]  ef_id         event flags ID obtained by \ref osEventFlagsNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_DESTROYED_DISABLE))
extern void EvrRtxEventFlagsDestroyed (osEventFlagsId_t ef_id);
#else
#define EvrRtxEventFlagsDestroyed(ef_id)
#endif


//  ==== Mutex Events ====

/**
  \brief  Event on mutex error (Error)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew or NULL when ID is unknown.
  \param[in]  status    extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ERROR_DISABLE))
extern void EvrRtxMutexError (osMutexId_t mutex_id, int32_t status);
#else
#define EvrRtxMutexError(mutex_id, status)
#endif

/**
  \brief  Event on mutex create and initialize (API)
  \param[in]  attr      mutex attributes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_NEW_DISABLE))
extern void EvrRtxMutexNew (const osMutexAttr_t *attr);
#else
#define EvrRtxMutexNew(attr)
#endif

/**
  \brief  Event on successful mutex create (Op)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
  \param[in]  name      pointer to mutex object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_CREATED_DISABLE))
extern void EvrRtxMutexCreated (osMutexId_t mutex_id, const char *name);
#else
#define EvrRtxMutexCreated(mutex_id, name)
#endif

/**
  \brief  Event on mutex name retrieve (API)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
  \param[in]  name      pointer to mutex object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_GET_NAME_DISABLE))
extern void EvrRtxMutexGetName (osMutexId_t mutex_id, const char *name);
#else
#define EvrRtxMutexGetName(mutex_id, name)
#endif

/**
  \brief  Event on mutex acquire (API)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
  \param[in]  timeout   \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ACQUIRE_DISABLE))
extern void EvrRtxMutexAcquire (osMutexId_t mutex_id, uint32_t timeout);
#else
#define EvrRtxMutexAcquire(mutex_id, timeout)
#endif

/**
  \brief  Event on pending mutex acquire (Op)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
  \param[in]  timeout   \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ACQUIRE_PENDING_DISABLE))
extern void EvrRtxMutexAcquirePending (osMutexId_t mutex_id, uint32_t timeout);
#else
#define EvrRtxMutexAcquirePending(mutex_id, timeout)
#endif

/**
  \brief  Event on mutex acquire timeout (Op)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ACQUIRE_TIMEOUT_DISABLE))
extern void EvrRtxMutexAcquireTimeout (osMutexId_t mutex_id);
#else
#define EvrRtxMutexAcquireTimeout(mutex_id)
#endif

/**
  \brief  Event on successful mutex acquire (Op)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
  \param[in]  lock      current number of times mutex object is locked.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ACQUIRED_DISABLE))
extern void EvrRtxMutexAcquired (osMutexId_t mutex_id, uint32_t lock);
#else
#define EvrRtxMutexAcquired(mutex_id, lock)
#endif

/**
  \brief  Event on unsuccessful mutex acquire (Op)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_NOT_ACQUIRED_DISABLE))
extern void EvrRtxMutexNotAcquired (osMutexId_t mutex_id);
#else
#define EvrRtxMutexNotAcquired(mutex_id)
#endif

/**
  \brief  Event on mutex release (API)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_RELEASE_DISABLE))
extern void EvrRtxMutexRelease (osMutexId_t mutex_id);
#else
#define EvrRtxMutexRelease(mutex_id)
#endif

/**
  \brief  Event on successful mutex release (Op)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
  \param[in]  lock      current number of times mutex object is locked.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_RELEASED_DISABLE))
extern void EvrRtxMutexReleased (osMutexId_t mutex_id, uint32_t lock);
#else
#define EvrRtxMutexReleased(mutex_id, lock)
#endif

/**
  \brief  Event on mutex owner retrieve (API)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
  \param[in]  thread_id thread ID obtained by \ref osThreadNew or \ref osThreadGetId.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_GET_OWNER_DISABLE))
extern void EvrRtxMutexGetOwner (osMutexId_t mutex_id, osThreadId_t thread_id);
#else
#define EvrRtxMutexGetOwner(mutex_id, thread_id)
#endif

/**
  \brief  Event on mutex delete (API)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_DELETE_DISABLE))
extern void EvrRtxMutexDelete (osMutexId_t mutex_id);
#else
#define EvrRtxMutexDelete(mutex_id)
#endif

/**
  \brief  Event on successful mutex delete (Op)
  \param[in]  mutex_id  mutex ID obtained by \ref osMutexNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_DESTROYED_DISABLE))
extern void EvrRtxMutexDestroyed (osMutexId_t mutex_id);
#else
#define EvrRtxMutexDestroyed(mutex_id)
#endif


//  ==== Semaphore Events ====

/**
  \brief  Event on semaphore error (Error)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew or NULL when ID is unknown.
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ERROR_DISABLE))
extern void EvrRtxSemaphoreError (osSemaphoreId_t semaphore_id, int32_t status);
#else
#define EvrRtxSemaphoreError(semaphore_id, status)
#endif

/**
  \brief  Event on semaphore create and initialize (API)
  \param[in]  max_count     maximum number of available tokens.
  \param[in]  initial_count initial number of available tokens.
  \param[in]  attr          semaphore attributes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_NEW_DISABLE))
extern void EvrRtxSemaphoreNew (uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr);
#else
#define EvrRtxSemaphoreNew(max_count, initial_count, attr)
#endif

/**
  \brief  Event on successful semaphore create (Op)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
  \param[in]  name          pointer to semaphore object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_CREATED_DISABLE))
extern void EvrRtxSemaphoreCreated (osSemaphoreId_t semaphore_id, const char *name);
#else
#define EvrRtxSemaphoreCreated(semaphore_id, name)
#endif

/**
  \brief  Event on semaphore name retrieve (API)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
  \param[in]  name          pointer to semaphore object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_GET_NAME_DISABLE))
extern void EvrRtxSemaphoreGetName (osSemaphoreId_t semaphore_id, const char *name);
#else
#define EvrRtxSemaphoreGetName(semaphore_id, name)
#endif

/**
  \brief  Event on semaphore acquire (API)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ACQUIRE_DISABLE))
extern void EvrRtxSemaphoreAcquire (osSemaphoreId_t semaphore_id, uint32_t timeout);
#else
#define EvrRtxSemaphoreAcquire(semaphore_id, timeout)
#endif

/**
  \brief  Event on pending semaphore acquire (Op)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ACQUIRE_PENDING_DISABLE))
extern void EvrRtxSemaphoreAcquirePending (osSemaphoreId_t semaphore_id, uint32_t timeout);
#else
#define EvrRtxSemaphoreAcquirePending(semaphore_id, timeout)
#endif

/**
  \brief  Event on semaphore acquire timeout (Op)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ACQUIRE_TIMEOUT_DISABLE))
extern void EvrRtxSemaphoreAcquireTimeout (osSemaphoreId_t semaphore_id);
#else
#define EvrRtxSemaphoreAcquireTimeout(semaphore_id)
#endif

/**
  \brief  Event on successful semaphore acquire (Op)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
  \param[in]  tokens        number of available tokens.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ACQUIRED_DISABLE))
extern void EvrRtxSemaphoreAcquired (osSemaphoreId_t semaphore_id, uint32_t tokens);
#else
#define EvrRtxSemaphoreAcquired(semaphore_id, tokens)
#endif

/**
  \brief  Event on unsuccessful semaphore acquire (Op)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_NOT_ACQUIRED_DISABLE))
extern void EvrRtxSemaphoreNotAcquired (osSemaphoreId_t semaphore_id);
#else
#define EvrRtxSemaphoreNotAcquired(semaphore_id)
#endif

/**
  \brief  Event on semaphore release (API)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_RELEASE_DISABLE))
extern void EvrRtxSemaphoreRelease (osSemaphoreId_t semaphore_id);
#else
#define EvrRtxSemaphoreRelease(semaphore_id)
#endif

/**
  \brief  Event on successful semaphore release (Op)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
  \param[in]  tokens        number of available tokens.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_RELEASED_DISABLE))
extern void EvrRtxSemaphoreReleased (osSemaphoreId_t semaphore_id, uint32_t tokens);
#else
#define EvrRtxSemaphoreReleased(semaphore_id, tokens)
#endif

/**
  \brief  Event on semaphore token count retrieval (API)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
  \param[in]  count         current number of available tokens.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_GET_COUNT_DISABLE))
extern void EvrRtxSemaphoreGetCount (osSemaphoreId_t semaphore_id, uint32_t count);
#else
#define EvrRtxSemaphoreGetCount(semaphore_id, count)
#endif

/**
  \brief  Event on semaphore delete (API)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_DELETE_DISABLE))
extern void EvrRtxSemaphoreDelete (osSemaphoreId_t semaphore_id);
#else
#define EvrRtxSemaphoreDelete(semaphore_id)
#endif

/**
  \brief  Event on successful semaphore delete (Op)
  \param[in]  semaphore_id  semaphore ID obtained by \ref osSemaphoreNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_DESTROYED_DISABLE))
extern void EvrRtxSemaphoreDestroyed (osSemaphoreId_t semaphore_id);
#else
#define EvrRtxSemaphoreDestroyed(semaphore_id)
#endif


//  ==== Memory Pool Events ====

/**
  \brief  Event on memory pool error (Error)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew or NULL when ID is unknown.
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ERROR_DISABLE))
extern void EvrRtxMemoryPoolError (osMemoryPoolId_t mp_id, int32_t status);
#else
#define EvrRtxMemoryPoolError(mp_id, status)
#endif

/**
  \brief  Event on memory pool create and initialize (API)
  \param[in]  block_count   maximum number of memory blocks in memory pool.
  \param[in]  block_size    memory block size in bytes.
  \param[in]  attr          memory pool attributes; NULL: default values.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_NEW_DISABLE))
extern void EvrRtxMemoryPoolNew (uint32_t block_count, uint32_t block_size, const osMemoryPoolAttr_t *attr);
#else
#define EvrRtxMemoryPoolNew(block_count, block_size, attr)
#endif

/**
  \brief  Event on successful memory pool create (Op)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  name          pointer to memory pool object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_CREATED_DISABLE))
extern void EvrRtxMemoryPoolCreated (osMemoryPoolId_t mp_id, const char *name);
#else
#define EvrRtxMemoryPoolCreated(mp_id, name)
#endif

/**
  \brief  Event on memory pool name retrieve (API)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  name          pointer to memory pool object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_NAME_DISABLE))
extern void EvrRtxMemoryPoolGetName (osMemoryPoolId_t mp_id, const char *name);
#else
#define EvrRtxMemoryPoolGetName(mp_id, name)
#endif

/**
  \brief  Event on memory pool allocation (API)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOC_DISABLE))
extern void EvrRtxMemoryPoolAlloc (osMemoryPoolId_t mp_id, uint32_t timeout);
#else
#define EvrRtxMemoryPoolAlloc(mp_id, timeout)
#endif

/**
  \brief  Event on pending memory pool allocation (Op)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOC_PENDING_DISABLE))
extern void EvrRtxMemoryPoolAllocPending (osMemoryPoolId_t mp_id, uint32_t timeout);
#else
#define EvrRtxMemoryPoolAllocPending(mp_id, timeout)
#endif

/**
  \brief  Event on memory pool allocation timeout (Op)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOC_TIMEOUT_DISABLE))
extern void EvrRtxMemoryPoolAllocTimeout (osMemoryPoolId_t mp_id);
#else
#define EvrRtxMemoryPoolAllocTimeout(mp_id)
#endif

/**
  \brief  Event on successful memory pool allocation (Op)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  block         address of the allocated memory block.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOCATED_DISABLE))
extern void EvrRtxMemoryPoolAllocated (osMemoryPoolId_t mp_id, void *block);
#else
#define EvrRtxMemoryPoolAllocated(mp_id, block)
#endif

/**
  \brief  Event on unsuccessful memory pool allocation (Op)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOC_FAILED_DISABLE))
extern void EvrRtxMemoryPoolAllocFailed (osMemoryPoolId_t mp_id);
#else
#define EvrRtxMemoryPoolAllocFailed(mp_id)
#endif

/**
  \brief  Event on memory pool free (API)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  block         address of the allocated memory block to be returned to the memory pool.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_FREE_DISABLE))
extern void EvrRtxMemoryPoolFree (osMemoryPoolId_t mp_id, void *block);
#else
#define EvrRtxMemoryPoolFree(mp_id, block)
#endif

/**
  \brief  Event on successful memory pool free (Op)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  block         address of the allocated memory block to be returned to the memory pool.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_DEALLOCATED_DISABLE))
extern void EvrRtxMemoryPoolDeallocated (osMemoryPoolId_t mp_id, void *block);
#else
#define EvrRtxMemoryPoolDeallocated(mp_id, block)
#endif

/**
  \brief  Event on unsuccessful memory pool free (Op)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  block         address of the allocated memory block to be returned to the memory pool.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_FREE_FAILED_DISABLE))
extern void EvrRtxMemoryPoolFreeFailed (osMemoryPoolId_t mp_id, void *block);
#else
#define EvrRtxMemoryPoolFreeFailed(mp_id, block)
#endif

/**
  \brief  Event on memory pool capacity retrieve (API)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  capacity      maximum number of memory blocks.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_CAPACITY_DISABLE))
extern void EvrRtxMemoryPoolGetCapacity (osMemoryPoolId_t mp_id, uint32_t capacity);
#else
#define EvrRtxMemoryPoolGetCapacity(mp_id, capacity)
#endif

/**
  \brief  Event on memory pool block size retrieve (API)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  block_size    memory block size in bytes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_BLOCK_SZIE_DISABLE))
extern void EvrRtxMemoryPoolGetBlockSize (osMemoryPoolId_t mp_id, uint32_t block_size);
#else
#define EvrRtxMemoryPoolGetBlockSize(mp_id, block_size)
#endif

/**
  \brief  Event on used memory pool blocks retrieve (API)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  count         number of memory blocks used.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_COUNT_DISABLE))
extern void EvrRtxMemoryPoolGetCount (osMemoryPoolId_t mp_id, uint32_t count);
#else
#define EvrRtxMemoryPoolGetCount(mp_id, count)
#endif

/**
  \brief  Event on available memory pool blocks retrieve (API)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
  \param[in]  space         number of memory blocks available.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_SPACE_DISABLE))
extern void EvrRtxMemoryPoolGetSpace (osMemoryPoolId_t mp_id, uint32_t space);
#else
#define EvrRtxMemoryPoolGetSpace(mp_id, space)
#endif

/**
  \brief  Event on memory pool delete (API)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_DELETE_DISABLE))
extern void EvrRtxMemoryPoolDelete (osMemoryPoolId_t mp_id);
#else
#define EvrRtxMemoryPoolDelete(mp_id)
#endif

/**
  \brief  Event on successful memory pool delete (Op)
  \param[in]  mp_id         memory pool ID obtained by \ref osMemoryPoolNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_DESTROYED_DISABLE))
extern void EvrRtxMemoryPoolDestroyed (osMemoryPoolId_t mp_id);
#else
#define EvrRtxMemoryPoolDestroyed(mp_id)
#endif


//  ==== Message Queue Events ====

/**
  \brief  Event on message queue error (Error)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew or NULL when ID is unknown.
  \param[in]  status        extended execution status.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_ERROR_DISABLE))
extern void EvrRtxMessageQueueError (osMessageQueueId_t mq_id, int32_t status);
#else
#define EvrRtxMessageQueueError(mq_id, status)
#endif

/**
  \brief  Event on message queue create and initialization (API)
  \param[in]  msg_count     maximum number of messages in queue.
  \param[in]  msg_size      maximum message size in bytes.
  \param[in]  attr          message queue attributes; NULL: default values.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_NEW_DISABLE))
extern void EvrRtxMessageQueueNew (uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr);
#else
#define EvrRtxMessageQueueNew(msg_count, msg_size, attr)
#endif

/**
  \brief  Event on successful message queue create (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  name          pointer to message queue object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_CREATED_DISABLE))
extern void EvrRtxMessageQueueCreated (osMessageQueueId_t mq_id, const char *name);
#else
#define EvrRtxMessageQueueCreated(mq_id, name)
#endif

/**
  \brief  Event on message queue name retrieve(API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  name          pointer to message queue object name.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_NAME_DISABLE))
extern void EvrRtxMessageQueueGetName (osMessageQueueId_t mq_id, const char *name);
#else
#define EvrRtxMessageQueueGetName(mq_id, name)
#endif

/**
  \brief  Event on message put (API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer with message to put into a queue.
  \param[in]  msg_prio      message priority.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_PUT_DISABLE))
extern void EvrRtxMessageQueuePut (osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout);
#else
#define EvrRtxMessageQueuePut(mq_id, msg_ptr, msg_prio, timeout)
#endif

/**
  \brief  Event on pending message put (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer with message to put into a queue.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_PUT_PENDING_DISABLE))
extern void EvrRtxMessageQueuePutPending (osMessageQueueId_t mq_id, const void *msg_ptr, uint32_t timeout);
#else
#define EvrRtxMessageQueuePutPending(mq_id, msg_ptr, timeout)
#endif

/**
  \brief  Event on message put timeout (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_PUT_TIMEOUT_DISABLE))
extern void EvrRtxMessageQueuePutTimeout (osMessageQueueId_t mq_id);
#else
#define EvrRtxMessageQueuePutTimeout(mq_id)
#endif

/**
  \brief  Event on pending message insert (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer with message to put into a queue.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_INSERT_PENDING_DISABLE))
extern void EvrRtxMessageQueueInsertPending (osMessageQueueId_t mq_id, const void *msg_ptr);
#else
#define EvrRtxMessageQueueInsertPending(mq_id, msg_ptr)
#endif

/**
  \brief  Event on successful message insert (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer with message to put into a queue.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_INSERTED_DISABLE))
extern void EvrRtxMessageQueueInserted (osMessageQueueId_t mq_id, const void *msg_ptr);
#else
#define EvrRtxMessageQueueInserted(mq_id, msg_ptr)
#endif

/**
  \brief  Event on unsuccessful message insert (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer with message to put into a queue.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_NOT_INSERTED_DISABLE))
extern void EvrRtxMessageQueueNotInserted (osMessageQueueId_t mq_id, const void *msg_ptr);
#else
#define EvrRtxMessageQueueNotInserted(mq_id, msg_ptr)
#endif

/**
  \brief  Event on message get (API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer for message to get from a queue.
  \param[in]  msg_prio      message priority.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_DISABLE))
extern void EvrRtxMessageQueueGet (osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout);
#else
#define EvrRtxMessageQueueGet(mq_id, msg_ptr, msg_prio, timeout)
#endif

/**
  \brief  Event on pending message get (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer for message to get from a queue.
  \param[in]  timeout       \ref CMSIS_RTOS_TimeOutValue or 0 in case of no time-out.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_PENDING_DISABLE))
extern void EvrRtxMessageQueueGetPending (osMessageQueueId_t mq_id, void *msg_ptr, uint32_t timeout);
#else
#define EvrRtxMessageQueueGetPending(mq_id, msg_ptr, timeout)
#endif

/**
  \brief  Event on message get timeout (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_TIMEOUT_DISABLE))
extern void EvrRtxMessageQueueGetTimeout (osMessageQueueId_t mq_id);
#else
#define EvrRtxMessageQueueGetTimeout(mq_id)
#endif

/**
  \brief  Event on successful message get (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer for message to get from a queue.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_RETRIEVED_DISABLE))
extern void EvrRtxMessageQueueRetrieved (osMessageQueueId_t mq_id, void *msg_ptr);
#else
#define EvrRtxMessageQueueRetrieved(mq_id, msg_ptr)
#endif

/**
  \brief  Event on unsuccessful message get (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_ptr       pointer to buffer for message to get from a queue.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_NOT_RETRIEVED_DISABLE))
extern void EvrRtxMessageQueueNotRetrieved (osMessageQueueId_t mq_id, void *msg_ptr);
#else
#define EvrRtxMessageQueueNotRetrieved(mq_id, msg_ptr)
#endif

/**
  \brief  Event on message queue capacity retrieve (API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  capacity      maximum number of messages.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_CAPACITY_DISABLE))
extern void EvrRtxMessageQueueGetCapacity (osMessageQueueId_t mq_id, uint32_t capacity);
#else
#define EvrRtxMessageQueueGetCapacity(mq_id, capacity)
#endif

/**
  \brief  Event on message queue message size retrieve (API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  msg_size      maximum message size in bytes.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_MSG_SIZE_DISABLE))
extern void EvrRtxMessageQueueGetMsgSize (osMessageQueueId_t mq_id, uint32_t msg_size);
#else
#define EvrRtxMessageQueueGetMsgSize(mq_id, msg_size)
#endif

/**
  \brief  Event on message queue message count retrieve (API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  count         number of queued messages.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_COUNT_DISABLE))
extern void EvrRtxMessageQueueGetCount (osMessageQueueId_t mq_id, uint32_t count);
#else
#define EvrRtxMessageQueueGetCount(mq_id, count)
#endif

/**
  \brief  Event on message queue message slots retrieve (API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
  \param[in]  space         number of available slots for messages.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_SPACE_DISABLE))
extern void EvrRtxMessageQueueGetSpace (osMessageQueueId_t mq_id, uint32_t space);
#else
#define EvrRtxMessageQueueGetSpace(mq_id, space)
#endif

/**
  \brief  Event on message queue reset (API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_RESET_DISABLE))
extern void EvrRtxMessageQueueReset (osMessageQueueId_t mq_id);
#else
#define EvrRtxMessageQueueReset(mq_id)
#endif

/**
  \brief  Event on successful message queue reset (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_RESET_DONE_DISABLE))
extern void EvrRtxMessageQueueResetDone (osMessageQueueId_t mq_id);
#else
#define EvrRtxMessageQueueResetDone(mq_id)
#endif

/**
  \brief  Event on message queue delete (API)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_DELETE_DISABLE))
extern void EvrRtxMessageQueueDelete (osMessageQueueId_t mq_id);
#else
#define EvrRtxMessageQueueDelete(mq_id)
#endif

/**
  \brief  Event on successful message queue delete (Op)
  \param[in]  mq_id         message queue ID obtained by \ref osMessageQueueNew.
*/
#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_DESTROYED_DISABLE))
extern void EvrRtxMessageQueueDestroyed (osMessageQueueId_t mq_id);
#else
#define EvrRtxMessageQueueDestroyed(mq_id)
#endif


#endif  // RTX_EVR_H_
