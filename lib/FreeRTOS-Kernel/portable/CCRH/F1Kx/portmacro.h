/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef PORTMACRO_H
    #define PORTMACRO_H

    #ifdef __cplusplus
        extern "C"
        {
    #endif

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions - These are a bit legacy and not really used now, other
 * than portSTACK_TYPE and portBASE_TYPE. */
    #define portCHAR          char
    #define portFLOAT         float
    #define portDOUBLE        double
    #define portLONG          long
    #define portSHORT         short
    #define portSTACK_TYPE    uint32_t
    #define portBASE_TYPE     long

    typedef portSTACK_TYPE   StackType_t;
    typedef long             BaseType_t;
    typedef unsigned long    UBaseType_t;

/* Defines the maximum time when using a wait command in a task */
    #if ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_16_BITS )
        typedef uint16_t     TickType_t;
        #define portMAX_DELAY              ( TickType_t ) 0xffff
    #elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS )
        typedef uint32_t     TickType_t;
        #define portMAX_DELAY              ( TickType_t ) 0xffffffffUL

/* 32-bit tick type on a 32-bit architecture, so reads of the tick count do
 * not need to be guarded with a critical section. */
        #define portTICK_TYPE_IS_ATOMIC    1
    #else
        #error configTICK_TYPE_WIDTH_IN_BITS set to unsupported tick type width.
    #endif

/*-----------------------------------------------------------*/

/* Architecture specifics */

    #define portSTSR( reg )              __stsr( ( reg ) )
    #define portLDSR( reg, val )         __ldsr( ( reg ), ( val ) )
    #define portSTSR_CCRH( reg, sel )    __stsr_rh( ( reg ), ( sel ) )
    #define portSYNCM()                  __syncm()

/* Determine the descending of the stack from high address to address */
    #define portSTACK_GROWTH      ( -1 )

/* Determine the time (in milliseconds) corresponding to each tick */
    #define portTICK_PERIOD_MS    ( ( TickType_t ) 1000 / configTICK_RATE_HZ )

/* It is a multiple of 4 (the two lower-order bits of the address = 0),
 * otherwise it will cause MAE (Misaligned Exception) according to the manual */
    #define portBYTE_ALIGNMENT    ( 4 )

/* Interrupt control macros. */

    #define portENABLE_INTERRUPTS()     __EI() /* Macro to enable all maskable interrupts. */
    #define portDISABLE_INTERRUPTS()    __DI() /* Macro to disable all maskable interrupts. */
    #define taskENABLE_INTERRUPTS()     portENABLE_INTERRUPTS()
    #define taskDISABLE_INTERRUPTS()    portDISABLE_INTERRUPTS()

/* SMP build which means configNUM_CORES is relevant */
    #define portSUPPORT_SMP              1

    #define portMAX_CORE_COUNT           2
    #ifndef configNUMBER_OF_CORES
        #define configNUMBER_OF_CORES    1
    #endif

/*-----------------------------------------------------------*/
/* Scheduler utilities */

/* Called at the end of an ISR that can cause a context switch */
    extern void vPortSetSwitch( BaseType_t xSwitchRequired );

    #define portEND_SWITCHING_ISR( x )    vPortSetSwitch( x )

    #define portYIELD_FROM_ISR( x )       portEND_SWITCHING_ISR( x )

/* Use to transfer control from one task to perform other tasks of
 * higher priority */
    extern void vPortYield( void );

    #define portYIELD()    vPortYield()
    #if ( configNUMBER_OF_CORES > 1 )

/* Return the core ID on which the code is running. */
        extern BaseType_t xPortGET_CORE_ID();

        #define portGET_CORE_ID()    xPortGET_CORE_ID()
        #define coreid    xPortGET_CORE_ID()

/* Request the core ID x to yield. */
        extern void vPortYieldCore( uint32_t coreID );

        #define portYIELD_CORE( x )                vPortYieldCore( x )

        #define portENTER_CRITICAL_FROM_ISR()      vTaskEnterCriticalFromISR()
        #define portEXIT_CRITICAL_FROM_ISR( x )    vTaskExitCriticalFromISR( x )

    #endif /* if ( configNUMBER_OF_CORES > 1 ) */

    #if ( configNUMBER_OF_CORES == 1 )
        #define portGET_ISR_LOCK()
        #define portRELEASE_ISR_LOCK()
        #define portGET_TASK_LOCK()
        #define portRELEASE_TASK_LOCK()
    #else
        extern void vPortRecursiveLockAcquire( BaseType_t xFromIsr );
        extern void vPortRecursiveLockRelease( BaseType_t xFromIsr );

        #define portGET_ISR_LOCK()         vPortRecursiveLockAcquire( pdTRUE )
        #define portRELEASE_ISR_LOCK()     vPortRecursiveLockRelease( pdTRUE )
        #define portGET_TASK_LOCK()        vPortRecursiveLockAcquire( pdFALSE )
        #define portRELEASE_TASK_LOCK()    vPortRecursiveLockRelease( pdFALSE )
    #endif /* if ( configNUMBER_OF_CORES == 1 ) */

/*-----------------------------------------------------------*/
/* Critical section management. */

/* The critical nesting functions defined within tasks.c */

    extern void vTaskEnterCritical( void );
    extern void vTaskExitCritical( void );

/* Macro to mark the start of a critical code region */
    #define portENTER_CRITICAL()    vTaskEnterCritical()

/* Macro to mark the end of a critical code region */
    #define portEXIT_CRITICAL()     vTaskExitCritical()

/*-----------------------------------------------------------*/
/* Macros to set and clear the interrupt mask. */
    portLONG xPortSetInterruptMask();
    void vPortClearInterruptMask( portLONG );

    #define portSET_INTERRUPT_MASK()                  xPortSetInterruptMask()
    #define portCLEAR_INTERRUPT_MASK( x )             vPortClearInterruptMask( ( x ) )
    #define portSET_INTERRUPT_MASK_FROM_ISR()         xPortSetInterruptMask()
    #define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )    vPortClearInterruptMask( ( x ) )

/*-----------------------------------------------------------*/
/* Task function macros as described on the FreeRTOS.org WEB site. */

    #define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )
    #define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )

/*-----------------------------------------------------------*/

    #ifdef __cplusplus
}
    #endif
#endif /* PORTMACRO_H */
