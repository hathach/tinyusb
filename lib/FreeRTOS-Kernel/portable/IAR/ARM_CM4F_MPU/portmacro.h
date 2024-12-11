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

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* IAR includes. */
#include <intrinsics.h>

/* Type definitions. */
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
    #error "configTICK_TYPE_WIDTH_IN_BITS set to unsupported tick type width."
#endif

/* Errata 837070 workaround must be enabled on Cortex-M7 r0p0
 * and r0p1 cores. */
#ifndef configENABLE_ERRATA_837070_WORKAROUND
    #define configENABLE_ERRATA_837070_WORKAROUND    0
#endif
/*-----------------------------------------------------------*/

/* MPU specific constants. */
#define portUSING_MPU_WRAPPERS                                   1
#define portPRIVILEGE_BIT                                        ( 0x80000000UL )

#define portMPU_REGION_READ_WRITE                                ( 0x03UL << 24UL )
#define portMPU_REGION_PRIVILEGED_READ_ONLY                      ( 0x05UL << 24UL )
#define portMPU_REGION_READ_ONLY                                 ( 0x06UL << 24UL )
#define portMPU_REGION_PRIVILEGED_READ_WRITE                     ( 0x01UL << 24UL )
#define portMPU_REGION_PRIVILEGED_READ_WRITE_UNPRIV_READ_ONLY    ( 0x02UL << 24UL )
#define portMPU_REGION_CACHEABLE_BUFFERABLE                      ( 0x07UL << 16UL )
#define portMPU_REGION_EXECUTE_NEVER                             ( 0x01UL << 28UL )

/* Location of the TEX,S,C,B bits in the MPU Region Attribute and Size
 * Register (RASR). */
#define portMPU_RASR_TEX_S_C_B_LOCATION                          ( 16UL )
#define portMPU_RASR_TEX_S_C_B_MASK                              ( 0x3FUL )

/* MPU settings that can be overridden in FreeRTOSConfig.h. */
#ifndef configTOTAL_MPU_REGIONS
    /* Define to 8 for backward compatibility. */
    #define configTOTAL_MPU_REGIONS    ( 8UL )
#endif

/*
 * The TEX, Shareable (S), Cacheable (C) and Bufferable (B) bits define the
 * memory type, and where necessary the cacheable and shareable properties
 * of the memory region.
 *
 * The TEX, C, and B bits together indicate the memory type of the region,
 * and:
 * - For Normal memory, the cacheable properties of the region.
 * - For Device memory, whether the region is shareable.
 *
 * For Normal memory regions, the S bit indicates whether the region is
 * shareable. For Strongly-ordered and Device memory, the S bit is ignored.
 *
 * See the following two tables for setting TEX, S, C and B bits for
 * unprivileged flash, privileged flash and privileged RAM regions.
 *
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | TEX | C | B | Memory type            |  Description or Normal region cacheability             |  Shareable?             |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 000 | 0 | 0 | Strongly-ordered       |  Strongly ordered                                      |  Shareable              |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 000 | 0 | 1 | Device                 |  Shared device                                         |  Shareable              |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 000 | 1 | 0 | Normal                 |  Outer and inner   write-through; no write allocate    |  S bit                  |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 000 | 1 | 1 | Normal                 |  Outer and inner   write-back; no write allocate       |  S bit                  |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 001 | 0 | 0 | Normal                 |  Outer and inner   Non-cacheable                       |  S bit                  |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 001 | 0 | 1 | Reserved               |  Reserved                                              |  Reserved               |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 001 | 1 | 0 | IMPLEMENTATION DEFINED |  IMPLEMENTATION DEFINED                                |  IMPLEMENTATION DEFINED |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 001 | 1 | 1 | Normal                 |  Outer and inner   write-back; write and read allocate |  S bit                  |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 010 | 0 | 0 | Device                 |  Non-shared device                                     |  Not shareable          |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 010 | 0 | 1 | Reserved               |  Reserved                                              |  Reserved               |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 010 | 1 | X | Reserved               |  Reserved                                              |  Reserved               |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 011 | X | X | Reserved               |  Reserved                                              |  Reserved               |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 | 1BB | A | A | Normal                 | Cached memory, with AA and BB indicating the inner and |  Reserved               |
 |     |   |   |                        | outer cacheability rules that must be exported on the  |                         |
 |     |   |   |                        | bus. See the table below for the cacheability policy   |                         |
 |     |   |   |                        | encoding. memory, BB=Outer policy, AA=Inner policy.    |                         |
 +-----+---+---+------------------------+--------------------------------------------------------+-------------------------+
 |
 +-----------------------------------------+----------------------------------------+
 | AA or BB subfield of {TEX,C,B} encoding |  Cacheability policy                   |
 +-----------------------------------------+----------------------------------------+
 | 00                                      |  Non-cacheable                         |
 +-----------------------------------------+----------------------------------------+
 | 01                                      |  Write-back, write and   read allocate |
 +-----------------------------------------+----------------------------------------+
 | 10                                      |  Write-through, no write   allocate    |
 +-----------------------------------------+----------------------------------------+
 | 11                                      |  Write-back, no write   allocate       |
 +-----------------------------------------+----------------------------------------+
 */

/* TEX, Shareable (S), Cacheable (C) and Bufferable (B) bits for flash
 * region. */
#ifndef configTEX_S_C_B_FLASH
    /* Default to TEX=000, S=1, C=1, B=1 for backward compatibility. */
    #define configTEX_S_C_B_FLASH    ( 0x07UL )
#endif

/* TEX, Shareable (S), Cacheable (C) and Bufferable (B) bits for RAM
 * region. */
#ifndef configTEX_S_C_B_SRAM
    /* Default to TEX=000, S=1, C=1, B=1 for backward compatibility. */
    #define configTEX_S_C_B_SRAM          ( 0x07UL )
#endif

#define portSTACK_REGION                  ( configTOTAL_MPU_REGIONS - 5UL )
#define portGENERAL_PERIPHERALS_REGION    ( configTOTAL_MPU_REGIONS - 4UL )
#define portUNPRIVILEGED_FLASH_REGION     ( configTOTAL_MPU_REGIONS - 3UL )
#define portPRIVILEGED_FLASH_REGION       ( configTOTAL_MPU_REGIONS - 2UL )
#define portPRIVILEGED_RAM_REGION         ( configTOTAL_MPU_REGIONS - 1UL )
#define portFIRST_CONFIGURABLE_REGION     ( 0UL )
#define portLAST_CONFIGURABLE_REGION      ( configTOTAL_MPU_REGIONS - 6UL )
#define portNUM_CONFIGURABLE_REGIONS      ( configTOTAL_MPU_REGIONS - 5UL )
#define portTOTAL_NUM_REGIONS_IN_TCB      ( portNUM_CONFIGURABLE_REGIONS + 1 ) /* Plus 1 to create space for the stack region. */

typedef struct MPU_REGION_REGISTERS
{
    uint32_t ulRegionBaseAddress;
    uint32_t ulRegionAttribute;
} xMPU_REGION_REGISTERS;

typedef struct MPU_REGION_SETTINGS
{
    uint32_t ulRegionStartAddress;
    uint32_t ulRegionEndAddress;
    uint32_t ulRegionPermissions;
} xMPU_REGION_SETTINGS;

#if ( configUSE_MPU_WRAPPERS_V1 == 0 )

    #ifndef configSYSTEM_CALL_STACK_SIZE
        #error "configSYSTEM_CALL_STACK_SIZE must be defined to the desired size of the system call stack in words for using MPU wrappers v2."
    #endif

    typedef struct SYSTEM_CALL_STACK_INFO
    {
        uint32_t ulSystemCallStackBuffer[ configSYSTEM_CALL_STACK_SIZE ];
        uint32_t * pulSystemCallStack;
        uint32_t * pulTaskStack;
        uint32_t ulLinkRegisterAtSystemCallEntry;
    } xSYSTEM_CALL_STACK_INFO;

#endif /* configUSE_MPU_WRAPPERS_V1 == 0 */

#define MAX_CONTEXT_SIZE                    ( 52 )

/* Size of an Access Control List (ACL) entry in bits. */
#define portACL_ENTRY_SIZE_BITS             ( 32U )

/* Flags used for xMPU_SETTINGS.ulTaskFlags member. */
#define portSTACK_FRAME_HAS_PADDING_FLAG    ( 1UL << 0UL )
#define portTASK_IS_PRIVILEGED_FLAG         ( 1UL << 1UL )

typedef struct MPU_SETTINGS
{
    xMPU_REGION_REGISTERS xRegion[ portTOTAL_NUM_REGIONS_IN_TCB ];
    xMPU_REGION_SETTINGS xRegionSettings[ portTOTAL_NUM_REGIONS_IN_TCB ];
    uint32_t ulContext[ MAX_CONTEXT_SIZE ];
    uint32_t ulTaskFlags;

    #if ( configUSE_MPU_WRAPPERS_V1 == 0 )
        xSYSTEM_CALL_STACK_INFO xSystemCallStackInfo;
        #if ( configENABLE_ACCESS_CONTROL_LIST == 1 )
            uint32_t ulAccessControlList[ ( configPROTECTED_KERNEL_OBJECT_POOL_SIZE / portACL_ENTRY_SIZE_BITS ) + 1 ];
        #endif
    #endif
} xMPU_SETTINGS;

/* Architecture specifics. */
#define portSTACK_GROWTH      ( -1 )
#define portTICK_PERIOD_MS    ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT    8
/*-----------------------------------------------------------*/

/* SVC numbers for various services. */
#define portSVC_START_SCHEDULER        100
#define portSVC_YIELD                  101
#define portSVC_RAISE_PRIVILEGE        102
#define portSVC_SYSTEM_CALL_EXIT       103

/* Scheduler utilities. */

#define portYIELD()    __asm volatile ( "   SVC %0  \n" ::"i" ( portSVC_YIELD ) : "memory" )
#define portYIELD_WITHIN_API()                          \
    {                                                   \
        /* Set a PendSV to request a context switch. */ \
        portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT; \
        __DSB();                                        \
        __ISB();                                        \
    }

#define portNVIC_INT_CTRL_REG     ( *( ( volatile uint32_t * ) 0xe000ed04 ) )
#define portNVIC_PENDSVSET_BIT    ( 1UL << 28UL )
#define portEND_SWITCHING_ISR( xSwitchRequired ) \
    do                                           \
    {                                            \
        if( xSwitchRequired != pdFALSE )         \
        {                                        \
            traceISR_EXIT_TO_SCHEDULER();        \
            portYIELD_WITHIN_API();              \
        }                                        \
        else                                     \
        {                                        \
            traceISR_EXIT();                     \
        }                                        \
    } while( 0 )
#define portYIELD_FROM_ISR( x )    portEND_SWITCHING_ISR( x )
/*-----------------------------------------------------------*/

/* Architecture specific optimisations. */
#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION    1
#endif

#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 1 )

/* Check the configuration. */
    #if ( configMAX_PRIORITIES > 32 )
        #error "configUSE_PORT_OPTIMISED_TASK_SELECTION can only be set to 1 when configMAX_PRIORITIES is less than or equal to 32.  It is very rare that a system requires more than 10 to 15 difference priorities as tasks that share a priority will time slice."
    #endif

/* Store/clear the ready priorities in a bit map. */
    #define portRECORD_READY_PRIORITY( uxPriority, uxReadyPriorities )    ( uxReadyPriorities ) |= ( 1UL << ( uxPriority ) )
    #define portRESET_READY_PRIORITY( uxPriority, uxReadyPriorities )     ( uxReadyPriorities ) &= ~( 1UL << ( uxPriority ) )

/*-----------------------------------------------------------*/

    #define portGET_HIGHEST_PRIORITY( uxTopPriority, uxReadyPriorities )    uxTopPriority = ( 31UL - ( ( uint32_t ) __CLZ( ( uxReadyPriorities ) ) ) )

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */
/*-----------------------------------------------------------*/

/* Critical section management. */
extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );

#if ( configENABLE_ERRATA_837070_WORKAROUND == 1 )
    #define portDISABLE_INTERRUPTS()                           \
    {                                                          \
        __disable_interrupt();                                 \
        __set_BASEPRI( configMAX_SYSCALL_INTERRUPT_PRIORITY ); \
        __DSB();                                               \
        __ISB();                                               \
        __enable_interrupt();                                  \
    }
#else
    #define portDISABLE_INTERRUPTS()                           \
    {                                                          \
        __set_BASEPRI( configMAX_SYSCALL_INTERRUPT_PRIORITY ); \
        __DSB();                                               \
        __ISB();                                               \
    }
#endif /* if ( configENABLE_ERRATA_837070_WORKAROUND == 1 ) */

#define portENABLE_INTERRUPTS()                   __set_BASEPRI( 0 )
#define portENTER_CRITICAL()                      vPortEnterCritical()
#define portEXIT_CRITICAL()                       vPortExitCritical()
#define portSET_INTERRUPT_MASK_FROM_ISR()         __get_BASEPRI(); portDISABLE_INTERRUPTS()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )    __set_BASEPRI( x )
/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site.  These are
 * not necessary for to use this port.  They are defined so the common demo files
 * (which build with all the ports) will build. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )
/*-----------------------------------------------------------*/

#if ( configASSERT_DEFINED == 1 )
    void vPortValidateInterruptPriority( void );
    #define portASSERT_IF_INTERRUPT_PRIORITY_INVALID()    vPortValidateInterruptPriority()
#endif

/* portNOP() is not required by this port. */
#define portNOP()

#define portINLINE              __inline

#ifndef portFORCE_INLINE
    #define portFORCE_INLINE    inline __attribute__( ( always_inline ) )
#endif

/*-----------------------------------------------------------*/

portFORCE_INLINE static BaseType_t xPortIsInsideInterrupt( void )
{
    uint32_t ulCurrentInterrupt;
    BaseType_t xReturn;

    /* Obtain the number of the currently executing interrupt. */
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

extern BaseType_t xIsPrivileged( void );
extern void vResetPrivilege( void );
extern void vPortSwitchToUserMode( void );

/**
 * @brief Checks whether or not the processor is privileged.
 *
 * @return 1 if the processor is already privileged, 0 otherwise.
 */
#define portIS_PRIVILEGED()          xIsPrivileged()

/**
 * @brief Raise an SVC request to raise privilege.
 */
#define portRAISE_PRIVILEGE()        __asm volatile ( "svc %0 \n" ::"i" ( portSVC_RAISE_PRIVILEGE ) : "memory" );

/**
 * @brief Lowers the privilege level by setting the bit 0 of the CONTROL
 * register.
 */
#define portRESET_PRIVILEGE()        vResetPrivilege()

/**
 * @brief Make a task unprivileged.
 *
 * It must be called from privileged tasks only. Calling it from unprivileged
 * task will result in a memory protection fault.
 */
#define portSWITCH_TO_USER_MODE()    vPortSwitchToUserMode()
/*-----------------------------------------------------------*/

extern BaseType_t xPortIsTaskPrivileged( void );

/**
 * @brief Checks whether or not the calling task is privileged.
 *
 * @return pdTRUE if the calling task is privileged, pdFALSE otherwise.
 */
#define portIS_TASK_PRIVILEGED()    xPortIsTaskPrivileged()
/*-----------------------------------------------------------*/

#ifndef configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY
    #warning "configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY is not defined. We recommend defining it to 1 in FreeRTOSConfig.h for better security. www.FreeRTOS.org/FreeRTOS-V10.3.x.html"
    #define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY    0
#endif
/*-----------------------------------------------------------*/

/* Suppress warnings that are generated by the IAR tools, but cannot be fixed in
 * the source code because to do so would cause other compilers to generate
 * warnings. */
#pragma diag_suppress=Pe191
#pragma diag_suppress=Pa082
#pragma diag_suppress=Be006
/*-----------------------------------------------------------*/

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* PORTMACRO_H */
