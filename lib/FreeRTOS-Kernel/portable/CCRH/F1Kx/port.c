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

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* This port uses xTaskGetCurrentTaskHandle to get TCB stack, it is required to
 * enable this API. */
#if ( ( INCLUDE_xTaskGetCurrentTaskHandle != 1 ) && ( configNUMBER_OF_CORES == 1 ) )
    #error INCLUDE_xTaskGetCurrentTaskHandle must be set to 1 in single core.
#endif

/***********************************************************
* Macro definitions
***********************************************************/

/* Hardware specific macros */
#define portPSW_REGISTER_ID          ( 5 )
#define portFPSR_REGISTER_ID         ( 6 )

/* PSW.EBV and PSW.CUx bits are kept as current status */
#define portINITIAL_PSW_MASK         ( 0x000f8000 )
#define portCURRENT_PSW_VALUE        ( portSTSR( portPSW_REGISTER_ID ) )
#define portCURRENT_SR_ZERO_VALUE    ( ( StackType_t ) 0x00000000 )
#define portCURRENT_FPSR_VALUE       ( portSTSR( portFPSR_REGISTER_ID ) )

/* Mask for FPU configuration bits (FN, PEM, RM, FS) */
#define portINITIAL_FPSR_MASK        ( 0x00ae0000 )
#define portPSW_ID_MASK              ( 0x00000020 )

/* Define necessary hardware IO for OSTM timer. OSTM0 is used by default as
 * it is common for almost device variants. If it conflicts with application,
 * the application shall implement another timer.*/
#define portOSTM_EIC_ADDR            ( 0xFFFFB0A8 )
#define portOSTM0CMP_ADDR            ( 0xFFD70000 )
#define portOSTM0CTL_ADDR            ( 0xFFD70020 )
#define portOSTM0TS_ADDR             ( 0xFFD70014 )

#if ( configNUMBER_OF_CORES > 1 )

/* IPIR  base address, the peripheral is used for Inter-Processor communication
 * Hardware supports 4 channels which is offset by 0x0, 0x4, 0x8, 0xC bytes from
 * base address. By default, channel 0 is selected. */
    #ifdef configIPIR_CHANNEL
        #define portIPIR_BASE_ADDR    ( ( 0xFFFEEC80 ) + ( configIPIR_CHANNEL << 2 ) )
    #else
        #define portIPIR_BASE_ADDR    ( 0xFFFEEC80 )
    #endif

/*  Address used for exclusive control for variable shared between PEs
 * (common resources), each CPU cores have independent access path to
 * this address. By default, G0MEV0 register is selected*/
    #ifdef configEXCLUSIVE_ADDRESS
        #define portMEV_BASE_ADDR    configEXCLUSIVE_ADDRESS
    #else
        #define portMEV_BASE_ADDR    ( 0xFFFEEC00 )
    #endif
#endif /* if ( configNUMBER_OF_CORES > 1 ) */

/* Macros required to set up the initial stack. */
#define portSTACK_INITIAL_VALUE_R1     ( ( StackType_t ) 0x01010101 )
#define portSTACK_INITIAL_VALUE_R2     ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x02 )
#define portSTACK_INITIAL_VALUE_R3     ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x03 )
#define portSTACK_INITIAL_VALUE_R4     ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x04 )
#define portSTACK_INITIAL_VALUE_R5     ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x05 )
#define portSTACK_INITIAL_VALUE_R6     ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x06 )
#define portSTACK_INITIAL_VALUE_R7     ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x07 )
#define portSTACK_INITIAL_VALUE_R8     ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x08 )
#define portSTACK_INITIAL_VALUE_R9     ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x09 )
#define portSTACK_INITIAL_VALUE_R10    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x10 )
#define portSTACK_INITIAL_VALUE_R11    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x11 )
#define portSTACK_INITIAL_VALUE_R12    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x12 )
#define portSTACK_INITIAL_VALUE_R13    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x13 )
#define portSTACK_INITIAL_VALUE_R14    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x14 )
#define portSTACK_INITIAL_VALUE_R15    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x15 )
#define portSTACK_INITIAL_VALUE_R16    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x16 )
#define portSTACK_INITIAL_VALUE_R17    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x17 )
#define portSTACK_INITIAL_VALUE_R18    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x18 )
#define portSTACK_INITIAL_VALUE_R19    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x19 )
#define portSTACK_INITIAL_VALUE_R20    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x20 )
#define portSTACK_INITIAL_VALUE_R21    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x21 )
#define portSTACK_INITIAL_VALUE_R22    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x22 )
#define portSTACK_INITIAL_VALUE_R23    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x23 )
#define portSTACK_INITIAL_VALUE_R24    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x24 )
#define portSTACK_INITIAL_VALUE_R25    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x25 )
#define portSTACK_INITIAL_VALUE_R26    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x26 )
#define portSTACK_INITIAL_VALUE_R27    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x27 )
#define portSTACK_INITIAL_VALUE_R28    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x28 )
#define portSTACK_INITIAL_VALUE_R29    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x29 )
#define portSTACK_INITIAL_VALUE_R30    ( ( StackType_t ) portSTACK_INITIAL_VALUE_R1 * 0x30 )

/***********************************************************
* Typedef definitions
***********************************************************/

/* OSTM Count Start Trigger Register (OSTMnTS) */
#define portOSTM_COUNTER_START              ( 0x01U ) /* Starts the counter */

/* OSTM Count Stop Trigger Register (OSTMnTT) */
#define portOSTM_COUNTER_STOP               ( 0x01U ) /* Stops the counter */

/* OSTM Control Register (OSTMnCTL) */
#define portOSTM_MODE_INTERVAL_TIMER        ( 0x00U )
#define portOSTM_MODE_FREE_RUNNING          ( 0x02U )

/* Disables or Enable the interrupts when counting starts */
#define portOSTM_START_INTERRUPT_DISABLE    ( 0x00U )
#define portOSTM_START_INTERRUPT_ENABLE     ( 0x01U )

/* Interrupt vector method select (TBxxx) */
#define portINT_DIRECT_VECTOR               ( 0x0U )
#define portINT_TABLE_VECTOR                ( 0x1U )

/* Interrupt mask (MKxxx) */
#define portINT_PROCESSING_ENABLED          ( 0x0U )
#define portINT_PROCESSING_DISABLED         ( 0x1U )

/* Specify 16 interrupt priority levels */
#define portINT_PRIORITY_HIGHEST            ( 0x0000U ) /* Level 0 (highest) */
#define portINT_PRIORITY_LEVEL1             ( 0x0001U ) /* Level 1 */
#define portINT_PRIORITY_LEVEL2             ( 0x0002U ) /* Level 2 */
#define portINT_PRIORITY_LEVEL3             ( 0x0003U ) /* Level 3 */
#define portINT_PRIORITY_LEVEL4             ( 0x0004U ) /* Level 4 */
#define portINT_PRIORITY_LEVEL5             ( 0x0005U ) /* Level 5 */
#define portINT_PRIORITY_LEVEL6             ( 0x0006U ) /* Level 6 */
#define portINT_PRIORITY_LEVEL7             ( 0x0007U ) /* Level 7 */
#define portINT_PRIORITY_LEVEL8             ( 0x0008U ) /* Level 8 */
#define portINT_PRIORITY_LEVEL9             ( 0x0009U ) /* Level 9 */
#define portINT_PRIORITY_LEVEL10            ( 0x000AU ) /* Level 10 */
#define portINT_PRIORITY_LEVEL11            ( 0x000BU ) /* Level 11 */
#define portINT_PRIORITY_LEVEL12            ( 0x000CU ) /* Level 12 */
#define portINT_PRIORITY_LEVEL13            ( 0x000DU ) /* Level 13 */
#define portINT_PRIORITY_LEVEL14            ( 0x000EU ) /* Level 14 */
#define portINT_PRIORITY_LOWEST             ( 0x000FU ) /* Level 15 (lowest) */

/* Macros indicating status of scheduler request */
#define PORT_SCHEDULER_NOREQUEST            0UL
#define PORT_SCHEDULER_TASKSWITCH           1UL       /* Do not modify */
#define PORT_SCHEDULER_STARTFIRSTTASK       2UL       /* Do not modify */

#ifndef configSETUP_TICK_INTERRUPT

/* The user has not provided their own tick interrupt configuration so use
 * the definition in this file (which uses the interval timer). */
    #define configSETUP_TICK_INTERRUPT()    prvSetupTimerInterrupt()
#endif /* configSETUP_TICK_INTERRUPT */

#if ( !defined( configMAX_INT_NESTING ) || ( configMAX_INT_NESTING == 0 ) )

/* Set the default value for depth of nested interrupt. In theory, the
 * microcontroller have mechanism to limit number of nested level of interrupt
 * by priority (maximum 16 levels). However, the large stack memory should be
 * prepared for each task to save resource in interrupt handler. Therefore, it
 * is necessary to limit depth of nesting interrupt to optimize memory usage.
 * In addition, the execution time of interrupt handler should be very short
 * (typically not exceed 20us), this constraint does not impact to system.
 */
    #define configMAX_INT_NESTING    2UL
#endif

/*
 * Used to catch tasks that attempt to return from their implementing function.
 */
static void prvTaskExitError( void );

/*
 * Sets up the periodic ISR used for the RTOS tick using the OSTM.
 * The application writer can define configSETUP_TICK_INTERRUPT() (in
 * FreeRTOSConfig.h) such that their own tick interrupt configuration is used
 * in place of prvSetupTimerInterrupt().
 */
static void prvSetupTimerInterrupt( void );

#if ( configNUMBER_OF_CORES > 1 )

/*
 * Functions implement spin-lock between cores by atomic accesses to Exclusive
 * Control Register (G0MEVm). There are separated access path between CPU cores,
 * but they should wait if access to same register
 */
    static void prvExclusiveLock( BaseType_t xFromIsr );
    static void prvExclusiveRelease( BaseType_t xFromIsr );

#endif

/*
 * Function to start the first task executing
 */
extern void vPortStartFirstTask( void );

/* Scheduler request on each cores which are starting first task and switching
 * context */
volatile BaseType_t xPortScheduleStatus[ configNUMBER_OF_CORES ] = { 0 };

/* Counts the interrupt nesting depth. A context switch is only performed if
 * the nesting depth is 0. In addition, the interrupt shares same stack
 * allocated for each tasks. With supporting nesting interrupt, the stack
 * may be overflowed.
 * It is necessary to control maximum stack depth.
 */
volatile UBaseType_t uxInterruptNesting[ configNUMBER_OF_CORES ] = { 0 };
volatile const UBaseType_t uxPortMaxInterruptDepth = configMAX_INT_NESTING;

/* Count number of nested locks by same cores. The lock is completely released
 * only if this count is decreased to 0, the lock is separated for task
 * and isr */
UBaseType_t uxLockNesting[ configNUMBER_OF_CORES ][ 2 ] = { 0 };

#if ( configNUMBER_OF_CORES > 1 )

/* Pointer to exclusive access memory */
    volatile BaseType_t * pxPortExclusiveReg = ( volatile BaseType_t * ) ( portMEV_BASE_ADDR );
#endif

/* Interrupt handler for OSTM timer which handling tick increment and resulting
 * to switch context. */
void vPortTickISR( void );

#if ( configNUMBER_OF_CORES > 1 )

/* Yield specific cores by send inter-processor interrupt */
    void vPortYieldCore( uint32_t xCoreID );

/*
 * Inter-processor interrupt handler. The interrupt is triggered by
 * portYIELD_CORE().
 */
    void vPortIPIHander( void );

/* These functions below implement recursive spinlock for exclusive access among
 * cores. The core will wait until lock will be available, whilst the core which
 * already had lock can acquire lock without waiting. This function could be
 * call from task and interrupt context, the critical section is called
 * as in ISR */
    void vPortRecursiveLockAcquire( BaseType_t xFromIsr );
    void vPortRecursiveLockRelease( BaseType_t xFromIsr );

#endif /* (configNUMBER_OF_CORES > 1) */

/*-----------------------------------------------------------*/

/*
 * These below functions implement interrupt mask from interrupt. They are not
 * called in nesting, it is protected by FreeRTOS kernel.
 */
portLONG xPortSetInterruptMask( void )
{
    portLONG ulPSWValue = portSTSR( portPSW_REGISTER_ID );

    portDISABLE_INTERRUPTS();

    /* It returns current value of Program Status Word register */
    return ulPSWValue;
}

/*-----------------------------------------------------------*/

void vPortClearInterruptMask( portLONG uxSavedInterruptStatus )
{
    portLONG ulPSWValue = portSTSR( portPSW_REGISTER_ID );

    /* Interrupt Disable status is indicates by bit#5 of PSW
    * (1: Interrupt is disabled; 0: Interrupt is enabled) */

    /* Revert to the status before interrupt mask. */
    ulPSWValue &= ( ~( portPSW_ID_MASK ) );
    ulPSWValue |= ( portPSW_ID_MASK & uxSavedInterruptStatus );
    portLDSR( portPSW_REGISTER_ID, ulPSWValue );
}

/*-----------------------------------------------------------*/

/*
 * Using CC-RH intrinsic function to get HTCFG0 (regID, selID) = (0,2)
 * Core ID is indicates by bit HTCFG0.PEID located at bit 18 to 16
 * Bit 31 to 19 are read only and always be read as 0. HTCFG0.PEID is 1 and 2
 * corresponding to core 0 (PE1) and core 1 (PE2). It is adjusted to 0 and 1.
 */
BaseType_t xPortGET_CORE_ID( void )
{
    #if ( configNUMBER_OF_CORES > 1 )
        return ( portSTSR_CCRH( 0, 2 ) >> 16 ) - 1;
    #else

        /* In single core, xPortGET_CORE_ID is used in this port only.
         * The dummy core ID could be controlled inside this port. */
        return 0;
    #endif
}

/*-----------------------------------------------------------*/

/*
 * This port supports both multi-cores and single-core, whilst TCB stack
 * variables are different which are respectively pxCurrentTCB (single-core)
 * and pxCurrentTCBs[] (multiple-cores). This function is defined to obtains
 * TCBs of current cores. Also, the C function could switch to corresponding
 * pointer by pre-compile conditions.
 */
void * pvPortGetCurrentTCB( void )
{
    void * pvCurrentTCB = ( void * ) xTaskGetCurrentTaskHandle();

    configASSERT( pvCurrentTCB != NULL );

    return pvCurrentTCB;
}

/*-----------------------------------------------------------*/

/*
 * This function checks if a context switch is required and, if so, updates
 * the scheduler status for the core on which the function is called. The
 * scheduler status is set to indicate that a task switch should occur.
 */
void vPortSetSwitch( BaseType_t xSwitchRequired )
{
    if( xSwitchRequired != pdFALSE )
    {
        xPortScheduleStatus[ xPortGET_CORE_ID() ] = PORT_SCHEDULER_TASKSWITCH;
    }
}

/*-----------------------------------------------------------*/

/*
 * Setup the stack of a new task so it is ready to be placed under the
 * scheduler control. The registers have to be placed on the stack in the
 * order that the port expects to find them.
 *
 * @param[in]  pxTopOfStack  Pointer to top of this task's stack
 * @param[in]  pxCode        Task function, stored as initial PC for the task
 * @param[in]  pvParameters  Parameters for task
 */
StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    /* Simulate the stack frame as it would be created by
     * a context switch interrupt. */
    *pxTopOfStack = ( StackType_t ) prvTaskExitError;            /* R31 (LP) */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R5;  /* R5 (TP)  */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pvParameters;                /* R6       */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R7;  /* R7       */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R8;  /* R8       */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R9;  /* R9       */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R10; /* R10      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R11; /* R11      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R12; /* R12      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R13; /* R13      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R14; /* R14      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R15; /* R15      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R16; /* R16      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R17; /* R17      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R18; /* R18      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R19; /* R19      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R20; /* R20      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R21; /* R21      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R22; /* R22      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R23; /* R23      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R24; /* R24      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R25; /* R25      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R26; /* R26      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R27; /* R27      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R28; /* R28      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R29; /* R29      */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R30; /* R30 (EP) */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R1;  /* R1        */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portSTACK_INITIAL_VALUE_R2;  /* R2        */

    pxTopOfStack--;

    /* Keep System pre-configuration (HV, CUx, EBV) as current setting in
     * PSW register */
    *pxTopOfStack = ( StackType_t ) ( portCURRENT_PSW_VALUE & portINITIAL_PSW_MASK ); /* EIPSW */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pxCode;                                           /* EIPC */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portCURRENT_SR_ZERO_VALUE;                        /* EIIC */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) ( portCURRENT_PSW_VALUE & portINITIAL_PSW_MASK ); /* CTPSW */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portCURRENT_SR_ZERO_VALUE;                        /* CTPC */

/* __FPU is defined by CCRH compiler if FPU is enabled */
    #if ( configENABLE_FPU == 1 )
        pxTopOfStack--;
        *pxTopOfStack = ( StackType_t ) ( portCURRENT_FPSR_VALUE & portINITIAL_FPSR_MASK ); /* FPSR */
        pxTopOfStack--;
        *pxTopOfStack = ( StackType_t ) portCURRENT_SR_ZERO_VALUE;                          /* FPEPC */
    #endif /* (configENABLE_FPU == 1) */

    return pxTopOfStack;
}

/*-----------------------------------------------------------*/

/*
 * Configures the tick frequency and starts the first task.
 */
BaseType_t xPortStartScheduler( void )
{
    #if ( configNUMBER_OF_CORES > 1 )
        BaseType_t xCurrentCore = xPortGET_CORE_ID();
    #endif

    /* Prevent interrupt by timer interrupt during starting first task.
     * The interrupt shall be enabled automatically by being restored from
     * task stack */
    portDISABLE_INTERRUPTS();

    /* Setup the tick interrupt */
    configSETUP_TICK_INTERRUPT();

    #if ( configNUMBER_OF_CORES > 1 )
        /* Start scheduler on other cores */
        for( uint16_t xCoreID = 0; xCoreID < configNUMBER_OF_CORES; xCoreID++ )
        {
            if( xCoreID != xCurrentCore )
            {
                /* Send yielding request to other cores with flag to start
                 * first task. TaskContextSwitch is not executed */
                xPortScheduleStatus[ xCoreID ] = PORT_SCHEDULER_STARTFIRSTTASK;
                vPortYieldCore( xCoreID );
            }
            else
            {
                /* Nothing to do. The first task is started in this call by
                 * below vPortStartFirstTask() */
                xPortScheduleStatus[ xCoreID ] = PORT_SCHEDULER_NOREQUEST;
            }
        }
    #endif /* if ( configNUMBER_OF_CORES > 1 ) */

    /* Start first task in primary core */
    vPortStartFirstTask();

    /* Should never get here as the tasks will now be executing! */
    prvTaskExitError();

    /* To prevent compiler warnings in the case that the application writer
     * overrides this functionality by defining configTASK_RETURN_ADDRESS.
     * Call vTaskSwitchContext() so link time optimization does not remove
     * the symbol. */
    vTaskSwitchContext(
        #if ( configNUMBER_OF_CORES > 1 )
            xCurrentCore
        #endif
        );

    return pdFALSE;
}

/*-----------------------------------------------------------*/

/*
 * Used to catch tasks that attempt to return from their implementing function.
 */
static void prvTaskExitError( void )
{
    /* A function that implements a task must not exit or attempt to return to
     * its caller as there is nothing to return to.  If a task wants to exit it
     * should instead call vTaskDelete( NULL ).
     *
     * Artificially force an assert() to be triggered if configASSERT() is
     * defined, then stop here so application writers can catch the error. */

    /* This statement will always fail, triggering the assert */
    configASSERT( pdFALSE );

    /*
     * The following statement may be unreachable because configASSERT(pdFALSE)
     * always triggers an assertion failure, which typically halts program
     * execution.
     * The warning may be reported to indicate to indicate that the compiler
     * detects the subsequent code will not be executed.
     * The warning is acceptable to ensure program is halt regardless of
     * configASSERT(pdFALSE) implementation
     */
    portDISABLE_INTERRUPTS();

    for( ; ; )
    {
        /* Infinite loop to ensure the function does not return. */
    }
}

/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
    /* Not implemented in ports where there is nothing to return to.
     * Artificially force an assert. */
    configASSERT( pdFALSE );
}

/*-----------------------------------------------------------*/

#if ( configNUMBER_OF_CORES > 1 )

    void vPortYieldCore( uint32_t xCoreID )
    {
        /* Check if we need to yield on a different core */
        if( xCoreID != xPortGET_CORE_ID() )
        {
            volatile uint32_t * pulIPIRReg;

            /* Determine the IPI register based on the target core ID */
            pulIPIRReg = ( volatile uint32_t * ) ( portIPIR_BASE_ADDR );

            /*Inter-processor interrupt generates an interrupt request by
             * writing 1 to applicable bits of target cores. The interrupt
             * should be enabled by application in  corresponding cores
             * including PSW.ID (EI instruction) and interrupt control setting
             * for ICIPIRn channel (interrupt mask, vector method)
             */
            *pulIPIRReg = ( 1 << xCoreID );
        }
        else
        {
            /* Yielding current core */
            vPortYield();
        }
    }

/*-----------------------------------------------------------*/

/*
 * Handler for inter-processor interrupt in second cores. The interrupt is
 * triggered by portYIELD_CORE(). vTaskSwitchContext() is invoked to
 * switch tasks
 */
    void vPortIPIHander( void )
    {
        BaseType_t xCurrentCore = xPortGET_CORE_ID();

        /* 1st execution starts 1st task, TaskSwitchContext is not executed */
        if( PORT_SCHEDULER_STARTFIRSTTASK != xPortScheduleStatus[ xCurrentCore ] )
        {
            xPortScheduleStatus[ xCurrentCore ] = PORT_SCHEDULER_TASKSWITCH;
        }
    }

/*-----------------------------------------------------------*/

#endif /* (configNUMBER_OF_CORES > 1) */

void vPortTickISR( void )
{
    /* In case of multicores with SMP,  xTaskIncrementTick is required to
     * called in critical section to avoid conflict resource as this function
     * could be called by xTaskResumeAll() from any cores. */
    #if ( configNUMBER_OF_CORES > 1 )
        BaseType_t xSavedInterruptStatus;

        xSavedInterruptStatus = portENTER_CRITICAL_FROM_ISR();
    #endif
    {
        /* Increment the RTOS tick. */
        if( xTaskIncrementTick() != pdFALSE )
        {
            /* Pend a context switch. */
            xPortScheduleStatus[ xPortGET_CORE_ID() ] = PORT_SCHEDULER_TASKSWITCH;
        }
    }
    #if ( configNUMBER_OF_CORES > 1 )
        portEXIT_CRITICAL_FROM_ISR( xSavedInterruptStatus );
    #endif
}

/*-----------------------------------------------------------*/

static void prvSetupTimerInterrupt( void )
{
    volatile uint32_t * pulOSTMIntReg;

    /* Interrupt configuration for OSTM Timer
     * By default, the second lowest priority is set for timer interrupt to
     * avoid blocking other interrupt. Normally, user could set the lowest
     * priority for non-critical event. It try to keep timer on time.
     * In addition, direct vector table is used by default.
     */
    pulOSTMIntReg = ( volatile uint32_t * ) portOSTM_EIC_ADDR;
    *pulOSTMIntReg = ( portINT_PROCESSING_ENABLED | portINT_DIRECT_VECTOR | portINT_PRIORITY_LEVEL14 );

    /* Set OSTM0 control setting */
    *( ( volatile uint32_t * ) portOSTM0CTL_ADDR ) =
        ( portOSTM_MODE_INTERVAL_TIMER | portOSTM_START_INTERRUPT_DISABLE );
    *( ( volatile uint32_t * ) portOSTM0CMP_ADDR ) =
        ( ( configCPU_CLOCK_HZ / configTIMER_PRESCALE ) / configTICK_RATE_HZ ) - 1;

    /* Enable OSTM0 operation */
    *( ( volatile uint32_t * ) portOSTM0TS_ADDR ) = portOSTM_COUNTER_START;
}

/*-----------------------------------------------------------*/

#if ( configNUMBER_OF_CORES > 1 )

/*
 * These functions implement spin-lock mechanism among cores using hardware
 * exclusive control with atomic access by CLR1 and SET1 instruction.
 * Nesting calls to these APIs are possible.
 */
    #pragma inline_asm prvExclusiveLock
    static void prvExclusiveLock( BaseType_t xBitPosition )
    {
        /* No problem with r19, CCRH does not required to restore same value
         * before and after function call. */
        mov     # _pxPortExclusiveReg, r19
        ld.w    0[ r19 ], r19

prvExclusiveLock_Lock:

        /* r6 is xBitPosition */
        set1 r6, [ r19 ]
        bz prvExclusiveLock_Lock_success
        snooze
        br prvExclusiveLock_Lock

prvExclusiveLock_Lock_success:
    }

/*-----------------------------------------------------------*/

    #pragma inline_asm prvExclusiveRelease
    static void prvExclusiveRelease( BaseType_t xBitPosition )
    {
        mov     # _pxPortExclusiveReg, r19
        ld.w    0[ r19 ], r19

        /* r6 is xBitPosition */
        clr1 r6, [ r19 ]
    }

/*-----------------------------------------------------------*/
    void vPortRecursiveLockAcquire( BaseType_t xFromIsr )
    {
        BaseType_t xSavedInterruptStatus;
        BaseType_t xCoreID = xPortGET_CORE_ID();
        BaseType_t xBitPosition = ( xFromIsr == pdTRUE );

        xSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

        if( uxLockNesting[ xCoreID ][ xBitPosition ] == 0 )
        {
            prvExclusiveLock( xBitPosition );
        }

        uxLockNesting[ xCoreID ][ xBitPosition ]++;
        portCLEAR_INTERRUPT_MASK_FROM_ISR( xSavedInterruptStatus );
    }

    void vPortRecursiveLockRelease( BaseType_t xFromIsr )
    {
        BaseType_t xSavedInterruptStatus;
        BaseType_t xCoreID = xPortGET_CORE_ID();
        BaseType_t xBitPosition = ( xFromIsr == pdTRUE );

        xSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

        /* Sync memory */
        portSYNCM();

        /* Error check whether vPortRecursiveLockRelease() is not called in
         * pair with vPortRecursiveLockAcquire() */
        configASSERT( ( uxLockNesting[ xCoreID ][ xBitPosition ] > 0 ) );
        uxLockNesting[ xCoreID ][ xBitPosition ]--;

        if( uxLockNesting[ xCoreID ][ xBitPosition ] == 0 )
        {
            prvExclusiveRelease( xBitPosition );
        }

        portCLEAR_INTERRUPT_MASK_FROM_ISR( xSavedInterruptStatus );
    }

/*-----------------------------------------------------------*/

#endif /* (configNUMBER_OF_CORES > 1) */
