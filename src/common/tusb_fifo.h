/**************************************************************************/
/*!
    @file     tusb_fifo.h
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

/** \ingroup Group_Common
 * \defgroup group_fifo fifo
 *  @{ */

#ifndef _TUSB_FIFO_H_
#define _TUSB_FIFO_H_

#define CFG_FIFO_MUTEX      1

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
 extern "C" {
#endif

#if CFG_FIFO_MUTEX
#define tu_fifo_mutex_t  osal_mutex_t
#endif


/** \struct tu_fifo_t
 * \brief Simple Circular FIFO
 */
typedef struct
{
           uint8_t* buffer    ; ///< buffer pointer
           uint16_t depth     ; ///< max items
           uint16_t item_size ; ///< size of each item

  volatile uint16_t count     ; ///< number of items in queue
  volatile uint16_t wr_idx    ; ///< write pointer
  volatile uint16_t rd_idx    ; ///< read pointer

           bool overwritable  ;

#if CFG_FIFO_MUTEX
  tu_fifo_mutex_t mutex;
#endif

} tu_fifo_t;

#define TU_FIFO_DEF(_name, _depth, _type, _overwritable) \
  uint8_t _name##_buf[_depth*sizeof(_type)]; \
  tu_fifo_t _name = {                        \
      .buffer       = _name##_buf,           \
      .depth        = _depth,                \
      .item_size    = sizeof(_type),         \
      .overwritable = _overwritable,         \
  }

bool tu_fifo_clear(tu_fifo_t *f);
bool tu_fifo_config(tu_fifo_t *f, void* buffer, uint16_t depth, uint16_t item_size, bool overwritable);

#if CFG_FIFO_MUTEX
static inline void tu_fifo_config_mutex(tu_fifo_t *f, tu_fifo_mutex_t mutex_hdl)
{
  f->mutex = mutex_hdl;
}
#endif

bool     tu_fifo_write   (tu_fifo_t* f, void const * p_data);
uint16_t tu_fifo_write_n (tu_fifo_t* f, void const * p_data, uint16_t count);

bool     tu_fifo_read    (tu_fifo_t* f, void * p_buffer);
uint16_t tu_fifo_read_n  (tu_fifo_t* f, void * p_buffer, uint16_t count);

bool     tu_fifo_peek_at (tu_fifo_t* f, uint16_t pos, void * p_buffer);

static inline bool tu_fifo_peek(tu_fifo_t* f, void * p_buffer)
{
  return tu_fifo_peek_at(f, 0, p_buffer);
}

static inline bool tu_fifo_empty(tu_fifo_t* f)
{
  return (f->count == 0);
}

static inline bool tu_fifo_full(tu_fifo_t* f)
{
  return (f->count == f->depth);
}

static inline uint16_t tu_fifo_count(tu_fifo_t* f)
{
  return f->count;
}

static inline uint16_t tu_fifo_remaining(tu_fifo_t* f)
{
  return f->depth - f->count;
}

static inline uint16_t tu_fifo_depth(tu_fifo_t* f)
{
  return f->depth;
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_FIFO_H_ */
