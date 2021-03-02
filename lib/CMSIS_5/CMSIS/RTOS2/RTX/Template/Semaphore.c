#include "cmsis_os2.h"                          // CMSIS RTOS header file
 
/*----------------------------------------------------------------------------
 *      Semaphore creation & usage
 *---------------------------------------------------------------------------*/
 
osSemaphoreId_t sid_Semaphore;                  // semaphore id
 
osThreadId_t tid_Thread_Semaphore;              // thread id
 
void Thread_Semaphore (void *argument);         // thread function
 
int Init_Semaphore (void) {
 
  sid_Semaphore = osSemaphoreNew(2U, 2U, NULL);
  if (sid_Semaphore == NULL) {
    ; // Semaphore object not created, handle failure
  }
 
  tid_Thread_Semaphore = osThreadNew(Thread_Semaphore, NULL, NULL);
  if (tid_Thread_Semaphore == NULL) {
    return(-1);
  }
 
  return(0);
}
 
void Thread_Semaphore (void *argument) {
  int32_t val;
 
  while (1) {
    ; // Insert thread code here...
 
    val = osSemaphoreAcquire(sid_Semaphore, 10U);       // wait 10 mSec
    switch (val) {
      case osOK:
        ; // Use protected code here...
        osSemaphoreRelease(sid_Semaphore);              // return a token back to a semaphore
        break;
      case osErrorResource:
        break;
      case osErrorParameter:
        break;
      default:
        break;
    }
 
    osThreadYield();                                    // suspend thread
  }
}
