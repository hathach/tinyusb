#ifndef __FREERTOS_CONFIG__H
#define __FREERTOS_CONFIG__H

//--------------------------------------------------------------------+
// See http://www.freertos.org/a00110.html.
//--------------------------------------------------------------------+
#include "chip.h"

#define configCPU_CLOCK_HZ                   SystemCoreClock

#if 0
#if CFG_TUSB_MCU == OPT_MCU_LPC43XX
  // TODO remove
  #include "lpc43xx_cgu.h"
  #define configCPU_CLOCK_HZ                   CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE)
#endif
#endif

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configTICK_RATE_HZ                      ( 1000 )
#define configMAX_PRIORITIES                    (8)
#define configMINIMAL_STACK_SIZE                (128 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 16*1024 ) )
#define configMAX_TASK_NAME_LEN                 32
#define configUSE_16_BIT_TICKS      		        0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               10 // used to name queue/semaphore with debugger
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  0
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     1

#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* Hook function related definitions. */
#define configUSE_IDLE_HOOK                    0
#define configUSE_TICK_HOOK                    0
#define configUSE_MALLOC_FAILED_HOOK           1
#define configCHECK_FOR_STACK_OVERFLOW         2

/* Run time and task stats gathering related definitions. */
#define configGENERATE_RUN_TIME_STATS          0
#define configUSE_TRACE_FACILITY               1 // legacy trace
#define configUSE_STATS_FORMATTING_FUNCTIONS   0

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES                  0
#define configMAX_CO_ROUTINE_PRIORITIES        2

/* Software timer related definitions. */
#define configUSE_TIMERS                       1
#define configTIMER_TASK_PRIORITY              ( configMAX_PRIORITIES - 3 )
#define configTIMER_QUEUE_LENGTH               10
#define configTIMER_TASK_STACK_DEPTH	         configMINIMAL_STACK_SIZE

/* Optional functions - most linkers will remove unused functions anyway. */
#define INCLUDE_vTaskPrioritySet               0
#define INCLUDE_uxTaskPriorityGet              0
#define INCLUDE_vTaskDelete                    0
#define INCLUDE_vTaskSuspend                   1 // required for queue, semaphore, mutex to be blocked indefinitely with portMAX_DELAY
#define INCLUDE_xResumeFromISR                 0
#define INCLUDE_vTaskDelayUntil                1
#define INCLUDE_vTaskDelay                     1
#define INCLUDE_xTaskGetSchedulerState         0
#define INCLUDE_xTaskGetCurrentTaskHandle      0
#define INCLUDE_uxTaskGetStackHighWaterMark    0
#define INCLUDE_xTaskGetIdleTaskHandle         0
#define INCLUDE_xTimerGetTimerDaemonTaskHandle 0
#define INCLUDE_pcTaskGetTaskName              0
#define INCLUDE_eTaskGetState                  0
#define INCLUDE_xEventGroupSetBitFromISR       0
#define INCLUDE_xTimerPendFunctionCall         0

/* Define to trap errors during development. */

// Halt CPU (breakpoint) when hitting error, only apply for Cortex M3, M4, M7
#if defined(__ARM_ARCH_7M__) || defined (__ARM_ARCH_7EM__)

static inline void configASSERT_breakpoint(void)
{
  // Cortex M CoreDebug->DHCSR
  volatile uint32_t* ARM_CM_DHCSR =  ((volatile uint32_t*) 0xE000EDF0UL);

  // Only halt mcu if debugger is attached
  if ( (*ARM_CM_DHCSR) & 1UL ) __asm("BKPT #0\n");
}

#else
#define configASSERT_breakpoint()
#endif


#define configASSERT( x )                      if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); configASSERT_breakpoint(); }

/* FreeRTOS hooks to NVIC vectors */
#define xPortPendSVHandler    PendSV_Handler
#define xPortSysTickHandler   SysTick_Handler
#define vPortSVCHandler       SVC_Handler

//--------------------------------------------------------------------+
// Interrupt nesting behaviour configuration.
//--------------------------------------------------------------------+
/* Cortex-M specific definitions. __NVIC_PRIO_BITS is defined in mcu_variant.h */
#ifdef __NVIC_PRIO_BITS
	#define configPRIO_BITS       __NVIC_PRIO_BITS
#else
  #error "This port requires __NVIC_PRIO_BITS to be defined"
#endif


/* The lowest interrupt priority that can be used in a call to a "set priority"
function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY			  0x1f

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY	2

/* Interrupt priorities used by the kernel port layer itself.  These are generic
to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY 		          ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	        ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#endif /* __FREERTOS_CONFIG__H */
