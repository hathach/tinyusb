// Stub of the STK C API (stk_c.h) osal_stk.h uses, for PVS-Studio compile-only analysis.
// Types and signatures follow osal_stk.h's calls.
#ifndef PVS_STUB_STK_C_H_
#define PVS_STUB_STK_C_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t   stk_timeout_t;
typedef uintptr_t stk_tid_t;
typedef uint8_t   stk_cs_session_t;

#define STK_WAIT_INFINITE      ((stk_timeout_t) INT32_MAX)
#define STK_DEFAULT_CS_SESSION ((stk_cs_session_t) 0)

typedef struct { uintptr_t words[8]; } stk_sem_mem_t;
typedef struct { uintptr_t words[9]; } stk_fastmutex_mem_t;
typedef struct { uintptr_t words[22]; } stk_msgq_mem_t;
typedef struct stk_sem       stk_sem_t;
typedef struct stk_fastmutex stk_fastmutex_t;
typedef struct stk_msgq      stk_msgq_t;

stk_tid_t stk_tid(void);
int64_t stk_time_now_ms(void);
void stk_yield(void);
void stk_sleep(stk_timeout_t ticks);
stk_timeout_t stk_ticks_from_ms_clamped_to_timeout(stk_timeout_t ms);

stk_cs_session_t stk_critical_section_enter_ex(stk_cs_session_t ses);
void stk_critical_section_exit_ex(stk_cs_session_t ses);

stk_sem_t* stk_sem_create(stk_sem_mem_t* mem, uint32_t initial_count, uint32_t max_count);
void stk_sem_destroy(stk_sem_t* sem);
bool stk_sem_trysignal(stk_sem_t* sem);
bool stk_sem_wait(stk_sem_t* sem, stk_timeout_t timeout);
bool stk_sem_trywait(stk_sem_t* sem);

stk_fastmutex_t* stk_fastmutex_create(stk_fastmutex_mem_t* mem);
void stk_fastmutex_destroy(stk_fastmutex_t* mtx);
bool stk_fastmutex_timed_lock(stk_fastmutex_t* mtx, stk_timeout_t timeout);
void stk_fastmutex_unlock(stk_fastmutex_t* mtx);

stk_msgq_t* stk_msgq_create(stk_msgq_mem_t* mem, uint8_t* buf, uint32_t buf_size, size_t capacity, size_t item_size);
void stk_msgq_destroy(stk_msgq_t* q);
bool stk_msgq_get(stk_msgq_t* q, void* data, stk_timeout_t timeout);
bool stk_msgq_put(stk_msgq_t* q, const void* data, stk_timeout_t timeout);
bool stk_msgq_tryput(stk_msgq_t* q, const void* data);
size_t stk_msgq_get_count(stk_msgq_t* q);

#endif
