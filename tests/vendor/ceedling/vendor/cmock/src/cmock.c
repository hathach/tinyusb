/* ==========================================
    CMock Project - Automatic Mock Generation for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#include "unity.h"
#include "cmock.h"

//define CMOCK_MEM_DYNAMIC to grab memory as needed with malloc
//when you do that, CMOCK_MEM_SIZE is used for incremental size instead of total
#ifdef CMOCK_MEM_STATIC
#undef CMOCK_MEM_DYNAMIC
#endif

#ifdef CMOCK_MEM_DYNAMIC
#include <stdlib.h>
#endif

//this is used internally during pointer arithmetic. make sure this type is the same size as the target's pointer type
#ifndef CMOCK_MEM_PTR_AS_INT
#define CMOCK_MEM_PTR_AS_INT unsigned long
#endif

//0 for no alignment, 1 for 16-bit, 2 for 32-bit, 3 for 64-bit
#ifndef CMOCK_MEM_ALIGN
#define CMOCK_MEM_ALIGN (2)
#endif

//amount of memory to allow cmock to use in its internal heap
#ifndef CMOCK_MEM_SIZE
#define CMOCK_MEM_SIZE (32768)
#endif

//automatically calculated defs for easier reading
#define CMOCK_MEM_ALIGN_SIZE  (1u << CMOCK_MEM_ALIGN)
#define CMOCK_MEM_ALIGN_MASK  (CMOCK_MEM_ALIGN_SIZE - 1)
#define CMOCK_MEM_INDEX_SIZE  ((sizeof(CMOCK_MEM_INDEX_TYPE) > CMOCK_MEM_ALIGN_SIZE) ? sizeof(CMOCK_MEM_INDEX_TYPE) : CMOCK_MEM_ALIGN_SIZE)

//private variables
#ifdef CMOCK_MEM_DYNAMIC
static unsigned char*         CMock_Guts_Buffer = NULL;
static CMOCK_MEM_INDEX_TYPE   CMock_Guts_BufferSize = CMOCK_MEM_ALIGN_SIZE;
static CMOCK_MEM_INDEX_TYPE   CMock_Guts_FreePtr;
#else
static unsigned char          CMock_Guts_Buffer[CMOCK_MEM_SIZE + CMOCK_MEM_ALIGN_SIZE];
static CMOCK_MEM_INDEX_TYPE   CMock_Guts_BufferSize = CMOCK_MEM_SIZE + CMOCK_MEM_ALIGN_SIZE;
static CMOCK_MEM_INDEX_TYPE   CMock_Guts_FreePtr;
#endif
//-------------------------------------------------------
// CMock_Guts_MemNew
//-------------------------------------------------------
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemNew(CMOCK_MEM_INDEX_TYPE size)
{
  CMOCK_MEM_INDEX_TYPE index;

  //verify arguments valid (we must be allocating space for at least 1 byte, and the existing chain must be in memory somewhere)
  if (size < 1)
    return CMOCK_GUTS_NONE;

  //verify we have enough room
  size = size + CMOCK_MEM_INDEX_SIZE;
  if (size & CMOCK_MEM_ALIGN_MASK)
    size = (size + CMOCK_MEM_ALIGN_MASK) & ~CMOCK_MEM_ALIGN_MASK;
  if ((CMock_Guts_BufferSize - CMock_Guts_FreePtr) < size)
  {
#ifdef CMOCK_MEM_DYNAMIC
    CMock_Guts_BufferSize += CMOCK_MEM_SIZE + size;
    CMock_Guts_Buffer = realloc(CMock_Guts_Buffer, CMock_Guts_BufferSize);
    if (CMock_Guts_Buffer == NULL)
#endif //yes that if will continue to the return below if TRUE
      return CMOCK_GUTS_NONE;
  }

  //determine where we're putting this new block, and init its pointer to be the end of the line
  index = CMock_Guts_FreePtr + CMOCK_MEM_INDEX_SIZE;
  *(CMOCK_MEM_INDEX_TYPE*)(&CMock_Guts_Buffer[CMock_Guts_FreePtr]) = CMOCK_GUTS_NONE;
  CMock_Guts_FreePtr += size;

  return index;
}

//-------------------------------------------------------
// CMock_Guts_MemChain
//-------------------------------------------------------
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemChain(CMOCK_MEM_INDEX_TYPE root_index, CMOCK_MEM_INDEX_TYPE obj_index)
{
  CMOCK_MEM_INDEX_TYPE index;
  void* root;
  void* obj;
  void* next;

  if (root_index == CMOCK_GUTS_NONE)
  {
    //if there is no root currently, we return this object as the root of the chain
    return obj_index;
  }
  else
  {
    //reject illegal nodes
    if ((root_index < CMOCK_MEM_ALIGN_SIZE) || (root_index >= CMock_Guts_FreePtr))
    {
      return CMOCK_GUTS_NONE;
    }
    if ((obj_index < CMOCK_MEM_ALIGN_SIZE) || (obj_index >= CMock_Guts_FreePtr))
    {
      return CMOCK_GUTS_NONE;
    }
    
    root = (void*)(&CMock_Guts_Buffer[root_index]);
    obj  = (void*)(&CMock_Guts_Buffer[obj_index]);

    //find the end of the existing chain and add us
    next = root;
    do {
      index = *(CMOCK_MEM_INDEX_TYPE*)((CMOCK_MEM_PTR_AS_INT)next - CMOCK_MEM_INDEX_SIZE);
      if (index >= CMock_Guts_FreePtr)
        return CMOCK_GUTS_NONE;
      if (index > 0)
        next = (void*)(&CMock_Guts_Buffer[index]);
    } while (index > 0);
    *(CMOCK_MEM_INDEX_TYPE*)((CMOCK_MEM_PTR_AS_INT)next - CMOCK_MEM_INDEX_SIZE) = ((CMOCK_MEM_PTR_AS_INT)obj - (CMOCK_MEM_PTR_AS_INT)CMock_Guts_Buffer);
    return root_index;
  }
}

//-------------------------------------------------------
// CMock_Guts_MemNext
//-------------------------------------------------------
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemNext(CMOCK_MEM_INDEX_TYPE previous_item_index)
{
  CMOCK_MEM_INDEX_TYPE index;
  void* previous_item;

  //There is nothing "next" if the pointer isn't from our buffer
  if ((previous_item_index < CMOCK_MEM_ALIGN_SIZE) || (previous_item_index  >= CMock_Guts_FreePtr))
    return CMOCK_GUTS_NONE;
  previous_item = (void*)(&CMock_Guts_Buffer[previous_item_index]);

  //if the pointer is good, then use it to look up the next index
  //(we know the first element always goes in zero, so NEXT must always be > 1)
  index = *(CMOCK_MEM_INDEX_TYPE*)((CMOCK_MEM_PTR_AS_INT)previous_item - CMOCK_MEM_INDEX_SIZE);
  if ((index > 1) && (index < CMock_Guts_FreePtr))
    return index;
  else
    return CMOCK_GUTS_NONE;
}

//-------------------------------------------------------
// CMock_GetAddressFor
//-------------------------------------------------------
void* CMock_Guts_GetAddressFor(CMOCK_MEM_INDEX_TYPE index)
{
  if ((index >= CMOCK_MEM_ALIGN_SIZE) && (index < CMock_Guts_FreePtr))
  {
    return (void*)(&CMock_Guts_Buffer[index]);
  }
  else
  {
    return NULL;
  }
}

//-------------------------------------------------------
// CMock_Guts_MemBytesFree
//-------------------------------------------------------
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemBytesFree(void)
{
  return CMock_Guts_BufferSize - CMock_Guts_FreePtr;
}

//-------------------------------------------------------
// CMock_Guts_MemBytesUsed
//-------------------------------------------------------
CMOCK_MEM_INDEX_TYPE CMock_Guts_MemBytesUsed(void)
{
  return CMock_Guts_FreePtr - CMOCK_MEM_ALIGN_SIZE;
}

//-------------------------------------------------------
// CMock_Guts_MemFreeAll
//-------------------------------------------------------
void CMock_Guts_MemFreeAll(void)
{
  CMock_Guts_FreePtr = CMOCK_MEM_ALIGN_SIZE; //skip the very beginning
}
