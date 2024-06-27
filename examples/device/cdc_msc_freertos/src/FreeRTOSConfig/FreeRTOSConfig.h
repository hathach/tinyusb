/**
 * \file
 *
 * \brief FreeRTOS configurations
 *
 * Copyright (c) 2014-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* For documentation for all the configuration symbols, go to:
 * http://www.freertos.org/a00110.html.
 */

#if defined( __GNUC__ ) || defined( __ICCARM__ )
//#include <gclk.h>
#include <stdint.h>
void assert_triggered( const char *file, uint32_t line );
#endif
#include <peripheral_clk_config.h>

//--------------------------------------------------------------------+
// RTOS System Settings
//--------------------------------------------------------------------+

#define configCPU_CLOCK_HZ ( CONF_CPU_FREQUENCY )
#define configTICK_RATE_HZ ( (portTickType)1000 )
#define configMINIMAL_STACK_SIZE ( (unsigned short) 128 )
#define configTOTAL_HEAP_SIZE ( ( size_t )( 120000 ) )
#define configMAX_TASK_NAME_LEN ( 16 )
#define configQUEUE_REGISTRY_SIZE 10

//--------------------------------------------------------------------+
// RTOS Feature Settings
//--------------------------------------------------------------------+
#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configUSE_IDLE_HOOK 1
#define configUSE_DAEMON_TASK_STARTUP_HOOK 1
#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_DAEMON_TASK_STARTUP_HOOK 1
#define configRECORD_STACK_HIGH_ADDRESS 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configIDLE_SHOULD_YIELD 1
//https://www.freertos.org/Pend-on-multiple-rtos-objects.html
#define configUSE_QUEUE_SETS 1
 
#define configUSE_TICK_HOOK 0
#define configUSE_16_BIT_TICKS 0


//--------------------------------------------------------------------+
// Priority Settings
// See https://www.freertos.org/RTOS-Cortex-M3-M4.html before making
// any changes!
//--------------------------------------------------------------------+

// // SAMD51 has only 8 priority levels
#define configPRIO_BITS 3

// 9 Priorities on SAMD51
#define configMAX_PRIORITIES ( 9 )
//#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

/* The lowest interrupt priority that can be used in a call to a "set priority"
function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 7

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 4

/* Interrupt priorities used by the kernel port layer itself.  These are generic
to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

//--------------------------------------------------------------------+
// Other Settings
//--------------------------------------------------------------------+

// https://www.freertos.org/Stacks-and-stack-overflow-checking.html
#define configCHECK_FOR_STACK_OVERFLOW 2

// Place 0 task return address on stack to help FreeRTOS-aware debugger (GDB unwind thread stack)
#define configTASK_RETURN_ADDRESS 1

#define configUSE_MALLOC_FAILED_HOOK 0

#define configENABLE_BACKWARD_COMPATIBILITY 1

// Generate runtime stats
// https://www.freertos.org/rtos-run-time-stats.html
#define configGENERATE_RUN_TIME_STATS 1

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES 0

/* Software timer definitions. */
#define configUSE_TIMERS 1

// <o> Timer task priority <1-10>
// <i> Default is 2
// <id> freertos_timer_task_priority
#ifndef configTIMER_TASK_PRIORITY
#define configTIMER_TASK_PRIORITY (2)
#endif

#define configTIMER_QUEUE_LENGTH 2

// <o> Timer task stack size <32-512:4>
// <i> Default is 64
// <id> freertos_timer_task_stack_depth
#ifndef TIMER_TASK_STACK_DEPTH
#define configTIMER_TASK_STACK_DEPTH (64)
#endif

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskCleanUpResources 0
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_xResumeFromISR 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_xTaskGetIdleTaskHandle 0
#define INCLUDE_xTimerGetTimerDaemonTaskHandle 0
#define INCLUDE_pcTaskGetTaskName 0
#define INCLUDE_eTaskGetState 0

/* Normal assert() semantics without relying on the provision of an assert.h
header file. */
#define configASSERT( x )                                                                          \
    if ( ( x ) == 0 ) {                                                                            \
        taskDISABLE_INTERRUPTS();                                                                  \
        for ( ;; )                                                                                 \
            ;                                                                                      \
    }

/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS
standard names - or at least those used in the unmodified vector table. */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler


/* Used when configGENERATE_RUN_TIME_STATS is 1. */
#if configGENERATE_RUN_TIME_STATS
extern void     vConfigureTimerForRunTimeStats(void);
extern uint32_t vGetRunTimeCounterValue(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
#define portGET_RUN_TIME_COUNTER_VALUE()  xTaskGetTickCount()
#endif

// Added by Voytek for CLI support
#define configCOMMAND_INT_MAX_OUTPUT_SIZE 512

// Added by Voytek to include event groups
// https://www.freertos.org/FreeRTOS-Event-Groups.html
#define INCLUDE_xTimerPendFunctionCall 1

// Added by Voytek to give debugger insight on stack usage.
#define configRECORD_STACK_HIGH_ADDRESS 1

// Added by Dan to allow support for xSemaphoreGetMutexHolder()
#define INCLUDE_xSemaphoreGetMutexHolder 1

#endif /* FREERTOS_CONFIG_H */
