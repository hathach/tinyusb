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
 * Title:       RTX Library Configuration
 *
 * -----------------------------------------------------------------------------
 */

#include "cmsis_compiler.h"
#include "RTX_Config.h"
#include "rtx_os.h"

#ifdef    RTE_Compiler_EventRecorder
#include "EventRecorder.h"
#include "EventRecorderConf.h"
#endif
#include "rtx_evr.h"


// System Configuration
// ====================

// Dynamic Memory
#if (OS_DYNAMIC_MEM_SIZE != 0)
#if ((OS_DYNAMIC_MEM_SIZE % 8) != 0)
#error "Invalid Dynamic Memory size!"
#endif
static uint64_t os_mem[OS_DYNAMIC_MEM_SIZE/8] \
__attribute__((section(".bss.os")));
#endif

// Kernel Tick Frequency
#if (OS_TICK_FREQ < 1)
#error "Invalid Kernel Tick Frequency!"
#endif

// ISR FIFO Queue
#if (OS_ISR_FIFO_QUEUE < 4)
#error "Invalid ISR FIFO Queue size!"
#endif
static void *os_isr_queue[OS_ISR_FIFO_QUEUE] \
__attribute__((section(".bss.os")));


// Thread Configuration
// ====================

#if (((OS_STACK_SIZE % 8) != 0) || (OS_STACK_SIZE < 72))
#error "Invalid default Thread Stack size!"
#endif

#if (((OS_IDLE_THREAD_STACK_SIZE % 8) != 0) || (OS_IDLE_THREAD_STACK_SIZE < 72))
#error "Invalid Idle Thread Stack size!"
#endif


#if (OS_THREAD_OBJ_MEM != 0)

#if (OS_THREAD_NUM == 0)
#error "Invalid number of user Threads!"
#endif

#if ((OS_THREAD_USER_STACK_SIZE != 0) && ((OS_THREAD_USER_STACK_SIZE % 8) != 0))
#error "Invalid total Stack size!"
#endif

// Thread Control Blocks
static osRtxThread_t os_thread_cb[OS_THREAD_NUM] \
__attribute__((section(".bss.os.thread.cb")));

// Thread Default Stack
#if (OS_THREAD_DEF_STACK_NUM != 0)
static uint64_t os_thread_def_stack[OS_THREAD_DEF_STACK_NUM*(OS_STACK_SIZE/8)] \
__attribute__((section(".bss.os.thread.stack")));
#endif

// Memory Pool for Thread Control Blocks
static osRtxMpInfo_t os_mpi_thread \
__attribute__((section(".data.os.thread.mpi"))) =
{ (uint32_t)OS_THREAD_NUM, 0U, (uint32_t)osRtxThreadCbSize, &os_thread_cb[0], NULL, NULL };

// Memory Pool for Thread Default Stack
#if (OS_THREAD_DEF_STACK_NUM != 0)
static osRtxMpInfo_t os_mpi_def_stack \
__attribute__((section(".data.os.thread.mpi"))) =
{ (uint32_t)OS_THREAD_DEF_STACK_NUM, 0U, (uint32_t)OS_STACK_SIZE, &os_thread_def_stack[0], NULL, NULL };
#endif

// Memory Pool for Thread Stack
#if (OS_THREAD_USER_STACK_SIZE != 0)
static uint64_t os_thread_stack[2 + OS_THREAD_NUM + (OS_THREAD_USER_STACK_SIZE/8)] \
__attribute__((section(".bss.os.thread.stack")));
#endif

#endif  // (OS_THREAD_OBJ_MEM != 0)


// Stack overrun checking
#if (OS_STACK_CHECK == 0)
// Override library function
extern void osRtxThreadStackCheck (void);
       void osRtxThreadStackCheck (void) {}
#endif


// Idle Thread Control Block
static osRtxThread_t os_idle_thread_cb \
__attribute__((section(".bss.os.thread.cb")));

// Idle Thread Stack
static uint64_t os_idle_thread_stack[OS_IDLE_THREAD_STACK_SIZE/8] \
__attribute__((section(".bss.os.thread.stack")));

// Idle Thread Attributes
static const osThreadAttr_t os_idle_thread_attr = {
#if defined(OS_IDLE_THREAD_NAME)
  OS_IDLE_THREAD_NAME,
#else
  NULL,
#endif
  osThreadDetached,
  &os_idle_thread_cb,
  (uint32_t)sizeof(os_idle_thread_cb),
  &os_idle_thread_stack[0],
  (uint32_t)sizeof(os_idle_thread_stack),
  osPriorityIdle,
#if defined(OS_IDLE_THREAD_TZ_MOD_ID)
  (uint32_t)OS_IDLE_THREAD_TZ_MOD_ID,
#else
  0U,
#endif
  0U
};


// Timer Configuration
// ===================

#if (OS_TIMER_OBJ_MEM != 0)

#if (OS_TIMER_NUM == 0)
#error "Invalid number of Timer objects!"
#endif

// Timer Control Blocks
static osRtxTimer_t os_timer_cb[OS_TIMER_NUM] \
__attribute__((section(".bss.os.timer.cb")));

// Memory Pool for Timer Control Blocks
static osRtxMpInfo_t os_mpi_timer \
__attribute__((section(".data.os.timer.mpi"))) =
{ (uint32_t)OS_TIMER_NUM, 0U, (uint32_t)osRtxTimerCbSize, &os_timer_cb[0], NULL, NULL };

#endif  // (OS_TIMER_OBJ_MEM != 0)


#if ((OS_TIMER_THREAD_STACK_SIZE != 0) && (OS_TIMER_CB_QUEUE != 0))

#if (((OS_TIMER_THREAD_STACK_SIZE % 8) != 0) || (OS_TIMER_THREAD_STACK_SIZE < 96))
#error "Invalid Timer Thread Stack size!"
#endif

// Timer Thread Control Block
static osRtxThread_t os_timer_thread_cb \
__attribute__((section(".bss.os.thread.cb")));

// Timer Thread Stack
static uint64_t os_timer_thread_stack[OS_TIMER_THREAD_STACK_SIZE/8] \
__attribute__((section(".bss.os.thread.stack")));

// Timer Thread Attributes
static const osThreadAttr_t os_timer_thread_attr = {
#if defined(OS_TIMER_THREAD_NAME)
  OS_TIMER_THREAD_NAME,
#else
  NULL,
#endif
  osThreadDetached,
  &os_timer_thread_cb,
  (uint32_t)sizeof(os_timer_thread_cb),
  &os_timer_thread_stack[0],
  (uint32_t)sizeof(os_timer_thread_stack),
  //lint -e{9030} -e{9034} "cast from signed to enum"
  (osPriority_t)OS_TIMER_THREAD_PRIO,
#if defined(OS_TIMER_THREAD_TZ_MOD_ID)
  (uint32_t)OS_TIMER_THREAD_TZ_MOD_ID,
#else
  0U,
#endif
  0U
};

// Timer Message Queue Control Block
static osRtxMessageQueue_t os_timer_mq_cb \
__attribute__((section(".bss.os.msgqueue.cb")));

// Timer Message Queue Data
static uint32_t os_timer_mq_data[osRtxMessageQueueMemSize(OS_TIMER_CB_QUEUE,8)/4] \
__attribute__((section(".bss.os.msgqueue.mem")));

// Timer Message Queue Attributes
static const osMessageQueueAttr_t os_timer_mq_attr = {
  NULL,
  0U,
  &os_timer_mq_cb,
  (uint32_t)sizeof(os_timer_mq_cb),
  &os_timer_mq_data[0],
  (uint32_t)sizeof(os_timer_mq_data)
};

#else

extern void osRtxTimerThread (void *argument);
       void osRtxTimerThread (void *argument) { (void)argument; }

#endif  // ((OS_TIMER_THREAD_STACK_SIZE != 0) && (OS_TIMER_CB_QUEUE != 0))


// Event Flags Configuration
// =========================

#if (OS_EVFLAGS_OBJ_MEM != 0)

#if (OS_EVFLAGS_NUM == 0)
#error "Invalid number of Event Flags objects!"
#endif

// Event Flags Control Blocks
static osRtxEventFlags_t os_ef_cb[OS_EVFLAGS_NUM] \
__attribute__((section(".bss.os.evflags.cb")));

// Memory Pool for Event Flags Control Blocks
static osRtxMpInfo_t os_mpi_ef \
__attribute__((section(".data.os.evflags.mpi"))) =
{ (uint32_t)OS_EVFLAGS_NUM, 0U, (uint32_t)osRtxEventFlagsCbSize, &os_ef_cb[0], NULL, NULL };

#endif  // (OS_EVFLAGS_OBJ_MEM != 0)


// Mutex Configuration
// ===================

#if (OS_MUTEX_OBJ_MEM != 0)

#if (OS_MUTEX_NUM == 0)
#error "Invalid number of Mutex objects!"
#endif

// Mutex Control Blocks
static osRtxMutex_t os_mutex_cb[OS_MUTEX_NUM] \
__attribute__((section(".bss.os.mutex.cb")));

// Memory Pool for Mutex Control Blocks
static osRtxMpInfo_t os_mpi_mutex \
__attribute__((section(".data.os.mutex.mpi"))) =
{ (uint32_t)OS_MUTEX_NUM, 0U, (uint32_t)osRtxMutexCbSize, &os_mutex_cb[0], NULL, NULL };

#endif  // (OS_MUTEX_OBJ_MEM != 0)


// Semaphore Configuration
// =======================

#if (OS_SEMAPHORE_OBJ_MEM != 0)

#if (OS_SEMAPHORE_NUM == 0)
#error "Invalid number of Semaphore objects!"
#endif

// Semaphore Control Blocks
static osRtxSemaphore_t os_semaphore_cb[OS_SEMAPHORE_NUM] \
__attribute__((section(".bss.os.semaphore.cb")));

// Memory Pool for Semaphore Control Blocks
static osRtxMpInfo_t os_mpi_semaphore \
__attribute__((section(".data.os.semaphore.mpi"))) =
{ (uint32_t)OS_SEMAPHORE_NUM, 0U, (uint32_t)osRtxSemaphoreCbSize, &os_semaphore_cb[0], NULL, NULL };

#endif  // (OS_SEMAPHORE_OBJ_MEM != 0)


// Memory Pool Configuration
// =========================

#if (OS_MEMPOOL_OBJ_MEM != 0)

#if (OS_MEMPOOL_NUM == 0)
#error "Invalid number of Memory Pool objects!"
#endif

// Memory Pool Control Blocks
static osRtxMemoryPool_t os_mp_cb[OS_MEMPOOL_NUM] \
__attribute__((section(".bss.os.mempool.cb")));

// Memory Pool for Memory Pool Control Blocks
static osRtxMpInfo_t os_mpi_mp \
__attribute__((section(".data.os.mempool.mpi"))) =
{ (uint32_t)OS_MEMPOOL_NUM, 0U, (uint32_t)osRtxMemoryPoolCbSize, &os_mp_cb[0], NULL, NULL };

// Memory Pool for Memory Pool Data Storage
#if (OS_MEMPOOL_DATA_SIZE != 0)
#if ((OS_MEMPOOL_DATA_SIZE % 8) != 0)
#error "Invalid Data Memory size for Memory Pools!"
#endif
static uint64_t os_mp_data[2 + OS_MEMPOOL_NUM + (OS_MEMPOOL_DATA_SIZE/8)] \
__attribute__((section(".bss.os.mempool.mem")));
#endif

#endif  // (OS_MEMPOOL_OBJ_MEM != 0)


// Message Queue Configuration
// ===========================

#if (OS_MSGQUEUE_OBJ_MEM != 0)

#if (OS_MSGQUEUE_NUM == 0)
#error "Invalid number of Message Queue objects!"
#endif

// Message Queue Control Blocks
static osRtxMessageQueue_t os_mq_cb[OS_MSGQUEUE_NUM] \
__attribute__((section(".bss.os.msgqueue.cb")));

// Memory Pool for Message Queue Control Blocks
static osRtxMpInfo_t os_mpi_mq \
__attribute__((section(".data.os.msgqueue.mpi"))) =
{ (uint32_t)OS_MSGQUEUE_NUM, 0U, (uint32_t)osRtxMessageQueueCbSize, &os_mq_cb[0], NULL, NULL };

// Memory Pool for Message Queue Data Storage
#if (OS_MSGQUEUE_DATA_SIZE != 0)
#if ((OS_MSGQUEUE_DATA_SIZE % 8) != 0)
#error "Invalid Data Memory size for Message Queues!"
#endif
static uint64_t os_mq_data[2 + OS_MSGQUEUE_NUM + (OS_MSGQUEUE_DATA_SIZE/8)] \
__attribute__((section(".bss.os.msgqueue.mem")));
#endif

#endif  // (OS_MSGQUEUE_OBJ_MEM != 0)


// Event Recorder Configuration
// ============================

#if (defined(OS_EVR_INIT) && (OS_EVR_INIT != 0))

// Initial Thread configuration covered also Thread Flags and Generic Wait
#if  defined(OS_EVR_THREAD_FILTER)
#if !defined(OS_EVR_THFLAGS_FILTER)
#define OS_EVR_THFLAGS_FILTER   OS_EVR_THREAD_FILTER
#endif
#if !defined(OS_EVR_WAIT_FILTER)
#define OS_EVR_WAIT_FILTER      OS_EVR_THREAD_FILTER
#endif
#endif

// Migrate initial filter configuration
#if  defined(OS_EVR_MEMORY_FILTER)
#define OS_EVR_MEMORY_LEVEL     (((OS_EVR_MEMORY_FILTER    & 0x80U) != 0U) ? (OS_EVR_MEMORY_FILTER    & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_KERNEL_FILTER)
#define OS_EVR_KERNEL_LEVEL     (((OS_EVR_KERNEL_FILTER    & 0x80U) != 0U) ? (OS_EVR_KERNEL_FILTER    & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_THREAD_FILTER)
#define OS_EVR_THREAD_LEVEL     (((OS_EVR_THREAD_FILTER    & 0x80U) != 0U) ? (OS_EVR_THREAD_FILTER    & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_WAIT_FILTER)
#define OS_EVR_WAIT_LEVEL       (((OS_EVR_WAIT_FILTER      & 0x80U) != 0U) ? (OS_EVR_WAIT_FILTER      & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_THFLAGS_FILTER)
#define OS_EVR_THFLAGS_LEVEL    (((OS_EVR_THFLAGS_FILTER   & 0x80U) != 0U) ? (OS_EVR_THFLAGS_FILTER   & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_EVFLAGS_FILTER)
#define OS_EVR_EVFLAGS_LEVEL    (((OS_EVR_EVFLAGS_FILTER   & 0x80U) != 0U) ? (OS_EVR_EVFLAGS_FILTER   & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_TIMER_FILTER)
#define OS_EVR_TIMER_LEVEL      (((OS_EVR_TIMER_FILTER     & 0x80U) != 0U) ? (OS_EVR_TIMER_FILTER     & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_MUTEX_FILTER)
#define OS_EVR_MUTEX_LEVEL      (((OS_EVR_MUTEX_FILTER     & 0x80U) != 0U) ? (OS_EVR_MUTEX_FILTER     & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_SEMAPHORE_FILTER)
#define OS_EVR_SEMAPHORE_LEVEL  (((OS_EVR_SEMAPHORE_FILTER & 0x80U) != 0U) ? (OS_EVR_SEMAPHORE_FILTER & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_MEMPOOL_FILTER)
#define OS_EVR_MEMPOOL_LEVEL    (((OS_EVR_MEMPOOL_FILTER   & 0x80U) != 0U) ? (OS_EVR_MEMPOOL_FILTER   & 0x0FU) : 0U)
#endif
#if  defined(OS_EVR_MSGQUEUE_FILTER)
#define OS_EVR_MSGQUEUE_LEVEL   (((OS_EVR_MSGQUEUE_FILTER  & 0x80U) != 0U) ? (OS_EVR_MSGQUEUE_FILTER  & 0x0FU) : 0U)
#endif

#if  defined(RTE_Compiler_EventRecorder)

// Event Recorder Initialize
__STATIC_INLINE void evr_initialize (void) {

  (void)EventRecorderInitialize(OS_EVR_LEVEL, (uint32_t)OS_EVR_START);

  (void)EventRecorderEnable(OS_EVR_MEMORY_LEVEL,    EvtRtxMemoryNo,       EvtRtxMemoryNo);
  (void)EventRecorderEnable(OS_EVR_KERNEL_LEVEL,    EvtRtxKernelNo,       EvtRtxKernelNo);
  (void)EventRecorderEnable(OS_EVR_THREAD_LEVEL,    EvtRtxThreadNo,       EvtRtxThreadNo);
  (void)EventRecorderEnable(OS_EVR_WAIT_LEVEL,      EvtRtxWaitNo,         EvtRtxWaitNo);
  (void)EventRecorderEnable(OS_EVR_THFLAGS_LEVEL,   EvtRtxThreadFlagsNo,  EvtRtxThreadFlagsNo);
  (void)EventRecorderEnable(OS_EVR_EVFLAGS_LEVEL,   EvtRtxEventFlagsNo,   EvtRtxEventFlagsNo);
  (void)EventRecorderEnable(OS_EVR_TIMER_LEVEL,     EvtRtxTimerNo,        EvtRtxTimerNo);
  (void)EventRecorderEnable(OS_EVR_MUTEX_LEVEL,     EvtRtxMutexNo,        EvtRtxMutexNo);
  (void)EventRecorderEnable(OS_EVR_SEMAPHORE_LEVEL, EvtRtxSemaphoreNo,    EvtRtxSemaphoreNo);
  (void)EventRecorderEnable(OS_EVR_MEMPOOL_LEVEL,   EvtRtxMemoryPoolNo,   EvtRtxMemoryPoolNo);
  (void)EventRecorderEnable(OS_EVR_MSGQUEUE_LEVEL,  EvtRtxMessageQueueNo, EvtRtxMessageQueueNo);
}

#else
#warning "Event Recorder cannot be initialized (Event Recorder component is not selected)!"
#define evr_initialize()
#endif

#endif  // (OS_EVR_INIT != 0)


// OS Configuration
// ================


const osRtxConfig_t osRtxConfig \
__USED \
__attribute__((section(".rodata"))) =
{
  //lint -e{835} "Zero argument to operator"
  0U   // Flags
#if (OS_PRIVILEGE_MODE != 0)
  | osRtxConfigPrivilegedMode
#endif
#if (OS_STACK_CHECK != 0)
  | osRtxConfigStackCheck
#endif
#if (OS_STACK_WATERMARK != 0)
  | osRtxConfigStackWatermark
#endif
  ,
  (uint32_t)OS_TICK_FREQ,
#if (OS_ROBIN_ENABLE != 0)
  (uint32_t)OS_ROBIN_TIMEOUT,
#else
  0U,
#endif
  { &os_isr_queue[0], (uint16_t)(sizeof(os_isr_queue)/sizeof(void *)), 0U },
  { 
    // Memory Pools (Variable Block Size)
#if ((OS_THREAD_OBJ_MEM != 0) && (OS_THREAD_USER_STACK_SIZE != 0))
    &os_thread_stack[0], sizeof(os_thread_stack),
#else
    NULL, 0U,
#endif
#if ((OS_MEMPOOL_OBJ_MEM != 0) && (OS_MEMPOOL_DATA_SIZE != 0))
    &os_mp_data[0], sizeof(os_mp_data),
#else
    NULL, 0U,
#endif
#if ((OS_MSGQUEUE_OBJ_MEM != 0) && (OS_MSGQUEUE_DATA_SIZE != 0))
    &os_mq_data[0], sizeof(os_mq_data),
#else
    NULL, 0U,
#endif
#if (OS_DYNAMIC_MEM_SIZE != 0)
    &os_mem[0], (uint32_t)OS_DYNAMIC_MEM_SIZE,
#else
    NULL, 0U
#endif
  },
  {
    // Memory Pools (Fixed Block Size)
#if (OS_THREAD_OBJ_MEM != 0)
#if (OS_THREAD_DEF_STACK_NUM != 0)
    &os_mpi_def_stack,
#else
    NULL,
#endif
    &os_mpi_thread,
#else
    NULL, 
    NULL,
#endif
#if (OS_TIMER_OBJ_MEM != 0)
    &os_mpi_timer,
#else
    NULL,
#endif
#if (OS_EVFLAGS_OBJ_MEM != 0)
    &os_mpi_ef,
#else
    NULL,
#endif
#if (OS_MUTEX_OBJ_MEM != 0)
    &os_mpi_mutex,
#else
    NULL,
#endif
#if (OS_SEMAPHORE_OBJ_MEM != 0)
    &os_mpi_semaphore,
#else
    NULL,
#endif
#if (OS_MEMPOOL_OBJ_MEM != 0)
    &os_mpi_mp,
#else
    NULL,
#endif
#if (OS_MSGQUEUE_OBJ_MEM != 0)
    &os_mpi_mq,
#else
    NULL,
#endif
  },
  (uint32_t)OS_STACK_SIZE,
  &os_idle_thread_attr,
#if ((OS_TIMER_THREAD_STACK_SIZE != 0) && (OS_TIMER_CB_QUEUE != 0))
  &os_timer_thread_attr,
  &os_timer_mq_attr,
  (uint32_t)OS_TIMER_CB_QUEUE
#else
  NULL,
  NULL,
  0U
#endif
};


// Non weak reference to library irq module
//lint -esym(526,irqRtxLib)    "Defined by Exception handlers"
//lint -esym(714,irqRtxLibRef) "Non weak reference"
//lint -esym(765,irqRtxLibRef) "Global scope"
extern       uint8_t  irqRtxLib;
extern const uint8_t *irqRtxLibRef;
       const uint8_t *irqRtxLibRef = &irqRtxLib;

// Default User SVC Table
//lint -esym(714,osRtxUserSVC) "Referenced by Exception handlers"
//lint -esym(765,osRtxUserSVC) "Global scope"
//lint -e{9067} "extern array declared without size"
extern void * const osRtxUserSVC[];
__WEAK void * const osRtxUserSVC[1] = { (void *)0 };


// OS Sections
// ===========

#if  defined(__CC_ARM) || \
    (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
static uint32_t __os_thread_cb_start__    __attribute__((weakref(".bss.os.thread.cb$$Base")));     //lint -esym(728,__os_thread_cb_start__)
static uint32_t __os_thread_cb_end__      __attribute__((weakref(".bss.os.thread.cb$$Limit")));    //lint -esym(728,__os_thread_cb_end__)
static uint32_t __os_timer_cb_start__     __attribute__((weakref(".bss.os.timer.cb$$Base")));      //lint -esym(728,__os_timer_cb_start__)
static uint32_t __os_timer_cb_end__       __attribute__((weakref(".bss.os.timer.cb$$Limit")));     //lint -esym(728,__os_timer_cb_end__)
static uint32_t __os_evflags_cb_start__   __attribute__((weakref(".bss.os.evflags.cb$$Base")));    //lint -esym(728,__os_evflags_cb_start__)
static uint32_t __os_evflags_cb_end__     __attribute__((weakref(".bss.os.evflags.cb$$Limit")));   //lint -esym(728,__os_evflags_cb_end__)
static uint32_t __os_mutex_cb_start__     __attribute__((weakref(".bss.os.mutex.cb$$Base")));      //lint -esym(728,__os_mutex_cb_start__)
static uint32_t __os_mutex_cb_end__       __attribute__((weakref(".bss.os.mutex.cb$$Limit")));     //lint -esym(728,__os_mutex_cb_end__)
static uint32_t __os_semaphore_cb_start__ __attribute__((weakref(".bss.os.semaphore.cb$$Base")));  //lint -esym(728,__os_semaphore_cb_start__)
static uint32_t __os_semaphore_cb_end__   __attribute__((weakref(".bss.os.semaphore.cb$$Limit"))); //lint -esym(728,__os_semaphore_cb_end__)
static uint32_t __os_mempool_cb_start__   __attribute__((weakref(".bss.os.mempool.cb$$Base")));    //lint -esym(728,__os_mempool_cb_start__)
static uint32_t __os_mempool_cb_end__     __attribute__((weakref(".bss.os.mempool.cb$$Limit")));   //lint -esym(728,__os_mempool_cb_end__)
static uint32_t __os_msgqueue_cb_start__  __attribute__((weakref(".bss.os.msgqueue.cb$$Base")));   //lint -esym(728,__os_msgqueue_cb_start__)
static uint32_t __os_msgqueue_cb_end__    __attribute__((weakref(".bss.os.msgqueue.cb$$Limit")));  //lint -esym(728,__os_msgqueue_cb_end__)
#else
extern uint32_t __os_thread_cb_start__    __attribute__((weak));
extern uint32_t __os_thread_cb_end__      __attribute__((weak));
extern uint32_t __os_timer_cb_start__     __attribute__((weak));
extern uint32_t __os_timer_cb_end__       __attribute__((weak));
extern uint32_t __os_evflags_cb_start__   __attribute__((weak));
extern uint32_t __os_evflags_cb_end__     __attribute__((weak));
extern uint32_t __os_mutex_cb_start__     __attribute__((weak));
extern uint32_t __os_mutex_cb_end__       __attribute__((weak));
extern uint32_t __os_semaphore_cb_start__ __attribute__((weak));
extern uint32_t __os_semaphore_cb_end__   __attribute__((weak));
extern uint32_t __os_mempool_cb_start__   __attribute__((weak));
extern uint32_t __os_mempool_cb_end__     __attribute__((weak));
extern uint32_t __os_msgqueue_cb_start__  __attribute__((weak));
extern uint32_t __os_msgqueue_cb_end__    __attribute__((weak));
#endif

//lint -e{9067} "extern array declared without size"
extern const uint32_t * const os_cb_sections[];

//lint -esym(714,os_cb_sections) "Referenced by debugger"
//lint -esym(765,os_cb_sections) "Global scope"
const uint32_t * const os_cb_sections[] \
__USED \
__attribute__((section(".rodata"))) =
{
  &__os_thread_cb_start__,
  &__os_thread_cb_end__,
  &__os_timer_cb_start__,
  &__os_timer_cb_end__,
  &__os_evflags_cb_start__,
  &__os_evflags_cb_end__,
  &__os_mutex_cb_start__,
  &__os_mutex_cb_end__,
  &__os_semaphore_cb_start__,
  &__os_semaphore_cb_end__,
  &__os_mempool_cb_start__,
  &__os_mempool_cb_end__,
  &__os_msgqueue_cb_start__,
  &__os_msgqueue_cb_end__
};


// OS Initialization
// =================

#if  defined(__CC_ARM) || \
    (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))

#ifndef __MICROLIB
//lint -esym(714,_platform_post_stackheap_init) "Referenced by C library"
//lint -esym(765,_platform_post_stackheap_init) "Global scope"
extern void _platform_post_stackheap_init (void);
__WEAK void _platform_post_stackheap_init (void) {
  (void)osKernelInitialize();
}
#endif

#elif defined(__GNUC__)

extern void software_init_hook (void);
__WEAK void software_init_hook (void) {
  (void)osKernelInitialize();
}

#endif


// OS Hooks
// ========

// RTOS Kernel Pre-Initialization Hook
#if (defined(OS_EVR_INIT) && (OS_EVR_INIT != 0))
void osRtxKernelPreInit (void);
void osRtxKernelPreInit (void) {
  if (osKernelGetState() == osKernelInactive) {
    evr_initialize();
  }
}
#endif


// C/C++ Standard Library Multithreading Interface
// ===============================================

#if ( !defined(RTX_NO_MULTITHREAD_CLIB) && \
     ( defined(__CC_ARM) || \
      (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))) && \
      !defined(__MICROLIB))

#define LIBSPACE_SIZE 96

//lint -esym(714,__user_perthread_libspace,_mutex_*) "Referenced by C library"
//lint -esym(765,__user_perthread_libspace,_mutex_*) "Global scope"
//lint -esym(9003, os_libspace*) "variables 'os_libspace*' defined at module scope"

// Memory for libspace
static uint32_t os_libspace[OS_THREAD_LIBSPACE_NUM+1][LIBSPACE_SIZE/4] \
__attribute__((section(".bss.os.libspace")));

// Thread IDs for libspace
static osThreadId_t os_libspace_id[OS_THREAD_LIBSPACE_NUM] \
__attribute__((section(".bss.os.libspace")));

// Check if Kernel has been started
static uint32_t os_kernel_is_active (void) {
  static uint8_t os_kernel_active = 0U;

  if (os_kernel_active == 0U) {
    if (osKernelGetState() > osKernelReady) {
      os_kernel_active = 1U;
    }
  }
  return (uint32_t)os_kernel_active;
}

// Provide libspace for current thread
void *__user_perthread_libspace (void);
void *__user_perthread_libspace (void) {
  osThreadId_t id;
  uint32_t     n;

  if (os_kernel_is_active() != 0U) {
    id = osThreadGetId();
    for (n = 0U; n < (uint32_t)OS_THREAD_LIBSPACE_NUM; n++) {
      if (os_libspace_id[n] == NULL) {
        os_libspace_id[n] = id;
      }
      if (os_libspace_id[n] == id) {
        break;
      }
    }
    if (n == (uint32_t)OS_THREAD_LIBSPACE_NUM) {
      (void)osRtxErrorNotify(osRtxErrorClibSpace, id);
    }
  } else {
    n = OS_THREAD_LIBSPACE_NUM;
  }

  //lint -e{9087} "cast between pointers to different object types"
  return (void *)&os_libspace[n][0];
}

// Mutex identifier
typedef void *mutex;

//lint -save "Function prototypes defined in C library"
//lint -e970 "Use of 'int' outside of a typedef"
//lint -e818 "Pointer 'm' could be declared as pointing to const"

// Initialize mutex
__USED
int _mutex_initialize(mutex *m);
int _mutex_initialize(mutex *m) {
  int result;

  *m = osMutexNew(NULL);
  if (*m != NULL) {
    result = 1;
  } else {
    result = 0;
    (void)osRtxErrorNotify(osRtxErrorClibMutex, m);
  }
  return result;
}

// Acquire mutex
__USED
void _mutex_acquire(mutex *m);
void _mutex_acquire(mutex *m) {
  if (os_kernel_is_active() != 0U) {
    (void)osMutexAcquire(*m, osWaitForever);
  }
}

// Release mutex
__USED
void _mutex_release(mutex *m);
void _mutex_release(mutex *m) {
  if (os_kernel_is_active() != 0U) {
    (void)osMutexRelease(*m);
  }
}

// Free mutex
__USED
void _mutex_free(mutex *m);
void _mutex_free(mutex *m) {
  (void)osMutexDelete(*m);
}

//lint -restore

#endif
