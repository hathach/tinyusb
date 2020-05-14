/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include <string.h>

#include "osal/osal.h"
#include "tusb_fifo.h"

// implement mutex lock and unlock
#if CFG_FIFO_MUTEX

static void tu_fifo_lock(tu_fifo_t *f)
{
  if (f->mutex)
  {
    osal_mutex_lock(f->mutex, OSAL_TIMEOUT_WAIT_FOREVER);
  }
}

static void tu_fifo_unlock(tu_fifo_t *f)
{
  if (f->mutex)
  {
    osal_mutex_unlock(f->mutex);
  }
}

#else

#define tu_fifo_lock(_ff)
#define tu_fifo_unlock(_ff)

#endif

bool tu_fifo_config(tu_fifo_t *f, void* buffer, uint16_t depth, uint16_t item_size, bool overwritable)
{
  tu_fifo_lock(f);

  f->buffer = (uint8_t*) buffer;
  f->depth  = depth;
  f->item_size = item_size;
  f->overwritable = overwritable;

  f->rd_idx = f->wr_idx = f->count = 0;

  tu_fifo_unlock(f);

  return true;
}

static inline uint16_t _ff_mod(uint16_t idx, uint16_t depth)
{
  return (idx < depth) ? idx : (idx-depth);
}

// retrieve data from fifo
static inline void _ff_pull(tu_fifo_t* f, void * buffer, uint16_t n)
{
  memcpy(buffer,
         f->buffer + (f->rd_idx * f->item_size),
         f->item_size*n);

  f->rd_idx = _ff_mod(f->rd_idx + n, f->depth);
  f->count -= n;
}

// send data to fifo
static inline void _ff_push(tu_fifo_t* f, void const * data, uint16_t n)
{
  memcpy(f->buffer + (f->wr_idx * f->item_size),
         data,
         f->item_size*n);

  f->wr_idx = _ff_mod(f->wr_idx + n, f->depth);

  if (tu_fifo_full(f))
  {
    f->rd_idx = f->wr_idx; // keep the full state (rd == wr && count = depth)
  }
  else
  {
    f->count += n;
  }
}

/******************************************************************************/
/*!
    @brief Read one element out of the RX buffer.

    This function will return the element located at the array index of the
    read pointer, and then increment the read pointer index.  If the read
    pointer exceeds the maximum buffer size, it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  buffer
                Pointer to the place holder for data read from the buffer

    @returns TRUE if the queue is not empty
*/
/******************************************************************************/
bool tu_fifo_read(tu_fifo_t* f, void * buffer)
{
  if( tu_fifo_empty(f) ) return false;

  tu_fifo_lock(f);

  _ff_pull(f, buffer, 1);

  tu_fifo_unlock(f);

  return true;
}

/******************************************************************************/
/*!
    @brief This function will read n elements from the array index specified by
    the read pointer and increment the read index. If the read index
    exceeds the max buffer size, then it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  buffer
                The pointer to data location
    @param[in]  count
                Number of element that buffer can afford

    @returns number of items read from the FIFO
*/
/******************************************************************************/
uint16_t tu_fifo_read_n (tu_fifo_t* f, void * buffer, uint16_t count)
{
  if(tu_fifo_empty(f)) return 0;

  tu_fifo_lock(f);

  // Limit up to fifo's count
  if(count > f->count) count = f->count;

  if(count + f->rd_idx <= f->depth)
  {
    _ff_pull(f, buffer, count);
  }
  else
  {
    uint16_t const part1 = f->depth - f->rd_idx;

    // Part 1: from rd_idx to end
    _ff_pull(f, buffer, part1);
    buffer = ((uint8_t*) buffer) + part1*f->item_size;

    // Part 2: start to remaining
    _ff_pull(f, buffer, count-part1);
  }

  tu_fifo_unlock(f);

  return count;
}

/******************************************************************************/
/*!
    @brief Read one item without removing it from the FIFO

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

  tu_fifo_lock(f);

  // rd_idx is pos=0
  uint16_t index = _ff_mod(f->rd_idx + pos, f->depth);
  memcpy(p_buffer,
         f->buffer + (index * f->item_size),
         f->item_size);

  tu_fifo_unlock(f);

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
    @param[in]  data
                The byte to add to the FIFO

    @returns TRUE if the data was written to the FIFO (overwrittable
             FIFO will always return TRUE)
*/
/******************************************************************************/
bool tu_fifo_write (tu_fifo_t* f, const void * data)
{
  if ( tu_fifo_full(f) && !f->overwritable ) return false;

  tu_fifo_lock(f);

  _ff_push(f, data, 1);

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
    @param[in]  data
                The pointer to data to add to the FIFO
    @param[in]  count
                Number of element
    @return Number of written elements
*/
/******************************************************************************/
uint16_t tu_fifo_write_n (tu_fifo_t* f, const void * data, uint16_t count)
{
  if ( count == 0 ) return 0;

  tu_fifo_lock(f);

  uint8_t const* buf8 = (uint8_t const*) data;

  if (!f->overwritable)
  {
    // Not overwritable limit up to full
    count = tu_min16(count, tu_fifo_remaining(f));
  }
  else if (count > f->depth)
  {
    // Only copy last part
    buf8 = buf8 + (count - f->depth) * f->item_size;
    count = f->depth;
    f->wr_idx = 0;
    f->rd_idx = 0;
    f->count = 0;
  }

  if (count + f->wr_idx <= f->depth )
  {
    _ff_push(f, buf8, count);
  }
  else
  {
    uint16_t const part1 = f->depth - f->wr_idx;

    // Part 1: from wr_idx to end
    _ff_push(f, buf8, part1);
    buf8 += part1*f->item_size;

    // Part 2: start to remaining
    _ff_push(f, buf8, count-part1);
  }
  
  tu_fifo_unlock(f);

  return count;
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
  tu_fifo_lock(f);

  f->rd_idx = f->wr_idx = f->count = 0;

  tu_fifo_unlock(f);

  return true;
}
