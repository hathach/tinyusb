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
 * Title:       RTX OS definitions
 *
 * -----------------------------------------------------------------------------
 */
 
#ifndef RTX_OS_H_
#define RTX_OS_H_
 
#include <stdint.h>
#include <stddef.h>
#include "cmsis_os2.h"
 
#ifdef  __cplusplus
extern "C"
{
#endif
 
 
/// Kernel Information
#define osRtxVersionAPI      20010003   ///< API version (2.1.3)
#define osRtxVersionKernel   50050002   ///< Kernel version (5.5.2)
#define osRtxKernelId     "RTX V5.5.2"  ///< Kernel identification string
 
 
//  ==== Common definitions ====
 
/// Object Identifier definitions
#define osRtxIdInvalid          0x00U
#define osRtxIdThread           0xF1U
#define osRtxIdTimer            0xF2U
#define osRtxIdEventFlags       0xF3U
#define osRtxIdMutex            0xF5U
#define osRtxIdSemaphore        0xF6U
#define osRtxIdMemoryPool       0xF7U
#define osRtxIdMessage          0xF9U
#define osRtxIdMessageQueue     0xFAU
 
/// Object Flags definitions
#define osRtxFlagSystemObject   0x01U
#define osRtxFlagSystemMemory   0x02U
 
 
//  ==== Kernel definitions ====
 
/// Kernel State definitions
#define osRtxKernelInactive             ((uint8_t)osKernelInactive)
#define osRtxKernelReady                ((uint8_t)osKernelReady)
#define osRtxKernelRunning              ((uint8_t)osKernelRunning)
#define osRtxKernelLocked               ((uint8_t)osKernelLocked)
#define osRtxKernelSuspended            ((uint8_t)osKernelSuspended)
 
 
//  ==== Thread definitions ====
 
/// Thread State definitions (extending osThreadState)
#define osRtxThreadStateMask            0x0FU
 
#define osRtxThreadInactive             ((uint8_t)osThreadInactive)
#define osRtxThreadReady                ((uint8_t)osThreadReady)
#define osRtxThreadRunning              ((uint8_t)osThreadRunning)
#define osRtxThreadBlocked              ((uint8_t)osThreadBlocked)
#define osRtxThreadTerminated           ((uint8_t)osThreadTerminated)
 
#define osRtxThreadWaitingDelay         ((uint8_t)(osRtxThreadBlocked | 0x10U))
#define osRtxThreadWaitingJoin          ((uint8_t)(osRtxThreadBlocked | 0x20U))
#define osRtxThreadWaitingThreadFlags   ((uint8_t)(osRtxThreadBlocked | 0x30U))
#define osRtxThreadWaitingEventFlags    ((uint8_t)(osRtxThreadBlocked | 0x40U))
#define osRtxThreadWaitingMutex         ((uint8_t)(osRtxThreadBlocked | 0x50U))
#define osRtxThreadWaitingSemaphore     ((uint8_t)(osRtxThreadBlocked | 0x60U))
#define osRtxThreadWaitingMemoryPool    ((uint8_t)(osRtxThreadBlocked | 0x70U))
#define osRtxThreadWaitingMessageGet    ((uint8_t)(osRtxThreadBlocked | 0x80U))
#define osRtxThreadWaitingMessagePut    ((uint8_t)(osRtxThreadBlocked | 0x90U))
 
/// Thread Flags definitions
#define osRtxThreadFlagDefStack 0x10U   ///< Default Stack flag
 
/// Stack Marker definitions
#define osRtxStackMagicWord     0xE25A2EA5U ///< Stack Magic Word (Stack Base)
#define osRtxStackFillPattern   0xCCCCCCCCU ///< Stack Fill Pattern 
 
/// Thread Control Block
typedef struct osRtxThread_s {
  uint8_t                          id;  ///< Object Identifier
  uint8_t                       state;  ///< Object State
  uint8_t                       flags;  ///< Object Flags
  uint8_t                        attr;  ///< Object Attributes
  const char                    *name;  ///< Object Name
  struct osRtxThread_s   *thread_next;  ///< Link pointer to next Thread in Object list
  struct osRtxThread_s   *thread_prev;  ///< Link pointer to previous Thread in Object list
  struct osRtxThread_s    *delay_next;  ///< Link pointer to next Thread in Delay list
  struct osRtxThread_s    *delay_prev;  ///< Link pointer to previous Thread in Delay list
  struct osRtxThread_s   *thread_join;  ///< Thread waiting to Join
  uint32_t                      delay;  ///< Delay Time
  int8_t                     priority;  ///< Thread Priority
  int8_t                priority_base;  ///< Base Priority
  uint8_t                 stack_frame;  ///< Stack Frame (EXC_RETURN[7..0])
  uint8_t               flags_options;  ///< Thread/Event Flags Options
  uint32_t                 wait_flags;  ///< Waiting Thread/Event Flags
  uint32_t               thread_flags;  ///< Thread Flags
  struct osRtxMutex_s     *mutex_list;  ///< Link pointer to list of owned Mutexes
  void                     *stack_mem;  ///< Stack Memory
  uint32_t                 stack_size;  ///< Stack Size
  uint32_t                         sp;  ///< Current Stack Pointer
  uint32_t                thread_addr;  ///< Thread entry address
  uint32_t                  tz_memory;  ///< TrustZone Memory Identifier
#ifdef RTX_TF_M_EXTENSION
  uint32_t                  tz_module;  ///< TrustZone Module Identifier
#endif
} osRtxThread_t;
 
 
//  ==== Timer definitions ====
 
/// Timer State definitions
#define osRtxTimerInactive      0x00U   ///< Timer Inactive
#define osRtxTimerStopped       0x01U   ///< Timer Stopped
#define osRtxTimerRunning       0x02U   ///< Timer Running
 
/// Timer Type definitions
#define osRtxTimerPeriodic      ((uint8_t)osTimerPeriodic)
 
/// Timer Function Information
typedef struct {
  osTimerFunc_t                  func;  ///< Function Pointer
  void                           *arg;  ///< Function Argument
} osRtxTimerFinfo_t;
 
/// Timer Control Block
typedef struct osRtxTimer_s {
  uint8_t                          id;  ///< Object Identifier
  uint8_t                       state;  ///< Object State
  uint8_t                       flags;  ///< Object Flags
  uint8_t                        type;  ///< Timer Type (Periodic/One-shot)
  const char                    *name;  ///< Object Name
  struct osRtxTimer_s           *prev;  ///< Pointer to previous active Timer
  struct osRtxTimer_s           *next;  ///< Pointer to next active Timer
  uint32_t                       tick;  ///< Timer current Tick
  uint32_t                       load;  ///< Timer Load value
  osRtxTimerFinfo_t             finfo;  ///< Timer Function Info
} osRtxTimer_t;
 
 
//  ==== Event Flags definitions ====
 
/// Event Flags Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  uint32_t                event_flags;  ///< Event Flags
} osRtxEventFlags_t;
 
 
//  ==== Mutex definitions ====
 
/// Mutex Control Block
typedef struct osRtxMutex_s {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                        attr;  ///< Object Attributes
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  osRtxThread_t         *owner_thread;  ///< Owner Thread
  struct osRtxMutex_s     *owner_prev;  ///< Pointer to previous owned Mutex
  struct osRtxMutex_s     *owner_next;  ///< Pointer to next owned Mutex
  uint8_t                        lock;  ///< Lock counter
  uint8_t                  padding[3];
} osRtxMutex_t;
 
 
//  ==== Semaphore definitions ====
 
/// Semaphore Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  uint16_t                     tokens;  ///< Current number of tokens
  uint16_t                 max_tokens;  ///< Maximum number of tokens
} osRtxSemaphore_t;
 
 
//  ==== Memory Pool definitions ====
 
/// Memory Pool Information
typedef struct {
  uint32_t                 max_blocks;  ///< Maximum number of Blocks
  uint32_t                used_blocks;  ///< Number of used Blocks
  uint32_t                 block_size;  ///< Block Size
  void                    *block_base;  ///< Block Memory Base Address
  void                     *block_lim;  ///< Block Memory Limit Address
  void                    *block_free;  ///< First free Block Address
} osRtxMpInfo_t;
 
/// Memory Pool Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  osRtxMpInfo_t               mp_info;  ///< Memory Pool Info
} osRtxMemoryPool_t;
 
 
//  ==== Message Queue definitions ====
 
/// Message Control Block
typedef struct osRtxMessage_s {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    priority;  ///< Message Priority
  struct osRtxMessage_s         *prev;  ///< Pointer to previous Message
  struct osRtxMessage_s         *next;  ///< Pointer to next Message
} osRtxMessage_t;
 
/// Message Queue Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  osRtxMpInfo_t               mp_info;  ///< Memory Pool Info
  uint32_t                   msg_size;  ///< Message Size
  uint32_t                  msg_count;  ///< Number of queued Messages
  osRtxMessage_t           *msg_first;  ///< Pointer to first Message
  osRtxMessage_t            *msg_last;  ///< Pointer to last Message
} osRtxMessageQueue_t;
 
 
//  ==== Generic Object definitions ====
 
/// Generic Object Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t                       state;  ///< Object State
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Threads List
} osRtxObject_t;
 
 
//  ==== OS Runtime Information definitions ====
 
/// OS Runtime Information structure
typedef struct {
  const char                   *os_id;  ///< OS Identification
  uint32_t                    version;  ///< OS Version
  struct {                              ///< Kernel Info
    uint8_t                     state;  ///< State
    volatile uint8_t          blocked;  ///< Blocked
    uint8_t                    pendSV;  ///< Pending SV
    uint8_t                  reserved;
    uint32_t                     tick;  ///< Tick counter
  } kernel;
  int32_t                   tick_irqn;  ///< Tick Timer IRQ Number
  struct {                              ///< Thread Info
    struct {                            ///< Thread Run Info
      osRtxThread_t             *curr;  ///< Current running Thread
      osRtxThread_t             *next;  ///< Next Thread to Run
    } run;
    osRtxObject_t               ready;  ///< Ready List Object
    osRtxThread_t               *idle;  ///< Idle Thread
    osRtxThread_t         *delay_list;  ///< Delay List
    osRtxThread_t          *wait_list;  ///< Wait List (no Timeout)
    osRtxThread_t     *terminate_list;  ///< Terminate Thread List
    struct {                            ///< Thread Round Robin Info
      osRtxThread_t           *thread;  ///< Round Robin Thread
      uint32_t                   tick;  ///< Round Robin Time Tick
      uint32_t                timeout;  ///< Round Robin Timeout
    } robin;
  } thread;
  struct {                              ///< Timer Info
    osRtxTimer_t                *list;  ///< Active Timer List
    osRtxThread_t             *thread;  ///< Timer Thread
    osRtxMessageQueue_t           *mq;  ///< Timer Message Queue
    void                (*tick)(void);  ///< Timer Tick Function
  } timer;
  struct {                              ///< ISR Post Processing Queue
    uint16_t                      max;  ///< Maximum Items
    uint16_t                      cnt;  ///< Item Count
    uint16_t                       in;  ///< Incoming Item Index
    uint16_t                      out;  ///< Outgoing Item Index
    void                       **data;  ///< Queue Data
  } isr_queue;
  struct {                                      ///< ISR Post Processing functions
    void          (*thread)(osRtxThread_t*);    ///< Thread Post Processing function
    void (*event_flags)(osRtxEventFlags_t*);    ///< Event Flags Post Processing function
    void    (*semaphore)(osRtxSemaphore_t*);    ///< Semaphore Post Processing function
    void (*memory_pool)(osRtxMemoryPool_t*);    ///< Memory Pool Post Processing function
    void        (*message)(osRtxMessage_t*);    ///< Message Post Processing function
  } post_process;
  struct {                              ///< Memory Pools (Variable Block Size)
    void                       *stack;  ///< Stack Memory
    void                     *mp_data;  ///< Memory Pool Data Memory
    void                     *mq_data;  ///< Message Queue Data Memory
    void                      *common;  ///< Common Memory
  } mem;
  struct {                              ///< Memory Pools (Fixed Block Size)
    osRtxMpInfo_t              *stack;  ///< Stack for Threads
    osRtxMpInfo_t             *thread;  ///< Thread Control Blocks
    osRtxMpInfo_t              *timer;  ///< Timer Control Blocks
    osRtxMpInfo_t        *event_flags;  ///< Event Flags Control Blocks
    osRtxMpInfo_t              *mutex;  ///< Mutex Control Blocks
    osRtxMpInfo_t          *semaphore;  ///< Semaphore Control Blocks
    osRtxMpInfo_t        *memory_pool;  ///< Memory Pool Control Blocks
    osRtxMpInfo_t      *message_queue;  ///< Message Queue Control Blocks
  } mpi;
} osRtxInfo_t;
 
extern osRtxInfo_t osRtxInfo;           ///< OS Runtime Information
 
/// OS Runtime Object Memory Usage structure
typedef struct {
  uint32_t cnt_alloc;                   ///< Counter for alloc
  uint32_t cnt_free;                    ///< Counter for free
  uint32_t max_used;                    ///< Maximum used
} osRtxObjectMemUsage_t;
 
/// OS Runtime Object Memory Usage variables
extern osRtxObjectMemUsage_t osRtxThreadMemUsage;
extern osRtxObjectMemUsage_t osRtxTimerMemUsage;
extern osRtxObjectMemUsage_t osRtxEventFlagsMemUsage;
extern osRtxObjectMemUsage_t osRtxMutexMemUsage;
extern osRtxObjectMemUsage_t osRtxSemaphoreMemUsage;
extern osRtxObjectMemUsage_t osRtxMemoryPoolMemUsage;
extern osRtxObjectMemUsage_t osRtxMessageQueueMemUsage;
 
 
//  ==== OS API definitions ====
 
// Object Limits definitions
#define osRtxThreadFlagsLimit    31U    ///< number of Thread Flags available per thread
#define osRtxEventFlagsLimit     31U    ///< number of Event Flags available per object
#define osRtxMutexLockLimit      255U   ///< maximum number of recursive mutex locks
#define osRtxSemaphoreTokenLimit 65535U ///< maximum number of tokens per semaphore
 
// Control Block sizes
#define osRtxThreadCbSize        sizeof(osRtxThread_t)
#define osRtxTimerCbSize         sizeof(osRtxTimer_t)
#define osRtxEventFlagsCbSize    sizeof(osRtxEventFlags_t)
#define osRtxMutexCbSize         sizeof(osRtxMutex_t)
#define osRtxSemaphoreCbSize     sizeof(osRtxSemaphore_t)
#define osRtxMemoryPoolCbSize    sizeof(osRtxMemoryPool_t)
#define osRtxMessageQueueCbSize  sizeof(osRtxMessageQueue_t)
 
/// Memory size in bytes for Memory Pool storage.
/// \param         block_count   maximum number of memory blocks in memory pool.
/// \param         block_size    memory block size in bytes.
#define osRtxMemoryPoolMemSize(block_count, block_size) \
  (4*(block_count)*(((block_size)+3)/4))
 
/// Memory size in bytes for Message Queue storage.
/// \param         msg_count     maximum number of messages in queue.
/// \param         msg_size      maximum message size in bytes.
#define osRtxMessageQueueMemSize(msg_count, msg_size) \
  (4*(msg_count)*(3+(((msg_size)+3)/4)))
 
 
//  ==== OS External Functions ====
 
// OS Error Codes
#define osRtxErrorStackUnderflow        1U  ///< Stack overflow, i.e. stack pointer below its lower memory limit for descending stacks.
#define osRtxErrorISRQueueOverflow      2U  ///< ISR Queue overflow detected when inserting object.
#define osRtxErrorTimerQueueOverflow    3U  ///< User Timer Callback Queue overflow detected for timer.
#define osRtxErrorClibSpace             4U  ///< Standard C/C++ library libspace not available: increase \c OS_THREAD_LIBSPACE_NUM.
#define osRtxErrorClibMutex             5U  ///< Standard C/C++ library mutex initialization failed.
 
/// OS Error Callback function
extern uint32_t osRtxErrorNotify (uint32_t code, void *object_id);
 
/// OS Idle Thread
extern void osRtxIdleThread (void *argument);
 
/// OS Exception handlers
extern void SVC_Handler     (void);
extern void PendSV_Handler  (void);
extern void SysTick_Handler (void);
 
/// OS Trusted Firmware M Extension
#ifdef RTX_TF_M_EXTENSION
extern uint32_t osRtxTzGetModuleId (void);
#endif
 
 
//  ==== OS External Configuration ====
 
/// OS Configuration flags
#define osRtxConfigPrivilegedMode   (1UL<<0)    ///< Threads in Privileged mode
#define osRtxConfigStackCheck       (1UL<<1)    ///< Stack overrun checking
#define osRtxConfigStackWatermark   (1UL<<2)    ///< Stack usage Watermark
 
/// OS Configuration structure
typedef struct {
  uint32_t                             flags;   ///< OS Configuration Flags
  uint32_t                         tick_freq;   ///< Kernel Tick Frequency
  uint32_t                     robin_timeout;   ///< Round Robin Timeout Tick
  struct {                                      ///< ISR Post Processing Queue
    void                              **data;   ///< Queue Data
    uint16_t                             max;   ///< Maximum Items
    uint16_t                         padding;
  } isr_queue;
  struct {                                      ///< Memory Pools (Variable Block Size)
    void                         *stack_addr;   ///< Stack Memory Address
    uint32_t                      stack_size;   ///< Stack Memory Size
    void                       *mp_data_addr;   ///< Memory Pool Memory Address
    uint32_t                    mp_data_size;   ///< Memory Pool Memory Size
    void                       *mq_data_addr;   ///< Message Queue Data Memory Address
    uint32_t                    mq_data_size;   ///< Message Queue Data Memory Size
    void                        *common_addr;   ///< Common Memory Address
    uint32_t                     common_size;   ///< Common Memory Size
  } mem;
  struct {                                      ///< Memory Pools (Fixed Block Size)
    osRtxMpInfo_t                     *stack;   ///< Stack for Threads
    osRtxMpInfo_t                    *thread;   ///< Thread Control Blocks
    osRtxMpInfo_t                     *timer;   ///< Timer Control Blocks
    osRtxMpInfo_t               *event_flags;   ///< Event Flags Control Blocks
    osRtxMpInfo_t                     *mutex;   ///< Mutex Control Blocks
    osRtxMpInfo_t                 *semaphore;   ///< Semaphore Control Blocks
    osRtxMpInfo_t               *memory_pool;   ///< Memory Pool Control Blocks
    osRtxMpInfo_t             *message_queue;   ///< Message Queue Control Blocks
  } mpi;
  uint32_t                 thread_stack_size;   ///< Default Thread Stack Size
  const 
  osThreadAttr_t           *idle_thread_attr;   ///< Idle Thread Attributes
  const
  osThreadAttr_t          *timer_thread_attr;   ///< Timer Thread Attributes
  const
  osMessageQueueAttr_t        *timer_mq_attr;   ///< Timer Message Queue Attributes
  uint32_t                     timer_mq_mcnt;   ///< Timer Message Queue maximum Messages
} osRtxConfig_t;
 
extern const osRtxConfig_t osRtxConfig;         ///< OS Configuration
 
 
#ifdef  __cplusplus
}
#endif
 
#endif  // RTX_OS_H_
