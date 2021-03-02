/*
 * Copyright (c) 2013-2018 Arm Limited. All rights reserved.
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
 * Title:       Memory Pool functions
 *
 * -----------------------------------------------------------------------------
 */

#include "rtx_lib.h"


//  OS Runtime Object Memory Usage
#if ((defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0)))
osRtxObjectMemUsage_t osRtxMemoryPoolMemUsage \
__attribute__((section(".data.os.mempool.obj"))) =
{ 0U, 0U, 0U };
#endif


//  ==== Library functions ====

/// Initialize Memory Pool.
/// \param[in]  mp_info         memory pool info.
/// \param[in]  block_count     maximum number of memory blocks in memory pool.
/// \param[in]  block_size      size of a memory block in bytes.
/// \param[in]  block_mem       pointer to memory for block storage.
/// \return 1 - success, 0 - failure.
uint32_t osRtxMemoryPoolInit (os_mp_info_t *mp_info, uint32_t block_count, uint32_t block_size, void *block_mem) {
  //lint --e{9079} --e{9087} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
  void *mem;
  void *block;

  // Check parameters
  if ((mp_info == NULL) || (block_count == 0U) || (block_size  == 0U) || (block_mem  == NULL)) {
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  // Initialize information structure
  mp_info->max_blocks  = block_count;
  mp_info->used_blocks = 0U;
  mp_info->block_size  = block_size;
  mp_info->block_base  = block_mem;
  mp_info->block_free  = block_mem;
  mp_info->block_lim   = &(((uint8_t *)block_mem)[block_count * block_size]);

  EvrRtxMemoryBlockInit(mp_info, block_count, block_size, block_mem);

  // Link all free blocks
  mem = block_mem;
  while (--block_count != 0U) {
    block = &((uint8_t *)mem)[block_size];
    *((void **)mem) = block;
    mem = block;
  }
  *((void **)mem) = NULL;

  return 1U;
}

/// Allocate a memory block from a Memory Pool.
/// \param[in]  mp_info         memory pool info.
/// \return address of the allocated memory block or NULL in case of no memory is available.
void *osRtxMemoryPoolAlloc (os_mp_info_t *mp_info) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask = __get_PRIMASK();
#endif
  void *block;

  if (mp_info == NULL) {
    EvrRtxMemoryBlockAlloc(NULL, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  block = mp_info->block_free;
  if (block != NULL) {
    //lint --e{9079} --e{9087} "conversion from pointer to void to pointer to other type"
    mp_info->block_free = *((void **)block);
    mp_info->used_blocks++;
  }

  if (primask == 0U) {
    __enable_irq();
  }
#else
  block = atomic_link_get(&mp_info->block_free);
  if (block != NULL) {
    (void)atomic_inc32(&mp_info->used_blocks);
  }
#endif

  EvrRtxMemoryBlockAlloc(mp_info, block);

  return block;
}

/// Return an allocated memory block back to a Memory Pool.
/// \param[in]  mp_info         memory pool info.
/// \param[in]  block           address of the allocated memory block to be returned to the memory pool.
/// \return status code that indicates the execution status of the function.
osStatus_t osRtxMemoryPoolFree (os_mp_info_t *mp_info, void *block) {
#if (EXCLUSIVE_ACCESS == 0)
  uint32_t primask = __get_PRIMASK();
#endif

  //lint -e{946} "Relational operator applied to pointers"
  if ((mp_info == NULL) || (block < mp_info->block_base) || (block >= mp_info->block_lim)) {
    EvrRtxMemoryBlockFree(mp_info, block, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

#if (EXCLUSIVE_ACCESS == 0)
  __disable_irq();

  //lint --e{9079} --e{9087} "conversion from pointer to void to pointer to other type"
  *((void **)block) = mp_info->block_free;
  mp_info->block_free = block;
  mp_info->used_blocks--;

  if (primask == 0U) {
    __enable_irq();
  }
#else
  atomic_link_put(&mp_info->block_free, block);
  (void)atomic_dec32(&mp_info->used_blocks);
#endif

  EvrRtxMemoryBlockFree(mp_info, block, (int32_t)osOK);

  return osOK;
}


//  ==== Post ISR processing ====

/// Memory Pool post ISR processing.
/// \param[in]  mp              memory pool object.
static void osRtxMemoryPoolPostProcess (os_memory_pool_t *mp) {
  void        *block;
  os_thread_t *thread;

  // Check if Thread is waiting to allocate memory
  if (mp->thread_list != NULL) {
    // Allocate memory
    block = osRtxMemoryPoolAlloc(&mp->mp_info);
    if (block != NULL) {
      // Wakeup waiting Thread with highest Priority
      thread = osRtxThreadListGet(osRtxObject(mp));
      //lint -e{923} "cast from pointer to unsigned int"
      osRtxThreadWaitExit(thread, (uint32_t)block, FALSE);
      EvrRtxMemoryPoolAllocated(mp, block);
    }
  }
}


//  ==== Service Calls ====

/// Create and Initialize a Memory Pool object.
/// \note API identical to osMemoryPoolNew
static osMemoryPoolId_t svcRtxMemoryPoolNew (uint32_t block_count, uint32_t block_size, const osMemoryPoolAttr_t *attr) {
  os_memory_pool_t *mp;
  void             *mp_mem;
  uint32_t          mp_size;
  uint32_t          b_count;
  uint32_t          b_size;
  uint32_t          size;
  uint8_t           flags;
  const char       *name;

  // Check parameters
  if ((block_count == 0U) || (block_size  == 0U)) {
    EvrRtxMemoryPoolError(NULL, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }
  b_count =  block_count;
  b_size  = (block_size + 3U) & ~3UL;
  if ((__CLZ(b_count) + __CLZ(b_size)) < 32U) {
    EvrRtxMemoryPoolError(NULL, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  size = b_count * b_size;

  // Process attributes
  if (attr != NULL) {
    name    = attr->name;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    mp      = attr->cb_mem;
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    mp_mem  = attr->mp_mem;
    mp_size = attr->mp_size;
    if (mp != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)mp & 3U) != 0U) || (attr->cb_size < sizeof(os_memory_pool_t))) {
        EvrRtxMemoryPoolError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    } else {
      if (attr->cb_size != 0U) {
        EvrRtxMemoryPoolError(NULL, osRtxErrorInvalidControlBlock);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
    if (mp_mem != NULL) {
      //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
      if ((((uint32_t)mp_mem & 3U) != 0U) || (mp_size < size)) {
        EvrRtxMemoryPoolError(NULL, osRtxErrorInvalidDataMemory);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    } else {
      if (mp_size != 0U) {
        EvrRtxMemoryPoolError(NULL, osRtxErrorInvalidDataMemory);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return NULL;
      }
    }
  } else {
    name   = NULL;
    mp     = NULL;
    mp_mem = NULL;
  }

  // Allocate object memory if not provided
  if (mp == NULL) {
    if (osRtxInfo.mpi.memory_pool != NULL) {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      mp = osRtxMemoryPoolAlloc(osRtxInfo.mpi.memory_pool);
    } else {
      //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
      mp = osRtxMemoryAlloc(osRtxInfo.mem.common, sizeof(os_memory_pool_t), 1U);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    if (mp != NULL) {
      uint32_t used;
      osRtxMemoryPoolMemUsage.cnt_alloc++;
      used = osRtxMemoryPoolMemUsage.cnt_alloc - osRtxMemoryPoolMemUsage.cnt_free;
      if (osRtxMemoryPoolMemUsage.max_used < used) {
        osRtxMemoryPoolMemUsage.max_used = used;
      }
    }
#endif
    flags = osRtxFlagSystemObject;
  } else {
    flags = 0U;
  }

  // Allocate data memory if not provided
  if ((mp != NULL) && (mp_mem == NULL)) {
    //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
    mp_mem = osRtxMemoryAlloc(osRtxInfo.mem.mp_data, size, 0U);
    if (mp_mem == NULL) {
      if ((flags & osRtxFlagSystemObject) != 0U) {
        if (osRtxInfo.mpi.memory_pool != NULL) {
          (void)osRtxMemoryPoolFree(osRtxInfo.mpi.memory_pool, mp);
        } else {
          (void)osRtxMemoryFree(osRtxInfo.mem.common, mp);
        }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
        osRtxMemoryPoolMemUsage.cnt_free++;
#endif
      }
      mp = NULL;
    } else {
      memset(mp_mem, 0, size);
    }
    flags |= osRtxFlagSystemMemory;
  }

  if (mp != NULL) {
    // Initialize control block
    mp->id          = osRtxIdMemoryPool;
    mp->flags       = flags;
    mp->name        = name;
    mp->thread_list = NULL;
    (void)osRtxMemoryPoolInit(&mp->mp_info, b_count, b_size, mp_mem);

    // Register post ISR processing function
    osRtxInfo.post_process.memory_pool = osRtxMemoryPoolPostProcess;

    EvrRtxMemoryPoolCreated(mp, mp->name);
  } else {
    EvrRtxMemoryPoolError(NULL, (int32_t)osErrorNoMemory);
  }

  return mp;
}

/// Get name of a Memory Pool object.
/// \note API identical to osMemoryPoolGetName
static const char *svcRtxMemoryPoolGetName (osMemoryPoolId_t mp_id) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolGetName(mp, NULL);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  EvrRtxMemoryPoolGetName(mp, mp->name);

  return mp->name;
}

/// Allocate a memory block from a Memory Pool.
/// \note API identical to osMemoryPoolAlloc
static void *svcRtxMemoryPoolAlloc (osMemoryPoolId_t mp_id, uint32_t timeout) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);
  void             *block;

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolError(mp, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  // Allocate memory
  block = osRtxMemoryPoolAlloc(&mp->mp_info);
  if (block != NULL) {
    EvrRtxMemoryPoolAllocated(mp, block);
  } else {
    // No memory available
    if (timeout != 0U) {
      EvrRtxMemoryPoolAllocPending(mp, timeout);
      // Suspend current Thread
      if (osRtxThreadWaitEnter(osRtxThreadWaitingMemoryPool, timeout)) {
        osRtxThreadListPut(osRtxObject(mp), osRtxThreadGetRunning());
      } else {
        EvrRtxMemoryPoolAllocTimeout(mp);
      }
    } else {
      EvrRtxMemoryPoolAllocFailed(mp);
    }
  }

  return block;
}

/// Return an allocated memory block back to a Memory Pool.
/// \note API identical to osMemoryPoolFree
static osStatus_t svcRtxMemoryPoolFree (osMemoryPoolId_t mp_id, void *block) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);
  void             *block0;
  os_thread_t      *thread;
  osStatus_t        status;

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolError(mp, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Free memory
  status = osRtxMemoryPoolFree(&mp->mp_info, block);
  if (status == osOK) {
    EvrRtxMemoryPoolDeallocated(mp, block);
    // Check if Thread is waiting to allocate memory
    if (mp->thread_list != NULL) {
      // Allocate memory
      block0 = osRtxMemoryPoolAlloc(&mp->mp_info);
      if (block0 != NULL) {
        // Wakeup waiting Thread with highest Priority
        thread = osRtxThreadListGet(osRtxObject(mp));
        //lint -e{923} "cast from pointer to unsigned int"
        osRtxThreadWaitExit(thread, (uint32_t)block0, TRUE);
        EvrRtxMemoryPoolAllocated(mp, block0);
      }
    }
  } else {
    EvrRtxMemoryPoolFreeFailed(mp, block);
  }

  return status;
}

/// Get maximum number of memory blocks in a Memory Pool.
/// \note API identical to osMemoryPoolGetCapacity
static uint32_t svcRtxMemoryPoolGetCapacity (osMemoryPoolId_t mp_id) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolGetCapacity(mp, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxMemoryPoolGetCapacity(mp, mp->mp_info.max_blocks);

  return mp->mp_info.max_blocks;
}

/// Get memory block size in a Memory Pool.
/// \note API identical to osMemoryPoolGetBlockSize
static uint32_t svcRtxMemoryPoolGetBlockSize (osMemoryPoolId_t mp_id) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolGetBlockSize(mp, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxMemoryPoolGetBlockSize(mp, mp->mp_info.block_size);

  return mp->mp_info.block_size;
}

/// Get number of memory blocks used in a Memory Pool.
/// \note API identical to osMemoryPoolGetCount
static uint32_t svcRtxMemoryPoolGetCount (osMemoryPoolId_t mp_id) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolGetCount(mp, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxMemoryPoolGetCount(mp, mp->mp_info.used_blocks);

  return mp->mp_info.used_blocks;
}

/// Get number of memory blocks available in a Memory Pool.
/// \note API identical to osMemoryPoolGetSpace
static uint32_t svcRtxMemoryPoolGetSpace (osMemoryPoolId_t mp_id) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolGetSpace(mp, 0U);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return 0U;
  }

  EvrRtxMemoryPoolGetSpace(mp, mp->mp_info.max_blocks - mp->mp_info.used_blocks);

  return (mp->mp_info.max_blocks - mp->mp_info.used_blocks);
}

/// Delete a Memory Pool object.
/// \note API identical to osMemoryPoolDelete
static osStatus_t svcRtxMemoryPoolDelete (osMemoryPoolId_t mp_id) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);
  os_thread_t      *thread;

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolError(mp, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Unblock waiting threads
  if (mp->thread_list != NULL) {
    do {
      thread = osRtxThreadListGet(osRtxObject(mp));
      osRtxThreadWaitExit(thread, 0U, FALSE);
    } while (mp->thread_list != NULL);
    osRtxThreadDispatch(NULL);
  }

  // Mark object as invalid
  mp->id = osRtxIdInvalid;

  // Free data memory
  if ((mp->flags & osRtxFlagSystemMemory) != 0U) {
    (void)osRtxMemoryFree(osRtxInfo.mem.mp_data, mp->mp_info.block_base);
  }

  // Free object memory
  if ((mp->flags & osRtxFlagSystemObject) != 0U) {
    if (osRtxInfo.mpi.memory_pool != NULL) {
      (void)osRtxMemoryPoolFree(osRtxInfo.mpi.memory_pool, mp);
    } else {
      (void)osRtxMemoryFree(osRtxInfo.mem.common, mp);
    }
#if (defined(OS_OBJ_MEM_USAGE) && (OS_OBJ_MEM_USAGE != 0))
    osRtxMemoryPoolMemUsage.cnt_free++;
#endif
  }

  EvrRtxMemoryPoolDestroyed(mp);

  return osOK;
}

//  Service Calls definitions
//lint ++flb "Library Begin" [MISRA Note 11]
SVC0_3(MemoryPoolNew,          osMemoryPoolId_t, uint32_t, uint32_t, const osMemoryPoolAttr_t *)
SVC0_1(MemoryPoolGetName,      const char *,     osMemoryPoolId_t)
SVC0_2(MemoryPoolAlloc,        void *,           osMemoryPoolId_t, uint32_t)
SVC0_2(MemoryPoolFree,         osStatus_t,       osMemoryPoolId_t, void *)
SVC0_1(MemoryPoolGetCapacity,  uint32_t,         osMemoryPoolId_t)
SVC0_1(MemoryPoolGetBlockSize, uint32_t,         osMemoryPoolId_t)
SVC0_1(MemoryPoolGetCount,     uint32_t,         osMemoryPoolId_t)
SVC0_1(MemoryPoolGetSpace,     uint32_t,         osMemoryPoolId_t)
SVC0_1(MemoryPoolDelete,       osStatus_t,       osMemoryPoolId_t)
//lint --flb "Library End"


//  ==== ISR Calls ====

/// Allocate a memory block from a Memory Pool.
/// \note API identical to osMemoryPoolAlloc
__STATIC_INLINE
void *isrRtxMemoryPoolAlloc (osMemoryPoolId_t mp_id, uint32_t timeout) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);
  void             *block;

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool) || (timeout != 0U)) {
    EvrRtxMemoryPoolError(mp, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return NULL;
  }

  // Allocate memory
  block = osRtxMemoryPoolAlloc(&mp->mp_info);
  if (block == NULL) {
    EvrRtxMemoryPoolAllocFailed(mp);
  } else {
    EvrRtxMemoryPoolAllocated(mp, block);
  }

  return block;
}

/// Return an allocated memory block back to a Memory Pool.
/// \note API identical to osMemoryPoolFree
__STATIC_INLINE
osStatus_t isrRtxMemoryPoolFree (osMemoryPoolId_t mp_id, void *block) {
  os_memory_pool_t *mp = osRtxMemoryPoolId(mp_id);
  osStatus_t        status;

  // Check parameters
  if ((mp == NULL) || (mp->id != osRtxIdMemoryPool)) {
    EvrRtxMemoryPoolError(mp, (int32_t)osErrorParameter);
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return osErrorParameter;
  }

  // Free memory
  status = osRtxMemoryPoolFree(&mp->mp_info, block);
  if (status == osOK) {
    // Register post ISR processing
    osRtxPostProcess(osRtxObject(mp));
    EvrRtxMemoryPoolDeallocated(mp, block);
  } else {
    EvrRtxMemoryPoolFreeFailed(mp, block);
  }

  return status;
}


//  ==== Public API ====

/// Create and Initialize a Memory Pool object.
osMemoryPoolId_t osMemoryPoolNew (uint32_t block_count, uint32_t block_size, const osMemoryPoolAttr_t *attr) {
  osMemoryPoolId_t mp_id;

  EvrRtxMemoryPoolNew(block_count, block_size, attr);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMemoryPoolError(NULL, (int32_t)osErrorISR);
    mp_id = NULL;
  } else {
    mp_id = __svcMemoryPoolNew(block_count, block_size, attr);
  }
  return mp_id;
}

/// Get name of a Memory Pool object.
const char *osMemoryPoolGetName (osMemoryPoolId_t mp_id) {
  const char *name;

  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMemoryPoolGetName(mp_id, NULL);
    name = NULL;
  } else {
    name = __svcMemoryPoolGetName(mp_id);
  }
  return name;
}

/// Allocate a memory block from a Memory Pool.
void *osMemoryPoolAlloc (osMemoryPoolId_t mp_id, uint32_t timeout) {
  void *memory;

  EvrRtxMemoryPoolAlloc(mp_id, timeout);
  if (IsIrqMode() || IsIrqMasked()) {
    memory = isrRtxMemoryPoolAlloc(mp_id, timeout);
  } else {
    memory =  __svcMemoryPoolAlloc(mp_id, timeout);
  }
  return memory;
}

/// Return an allocated memory block back to a Memory Pool.
osStatus_t osMemoryPoolFree (osMemoryPoolId_t mp_id, void *block) {
  osStatus_t status;

  EvrRtxMemoryPoolFree(mp_id, block);
  if (IsIrqMode() || IsIrqMasked()) {
    status = isrRtxMemoryPoolFree(mp_id, block);
  } else {
    status =  __svcMemoryPoolFree(mp_id, block);
  }
  return status;
}

/// Get maximum number of memory blocks in a Memory Pool.
uint32_t osMemoryPoolGetCapacity (osMemoryPoolId_t mp_id) {
  uint32_t capacity;

  if (IsIrqMode() || IsIrqMasked()) {
    capacity = svcRtxMemoryPoolGetCapacity(mp_id);
  } else {
    capacity =  __svcMemoryPoolGetCapacity(mp_id);
  }
  return capacity;
}

/// Get memory block size in a Memory Pool.
uint32_t osMemoryPoolGetBlockSize (osMemoryPoolId_t mp_id) {
  uint32_t block_size;

  if (IsIrqMode() || IsIrqMasked()) {
    block_size = svcRtxMemoryPoolGetBlockSize(mp_id);
  } else {
    block_size =  __svcMemoryPoolGetBlockSize(mp_id);
  }
  return block_size;
}

/// Get number of memory blocks used in a Memory Pool.
uint32_t osMemoryPoolGetCount (osMemoryPoolId_t mp_id) {
  uint32_t count;

  if (IsIrqMode() || IsIrqMasked()) {
    count = svcRtxMemoryPoolGetCount(mp_id);
  } else {
    count =  __svcMemoryPoolGetCount(mp_id);
  }
  return count;
}

/// Get number of memory blocks available in a Memory Pool.
uint32_t osMemoryPoolGetSpace (osMemoryPoolId_t mp_id) {
  uint32_t space;

  if (IsIrqMode() || IsIrqMasked()) {
    space = svcRtxMemoryPoolGetSpace(mp_id);
  } else {
    space =  __svcMemoryPoolGetSpace(mp_id);
  }
  return space;
}

/// Delete a Memory Pool object.
osStatus_t osMemoryPoolDelete (osMemoryPoolId_t mp_id) {
  osStatus_t status;

  EvrRtxMemoryPoolDelete(mp_id);
  if (IsIrqMode() || IsIrqMasked()) {
    EvrRtxMemoryPoolError(mp_id, (int32_t)osErrorISR);
    status = osErrorISR;
  } else {
    status = __svcMemoryPoolDelete(mp_id);
  }
  return status;
}
