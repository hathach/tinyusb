/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber - rework to unmasked pointers
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

#ifndef TUSB_FIFO_H_
#define TUSB_FIFO_H_

#ifdef __cplusplus
extern "C" {
#endif

// Due to the use of unmasked pointers, this FIFO does not suffer from losing
// one item slice. Furthermore, write and read operations are completely
// decoupled as write and read functions do not modify a common state. Henceforth,
// writing or reading from the FIFO within an ISR is safe as long as no other
// process (thread or ISR) interferes.
// Also, this FIFO is ready to be used in combination with a DMA as the write and
// read pointers can be updated from within a DMA ISR. Overflows are detectable
// within a certain number (see tu_fifo_overflow()).

#include "common/tusb_common.h"
#include "osal/osal.h"

// mutex is only needed for RTOS
// for OS None, we don't get preempted
#define CFG_FIFO_MUTEX      OSAL_MUTEX_REQUIRED

#if CFG_TUD_EDPT_DEDICATED_HWFIFO || CFG_TUH_EDPT_DEDICATED_HWFIFO
  #define CFG_TUSB_FIFO_ACCESS_FIXED_ADDR_RW32
#endif

/* Write/Read "pointer" is in the range of: 0 .. depth - 1, and is used to get the fifo data.
 * Write/Read "index" is always in the range of: 0 .. 2*depth-1
 *
 * The extra window allow us to determine the fifo state of empty or full with only 2 indices
 * Following are examples with depth = 3
 *
 * - empty: W = R
 *                |
 *    -------------------------
 *    | 0 | RW| 2 | 3 | 4 | 5 |
 *
 * - full 1: W > R
 *                |
 *    -------------------------
 *    | 0 | R | 2 | 3 | W | 5 |
 *
 * - full 2: W < R
 *                |
 *    -------------------------
 *    | 0 | 1 | W | 3 | 4 | R |
 *
 * - Number of items in the fifo can be determined in either cases:
 *    - case W >= R: Count = W - R
 *    - case W <  R: Count = 2*depth - (R - W)
 *
 * In non-overwritable mode, computed Count (in above 2 cases) is at most equal to depth.
 * However, in over-writable mode, write index can be repeatedly increased and count can be
 * temporarily larger than depth (overflowed condition) e.g
 *
 *  - Overflowed 1: write(3), write(1)
 *    In this case we will adjust Read index when read()/peek() is called so that count = depth.
 *                  |
 *      -------------------------
 *      | R | 1 | 2 | 3 | W | 5 |
 *
 *  - Double Overflowed i.e index is out of allowed range [0,2*depth)
 *    This occurs when we continue to write after 1st overflowed to 2nd overflowed. e.g:
 *      write(3), write(1), write(2)
 *    This must be prevented since it will cause unrecoverable state, in above example
 *    if not handled the fifo will be empty instead of continue-to-be full. Since we must not modify
 *    read index in write() function, which cause race condition. We will re-position write index so that
 *    after data is written it is a full fifo i.e W = depth - R
 *
 *      re-position W = 1 before write(2)
 *      Note: we should also move data from mem[3] to read index as well, but deliberately skipped here
 *      since it is an expensive operation !!!
 *                  |
 *      -------------------------
 *      | R | W | 2 | 3 | 4 | 5 |
 *
 *      perform write(2), result is still a full fifo.
 *
 *                  |
 *      -------------------------
 *      | R | 1 | 2 | W | 4 | 5 |
 */
typedef struct {
  uint8_t *buffer;              // buffer pointer
  uint16_t depth;               // max items

  struct TU_ATTR_PACKED {
    uint16_t item_size    : 15; // size of each item
    bool     overwritable : 1;  // ovwerwritable when full
  };

  volatile uint16_t wr_idx;     // write index
  volatile uint16_t rd_idx;     // read index

#if OSAL_MUTEX_REQUIRED
  osal_mutex_t mutex_wr;
  osal_mutex_t mutex_rd;
#endif

} tu_fifo_t;

typedef struct {
  struct {
    uint16_t len; // length
    uint8_t *ptr; // buffer pointer
  } linear, wrapped;
} tu_fifo_buffer_info_t;

#define TU_FIFO_INIT(_buffer, _depth, _type, _overwritable) \
  {                                                         \
    .buffer       = _buffer,                                \
    .depth        = _depth,                                 \
    .item_size    = sizeof(_type),                          \
    .overwritable = _overwritable,                          \
  }

#define TU_FIFO_DEF(_name, _depth, _type, _overwritable)                      \
    uint8_t _name##_buf[_depth*sizeof(_type)];                                \
    tu_fifo_t _name = TU_FIFO_INIT(_name##_buf, _depth, _type, _overwritable)

// Write modes intended to allow special read and write functions to be able to
// copy data to and from USB hardware FIFOs as needed for e.g. STM32s and others
typedef enum {
  TU_FIFO_INC_ADDR_RW8,    // increased address read/write by bytes - normal (default) mode
  TU_FIFO_FIXED_ADDR_RW32, // fixed address read/write by 4 bytes (word). Used for STM32 access into USB hardware FIFO
} tu_fifo_access_mode_t;

//--------------------------------------------------------------------+
// Setup API
//--------------------------------------------------------------------+
bool tu_fifo_config(tu_fifo_t *f, void *buffer, uint16_t depth, uint16_t item_size, bool overwritable);
void tu_fifo_set_overwritable(tu_fifo_t *f, bool overwritable);
void tu_fifo_clear(tu_fifo_t *f);

#if OSAL_MUTEX_REQUIRED
TU_ATTR_ALWAYS_INLINE static inline
void tu_fifo_config_mutex(tu_fifo_t *f, osal_mutex_t wr_mutex, osal_mutex_t rd_mutex) {
  f->mutex_wr = wr_mutex;
  f->mutex_rd = rd_mutex;
}
#else
#define tu_fifo_config_mutex(_f, _wr_mutex, _rd_mutex)
#endif

//--------------------------------------------------------------------+
// Index API
//--------------------------------------------------------------------+
void tu_fifo_correct_read_pointer(tu_fifo_t *f);

// Pointer modifications intended to be used in combinations with DMAs.
// USE WITH CARE - NO SAFETY CHECKS CONDUCTED HERE! NOT MUTEX PROTECTED!
void tu_fifo_advance_write_pointer(tu_fifo_t *f, uint16_t n);
void tu_fifo_advance_read_pointer(tu_fifo_t *f, uint16_t n);

// If you want to read/write from/to the FIFO by use of a DMA, you may need to conduct two copies
// to handle a possible wrapping part. These functions deliver a pointer to start
// reading/writing from/to and a valid linear length along which no wrap occurs.
void tu_fifo_get_read_info(tu_fifo_t *f, tu_fifo_buffer_info_t *info);
void tu_fifo_get_write_info(tu_fifo_t *f, tu_fifo_buffer_info_t *info);

//--------------------------------------------------------------------+
// Peek API
// peek() will correct/re-index read pointer in case of an overflowed fifo to form a full fifo
//--------------------------------------------------------------------+
uint16_t tu_fifo_peek_n_access_mode(tu_fifo_t *f, void *p_buffer, uint16_t n, uint16_t wr_idx, uint16_t rd_idx,
                                    tu_fifo_access_mode_t access_mode);
bool     tu_fifo_peek(tu_fifo_t *f, void *p_buffer);
uint16_t tu_fifo_peek_n(tu_fifo_t *f, void *p_buffer, uint16_t n);

//--------------------------------------------------------------------+
// Read API
// peek() + advance read index
//--------------------------------------------------------------------+
uint16_t tu_fifo_read_n_access_mode(tu_fifo_t *f, void *buffer, uint16_t n, tu_fifo_access_mode_t access_mode);
bool     tu_fifo_read(tu_fifo_t *f, void *buffer);
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_fifo_read_n(tu_fifo_t *f, void *buffer, uint16_t n) {
  return tu_fifo_read_n_access_mode(f, buffer, n, TU_FIFO_INC_ADDR_RW8);
}

// discard first n items from fifo i.e advance read pointer by n with mutex
// return number of discarded items
uint16_t tu_fifo_discard_n(tu_fifo_t *f, uint16_t n);

//--------------------------------------------------------------------+
// Write API
//--------------------------------------------------------------------+
uint16_t tu_fifo_write_n_access_mode(tu_fifo_t *f, const void *data, uint16_t n, tu_fifo_access_mode_t access_mode);
bool     tu_fifo_write(tu_fifo_t *f, const void *data);
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_fifo_write_n(tu_fifo_t *f, const void *data, uint16_t n) {
  return tu_fifo_write_n_access_mode(f, data, n, TU_FIFO_INC_ADDR_RW8);
}

//--------------------------------------------------------------------+
// Internal Helper Local
// work on local copies of read/write indices in order to only access them once for re-entrancy
//--------------------------------------------------------------------+
// return overflowable count (index difference), which can be used to determine both fifo count and an overflow state
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_ff_overflow_count(uint16_t depth, uint16_t wr_idx, uint16_t rd_idx) {
  const int32_t diff = (int32_t)wr_idx - (int32_t)rd_idx;
  if (diff >= 0) {
    return (uint16_t)diff;
  } else {
    return (uint16_t)(2 * depth + diff);
  }
}

// return remaining slot in fifo
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_ff_remaining_local(uint16_t depth, uint16_t wr_idx, uint16_t rd_idx) {
  const uint16_t ovf_count = tu_ff_overflow_count(depth, wr_idx, rd_idx);
  return (depth > ovf_count) ? (depth - ovf_count) : 0;
}

//--------------------------------------------------------------------+
// State API
// Following functions are reentrant since they only access read/write indices once, therefore can be used in thread and
// ISRs context without the need of mutexes
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_fifo_depth(const tu_fifo_t *f) {
  return f->depth;
}

TU_ATTR_ALWAYS_INLINE static inline bool tu_fifo_empty(const tu_fifo_t *f) {
  const uint16_t wr_idx = f->wr_idx;
  const uint16_t rd_idx = f->rd_idx;
  return wr_idx == rd_idx;
}

// return number of items in fifo, capped to fifo's depth
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_fifo_count(const tu_fifo_t *f) {
  return tu_min16(tu_ff_overflow_count(f->depth, f->wr_idx, f->rd_idx), f->depth);
}

// check if fifo is full
TU_ATTR_ALWAYS_INLINE static inline bool tu_fifo_full(const tu_fifo_t *f) {
  return tu_ff_overflow_count(f->depth, f->wr_idx, f->rd_idx) >= f->depth;
}

TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_fifo_remaining(const tu_fifo_t *f) {
  return tu_ff_remaining_local(f->depth, f->wr_idx, f->rd_idx);
}

#ifdef __cplusplus
}
#endif

#endif
