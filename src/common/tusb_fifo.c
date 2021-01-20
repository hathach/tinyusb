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

#include <string.h>

#include "osal/osal.h"
#include "tusb_fifo.h"

// Supress IAR warning
// Warning[Pa082]: undefined behavior: the order of volatile accesses is undefined in this statement
#if defined(__ICCARM__)
#pragma diag_suppress = Pa082
#endif

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
  if (depth > 0x8000) return false;               // Maximum depth is 2^15 items

  tu_fifo_lock(f);

  f->buffer = (uint8_t*) buffer;
  f->depth  = depth;
  f->item_size = item_size;
  f->overwritable = overwritable;

  f->max_pointer_idx = 2*depth - 1;               // Limit index space to 2*depth - this allows for a fast "modulo" calculation but limits the maximum depth to 2^16/2 = 2^15 and buffer overflows are detectable only if overflow happens once (important for unsupervised DMA applications)
  f->non_used_index_space = 0xFFFF - f->max_pointer_idx;

  f->rd_mode = f->wr_mode = TU_FIFO_COPY_INC;     // Default copy mode is incrementing addresses

  f->rd_idx = f->wr_idx = 0;

  tu_fifo_unlock(f);

  return true;
}

// Static functions are intended to work on local variables

static inline uint16_t _ff_mod(uint16_t idx, uint16_t depth)
{
  while ( idx >= depth) idx -= depth;
  return idx;
}

// Intended to be used to read from hardware USB FIFO in e.g. STM32 where all data is read from a constant address
// Code adapted from dcd_synopsis.c
static void _tu_fifo_read_from_const_src_ptr(void * __restrict dst, const void * __restrict src, uint16_t len)
{
  uint8_t CFG_TUSB_MEM_ALIGN * dst_u8 = (uint8_t *)dst;
  volatile uint32_t * rx_fifo = (volatile uint32_t *) src;

  // Reading full available 32 bit words from FIFO
  uint16_t full_words = len >> 2;
  for(uint16_t i = 0; i < full_words; i++) {
    uint32_t tmp = *rx_fifo;
    dst_u8[0] = tmp & 0x000000FF;
    dst_u8[1] = (tmp & 0x0000FF00) >> 8;
    dst_u8[2] = (tmp & 0x00FF0000) >> 16;
    dst_u8[3] = (tmp & 0xFF000000) >> 24;
    dst_u8 += 4;
  }

  // Read the remaining 1-3 bytes from FIFO
  uint8_t bytes_rem = len & 0x03;
  if(bytes_rem != 0) {
    uint32_t tmp = *rx_fifo;
    dst_u8[0] = tmp & 0x000000FF;
    if(bytes_rem > 1) {
      dst_u8[1] = (tmp & 0x0000FF00) >> 8;
    }
    if(bytes_rem > 2) {
      dst_u8[2] = (tmp & 0x00FF0000) >> 16;
    }
  }
}

// Intended to be used to write to hardware USB FIFO in e.g. STM32 where all data is written to a constant address
// Code adapted from dcd_synopsis.c
static void _tu_fifo_write_to_const_dst_ptr(void * __restrict dst, const void * __restrict src, uint16_t len)
{
  volatile uint32_t * tx_fifo = (volatile uint32_t *) dst;
  uint8_t CFG_TUSB_MEM_ALIGN * src_u8 = (uint8_t *)src;

  // Pushing full available 32 bit words to FIFO
  uint16_t full_words = len >> 2;
  for(uint16_t i = 0; i < full_words; i++){
    *tx_fifo = ((uint32_t)(src_u8[3]) << 24) | ((uint32_t)(src_u8[2]) << 16) | ((uint32_t)(src_u8[1]) << 8) | (uint32_t)src_u8[0];
    src_u8 += 4;
  }

  // Write the remaining 1-3 bytes into FIFO
  uint8_t bytes_rem = len & 0x03;
  if(bytes_rem){
    uint32_t tmp_word = 0;
    tmp_word |= src_u8[0];
    if(bytes_rem > 1){
      tmp_word |= (uint32_t)(src_u8[1]) << 8;
    }
    if(bytes_rem > 2){
      tmp_word |= (uint32_t)(src_u8[2]) << 16;
    }
    *tx_fifo = tmp_word;
  }
}

// send one item to FIFO WITHOUT updating write pointer
static inline void _ff_push(tu_fifo_t* f, void const * data, uint16_t wRel)
{
  memcpy(f->buffer + (wRel * f->item_size), data, f->item_size);
}

static inline void _ff_push_copy_fct(tu_fifo_t* f, void * dst, const void * src, uint16_t len)
{
  switch (f->rd_mode)
  {
    case TU_FIFO_COPY_INC:
      memcpy(dst, src, len);
      break;

    case TU_FIFO_COPY_CST:
      _tu_fifo_read_from_const_src_ptr(dst, src, len);
      break;
  }
}

static inline void _ff_pull_copy_fct(tu_fifo_t* f, void * dst, const void * src, uint16_t len)
{
  switch (f->wr_mode)
  {
    case TU_FIFO_COPY_INC:
      memcpy(dst, src, len);
      break;

    case TU_FIFO_COPY_CST:
      _tu_fifo_write_to_const_dst_ptr(dst, src, len);
      break;
  }
}

// send n items to FIFO WITHOUT updating write pointer
static void _ff_push_n(tu_fifo_t* f, void const * data, uint16_t n, uint16_t wRel)
{
  if(wRel + n <= f->depth)  // Linear mode only
  {
    _ff_push_copy_fct(f, f->buffer + (wRel * f->item_size), data, n*f->item_size);
  }
  else      // Wrap around
  {
    uint16_t nLin = f->depth - wRel;

    // Write data to linear part of buffer
    _ff_push_copy_fct(f, f->buffer + (wRel * f->item_size), data, nLin*f->item_size);

    // Write data wrapped around
    _ff_push_copy_fct(f, f->buffer, ((uint8_t const*) data) + nLin*f->item_size, (n - nLin) * f->item_size);
  }
}

// get one item from FIFO WITHOUT updating read pointer
static inline void _ff_pull(tu_fifo_t* f, void * p_buffer, uint16_t rRel)
{
  memcpy(p_buffer, f->buffer + (rRel * f->item_size), f->item_size);
}

// get n items from FIFO WITHOUT updating read pointer
static void _ff_pull_n(tu_fifo_t* f, void * p_buffer, uint16_t n, uint16_t rRel)
{
  if(rRel + n <= f->depth)       // Linear mode only
  {
    _ff_pull_copy_fct(f, p_buffer, f->buffer + (rRel * f->item_size), n*f->item_size);
  }
  else      // Wrap around
  {
    uint16_t nLin = f->depth - rRel;

    // Read data from linear part of buffer
    _ff_pull_copy_fct(f, p_buffer, f->buffer + (rRel * f->item_size), nLin*f->item_size);

    // Read data wrapped part
    _ff_pull_copy_fct(f, (uint8_t*)p_buffer + nLin*f->item_size, f->buffer, (n - nLin) * f->item_size);
  }
}

// Advance an absolute pointer
static uint16_t advance_pointer(tu_fifo_t* f, uint16_t p, uint16_t offset)
{
  // We limit the index space of p such that a correct wrap around happens
  // Check for a wrap around or if we are in unused index space - This has to be checked first!! We are exploiting the wrap around to the correct index
  if ((p > p + offset) || (p + offset > f->max_pointer_idx))
  {
    p = (p + offset) + f->non_used_index_space;
  }
  else
  {
    p += offset;
  }
  return p;
}

// Backward an absolute pointer
static uint16_t backward_pointer(tu_fifo_t* f, uint16_t p, uint16_t offset)
{
  // We limit the index space of p such that a correct wrap around happens
  // Check for a wrap around or if we are in unused index space - This has to be checked first!! We are exploiting the wrap around to the correct index
  if ((p < p - offset) || (p - offset > f->max_pointer_idx))
  {
    p = (p - offset) - f->non_used_index_space;
  }
  else
  {
    p -= offset;
  }
  return p;
}

// get relative from absolute pointer
static uint16_t get_relative_pointer(tu_fifo_t* f, uint16_t p, uint16_t offset)
{
  return _ff_mod(advance_pointer(f, p, offset), f->depth);
}

// Works on local copies of w and r - return only the difference and as such can be used to determine an overflow
static inline uint16_t _tu_fifo_count(tu_fifo_t* f, uint16_t wAbs, uint16_t rAbs)
{
  uint16_t cnt = wAbs-rAbs;

  // In case we have non-power of two depth we need a further modification
  if (rAbs > wAbs) cnt -= f->non_used_index_space;

  return cnt;
}

// Works on local copies of w and r
static inline bool _tu_fifo_empty(uint16_t wAbs, uint16_t rAbs)
{
  return wAbs == rAbs;
}

// Works on local copies of w and r
static inline bool _tu_fifo_full(tu_fifo_t* f, uint16_t wAbs, uint16_t rAbs)
{
  return (_tu_fifo_count(f, wAbs, rAbs) == f->depth);
}

// Works on local copies of w and r
// BE AWARE - THIS FUNCTION MIGHT NOT GIVE A CORRECT ANSWERE IN CASE WRITE POINTER "OVERFLOWS"
// Only one overflow is allowed for this function to work e.g. if depth = 100, you must not
// write more than 2*depth-1 items in one rush without updating write pointer. Otherwise
// write pointer wraps and you pointer states are messed up. This can only happen if you
// use DMAs, write functions do not allow such an error.
static inline bool _tu_fifo_overflowed(tu_fifo_t* f, uint16_t wAbs, uint16_t rAbs)
{
  return (_tu_fifo_count(f, wAbs, rAbs) > f->depth);
}

// Works on local copies of w
// For more details see _tu_fifo_overflow()!
static inline void _tu_fifo_correct_read_pointer(tu_fifo_t* f, uint16_t wAbs)
{
  f->rd_idx = backward_pointer(f, wAbs, f->depth);
}

// Works on local copies of w and r
// Must be protected by mutexes since in case of an overflow read pointer gets modified
static bool _tu_fifo_peek_at(tu_fifo_t* f, uint16_t offset, void * p_buffer, uint16_t wAbs, uint16_t rAbs)
{
  uint16_t cnt = _tu_fifo_count(f, wAbs, rAbs);

  // Check overflow and correct if required
  if (cnt > f->depth)
  {
    _tu_fifo_correct_read_pointer(f, wAbs);
    cnt = f->depth;
  }

  // Skip beginning of buffer
  if (cnt == 0 || offset >= cnt) return false;

  uint16_t rRel = get_relative_pointer(f, rAbs, offset);

  // Peek data
  _ff_pull(f, p_buffer, rRel);

  return true;
}

// Works on local copies of w and r
// Must be protected by mutexes since in case of an overflow read pointer gets modified
static uint16_t _tu_fifo_peek_at_n(tu_fifo_t* f, uint16_t offset, void * p_buffer, uint16_t n, uint16_t wAbs, uint16_t rAbs)
{
  uint16_t cnt = _tu_fifo_count(f, wAbs, rAbs);

  // Check overflow and correct if required
  if (cnt > f->depth)
  {
    _tu_fifo_correct_read_pointer(f, wAbs);
    rAbs = f->rd_idx;
    cnt = f->depth;
  }

  // Skip beginning of buffer
  if (cnt == 0 || offset >= cnt) return 0;

  // Check if we can read something at and after offset - if too less is available we read what remains
  cnt -= offset;
  if (cnt < n) {
    if (cnt == 0) return 0;
    n = cnt;
  }

  uint16_t rRel = get_relative_pointer(f, rAbs, offset);

  // Peek data
  _ff_pull_n(f, p_buffer, n, rRel);

  return n;
}

// Works on local copies of w and r
static inline uint16_t _tu_fifo_remaining(tu_fifo_t* f, uint16_t wAbs, uint16_t rAbs)
{
  return f->depth - _tu_fifo_count(f, wAbs, rAbs);
}

/******************************************************************************/
/*!
    @brief Get number of items in FIFO.

    As this function only reads the read and write pointers once, this function is
    reentrant and thus thread and ISR save without any mutexes. In case an
    overflow occurred, this function return f.depth at maximum. Overflows are
    checked and corrected for in the read functions!

    @param[in]  f
                Pointer to the FIFO buffer to manipulate

    @returns Number of items in FIFO
 */
/******************************************************************************/
uint16_t tu_fifo_count(tu_fifo_t* f)
{
  return tu_min16(_tu_fifo_count(f, f->wr_idx, f->rd_idx), f->depth);
}

/******************************************************************************/
/*!
    @brief Check if FIFO is empty.

    As this function only reads the read and write pointers once, this function is
    reentrant and thus thread and ISR save without any mutexes.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate

    @returns Number of items in FIFO
 */
/******************************************************************************/
bool tu_fifo_empty(tu_fifo_t* f)
{
  return _tu_fifo_empty(f->wr_idx, f->rd_idx);
}

/******************************************************************************/
/*!
    @brief Check if FIFO is full.

    As this function only reads the read and write pointers once, this function is
    reentrant and thus thread and ISR save without any mutexes.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate

    @returns Number of items in FIFO
 */
/******************************************************************************/
bool tu_fifo_full(tu_fifo_t* f)
{
  return _tu_fifo_full(f, f->wr_idx, f->rd_idx);
}

/******************************************************************************/
/*!
    @brief Get remaining space in FIFO.

    As this function only reads the read and write pointers once, this function is
    reentrant and thus thread and ISR save without any mutexes.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate

    @returns Number of items in FIFO
 */
/******************************************************************************/
uint16_t tu_fifo_remaining(tu_fifo_t* f)
{
  return _tu_fifo_remaining(f, f->wr_idx, f->rd_idx);
}

/******************************************************************************/
/*!
    @brief Check if overflow happened.

     BE AWARE - THIS FUNCTION MIGHT NOT GIVE A CORRECT ANSWERE IN CASE WRITE POINTER "OVERFLOWS"
     Only one overflow is allowed for this function to work e.g. if depth = 100, you must not
     write more than 2*depth-1 items in one rush without updating write pointer. Otherwise
     write pointer wraps and your pointer states are messed up. This can only happen if you
     use DMAs, write functions do not allow such an error. Avoid such nasty things!

     All reading functions (read, peek) check for overflows and correct read pointer on their own such
     that latest items are read.
     If required (e.g. for DMA use) you can also correct the read pointer by
     tu_fifo_correct_read_pointer().

    @param[in]  f
                Pointer to the FIFO buffer to manipulate

    @returns True if overflow happened
 */
/******************************************************************************/
bool tu_fifo_overflowed(tu_fifo_t* f)
{
  return _tu_fifo_overflowed(f, f->wr_idx, f->rd_idx);
}

// Only use in case tu_fifo_overflow() returned true!
void tu_fifo_correct_read_pointer(tu_fifo_t* f)
{
  tu_fifo_lock(f);
  _tu_fifo_correct_read_pointer(f, f->wr_idx);
  tu_fifo_unlock(f);
}

/******************************************************************************/
/*!
    @brief Read one element out of the buffer.

    This function will return the element located at the array index of the
    read pointer, and then increment the read pointer index.
    This function checks for an overflow and corrects read pointer if required.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  buffer
                Pointer to the place holder for data read from the buffer

    @returns TRUE if the queue is not empty
 */
/******************************************************************************/
bool tu_fifo_read(tu_fifo_t* f, void * buffer)
{
  tu_fifo_lock(f);                                          // TODO: Here we may distinguish for read and write pointer mutexes!

  // Peek the data
  bool ret = _tu_fifo_peek_at(f, 0, buffer, f->wr_idx, f->rd_idx);    // f->rd_idx might get modified in case of an overflow so we can not use a local variable

  // Advance pointer
  f->rd_idx = advance_pointer(f, f->rd_idx, ret);

  tu_fifo_unlock(f);
  return ret;
}

/******************************************************************************/
/*!
    @brief This function will read n elements from the array index specified by
    the read pointer and increment the read index.
    This function checks for an overflow and corrects read pointer if required.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  buffer
                The pointer to data location
    @param[in]  n
                Number of element that buffer can afford

    @returns number of items read from the FIFO
 */
/******************************************************************************/
uint16_t tu_fifo_read_n(tu_fifo_t* f, void * buffer, uint16_t n)
{
  tu_fifo_lock(f);                                          // TODO: Here we may distinguish for read and write pointer mutexes!

  // Peek the data
  n = _tu_fifo_peek_at_n(f, 0, buffer, n, f->wr_idx, f->rd_idx);        // f->rd_idx might get modified in case of an overflow so we can not use a local variable

  // Advance read pointer
  f->rd_idx = advance_pointer(f, f->rd_idx, n);

  tu_fifo_unlock(f);
  return n;
}

/******************************************************************************/
/*!
    @brief This function will read n elements from the array index specified by
    the read pointer and increment the read index. It copies the elements
    into another FIFO and as such takes care of wraps etc.
    This function checks for an overflow and corrects read pointer if required.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  f_target
                Pointer to target FIFO i.e. to copy into
    @param[in]  offset
                Position to read from in the FIFO buffer with respect to read pointer
    @param[in]  n
                Number of items to peek

    @returns number of items read from the FIFO
 */
/******************************************************************************/
uint16_t tu_fifo_read_n_into_other_fifo(tu_fifo_t* f, tu_fifo_t* f_target, uint16_t offset, uint16_t n)
{
  tu_fifo_lock(f);            // TODO: Here we may distinguish for read and write pointer mutexes!

  // Conduct copy
  n = tu_fifo_peek_n_into_other_fifo(f, f_target, offset, n);

  // Advance read pointer
  f->rd_idx = advance_pointer(f, f->rd_idx, n);

  tu_fifo_unlock(f);

  return n;
}

/******************************************************************************/
/*!
    @brief Read one item without removing it from the FIFO.
    This function checks for an overflow and corrects read pointer if required.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  offset
                Position to read from in the FIFO buffer with respect to read pointer
    @param[in]  p_buffer
                Pointer to the place holder for data read from the buffer

    @returns TRUE if the queue is not empty
 */
/******************************************************************************/
bool tu_fifo_peek_at(tu_fifo_t* f, uint16_t offset, void * p_buffer)
{
  tu_fifo_lock(f);                                          // TODO: Here we may distinguish for read and write pointer mutexes!
  bool ret = _tu_fifo_peek_at(f, offset, p_buffer, f->wr_idx, f->rd_idx);
  tu_fifo_unlock(f);
  return ret;
}

/******************************************************************************/
/*!
    @brief Read n items without removing it from the FIFO
    This function checks for an overflow and corrects read pointer if required.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  offset
                Position to read from in the FIFO buffer with respect to read pointer
    @param[in]  p_buffer
                Pointer to the place holder for data read from the buffer
    @param[in]  n
                Number of items to peek

    @returns Number of bytes written to p_buffer
 */
/******************************************************************************/
uint16_t tu_fifo_peek_at_n(tu_fifo_t* f, uint16_t offset, void * p_buffer, uint16_t n)
{
  tu_fifo_lock(f);                                          // TODO: Here we may distinguish for read and write pointer mutexes!
  bool ret = _tu_fifo_peek_at_n(f, offset, p_buffer, n, f->wr_idx, f->rd_idx);
  tu_fifo_unlock(f);
  return ret;
}

/******************************************************************************/
/*!
    @brief Read n items without removing it from the FIFO and copy them into another FIFO.
    This function checks for an overflow and corrects read pointer if required.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  f_target
                Pointer to target FIFO i.e. to copy into
    @param[in]  offset
                Position to read from in the FIFO buffer with respect to read pointer
    @param[in]  n
                Number of items to peek

    @returns Number of bytes written to p_buffer
 */
/******************************************************************************/
uint16_t tu_fifo_peek_n_into_other_fifo (tu_fifo_t* f, tu_fifo_t* f_target, uint16_t offset, uint16_t n)
{
  // Copy is only possible if both FIFOs have common element size
  TU_VERIFY(f->item_size == f_target->item_size);

  // Work on local copies on case any pointer changes in between (only necessary if something is written into FIFO f in the meantime)
  uint16_t f_wr_idx = f->wr_idx;
  uint16_t f_rd_idx = f->rd_idx;

  uint16_t cnt = _tu_fifo_count(f, f_wr_idx, f_rd_idx);

  // Check overflow and correct if required
  if (cnt > f->depth)
  {
    _tu_fifo_correct_read_pointer(f, f->wr_idx);
    f_rd_idx = f->rd_idx;
    cnt = f->depth;
  }

  // Skip beginning of buffer
  if (cnt == 0 || offset >= cnt) return 0;

  // Check if we can read something at and after offset - if too less is available we read what remains
  cnt -= offset;
  if (cnt < n) {
    if (cnt == 0) return 0;
    n = cnt;
  }

  tu_fifo_lock(f_target);     // Lock both read and write pointers - in case of an overwritable FIFO both may be modified

  uint16_t wr_rel_tgt = get_relative_pointer(f_target, f_target->wr_idx, 0);

  if (!f_target->overwritable)
  {
    // Not overwritable limit up to full
    n = tu_min16(n, tu_fifo_remaining(f_target));
  }

  // Advance write pointer - not required for later
  f_target->wr_idx = advance_pointer(f_target, f_target->wr_idx, n);

  if (n >= f_target->depth)
  {
    offset += n - f_target->depth;

    // We start writing at the read pointer's position since we fill the complete
    // buffer and we do not want to modify the read pointer within a write function!
    // This would end up in a race condition with read functions!
    wr_rel_tgt = get_relative_pointer(f_target, f_target->rd_idx, 0);

    n = f_target->depth;

    // Update write pointer
    f_target->wr_idx = advance_pointer(f_target, f_target->rd_idx, n);
  }

  // Copy linear size
  uint16_t sz = f_target->depth - wr_rel_tgt;
  _tu_fifo_peek_at_n(f, offset, &f_target->buffer[wr_rel_tgt], sz, f_wr_idx, f_rd_idx);

  if (n > sz)
  {
    // Copy remaining, now wrapped part, into target buffer
    _tu_fifo_peek_at_n(f, offset + sz, f_target->buffer, n-sz, f_wr_idx, f_rd_idx);
  }

  tu_fifo_unlock(f_target);

  return n;
}

/******************************************************************************/
/*!
    @brief Write one element into the buffer.

    This function will write one element into the array index specified by
    the write pointer and increment the write index.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  data
                The byte to add to the FIFO

    @returns TRUE if the data was written to the FIFO (overwrittable
             FIFO will always return TRUE)
 */
/******************************************************************************/
bool tu_fifo_write(tu_fifo_t* f, const void * data)
{
  tu_fifo_lock(f);

  uint16_t w = f->wr_idx;

  if ( _tu_fifo_full(f, w, f->rd_idx) && !f->overwritable ) return false;

  uint16_t wRel = get_relative_pointer(f, w, 0);

  // Write data
  _ff_push(f, data, wRel);

  // Advance pointer
  f->wr_idx = advance_pointer(f, w, 1);

  tu_fifo_unlock(f);

  return true;
}

/******************************************************************************/
/*!
    @brief This function will write n elements into the array index specified by
    the write pointer and increment the write index.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  data
                The pointer to data to add to the FIFO
    @param[in]  count
                Number of element
    @return Number of written elements
 */
/******************************************************************************/
uint16_t tu_fifo_write_n(tu_fifo_t* f, const void * data, uint16_t n)
{
  if ( n == 0 ) return 0;

  tu_fifo_lock(f);

  uint16_t w = f->wr_idx, r = f->rd_idx;
  uint8_t const* buf8 = (uint8_t const*) data;

  if (!f->overwritable)
  {
    // Not overwritable limit up to full
    n = tu_min16(n, _tu_fifo_remaining(f, w, r));
  }
  else if (n >= f->depth)
  {
    // Only copy last part
    buf8 = buf8 + (n - f->depth) * f->item_size;
    n = f->depth;

    // We start writing at the read pointer's position since we fill the complete
    // buffer and we do not want to modify the read pointer within a write function!
    // This would end up in a race condition with read functions!
    w = r;
  }

  uint16_t wRel = get_relative_pointer(f, w, 0);

  // Write data
  _ff_push_n(f, buf8, n, wRel);

  // Advance pointer
  f->wr_idx = advance_pointer(f, w, n);

  tu_fifo_unlock(f);

  return n;
}

/******************************************************************************/
/*!
    @brief Clear the fifo read and write pointers

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
 */
/******************************************************************************/
bool tu_fifo_clear(tu_fifo_t *f)
{
  tu_fifo_lock(f);
  f->rd_idx = f->wr_idx = 0;
  tu_fifo_unlock(f);

  return true;
}

/******************************************************************************/
/*!
    @brief Change the fifo mode to overwritable or not overwritable

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  overwritable
                Overwritable mode the fifo is set to
*/
/******************************************************************************/
bool tu_fifo_set_overwritable(tu_fifo_t *f, bool overwritable)
{
  tu_fifo_lock(f);

  f->overwritable = overwritable;

  tu_fifo_unlock(f);

  return true;
}

/******************************************************************************/
/*!
    @brief Advance write pointer - intended to be used in combination with DMA.
    It is possible to fill the FIFO by use of a DMA in circular mode. Within
    DMA ISRs you may update the write pointer to be able to read from the FIFO.
    As long as the DMA is the only process writing into the FIFO this is safe
    to use.

    USE WITH CARE - WE DO NOT CONDUCT SAFTY CHECKS HERE!

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  n
                Number of items the write pointer moves forward
 */
/******************************************************************************/
void tu_fifo_advance_write_pointer(tu_fifo_t *f, uint16_t n)
{
  f->wr_idx = advance_pointer(f, f->wr_idx, n);
}

/******************************************************************************/
/*!
    @brief Advance read pointer - intended to be used in combination with DMA.
    It is possible to read from the FIFO by use of a DMA in linear mode. Within
    DMA ISRs you may update the read pointer to be able to again write into the
    FIFO. As long as the DMA is the only process reading from the FIFO this is
    safe to use.

    USE WITH CARE - WE DO NOT CONDUCT SAFTY CHECKS HERE!

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  n
                Number of items the read pointer moves forward
 */
/******************************************************************************/
void tu_fifo_advance_read_pointer(tu_fifo_t *f, uint16_t n)
{
  f->rd_idx = advance_pointer(f, f->rd_idx, n);
}
