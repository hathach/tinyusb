// Stub of the RT-Thread 5.x kernel API osal_rtthread.h uses, for PVS-Studio compile-only analysis.
#ifndef PVS_STUB_RTTHREAD_H_
#define PVS_STUB_RTTHREAD_H_

typedef signed int    rt_int32_t;
typedef unsigned int  rt_uint32_t;
typedef unsigned short rt_uint16_t;
typedef unsigned char rt_uint8_t;
typedef rt_uint32_t   rt_tick_t;
typedef long          rt_err_t;
typedef unsigned long rt_size_t;
typedef long          rt_ssize_t;

#define RT_VERSION_MAJOR   5
#define RT_TICK_PER_SECOND 1000
#define RT_EOK             0
#define RT_IPC_FLAG_PRIO   0x01
#define RT_IPC_CMD_RESET   0x01

struct rt_thread { rt_uint32_t stat; };
typedef struct rt_thread* rt_thread_t;

struct rt_spinlock { volatile rt_uint32_t lock; };

struct rt_semaphore { rt_uint32_t value; };
typedef struct rt_semaphore* rt_sem_t;

struct rt_mutex { rt_uint32_t hold; };
typedef struct rt_mutex* rt_mutex_t;

struct rt_messagequeue {
  rt_uint16_t msg_size;
  rt_uint16_t max_msgs;
  rt_uint16_t entry;
};
typedef struct rt_messagequeue* rt_mq_t;

rt_thread_t rt_thread_self(void);
rt_err_t rt_thread_mdelay(rt_int32_t ms);
rt_tick_t rt_tick_get(void);
rt_tick_t rt_tick_from_millisecond(rt_int32_t ms);

void rt_spin_lock_init(struct rt_spinlock* lock);
void rt_spin_lock(struct rt_spinlock* lock);
void rt_spin_unlock(struct rt_spinlock* lock);

rt_err_t rt_sem_init(rt_sem_t sem, const char* name, rt_uint32_t value, rt_uint8_t flag);
rt_err_t rt_sem_detach(rt_sem_t sem);
rt_err_t rt_sem_take(rt_sem_t sem, rt_int32_t timeout);
rt_err_t rt_sem_release(rt_sem_t sem);
rt_err_t rt_sem_control(rt_sem_t sem, int cmd, void* arg);

rt_err_t rt_mutex_init(rt_mutex_t mutex, const char* name, rt_uint8_t flag);
rt_err_t rt_mutex_detach(rt_mutex_t mutex);
rt_err_t rt_mutex_take(rt_mutex_t mutex, rt_int32_t time);
rt_err_t rt_mutex_release(rt_mutex_t mutex);

rt_err_t rt_mq_init(rt_mq_t mq, const char* name, void* msgpool, rt_size_t msg_size, rt_size_t pool_size, rt_uint8_t flag);
rt_err_t rt_mq_detach(rt_mq_t mq);
rt_ssize_t rt_mq_recv(rt_mq_t mq, void* buffer, rt_size_t size, rt_int32_t timeout);
rt_err_t rt_mq_send(rt_mq_t mq, const void* buffer, rt_size_t size);

#endif
