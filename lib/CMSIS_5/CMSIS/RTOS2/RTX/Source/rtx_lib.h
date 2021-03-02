/*
 * Copyright (c) 2013-2020 Arm Limited. All rights reserved.
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
 * Title:       RTX Library definitions
 *
 * -----------------------------------------------------------------------------
 */

#ifndef RTX_LIB_H_
#define RTX_LIB_H_

#include <string.h>
#include "rtx_core_c.h"                 // Cortex core definitions
#if ((defined(__ARM_ARCH_8M_BASE__)   && (__ARM_ARCH_8M_BASE__   != 0)) || \
     (defined(__ARM_ARCH_8M_MAIN__)   && (__ARM_ARCH_8M_MAIN__   != 0)) || \
     (defined(__ARM_ARCH_8_1M_MAIN__) && (__ARM_ARCH_8_1M_MAIN__ != 0)))
#include "tz_context.h"                 // TrustZone Context API
#endif
#include "os_tick.h"                    // CMSIS OS Tick API
#include "cmsis_os2.h"                  // CMSIS RTOS API
#include "RTX_Config.h"                 // RTX Configuration
#include "rtx_os.h"                     // RTX OS definitions
#include "rtx_evr.h"                    // RTX Event Recorder definitions


//  ==== Library defines ====

#define os_thread_t         osRtxThread_t
#define os_timer_t          osRtxTimer_t
#define os_timer_finfo_t    osRtxTimerFinfo_t
#define os_event_flags_t    osRtxEventFlags_t
#define os_mutex_t          osRtxMutex_t
#define os_semaphore_t      osRtxSemaphore_t
#define os_mp_info_t        osRtxMpInfo_t
#define os_memory_pool_t    osRtxMemoryPool_t
#define os_message_t        osRtxMessage_t
#define os_message_queue_t  osRtxMessageQueue_t
#define os_object_t         osRtxObject_t

//  ==== Inline functions ====

// Thread ID
__STATIC_INLINE os_thread_t *osRtxThreadId (osThreadId_t thread_id) {
  //lint -e{9079} -e{9087} "cast from pointer to void to pointer to object type" [MISRA Note 2]
  return ((os_thread_t *)thread_id);
}
// Timer ID
__STATIC_INLINE os_timer_t *osRtxTimerId (osTimerId_t timer_id) {
  //lint -e{9079} -e{9087} "cast from pointer to void to pointer to object type" [MISRA Note 2]
  return ((os_timer_t *)timer_id);
}
// Event Flags ID
__STATIC_INLINE os_event_flags_t *osRtxEventFlagsId (osEventFlagsId_t ef_id) {
  //lint -e{9079} -e{9087} "cast from pointer to void to pointer to object type" [MISRA Note 2]
  return ((os_event_flags_t *)ef_id);
}
// Mutex ID
__STATIC_INLINE os_mutex_t *osRtxMutexId (osMutexId_t mutex_id) {
  //lint -e{9079} -e{9087} "cast from pointer to void to pointer to object type" [MISRA Note 2]
  return ((os_mutex_t *)mutex_id);
}
// Semaphore ID
__STATIC_INLINE os_semaphore_t *osRtxSemaphoreId (osSemaphoreId_t semaphore_id) {
  //lint -e{9079} -e{9087} "cast from pointer to void to pointer to object type" [MISRA Note 2]
  return ((os_semaphore_t *)semaphore_id);
}
// Memory Pool ID
__STATIC_INLINE os_memory_pool_t *osRtxMemoryPoolId (osMemoryPoolId_t mp_id) {
  //lint -e{9079} -e{9087} "cast from pointer to void to pointer to object type" [MISRA Note 2]
  return ((os_memory_pool_t *)mp_id);
}
// Message Queue ID
__STATIC_INLINE os_message_queue_t *osRtxMessageQueueId (osMessageQueueId_t mq_id) {
  //lint -e{9079} -e{9087} "cast from pointer to void to pointer to object type" [MISRA Note 2]
  return ((os_message_queue_t *)mq_id);
}

// Generic Object
__STATIC_INLINE os_object_t *osRtxObject (void *object) {
  //lint -e{9079} -e{9087} "cast from pointer to void to pointer to object type" [MISRA Note 3]
  return ((os_object_t *)object);
}

// Thread Object
__STATIC_INLINE os_thread_t *osRtxThreadObject (os_object_t *object) {
  //lint -e{740} -e{826} -e{9087} "cast from pointer to generic object to specific object" [MISRA Note 4]
  return ((os_thread_t *)object);
}
// Timer Object
__STATIC_INLINE os_timer_t *osRtxTimerObject (os_object_t *object) {
  //lint -e{740} -e{826} -e{9087} "cast from pointer to generic object to specific object" [MISRA Note 4]
  return ((os_timer_t *)object);
}
// Event Flags Object
__STATIC_INLINE os_event_flags_t *osRtxEventFlagsObject (os_object_t *object) {
  //lint -e{740} -e{826} -e{9087} "cast from pointer to generic object to specific object" [MISRA Note 4]
  return ((os_event_flags_t *)object);
}
// Mutex Object
__STATIC_INLINE os_mutex_t *osRtxMutexObject (os_object_t *object) {
  //lint -e{740} -e{826} -e{9087} "cast from pointer to generic object to specific object" [MISRA Note 4]
  return ((os_mutex_t *)object);
}
// Semaphore Object
__STATIC_INLINE os_semaphore_t *osRtxSemaphoreObject (os_object_t *object) {
  //lint -e{740} -e{826} -e{9087} "cast from pointer to generic object to specific object" [MISRA Note 4]
  return ((os_semaphore_t *)object);
}
// Memory Pool Object
__STATIC_INLINE os_memory_pool_t *osRtxMemoryPoolObject (os_object_t *object) {
  //lint -e{740} -e{826} -e{9087} "cast from pointer to generic object to specific object" [MISRA Note 4]
  return ((os_memory_pool_t *)object);
}
// Message Queue Object
__STATIC_INLINE os_message_queue_t *osRtxMessageQueueObject (os_object_t *object) {
  //lint -e{740} -e{826} -e{9087} "cast from pointer to generic object to specific object" [MISRA Note 4]
  return ((os_message_queue_t *)object);
}
// Message Object
__STATIC_INLINE os_message_t *osRtxMessageObject (os_object_t *object) {
  //lint -e{740} -e{826} -e{9087} "cast from pointer to generic object to specific object" [MISRA Note 4]
  return ((os_message_t *)object);
}

// Kernel State
__STATIC_INLINE osKernelState_t osRtxKernelState (void) {
  //lint -e{9030} -e{9034} "cast to enum"
  return ((osKernelState_t)(osRtxInfo.kernel.state));
}

// Thread State
__STATIC_INLINE osThreadState_t osRtxThreadState (const os_thread_t *thread) {
  uint8_t state = thread->state & osRtxThreadStateMask;
  //lint -e{9030} -e{9034} "cast to enum"
  return ((osThreadState_t)state);
}

// Thread Priority
__STATIC_INLINE osPriority_t osRtxThreadPriority (const os_thread_t *thread) {
  //lint -e{9030} -e{9034} "cast to enum"
  return ((osPriority_t)thread->priority);
}

// Kernel Get State
__STATIC_INLINE uint8_t osRtxKernelGetState (void) {
  return osRtxInfo.kernel.state;
}

// Thread Get/Set Running
__STATIC_INLINE os_thread_t *osRtxThreadGetRunning (void) {
  return osRtxInfo.thread.run.curr;
}
__STATIC_INLINE void osRtxThreadSetRunning (os_thread_t *thread) {
  osRtxInfo.thread.run.curr = thread;
}


//  ==== Library functions ====

// Kernel Library functions
extern void         osRtxKernelPreInit (void);

// Thread Library functions
extern void         osRtxThreadListPut    (os_object_t *object, os_thread_t *thread);
extern os_thread_t *osRtxThreadListGet    (os_object_t *object);
extern void         osRtxThreadListSort   (os_thread_t *thread);
extern void         osRtxThreadListRemove (os_thread_t *thread);
extern void         osRtxThreadReadyPut   (os_thread_t *thread);
extern void         osRtxThreadDelayTick  (void);
extern uint32_t    *osRtxThreadRegPtr     (const os_thread_t *thread);
extern void         osRtxThreadSwitch     (os_thread_t *thread);
extern void         osRtxThreadDispatch   (os_thread_t *thread);
extern void         osRtxThreadWaitExit   (os_thread_t *thread, uint32_t ret_val, bool_t dispatch);
extern bool_t       osRtxThreadWaitEnter  (uint8_t state, uint32_t timeout);
extern void         osRtxThreadStackCheck (void);
extern bool_t       osRtxThreadStartup    (void);

// Timer Library functions
extern void osRtxTimerThread (void *argument);

// Mutex Library functions
extern void osRtxMutexOwnerRelease (os_mutex_t *mutex_list);
extern void osRtxMutexOwnerRestore (const os_mutex_t *mutex, const os_thread_t *thread_wakeup);

// Memory Heap Library functions
extern uint32_t osRtxMemoryInit (void *mem, uint32_t size);
extern void    *osRtxMemoryAlloc(void *mem, uint32_t size, uint32_t type);
extern uint32_t osRtxMemoryFree (void *mem, void *block);

// Memory Pool Library functions
extern uint32_t   osRtxMemoryPoolInit  (os_mp_info_t *mp_info, uint32_t block_count, uint32_t block_size, void *block_mem);
extern void      *osRtxMemoryPoolAlloc (os_mp_info_t *mp_info);
extern osStatus_t osRtxMemoryPoolFree  (os_mp_info_t *mp_info, void *block);

// System Library functions
extern void osRtxTick_Handler   (void);
extern void osRtxPendSV_Handler (void);
extern void osRtxPostProcess    (os_object_t *object);


#endif  // RTX_LIB_H_
