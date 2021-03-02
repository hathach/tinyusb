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
 * Title:       System functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  ==== Helper functions ====

/// Put Object into ISR Queue.
/// \param[in]  object          object.
/// \return 1 - success, 0 - failure.
static uint32_t isr_queue_put (os_object_t *object) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask = __get_PRIMASK();
#else
  uint32_t n;
#endif
  uint16_t max;
  uint32_t ret;

  max = osRtxInfo.isr_queue.max;

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  if (osRtxInfo.isr_queue.cnt < max) {
    osRtxInfo.isr_queue.cnt++;
    osRtxInfo.isr_queue.data[osRtxInfo.isr_queue.in] = object;
    if (++osRtxInfo.isr_queue.in == max) {
      osRtxInfo.isr_queue.in = 0U;
    }
    ret = 1U;
  } else {
    ret = 0U;
  }
  
  if (primask == 0U) {
    __enable_irq();
  }
#else
  if (atomic_inc16_lt(&osRtxInfo.isr_queue.cnt, max) < max) {
    n = atomic_inc16_lim(&osRtxInfo.isr_queue.in, max);
    osRtxInfo.isr_queue.data[n] = object;
    ret = 1U;
  } else {
    ret = 0U;
  }
#endif

  return ret;
}

/// Get Object from ISR Queue.
/// \return object or NULL.
static os_object_t *isr_queue_get (void) {
#if (EXCLUSIVE_ACCESS != 0)
  uint32_t     n;
#endif
  uint16_t     max;
  os_object_t *ret;

  max = osRtxInfo.isr_queue.max;

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  if (osRtxInfo.isr_queue.cnt != 0U) {
    osRtxInfo.isr_queue.cnt--;
    ret = osRtxObject(osRtxInfo.isr_queue.data[osRtxInfo.isr_queue.out]);
    if (++osRtxInfo.isr_queue.out == max) {
      osRtxInfo.isr_queue.out = 0U;
    }
  } else {
    ret = NULL;
  }

  __enable_irq();
#else
  if (atomic_dec16_nz(&osRtxInfo.isr_queue.cnt) != 0U) {
    n = atomic_inc16_lim(&osRtxInfo.isr_queue.out, max);
    ret = osRtxObject(osRtxInfo.isr_queue.data[n]);
  } else {
    ret = NULL;
  }
#endif

  return ret;
}


//  ==== Library Functions ====

/// Tick Handler.
//lint -esym(714,osRtxTick_Handler) "Referenced by Exception handlers"
//lint -esym(759,osRtxTick_Handler) "Prototype in header"
//lint -esym(765,osRtxTick_Handler) "Global scope"
void osRtxTick_Handler (void) {
  os_thread_t *thread;

  OS_Tick_AcknowledgeIRQ();
  osRtxInfo.kernel.tick++;

  // Process Timers
  if (osRtxInfo.timer.tick != NULL) {
    osRtxInfo.timer.tick();
  }

  // Process Thread Delays
  osRtxThreadDelayTick();

  osRtxThreadDispatch(NULL);

  // Check Round Robin timeout
  if (osRtxInfo.thread.robin.timeout != 0U) {
    if (osRtxInfo.thread.robin.thread != osRtxInfo.thread.run.next) {
      // Reset Round Robin
      osRtxInfo.thread.robin.thread = osRtxInfo.thread.run.next;
      osRtxInfo.thread.robin.tick   = osRtxInfo.thread.robin.timeout;
    } else {
      if (osRtxInfo.thread.robin.tick != 0U) {
        osRtxInfo.thread.robin.tick--;
      }
      if (osRtxInfo.thread.robin.tick == 0U) {
        // Round Robin Timeout
        if (osRtxKernelGetState() == osRtxKernelRunning) {
          thread = osRtxInfo.thread.ready.thread_list;
          if ((thread != NULL) && (thread->priority == osRtxInfo.thread.robin.thread->priority)) {
            osRtxThreadListRemove(thread);
            osRtxThreadReadyPut(osRtxInfo.thread.robin.thread);
            EvrRtxThreadPreempted(osRtxInfo.thread.robin.thread);
            osRtxThreadSwitch(thread);
            osRtxInfo.thread.robin.thread = thread;
            osRtxInfo.thread.robin.tick   = osRtxInfo.thread.robin.timeout;
          }
        }
      }
    }
  }
}

/// Pending Service Call Handler.
//lint -esym(714,osRtxPendSV_Handler) "Referenced by Exception handlers"
//lint -esym(759,osRtxPendSV_Handler) "Prototype in header"
//lint -esym(765,osRtxPendSV_Handler) "Global scope"
void osRtxPendSV_Handler (void) {
  os_object_t *object;

  for (;;) {
    object = isr_queue_get();
    if (object == NULL) {
      break;
    }
    switch (object->id) {
      case osRtxIdThread:
        osRtxInfo.post_process.thread(osRtxThreadObject(object));
        break;
      case osRtxIdEventFlags:
        osRtxInfo.post_process.event_flags(osRtxEventFlagsObject(object));
        break;
      case osRtxIdSemaphore:
        osRtxInfo.post_process.semaphore(osRtxSemaphoreObject(object));
        break;
      case osRtxIdMemoryPool:
        osRtxInfo.post_process.memory_pool(osRtxMemoryPoolObject(object));
        break;
      case osRtxIdMessage:
        osRtxInfo.post_process.message(osRtxMessageObject(object));
        break;
      default:
        // Should never come here
        break;
    }
  }

  osRtxThreadDispatch(NULL);
}

/// Register post ISR processing.
/// \param[in]  object          generic object.
void osRtxPostProcess (os_object_t *object) {

  if (isr_queue_put(object) != 0U) {
    if (osRtxInfo.kernel.blocked == 0U) {
      SetPendSV();
    } else {
      osRtxInfo.kernel.pendSV = 1U;
    }
  } else {
    (void)osRtxErrorNotify(osRtxErrorISRQueueOverflow, object);
  }
}
