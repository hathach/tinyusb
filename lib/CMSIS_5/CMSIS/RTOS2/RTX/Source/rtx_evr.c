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
 * Title:       RTX Event Recorder
 *
 * -----------------------------------------------------------------------------
 */

#include <string.h>
#include "cmsis_compiler.h"
#include "rtx_evr.h"                    // RTX Event Recorder definitions

#ifdef  RTE_Compiler_EventRecorder

//lint -e923 -e9074 -e9078 [MISRA Note 13]

/// Event IDs for "RTX Memory Management"
#define EvtRtxMemoryInit                    EventID(EventLevelOp,     EvtRtxMemoryNo, 0x00U)
#define EvtRtxMemoryAlloc                   EventID(EventLevelOp,     EvtRtxMemoryNo, 0x01U)
#define EvtRtxMemoryFree                    EventID(EventLevelOp,     EvtRtxMemoryNo, 0x02U)
#define EvtRtxMemoryBlockInit               EventID(EventLevelOp,     EvtRtxMemoryNo, 0x03U)
#define EvtRtxMemoryBlockAlloc              EventID(EventLevelOp,     EvtRtxMemoryNo, 0x04U)
#define EvtRtxMemoryBlockFree               EventID(EventLevelOp,     EvtRtxMemoryNo, 0x05U)

/// Event IDs for "RTX Kernel"
#define EvtRtxKernelError                   EventID(EventLevelError,  EvtRtxKernelNo, 0x00U)
#define EvtRtxKernelInitialize              EventID(EventLevelAPI,    EvtRtxKernelNo, 0x01U)
#define EvtRtxKernelInitialized             EventID(EventLevelOp,     EvtRtxKernelNo, 0x02U)
#define EvtRtxKernelGetInfo                 EventID(EventLevelAPI,    EvtRtxKernelNo, 0x03U)
#define EvtRtxKernelInfoRetrieved           EventID(EventLevelOp,     EvtRtxKernelNo, 0x04U)
#define EvtRtxKernelInfoRetrieved_Detail    EventID(EventLevelDetail, EvtRtxKernelNo, 0x05U)
#define EvtRtxKernelGetState                EventID(EventLevelAPI,    EvtRtxKernelNo, 0x06U)
#define EvtRtxKernelStart                   EventID(EventLevelAPI,    EvtRtxKernelNo, 0x07U)
#define EvtRtxKernelStarted                 EventID(EventLevelOp,     EvtRtxKernelNo, 0x08U)
#define EvtRtxKernelLock                    EventID(EventLevelAPI,    EvtRtxKernelNo, 0x09U)
#define EvtRtxKernelLocked                  EventID(EventLevelOp,     EvtRtxKernelNo, 0x0AU)
#define EvtRtxKernelUnlock                  EventID(EventLevelAPI,    EvtRtxKernelNo, 0x0BU)
#define EvtRtxKernelUnlocked                EventID(EventLevelOp,     EvtRtxKernelNo, 0x0CU)
#define EvtRtxKernelRestoreLock             EventID(EventLevelAPI,    EvtRtxKernelNo, 0x0DU)
#define EvtRtxKernelLockRestored            EventID(EventLevelOp,     EvtRtxKernelNo, 0x0EU)
#define EvtRtxKernelSuspend                 EventID(EventLevelAPI,    EvtRtxKernelNo, 0x0FU)
#define EvtRtxKernelSuspended               EventID(EventLevelOp,     EvtRtxKernelNo, 0x10U)
#define EvtRtxKernelResume                  EventID(EventLevelAPI,    EvtRtxKernelNo, 0x11U)
#define EvtRtxKernelResumed                 EventID(EventLevelOp,     EvtRtxKernelNo, 0x12U)
#define EvtRtxKernelGetTickCount            EventID(EventLevelAPI,    EvtRtxKernelNo, 0x13U)
#define EvtRtxKernelGetTickFreq             EventID(EventLevelAPI,    EvtRtxKernelNo, 0x14U)
#define EvtRtxKernelGetSysTimerCount        EventID(EventLevelAPI,    EvtRtxKernelNo, 0x15U)
#define EvtRtxKernelGetSysTimerFreq         EventID(EventLevelAPI,    EvtRtxKernelNo, 0x16U)

/// Event IDs for "RTX Thread"
#define EvtRtxThreadError                   EventID(EventLevelError,  EvtRtxThreadNo, 0x00U)
#define EvtRtxThreadNew                     EventID(EventLevelAPI,    EvtRtxThreadNo, 0x01U)
#define EvtRtxThreadCreated_Addr            EventID(EventLevelOp,     EvtRtxThreadNo, 0x03U)
#define EvtRtxThreadCreated_Name            EventID(EventLevelOp,     EvtRtxThreadNo, 0x2CU)
#define EvtRtxThreadGetName                 EventID(EventLevelAPI,    EvtRtxThreadNo, 0x04U)
#define EvtRtxThreadGetId                   EventID(EventLevelAPI,    EvtRtxThreadNo, 0x06U)
#define EvtRtxThreadGetState                EventID(EventLevelAPI,    EvtRtxThreadNo, 0x07U)
#define EvtRtxThreadGetStackSize            EventID(EventLevelAPI,    EvtRtxThreadNo, 0x08U)
#define EvtRtxThreadGetStackSpace           EventID(EventLevelAPI,    EvtRtxThreadNo, 0x09U)
#define EvtRtxThreadSetPriority             EventID(EventLevelAPI,    EvtRtxThreadNo, 0x0AU)
#define EvtRtxThreadPriorityUpdated         EventID(EventLevelOp,     EvtRtxThreadNo, 0x2DU)
#define EvtRtxThreadGetPriority             EventID(EventLevelAPI,    EvtRtxThreadNo, 0x0BU)
#define EvtRtxThreadYield                   EventID(EventLevelAPI,    EvtRtxThreadNo, 0x0CU)
#define EvtRtxThreadSuspend                 EventID(EventLevelAPI,    EvtRtxThreadNo, 0x0DU)
#define EvtRtxThreadSuspended               EventID(EventLevelOp,     EvtRtxThreadNo, 0x0EU)
#define EvtRtxThreadResume                  EventID(EventLevelAPI,    EvtRtxThreadNo, 0x0FU)
#define EvtRtxThreadResumed                 EventID(EventLevelOp,     EvtRtxThreadNo, 0x10U)
#define EvtRtxThreadDetach                  EventID(EventLevelAPI,    EvtRtxThreadNo, 0x11U)
#define EvtRtxThreadDetached                EventID(EventLevelOp,     EvtRtxThreadNo, 0x12U)
#define EvtRtxThreadJoin                    EventID(EventLevelAPI,    EvtRtxThreadNo, 0x13U)
#define EvtRtxThreadJoinPending             EventID(EventLevelOp,     EvtRtxThreadNo, 0x14U)
#define EvtRtxThreadJoined                  EventID(EventLevelOp,     EvtRtxThreadNo, 0x15U)
#define EvtRtxThreadBlocked                 EventID(EventLevelDetail, EvtRtxThreadNo, 0x16U)
#define EvtRtxThreadUnblocked               EventID(EventLevelDetail, EvtRtxThreadNo, 0x17U)
#define EvtRtxThreadPreempted               EventID(EventLevelDetail, EvtRtxThreadNo, 0x18U)
#define EvtRtxThreadSwitched                EventID(EventLevelOp,     EvtRtxThreadNo, 0x19U)
#define EvtRtxThreadExit                    EventID(EventLevelAPI,    EvtRtxThreadNo, 0x1AU)
#define EvtRtxThreadTerminate               EventID(EventLevelAPI,    EvtRtxThreadNo, 0x1BU)
#define EvtRtxThreadDestroyed               EventID(EventLevelOp,     EvtRtxThreadNo, 0x1CU)
#define EvtRtxThreadGetCount                EventID(EventLevelAPI,    EvtRtxThreadNo, 0x1DU)
#define EvtRtxThreadEnumerate               EventID(EventLevelAPI,    EvtRtxThreadNo, 0x1EU)

/// Event IDs for "RTX Thread Flags"
#define EvtRtxThreadFlagsError              EventID(EventLevelError,  EvtRtxThreadFlagsNo, 0x00U)
#define EvtRtxThreadFlagsSet                EventID(EventLevelAPI,    EvtRtxThreadFlagsNo, 0x01U)
#define EvtRtxThreadFlagsSetDone            EventID(EventLevelOp,     EvtRtxThreadFlagsNo, 0x02U)
#define EvtRtxThreadFlagsClear              EventID(EventLevelAPI,    EvtRtxThreadFlagsNo, 0x03U)
#define EvtRtxThreadFlagsClearDone          EventID(EventLevelOp,     EvtRtxThreadFlagsNo, 0x04U)
#define EvtRtxThreadFlagsGet                EventID(EventLevelAPI,    EvtRtxThreadFlagsNo, 0x05U)
#define EvtRtxThreadFlagsWait               EventID(EventLevelAPI,    EvtRtxThreadFlagsNo, 0x06U)
#define EvtRtxThreadFlagsWaitPending        EventID(EventLevelOp,     EvtRtxThreadFlagsNo, 0x07U)
#define EvtRtxThreadFlagsWaitTimeout        EventID(EventLevelOp,     EvtRtxThreadFlagsNo, 0x08U)
#define EvtRtxThreadFlagsWaitCompleted      EventID(EventLevelOp,     EvtRtxThreadFlagsNo, 0x09U)
#define EvtRtxThreadFlagsWaitNotCompleted   EventID(EventLevelOp,     EvtRtxThreadFlagsNo, 0x0AU)

/// Event IDs for "RTX Generic Wait"
#define EvtRtxDelayError                    EventID(EventLevelError,  EvtRtxWaitNo, 0x00U)
#define EvtRtxDelay                         EventID(EventLevelAPI,    EvtRtxWaitNo, 0x01U)
#define EvtRtxDelayUntil                    EventID(EventLevelAPI,    EvtRtxWaitNo, 0x02U)
#define EvtRtxDelayStarted                  EventID(EventLevelOp,     EvtRtxWaitNo, 0x03U)
#define EvtRtxDelayUntilStarted             EventID(EventLevelOp,     EvtRtxWaitNo, 0x04U)
#define EvtRtxDelayCompleted                EventID(EventLevelOp,     EvtRtxWaitNo, 0x05U)

/// Event IDs for "RTX Timer"
#define EvtRtxTimerError                    EventID(EventLevelError,  EvtRtxTimerNo, 0x00U)
#define EvtRtxTimerCallback                 EventID(EventLevelOp,     EvtRtxTimerNo, 0x01U)
#define EvtRtxTimerNew                      EventID(EventLevelAPI,    EvtRtxTimerNo, 0x02U)
#define EvtRtxTimerCreated                  EventID(EventLevelOp,     EvtRtxTimerNo, 0x04U)
#define EvtRtxTimerGetName                  EventID(EventLevelAPI,    EvtRtxTimerNo, 0x05U)
#define EvtRtxTimerStart                    EventID(EventLevelAPI,    EvtRtxTimerNo, 0x07U)
#define EvtRtxTimerStarted                  EventID(EventLevelOp,     EvtRtxTimerNo, 0x08U)
#define EvtRtxTimerStop                     EventID(EventLevelAPI,    EvtRtxTimerNo, 0x09U)
#define EvtRtxTimerStopped                  EventID(EventLevelOp,     EvtRtxTimerNo, 0x0AU)
#define EvtRtxTimerIsRunning                EventID(EventLevelAPI,    EvtRtxTimerNo, 0x0BU)
#define EvtRtxTimerDelete                   EventID(EventLevelAPI,    EvtRtxTimerNo, 0x0CU)
#define EvtRtxTimerDestroyed                EventID(EventLevelOp,     EvtRtxTimerNo, 0x0DU)

/// Event IDs for "RTX Event Flags"
#define EvtRtxEventFlagsError               EventID(EventLevelError,  EvtRtxEventFlagsNo, 0x00U)
#define EvtRtxEventFlagsNew                 EventID(EventLevelAPI,    EvtRtxEventFlagsNo, 0x01U)
#define EvtRtxEventFlagsCreated             EventID(EventLevelOp,     EvtRtxEventFlagsNo, 0x03U)
#define EvtRtxEventFlagsGetName             EventID(EventLevelAPI,    EvtRtxEventFlagsNo, 0x04U)
#define EvtRtxEventFlagsSet                 EventID(EventLevelAPI,    EvtRtxEventFlagsNo, 0x06U)
#define EvtRtxEventFlagsSetDone             EventID(EventLevelOp,     EvtRtxEventFlagsNo, 0x07U)
#define EvtRtxEventFlagsClear               EventID(EventLevelAPI,    EvtRtxEventFlagsNo, 0x08U)
#define EvtRtxEventFlagsClearDone           EventID(EventLevelOp,     EvtRtxEventFlagsNo, 0x09U)
#define EvtRtxEventFlagsGet                 EventID(EventLevelAPI,    EvtRtxEventFlagsNo, 0x0AU)
#define EvtRtxEventFlagsWait                EventID(EventLevelAPI,    EvtRtxEventFlagsNo, 0x0BU)
#define EvtRtxEventFlagsWaitPending         EventID(EventLevelOp,     EvtRtxEventFlagsNo, 0x0CU)
#define EvtRtxEventFlagsWaitTimeout         EventID(EventLevelOp,     EvtRtxEventFlagsNo, 0x0DU)
#define EvtRtxEventFlagsWaitCompleted       EventID(EventLevelOp,     EvtRtxEventFlagsNo, 0x0EU)
#define EvtRtxEventFlagsWaitNotCompleted    EventID(EventLevelOp,     EvtRtxEventFlagsNo, 0x0FU)
#define EvtRtxEventFlagsDelete              EventID(EventLevelAPI,    EvtRtxEventFlagsNo, 0x10U)
#define EvtRtxEventFlagsDestroyed           EventID(EventLevelOp,     EvtRtxEventFlagsNo, 0x11U)

/// Event IDs for "RTX Mutex"
#define EvtRtxMutexError                    EventID(EventLevelError,  EvtRtxMutexNo, 0x00U)
#define EvtRtxMutexNew                      EventID(EventLevelAPI,    EvtRtxMutexNo, 0x01U)
#define EvtRtxMutexCreated                  EventID(EventLevelOp,     EvtRtxMutexNo, 0x03U)
#define EvtRtxMutexGetName                  EventID(EventLevelAPI,    EvtRtxMutexNo, 0x04U)
#define EvtRtxMutexAcquire                  EventID(EventLevelAPI,    EvtRtxMutexNo, 0x06U)
#define EvtRtxMutexAcquirePending           EventID(EventLevelOp,     EvtRtxMutexNo, 0x07U)
#define EvtRtxMutexAcquireTimeout           EventID(EventLevelOp,     EvtRtxMutexNo, 0x08U)
#define EvtRtxMutexAcquired                 EventID(EventLevelOp,     EvtRtxMutexNo, 0x09U)
#define EvtRtxMutexNotAcquired              EventID(EventLevelOp,     EvtRtxMutexNo, 0x0AU)
#define EvtRtxMutexRelease                  EventID(EventLevelAPI,    EvtRtxMutexNo, 0x0BU)
#define EvtRtxMutexReleased                 EventID(EventLevelOp,     EvtRtxMutexNo, 0x0CU)
#define EvtRtxMutexGetOwner                 EventID(EventLevelAPI,    EvtRtxMutexNo, 0x0DU)
#define EvtRtxMutexDelete                   EventID(EventLevelAPI,    EvtRtxMutexNo, 0x0EU)
#define EvtRtxMutexDestroyed                EventID(EventLevelOp,     EvtRtxMutexNo, 0x0FU)

/// Event IDs for "RTX Semaphore"
#define EvtRtxSemaphoreError                EventID(EventLevelError,  EvtRtxSemaphoreNo, 0x00U)
#define EvtRtxSemaphoreNew                  EventID(EventLevelAPI,    EvtRtxSemaphoreNo, 0x01U)
#define EvtRtxSemaphoreCreated              EventID(EventLevelOp,     EvtRtxSemaphoreNo, 0x03U)
#define EvtRtxSemaphoreGetName              EventID(EventLevelAPI,    EvtRtxSemaphoreNo, 0x04U)
#define EvtRtxSemaphoreAcquire              EventID(EventLevelAPI,    EvtRtxSemaphoreNo, 0x06U)
#define EvtRtxSemaphoreAcquirePending       EventID(EventLevelOp,     EvtRtxSemaphoreNo, 0x07U)
#define EvtRtxSemaphoreAcquireTimeout       EventID(EventLevelOp,     EvtRtxSemaphoreNo, 0x08U)
#define EvtRtxSemaphoreAcquired             EventID(EventLevelOp,     EvtRtxSemaphoreNo, 0x09U)
#define EvtRtxSemaphoreNotAcquired          EventID(EventLevelOp,     EvtRtxSemaphoreNo, 0x0AU)
#define EvtRtxSemaphoreRelease              EventID(EventLevelAPI,    EvtRtxSemaphoreNo, 0x0BU)
#define EvtRtxSemaphoreReleased             EventID(EventLevelOp,     EvtRtxSemaphoreNo, 0x0CU)
#define EvtRtxSemaphoreGetCount             EventID(EventLevelAPI,    EvtRtxSemaphoreNo, 0x0DU)
#define EvtRtxSemaphoreDelete               EventID(EventLevelAPI,    EvtRtxSemaphoreNo, 0x0EU)
#define EvtRtxSemaphoreDestroyed            EventID(EventLevelOp,     EvtRtxSemaphoreNo, 0x0FU)

/// Event IDs for "RTX Memory Pool"
#define EvtRtxMemoryPoolError               EventID(EventLevelError,  EvtRtxMemoryPoolNo, 0x00U)
#define EvtRtxMemoryPoolNew                 EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x01U)
#define EvtRtxMemoryPoolCreated             EventID(EventLevelOp,     EvtRtxMemoryPoolNo, 0x03U)
#define EvtRtxMemoryPoolGetName             EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x04U)
#define EvtRtxMemoryPoolAlloc               EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x06U)
#define EvtRtxMemoryPoolAllocPending        EventID(EventLevelOp,     EvtRtxMemoryPoolNo, 0x07U)
#define EvtRtxMemoryPoolAllocTimeout        EventID(EventLevelOp,     EvtRtxMemoryPoolNo, 0x08U)
#define EvtRtxMemoryPoolAllocated           EventID(EventLevelOp,     EvtRtxMemoryPoolNo, 0x09U)
#define EvtRtxMemoryPoolAllocFailed         EventID(EventLevelOp,     EvtRtxMemoryPoolNo, 0x0AU)
#define EvtRtxMemoryPoolFree                EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x0BU)
#define EvtRtxMemoryPoolDeallocated         EventID(EventLevelOp,     EvtRtxMemoryPoolNo, 0x0CU)
#define EvtRtxMemoryPoolFreeFailed          EventID(EventLevelOp,     EvtRtxMemoryPoolNo, 0x0DU)
#define EvtRtxMemoryPoolGetCapacity         EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x0EU)
#define EvtRtxMemoryPoolGetBlockSize        EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x0FU)
#define EvtRtxMemoryPoolGetCount            EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x10U)
#define EvtRtxMemoryPoolGetSpace            EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x11U)
#define EvtRtxMemoryPoolDelete              EventID(EventLevelAPI,    EvtRtxMemoryPoolNo, 0x12U)
#define EvtRtxMemoryPoolDestroyed           EventID(EventLevelOp,     EvtRtxMemoryPoolNo, 0x13U)

/// Event IDs for "RTX Message Queue"
#define EvtRtxMessageQueueError             EventID(EventLevelError,  EvtRtxMessageQueueNo, 0x00U)
#define EvtRtxMessageQueueNew               EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x01U)
#define EvtRtxMessageQueueCreated           EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x03U)
#define EvtRtxMessageQueueGetName           EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x04U)
#define EvtRtxMessageQueuePut               EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x06U)
#define EvtRtxMessageQueuePutPending        EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x07U)
#define EvtRtxMessageQueuePutTimeout        EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x08U)
#define EvtRtxMessageQueueInsertPending     EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x09U)
#define EvtRtxMessageQueueInserted          EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x0AU)
#define EvtRtxMessageQueueNotInserted       EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x0BU)
#define EvtRtxMessageQueueGet               EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x0CU)
#define EvtRtxMessageQueueGetPending        EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x0DU)
#define EvtRtxMessageQueueGetTimeout        EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x0EU)
#define EvtRtxMessageQueueRetrieved         EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x0FU)
#define EvtRtxMessageQueueNotRetrieved      EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x10U)
#define EvtRtxMessageQueueGetCapacity       EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x11U)
#define EvtRtxMessageQueueGetMsgSize        EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x12U)
#define EvtRtxMessageQueueGetCount          EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x13U)
#define EvtRtxMessageQueueGetSpace          EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x14U)
#define EvtRtxMessageQueueReset             EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x15U)
#define EvtRtxMessageQueueResetDone         EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x16U)
#define EvtRtxMessageQueueDelete            EventID(EventLevelAPI,    EvtRtxMessageQueueNo, 0x17U)
#define EvtRtxMessageQueueDestroyed         EventID(EventLevelOp,     EvtRtxMessageQueueNo, 0x18U)

#endif  // RTE_Compiler_EventRecorder

//lint -esym(522, EvrRtx*) "Functions 'EvrRtx*' can be overridden (do not lack side-effects)"


//  ==== Memory Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_INIT_DISABLE))
__WEAK void EvrRtxMemoryInit (void *mem, uint32_t size, uint32_t result) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMemoryInit, (uint32_t)mem, size, result, 0U);
#else
  (void)mem;
  (void)size;
  (void)result;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_ALLOC_DISABLE))
__WEAK void EvrRtxMemoryAlloc (void *mem, uint32_t size, uint32_t type, void *block) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMemoryAlloc, (uint32_t)mem, size, type, (uint32_t)block);
#else
  (void)mem;
  (void)size;
  (void)type;
  (void)block;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_FREE_DISABLE))
__WEAK void EvrRtxMemoryFree (void *mem, void *block, uint32_t result) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMemoryFree, (uint32_t)mem, (uint32_t)block, result, 0U);
#else
  (void)mem;
  (void)block;
  (void)result;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_BLOCK_INIT_DISABLE))
__WEAK void EvrRtxMemoryBlockInit (osRtxMpInfo_t *mp_info, uint32_t block_count, uint32_t block_size, void *block_mem) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMemoryBlockInit, (uint32_t)mp_info, block_count, block_size, (uint32_t)block_mem);
#else
  (void)mp_info;
  (void)block_count;
  (void)block_size;
  (void)block_mem;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_BLOCK_ALLOC_DISABLE))
__WEAK void EvrRtxMemoryBlockAlloc (osRtxMpInfo_t *mp_info, void *block) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryBlockAlloc, (uint32_t)mp_info, (uint32_t)block);
#else
  (void)mp_info;
  (void)block;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMORY != 0) && !defined(EVR_RTX_MEMORY_BLOCK_FREE_DISABLE))
__WEAK void EvrRtxMemoryBlockFree (osRtxMpInfo_t *mp_info, void *block, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMemoryBlockFree, (uint32_t)mp_info, (uint32_t)block, (uint32_t)status, 0U);
#else
  (void)mp_info;
  (void)block;
  (void)status;
#endif
}
#endif


//  ==== Kernel Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_ERROR_DISABLE))
__WEAK void EvrRtxKernelError (int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelError, (uint32_t)status, 0U); 
#else
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_INITIALIZE_DISABLE))
__WEAK void EvrRtxKernelInitialize (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelInitialize, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_INITIALIZED_DISABLE))
__WEAK void EvrRtxKernelInitialized (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelInitialized, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_INFO_DISABLE))
__WEAK void EvrRtxKernelGetInfo (osVersion_t *version, char *id_buf, uint32_t id_size) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxKernelGetInfo, (uint32_t)version, (uint32_t)id_buf, id_size, 0U);
#else
  (void)version;
  (void)id_buf;
  (void)id_size;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_INFO_RETRIEVED_DISABLE))
__WEAK void EvrRtxKernelInfoRetrieved (const osVersion_t *version, const char *id_buf, uint32_t id_size) {
#if defined(RTE_Compiler_EventRecorder)
  if (version != NULL) {
    (void)EventRecord2(EvtRtxKernelInfoRetrieved, version->api, version->kernel);
  }
  if (id_buf != NULL) {
    (void)EventRecordData(EvtRtxKernelInfoRetrieved_Detail, id_buf, id_size);
  }
#else
  (void)version;
  (void)id_buf;
  (void)id_size;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_STATE_DISABLE))
__WEAK void EvrRtxKernelGetState (osKernelState_t state) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelGetState, (uint32_t)state, 0U);
#else
  (void)state;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_START_DISABLE))
__WEAK void EvrRtxKernelStart (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelStart, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_STARTED_DISABLE))
__WEAK void EvrRtxKernelStarted (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelStarted, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_LOCK_DISABLE))
__WEAK void EvrRtxKernelLock (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelLock, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_LOCKED_DISABLE))
__WEAK void EvrRtxKernelLocked (int32_t lock) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelLocked, (uint32_t)lock, 0U);
#else
  (void)lock;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_UNLOCK_DISABLE))
__WEAK void EvrRtxKernelUnlock (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelUnlock, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_UNLOCKED_DISABLE))
__WEAK void EvrRtxKernelUnlocked (int32_t lock) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelUnlocked, (uint32_t)lock, 0U);
#else
  (void)lock;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_RESTORE_LOCK_DISABLE))
__WEAK void EvrRtxKernelRestoreLock (int32_t lock) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelRestoreLock, (uint32_t)lock, 0U);
#else
  (void)lock;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_LOCK_RESTORED_DISABLE))
__WEAK void EvrRtxKernelLockRestored (int32_t lock) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelLockRestored, (uint32_t)lock, 0U);
#else
  (void)lock;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_SUSPEND_DISABLE))
__WEAK void EvrRtxKernelSuspend (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelSuspend, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_SUSPENDED_DISABLE))
__WEAK void EvrRtxKernelSuspended (uint32_t sleep_ticks) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelSuspended, sleep_ticks, 0U);
#else
  (void)sleep_ticks;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_RESUME_DISABLE))
__WEAK void EvrRtxKernelResume (uint32_t sleep_ticks) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelResume, sleep_ticks, 0U);
#else
  (void)sleep_ticks;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_RESUMED_DISABLE))
__WEAK void EvrRtxKernelResumed (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelResumed, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_TICK_COUNT_DISABLE))
__WEAK void EvrRtxKernelGetTickCount (uint32_t count) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelGetTickCount, count, 0U);
#else
  (void)count;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_TICK_FREQ_DISABLE))
__WEAK void EvrRtxKernelGetTickFreq (uint32_t freq) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelGetTickFreq, freq, 0U);
#else
  (void)freq;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_SYS_TIMER_COUNT_DISABLE))
__WEAK void EvrRtxKernelGetSysTimerCount (uint32_t count) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelGetSysTimerCount, count, 0U);
#else
  (void)count;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_KERNEL != 0) && !defined(EVR_RTX_KERNEL_GET_SYS_TIMER_FREQ_DISABLE))
__WEAK void EvrRtxKernelGetSysTimerFreq (uint32_t freq) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxKernelGetSysTimerFreq, freq, 0U);
#else
  (void)freq;
#endif
}
#endif


//  ==== Thread Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_ERROR_DISABLE))
__WEAK void EvrRtxThreadError (osThreadId_t thread_id, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadError, (uint32_t)thread_id, (uint32_t)status);
#else
  (void)thread_id;
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_NEW_DISABLE))
__WEAK void EvrRtxThreadNew (osThreadFunc_t func, void *argument, const osThreadAttr_t *attr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxThreadNew, (uint32_t)func, (uint32_t)argument, (uint32_t)attr, 0U);
#else
  (void)func;
  (void)argument;
  (void)attr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_CREATED_DISABLE))
__WEAK void EvrRtxThreadCreated (osThreadId_t thread_id, uint32_t thread_addr, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  if (name != NULL) {
    (void)EventRecord2(EvtRtxThreadCreated_Name, (uint32_t)thread_id, (uint32_t)name);
  } else {
    (void)EventRecord2(EvtRtxThreadCreated_Addr, (uint32_t)thread_id, thread_addr);
  }
#else
  (void)thread_id;
  (void)thread_addr;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_NAME_DISABLE))
__WEAK void EvrRtxThreadGetName (osThreadId_t thread_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadGetName, (uint32_t)thread_id, (uint32_t)name);
#else
  (void)thread_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_ID_DISABLE))
__WEAK void EvrRtxThreadGetId (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadGetId, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_STATE_DISABLE))
__WEAK void EvrRtxThreadGetState (osThreadId_t thread_id, osThreadState_t state) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadGetState, (uint32_t)thread_id, (uint32_t)state);
#else
  (void)thread_id;
  (void)state;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_STACK_SIZE_DISABLE))
__WEAK void EvrRtxThreadGetStackSize (osThreadId_t thread_id, uint32_t stack_size) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadGetStackSize, (uint32_t)thread_id, stack_size);
#else
  (void)thread_id;
  (void)stack_size;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_STACK_SPACE_DISABLE))
__WEAK void EvrRtxThreadGetStackSpace (osThreadId_t thread_id, uint32_t stack_space) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadGetStackSpace, (uint32_t)thread_id, stack_space);
#else
  (void)thread_id;
  (void)stack_space;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_SET_PRIORITY_DISABLE))
__WEAK void EvrRtxThreadSetPriority (osThreadId_t thread_id, osPriority_t priority) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadSetPriority, (uint32_t)thread_id, (uint32_t)priority);
#else
  (void)thread_id;
  (void)priority;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_PRIORITY_UPDATED_DISABLE))
__WEAK void EvrRtxThreadPriorityUpdated (osThreadId_t thread_id, osPriority_t priority) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadPriorityUpdated, (uint32_t)thread_id, (uint32_t)priority);
#else
  (void)thread_id;
  (void)priority;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_PRIORITY_DISABLE))
__WEAK void EvrRtxThreadGetPriority (osThreadId_t thread_id, osPriority_t priority) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadGetPriority, (uint32_t)thread_id, (uint32_t)priority);
#else
  (void)thread_id;
  (void)priority;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_YIELD_DISABLE))
__WEAK void EvrRtxThreadYield (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadYield, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_SUSPEND_DISABLE))
__WEAK void EvrRtxThreadSuspend (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadSuspend, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_SUSPENDED_DISABLE))
__WEAK void EvrRtxThreadSuspended (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadSuspended, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_RESUME_DISABLE))
__WEAK void EvrRtxThreadResume (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadResume, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_RESUMED_DISABLE))
__WEAK void EvrRtxThreadResumed (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadResumed, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_DETACH_DISABLE))
__WEAK void EvrRtxThreadDetach (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadDetach, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_DETACHED_DISABLE))
__WEAK void EvrRtxThreadDetached (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadDetached, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_JOIN_DISABLE))
__WEAK void EvrRtxThreadJoin (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadJoin, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_JOIN_PENDING_DISABLE))
__WEAK void EvrRtxThreadJoinPending (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadJoinPending, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_JOINED_DISABLE))
__WEAK void EvrRtxThreadJoined (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadJoined, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_BLOCKED_DISABLE))
__WEAK void EvrRtxThreadBlocked (osThreadId_t thread_id, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadBlocked, (uint32_t)thread_id, timeout);
#else
  (void)thread_id;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_UNBLOCKED_DISABLE))
__WEAK void EvrRtxThreadUnblocked (osThreadId_t thread_id, uint32_t ret_val) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadUnblocked, (uint32_t)thread_id, ret_val);
#else
  (void)thread_id;
  (void)ret_val;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_PREEMPTED_DISABLE))
__WEAK void EvrRtxThreadPreempted (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadPreempted, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_SWITCHED_DISABLE))
__WEAK void EvrRtxThreadSwitched (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadSwitched, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_EXIT_DISABLE))
__WEAK void EvrRtxThreadExit (void) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadExit, 0U, 0U);
#else
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_TERMINATE_DISABLE))
__WEAK void EvrRtxThreadTerminate (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadTerminate, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_DESTROYED_DISABLE))
__WEAK void EvrRtxThreadDestroyed (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadDestroyed, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_GET_COUNT_DISABLE))
__WEAK void EvrRtxThreadGetCount (uint32_t count) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadGetCount, count, 0U);
#else
  (void)count;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THREAD != 0) && !defined(EVR_RTX_THREAD_ENUMERATE_DISABLE))
__WEAK void EvrRtxThreadEnumerate (osThreadId_t *thread_array, uint32_t array_items, uint32_t count) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxThreadEnumerate, (uint32_t)thread_array, array_items, count, 0U);
#else
  (void)thread_array;
  (void)array_items;
  (void)count;
#endif
}
#endif


//  ==== Thread Flags Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_ERROR_DISABLE))
__WEAK void EvrRtxThreadFlagsError (osThreadId_t thread_id, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadFlagsError, (uint32_t)thread_id, (uint32_t)status);
#else
  (void)thread_id;
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_SET_DISABLE))
__WEAK void EvrRtxThreadFlagsSet (osThreadId_t thread_id, uint32_t flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadFlagsSet, (uint32_t)thread_id, flags);
#else
  (void)thread_id;
  (void)flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_SET_DONE_DISABLE))
__WEAK void EvrRtxThreadFlagsSetDone (osThreadId_t thread_id, uint32_t thread_flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadFlagsSetDone, (uint32_t)thread_id, thread_flags);
#else
  (void)thread_id;
  (void)thread_flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_CLEAR_DISABLE))
__WEAK void EvrRtxThreadFlagsClear (uint32_t flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadFlagsClear, flags, 0U);
#else
  (void)flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_CLEAR_DONE_DISABLE))
__WEAK void EvrRtxThreadFlagsClearDone (uint32_t thread_flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadFlagsClearDone, thread_flags, 0U);
#else
  (void)thread_flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_GET_DISABLE))
__WEAK void EvrRtxThreadFlagsGet (uint32_t thread_flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadFlagsGet, thread_flags, 0U);
#else
  (void)thread_flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_DISABLE))
__WEAK void EvrRtxThreadFlagsWait (uint32_t flags, uint32_t options, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxThreadFlagsWait, flags, options, timeout, 0U);
#else
  (void)flags;
  (void)options;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_PENDING_DISABLE))
__WEAK void EvrRtxThreadFlagsWaitPending (uint32_t flags, uint32_t options, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxThreadFlagsWaitPending, flags, options, timeout, 0U);
#else
  (void)flags;
  (void)options;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_TIMEOUT_DISABLE))
__WEAK void EvrRtxThreadFlagsWaitTimeout (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadFlagsWaitTimeout, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_COMPLETED_DISABLE))
__WEAK void EvrRtxThreadFlagsWaitCompleted (uint32_t flags, uint32_t options, uint32_t thread_flags, osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxThreadFlagsWaitCompleted, flags, options, thread_flags, (uint32_t)thread_id);
#else
  (void)flags;
  (void)options;
  (void)thread_flags;
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_THFLAGS != 0) && !defined(EVR_RTX_THREAD_FLAGS_WAIT_NOT_COMPLETED_DISABLE))
__WEAK void EvrRtxThreadFlagsWaitNotCompleted (uint32_t flags, uint32_t options) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxThreadFlagsWaitNotCompleted, flags, options);
#else
  (void)flags;
  (void)options;
#endif
}
#endif


//  ==== Generic Wait Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_ERROR_DISABLE))
__WEAK void EvrRtxDelayError (int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxDelayError, (uint32_t)status, 0U);
#else
  (void)status;
#endif
}
#endif


#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_DISABLE))
__WEAK void EvrRtxDelay (uint32_t ticks) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxDelay, ticks, 0U);
#else
  (void)ticks;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_UNTIL_DISABLE))
__WEAK void EvrRtxDelayUntil (uint32_t ticks) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxDelayUntil, ticks, 0U);
#else
  (void)ticks;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_STARTED_DISABLE))
__WEAK void EvrRtxDelayStarted (uint32_t ticks) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxDelayStarted, ticks, 0U);
#else
  (void)ticks;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_UNTIL_STARTED_DISABLE))
__WEAK void EvrRtxDelayUntilStarted (uint32_t ticks) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxDelayUntilStarted, ticks, 0U);
#else
  (void)ticks;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_WAIT != 0) && !defined(EVR_RTX_DELAY_COMPLETED_DISABLE))
__WEAK void EvrRtxDelayCompleted (osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxDelayCompleted, (uint32_t)thread_id, 0U);
#else
  (void)thread_id;
#endif
}
#endif


//  ==== Timer Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_ERROR_DISABLE))
__WEAK void EvrRtxTimerError (osTimerId_t timer_id, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerError, (uint32_t)timer_id, (uint32_t)status);
#else
  (void)timer_id;
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_CALLBACK_DISABLE))
__WEAK void EvrRtxTimerCallback (osTimerFunc_t func, void *argument) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerCallback, (uint32_t)func, (uint32_t)argument);
#else
  (void)func;
  (void)argument;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_NEW_DISABLE))
__WEAK void EvrRtxTimerNew (osTimerFunc_t func, osTimerType_t type, void *argument, const osTimerAttr_t *attr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxTimerNew, (uint32_t)func, (uint32_t)type, (uint32_t)argument, (uint32_t)attr);
#else
  (void)func;
  (void)type;
  (void)argument;
  (void)attr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_CREATED_DISABLE))
__WEAK void EvrRtxTimerCreated (osTimerId_t timer_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerCreated, (uint32_t)timer_id, (uint32_t)name);
#else
  (void)timer_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_GET_NAME_DISABLE))
__WEAK void EvrRtxTimerGetName (osTimerId_t timer_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerGetName, (uint32_t)timer_id, (uint32_t)name);
#else
  (void)timer_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_START_DISABLE))
__WEAK void EvrRtxTimerStart (osTimerId_t timer_id, uint32_t ticks) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerStart, (uint32_t)timer_id, ticks);
#else
  (void)timer_id;
  (void)ticks;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_STARTED_DISABLE))
__WEAK void EvrRtxTimerStarted (osTimerId_t timer_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerStarted, (uint32_t)timer_id, 0U);
#else
  (void)timer_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_STOP_DISABLE))
__WEAK void EvrRtxTimerStop (osTimerId_t timer_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerStop, (uint32_t)timer_id, 0U);
#else
  (void)timer_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_STOPPED_DISABLE))
__WEAK void EvrRtxTimerStopped (osTimerId_t timer_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerStopped, (uint32_t)timer_id, 0U);
#else
  (void)timer_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_IS_RUNNING_DISABLE))
__WEAK void EvrRtxTimerIsRunning (osTimerId_t timer_id, uint32_t running) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerIsRunning, (uint32_t)timer_id, running);
#else
  (void)timer_id;
  (void)running;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_DELETE_DISABLE))
__WEAK void EvrRtxTimerDelete (osTimerId_t timer_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerDelete, (uint32_t)timer_id, 0U);
#else
  (void)timer_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_TIMER != 0) && !defined(EVR_RTX_TIMER_DESTROYED_DISABLE))
__WEAK void EvrRtxTimerDestroyed (osTimerId_t timer_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxTimerDestroyed, (uint32_t)timer_id, 0U);
#else
  (void)timer_id;
#endif
}
#endif


//  ==== Event Flags Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_ERROR_DISABLE))
__WEAK void EvrRtxEventFlagsError (osEventFlagsId_t ef_id, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsError, (uint32_t)ef_id, (uint32_t)status);
#else
  (void)ef_id;
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_NEW_DISABLE))
__WEAK void EvrRtxEventFlagsNew (const osEventFlagsAttr_t *attr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsNew, (uint32_t)attr, 0U);
#else
  (void)attr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_CREATED_DISABLE))
__WEAK void EvrRtxEventFlagsCreated (osEventFlagsId_t ef_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsCreated, (uint32_t)ef_id, (uint32_t)name);
#else
  (void)ef_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_GET_NAME_DISABLE))
__WEAK void EvrRtxEventFlagsGetName (osEventFlagsId_t ef_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsGetName, (uint32_t)ef_id, (uint32_t)name);
#else
  (void)ef_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_SET_DISABLE))
__WEAK void EvrRtxEventFlagsSet (osEventFlagsId_t ef_id, uint32_t flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsSet, (uint32_t)ef_id, flags);
#else
  (void)ef_id;
  (void)flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_SET_DONE_DISABLE))
__WEAK void EvrRtxEventFlagsSetDone (osEventFlagsId_t ef_id, uint32_t event_flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsSetDone, (uint32_t)ef_id, event_flags);
#else
  (void)ef_id;
  (void)event_flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_CLEAR_DISABLE))
__WEAK void EvrRtxEventFlagsClear (osEventFlagsId_t ef_id, uint32_t flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsClear, (uint32_t)ef_id, flags);
#else
  (void)ef_id;
  (void)flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_CLEAR_DONE_DISABLE))
__WEAK void EvrRtxEventFlagsClearDone (osEventFlagsId_t ef_id, uint32_t event_flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsClearDone, (uint32_t)ef_id, event_flags);
#else
  (void)ef_id;
  (void)event_flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_GET_DISABLE))
__WEAK void EvrRtxEventFlagsGet (osEventFlagsId_t ef_id, uint32_t event_flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsGet, (uint32_t)ef_id, event_flags);
#else
  (void)ef_id;
  (void)event_flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_DISABLE))
__WEAK void EvrRtxEventFlagsWait (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxEventFlagsWait, (uint32_t)ef_id, flags, options, timeout);
#else
  (void)ef_id;
  (void)flags;
  (void)options;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_PENDING_DISABLE))
__WEAK void EvrRtxEventFlagsWaitPending (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxEventFlagsWaitPending, (uint32_t)ef_id, flags, options, timeout);
#else
  (void)ef_id;
  (void)flags;
  (void)options;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_TIMEOUT_DISABLE))
__WEAK void EvrRtxEventFlagsWaitTimeout (osEventFlagsId_t ef_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsWaitTimeout, (uint32_t)ef_id, 0U);
#else
  (void)ef_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_COMPLETED_DISABLE))
__WEAK void EvrRtxEventFlagsWaitCompleted (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t event_flags) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxEventFlagsWaitCompleted, (uint32_t)ef_id, flags, options, event_flags);
#else
  (void)ef_id;
  (void)flags;
  (void)options;
  (void)event_flags;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_WAIT_NOT_COMPLETED_DISABLE))
__WEAK void EvrRtxEventFlagsWaitNotCompleted (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxEventFlagsWaitNotCompleted, (uint32_t)ef_id, flags, options, 0U);
#else
  (void)ef_id;
  (void)flags;
  (void)options;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_DELETE_DISABLE))
__WEAK void EvrRtxEventFlagsDelete (osEventFlagsId_t ef_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsDelete, (uint32_t)ef_id, 0U);
#else
  (void)ef_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_EVFLAGS != 0) && !defined(EVR_RTX_EVENT_FLAGS_DESTROYED_DISABLE))
__WEAK void EvrRtxEventFlagsDestroyed (osEventFlagsId_t ef_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxEventFlagsDestroyed, (uint32_t)ef_id, 0U);
#else
  (void)ef_id;
#endif
}
#endif


//  ==== Mutex Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ERROR_DISABLE))
__WEAK void EvrRtxMutexError (osMutexId_t mutex_id, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexError, (uint32_t)mutex_id, (uint32_t)status);
#else
  (void)mutex_id;
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_NEW_DISABLE))
__WEAK void EvrRtxMutexNew (const osMutexAttr_t *attr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexNew, (uint32_t)attr, 0U);
#else
  (void)attr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_CREATED_DISABLE))
__WEAK void EvrRtxMutexCreated (osMutexId_t mutex_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexCreated, (uint32_t)mutex_id, (uint32_t)name);
#else
  (void)mutex_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_GET_NAME_DISABLE))
__WEAK void EvrRtxMutexGetName (osMutexId_t mutex_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexGetName, (uint32_t)mutex_id, (uint32_t)name);
#else
  (void)mutex_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ACQUIRE_DISABLE))
__WEAK void EvrRtxMutexAcquire (osMutexId_t mutex_id, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexAcquire, (uint32_t)mutex_id, timeout);
#else
  (void)mutex_id;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ACQUIRE_PENDING_DISABLE))
__WEAK void EvrRtxMutexAcquirePending (osMutexId_t mutex_id, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexAcquirePending, (uint32_t)mutex_id, timeout);
#else
  (void)mutex_id;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ACQUIRE_TIMEOUT_DISABLE))
__WEAK void EvrRtxMutexAcquireTimeout (osMutexId_t mutex_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexAcquireTimeout, (uint32_t)mutex_id, 0U);
#else
  (void)mutex_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_ACQUIRED_DISABLE))
__WEAK void EvrRtxMutexAcquired (osMutexId_t mutex_id, uint32_t lock) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexAcquired, (uint32_t)mutex_id, lock);
#else
  (void)mutex_id;
  (void)lock;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_NOT_ACQUIRED_DISABLE))
__WEAK void EvrRtxMutexNotAcquired (osMutexId_t mutex_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexNotAcquired, (uint32_t)mutex_id, 0U);
#else
  (void)mutex_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_RELEASE_DISABLE))
__WEAK void EvrRtxMutexRelease (osMutexId_t mutex_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexRelease, (uint32_t)mutex_id, 0U);
#else
  (void)mutex_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_RELEASED_DISABLE))
__WEAK void EvrRtxMutexReleased (osMutexId_t mutex_id, uint32_t lock) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexReleased, (uint32_t)mutex_id, lock);
#else
  (void)mutex_id;
  (void)lock;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_GET_OWNER_DISABLE))
__WEAK void EvrRtxMutexGetOwner (osMutexId_t mutex_id, osThreadId_t thread_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexGetOwner, (uint32_t)mutex_id, (uint32_t)thread_id);
#else
  (void)mutex_id;
  (void)thread_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_DELETE_DISABLE))
__WEAK void EvrRtxMutexDelete (osMutexId_t mutex_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexDelete, (uint32_t)mutex_id, 0U);
#else
  (void)mutex_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MUTEX != 0) && !defined(EVR_RTX_MUTEX_DESTROYED_DISABLE))
__WEAK void EvrRtxMutexDestroyed (osMutexId_t mutex_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMutexDestroyed, (uint32_t)mutex_id, 0U);
#else
  (void)mutex_id;
#endif
}
#endif


//  ==== Semaphore Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ERROR_DISABLE))
__WEAK void EvrRtxSemaphoreError (osSemaphoreId_t semaphore_id, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreError, (uint32_t)semaphore_id, (uint32_t)status);
#else
  (void)semaphore_id;
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_NEW_DISABLE))
__WEAK void EvrRtxSemaphoreNew (uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxSemaphoreNew, max_count, initial_count, (uint32_t)attr, 0U);
#else
  (void)max_count;
  (void)initial_count;
  (void)attr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_CREATED_DISABLE))
__WEAK void EvrRtxSemaphoreCreated (osSemaphoreId_t semaphore_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreCreated, (uint32_t)semaphore_id, (uint32_t)name);
#else
  (void)semaphore_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_GET_NAME_DISABLE))
__WEAK void EvrRtxSemaphoreGetName (osSemaphoreId_t semaphore_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreGetName, (uint32_t)semaphore_id, (uint32_t)name);
#else
#endif
  (void)semaphore_id;
  (void)name;
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ACQUIRE_DISABLE))
__WEAK void EvrRtxSemaphoreAcquire (osSemaphoreId_t semaphore_id, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreAcquire, (uint32_t)semaphore_id, timeout);
#else
  (void)semaphore_id;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ACQUIRE_PENDING_DISABLE))
__WEAK void EvrRtxSemaphoreAcquirePending (osSemaphoreId_t semaphore_id, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreAcquirePending, (uint32_t)semaphore_id, (uint32_t)timeout);
#else
  (void)semaphore_id;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ACQUIRE_TIMEOUT_DISABLE))
__WEAK void EvrRtxSemaphoreAcquireTimeout (osSemaphoreId_t semaphore_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreAcquireTimeout, (uint32_t)semaphore_id, 0U);
#else
  (void)semaphore_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_ACQUIRED_DISABLE))
__WEAK void EvrRtxSemaphoreAcquired (osSemaphoreId_t semaphore_id, uint32_t tokens) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreAcquired, (uint32_t)semaphore_id, tokens);
#else
  (void)semaphore_id;
  (void)tokens;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_NOT_ACQUIRED_DISABLE))
__WEAK void EvrRtxSemaphoreNotAcquired (osSemaphoreId_t semaphore_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreNotAcquired, (uint32_t)semaphore_id, 0U);
#else
  (void)semaphore_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_RELEASE_DISABLE))
__WEAK void EvrRtxSemaphoreRelease (osSemaphoreId_t semaphore_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreRelease, (uint32_t)semaphore_id, 0U);
#else
  (void)semaphore_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_RELEASED_DISABLE))
__WEAK void EvrRtxSemaphoreReleased (osSemaphoreId_t semaphore_id, uint32_t tokens) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreReleased, (uint32_t)semaphore_id, tokens);
#else
  (void)semaphore_id;
  (void)tokens;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_GET_COUNT_DISABLE))
__WEAK void EvrRtxSemaphoreGetCount (osSemaphoreId_t semaphore_id, uint32_t count) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreGetCount, (uint32_t)semaphore_id, count);
#else
  (void)semaphore_id;
  (void)count;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_DELETE_DISABLE))
__WEAK void EvrRtxSemaphoreDelete (osSemaphoreId_t semaphore_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreDelete, (uint32_t)semaphore_id, 0U);
#else
  (void)semaphore_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_SEMAPHORE != 0) && !defined(EVR_RTX_SEMAPHORE_DESTROYED_DISABLE))
__WEAK void EvrRtxSemaphoreDestroyed (osSemaphoreId_t semaphore_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxSemaphoreDestroyed, (uint32_t)semaphore_id, 0U);
#else
  (void)semaphore_id;
#endif
}
#endif


//  ==== Memory Pool Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ERROR_DISABLE))
__WEAK void EvrRtxMemoryPoolError (osMemoryPoolId_t mp_id, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolError, (uint32_t)mp_id, (uint32_t)status);
#else
  (void)mp_id;
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_NEW_DISABLE))
__WEAK void EvrRtxMemoryPoolNew (uint32_t block_count, uint32_t block_size, const osMemoryPoolAttr_t *attr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMemoryPoolNew, block_count, block_size, (uint32_t)attr, 0U);
#else
  (void)block_count;
  (void)block_size;
  (void)attr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_CREATED_DISABLE))
__WEAK void EvrRtxMemoryPoolCreated (osMemoryPoolId_t mp_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolCreated, (uint32_t)mp_id, (uint32_t)name);
#else
  (void)mp_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_NAME_DISABLE))
__WEAK void EvrRtxMemoryPoolGetName (osMemoryPoolId_t mp_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolGetName, (uint32_t)mp_id, (uint32_t)name);
#else
  (void)mp_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOC_DISABLE))
__WEAK void EvrRtxMemoryPoolAlloc (osMemoryPoolId_t mp_id, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolAlloc, (uint32_t)mp_id, timeout);
#else
  (void)mp_id;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOC_PENDING_DISABLE))
__WEAK void EvrRtxMemoryPoolAllocPending (osMemoryPoolId_t mp_id, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolAllocPending, (uint32_t)mp_id, timeout);
#else
  (void)mp_id;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOC_TIMEOUT_DISABLE))
__WEAK void EvrRtxMemoryPoolAllocTimeout (osMemoryPoolId_t mp_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolAllocTimeout, (uint32_t)mp_id, 0U);
#else
  (void)mp_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOCATED_DISABLE))
__WEAK void EvrRtxMemoryPoolAllocated (osMemoryPoolId_t mp_id, void *block) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolAllocated, (uint32_t)mp_id, (uint32_t)block);
#else
  (void)mp_id;
  (void)block;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_ALLOC_FAILED_DISABLE))
__WEAK void EvrRtxMemoryPoolAllocFailed (osMemoryPoolId_t mp_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolAllocFailed, (uint32_t)mp_id, 0U);
#else
  (void)mp_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_FREE_DISABLE))
__WEAK void EvrRtxMemoryPoolFree (osMemoryPoolId_t mp_id, void *block) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolFree, (uint32_t)mp_id, (uint32_t)block);
#else
  (void)mp_id;
  (void)block;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_DEALLOCATED_DISABLE))
__WEAK void EvrRtxMemoryPoolDeallocated (osMemoryPoolId_t mp_id, void *block) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolDeallocated, (uint32_t)mp_id, (uint32_t)block);
#else
  (void)mp_id;
  (void)block;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_FREE_FAILED_DISABLE))
__WEAK void EvrRtxMemoryPoolFreeFailed (osMemoryPoolId_t mp_id, void *block) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolFreeFailed, (uint32_t)mp_id, (uint32_t)block);
#else
  (void)mp_id;
  (void)block;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_CAPACITY_DISABLE))
__WEAK void EvrRtxMemoryPoolGetCapacity (osMemoryPoolId_t mp_id, uint32_t capacity) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolGetCapacity, (uint32_t)mp_id, capacity);
#else
  (void)mp_id;
  (void)capacity;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_BLOCK_SZIE_DISABLE))
__WEAK void EvrRtxMemoryPoolGetBlockSize (osMemoryPoolId_t mp_id, uint32_t block_size) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolGetBlockSize, (uint32_t)mp_id, block_size);
#else
  (void)mp_id;
  (void)block_size;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_COUNT_DISABLE))
__WEAK void EvrRtxMemoryPoolGetCount (osMemoryPoolId_t mp_id, uint32_t count) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolGetCount, (uint32_t)mp_id, count);
#else
  (void)mp_id;
  (void)count;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_GET_SPACE_DISABLE))
__WEAK void EvrRtxMemoryPoolGetSpace (osMemoryPoolId_t mp_id, uint32_t space) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolGetSpace, (uint32_t)mp_id, space);
#else
  (void)mp_id;
  (void)space;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_DELETE_DISABLE))
__WEAK void EvrRtxMemoryPoolDelete (osMemoryPoolId_t mp_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolDelete, (uint32_t)mp_id, 0U);
#else
  (void)mp_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MEMPOOL != 0) && !defined(EVR_RTX_MEMORY_POOL_DESTROYED_DISABLE))
__WEAK void EvrRtxMemoryPoolDestroyed (osMemoryPoolId_t mp_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMemoryPoolDestroyed, (uint32_t)mp_id, 0U);
#else
  (void)mp_id;
#endif
}
#endif


//  ==== Message Queue Events ====

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_ERROR_DISABLE))
__WEAK void EvrRtxMessageQueueError (osMessageQueueId_t mq_id, int32_t status) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2 (EvtRtxMessageQueueError, (uint32_t)mq_id, (uint32_t)status);
#else
  (void)mq_id;
  (void)status;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_NEW_DISABLE))
__WEAK void EvrRtxMessageQueueNew (uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMessageQueueNew, msg_count, msg_size, (uint32_t)attr, 0U);
#else
  (void)msg_count;
  (void)msg_size;
  (void)attr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_CREATED_DISABLE))
__WEAK void EvrRtxMessageQueueCreated (osMessageQueueId_t mq_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueCreated, (uint32_t)mq_id, (uint32_t)name);
#else
  (void)mq_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_NAME_DISABLE))
__WEAK void EvrRtxMessageQueueGetName (osMessageQueueId_t mq_id, const char *name) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueGetName, (uint32_t)mq_id, (uint32_t)name);
#else
  (void)mq_id;
  (void)name;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_PUT_DISABLE))
__WEAK void EvrRtxMessageQueuePut (osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMessageQueuePut, (uint32_t)mq_id, (uint32_t)msg_ptr, (uint32_t)msg_prio, timeout);
#else
  (void)mq_id;
  (void)msg_ptr;
  (void)msg_prio;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_PUT_PENDING_DISABLE))
__WEAK void EvrRtxMessageQueuePutPending (osMessageQueueId_t mq_id, const void *msg_ptr, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMessageQueuePutPending, (uint32_t)mq_id, (uint32_t)msg_ptr, timeout, 0U);
#else
  (void)mq_id;
  (void)msg_ptr;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_PUT_TIMEOUT_DISABLE))
__WEAK void EvrRtxMessageQueuePutTimeout (osMessageQueueId_t mq_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueuePutTimeout, (uint32_t)mq_id, 0U);
#else
  (void)mq_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_INSERT_PENDING_DISABLE))
__WEAK void EvrRtxMessageQueueInsertPending (osMessageQueueId_t mq_id, const void *msg_ptr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueInsertPending, (uint32_t)mq_id, (uint32_t)msg_ptr);
#else
  (void)mq_id;
  (void)msg_ptr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_INSERTED_DISABLE))
__WEAK void EvrRtxMessageQueueInserted (osMessageQueueId_t mq_id, const void *msg_ptr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueInserted, (uint32_t)mq_id, (uint32_t)msg_ptr);
#else
  (void)mq_id;
  (void)msg_ptr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_NOT_INSERTED_DISABLE))
__WEAK void EvrRtxMessageQueueNotInserted (osMessageQueueId_t mq_id, const void *msg_ptr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueNotInserted, (uint32_t)mq_id, (uint32_t)msg_ptr);
#else
  (void)mq_id;
  (void)msg_ptr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_DISABLE))
__WEAK void EvrRtxMessageQueueGet (osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMessageQueueGet, (uint32_t)mq_id, (uint32_t)msg_ptr, (uint32_t)msg_prio, timeout);
#else
  (void)mq_id;
  (void)msg_ptr;
  (void)msg_prio;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_PENDING_DISABLE))
__WEAK void EvrRtxMessageQueueGetPending (osMessageQueueId_t mq_id, void *msg_ptr, uint32_t timeout) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord4(EvtRtxMessageQueueGetPending, (uint32_t)mq_id, (uint32_t)msg_ptr, timeout, 0U);
#else
  (void)mq_id;
  (void)msg_ptr;
  (void)timeout;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_TIMEOUT_DISABLE))
__WEAK void EvrRtxMessageQueueGetTimeout (osMessageQueueId_t mq_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueGetTimeout, (uint32_t)mq_id, 0U);
#else
  (void)mq_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_RETRIEVED_DISABLE))
__WEAK void EvrRtxMessageQueueRetrieved (osMessageQueueId_t mq_id, void *msg_ptr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueRetrieved, (uint32_t)mq_id, (uint32_t)msg_ptr);
#else
  (void)mq_id;
  (void)msg_ptr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_NOT_RETRIEVED_DISABLE))
__WEAK void EvrRtxMessageQueueNotRetrieved (osMessageQueueId_t mq_id, void *msg_ptr) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueNotRetrieved, (uint32_t)mq_id, (uint32_t)msg_ptr);
#else
  (void)mq_id;
  (void)msg_ptr;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_CAPACITY_DISABLE))
__WEAK void EvrRtxMessageQueueGetCapacity (osMessageQueueId_t mq_id, uint32_t capacity) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueGetCapacity, (uint32_t)mq_id, capacity);
#else
  (void)mq_id;
  (void)capacity;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_MSG_SIZE_DISABLE))
__WEAK void EvrRtxMessageQueueGetMsgSize (osMessageQueueId_t mq_id, uint32_t msg_size) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueGetMsgSize, (uint32_t)mq_id, msg_size);
#else
  (void)mq_id;
  (void)msg_size;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_COUNT_DISABLE))
__WEAK void EvrRtxMessageQueueGetCount (osMessageQueueId_t mq_id, uint32_t count) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueGetCount, (uint32_t)mq_id, count);
#else
  (void)mq_id;
  (void)count;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_GET_SPACE_DISABLE))
__WEAK void EvrRtxMessageQueueGetSpace (osMessageQueueId_t mq_id, uint32_t space) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueGetSpace, (uint32_t)mq_id, space);
#else
  (void)mq_id;
  (void)space;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_RESET_DISABLE))
__WEAK void EvrRtxMessageQueueReset (osMessageQueueId_t mq_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueReset, (uint32_t)mq_id, 0U);
#else
  (void)mq_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_RESET_DONE_DISABLE))
__WEAK void EvrRtxMessageQueueResetDone (osMessageQueueId_t mq_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueResetDone, (uint32_t)mq_id, 0U);
#else
  (void)mq_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_DELETE_DISABLE))
__WEAK void EvrRtxMessageQueueDelete (osMessageQueueId_t mq_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueDelete, (uint32_t)mq_id, 0U);
#else
  (void)mq_id;
#endif
}
#endif

#if (!defined(EVR_RTX_DISABLE) && (OS_EVR_MSGQUEUE != 0) && !defined(EVR_RTX_MESSAGE_QUEUE_DESTROYED_DISABLE))
__WEAK void EvrRtxMessageQueueDestroyed (osMessageQueueId_t mq_id) {
#if defined(RTE_Compiler_EventRecorder)
  (void)EventRecord2(EvtRtxMessageQueueDestroyed, (uint32_t)mq_id, 0U);
#else
  (void)mq_id;
#endif
}
#endif
