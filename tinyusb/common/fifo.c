/*
 * fifo.c
 *
 *  Created on: Nov 27, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tinyUSB stack.
 */

#include "fifo.h"

/**************************************************************************/
/*!
    @brief Disables the IRQ specified in the FIFO's 'irq' field
           to prevent reads/write issues with interrupts

    @param[in]  f
                Pointer to the FIFO that should be protected
*/
/**************************************************************************/
static inline void mutex_lock (fifo_t* f)
{
  if (f->irq > 0)
    NVIC_DisableIRQ(f->irq);
}

/**************************************************************************/
/*!
    @brief Re-enables the IRQ specified in the FIFO's 'irq' field

    @param[in]  f
                Pointer to the FIFO that should be protected
*/
/**************************************************************************/
static inline void mutex_unlock (fifo_t* f)
{
  if (f->irq > 0)
    NVIC_EnableIRQ(f->irq);
}

/**************************************************************************/
/*!
    @brief Initialises the FIFO buffer

    @param[in]  f
                Pointer to the fifo_t object to intiialize
    @param[in]  buffer
                Pointer to the buffer's location in memory
    @param[in]  size
                The buffer size in bytes
    @param[in]  overwritable
                Set to TRUE is the FIFO is overwritable when the FIFO
                is full (the first element will be overwritten)
    @param[in]  irq
                The IRQ number to disable for MUTEX protection.
                Set the -1 if not required.
*/
/**************************************************************************/
bool fifo_init(fifo_t* f, uint8_t* buffer, uint16_t size, bool overwritable, IRQn_Type irq)
{
  ASSERT(size > 0, false);

  f->buf = buffer;
  f->size = size;
  f->rd_ptr = f->wr_ptr = f->len = 0;
  f->overwritable = overwritable;
  f->irq = irq;

  return true;
}

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
bool fifo_read(fifo_t* f, uint8_t *data)
{
  if (fifo_is_empty(f))
    return false;

  mutex_lock(f);

  *data = f->buf[f->rd_ptr];
  f->rd_ptr = (f->rd_ptr + 1) % f->size;
  f->len--;

  mutex_unlock(f);

  return true;
}

/**************************************************************************/
/*!
    @brief Read a byte array from FIFO

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  rx
                Pointer to the place holder for data read from the buffer
    @param[in]  maxlen
                The maximum number of bytes to read from the FIFO

    @returns The actual number of bytes read from the FIFO
 */
/**************************************************************************/
uint16_t fifo_read_n(fifo_t* f, uint8_t* rx, uint16_t maxlen)
{
  uint16_t len = 0;
  
  while ( len < maxlen && fifo_read(f, rx) )
  {
    len++;
    rx++;
  }
  
  return len;
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
bool fifo_write(fifo_t* f, uint8_t data)
{
  if ( fifo_is_full(f) && f->overwritable == false)
      return false;

  mutex_lock(f);

  f->buf[f->wr_ptr] = data;
  f->wr_ptr = (f->wr_ptr + 1) % f->size;

  if (fifo_is_full(f))
  {
    f->rd_ptr = f->wr_ptr; // keep the full state (rd == wr && len = size)
  }else
  {
    f->len++;
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

  f->rd_ptr = 0;
  f->wr_ptr = 0;
  f->len = 0;

  mutex_unlock(f);
}
