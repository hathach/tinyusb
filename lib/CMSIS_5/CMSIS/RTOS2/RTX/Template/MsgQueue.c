#include "cmsis_os2.h"                          // CMSIS RTOS header file
 
/*----------------------------------------------------------------------------
 *      Message Queue creation & usage
 *---------------------------------------------------------------------------*/
 
#define MSGQUEUE_OBJECTS 16                     // number of Message Queue Objects
 
typedef struct {                                // object data type
  uint8_t Buf[32];
  uint8_t Idx;
} MSGQUEUE_OBJ_t;
 
osMessageQueueId_t mid_MsgQueue;                // message queue id
 
osThreadId_t tid_Thread_MsgQueue1;              // thread id 1
osThreadId_t tid_Thread_MsgQueue2;              // thread id 2
 
void Thread_MsgQueue1 (void *argument);         // thread function 1
void Thread_MsgQueue2 (void *argument);         // thread function 2
 
int Init_MsgQueue (void) {
 
  mid_MsgQueue = osMessageQueueNew(MSGQUEUE_OBJECTS, sizeof(MSGQUEUE_OBJ_t), NULL);
  if (mid_MsgQueue == NULL) {
    ; // Message Queue object not created, handle failure
  }
 
  tid_Thread_MsgQueue1 = osThreadNew(Thread_MsgQueue1, NULL, NULL);
  if (tid_Thread_MsgQueue1 == NULL) {
    return(-1);
  }
  tid_Thread_MsgQueue2 = osThreadNew(Thread_MsgQueue2, NULL, NULL);
  if (tid_Thread_MsgQueue2 == NULL) {
    return(-1);
  }
 
  return(0);
}
 
void Thread_MsgQueue1 (void *argument) {
  MSGQUEUE_OBJ_t msg;
 
  while (1) {
    ; // Insert thread code here...
    msg.Buf[0] = 0x55U;                                         // do some work...
    msg.Idx    = 0U;
    osMessageQueuePut(mid_MsgQueue, &msg, 0U, 0U);
    osThreadYield();                                            // suspend thread
  }
}
 
void Thread_MsgQueue2 (void *argument) {
  MSGQUEUE_OBJ_t msg;
  osStatus_t status;

  while (1) {
    ; // Insert thread code here...
    status = osMessageQueueGet(mid_MsgQueue, &msg, NULL, 0U);   // wait for message
    if (status == osOK) {
      ; // process data
    }
  }
}
