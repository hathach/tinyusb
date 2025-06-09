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

#ifndef TUSB_OSAL_FREERTOS_H_
#define TUSB_OSAL_FREERTOS_H_

// FreeRTOS Headers
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,FreeRTOS.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,semphr.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,queue.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,task.h)

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

#if configSUPPORT_STATIC_ALLOCATION
typedef StaticSemaphore_t osal_semaphore_def_t;
typedef StaticSemaphore_t osal_mutex_def_t;
#else

// not used therefore defined to the smallest possible type to save space
typedef uint8_t osal_semaphore_def_t;
typedef uint8_t osal_mutex_def_t;
#endif

typedef SemaphoreHandle_t osal_semaphore_t;
typedef SemaphoreHandle_t osal_mutex_t;
typedef QueueHandle_t osal_queue_t;

typedef struct {
  uint16_t depth;
  uint16_t item_sz;
  void*    buf;

#if defined(configQUEUE_REGISTRY_SIZE) && (configQUEUE_REGISTRY_SIZE>0)
  char const* name;
#endif

#if configSUPPORT_STATIC_ALLOCATION
  StaticQueue_t sq;
#endif
} osal_queue_def_t;

#if defined(configQUEUE_REGISTRY_SIZE) && (configQUEUE_REGISTRY_SIZE>0)
  #define _OSAL_Q_NAME(_name) .name = #_name
#else
  #define _OSAL_Q_NAME(_name)
#endif

// _int_set is not used with an RTOS
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
  static _type _name##_##buf[_depth];\
  osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf, _OSAL_Q_NAME(_name) }

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline uint32_t _osal_ms2tick(uint32_t msec) {
  if (msec == OSAL_TIMEOUT_WAIT_FOREVER) { return portMAX_DELAY; }
  if (msec == 0) { return 0; }

  uint32_t ticks = pdMS_TO_TICKS(msec);

  // If configTICK_RATE_HZ is less than 1000 and 1 tick > 1 ms, we still need to delay at least 1 tick
  if (ticks == 0) { ticks = 1; }

  return ticks;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_task_delay(uint32_t msec) {
  vTaskDelay(pdMS_TO_TICKS(msec));
}

//--------------------------------------------------------------------+
// Spinlock API
//--------------------------------------------------------------------+
#define OSAL_SPINLOCK_DEF(_name, _int_set) \
  osal_spinlock_t _name

#if TUSB_MCU_VENDOR_ESPRESSIF
// Espressif critical take spinlock as argument and does not use in_isr
typedef portMUX_TYPE osal_spinlock_t;

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_init(osal_spinlock_t *ctx) {
  spinlock_initialize(ctx);
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_lock(osal_spinlock_t *ctx, bool in_isr) {
  if (!TUP_MCU_MULTIPLE_CORE && in_isr) {
    return; // single core MCU does not need to lock in ISR
  }
  portENTER_CRITICAL(ctx);
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_unlock(osal_spinlock_t *ctx, bool in_isr) {
  if (!TUP_MCU_MULTIPLE_CORE && in_isr) {
    return; // single core MCU does not need to lock in ISR
  }
  portEXIT_CRITICAL(ctx);
}

#else

typedef UBaseType_t osal_spinlock_t;

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_init(osal_spinlock_t *ctx) {
  (void) ctx;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_lock(osal_spinlock_t *ctx, bool in_isr) {
  if (in_isr) {
    if (!TUP_MCU_MULTIPLE_CORE) {
      (void) ctx;
      return; // single core MCU does not need to lock in ISR
    }
    *ctx = taskENTER_CRITICAL_FROM_ISR();
  } else {
    taskENTER_CRITICAL();
  }
}

TU_ATTR_ALWAYS_INLINE static inline void osal_spin_unlock(osal_spinlock_t *ctx, bool in_isr) {
  if (in_isr) {
    if (!TUP_MCU_MULTIPLE_CORE) {
      (void) ctx;
      return; // single core MCU does not need to lock in ISR
    }
    taskEXIT_CRITICAL_FROM_ISR(*ctx);
  } else {
    taskEXIT_CRITICAL();
  }
}

#endif

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t *semdef) {
#if configSUPPORT_STATIC_ALLOCATION
  return xSemaphoreCreateBinaryStatic(semdef);
#else
  (void) semdef;
  return xSemaphoreCreateBinary();
#endif
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_delete(osal_semaphore_t semd_hdl) {
  vSemaphoreDelete(semd_hdl);
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr) {
  if (!in_isr) {
    return xSemaphoreGive(sem_hdl) != 0;
  } else {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t res = xSemaphoreGiveFromISR(sem_hdl, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return res != 0;
  }
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec) {
  return xSemaphoreTake(sem_hdl, _osal_ms2tick(msec));
}

TU_ATTR_ALWAYS_INLINE static inline void osal_semaphore_reset(osal_semaphore_t const sem_hdl) {
  xQueueReset(sem_hdl);
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t *mdef) {
#if configSUPPORT_STATIC_ALLOCATION
  return xSemaphoreCreateMutexStatic(mdef);
#else
  (void) mdef;
  return xSemaphoreCreateMutex();
#endif
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_delete(osal_mutex_t mutex_hdl) {
  vSemaphoreDelete(mutex_hdl);
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_lock(osal_mutex_t mutex_hdl, uint32_t msec) {
  return osal_semaphore_wait(mutex_hdl, msec);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl) {
  return xSemaphoreGive(mutex_hdl);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef) {
  osal_queue_t q;

#if configSUPPORT_STATIC_ALLOCATION
  q = xQueueCreateStatic(qdef->depth, qdef->item_sz, (uint8_t*) qdef->buf, &qdef->sq);
#else
  q = xQueueCreate(qdef->depth, qdef->item_sz);
#endif

#if defined(configQUEUE_REGISTRY_SIZE) && (configQUEUE_REGISTRY_SIZE>0)
  vQueueAddToRegistry(q, qdef->name);
#endif

  return q;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_delete(osal_queue_t qhdl) {
  vQueueDelete(qhdl);
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec) {
  return xQueueReceive(qhdl, data, _osal_ms2tick(msec));
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_send(osal_queue_t qhdl, void const *data, bool in_isr) {
  if (!in_isr) {
    return xQueueSendToBack(qhdl, data, OSAL_TIMEOUT_WAIT_FOREVER) != 0;
  } else {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t res = xQueueSendToBackFromISR(qhdl, data, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return res != 0;
  }
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_empty(osal_queue_t qhdl) {
  return uxQueueMessagesWaiting(qhdl) == 0;
}

#ifdef __cplusplus
}
#endif

#endif
