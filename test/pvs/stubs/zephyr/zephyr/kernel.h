// Stub of the Zephyr kernel API osal_zephyr.h uses, for PVS-Studio compile-only analysis.
#ifndef PVS_STUB_ZEPHYR_KERNEL_H_
#define PVS_STUB_ZEPHYR_KERNEL_H_

#include <stdint.h>

typedef struct { int64_t ticks; } k_timeout_t;
#define K_NO_WAIT  ((k_timeout_t) {0})
#define K_FOREVER  ((k_timeout_t) {-1})
#define K_MSEC(ms) ((k_timeout_t) {(int64_t) (ms)})

struct k_thread { int prio; };
typedef struct k_thread* k_tid_t;

struct k_spinlock { volatile int locked; };
typedef struct { int key; } k_spinlock_key_t;

struct k_sem { unsigned int count; unsigned int limit; };
struct k_mutex { unsigned int lock_count; };
struct k_msgq { size_t msg_size; uint32_t max_msgs; char* buffer_start; uint32_t used_msgs; };

#define K_MSGQ_DEFINE(name, size, max, align)                                    \
  static char __attribute__((aligned(align))) _k_msgq_buf_##name[(max) * (size)]; \
  struct k_msgq name = {(size), (max), _k_msgq_buf_##name, 0}

k_tid_t k_current_get(void);
int32_t k_msleep(int32_t ms);
uint32_t k_uptime_get_32(void);

k_spinlock_key_t k_spin_lock(struct k_spinlock* l);
void k_spin_unlock(struct k_spinlock* l, k_spinlock_key_t key);

int k_sem_init(struct k_sem* sem, unsigned int initial_count, unsigned int limit);
void k_sem_give(struct k_sem* sem);
int k_sem_take(struct k_sem* sem, k_timeout_t timeout);
void k_sem_reset(struct k_sem* sem);

int k_mutex_init(struct k_mutex* mutex);
int k_mutex_lock(struct k_mutex* mutex, k_timeout_t timeout);
int k_mutex_unlock(struct k_mutex* mutex);

int k_msgq_get(struct k_msgq* msgq, void* data, k_timeout_t timeout);
int k_msgq_put(struct k_msgq* msgq, const void* data, k_timeout_t timeout);
uint32_t k_msgq_num_used_get(struct k_msgq* msgq);

#endif
