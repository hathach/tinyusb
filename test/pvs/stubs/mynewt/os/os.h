// Stub of the Apache Mynewt kernel API osal_mynewt.h uses, for PVS-Studio compile-only analysis.
#ifndef PVS_STUB_MYNEWT_OS_H_
#define PVS_STUB_MYNEWT_OS_H_

#include <stddef.h>
#include <stdint.h>

#define STAILQ_ENTRY(type)      struct { struct type* stqe_next; }
#define STAILQ_HEAD(name, type) struct name { struct type* stqh_first; struct type** stqh_last; }
#define STAILQ_EMPTY(head)      ((head)->stqh_first == NULL)

typedef uint32_t os_time_t;
typedef uint32_t os_sr_t;
typedef int      os_error_t;

#define OS_OK            0
#define OS_TIMEOUT_NEVER (UINT32_MAX)

os_sr_t os_arch_save_sr(void);
void os_arch_restore_sr(os_sr_t sr);
#define OS_ENTER_CRITICAL(sr) ((sr) = os_arch_save_sr())
#define OS_EXIT_CRITICAL(sr)  (os_arch_restore_sr(sr))

struct os_task { uint8_t t_prio; };
struct os_sem { uint16_t sem_tokens; };
struct os_mutex { uint16_t mu_level; };
struct os_mempool { uint16_t mp_num_free; uint32_t mp_block_size; };

struct os_event;
typedef void os_event_fn(struct os_event* ev);
struct os_event {
  uint8_t      ev_queued;
  os_event_fn* ev_cb;
  void*        ev_arg;
  STAILQ_ENTRY(os_event) ev_next;
};
struct os_eventq {
  struct os_task* evq_owner;
  STAILQ_HEAD(, os_event) evq_list;
};

struct os_task* os_sched_get_current_task(void);
void os_time_delay(os_time_t osticks);
os_time_t os_time_get(void);
os_time_t os_time_ms_to_ticks32(uint32_t ms);
uint32_t os_time_ticks_to_ms32(os_time_t ticks);

os_error_t os_sem_init(struct os_sem* sem, uint16_t tokens);
os_error_t os_sem_release(struct os_sem* sem);
os_error_t os_sem_pend(struct os_sem* sem, os_time_t timeout);

os_error_t os_mutex_init(struct os_mutex* mu);
os_error_t os_mutex_release(struct os_mutex* mu);
os_error_t os_mutex_pend(struct os_mutex* mu, os_time_t timeout);

os_error_t os_mempool_init(struct os_mempool* mp, uint16_t blocks, uint32_t block_size, void* membuf, const char* name);
void* os_memblock_get(struct os_mempool* mp);
os_error_t os_memblock_put(struct os_mempool* mp, void* block_addr);

void os_eventq_init(struct os_eventq* evq);
void os_eventq_put(struct os_eventq* evq, struct os_event* ev);
struct os_event* os_eventq_poll(struct os_eventq** evq, int nevqs, os_time_t timo);

#endif
