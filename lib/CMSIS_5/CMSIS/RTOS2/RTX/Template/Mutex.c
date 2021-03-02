#include "cmsis_os2.h"                          // CMSIS RTOS header file
 
/*----------------------------------------------------------------------------
 *      Mutex creation & usage
 *---------------------------------------------------------------------------*/
 
osMutexId_t mid_Mutex;                          // mutex id
 
osThreadId_t tid_Thread_Mutex;                  // thread id
 
void Thread_Mutex (void *argument);             // thread function
 
int Init_Mutex (void) {
 
  mid_Mutex = osMutexNew(NULL);
  if (mid_Mutex == NULL) {
    ; // Mutex object not created, handle failure
  }
 
  tid_Thread_Mutex = osThreadNew(Thread_Mutex, NULL, NULL);
  if (tid_Thread_Mutex == NULL) {
    return(-1);
  }
 
  return(0);
}
 
void Thread_Mutex (void *argument) {
  osStatus_t status;
 
  while (1) {
    ; // Insert thread code here...
 
    status = osMutexAcquire(mid_Mutex, 0U);
    switch (status) {
      case osOK:
        ; // Use protected code here...
        osMutexRelease(mid_Mutex);
        break;
      case osErrorResource:
        break;
      case osErrorParameter:
        break;
      case osErrorISR:
        break;
      default:
        break;
    }
 
    osThreadYield();                            // suspend thread
  }
}
