/**************************************************************************/
/*!
    @file     tusb_fifo.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 This file is part of the tinyusb stack.
 */
/**************************************************************************/

#include <string.h>

#include "osal/osal.h"
#include "tusb_fifo.h"

// implement mutex lock and unlock
// For OSAL_NONE: if mutex is locked by other, function return immediately (since there is no task context)
// For Real RTOS: fifo lock is a blocking API
#if CFG_FIFO_MUTEX

static bool tu_fifo_lock(tu_fifo_t *f)
{
  if (f->mutex)
  {
#if CFG_TUSB_OS == OPT_OS_NONE
    // There is no subtask context for blocking mutex, we will check and return if cannot lock the mutex
    if ( !osal_mutex_lock_notask(f->mutex) ) return false;
#else
    uint32_t err;
    (void) err;
    osal_mutex_lock(f->mutex, OSAL_TIMEOUT_WAIT_FOREVER, &err);
#endif
  }

  return true;
}

static void tu_fifo_unlock(tu_fifo_t *f)
{
  if (f->mutex)
  {
    osal_mutex_unlock(f->mutex);
  }
}

#else

#define tu_fifo_lock(_ff)       true
#define tu_fifo_unlock(_ff)

#endif

bool tu_fifo_config(tu_fifo_t *f, void* buffer, uint16_t depth, uint16_t item_size, bool overwritable)
{
  if ( !tu_fifo_lock(f) ) return false;

  f->buffer = (uint8_t*) buffer;
  f->depth  = depth;
  f->item_size = item_size;
  f->overwritable = overwritable;

  f->rd_idx = f->wr_idx = f->count = 0;

  tu_fifo_unlock(f);

  return true;
}


/******************************************************************************/
/*!
    @brief Read one byte out of the RX buffer.

    This function will return the byte located at the array index of the
    read pointer, and then increment the read pointer index.  If the read
    pointer exceeds the maximum buffer size, it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  p_buffer
                Pointer to the place holder for data read from the buffer

    @returns TRUE if the queue is not empty
*/
/******************************************************************************/
bool tu_fifo_read(tu_fifo_t* f, void * p_buffer)
{
  if( tu_fifo_empty(f) ) return false;

  if ( !tu_fifo_lock(f) ) return false;

  memcpy(p_buffer,
         f->buffer + (f->rd_idx * f->item_size),
         f->item_size);
  f->rd_idx = (f->rd_idx + 1) % f->depth;
  f->count--;

  tu_fifo_unlock(f);

  return true;
}

/******************************************************************************/
/*!
    @brief This function will read n elements into the array index specified by
    the write pointer and increment the write index. If the write index
    exceeds the max buffer size, then it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  p_data
                The pointer to data location
    @param[in]  count
                Number of element that buffer can afford

    @returns number of items read from the FIFO
*/
/******************************************************************************/
uint16_t tu_fifo_read_n (tu_fifo_t* f, void * p_buffer, uint16_t count)
{
  if( tu_fifo_empty(f) ) return 0;

  /* Limit up to fifo's count */
  if ( count > f->count ) count = f->count;

  /* Could copy up to 2 portions marked as 'x' if queue is wrapped around
   * case 1: ....RxxxxW.......
   * case 2: xxxxxW....Rxxxxxx
   */
//  uint16_t index2upper = tu_min16(count, f->count-f->rd_idx);

  uint8_t* p_buf = (uint8_t*) p_buffer;
  uint16_t len = 0;
  while( (len < count) && tu_fifo_read(f, p_buf) )
  {
    len++;
    p_buf += f->item_size;
  }

  return len;
}

/******************************************************************************/
/*!
    @brief Reads one item without removing it from the FIFO

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  pos
                Position to read from in the FIFO buffer
    @param[in]  p_buffer
                Pointer to the place holder for data read from the buffer

    @returns TRUE if the queue is not empty
*/
/******************************************************************************/
bool tu_fifo_peek_at(tu_fifo_t* f, uint16_t pos, void * p_buffer)
{
  if ( pos >= f->count ) return false;

  // rd_idx is pos=0
  uint16_t index = (f->rd_idx + pos) % f->depth;
  memcpy(p_buffer,
         f->buffer + (index * f->item_size),
         f->item_size);

  return true;
}

/******************************************************************************/
/*!
    @brief Write one element into the RX buffer.

    This function will write one element into the array index specified by
    the write pointer and increment the write index. If the write index
    exceeds the max buffer size, then it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  p_data
                The byte to add to the FIFO

    @returns TRUE if the data was written to the FIFO (overwrittable
             FIFO will always return TRUE)
*/
/******************************************************************************/
bool tu_fifo_write (tu_fifo_t* f, const void * p_data)
{
  if ( tu_fifo_full(f) && !f->overwritable ) return false;

  if ( !tu_fifo_lock(f) ) return false;

  memcpy( f->buffer + (f->wr_idx * f->item_size),
          p_data,
          f->item_size);

  f->wr_idx = (f->wr_idx + 1) % f->depth;

  if (tu_fifo_full(f))
  {
    f->rd_idx = f->wr_idx; // keep the full state (rd == wr && len = size)
  }
  else
  {
    f->count++;
  }

  tu_fifo_unlock(f);

  return true;
}

/******************************************************************************/
/*!
    @brief This function will write n elements into the array index specified by
    the write pointer and increment the write index. If the write index
    exceeds the max buffer size, then it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  p_data
                The pointer to data to add to the FIFO
    @param[in]  count
                Number of element
    @return Number of written elements
*/
/******************************************************************************/
uint16_t tu_fifo_write_n (tu_fifo_t* f, const void * p_data, uint16_t count)
{
  if ( count == 0 ) return 0;

  uint8_t const* p_buf = (uint8_t const*) p_data;

  uint16_t len = 0;
  while( (len < count) && tu_fifo_write(f, p_buf) )
  {
    len++;
    p_buf += f->item_size;
  }

  return len;
}

/******************************************************************************/
/*!
    @brief Clear the fifo read and write pointers and set length to zero

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
*/
/******************************************************************************/
bool tu_fifo_clear(tu_fifo_t *f)
{
  if ( !tu_fifo_lock(f) ) return false;

  f->rd_idx = f->wr_idx = f->count = 0;

  tu_fifo_unlock(f);

  return true;
}
