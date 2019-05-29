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

// retrieve data from fifo
static void _tu_ff_pull(tu_fifo_t* f, void * buffer)
{
  memcpy(buffer,
         f->buffer + (f->rd_idx * f->item_size),
         f->item_size);

  f->rd_idx = (f->rd_idx + 1) % f->depth;
  f->count--;
}

// send data to fifo
static void _tu_ff_push(tu_fifo_t* f, void const * data)
{
  memcpy( f->buffer + (f->wr_idx * f->item_size),
          data,
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
}

/******************************************************************************/
/*!
    @brief Read one byte out of the RX buffer.

    This function will return the byte located at the array index of the
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

  _tu_ff_pull(f, buffer);

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
    @param[in]  buffer
                The pointer to data location
    @param[in]  count
                Number of element that buffer can afford

    @returns number of items read from the FIFO
*/
/******************************************************************************/
uint16_t tu_fifo_read_n (tu_fifo_t* f, void * buffer, uint16_t count)
{
  if( tu_fifo_empty(f) ) return 0;

  tu_fifo_lock(f);

  /* Limit up to fifo's count */
  if ( count > f->count ) count = f->count;

  uint8_t* buf8 = (uint8_t*) buffer;
  uint16_t len = 0;

  while (len < count)
  {
    _tu_ff_pull(f, buf8);

    len++;
    buf8 += f->item_size;
  }

  tu_fifo_unlock(f);

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

  _tu_ff_push(f, data);

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

  // Not overwritable limit up to full
  if (!f->overwritable) count = tu_min16(count, tu_fifo_remaining(f));

  uint8_t const* buf8 = (uint8_t const*) data;
  uint16_t len = 0;

  while (len < count)
  {
    _tu_ff_push(f, buf8);

    len++;
    buf8 += f->item_size;
  }

  tu_fifo_unlock(f);

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
  tu_fifo_lock(f);

  f->rd_idx = f->wr_idx = f->count = 0;

  tu_fifo_unlock(f);

  return true;
}
