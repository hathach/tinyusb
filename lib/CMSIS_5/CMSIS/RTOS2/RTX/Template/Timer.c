#include "cmsis_os2.h"                          // CMSIS RTOS header file
 
/*----------------------------------------------------------------------------
 *      Timer: Sample timer functions
 *---------------------------------------------------------------------------*/
 
/*----- One-Shoot Timer Example -----*/
osTimerId_t tim_id1;                            // timer id
static uint32_t exec1;                          // argument for the timer call back function

// One-Shoot Timer Function
static void Timer1_Callback (void const *arg) {
  // add user code here
}
 
/*----- Periodic Timer Example -----*/
osTimerId_t tim_id2;                            // timer id
static uint32_t exec2;                          // argument for the timer call back function
 
// Periodic Timer Function
static void Timer2_Callback (void const *arg) {
  // add user code here
}
 
// Example: Create and Start timers
int Init_Timers (void) {
  osStatus_t status;                            // function return status
 
  // Create one-shoot timer
  exec1 = 1U;
  tim_id1 = osTimerNew((osTimerFunc_t)&Timer1_Callback, osTimerOnce, &exec1, NULL);
  if (tim_id1 != NULL) {  // One-shot timer created
    // start timer with delay 100ms
    status = osTimerStart(tim_id1, 100U); 
    if (status != osOK) {
      return -1;
    }
  }
 
  // Create periodic timer
  exec2 = 2U;
  tim_id2 = osTimerNew((osTimerFunc_t)&Timer2_Callback, osTimerPeriodic, &exec2, NULL);
  if (tim_id2 != NULL) {  // Periodic timer created
    // start timer with periodic 1000ms interval
    status = osTimerStart(tim_id2, 1000U);            
    if (status != osOK) {
      return -1;
    }
  }
  return NULL;
}
