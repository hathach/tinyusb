// Stub of the RL-ARM RTX4 kernel API osal_rtx4.h uses, for PVS-Studio compile-only analysis.
#ifndef PVS_STUB_RTL_H_
#define PVS_STUB_RTL_H_

typedef unsigned short U16;
typedef unsigned int   U32;
typedef U32            OS_TID;
typedef void*          OS_ID;
typedef U32            OS_RESULT;
typedef U32            OS_SEM[2];
typedef U32            OS_MUT[3];

#define OS_R_OK  0
#define OS_R_TMO 1

#define os_mbx_declare(name, cnt) U32 name[4 + (cnt)]
#define _declare_box(pool, size, cnt) U32 pool[(((size) + 3) / 4) * (cnt) + 3]

int _init_box(void* box_mem, U32 box_size, U32 blk_size);
void* _alloc_box(void* box_mem);
int _free_box(void* box_mem, void* box);

OS_TID os_tsk_self(void);
void os_dly_wait(U16 delay_time);
U32 os_time_get(void);

void os_sem_init(OS_ID semaphore, U16 token_count);
OS_RESULT os_sem_send(OS_ID semaphore);
OS_RESULT os_sem_wait(OS_ID semaphore, U16 timeout);
void isr_sem_send(OS_ID semaphore);

void os_mut_init(OS_ID mutex);
OS_RESULT os_mut_wait(OS_ID mutex, U16 timeout);
OS_RESULT os_mut_release(OS_ID mutex);

void os_mbx_init(OS_ID mailbox, U16 mbx_size);
OS_RESULT os_mbx_wait(OS_ID mailbox, void** message, U16 timeout);
OS_RESULT os_mbx_send(OS_ID mailbox, void* message_ptr, U16 timeout);
OS_RESULT os_mbx_check(OS_ID mailbox);
void isr_mbx_send(OS_ID mailbox, void* message_ptr);

#endif
