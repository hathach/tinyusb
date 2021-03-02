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
 * Title:       Message Queue functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  OS Runtime Object Memory Usage
#if ((defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0)))
osRtxObjectMemUsage_t osRtxMessageQueueMemUsage \
__attribute__((section(".data.os.msgqueue.obj"))) =
{ 0U, 0U, 0U };
#endif


//  ==== Helper functions ====

/// Put a Message into Queue sorted by Priority (Highest at Head).
/// \param[in]  mq              message queue object.
/// \param[in]  msg             message object.
static void MessageQueuePut (os_message_queue_t *mq, os_message_t *msg) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t      primask = __get_PRIMASK();
#endif
  os_message_t *prev, *next;

  if (mq->msg_last != NULL) {
    prev = mq->msg_last;
    next = NULL;
    while ((prev != NULL) && (prev->priority < msg->priority)) {
      next = prev;
      prev = prev->prev;
    }
    msg->prev = prev;
    msg->next = next;
    if (prev != NULL) {
      prev->next = msg;
    } else {
      mq->msg_first = msg;
    }
    if (next != NULL) {
      next->prev = msg;
    } else {
      mq->msg_last = msg;
    }
  } else {
    msg->prev = NULL;
    msg->next = NULL;
    mq->msg_first= msg;
    mq->msg_last = msg;
  }

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  mq->msg_count++;

  if (primask == 0U) {
    __enable_irq();
  }
#else
  (void)atomic_inc32(&mq->msg_count);
#endif
}

/// Get a Message from Queue with Highest Priority.
/// \param[in]  mq              message queue object.
/// \return message object or NULL.
static os_message_t *MessageQueueGet (os_message_queue_t *mq) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t      primask = __get_PRIMASK();
#endif
  os_message_t *msg;
  uint32_t      count;
  uint8_t       flags;

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  count = mq->msg_count;
  if (count != 0U) {
    mq->msg_count--;
  }

  if (primask == 0U) {
    __enable_irq();
  }
#else
  count = atomic_dec32_nz(&mq->msg_count);
#endif

  if (count != 0U) {
    msg = mq->msg_first;

    while (msg != NULL) {
#if (EXCLUSIVE_ACCESS == 0)
      __disable_irq();

      flags = msg->flags;
      msg->flags = 1U;

      if (primask == 0U) {
        __enable_irq();
      }
#else
      flags = atomic_wr8(&msg->flags, 1U);
#endif
      if (flags == 0U) {
        break;
      }
      msg = msg->next;
    }
  } else {
    msg = NULL;
  }

  return msg;
}

/// Remove a Message from Queue
/// \param[in]  mq              message queue object.
/// \param[in]  msg             message object.
static void MessageQueueRemove (os_message_queue_t *mq, const os_message_t *msg) {

  if (msg->prev != NULL) {
    msg->prev->next = msg->next;
  } else {
    mq->msg_first = msg->next;
  }
  if (msg->next != NULL) {
    msg->next->prev = msg->prev;
  } else {
    mq->msg_last = msg->prev;
  }
}


//  ==== Post ISR processing ====

/// Message Queue post ISR processing.
/// \param[in]  msg             message object.
static void osRtxMessageQueuePostProcess (os_message_t *msg) {
  os_message_queue_t *mq;
  os_message_t       *msg0;
  os_thread_t        *thread;
  const uint32_t     *reg;
  const void         *ptr_src;
        void         *ptr_dst;

  if (msg->flags != 0U) {
    // Remove Message
    //lint -e{9079} -e{9087} "cast between pointers to different object types"
    mq = *((os_message_queue_t **)(void *)&msg[1]);
    MessageQueueRemove(mq, msg);
    // Free memory
    msg->id = osRtxIdInvalid;
    (void)osRtxMemoryPoolFree(&mq->mp_info, msg);
    // Check if Thread is waiting to send a Message
    if (mq->thread_list != NULL) {
      // Try to allocate memory
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      msg0 = osRtxMemoryPoolAlloc(&mq->mp_info);
      if (msg0 != NULL) {
        // Wakeup waiting Thread with highest Priority
        thread = osRtxThreadListGet(osRtxObject(mq));
        osRtxThreadWaitExit(thread, (uint32_t)osOK, FALSE);
        // Copy Message (R2: const void *msg_ptr, R3: uint8_t msg_prio)
        reg = osRtxThreadRegPtr(thread);
        //lint -e{923} "cast from unsigned int to pointer"
        ptr_src = (const void *)reg[2];
        memcpy(&msg0[1], ptr_src, mq->msg_size);
        // Store Message into Queue
        msg0->id       = osRtxIdMessage;
        msg0->flags    = 0U;
        msg0->priority = (uint8_t)reg[3];
        MessageQueuePut(mq, msg0);
        EvrRtxMessageQueueInserted(mq, ptr_src);
      }
    }
  } else {
    // New Message
    //lint -e{9079} -e{9087} "cast between pointers to different object types"
    mq = (void *)msg->next;
    //lint -e{9087} "cast between pointers to different object types"
    ptr_src = (const void *)msg->prev;
    // Check if Thread is waiting to receive a Message
    if ((mq->thread_list != NULL) && (mq->thread_list->state == osRtxThreadWaitingMessageGet)) {
      EvrRtxMessageQueueInserted(mq, ptr_src);
      // Wakeup waiting Thread with highest Priority
      thread = osRtxThreadListGet(osRtxObject(mq));
      osRtxThreadWaitExit(thread, (uint32_t)osOK, FALSE);
      // Copy Message (R2: void *msg_ptr, R3: uint8_t *msg_prio)
      reg = osRtxThreadRegPtr(thread);
      //lint -e{923} "cast from unsigned int to pointer"
      ptr_dst = (void *)reg[2];
      memcpy(ptr_dst, &msg[1], mq->msg_size);
      if (reg[3] != 0U) {
        //lint -e{923} -e{9078} "cast from unsigned int to pointer"
        *((uint8_t *)reg[3]) = msg->priority;
      }
      EvrRtxMessageQueueRetrieved(mq, ptr_dst);
      // Free memory
      msg->id = osRtxIdInvalid;
      (void)osRtxMemoryPoolFree(&mq->mp_info, msg);
    } else {
      EvrRtxMessageQueueInserted(mq, ptr_src);
      MessageQueuePut(mq, msg);
    }
  }
}


//  ==== Service Calls ====

/// Create and Initialize a Message Queue object.
/// \note API identical to osMessageQueueNew
static osMessageQueueId_t svcRtxMessageQueueNew (uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr) {
  os_message_queue_t *mq;
  void               *mq_mem;
  uint32_t            mq_size;
  uint32_t            block_size;
  uint32_t            size;
  uint8_t             flags;
  const char         *name;

  // Check parameters
  if ((msg_count == 0U) || (msg_size  == 0U)) {
    EvrRtxMessageQueueError(NULL, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }
  block_size = ((msg_size + 3U) & ~3UL) + sizeof(os_message_t);
  if ((__CLZ(msg_count) + __CLZ(block_size)) < 32U) {
    EvrRtxMessageQueueError(NULL, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  size = msg_count * block_size;

  // Process attributes
  if (attr != NULL) {
    name    = attr->name;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    mq      = attr->cb_mem;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    mq_mem  = attr->mq_mem;
    mq_size = attr->mq_size;
    if (mq != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)mq & 3U) != 0U) || (attr->cb_size < sizeof(os_message_queue_t))) {
        EvrRtxMessageQueueError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    } else {
      if (attr->cb_size != 0U) {
        EvrRtxMessageQueueError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
    if (mq_mem != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)mq_mem & 3U) != 0U) || (mq_size < size)) {
        EvrRtxMessageQueueError(NULL, osRtxErrorInvalidDataMemory);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    } else {
      if (mq_size != 0U) {
        EvrRtxMessageQueueError(NULL, osRtxErrorInvalidDataMemory);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
  } else {
    name   = NULL;
    mq     = NULL;
    mq_mem = NULL;
  }

  // Allocate object memory if not provided
  if (mq == NULL) {
    if (osRtxInfo.mpi.message_queue != NULL) {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      mq = osRtxMemoryPoolAlloc(osRtxInfo.mpi.message_queue);
    } else {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      mq = osRtxMemoryAlloc(osRtxInfo.mem.common, sizeof(os_message_queue_t), 1U);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    if (mq != NULL) {
      uint32_t used;
      osRtxMessageQueueMemUsage.cnt_alloc++;
      used = osRtxMessageQueueMemUsage.cnt_alloc - osRtxMessageQueueMemUsage.cnt_free;
      if (osRtxMessageQueueMemUsage.max_used < used) {
        osRtxMessageQueueMemUsage.max_used = used;
      }
    }
#endif
    flags = osRtxFlagSystemObject;
  } else {
    flags = 0U;
  }

  // Allocate data memory if not provided
  if ((mq != NULL) && (mq_mem == NULL)) {
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
    mq_mem = osRtxMemoryAlloc(osRtxInfo.mem.mq_data, size, 0U);
    if (mq_mem == NULL) {
      if ((flags & osRtxFlagSystemObject) != 0U) {
        if (osRtxInfo.mpi.message_queue != NULL) {
          (void)osRtxMemoryPoolFree(osRtxInfo.mpi.message_queue, mq);
        } else {
          (void)osRtxMemoryFree(osRtxInfo.mem.common, mq);
        }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
        osRtxMessageQueueMemUsage.cnt_free++;
#endif
      }
      mq = NULL;
    } else {
      memset(mq_mem, 0, size);
    }
    flags |= osRtxFlagSystemMemory;
  }

  if (mq != NULL) {
    // Initialize control block
    mq->id          = osRtxIdMessageQueue;
    mq->flags       = flags;
    mq->name        = name;
    mq->thread_list = NULL;
    mq->msg_size    = msg_size;
    mq->msg_count   = 0U;
    mq->msg_first   = NULL;
    mq->msg_last    = NULL;
    (void)osRtxMemoryPoolInit(&mq->mp_info, msg_count, block_size, mq_mem);

    // Register post ISR processing function
    osRtxInfo.post_process.message = osRtxMessageQueuePostProcess;

    EvrRtxMessageQueueCreated(mq, mq->name);
  } else {
    EvrRtxMessageQueueError(NULL, (int32_t)osErrorNoMemory);
  }

  return mq;
}

/// Get name of a Message Queue object.
/// \note API identical to osMessageQueueGetName
static const char *svcRtxMessageQueueGetName (osMessageQueueId_t mq_id) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue)) {
    EvrRtxMessageQueueGetName(mq, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  EvrRtxMessageQueueGetName(mq, mq->name);

  return mq->name;
}

/// Put a Message into a Queue or timeout if Queue is full.
/// \note API identical to osMessageQueuePut
static osStatus_t svcRtxMessageQueuePut (osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);
  os_message_t       *msg;
  os_thread_t        *thread;
  uint32_t           *reg;
  void               *ptr;
  osStatus_t          status;

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue) || (msg_ptr == NULL)) {
    EvrRtxMessageQueueError(mq, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Check if Thread is waiting to receive a Message
  if ((mq->thread_list != NULL) && (mq->thread_list->state == osRtxThreadWaitingMessageGet)) {
    EvrRtxMessageQueueInserted(mq, msg_ptr);
    // Wakeup waiting Thread with highest Priority
    thread = osRtxThreadListGet(osRtxObject(mq));
    osRtxThreadWaitExit(thread, (uint32_t)osOK, TRUE);
    // Copy Message (R2: void *msg_ptr, R3: uint8_t *msg_prio)
    reg = osRtxThreadRegPtr(thread);
    //lint -e{923} "cast from unsigned int to pointer"
    ptr = (void *)reg[2];
    memcpy(ptr, msg_ptr, mq->msg_size);
    if (reg[3] != 0U) {
      //lint -e{923} -e{9078} "cast from unsigned int to pointer"
      *((uint8_t *)reg[3]) = msg_prio;
    }
    EvrRtxMessageQueueRetrieved(mq, ptr);
    status = osOK;
  } else {
    // Try to allocate memory
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
    msg = osRtxMemoryPoolAlloc(&mq->mp_info);
    if (msg != NULL) {
      // Copy Message
      memcpy(&msg[1], msg_ptr, mq->msg_size);
      // Put Message into Queue
      msg->id       = osRtxIdMessage;
      msg->flags    = 0U;
      msg->priority = msg_prio;
      MessageQueuePut(mq, msg);
      EvrRtxMessageQueueInserted(mq, msg_ptr);
      status = osOK;
    } else {
      // No memory available
      if (timeout != 0U) {
        EvrRtxMessageQueuePutPending(mq, msg_ptr, timeout);
        // Suspend current Thread
        if (osRtxThreadWaitEnter(osRtxThreadWaitingMessagePut, timeout)) {
          osRtxThreadListPut(osRtxObject(mq), osRtxThreadGetRunning());
          // Save arguments (R2: const void *msg_ptr, R3: uint8_t msg_prio)
          //lint -e{923} -e{9078} "cast from unsigned int to pointer"
          reg = (uint32_t *)(__get_PSP());
          //lint -e{923} -e{9078} "cast from pointer to unsigned int"
          reg[2] = (uint32_t)msg_ptr;
          //lint -e{923} -e{9078} "cast from pointer to unsigned int"
          reg[3] = (uint32_t)msg_prio;
        } else {
          EvrRtxMessageQueuePutTimeout(mq);
        }
        status = osErrorTimeout;
      } else {
        EvrRtxMessageQueueNotInserted(mq, msg_ptr);
        status = osErrorResource;
      }
    }
  }

  return status;
}

/// Get a Message from a Queue or timeout if Queue is empty.
/// \note API identical to osMessageQueueGet
static osStatus_t svcRtxMessageQueueGet (osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);
  os_message_t       *msg;
  os_thread_t        *thread;
  uint32_t           *reg;
  const void         *ptr;
  osStatus_t          status;

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue) || (msg_ptr == NULL)) {
    EvrRtxMessageQueueError(mq, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Get Message from Queue
  msg = MessageQueueGet(mq);
  if (msg != NULL) {
    MessageQueueRemove(mq, msg);
    // Copy Message
    memcpy(msg_ptr, &msg[1], mq->msg_size);
    if (msg_prio != NULL) {
      *msg_prio = msg->priority;
    }
    EvrRtxMessageQueueRetrieved(mq, msg_ptr);
    // Free memory
    msg->id = osRtxIdInvalid;
    (void)osRtxMemoryPoolFree(&mq->mp_info, msg);
    // Check if Thread is waiting to send a Message
    if (mq->thread_list != NULL) {
      // Try to allocate memory
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      msg = osRtxMemoryPoolAlloc(&mq->mp_info);
      if (msg != NULL) {
        // Wakeup waiting Thread with highest Priority
        thread = osRtxThreadListGet(osRtxObject(mq));
        osRtxThreadWaitExit(thread, (uint32_t)osOK, TRUE);
        // Copy Message (R2: const void *msg_ptr, R3: uint8_t msg_prio)
        reg = osRtxThreadRegPtr(thread);
        //lint -e{923} "cast from unsigned int to pointer"
        ptr = (const void *)reg[2];
        memcpy(&msg[1], ptr, mq->msg_size);
        // Store Message into Queue
        msg->id       = osRtxIdMessage;
        msg->flags    = 0U;
        msg->priority = (uint8_t)reg[3];
        MessageQueuePut(mq, msg);
        EvrRtxMessageQueueInserted(mq, ptr);
      }
    }
    status = osOK;
  } else {
    // No Message available
    if (timeout != 0U) {
      EvrRtxMessageQueueGetPending(mq, msg_ptr, timeout);
      // Suspend current Thread
      if (osRtxThreadWaitEnter(osRtxThreadWaitingMessageGet, timeout)) {
        osRtxThreadListPut(osRtxObject(mq), osRtxThreadGetRunning());
        // Save arguments (R2: void *msg_ptr, R3: uint8_t *msg_prio)
        //lint -e{923} -e{9078} "cast from unsigned int to pointer"
        reg = (uint32_t *)(__get_PSP());
        //lint -e{923} -e{9078} "cast from pointer to unsigned int"
        reg[2] = (uint32_t)msg_ptr;
        //lint -e{923} -e{9078} "cast from pointer to unsigned int"
        reg[3] = (uint32_t)msg_prio;
      } else {
        EvrRtxMessageQueueGetTimeout(mq);
      }
      status = osErrorTimeout;
    } else {
      EvrRtxMessageQueueNotRetrieved(mq, msg_ptr);
      status = osErrorResource;
    }
  }

  return status;
}

/// Get maximum number of messages in a Message Queue.
/// \note API identical to osMessageQueueGetCapacity
static uint32_t svcRtxMessageQueueGetCapacity (osMessageQueueId_t mq_id) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue)) {
    EvrRtxMessageQueueGetCapacity(mq, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxMessageQueueGetCapacity(mq, mq->mp_info.max_blocks);

  return mq->mp_info.max_blocks;
}

/// Get maximum message size in a Memory Pool.
/// \note API identical to osMessageQueueGetMsgSize
static uint32_t svcRtxMessageQueueGetMsgSize (osMessageQueueId_t mq_id) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue)) {
    EvrRtxMessageQueueGetMsgSize(mq, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxMessageQueueGetMsgSize(mq, mq->msg_size);

  return mq->msg_size;
}

/// Get number of queued messages in a Message Queue.
/// \note API identical to osMessageQueueGetCount
static uint32_t svcRtxMessageQueueGetCount (osMessageQueueId_t mq_id) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue)) {
    EvrRtxMessageQueueGetCount(mq, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxMessageQueueGetCount(mq, mq->msg_count);

  return mq->msg_count;
}

/// Get number of available slots for messages in a Message Queue.
/// \note API identical to osMessageQueueGetSpace
static uint32_t svcRtxMessageQueueGetSpace (osMessageQueueId_t mq_id) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue)) {
    EvrRtxMessageQueueGetSpace(mq, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxMessageQueueGetSpace(mq, mq->mp_info.max_blocks - mq->msg_count);

  return (mq->mp_info.max_blocks - mq->msg_count);
}

/// Reset a Message Queue to initial empty state.
/// \note API identical to osMessageQueueReset
static osStatus_t svcRtxMessageQueueReset (osMessageQueueId_t mq_id) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);
  os_message_t       *msg;
  os_thread_t        *thread;
  const uint32_t     *reg;
  const void         *ptr;

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue)) {
    EvrRtxMessageQueueError(mq, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Remove Messages from Queue
  for (;;) {
    // Get Message from Queue
    msg = MessageQueueGet(mq);
    if (msg == NULL) {
      break;
    }
    MessageQueueRemove(mq, msg);
    EvrRtxMessageQueueRetrieved(mq, NULL);
    // Free memory
    msg->id = osRtxIdInvalid;
    (void)osRtxMemoryPoolFree(&mq->mp_info, msg);
  }

  // Check if Threads are waiting to send Messages
  if ((mq->thread_list != NULL) && (mq->thread_list->state == osRtxThreadWaitingMessagePut)) {
    do {
      // Try to allocate memory
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      msg = osRtxMemoryPoolAlloc(&mq->mp_info);
      if (msg != NULL) {
        // Wakeup waiting Thread with highest Priority
        thread = osRtxThreadListGet(osRtxObject(mq));
        osRtxThreadWaitExit(thread, (uint32_t)osOK, FALSE);
        // Copy Message (R2: const void *msg_ptr, R3: uint8_t msg_prio)
        reg = osRtxThreadRegPtr(thread);
        //lint -e{923} "cast from unsigned int to pointer"
        ptr = (const void *)reg[2];
        memcpy(&msg[1], ptr, mq->msg_size);
        // Store Message into Queue
        msg->id       = osRtxIdMessage;
        msg->flags    = 0U;
        msg->priority = (uint8_t)reg[3];
        MessageQueuePut(mq, msg);
        EvrRtxMessageQueueInserted(mq, ptr);
      }
    } while ((msg != NULL) && (mq->thread_list != NULL));
    osRtxThreadDispatch(NULL);
  }

  EvrRtxMessageQueueResetDone(mq);

  return osOK;
}

/// Delete a Message Queue object.
/// \note API identical to osMessageQueueDelete
static osStatus_t svcRtxMessageQueueDelete (osMessageQueueId_t mq_id) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);
  os_thread_t        *thread;

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue)) {
    EvrRtxMessageQueueError(mq, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Unblock waiting threads
  if (mq->thread_list != NULL) {
    do {
      thread = osRtxThreadListGet(osRtxObject(mq));
      osRtxThreadWaitExit(thread, (uint32_t)osErrorResource, FALSE);
    } while (mq->thread_list != NULL);
    osRtxThreadDispatch(NULL);
  }

  // Mark object as invalid
  mq->id = osRtxIdInvalid;

  // Free data memory
  if ((mq->flags & osRtxFlagSystemMemory) != 0U) {
    (void)osRtxMemoryFree(osRtxInfo.mem.mq_data, mq->mp_info.block_base);
  }

  // Free object memory
  if ((mq->flags & osRtxFlagSystemObject) != 0U) {
    if (osRtxInfo.mpi.message_queue != NULL) {
      (void)osRtxMemoryPoolFree(osRtxInfo.mpi.message_queue, mq);
    } else {
      (void)osRtxMemoryFree(osRtxInfo.mem.common, mq);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    osRtxMessageQueueMemUsage.cnt_free++;
#endif
  }

  EvrRtxMessageQueueDestroyed(mq);

  return osOK;
}

//  Service Calls definitions
//lint ++flb "Library Begin" [MISRA Note 11]
SVC0_3(MessageQueueNew,         osMessageQueueId_t, uint32_t, uint32_t, const osMessageQueueAttr_t *)
SVC0_1(MessageQueueGetName,     const char *,       osMessageQueueId_t)
SVC0_4(MessageQueuePut,         osStatus_t,         osMessageQueueId_t, const void *, uint8_t,   uint32_t)
SVC0_4(MessageQueueGet,         osStatus_t,         osMessageQueueId_t,       void *, uint8_t *, uint32_t)
SVC0_1(MessageQueueGetCapacity, uint32_t,           osMessageQueueId_t)
SVC0_1(MessageQueueGetMsgSize,  uint32_t,           osMessageQueueId_t)
SVC0_1(MessageQueueGetCount,    uint32_t,           osMessageQueueId_t)
SVC0_1(MessageQueueGetSpace,    uint32_t,           osMessageQueueId_t)
SVC0_1(MessageQueueReset,       osStatus_t,         osMessageQueueId_t)
SVC0_1(MessageQueueDelete,      osStatus_t,         osMessageQueueId_t)
//lint --flb "Library End"


//  ==== ISR Calls ====

/// Put a Message into a Queue or timeout if Queue is full.
/// \note API identical to osMessageQueuePut
__STATIC_INLINE
osStatus_t isrRtxMessageQueuePut (osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);
  os_message_t       *msg;
  osStatus_t          status;

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue) || (msg_ptr == NULL) || (timeout != 0U)) {
    EvrRtxMessageQueueError(mq, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Try to allocate memory
  //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
  msg = osRtxMemoryPoolAlloc(&mq->mp_info);
  if (msg != NULL) {
    // Copy Message
    memcpy(&msg[1], msg_ptr, mq->msg_size);
    msg->id       = osRtxIdMessage;
    msg->flags    = 0U;
    msg->priority = msg_prio;
    // Register post ISR processing
    //lint -e{9079} -e{9087} "cast between pointers to different object types"
    *((const void **)(void *)&msg->prev) = msg_ptr;
    //lint -e{9079} -e{9087} "cast between pointers to different object types"
    *(      (void **)        &msg->next) = mq;
    osRtxPostProcess(osRtxObject(msg));
    EvrRtxMessageQueueInsertPending(mq, msg_ptr);
    status = osOK;
  } else {
    // No memory available
    EvrRtxMessageQueueNotInserted(mq, msg_ptr);
    status = osErrorResource;
  }

  return status;
}

/// Get a Message from a Queue or timeout if Queue is empty.
/// \note API identical to osMessageQueueGet
__STATIC_INLINE
osStatus_t isrRtxMessageQueueGet (osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
  os_message_queue_t *mq = osRtxMessageQueueId(mq_id);
  os_message_t       *msg;
  osStatus_t          status;

  // Check parameters
  if ((mq == NULL) || (mq->id != osRtxIdMessageQueue) || (msg_ptr == NULL) || (timeout != 0U)) {
    EvrRtxMessageQueueError(mq, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Get Message from Queue
  msg = MessageQueueGet(mq);
  if (msg != NULL) {
    // Copy Message
    memcpy(msg_ptr, &msg[1], mq->msg_size);
    if (msg_prio != NULL) {
      *msg_prio = msg->priority;
    }
    // Register post ISR processing
    //lint -e{9079} -e{9087} "cast between pointers to different object types"
    *((os_message_queue_t **)(void *)&msg[1]) = mq;
    osRtxPostProcess(osRtxObject(msg));
    EvrRtxMessageQueueRetrieved(mq, msg_ptr);
    status = osOK;
  } else {
    // No Message available
    EvrRtxMessageQueueNotRetrieved(mq, msg_ptr);
    status = osErrorResource;
  }

  return status;
}


//  ==== Public API ====

/// Create and Initialize a Message Queue object.
osMessageQueueId_t osMessageQueueNew (uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr) {
  osMessageQueueId_t mq_id;

  EvrRtxMessageQueueNew(msg_count, msg_size, attr);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMessageQueueError(NULL, (int32_t)osErrorISR);
    mq_id = NULL;
  } else {
    mq_id = __svcMessageQueueNew(msg_count, msg_size, attr);
  }
  return mq_id;
}

/// Get name of a Message Queue object.
const char *osMessageQueueGetName (osMessageQueueId_t mq_id) {
  const char *name;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMessageQueueGetName(mq_id, NULL);
    name = NULL;
  } else {
    name = __svcMessageQueueGetName(mq_id);
  }
  return name;
}

/// Put a Message into a Queue or timeout if Queue is full.
osStatus_t osMessageQueuePut (osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout) {
  osStatus_t status;

  EvrRtxMessageQueuePut(mq_id, msg_ptr, msg_prio, timeout);
  if (IsIrqMode() || IsIrqMasked()) {
    status = isrRtxMessageQueuePut(mq_id, msg_ptr, msg_prio, timeout);
  } else {
    status =  __svcMessageQueuePut(mq_id, msg_ptr, msg_prio, timeout);
  }
  return status;
}

/// Get a Message from a Queue or timeout if Queue is empty.
osStatus_t osMessageQueueGet (osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
  osStatus_t status;

  EvrRtxMessageQueueGet(mq_id, msg_ptr, msg_prio, timeout);
  if (IsIrqMode() || IsIrqMasked()) {
    status = isrRtxMessageQueueGet(mq_id, msg_ptr, msg_prio, timeout);
  } else {
    status =  __svcMessageQueueGet(mq_id, msg_ptr, msg_prio, timeout);
  }
  return status;
}

/// Get maximum number of messages in a Message Queue.
uint32_t osMessageQueueGetCapacity (osMessageQueueId_t mq_id) {
  uint32_t capacity;

  if (IsIrqMode() || IsIrqMasked()) {
    capacity = svcRtxMessageQueueGetCapacity(mq_id);
  } else {
    capacity =  __svcMessageQueueGetCapacity(mq_id);
  }
  return capacity;
}

/// Get maximum message size in a Memory Pool.
uint32_t osMessageQueueGetMsgSize (osMessageQueueId_t mq_id) {
  uint32_t msg_size;

  if (IsIrqMode() || IsIrqMasked()) {
    msg_size = svcRtxMessageQueueGetMsgSize(mq_id);
  } else {
    msg_size =  __svcMessageQueueGetMsgSize(mq_id);
  }
  return msg_size;
}

/// Get number of queued messages in a Message Queue.
uint32_t osMessageQueueGetCount (osMessageQueueId_t mq_id) {
  uint32_t count;

  if (IsIrqMode() || IsIrqMasked()) {
    count = svcRtxMessageQueueGetCount(mq_id);
  } else {
    count =  __svcMessageQueueGetCount(mq_id);
  }
  return count;
}

/// Get number of available slots for messages in a Message Queue.
uint32_t osMessageQueueGetSpace (osMessageQueueId_t mq_id) {
  uint32_t space;

  if (IsIrqMode() || IsIrqMasked()) {
    space = svcRtxMessageQueueGetSpace(mq_id);
  } else {
    space =  __svcMessageQueueGetSpace(mq_id);
  }
  return space;
}

/// Reset a Message Queue to initial empty state.
osStatus_t osMessageQueueReset (osMessageQueueId_t mq_id) {
  osStatus_t status;

  EvrRtxMessageQueueReset(mq_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMessageQueueError(mq_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcMessageQueueReset(mq_id);
  }
  return status;
}

/// Delete a Message Queue object.
osStatus_t osMessageQueueDelete (osMessageQueueId_t mq_id) {
  osStatus_t status;

  EvrRtxMessageQueueDelete(mq_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMessageQueueError(mq_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcMessageQueueDelete(mq_id);
  }
  return status;
}
