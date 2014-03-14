/**************************************************************************/
/*!
    @file     fifo.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
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
#include "fifo.h"

//static inline void mutex_lock   (fifo_t* f) ATTR_ALWAYS_INLINE;
//static inline void mutex_unlock (fifo_t* f) ATTR_ALWAYS_INLINE;
#define mutex_lock(f)
#define mutex_unlock(f)

static inline bool is_fifo_initalized(fifo_t* f) ATTR_ALWAYS_INLINE;


/**************************************************************************/
/*!
    @brief Read one byte out of the RX buffer.

    This function will return the byte located at the array index of the
    read pointer, and then increment the read pointer index.  If the read
    pointer exceeds the maximum buffer size, it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  data
                Pointer to the place holder for data read from the buffer

    @returns TRUE if the queue is not empty
*/
/**************************************************************************/
bool fifo_read(fifo_t* f, void * p_buffer)
{
  if( !is_fifo_initalized(f) || fifo_is_empty(f) )
  {
    return false;
  }

  mutex_lock(f);

  memcpy(p_buffer,
         f->buffer + (f->rd_idx * f->item_size),
         f->item_size);
  f->rd_idx = (f->rd_idx + 1) % f->depth;
  f->count--;

  mutex_unlock(f);

  return true;
}

/**************************************************************************/
/*!
    @brief Write one byte into the RX buffer.

    This function will write one byte into the array index specified by
    the write pointer and increment the write index. If the write index
    exceeds the max buffer size, then it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  data
                The byte to add to the FIFO

    @returns TRUE if the data was written to the FIFO (overwrittable
             FIFO will always return TRUE)
*/
/**************************************************************************/
bool fifo_write(fifo_t* f, void const * p_data)
{
  if ( !is_fifo_initalized(f) || (fifo_is_full(f) && !f->overwritable) )
  {
    return false;
  }

  mutex_lock(f);

  memcpy( f->buffer + (f->wr_idx * f->item_size),
          p_data,
          f->item_size);

  f->wr_idx = (f->wr_idx + 1) % f->depth;

  if (fifo_is_full(f))
  {
    f->rd_idx = f->wr_idx; // keep the full state (rd == wr && len = size)
  }else
  {
    f->count++;
  }

  mutex_unlock(f);

  return true;
}

/**************************************************************************/
/*!
    @brief Clear the fifo read and write pointers and set length to zero

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
*/
/**************************************************************************/
void fifo_clear(fifo_t *f)
{
  mutex_lock(f);

  f->rd_idx = f->wr_idx = f->count = 0;

  mutex_unlock(f);
}

//--------------------------------------------------------------------+
// HELPER FUNCTIONS
//--------------------------------------------------------------------+

/**************************************************************************/
/*!
    @brief Disables the IRQ specified in the FIFO's 'irq' field
           to prevent reads/write issues with interrupts

    @param[in]  f
                Pointer to the FIFO that should be protected
*/
/**************************************************************************/
//static inline void mutex_lock (fifo_t* f)
//{
//  if (f->irq > 0)
//  {
//    #if !defined (_TEST_)
//    NVIC_DisableIRQ(f->irq);
//    #endif
//  }
//}

/**************************************************************************/
/*!
    @brief Re-enables the IRQ specified in the FIFO's 'irq' field

    @param[in]  f
                Pointer to the FIFO that should be protected
*/
/**************************************************************************/
//static inline void mutex_unlock (fifo_t* f)
//{
//  if (f->irq > 0)
//  {
//    #if !defined (_TEST_)
//    NVIC_EnableIRQ(f->irq);
//    #endif
//  }
//}

static inline bool is_fifo_initalized(fifo_t* f)
{
  return !( f->buffer == NULL || f->depth == 0 || f->item_size == 0);
}
