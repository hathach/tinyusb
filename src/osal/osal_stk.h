/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-FileCopyrightText: Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com> (STK port)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 *
 * ---------------------------------------------------------------------------
 * This port can bind to either of STK's two APIs, selected via CFG_TUH_OSAL_STK_USE_CPP:
 *
 *   CFG_TUH_OSAL_STK_USE_CPP=0 (default) - STK's pure C API (stk_c.h). Every function below is
 *     a "static inline" wrapper living entirely in this header, exactly like the header-only
 *     osal_freertos.h / osal_threadx.h / osal_rtx4.h ports. No companion .cpp file is needed and
 *     this header stays includable from TinyUSB's plain-C translation units directly.
 *
 *   CFG_TUH_OSAL_STK_USE_CPP=1 - STK's native C++ API (stk::sync::Mutex/Semaphore/MessageQueue,
 *     stk::hw::CriticalSection). STK's C++ API is not includable from plain C, so in this mode
 *     every STK-touching function is only *declared* here (as extern "C") and *defined* in the
 *     companion osal_stk.cpp, which must be added to the build and is compiled as C++. Pick this 
 *     mode only if something else in your build already requires STK's C++ API; the C backend 
 *     above covers everything this OSAL port needs.
 *
 * The other consequence of the C++ backend being C++-only: TinyUSB's OSAL_*_DEF macros are meant
 * to produce statically-allocated control blocks at file scope in plain C (mirroring FreeRTOS's
 * configSUPPORT_STATIC_ALLOCATION path) - but a C struct literal can't invoke a C++ constructor.
 * So in CFG_TUH_OSAL_STK_USE_CPP=1 mode, each opaque struct reserves fixed-size byte storage,
 * sized to fit the corresponding stk::sync:: object; osal_stk.cpp placement-constructs the real
 * object into that storage the first time it's created, and a static_assert there guards every
 * size guess - raise the matching *_STORAGE_WORDS macro below if one of those ever fires (e.g.
 * after an STK upgrade changes a class's layout). The C backend has no such concern: stk_c.h's
 * own stk_sem_mem_t / stk_mutex_mem_t / stk_msgq_mem_t types are already correctly-sized POD
 * structs, so they're embedded directly - no size macros, no static_assert, no placement-new.
 * ---------------------------------------------------------------------------
 */

#ifndef TUSB_OSAL_STK_H_
#define TUSB_OSAL_STK_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "tusb_config.h"

#define CFG_TUH_OSAL_STK_VERSION (0x20260801)

#ifndef CFG_TUH_OSAL_STK_USE_CPP
#define CFG_TUH_OSAL_STK_USE_CPP (0)
#endif

#ifndef CFG_TUH_OSAL_STK_SYNC_DEBUG_NAMES
#define CFG_TUH_OSAL_STK_SYNC_DEBUG_NAMES (0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------+
// TASK API
//----------------------------------------------------------------------------+

/* Underlying task-id type isn't part of this header's C-visible surface in either backend (it's
 * a plain STK type, not necessarily a pointer) - uintptr_t is wide enough to hold it regardless
 * of size; a static_assert (osal_stk.cpp, or stk_c.h's own stk_tid_t definition) guards the fit. */
typedef uintptr_t osal_task_handle_t;

//----------------------------------------------------------------------------+
// Spinlock API
//----------------------------------------------------------------------------+
// Backed by CriticalSection (stk::hw::CriticalSection in C++ mode, stk_critical_section_enter_ex()
// / stk_critical_section_exit_ex() in C mode), which already masks local interrupts and acquires
// STK's own global cross-core spinlock as one nestable primitive - unlike the FreeRTOS port's
// ESP32 dual-core case, no separate portMUX_TYPE-style per-instance hardware spinlock is needed
// here. STK's bare, non-interrupt-masking cross-core SpinLock is deliberately NOT used for this:
// STK's own documentation warns it deadlocks if an ISR on the same core tries to acquire a
// SpinLock the interrupted task is already holding - exactly the scenario osal_spin_lock's
// in_isr=true case has to support, so CriticalSection is the correct (and only safe) choice.
//
// CriticalSection's Enter() returns a session token that its matching Exit() call must be given
// back (it encodes the privileged/unprivileged path Enter() took); osal_spinlock_t exists to
// carry that token from osal_spin_lock() to the matching osal_spin_unlock() call - it is
// byte-identical to both stk::hw::CriticalSection::Session and the C API's stk_cs_session_t.
typedef uint8_t osal_spinlock_t;

#define OSAL_SPINLOCK_DEF(_name, _int_set) osal_spinlock_t _name

#if CFG_TUH_OSAL_STK_USE_CPP

//=============================================================================
// C++ backend (stk::sync::*, stk::hw::CriticalSection) - declared here,
// defined in the companion osal_stk.cpp. See the file-level comment above for
// when to use this.
//=============================================================================

//----------------------------------------------------------------------------+
// Semaphore API
//----------------------------------------------------------------------------+

/* Sized to fit a placement-constructed stk::sync::Semaphore - see the file-level note above. */
#ifndef OSAL_STK_SEMAPHORE_STORAGE_WORDS
#define OSAL_STK_SEMAPHORE_STORAGE_WORDS (8U + (CFG_TUH_OSAL_STK_SYNC_DEBUG_NAMES ? 1U : 0U))
#endif

typedef struct {
  uintptr_t storage[OSAL_STK_SEMAPHORE_STORAGE_WORDS];
} osal_semaphore_def_t;
typedef osal_semaphore_def_t *osal_semaphore_t;

//----------------------------------------------------------------------------+
// MUTEX API
//----------------------------------------------------------------------------+

/* Sized to fit a placement-constructed stk::sync::Mutex - see the file-level note above. */
#ifndef OSAL_STK_MUTEX_STORAGE_WORDS
#define OSAL_STK_MUTEX_STORAGE_WORDS (10U + (CFG_TUH_OSAL_STK_SYNC_DEBUG_NAMES ? 1U : 0U))
#endif

typedef struct {
  uintptr_t storage[OSAL_STK_MUTEX_STORAGE_WORDS];
} osal_mutex_def_t;
typedef osal_mutex_def_t *osal_mutex_t;

//----------------------------------------------------------------------------+
// QUEUE API
//----------------------------------------------------------------------------+

/* Sized to fit a placement-constructed stk::sync::MessageQueue control block (the ring buffer
 * itself is separate - see osal_queue_def_t.buf below). See the file-level note above. */
#ifndef OSAL_STK_MSGQUEUE_STORAGE_WORDS
#define OSAL_STK_MSGQUEUE_STORAGE_WORDS (6U + (2U * 8U) + (CFG_TUH_OSAL_STK_SYNC_DEBUG_NAMES ? 1U : 0U))
#endif

typedef struct {
  struct {
    uintptr_t storage[OSAL_STK_MSGQUEUE_STORAGE_WORDS];
  } mq;
  void    *buf;      /* item ring buffer, capacity*item_sz bytes - supplied by OSAL_QUEUE_DEF */
  uint16_t depth;
  uint16_t item_sz;
} osal_queue_def_t;
typedef osal_queue_def_t *osal_queue_t;

// _int_set is not used with an RTOS (STK, like the other RTOS ports here, provides its own
// synchronization - no application-supplied ISR-disable function is needed).
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
  static _type _name##_buf[_depth];                    \
  osal_queue_def_t _name = { .buf = (void *) _name##_buf, .depth = (_depth), .item_sz = sizeof(_type) }

osal_task_handle_t osal_task_get_current_handle(void);
uint32_t osal_time_millis(void);
void osal_task_delay(uint32_t msec);

void osal_spin_init(osal_spinlock_t *ctx);
void osal_spin_deinit(osal_spinlock_t *ctx);
void osal_spin_lock(osal_spinlock_t *ctx, bool in_isr);
void osal_spin_unlock(osal_spinlock_t *ctx, bool in_isr);

osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t *semdef);
bool osal_semaphore_delete(osal_semaphore_t sem_hdl);
bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr);
bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec);
void osal_semaphore_reset(osal_semaphore_t sem_hdl);

osal_mutex_t osal_mutex_create(osal_mutex_def_t *mdef);
bool osal_mutex_delete(osal_mutex_t mutex_hdl);
bool osal_mutex_lock(osal_mutex_t mutex_hdl, uint32_t msec);
bool osal_mutex_unlock(osal_mutex_t mutex_hdl);

osal_queue_t osal_queue_create(osal_queue_def_t *qdef);
bool osal_queue_delete(osal_queue_t qhdl);
bool osal_queue_receive(osal_queue_t qhdl, void *data, uint32_t msec);
bool osal_queue_send(osal_queue_t qhdl, void const *data, bool in_isr);
bool osal_queue_empty(osal_queue_t qhdl);

#else /* !CFG_TUH_OSAL_STK_USE_CPP */

//=============================================================================
// C backend (STK's stk_c.h) - implemented entirely inline below,
// no companion .cpp needed.
//=============================================================================

#include "stk_c.h"

//----------------------------------------------------------------------------+
// Semaphore API
//----------------------------------------------------------------------------+

/* stk_sem_mem_t is already a correctly-sized/aligned POD (see stk_c.h) - embed it directly
 * instead of a hand-sized byte buffer. hdl is filled in by osal_semaphore_create(). */
typedef struct {
  stk_sem_mem_t mem;
  stk_sem_t    *hdl;
} osal_semaphore_def_t;
typedef osal_semaphore_def_t *osal_semaphore_t;

//----------------------------------------------------------------------------+
// MUTEX API
//----------------------------------------------------------------------------+

typedef struct {
  stk_mutex_mem_t mem;
  stk_mutex_t    *hdl;
} osal_mutex_def_t;
typedef osal_mutex_def_t *osal_mutex_t;

//----------------------------------------------------------------------------+
// QUEUE API
//----------------------------------------------------------------------------+

typedef struct {
  stk_msgq_mem_t mem;
  stk_msgq_t    *hdl;
  void          *buf;      /* item ring buffer, capacity*item_sz bytes - supplied by OSAL_QUEUE_DEF */
  uint16_t       depth;
  uint16_t       item_sz;
} osal_queue_def_t;
typedef osal_queue_def_t *osal_queue_t;

// _int_set is not used with an RTOS (STK, like the other RTOS ports here, provides its own
// synchronization - no application-supplied ISR-disable function is needed).
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
  static _type _name##_buf[_depth];                    \
  osal_queue_def_t _name = { .buf = (void *) _name##_buf, .depth = (_depth), .item_sz = sizeof(_type) }

//----------------------------------------------------------------------------+
// TASK API
//----------------------------------------------------------------------------+

static inline osal_task_handle_t osal_task_get_current_handle(void)
{
  return (osal_task_handle_t)stk_tid();
}

static inline uint32_t osal_time_millis(void)
{
  return (uint32_t)stk_time_now_ms();
}

static inline void osal_task_delay(uint32_t msec)
{
  const stk_timeout_t ms_clamped =
      (msec > (uint32_t)INT32_MAX) ? (stk_timeout_t)INT32_MAX : (stk_timeout_t)msec;
  stk_sleep_ms(ms_clamped);
}

//----------------------------------------------------------------------------+
// Small helper
//----------------------------------------------------------------------------+

/* OSAL_TIMEOUT_WAIT_FOREVER (UINT32_MAX) -> STK_WAIT_INFINITE; anything else -> STK ticks,
 * via stk_ticks_from_ms_clamped_to_timeout() for overflow-safe conversion across tick rates
 * (see stk_c.h). */
static inline stk_timeout_t osal_stk_timeout_from_ms(uint32_t msec)
{
  if (msec == OSAL_TIMEOUT_WAIT_FOREVER) {
    return STK_WAIT_INFINITE;
  }
  const stk_timeout_t ms_clamped =
      (msec > (uint32_t)INT32_MAX) ? (stk_timeout_t)INT32_MAX : (stk_timeout_t) msec;
  return stk_ticks_from_ms_clamped_to_timeout(ms_clamped);
}

//----------------------------------------------------------------------------+
// Spinlock API
//----------------------------------------------------------------------------+

static inline void osal_spin_init(osal_spinlock_t *ctx)
{
  if (ctx != NULL) {
    *ctx = (osal_spinlock_t)STK_DEFAULT_CS_SESSION;
  }
}

static inline void osal_spin_deinit(osal_spinlock_t *ctx)
{
  (void)ctx;
}

static inline void osal_spin_lock(osal_spinlock_t *ctx, bool in_isr)
{
  (void)in_isr; /* stk_critical_section_enter_ex() is safe from ISR context */

  /* The return value encodes which privileged/unprivileged path was taken and MUST be handed
   * back to the matching stk_critical_section_exit_ex() call - passing STK_DEFAULT_CS_SESSION
   * there instead would restore the wrong path on unprivileged/TrustZone callers. */
  const stk_cs_session_t ses = stk_critical_section_enter_ex(STK_DEFAULT_CS_SESSION);
  if (ctx != NULL) {
    *ctx = (osal_spinlock_t)ses;
  }
}

static inline void osal_spin_unlock(osal_spinlock_t *ctx, bool in_isr)
{
  (void)in_isr; /* stk_critical_section_exit_ex() is safe from ISR context */

  const stk_cs_session_t ses =
      ((ctx != NULL) ? (stk_cs_session_t)(*ctx) : (stk_cs_session_t)STK_DEFAULT_CS_SESSION);
  stk_critical_section_exit_ex(ses);
}

//----------------------------------------------------------------------------+
// Semaphore API
//----------------------------------------------------------------------------+

static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t *semdef)
{
  /* Always constructed at count 0 (not-yet-signaled), matching every other OSAL backend here.
   * max_count 0 means "use STK's default maximum" (65534) - see stk_sem_create() in stk_c.h. */
  semdef->hdl = stk_sem_create(&semdef->mem, (uint32_t)sizeof(semdef->mem), 0U, 0U);
  return ((semdef->hdl != NULL) ? semdef : NULL);
}

static inline bool osal_semaphore_delete(osal_semaphore_t sem_hdl)
{
  stk_sem_destroy(sem_hdl->hdl);
  return true;
}

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  (void)in_isr; /* stk_sem_trysignal() is ISR-safe - no separate ISR path needed */

  return stk_sem_trysignal(sem_hdl->hdl);
}

static inline bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec)
{
  return stk_sem_wait(sem_hdl->hdl, osal_stk_timeout_from_ms(msec));
}

static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl)
{
  /* stk_sem_t has no dedicated reset op; drain any pending count/tokens back to 0 instead,
   * mirroring what FreeRTOS's xQueueReset() does for a binary semaphore. */
  while (stk_sem_trywait(sem_hdl->hdl)) {
    /* keep draining */
  }
}

//----------------------------------------------------------------------------+
// MUTEX API (STK's Mutex is already recursive; TinyUSB never relies on that, but it's harmless)
//----------------------------------------------------------------------------+

static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t *mdef)
{
  mdef->hdl = stk_mutex_create(&mdef->mem, (uint32_t)sizeof(mdef->mem));
  return ((mdef->hdl != NULL) ? mdef : NULL);
}

static inline bool osal_mutex_delete(osal_mutex_t mutex_hdl)
{
  stk_mutex_destroy(mutex_hdl->hdl);
  return true;
}

static inline bool osal_mutex_lock(osal_mutex_t mutex_hdl, uint32_t msec)
{
  return stk_mutex_timed_lock(mutex_hdl->hdl, osal_stk_timeout_from_ms(msec));
}

static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
  stk_mutex_unlock(mutex_hdl->hdl);
  return true;
}

//----------------------------------------------------------------------------+
// QUEUE API
//----------------------------------------------------------------------------+

static inline osal_queue_t osal_queue_create(osal_queue_def_t *qdef)
{
  qdef->hdl = stk_msgq_create(&qdef->mem, (uint32_t)sizeof(qdef->mem),
                              (uint8_t *)qdef->buf, (uint32_t)(qdef->depth * qdef->item_sz),
                              (size_t)qdef->depth, (size_t)qdef->item_sz);

  return ((qdef->hdl != NULL) ? qdef : NULL);
}

static inline bool osal_queue_delete(osal_queue_t qhdl)
{
  stk_msgq_destroy(qhdl->hdl);
  return true;
}

static inline bool osal_queue_receive(osal_queue_t qhdl, void *data, uint32_t msec)
{
  return stk_msgq_get(qhdl->hdl, data, osal_stk_timeout_from_ms(msec));
}

static inline bool osal_queue_send(osal_queue_t qhdl, void const *data, bool in_isr)
{
  bool result;

  /* Never block from ISR context, regardless of the caller's requested timeout - matching every
   * other OSAL backend here, whose FromISR() send variants are inherently non-blocking too. */
  if (in_isr) {
    result = stk_msgq_tryput(qhdl->hdl, data);
  } else {
    result = stk_msgq_put(qhdl->hdl, data, STK_WAIT_INFINITE);
  }

  return result;
}

static inline bool osal_queue_empty(osal_queue_t qhdl)
{
  return (stk_msgq_get_count(qhdl->hdl) == 0U);
}

#endif /* CFG_TUH_OSAL_STK_USE_CPP */

#ifdef __cplusplus
}
#endif

#endif
