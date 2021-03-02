#include "cmsis_os2.h"                          // CMSIS RTOS header file
 
/*----------------------------------------------------------------------------
 *  Event Flags creation & usage
 *---------------------------------------------------------------------------*/
 
#define FLAGS_MSK1 0x00000001U
 
osEventFlagsId_t evt_id;                        // event flasg id
 
osThreadId_t tid_Thread_EventSender;            // thread id 1
osThreadId_t tid_Thread_EventReceiver;          // thread id 2
 
void Thread_EventSender   (void *argument);     // thread function 1
void Thread_EventReceiver (void *argument);     // thread function 2
 
int Init_Events (void) {
 
  evt_id = osEventFlagsNew(NULL);
  if (evt_id == NULL) {
    ; // Event Flags object not created, handle failure
  }
 
  tid_Thread_EventSender = osThreadNew(Thread_EventSender, NULL, NULL);
  if (tid_Thread_EventSender == NULL) {
    return(-1);
  }
  tid_Thread_EventReceiver = osThreadNew(Thread_EventReceiver, NULL, NULL);
  if (tid_Thread_EventReceiver == NULL) {
    return(-1);
  }

  return(0);
}
 
void Thread_EventSender (void *argument) {
 
  while (1) {    
    osEventFlagsSet(evt_id, FLAGS_MSK1);
    osThreadYield();                            // suspend thread
  }
}
 
void Thread_EventReceiver (void *argument) {
  uint32_t flags;
 
  while (1) {
    flags = osEventFlagsWait(evt_id, FLAGS_MSK1, osFlagsWaitAny, osWaitForever);
    //handle event
  }
}
