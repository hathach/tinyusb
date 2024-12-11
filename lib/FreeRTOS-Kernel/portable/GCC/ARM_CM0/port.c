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

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
 * all the API functions to use the MPU wrappers. That should only be done when
 * task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* MPU includes. */
#include "mpu_wrappers.h"
#include "mpu_syscall_numbers.h"

/* Portasm includes. */
#include "portasm.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/*-----------------------------------------------------------*/

/**
 * @brief Prototype of all Interrupt Service Routines (ISRs).
 */
typedef void ( * portISR_t )( void );

/*-----------------------------------------------------------*/

/**
 * @brief Constants required to manipulate the NVIC.
 */
#define portNVIC_SYSTICK_CTRL_REG             ( *( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG             ( *( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG    ( *( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SHPR3_REG                    ( *( ( volatile uint32_t * ) 0xe000ed20 ) )
#define portNVIC_SHPR2_REG                    ( *( ( volatile uint32_t * ) 0xe000ed1c ) )
#define portNVIC_SYSTICK_ENABLE_BIT           ( 1UL << 0UL )
#define portNVIC_SYSTICK_INT_BIT              ( 1UL << 1UL )
#define portNVIC_SYSTICK_CLK_BIT              ( 1UL << 2UL )
#define portNVIC_SYSTICK_COUNT_FLAG_BIT       ( 1UL << 16UL )
#define portNVIC_PEND_SYSTICK_CLEAR_BIT       ( 1UL << 25UL )
#define portNVIC_PEND_SYSTICK_SET_BIT         ( 1UL << 26UL )
#define portMIN_INTERRUPT_PRIORITY            ( 255UL )
#define portNVIC_PENDSV_PRI                   ( portMIN_INTERRUPT_PRIORITY << 16UL )
#define portNVIC_SYSTICK_PRI                  ( portMIN_INTERRUPT_PRIORITY << 24UL )

/*-----------------------------------------------------------*/

/**
 * @brief Constants required to manipulate the SCB.
 */
#define portSCB_VTOR_REG                      ( *( ( portISR_t ** ) 0xe000ed08 ) )
#define portSCB_SYS_HANDLER_CTRL_STATE_REG    ( *( ( volatile uint32_t * ) 0xe000ed24 ) )
#define portSCB_MEM_FAULT_ENABLE_BIT          ( 1UL << 16UL )

/*-----------------------------------------------------------*/

/**
 * @brief Constants used to check the installation of the FreeRTOS interrupt handlers.
 */
#define portVECTOR_INDEX_SVC       ( 11 )
#define portVECTOR_INDEX_PENDSV    ( 14 )

/*-----------------------------------------------------------*/

/**
 * @brief Constants used during system call enter and exit.
 */
#define portPSR_STACK_PADDING_MASK              ( 1UL << 9UL )
#define portEXC_RETURN_STACK_FRAME_TYPE_MASK    ( 1UL << 4UL )

/*-----------------------------------------------------------*/

/**
 * @brief Offsets in the stack to the parameters when inside the SVC handler.
 */
#define portOFFSET_TO_LR     ( 5 )
#define portOFFSET_TO_PC     ( 6 )
#define portOFFSET_TO_PSR    ( 7 )

/*-----------------------------------------------------------*/

/**
 * @brief Constants required to manipulate the MPU.
 */
#define portMPU_TYPE_REG                            ( *( ( volatile uint32_t * ) 0xe000ed90 ) )
#define portMPU_CTRL_REG                            ( *( ( volatile uint32_t * ) 0xe000ed94 ) )

#define portMPU_RBAR_REG                            ( *( ( volatile uint32_t * ) 0xe000ed9c ) )
#define portMPU_RASR_REG                            ( *( ( volatile uint32_t * ) 0xe000eda0 ) )

/* MPU Region Attribute and Size Register (RASR) bitmasks. */
#define portMPU_RASR_AP_BITMASK                     ( 0x7UL << 24UL )
#define portMPU_RASR_S_C_B_BITMASK                  ( 0x7UL )
#define portMPU_RASR_S_C_B_LOCATION                 ( 16UL )
#define portMPU_RASR_SIZE_BITMASK                   ( 0x1FUL << 1UL )
#define portMPU_RASR_REGION_ENABLE_BITMASK          ( 0x1UL )

/* MPU Region Base Address Register (RBAR) bitmasks. */
#define portMPU_RBAR_ADDRESS_BITMASK                ( 0xFFFFFF00UL )
#define portMPU_RBAR_REGION_NUMBER_VALID_BITMASK    ( 0x1UL << 4UL )
#define portMPU_RBAR_REGION_NUMBER_BITMASK          ( 0x0000000FUL )

/* MPU Control Register (MPU_CTRL) bitmasks. */
#define portMPU_CTRL_ENABLE_BITMASK                 ( 0x1UL )
#define portMPU_CTRL_PRIV_BACKGROUND_ENABLE_BITMASK ( 0x1UL << 2UL ) /* PRIVDEFENA bit. */

/* Expected value of the portMPU_TYPE register. */
#define portEXPECTED_MPU_TYPE_VALUE                 ( 0x8UL << 8UL ) /* 8 DREGION unified. */

/* Extract first address of the MPU region as encoded in the
 * RBAR (Region Base Address Register) value. */
#define portEXTRACT_FIRST_ADDRESS_FROM_RBAR( rbar ) \
    ( ( rbar ) & portMPU_RBAR_ADDRESS_BITMASK )

/* Extract size of the MPU region as encoded in the
 * RASR (Region Attribute and Size Register) value. */
#define portEXTRACT_REGION_SIZE_FROM_RASR( rasr ) \
    ( 1 << ( ( ( ( rasr ) & portMPU_RASR_SIZE_BITMASK ) >> 1 )+ 1 ) )

/* Does addr lies within [start, end] address range? */
#define portIS_ADDRESS_WITHIN_RANGE( addr, start, end ) \
    ( ( ( addr ) >= ( start ) ) && ( ( addr ) <= ( end ) ) )

/* Is the access request satisfied by the available permissions? */
#define portIS_AUTHORIZED( accessRequest, permissions ) \
    ( ( ( permissions ) & ( accessRequest ) ) == accessRequest )

/* Max value that fits in a uint32_t type. */
#define portUINT32_MAX    ( ~( ( uint32_t ) 0 ) )

/* Check if adding a and b will result in overflow. */
#define portADD_UINT32_WILL_OVERFLOW( a, b )    ( ( a ) > ( portUINT32_MAX - ( b ) ) )

/*-----------------------------------------------------------*/

/**
 * @brief The maximum 24-bit number.
 *
 * It is needed because the systick is a 24-bit counter.
 */
#define portMAX_24_BIT_NUMBER       ( 0xffffffUL )

/**
 * @brief A fiddle factor to estimate the number of SysTick counts that would
 * have occurred while the SysTick counter is stopped during tickless idle
 * calculations.
 */
#define portMISSED_COUNTS_FACTOR    ( 94UL )

/*-----------------------------------------------------------*/

/**
 * @brief Constants required to set up the initial stack.
 */
#define portINITIAL_XPSR    ( 0x01000000 )

/**
 * @brief Initial EXC_RETURN value.
 *
 *     FF         FF         FF         FD
 * 1111 1111  1111 1111  1111 1111  1111 1101
 *
 * Bit[3] - 1 --> Return to the Thread mode.
 * Bit[2] - 1 --> Restore registers from the process stack.
 * Bit[1] - 0 --> Reserved, 0.
 * Bit[0] - 0 --> Reserved, 1.
 */
#define portINITIAL_EXC_RETURN    ( 0xfffffffdUL )

/**
 * @brief CONTROL register privileged bit mask.
 *
 * Bit[0] in CONTROL register tells the privilege:
 *  Bit[0] = 0 ==> The task is privileged.
 *  Bit[0] = 1 ==> The task is not privileged.
 */
#define portCONTROL_PRIVILEGED_MASK         ( 1UL << 0UL )

/**
 * @brief Initial CONTROL register values.
 */
#define portINITIAL_CONTROL_UNPRIVILEGED    ( 0x3 )
#define portINITIAL_CONTROL_PRIVILEGED      ( 0x2 )

/**
 * @brief Let the user override the default SysTick clock rate.  If defined by the
 * user, this symbol must equal the SysTick clock rate when the CLK bit is 0 in the
 * configuration register.
 */
#ifndef configSYSTICK_CLOCK_HZ
    #define configSYSTICK_CLOCK_HZ             ( configCPU_CLOCK_HZ )
    /* Ensure the SysTick is clocked at the same frequency as the core. */
    #define portNVIC_SYSTICK_CLK_BIT_CONFIG    ( portNVIC_SYSTICK_CLK_BIT )
#else
    /* Select the option to clock SysTick not at the same frequency as the core. */
    #define portNVIC_SYSTICK_CLK_BIT_CONFIG    ( 0 )
#endif

/**
 * @brief Let the user override the pre-loading of the initial LR with the
 * address of prvTaskExitError() in case it messes up unwinding of the stack
 * in the debugger.
 */
#ifdef configTASK_RETURN_ADDRESS
    #define portTASK_RETURN_ADDRESS    configTASK_RETURN_ADDRESS
#else
    #define portTASK_RETURN_ADDRESS    prvTaskExitError
#endif

/**
 * @brief If portPRELOAD_REGISTERS then registers will be given an initial value
 * when a task is created. This helps in debugging at the cost of code size.
 */
#define portPRELOAD_REGISTERS    1

/*-----------------------------------------------------------*/

/**
 * @brief Used to catch tasks that attempt to return from their implementing
 * function.
 */
static void prvTaskExitError( void );

#if ( configENABLE_MPU == 1 )

    /**
     * @brief Setup the Memory Protection Unit (MPU).
     */
    static void prvSetupMPU( void ) PRIVILEGED_FUNCTION;

#endif /* configENABLE_MPU */

/**
 * @brief Setup the timer to generate the tick interrupts.
 *
 * The implementation in this file is weak to allow application writers to
 * change the timer used to generate the tick interrupt.
 */
void vPortSetupTimerInterrupt( void ) PRIVILEGED_FUNCTION;

/**
 * @brief Checks whether the current execution context is interrupt.
 *
 * @return pdTRUE if the current execution context is interrupt, pdFALSE
 * otherwise.
 */
BaseType_t xPortIsInsideInterrupt( void );

/**
 * @brief Yield the processor.
 */
void vPortYield( void ) PRIVILEGED_FUNCTION;

/**
 * @brief Enter critical section.
 */
void vPortEnterCritical( void ) PRIVILEGED_FUNCTION;

/**
 * @brief Exit from critical section.
 */
void vPortExitCritical( void ) PRIVILEGED_FUNCTION;

/**
 * @brief SysTick handler.
 */
void SysTick_Handler( void ) PRIVILEGED_FUNCTION;

/**
 * @brief C part of SVC handler.
 */
portDONT_DISCARD void vPortSVCHandler_C( uint32_t * pulCallerStackAddress ) PRIVILEGED_FUNCTION;

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    /**
     * @brief Sets up the system call stack so that upon returning from
     * SVC, the system call stack is used.
     *
     * @param pulTaskStack The current SP when the SVC was raised.
     * @param ulLR The value of Link Register (EXC_RETURN) in the SVC handler.
     * @param ucSystemCallNumber The system call number of the system call.
     */
    void vSystemCallEnter( uint32_t * pulTaskStack,
                           uint32_t ulLR,
                           uint8_t ucSystemCallNumber ) PRIVILEGED_FUNCTION;

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    /**
     * @brief Raise SVC for exiting from a system call.
     */
    void vRequestSystemCallExit( void ) __attribute__( ( naked ) ) PRIVILEGED_FUNCTION;

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    /**
     * @brief Sets up the task stack so that upon returning from
     * SVC, the task stack is used again.
     *
     * @param pulSystemCallStack The current SP when the SVC was raised.
     * @param ulLR The value of Link Register (EXC_RETURN) in the SVC handler.
     */
    void vSystemCallExit( uint32_t * pulSystemCallStack,
                          uint32_t ulLR ) PRIVILEGED_FUNCTION;

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

#if ( configENABLE_MPU == 1 )

    /**
     * @brief Checks whether or not the calling task is privileged.
     *
     * @return pdTRUE if the calling task is privileged, pdFALSE otherwise.
     */
    BaseType_t xPortIsTaskPrivileged( void ) PRIVILEGED_FUNCTION;

#endif /* configENABLE_MPU == 1 */

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    /**
     * @brief This variable is set to pdTRUE when the scheduler is started.
     */
    PRIVILEGED_DATA static BaseType_t xSchedulerRunning = pdFALSE;

#endif

/**
 * @brief Each task maintains its own interrupt status in the critical nesting
 * variable.
 */
PRIVILEGED_DATA static volatile uint32_t ulCriticalNesting = 0xaaaaaaaaUL;

#if ( configUSE_TICKLESS_IDLE == 1 )

    /**
     * @brief The number of SysTick increments that make up one tick period.
     */
    PRIVILEGED_DATA static uint32_t ulTimerCountsForOneTick = 0;

    /**
     * @brief The maximum number of tick periods that can be suppressed is
     * limited by the 24 bit resolution of the SysTick timer.
     */
    PRIVILEGED_DATA static uint32_t xMaximumPossibleSuppressedTicks = 0;

    /**
     * @brief Compensate for the CPU cycles that pass while the SysTick is
     * stopped (low power functionality only).
     */
    PRIVILEGED_DATA static uint32_t ulStoppedTimerCompensation = 0;

#endif /* configUSE_TICKLESS_IDLE */

/*-----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE == 1 )

    __attribute__( ( weak ) ) void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
    {
        uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements, ulSysTickDecrementsLeft;
        TickType_t xModifiableIdleTime;

        /* Make sure the SysTick reload value does not overflow the counter. */
        if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
        {
            xExpectedIdleTime = xMaximumPossibleSuppressedTicks;
        }

        /* Enter a critical section but don't use the taskENTER_CRITICAL()
         * method as that will mask interrupts that should exit sleep mode. */
        __asm volatile ( "cpsid i" ::: "memory" );
        __asm volatile ( "dsb" );
        __asm volatile ( "isb" );

        /* If a context switch is pending or a task is waiting for the scheduler
         * to be unsuspended then abandon the low power entry. */
        if( eTaskConfirmSleepModeStatus() == eAbortSleep )
        {
            /* Re-enable interrupts - see comments above the cpsid instruction
             * above. */
            __asm volatile ( "cpsie i" ::: "memory" );
        }
        else
        {
            /* Stop the SysTick momentarily.  The time the SysTick is stopped for
             * is accounted for as best it can be, but using the tickless mode will
             * inevitably result in some tiny drift of the time maintained by the
             * kernel with respect to calendar time. */
            portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT );

            /* Use the SysTick current-value register to determine the number of
             * SysTick decrements remaining until the next tick interrupt.  If the
             * current-value register is zero, then there are actually
             * ulTimerCountsForOneTick decrements remaining, not zero, because the
             * SysTick requests the interrupt when decrementing from 1 to 0. */
            ulSysTickDecrementsLeft = portNVIC_SYSTICK_CURRENT_VALUE_REG;

            if( ulSysTickDecrementsLeft == 0 )
            {
                ulSysTickDecrementsLeft = ulTimerCountsForOneTick;
            }

            /* Calculate the reload value required to wait xExpectedIdleTime
             * tick periods.  -1 is used because this code normally executes part
             * way through the first tick period.  But if the SysTick IRQ is now
             * pending, then clear the IRQ, suppressing the first tick, and correct
             * the reload value to reflect that the second tick period is already
             * underway.  The expected idle time is always at least two ticks. */
            ulReloadValue = ulSysTickDecrementsLeft + ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );

            if( ( portNVIC_INT_CTRL_REG & portNVIC_PEND_SYSTICK_SET_BIT ) != 0 )
            {
                portNVIC_INT_CTRL_REG = portNVIC_PEND_SYSTICK_CLEAR_BIT;
                ulReloadValue -= ulTimerCountsForOneTick;
            }

            if( ulReloadValue > ulStoppedTimerCompensation )
            {
                ulReloadValue -= ulStoppedTimerCompensation;
            }

            /* Set the new reload value. */
            portNVIC_SYSTICK_LOAD_REG = ulReloadValue;

            /* Clear the SysTick count flag and set the count value back to
             * zero. */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

            /* Restart SysTick. */
            portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

            /* Sleep until something happens.  configPRE_SLEEP_PROCESSING() can
             * set its parameter to 0 to indicate that its implementation contains
             * its own wait for interrupt or wait for event instruction, and so wfi
             * should not be executed again.  However, the original expected idle
             * time variable must remain unmodified, so a copy is taken. */
            xModifiableIdleTime = xExpectedIdleTime;
            configPRE_SLEEP_PROCESSING( xModifiableIdleTime );

            if( xModifiableIdleTime > 0 )
            {
                __asm volatile ( "dsb" ::: "memory" );
                __asm volatile ( "wfi" );
                __asm volatile ( "isb" );
            }

            configPOST_SLEEP_PROCESSING( xExpectedIdleTime );

            /* Re-enable interrupts to allow the interrupt that brought the MCU
             * out of sleep mode to execute immediately.  See comments above
             * the cpsid instruction above. */
            __asm volatile ( "cpsie i" ::: "memory" );
            __asm volatile ( "dsb" );
            __asm volatile ( "isb" );

            /* Disable interrupts again because the clock is about to be stopped
             * and interrupts that execute while the clock is stopped will increase
             * any slippage between the time maintained by the RTOS and calendar
             * time. */
            __asm volatile ( "cpsid i" ::: "memory" );
            __asm volatile ( "dsb" );
            __asm volatile ( "isb" );

            /* Disable the SysTick clock without reading the
             * portNVIC_SYSTICK_CTRL_REG register to ensure the
             * portNVIC_SYSTICK_COUNT_FLAG_BIT is not cleared if it is set.  Again,
             * the time the SysTick is stopped for is accounted for as best it can
             * be, but using the tickless mode will inevitably result in some tiny
             * drift of the time maintained by the kernel with respect to calendar
             * time*/
            portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT );

            /* Determine whether the SysTick has already counted to zero. */
            if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
            {
                uint32_t ulCalculatedLoadValue;

                /* The tick interrupt ended the sleep (or is now pending), and
                 * a new tick period has started.  Reset portNVIC_SYSTICK_LOAD_REG
                 * with whatever remains of the new tick period. */
                ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL ) - ( ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG );

                /* Don't allow a tiny value, or values that have somehow
                 * underflowed because the post sleep hook did something
                 * that took too long or because the SysTick current-value register
                 * is zero. */
                if( ( ulCalculatedLoadValue <= ulStoppedTimerCompensation ) || ( ulCalculatedLoadValue > ulTimerCountsForOneTick ) )
                {
                    ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL );
                }

                portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;

                /* As the pending tick will be processed as soon as this
                 * function exits, the tick value maintained by the tick is stepped
                 * forward by one less than the time spent waiting. */
                ulCompleteTickPeriods = xExpectedIdleTime - 1UL;
            }
            else
            {
                /* Something other than the tick interrupt ended the sleep. */

                /* Use the SysTick current-value register to determine the
                 * number of SysTick decrements remaining until the expected idle
                 * time would have ended. */
                ulSysTickDecrementsLeft = portNVIC_SYSTICK_CURRENT_VALUE_REG;
                #if ( portNVIC_SYSTICK_CLK_BIT_CONFIG != portNVIC_SYSTICK_CLK_BIT )
                {
                    /* If the SysTick is not using the core clock, the current-
                     * value register might still be zero here.  In that case, the
                     * SysTick didn't load from the reload register, and there are
                     * ulReloadValue decrements remaining in the expected idle
                     * time, not zero. */
                    if( ulSysTickDecrementsLeft == 0 )
                    {
                        ulSysTickDecrementsLeft = ulReloadValue;
                    }
                }
                #endif /* portNVIC_SYSTICK_CLK_BIT_CONFIG */

                /* Work out how long the sleep lasted rounded to complete tick
                 * periods (not the ulReload value which accounted for part
                 * ticks). */
                ulCompletedSysTickDecrements = ( xExpectedIdleTime * ulTimerCountsForOneTick ) - ulSysTickDecrementsLeft;

                /* How many complete tick periods passed while the processor
                 * was waiting? */
                ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulTimerCountsForOneTick;

                /* The reload value is set to whatever fraction of a single tick
                 * period remains. */
                portNVIC_SYSTICK_LOAD_REG = ( ( ulCompleteTickPeriods + 1UL ) * ulTimerCountsForOneTick ) - ulCompletedSysTickDecrements;
            }

            /* Restart SysTick so it runs from portNVIC_SYSTICK_LOAD_REG again,
             * then set portNVIC_SYSTICK_LOAD_REG back to its standard value.  If
             * the SysTick is not using the core clock, temporarily configure it to
             * use the core clock.  This configuration forces the SysTick to load
             * from portNVIC_SYSTICK_LOAD_REG immediately instead of at the next
             * cycle of the other clock.  Then portNVIC_SYSTICK_LOAD_REG is ready
             * to receive the standard value immediately. */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
            portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
            #if ( portNVIC_SYSTICK_CLK_BIT_CONFIG == portNVIC_SYSTICK_CLK_BIT )
            {
                portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;
            }
            #else
            {
                /* The temporary usage of the core clock has served its purpose,
                 * as described above.  Resume usage of the other clock. */
                portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT;

                if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
                {
                    /* The partial tick period already ended.  Be sure the SysTick
                     * counts it only once. */
                    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0;
                }

                portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;
                portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
            }
            #endif /* portNVIC_SYSTICK_CLK_BIT_CONFIG */

            /* Step the tick to account for any tick periods that elapsed. */
            vTaskStepTick( ulCompleteTickPeriods );

            /* Exit with interrupts enabled. */
            __asm volatile ( "cpsie i" ::: "memory" );
        }
    }

#endif /* configUSE_TICKLESS_IDLE */

/*-----------------------------------------------------------*/

__attribute__( ( weak ) ) void vPortSetupTimerInterrupt( void ) /* PRIVILEGED_FUNCTION */
{
    /* Calculate the constants required to configure the tick interrupt. */
    #if ( configUSE_TICKLESS_IDLE == 1 )
    {
        ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );
        xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;
        ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );
    }
    #endif /* configUSE_TICKLESS_IDLE */

    /* Stop and reset SysTick.
     *
     * QEMU versions older than 7.0.0 contain a bug which causes an error if we
     * enable SysTick without first selecting a valid clock source. We trigger
     * the bug if we change clock sources from a clock with a zero clock period
     * to one with a nonzero clock period and enable Systick at the same time.
     * So we configure the CLKSOURCE bit here, prior to setting the ENABLE bit.
     * This workaround avoids the bug in QEMU versions older than 7.0.0. */
    portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT_CONFIG;
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

    /* Configure SysTick to interrupt at the requested rate. */
    portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
    portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
}

/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
    volatile uint32_t ulDummy = 0UL;

    /* A function that implements a task must not exit or attempt to return to
     * its caller as there is nothing to return to. If a task wants to exit it
     * should instead call vTaskDelete( NULL ). Artificially force an assert()
     * to be triggered if configASSERT() is defined, then stop here so
     * application writers can catch the error. */
    configASSERT( ulCriticalNesting == ~0UL );
    portDISABLE_INTERRUPTS();

    while( ulDummy == 0 )
    {
        /* This file calls prvTaskExitError() after the scheduler has been
         * started to remove a compiler warning about the function being
         * defined but never called.  ulDummy is used purely to quieten other
         * warnings about code appearing after this function is called - making
         * ulDummy volatile makes the compiler think the function could return
         * and therefore not output an 'unreachable code' warning for code that
         * appears after it. */
    }
}

/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

    static uint32_t prvGetMPURegionSizeSetting( uint32_t ulActualSizeInBytes )
    {
        uint32_t ulRegionSize, ulReturnValue = 7UL;

        /* 256 is the smallest region size, 31 is the largest valid value for
         * ulReturnValue. */
        for( ulRegionSize = 256UL; ulReturnValue < 31UL; ( ulRegionSize <<= 1UL ) )
        {
            if( ulActualSizeInBytes <= ulRegionSize )
            {
                break;
            }
            else
            {
                ulReturnValue++;
            }
        }

        /* Shift the code by one before returning so it can be written directly
         * into the the correct bit position of the attribute register. */
        return( ulReturnValue << 1UL );
    }

#endif /* configENABLE_MPU */

/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

    static void prvSetupMPU( void ) /* PRIVILEGED_FUNCTION */
    {
        #if defined( __ARMCC_VERSION )

            /* Declaration when these variable are defined in code instead of being
            * exported from linker scripts. */
            extern uint32_t * __privileged_functions_start__;
            extern uint32_t * __privileged_functions_end__;
            extern uint32_t * __FLASH_segment_start__;
            extern uint32_t * __FLASH_segment_end__;
            extern uint32_t * __privileged_sram_start__;
            extern uint32_t * __privileged_sram_end__;

        #else /* if defined( __ARMCC_VERSION ) */

            /* Declaration when these variable are exported from linker scripts. */
            extern uint32_t __privileged_functions_start__[];
            extern uint32_t __privileged_functions_end__[];
            extern uint32_t __FLASH_segment_start__[];
            extern uint32_t __FLASH_segment_end__[];
            extern uint32_t __privileged_sram_start__[];
            extern uint32_t __privileged_sram_end__[];

        #endif /* defined( __ARMCC_VERSION ) */

        /* Ensure that the MPU is present. */
        configASSERT( portMPU_TYPE_REG == portEXPECTED_MPU_TYPE_VALUE );

        /* Check that the MPU is present. */
        if( portMPU_TYPE_REG == portEXPECTED_MPU_TYPE_VALUE )
        {
            /* Setup privileged flash as Read Only so that privileged tasks can
             * read it but not modify. */
            portMPU_RBAR_REG = ( ( ( uint32_t ) __privileged_functions_start__ ) | /* Base address. */
                                 ( portMPU_RBAR_REGION_NUMBER_VALID_BITMASK ) |
                                 ( portPRIVILEGED_FLASH_REGION ) );

            portMPU_RASR_REG = ( ( portMPU_REGION_PRIV_RO_UNPRIV_NA ) |
                                 ( ( configS_C_B_FLASH & portMPU_RASR_S_C_B_BITMASK ) << portMPU_RASR_S_C_B_LOCATION ) |
                                 ( prvGetMPURegionSizeSetting( ( uint32_t ) __privileged_functions_end__ - ( uint32_t ) __privileged_functions_start__ ) ) |
                                 ( portMPU_RASR_REGION_ENABLE_BITMASK ) );

            /* Setup unprivileged flash as Read Only by both privileged and
             * unprivileged tasks. All tasks can read it but no-one can modify. */
            portMPU_RBAR_REG = ( ( ( uint32_t ) __FLASH_segment_start__ ) | /* Base address. */
                                 ( portMPU_RBAR_REGION_NUMBER_VALID_BITMASK ) |
                                 ( portUNPRIVILEGED_FLASH_REGION ) );

            portMPU_RASR_REG = ( ( portMPU_REGION_PRIV_RO_UNPRIV_RO ) |
                                 ( ( configS_C_B_FLASH & portMPU_RASR_S_C_B_BITMASK ) << portMPU_RASR_S_C_B_LOCATION ) |
                                 ( prvGetMPURegionSizeSetting( ( uint32_t ) __FLASH_segment_end__ - ( uint32_t ) __FLASH_segment_start__ ) ) |
                                 ( portMPU_RASR_REGION_ENABLE_BITMASK ) );

            /* Setup RAM containing kernel data for privileged access only. */
            portMPU_RBAR_REG = ( ( uint32_t ) __privileged_sram_start__ ) | /* Base address. */
                                 ( portMPU_RBAR_REGION_NUMBER_VALID_BITMASK ) |
                                 ( portPRIVILEGED_RAM_REGION );

            portMPU_RASR_REG = ( ( portMPU_REGION_PRIV_RW_UNPRIV_NA ) |
                                 ( portMPU_REGION_EXECUTE_NEVER ) |
                                 ( ( configS_C_B_SRAM & portMPU_RASR_S_C_B_BITMASK ) << portMPU_RASR_S_C_B_LOCATION ) |
                                 prvGetMPURegionSizeSetting( ( uint32_t ) __privileged_sram_end__ - ( uint32_t ) __privileged_sram_start__ ) |
                                 ( portMPU_RASR_REGION_ENABLE_BITMASK ) );

            /* Enable MPU with privileged background access i.e. unmapped
             * regions have privileged access. */
            portMPU_CTRL_REG |= ( portMPU_CTRL_PRIV_BACKGROUND_ENABLE_BITMASK |
                                  portMPU_CTRL_ENABLE_BITMASK );
        }
    }

#endif /* configENABLE_MPU */

/*-----------------------------------------------------------*/

void vPortYield( void ) /* PRIVILEGED_FUNCTION */
{
    /* Set a PendSV to request a context switch. */
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;

    /* Barriers are normally not required but do ensure the code is
     * completely within the specified behaviour for the architecture. */
    __asm volatile ( "dsb" ::: "memory" );
    __asm volatile ( "isb" );
}

/*-----------------------------------------------------------*/

void vPortEnterCritical( void ) /* PRIVILEGED_FUNCTION */
{
    portDISABLE_INTERRUPTS();
    ulCriticalNesting++;

    /* Barriers are normally not required but do ensure the code is
     * completely within the specified behaviour for the architecture. */
    __asm volatile ( "dsb" ::: "memory" );
    __asm volatile ( "isb" );
}

/*-----------------------------------------------------------*/

void vPortExitCritical( void ) /* PRIVILEGED_FUNCTION */
{
    configASSERT( ulCriticalNesting );
    ulCriticalNesting--;

    if( ulCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}

/*-----------------------------------------------------------*/

void SysTick_Handler( void ) /* PRIVILEGED_FUNCTION */
{
    uint32_t ulPreviousMask;

    ulPreviousMask = portSET_INTERRUPT_MASK_FROM_ISR();

    traceISR_ENTER();
    {
        /* Increment the RTOS tick. */
        if( xTaskIncrementTick() != pdFALSE )
        {
            traceISR_EXIT_TO_SCHEDULER();
            /* Pend a context switch. */
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }
        else
        {
            traceISR_EXIT();
        }
    }

    portCLEAR_INTERRUPT_MASK_FROM_ISR( ulPreviousMask );
}

/*-----------------------------------------------------------*/

void vPortSVCHandler_C( uint32_t * pulCallerStackAddress ) /* PRIVILEGED_FUNCTION portDONT_DISCARD */
{
    #if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 1 ) )

        #if defined( __ARMCC_VERSION )

            /* Declaration when these variable are defined in code instead of being
             * exported from linker scripts. */
            extern uint32_t * __syscalls_flash_start__;
            extern uint32_t * __syscalls_flash_end__;

        #else

            /* Declaration when these variable are exported from linker scripts. */
            extern uint32_t __syscalls_flash_start__[];
            extern uint32_t __syscalls_flash_end__[];

        #endif /* defined( __ARMCC_VERSION ) */

    #endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 1 ) */

    uint32_t ulPC;
    uint8_t ucSVCNumber;

    /* Register are stored on the stack in the following order - R0, R1, R2, R3,
     * R12, LR, PC, xPSR. */
    ulPC = pulCallerStackAddress[ portOFFSET_TO_PC ];
    ucSVCNumber = ( ( uint8_t * ) ulPC )[ -2 ];

    switch( ucSVCNumber )
    {
        case portSVC_START_SCHEDULER:
            /* Setup the context of the first task so that the first task starts
             * executing. */
            vRestoreContextOfFirstTask();
            break;

    #if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 1 ) )

        case portSVC_RAISE_PRIVILEGE:
            /* Only raise the privilege, if the svc was raised from any of
             * the system calls. */
            if( ( ulPC >= ( uint32_t ) __syscalls_flash_start__ ) &&
                ( ulPC <= ( uint32_t ) __syscalls_flash_end__ ) )
            {
                vRaisePrivilege();
            }
            break;

    #endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 1 ) */

    #if ( configENABLE_MPU == 1 )

        case portSVC_YIELD:
            vPortYield();
            break;

    #endif /* configENABLE_MPU == 1 */

        default:
            /* Incorrect SVC call. */
            configASSERT( pdFALSE );
    }
}

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    void vSystemCallEnter( uint32_t * pulTaskStack,
                           uint32_t ulLR,
                           uint8_t ucSystemCallNumber ) /* PRIVILEGED_FUNCTION */
    {
        extern TaskHandle_t pxCurrentTCB;
        extern UBaseType_t uxSystemCallImplementations[ NUM_SYSTEM_CALLS ];
        xMPU_SETTINGS * pxMpuSettings;
        uint32_t * pulSystemCallStack;
        uint32_t ulSystemCallLocation, i;
        const uint32_t ulStackFrameSize = 8;

        #if defined( __ARMCC_VERSION )

            /* Declaration when these variable are defined in code instead of being
             * exported from linker scripts. */
            extern uint32_t * __syscalls_flash_start__;
            extern uint32_t * __syscalls_flash_end__;

        #else

            /* Declaration when these variable are exported from linker scripts. */
            extern uint32_t __syscalls_flash_start__[];
            extern uint32_t __syscalls_flash_end__[];

        #endif /* #if defined( __ARMCC_VERSION ) */

        ulSystemCallLocation = pulTaskStack[ portOFFSET_TO_PC ];
        pxMpuSettings = xTaskGetMPUSettings( pxCurrentTCB );

        /* Checks:
         * 1. SVC is raised from the system call section (i.e. application is
         *    not raising SVC directly).
         * 2. pxMpuSettings->xSystemCallStackInfo.pulTaskStack must be NULL as
         *    it is non-NULL only during the execution of a system call (i.e.
         *    between system call enter and exit).
         * 3. System call is not for a kernel API disabled by the configuration
         *    in FreeRTOSConfig.h.
         * 4. We do not need to check that ucSystemCallNumber is within range
         *    because the assembly SVC handler checks that before calling
         *    this function.
         */
        if( ( ulSystemCallLocation >= ( uint32_t ) __syscalls_flash_start__ ) &&
            ( ulSystemCallLocation <= ( uint32_t ) __syscalls_flash_end__ ) &&
            ( pxMpuSettings->xSystemCallStackInfo.pulTaskStack == NULL ) &&
            ( uxSystemCallImplementations[ ucSystemCallNumber ] != ( UBaseType_t ) 0 ) )
        {
            pulSystemCallStack = pxMpuSettings->xSystemCallStackInfo.pulSystemCallStack;

            /* Make space on the system call stack for the stack frame. */
            pulSystemCallStack = pulSystemCallStack - ulStackFrameSize;

            /* Copy the stack frame. */
            for( i = 0; i < ulStackFrameSize; i++ )
            {
                pulSystemCallStack[ i ] = pulTaskStack[ i ];
            }

            /* Store the value of the Link Register before the SVC was raised.
             * It contains the address of the caller of the System Call entry
             * point (i.e. the caller of the MPU_<API>). We need to restore it
             * when we exit from the system call. */
            pxMpuSettings->xSystemCallStackInfo.ulLinkRegisterAtSystemCallEntry = pulTaskStack[ portOFFSET_TO_LR ];

            /* Use the pulSystemCallStack in thread mode. */
            __asm volatile ( "msr psp, %0" : : "r" ( pulSystemCallStack ) );

            /* Start executing the system call upon returning from this handler. */
            pulSystemCallStack[ portOFFSET_TO_PC ] = uxSystemCallImplementations[ ucSystemCallNumber ];

            /* Raise a request to exit from the system call upon finishing the
             * system call. */
            pulSystemCallStack[ portOFFSET_TO_LR ] = ( uint32_t ) vRequestSystemCallExit;

            /* Remember the location where we should copy the stack frame when we exit from
             * the system call. */
            pxMpuSettings->xSystemCallStackInfo.pulTaskStack = pulTaskStack + ulStackFrameSize;

            /* Record if the hardware used padding to force the stack pointer
             * to be double word aligned. */
            if( ( pulTaskStack[ portOFFSET_TO_PSR ] & portPSR_STACK_PADDING_MASK ) == portPSR_STACK_PADDING_MASK )
            {
                pxMpuSettings->ulTaskFlags |= portSTACK_FRAME_HAS_PADDING_FLAG;
            }
            else
            {
                pxMpuSettings->ulTaskFlags &= ( ~portSTACK_FRAME_HAS_PADDING_FLAG );
            }

            /* We ensure in pxPortInitialiseStack that the system call stack is
             * double word aligned and therefore, there is no need of padding.
             * Clear the bit[9] of stacked xPSR. */
            pulSystemCallStack[ portOFFSET_TO_PSR ] &= ( ~portPSR_STACK_PADDING_MASK );

            /* Raise the privilege for the duration of the system call. */
            __asm volatile
            (
                " .syntax unified     \n"
                " mrs r0, control     \n" /* Obtain current control value. */
                " movs r1, #1         \n" /* r1 = 1. */
                " bics r0, r1         \n" /* Clear nPRIV bit. */
                " msr control, r0     \n" /* Write back new control value. */
                ::: "r0", "r1", "memory"
            );
        }
    }

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    void vRequestSystemCallExit( void ) /* __attribute__( ( naked ) ) PRIVILEGED_FUNCTION */
    {
        __asm volatile ( "svc %0 \n" ::"i" ( portSVC_SYSTEM_CALL_EXIT ) : "memory" );
    }

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    void vSystemCallExit( uint32_t * pulSystemCallStack,
                          uint32_t ulLR ) /* PRIVILEGED_FUNCTION */
    {
        extern TaskHandle_t pxCurrentTCB;
        xMPU_SETTINGS * pxMpuSettings;
        uint32_t * pulTaskStack;
        uint32_t ulSystemCallLocation, i;
        const uint32_t ulStackFrameSize = 8;

        #if defined( __ARMCC_VERSION )

            /* Declaration when these variable are defined in code instead of being
             * exported from linker scripts. */
            extern uint32_t * __privileged_functions_start__;
            extern uint32_t * __privileged_functions_end__;

        #else

            /* Declaration when these variable are exported from linker scripts. */
            extern uint32_t __privileged_functions_start__[];
            extern uint32_t __privileged_functions_end__[];

        #endif /* #if defined( __ARMCC_VERSION ) */

        ulSystemCallLocation = pulSystemCallStack[ portOFFSET_TO_PC ];
        pxMpuSettings = xTaskGetMPUSettings( pxCurrentTCB );

        /* Checks:
         * 1. SVC is raised from the privileged code (i.e. application is not
         *    raising SVC directly). This SVC is only raised from
         *    vRequestSystemCallExit which is in the privileged code section.
         * 2. pxMpuSettings->xSystemCallStackInfo.pulTaskStack must not be NULL -
         *    this means that we previously entered a system call and the
         *    application is not attempting to exit without entering a system
         *    call.
         */
        if( ( ulSystemCallLocation >= ( uint32_t ) __privileged_functions_start__ ) &&
            ( ulSystemCallLocation <= ( uint32_t ) __privileged_functions_end__ ) &&
            ( pxMpuSettings->xSystemCallStackInfo.pulTaskStack != NULL ) )
        {
            pulTaskStack = pxMpuSettings->xSystemCallStackInfo.pulTaskStack;

            /* Make space on the task stack for the stack frame. */
            pulTaskStack = pulTaskStack - ulStackFrameSize;

            /* Copy the stack frame. */
            for( i = 0; i < ulStackFrameSize; i++ )
            {
                pulTaskStack[ i ] = pulSystemCallStack[ i ];
            }

            /* Use the pulTaskStack in thread mode. */
            __asm volatile ( "msr psp, %0" : : "r" ( pulTaskStack ) );

            /* Return to the caller of the System Call entry point (i.e. the
             * caller of the MPU_<API>). */
            pulTaskStack[ portOFFSET_TO_PC ] = pxMpuSettings->xSystemCallStackInfo.ulLinkRegisterAtSystemCallEntry;

            /* Ensure that LR has a valid value.*/
            pulTaskStack[ portOFFSET_TO_LR ] = pxMpuSettings->xSystemCallStackInfo.ulLinkRegisterAtSystemCallEntry;

            /* If the hardware used padding to force the stack pointer
             * to be double word aligned, set the stacked xPSR bit[9],
             * otherwise clear it. */
            if( ( pxMpuSettings->ulTaskFlags & portSTACK_FRAME_HAS_PADDING_FLAG ) == portSTACK_FRAME_HAS_PADDING_FLAG )
            {
                pulTaskStack[ portOFFSET_TO_PSR ] |= portPSR_STACK_PADDING_MASK;
            }
            else
            {
                pulTaskStack[ portOFFSET_TO_PSR ] &= ( ~portPSR_STACK_PADDING_MASK );
            }

            /* This is not NULL only for the duration of the system call. */
            pxMpuSettings->xSystemCallStackInfo.pulTaskStack = NULL;

            /* Drop the privilege before returning to the thread mode. */
            __asm volatile
            (
                " .syntax unified     \n"
                " mrs r0, control     \n" /* Obtain current control value. */
                " movs r1, #1         \n" /* r1 = 1. */
                " orrs r0, r1         \n" /* Set nPRIV bit. */
                " msr control, r0     \n" /* Write back new control value. */
                ::: "r0", "r1", "memory"
            );
        }
    }

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

    BaseType_t xPortIsTaskPrivileged( void ) /* PRIVILEGED_FUNCTION */
    {
        BaseType_t xTaskIsPrivileged = pdFALSE;
        const xMPU_SETTINGS * xTaskMpuSettings = xTaskGetMPUSettings( NULL ); /* Calling task's MPU settings. */

        if( ( xTaskMpuSettings->ulTaskFlags & portTASK_IS_PRIVILEGED_FLAG ) == portTASK_IS_PRIVILEGED_FLAG )
        {
            xTaskIsPrivileged = pdTRUE;
        }

        return xTaskIsPrivileged;
    }

#endif /* configENABLE_MPU == 1 */

/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

    StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                         TaskFunction_t pxCode,
                                         void * pvParameters,
                                         BaseType_t xRunPrivileged,
                                         xMPU_SETTINGS * xMPUSettings ) /* PRIVILEGED_FUNCTION */
    {
        xMPUSettings->ulContext[ 0 ] = 0x04040404; /* r4. */
        xMPUSettings->ulContext[ 1 ] = 0x05050505; /* r5. */
        xMPUSettings->ulContext[ 2 ] = 0x06060606; /* r6. */
        xMPUSettings->ulContext[ 3 ] = 0x07070707; /* r7. */
        xMPUSettings->ulContext[ 4 ] = 0x08080808; /* r8. */
        xMPUSettings->ulContext[ 5 ] = 0x09090909; /* r9. */
        xMPUSettings->ulContext[ 6 ] = 0x10101010; /* r10. */
        xMPUSettings->ulContext[ 7 ] = 0x11111111; /* r11. */

        xMPUSettings->ulContext[ 8 ] = ( uint32_t ) pvParameters;            /* r0. */
        xMPUSettings->ulContext[ 9 ] = 0x01010101;                           /* r1. */
        xMPUSettings->ulContext[ 10 ] = 0x02020202;                           /* r2. */
        xMPUSettings->ulContext[ 11 ] = 0x03030303;                           /* r3. */
        xMPUSettings->ulContext[ 12 ] = 0x12121212;                           /* r12. */
        xMPUSettings->ulContext[ 13 ] = ( uint32_t ) portTASK_RETURN_ADDRESS; /* LR. */
        xMPUSettings->ulContext[ 14 ] = ( uint32_t ) pxCode;                  /* PC. */
        xMPUSettings->ulContext[ 15 ] = portINITIAL_XPSR;                     /* xPSR. */

        xMPUSettings->ulContext[ 16 ] = ( uint32_t ) ( pxTopOfStack - 8 ); /* PSP with the hardware saved stack. */
        if( xRunPrivileged == pdTRUE )
        {
            xMPUSettings->ulTaskFlags |= portTASK_IS_PRIVILEGED_FLAG;
            xMPUSettings->ulContext[ 17 ] = ( uint32_t ) portINITIAL_CONTROL_PRIVILEGED; /* CONTROL. */
        }
        else
        {
            xMPUSettings->ulTaskFlags &= ( ~portTASK_IS_PRIVILEGED_FLAG );
            xMPUSettings->ulContext[ 17 ] = ( uint32_t ) portINITIAL_CONTROL_UNPRIVILEGED; /* CONTROL. */
        }
        xMPUSettings->ulContext[ 18 ] = portINITIAL_EXC_RETURN; /* LR (EXC_RETURN). */

        #if ( configUSE_MPU_WRAPPERS_V1 == 0 )
        {
            /* Ensure that the system call stack is double word aligned. */
            xMPUSettings->xSystemCallStackInfo.pulSystemCallStack = &( xMPUSettings->xSystemCallStackInfo.ulSystemCallStackBuffer[ configSYSTEM_CALL_STACK_SIZE - 1 ] );
            xMPUSettings->xSystemCallStackInfo.pulSystemCallStack = ( uint32_t * ) ( ( uint32_t ) ( xMPUSettings->xSystemCallStackInfo.pulSystemCallStack ) &
                                                                                     ( uint32_t ) ( ~( portBYTE_ALIGNMENT_MASK ) ) );

            /* This is not NULL only for the duration of a system call. */
            xMPUSettings->xSystemCallStackInfo.pulTaskStack = NULL;
        }
        #endif /* configUSE_MPU_WRAPPERS_V1 == 0 */

        return &( xMPUSettings->ulContext[ 19 ] );
    }

#else /* configENABLE_MPU */

    StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                         TaskFunction_t pxCode,
                                         void * pvParameters ) /* PRIVILEGED_FUNCTION */
    {
        /* Simulate the stack frame as it would be created by a context switch
         * interrupt. */
        #if ( portPRELOAD_REGISTERS == 0 )
        {
            pxTopOfStack--;                                          /* Offset added to account for the way the MCU uses the stack on entry/exit of interrupts. */
            *pxTopOfStack = portINITIAL_XPSR;                        /* xPSR. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) pxCode;                  /* PC. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) portTASK_RETURN_ADDRESS; /* LR. */
            pxTopOfStack -= 5;                                       /* R12, R3, R2 and R1. */
            *pxTopOfStack = ( StackType_t ) pvParameters;            /* R0. */
            pxTopOfStack -= 9;                                       /* R11..R4, EXC_RETURN. */
            *pxTopOfStack = portINITIAL_EXC_RETURN;
        }
        #else /* portPRELOAD_REGISTERS */
        {
            pxTopOfStack--;                                          /* Offset added to account for the way the MCU uses the stack on entry/exit of interrupts. */
            *pxTopOfStack = portINITIAL_XPSR;                        /* xPSR. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) pxCode;                  /* PC. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) portTASK_RETURN_ADDRESS; /* LR. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x12121212UL;            /* R12. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x03030303UL;            /* R3. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x02020202UL;            /* R2. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x01010101UL;            /* R1. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) pvParameters;            /* R0. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x11111111UL;            /* R11. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x10101010UL;            /* R10. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x09090909UL;            /* R09. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x08080808UL;            /* R08. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x07070707UL;            /* R07. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x06060606UL;            /* R06. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x05050505UL;            /* R05. */
            pxTopOfStack--;
            *pxTopOfStack = ( StackType_t ) 0x04040404UL;            /* R04. */
            pxTopOfStack--;
            *pxTopOfStack = portINITIAL_EXC_RETURN;                  /* EXC_RETURN. */
        }
        #endif /* portPRELOAD_REGISTERS */

        return pxTopOfStack;
    }

#endif /* configENABLE_MPU */

/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void ) /* PRIVILEGED_FUNCTION */
{
    /* An application can install FreeRTOS interrupt handlers in one of the
     * following ways:
     * 1. Direct Routing - Install the functions SVC_Handler and PendSV_Handler
     *    for SVCall and PendSV interrupts respectively.
     * 2. Indirect Routing - Install separate handlers for SVCall and PendSV
     *    interrupts and route program control from those handlers to
     *    SVC_Handler and PendSV_Handler functions.
     *
     * Applications that use Indirect Routing must set
     * configCHECK_HANDLER_INSTALLATION to 0 in their FreeRTOSConfig.h. Direct
     * routing, which is validated here when configCHECK_HANDLER_INSTALLATION
     * is 1, should be preferred when possible. */
    #if ( configCHECK_HANDLER_INSTALLATION == 1 )
    {
        const portISR_t * const pxVectorTable = portSCB_VTOR_REG;

        /* Validate that the application has correctly installed the FreeRTOS
         * handlers for SVCall and PendSV interrupts. We do not check the
         * installation of the SysTick handler because the application may
         * choose to drive the RTOS tick using a timer other than the SysTick
         * timer by overriding the weak function vPortSetupTimerInterrupt().
         *
         * Assertion failures here indicate incorrect installation of the
         * FreeRTOS handlers. For help installing the FreeRTOS handlers, see
         * https://www.freertos.org/Why-FreeRTOS/FAQs.
         *
         * Systems with a configurable address for the interrupt vector table
         * can also encounter assertion failures or even system faults here if
         * VTOR is not set correctly to point to the application's vector table. */
        configASSERT( pxVectorTable[ portVECTOR_INDEX_SVC ] == SVC_Handler );
        configASSERT( pxVectorTable[ portVECTOR_INDEX_PENDSV ] == PendSV_Handler );
    }
    #endif /* configCHECK_HANDLER_INSTALLATION */

    /* Make PendSV and SysTick the lowest priority interrupts, and make SVCall
     * the highest priority. */
    portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
    portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;
    portNVIC_SHPR2_REG = 0;

    #if ( configENABLE_MPU == 1 )
    {
        /* Setup the Memory Protection Unit (MPU). */
        prvSetupMPU();
    }
    #endif /* configENABLE_MPU */

    /* Start the timer that generates the tick ISR. Interrupts are disabled
     * here already. */
    vPortSetupTimerInterrupt();

    /* Initialize the critical nesting count ready for the first task. */
    ulCriticalNesting = 0;

    #if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )
    {
        xSchedulerRunning = pdTRUE;
    }
    #endif

    /* Start the first task. */
    vStartFirstTask();

    /* Should never get here as the tasks will now be executing. Call the task
     * exit error function to prevent compiler warnings about a static function
     * not being called in the case that the application writer overrides this
     * functionality by defining configTASK_RETURN_ADDRESS. Call
     * vTaskSwitchContext() so link time optimization does not remove the
     * symbol. */
    vTaskSwitchContext();
    prvTaskExitError();

    /* Should not get here. */
    return 0;
}

/*-----------------------------------------------------------*/

void vPortEndScheduler( void ) /* PRIVILEGED_FUNCTION */
{
    /* Not implemented in ports where there is nothing to return to.
     * Artificially force an assert. */
    configASSERT( ulCriticalNesting == 1000UL );
}

/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

    void vPortStoreTaskMPUSettings( xMPU_SETTINGS * xMPUSettings,
                                    const struct xMEMORY_REGION * const xRegions,
                                    StackType_t * pxBottomOfStack,
                                    configSTACK_DEPTH_TYPE uxStackDepth )
    {
        #if defined( __ARMCC_VERSION )

            /* Declaration when these variable are defined in code instead of being
             * exported from linker scripts. */
            extern uint32_t * __SRAM_segment_start__;
            extern uint32_t * __SRAM_segment_end__;
            extern uint32_t * __privileged_sram_start__;
            extern uint32_t * __privileged_sram_end__;

        #else
            /* Declaration when these variable are exported from linker scripts. */
            extern uint32_t __SRAM_segment_start__[];
            extern uint32_t __SRAM_segment_end__[];
            extern uint32_t __privileged_sram_start__[];
            extern uint32_t __privileged_sram_end__[];

        #endif /* defined( __ARMCC_VERSION ) */

        int32_t lIndex;
        uint32_t ul;

        if( xRegions == NULL )
        {
            /* No MPU regions are specified so allow access to all RAM. */
            xMPUSettings->xRegionsSettings[ 0 ].ulRBAR =
                ( ( ( uint32_t ) __SRAM_segment_start__ ) | /* Base address. */
                  ( portMPU_RBAR_REGION_NUMBER_VALID_BITMASK ) |
                  ( portSTACK_REGION ) );                   /* Region number. */

            xMPUSettings->xRegionsSettings[ 0 ].ulRASR =
                ( ( portMPU_REGION_PRIV_RW_UNPRIV_RW ) |
                  ( portMPU_REGION_EXECUTE_NEVER ) |
                  ( ( configS_C_B_SRAM & portMPU_RASR_S_C_B_BITMASK ) << portMPU_RASR_S_C_B_LOCATION ) |
                  ( prvGetMPURegionSizeSetting( ( uint32_t ) __SRAM_segment_end__ - ( uint32_t ) __SRAM_segment_start__ ) ) |
                  ( portMPU_RASR_REGION_ENABLE_BITMASK ) );


            /* Invalidate user configurable regions. */
            for( ul = 1UL; ul <= portNUM_CONFIGURABLE_REGIONS; ul++ )
            {
                xMPUSettings->xRegionsSettings[ ul ].ulRBAR = ( ( ul - 1UL ) | portMPU_RBAR_REGION_NUMBER_VALID_BITMASK );
                xMPUSettings->xRegionsSettings[ ul ].ulRASR = 0UL;
            }
        }
        else
        {
            /* This function is called automatically when the task is created - in
             * which case the stack region parameters will be valid.  At all other
             * times the stack parameters will not be valid and it is assumed that the
             * stack region has already been configured. */
            if( uxStackDepth > 0 )
            {
                /* Define the region that allows access to the stack. */
                xMPUSettings->xRegionsSettings[ 0 ].ulRBAR =
                    ( ( ( uint32_t ) pxBottomOfStack ) |
                      ( portMPU_RBAR_REGION_NUMBER_VALID_BITMASK ) |
                      ( portSTACK_REGION ) ); /* Region number. */

                xMPUSettings->xRegionsSettings[ 0 ].ulRASR =
                    ( ( portMPU_REGION_PRIV_RW_UNPRIV_RW ) |
                      ( portMPU_REGION_EXECUTE_NEVER ) |
                      ( prvGetMPURegionSizeSetting( uxStackDepth * ( uint32_t ) sizeof( StackType_t ) ) ) |
                      ( ( configS_C_B_SRAM & portMPU_RASR_S_C_B_BITMASK ) << portMPU_RASR_S_C_B_LOCATION ) |
                      ( portMPU_RASR_REGION_ENABLE_BITMASK ) );
            }

            lIndex = 0;

            for( ul = 1UL; ul <= portNUM_CONFIGURABLE_REGIONS; ul++ )
            {
                if( ( xRegions[ lIndex ] ).ulLengthInBytes > 0UL )
                {
                    /* Translate the generic region definition contained in
                     * xRegions into the CM0+ specific MPU settings that are then
                     * stored in xMPUSettings. */
                    xMPUSettings->xRegionsSettings[ ul ].ulRBAR =
                        ( ( uint32_t ) xRegions[ lIndex ].pvBaseAddress ) |
                        ( portMPU_RBAR_REGION_NUMBER_VALID_BITMASK ) |
                        ( ul - 1UL ); /* Region number. */

                    xMPUSettings->xRegionsSettings[ ul ].ulRASR =
                        ( prvGetMPURegionSizeSetting( xRegions[ lIndex ].ulLengthInBytes ) ) |
                        ( xRegions[ lIndex ].ulParameters ) |
                        ( portMPU_RASR_REGION_ENABLE_BITMASK );
                }
                else
                {
                    /* Invalidate the region. */
                    xMPUSettings->xRegionsSettings[ ul ].ulRBAR = ( ( ul - 1UL ) | portMPU_RBAR_REGION_NUMBER_VALID_BITMASK );
                    xMPUSettings->xRegionsSettings[ ul ].ulRASR = 0UL;
                }

            lIndex++;
        }
    }
}

#endif /* configENABLE_MPU */

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    BaseType_t xPortIsAuthorizedToAccessBuffer( const void * pvBuffer,
                                                uint32_t ulBufferLength,
                                                uint32_t ulAccessRequested ) /* PRIVILEGED_FUNCTION */

    {
        uint32_t i, ulBufferStartAddress, ulBufferEndAddress;
        uint32_t ulRegionStart, ulRegionSize, ulRegionEnd;
        uint32_t ulMPURegionAccessPermissions;
        BaseType_t xAccessGranted = pdFALSE;
        const xMPU_SETTINGS * xTaskMpuSettings = xTaskGetMPUSettings( NULL ); /* Calling task's MPU settings. */

        if( xSchedulerRunning == pdFALSE )
        {
            /* Grant access to all the kernel objects before the scheduler
             * is started. It is necessary because there is no task running
             * yet and therefore, we cannot use the permissions of any
             * task. */
            xAccessGranted = pdTRUE;
        }
        else if( ( xTaskMpuSettings->ulTaskFlags & portTASK_IS_PRIVILEGED_FLAG ) == portTASK_IS_PRIVILEGED_FLAG )
        {
            xAccessGranted = pdTRUE;
        }
        else
        {
            if( portADD_UINT32_WILL_OVERFLOW( ( ( uint32_t ) pvBuffer ), ( ulBufferLength - 1UL ) ) == pdFALSE )
            {
                ulBufferStartAddress = ( uint32_t ) pvBuffer;
                ulBufferEndAddress = ( ( ( uint32_t ) pvBuffer ) + ulBufferLength - 1UL );

                for( i = 0; i < portTOTAL_NUM_REGIONS; i++ )
                {
                    /* Is the MPU region enabled? */
                    if( ( xTaskMpuSettings->xRegionsSettings[ i ].ulRASR &
                          portMPU_RASR_REGION_ENABLE_BITMASK ) == portMPU_RASR_REGION_ENABLE_BITMASK )
                    {
                        ulRegionStart = portEXTRACT_FIRST_ADDRESS_FROM_RBAR( xTaskMpuSettings->xRegionsSettings[ i ].ulRBAR );
                        ulRegionSize = portEXTRACT_REGION_SIZE_FROM_RASR( xTaskMpuSettings->xRegionsSettings[ i ].ulRASR );
                        ulRegionEnd = ulRegionStart + ulRegionSize;

                        if( portIS_ADDRESS_WITHIN_RANGE( ulBufferStartAddress,
                                                         ulRegionStart,
                                                         ulRegionEnd ) &&
                            portIS_ADDRESS_WITHIN_RANGE( ulBufferEndAddress,
                                                         ulRegionStart,
                                                         ulRegionEnd ) )
                        {
                            ulMPURegionAccessPermissions = xTaskMpuSettings->xRegionsSettings[ i ].ulRASR &
                                                           portMPU_RASR_AP_BITMASK;

                            if( ulAccessRequested == tskMPU_READ_PERMISSION ) /* RO. */
                            {
                                if( ( ulMPURegionAccessPermissions == portMPU_REGION_PRIV_RW_UNPRIV_RO ) ||
                                    ( ulMPURegionAccessPermissions == portMPU_REGION_PRIV_RO_UNPRIV_RO ) ||
                                    ( ulMPURegionAccessPermissions == portMPU_REGION_PRIV_RW_UNPRIV_RW ) )
                                {
                                    xAccessGranted = pdTRUE;
                                    break;
                                }
                            }
                            else if( ( ulAccessRequested & tskMPU_WRITE_PERMISSION ) != 0UL ) /* W or RW. */
                            {
                                if( ulMPURegionAccessPermissions == portMPU_REGION_PRIV_RW_UNPRIV_RW )
                                {
                                    xAccessGranted = pdTRUE;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        return xAccessGranted;
    }

#endif /* #if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) ) */

/*-----------------------------------------------------------*/

BaseType_t xPortIsInsideInterrupt( void )
{
    uint32_t ulCurrentInterrupt;
    BaseType_t xReturn;

    /* Obtain the number of the currently executing interrupt. Interrupt Program
     * Status Register (IPSR) holds the exception number of the currently-executing
     * exception or zero for Thread mode.*/
    __asm volatile ( "mrs %0, ipsr" : "=r" ( ulCurrentInterrupt )::"memory" );

    if( ulCurrentInterrupt == 0 )
    {
        xReturn = pdFALSE;
    }
    else
    {
        xReturn = pdTRUE;
    }

    return xReturn;
}

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) && ( configENABLE_ACCESS_CONTROL_LIST == 1 ) )

    void vPortGrantAccessToKernelObject( TaskHandle_t xInternalTaskHandle,
                                         int32_t lInternalIndexOfKernelObject ) /* PRIVILEGED_FUNCTION */
    {
        uint32_t ulAccessControlListEntryIndex, ulAccessControlListEntryBit;
        xMPU_SETTINGS * xTaskMpuSettings;

        ulAccessControlListEntryIndex = ( ( uint32_t ) lInternalIndexOfKernelObject / portACL_ENTRY_SIZE_BITS );
        ulAccessControlListEntryBit = ( ( uint32_t ) lInternalIndexOfKernelObject % portACL_ENTRY_SIZE_BITS );

        xTaskMpuSettings = xTaskGetMPUSettings( xInternalTaskHandle );

        xTaskMpuSettings->ulAccessControlList[ ulAccessControlListEntryIndex ] |= ( 1U << ulAccessControlListEntryBit );
    }

#endif /* #if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) && ( configENABLE_ACCESS_CONTROL_LIST == 1 ) ) */

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) && ( configENABLE_ACCESS_CONTROL_LIST == 1 ) )

    void vPortRevokeAccessToKernelObject( TaskHandle_t xInternalTaskHandle,
                                          int32_t lInternalIndexOfKernelObject ) /* PRIVILEGED_FUNCTION */
    {
        uint32_t ulAccessControlListEntryIndex, ulAccessControlListEntryBit;
        xMPU_SETTINGS * xTaskMpuSettings;

        ulAccessControlListEntryIndex = ( ( uint32_t ) lInternalIndexOfKernelObject / portACL_ENTRY_SIZE_BITS );
        ulAccessControlListEntryBit = ( ( uint32_t ) lInternalIndexOfKernelObject % portACL_ENTRY_SIZE_BITS );

        xTaskMpuSettings = xTaskGetMPUSettings( xInternalTaskHandle );

        xTaskMpuSettings->ulAccessControlList[ ulAccessControlListEntryIndex ] &= ~( 1U << ulAccessControlListEntryBit );
    }

#endif /* #if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) && ( configENABLE_ACCESS_CONTROL_LIST == 1 ) ) */

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    #if ( configENABLE_ACCESS_CONTROL_LIST == 1 )

        BaseType_t xPortIsAuthorizedToAccessKernelObject( int32_t lInternalIndexOfKernelObject ) /* PRIVILEGED_FUNCTION */
        {
            uint32_t ulAccessControlListEntryIndex, ulAccessControlListEntryBit;
            BaseType_t xAccessGranted = pdFALSE;
            const xMPU_SETTINGS * xTaskMpuSettings;

            if( xSchedulerRunning == pdFALSE )
            {
                /* Grant access to all the kernel objects before the scheduler
                 * is started. It is necessary because there is no task running
                 * yet and therefore, we cannot use the permissions of any
                 * task. */
                xAccessGranted = pdTRUE;
            }
            else
            {
                xTaskMpuSettings = xTaskGetMPUSettings( NULL ); /* Calling task's MPU settings. */

                ulAccessControlListEntryIndex = ( ( uint32_t ) lInternalIndexOfKernelObject / portACL_ENTRY_SIZE_BITS );
                ulAccessControlListEntryBit = ( ( uint32_t ) lInternalIndexOfKernelObject % portACL_ENTRY_SIZE_BITS );

                if( ( xTaskMpuSettings->ulTaskFlags & portTASK_IS_PRIVILEGED_FLAG ) == portTASK_IS_PRIVILEGED_FLAG )
                {
                    xAccessGranted = pdTRUE;
                }
                else
                {
                    if( ( xTaskMpuSettings->ulAccessControlList[ ulAccessControlListEntryIndex ] & ( 1U << ulAccessControlListEntryBit ) ) != 0 )
                    {
                        xAccessGranted = pdTRUE;
                    }
                }
            }

            return xAccessGranted;
        }

    #else /* #if ( configENABLE_ACCESS_CONTROL_LIST == 1 ) */

        BaseType_t xPortIsAuthorizedToAccessKernelObject( int32_t lInternalIndexOfKernelObject ) /* PRIVILEGED_FUNCTION */
        {
            ( void ) lInternalIndexOfKernelObject;

            /* If Access Control List feature is not used, all the tasks have
             * access to all the kernel objects. */
            return pdTRUE;
        }

    #endif /* #if ( configENABLE_ACCESS_CONTROL_LIST == 1 ) */

#endif /* #if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) ) */

/*-----------------------------------------------------------*/
