/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_OSAL_NONE_H_
#define TUSB_OSAL_NONE_H_

#ifdef __cplusplus
extern "C" {
#endif

// osal_time_millis() is not provided, tusb_time_millis_api() must be implemented by user application

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
// Bare-metal single context: return a non-NULL sentinel so equality compares true.
typedef void* osal_task_handle_t;

TU_ATTR_ALWAYS_INLINE static inline osal_task_handle_t osal_task_get_current_handle(void) {
  return (osal_task_handle_t) 1;
}

// Bare-metal has no scheduler to yield to; this is dead code in practice because
// callers gate it on running outside the host task, which can't happen here.
TU_ATTR_ALWAYS_INLINE static inline void osal_task_delay(uint32_t msec) {
  (void) msec;
}

//--------------------------------------------------------------------+
// Spinlock API
//--------------------------------------------------------------------+
// Note: This implementation is designed for bare-metal single-core systems without RTOS.
// - Supports nested locking within the same execution context
// - NOT suitable for true SMP (Symmetric Multi-Processing) systems
// - NOT thread-safe for multi-threaded environments
// - Primarily manages interrupt enable/disable state for critical sections
typedef struct {
  void (* interrupt_set)(bool enabled);
  uint32_t nested_count;
} osal_spinlock_t;

// For SMP, spinlock must be locked by hardware, cannot just use interrupt
#define OSAL_SPINLOCK_DEF(_name, _int_set) \
  osal_spinlock_t _name = { .interrupt_set = _int_set, .nested_count = 0 }

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_init(osal_spinlock_t *ctx) {
  (void) ctx;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_deinit(osal_spinlock_t *ctx) {
  (void) ctx;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_lock(osal_spinlock_t *ctx, bool in_isr) {
  // Disable interrupts first to make nested_count increment atomic
  if (!in_isr && ctx->nested_count == 0) {
    ctx->interrupt_set(false);
  }
  ctx->nested_count++;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_unlock(osal_spinlock_t *ctx, bool in_isr) {
  if (ctx->nested_count == 0) {
    return; // spin is not locked to begin with
  }

  ctx->nested_count--;

  // Only re-enable interrupts when fully unlocked
  if (!in_isr && ctx->nested_count == 0) {
    ctx->interrupt_set(true);
  }
}

//--------------------------------------------------------------------+
// Binary Semaphore API
//--------------------------------------------------------------------+
typedef struct {
  volatile uint16_t count;
} osal_semaphore_def_t;

typedef osal_semaphore_def_t* osal_semaphore_t;

TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef) {
  semdef->count = 0;
  return semdef;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_delete(osal_semaphore_t semd_hdl) {
  (void) semd_hdl;
  return true; // nothing to do
}


TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr) {
  (void) in_isr;
  sem_hdl->count = (uint16_t) (sem_hdl->count + 1u);
  return true;
}

// TODO blocking for now
TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec) {
  (void) msec;

  while (sem_hdl->count == 0) {}
  sem_hdl->count = (uint16_t) (sem_hdl->count - 1u);

  return true;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl) {
  sem_hdl->count = 0;
}

//--------------------------------------------------------------------+
// MUTEX API
// Within tinyusb, mutex is never used in ISR context
//--------------------------------------------------------------------+
typedef osal_semaphore_def_t osal_mutex_def_t;
typedef osal_semaphore_t osal_mutex_t;

#if OSAL_MUTEX_REQUIRED
// Note: multiple cores MCUs usually do provide IPC API for mutex
// or we can use std atomic function

TU_ATTR_ALWAYS_INLINE static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef) {
  mdef->count = 1;
  return mdef;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_delete(osal_mutex_t mutex_hdl) {
  (void) mutex_hdl;
  return true; // nothing to do
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_lock (osal_mutex_t mutex_hdl, uint32_t msec) {
  return osal_semaphore_wait(mutex_hdl, msec);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl) {
  return osal_semaphore_post(mutex_hdl, false);
}

#else

#define osal_mutex_create(_mdef)          (NULL)
#define osal_mutex_lock(_mutex_hdl, _ms)  (true)
#define osal_mutex_unlock(_mutex_hdl)     (true)

#endif

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
// A ring of fixed-size items with a single reader, the stack's task. Reading takes no lock: a writer
// publishes wr_idx only after the item is complete, and a slot is reused only after the reader has moved
// rd_idx past it. Writers are the USB ISR, which runs to completion against the task, and the task, which
// masks the USB interrupt (interrupt_set) while it writes. So the task never holds off the ISR to read,
// which matters on controllers that NAK every endpoint until the ISR runs (WCH USBHS INT_BUSY).
//
// Items are copied as pointer-sized elements: an item type must be a whole number of intptr_t and
// intptr_t aligned (event types are; they hold pointers or 32-bit fields and are not packed), which
// OSAL_QUEUE_DEF checks. Wrap a packed or byte type in a union with an intptr_t to queue it. Callers pass
// objects of the queue's type, so their alignment is the type's.
//
// Indexes run to twice the depth, so full and empty differ without a spare slot or a shared count. The
// writer moves only wr_idx and the reader only rd_idx.
typedef struct {
  void (* interrupt_set)(bool enabled);
  intptr_t* buffer;
  uint16_t item_len; // item size in intptr_t elements
  uint16_t depth;
  volatile uint16_t wr_idx;
  volatile uint16_t rd_idx;
} osal_queue_def_t;

typedef osal_queue_def_t* osal_queue_t;

// Alignment of _type without C11 _Alignof: the offset of a member that follows a char
#define OSAL_QUEUE_ALIGNOF_(_type) offsetof(struct { char c_; _type t_; }, t_)

// _int_set is used as mutex in OS NONE (disable/enable USB ISR)
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type)                                                 \
  TU_VERIFY_STATIC(sizeof(_type) % sizeof(intptr_t) == 0 &&                                            \
                   OSAL_QUEUE_ALIGNOF_(_type) % sizeof(intptr_t) == 0,                                 \
                   "queue item must be whole, aligned intptr_t elements");                             \
  intptr_t         _name##_buf[(_depth) * (sizeof(_type) / sizeof(intptr_t))];                         \
  osal_queue_def_t _name = {.interrupt_set = _int_set,                                                 \
                            .buffer        = _name##_buf,                                              \
                            .item_len      = sizeof(_type) / sizeof(intptr_t),                         \
                            .depth         = _depth}

TU_ATTR_ALWAYS_INLINE static inline uint16_t osal_queue_next_(osal_queue_def_t const* q, uint16_t idx) {
  return (uint16_t) ((idx + 1u == 2u * q->depth) ? 0u : idx + 1u);
}

TU_ATTR_ALWAYS_INLINE static inline intptr_t* osal_queue_item_(osal_queue_def_t const* q, uint16_t idx) {
  uint32_t const slot = (uint32_t) (idx >= q->depth ? idx - q->depth : idx);
  return q->buffer + slot * q->item_len;
}

// Copy one item between the queue storage and the caller's object
TU_ATTR_ALWAYS_INLINE static inline void osal_queue_copy_(osal_queue_def_t const* q, void* dst, void const* src) {
  // The item is accessed as intptr_t elements: like the rest of TinyUSB this needs -fno-strict-aliasing
  // (TinyUSB's build sets it). volatile keeps the element accesses in order with the volatile indexes,
  // which the lock-free publish relies on, and keeps compilers from turning the loop into a memcpy call.
  volatile intptr_t* d = (volatile intptr_t*) dst;
  volatile intptr_t const* s = (volatile intptr_t const*) src;
  uint32_t const len = q->item_len;
  for (uint32_t i = 0; i < len; i++) {
    d[i] = s[i];
  }
}

TU_ATTR_ALWAYS_INLINE static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef) {
  qdef->wr_idx = 0;
  qdef->rd_idx = 0;
  return (osal_queue_t) qdef;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_delete(osal_queue_t qhdl) {
  (void) qhdl;
  return true; // nothing to do
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec) {
  (void) msec; // not used, always behave as msec = 0

  // Lock-free: only the task reads. Read wr_idx, then the item, then release the slot through rd_idx.
  uint16_t const rd = qhdl->rd_idx;
  if (qhdl->wr_idx == rd) {
    return false;
  }
  osal_queue_copy_(qhdl, data, osal_queue_item_(qhdl, rd));
  qhdl->rd_idx = osal_queue_next_(qhdl, rd);
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_send(osal_queue_t qhdl, void const* data, bool in_isr) {
  if (!in_isr) {
    qhdl->interrupt_set(false);
  }

  uint16_t const wr = qhdl->wr_idx;
  uint16_t const rd = qhdl->rd_idx;
  uint16_t const count = (uint16_t) (wr >= rd ? (unsigned) (wr - rd) : wr + 2u * qhdl->depth - rd);
  bool const success = (count < qhdl->depth);
  if (success) {
    osal_queue_copy_(qhdl, osal_queue_item_(qhdl, wr), data);
    qhdl->wr_idx = osal_queue_next_(qhdl, wr);
  }

  if (!in_isr) {
    qhdl->interrupt_set(true);
  }

  return success;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_empty(osal_queue_t qhdl) {
  // Lock-free on purpose: also polled with interrupts enabled by the bare-metal tud/tuh_task fast path,
  // where a stale empty only defers the event to the next poll. One volatile access per statement, so the
  // read order is defined (IAR Pa082)
  uint16_t const wr = qhdl->wr_idx;
  return wr == qhdl->rd_idx;
}

#ifdef __cplusplus
}
#endif

#endif
