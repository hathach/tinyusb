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

/* Including FreeRTOSConfig.h here will cause build errors if the header file
contains code not understood by the assembler - for example the 'extern' keyword.
To avoid errors place any such code inside a #ifdef __ICCARM__/#endif block so
the code is included in C files but excluded by the preprocessor in assembly
files (__ICCARM__ is defined by the IAR C compiler but not by the IAR assembler. */
#include <FreeRTOSConfig.h>
#include <mpu_syscall_numbers.h>

    RSEG    CODE:CODE(2)
    thumb

    EXTERN pxCurrentTCB
    EXTERN vTaskSwitchContext
    EXTERN vPortSVCHandler_C
    EXTERN vSystemCallEnter
    EXTERN vSystemCallExit

    PUBLIC xPortPendSVHandler
    PUBLIC vPortSVCHandler
    PUBLIC vPortStartFirstTask
    PUBLIC vPortEnableVFP
    PUBLIC vPortRestoreContextOfFirstTask
    PUBLIC xIsPrivileged
    PUBLIC vResetPrivilege

/*-----------------------------------------------------------*/

#ifndef configUSE_MPU_WRAPPERS_V1
    #define configUSE_MPU_WRAPPERS_V1 0
#endif

/* Errata 837070 workaround must be enabled on Cortex-M7 r0p0
 * and r0p1 cores. */
#ifndef configENABLE_ERRATA_837070_WORKAROUND
    #define configENABLE_ERRATA_837070_WORKAROUND 0
#endif

/* These must be in sync with portmacro.h. */
#define portSVC_START_SCHEDULER        100
#define portSVC_SYSTEM_CALL_EXIT       103
/*-----------------------------------------------------------*/

xPortPendSVHandler:

    ldr r3, =pxCurrentTCB
    ldr r2, [r3]                           /* r2 = pxCurrentTCB. */
    ldr r1, [r2]                           /* r1 = Location where the context should be saved. */

    /*------------ Save Context. ----------- */
    mrs r3, control
    mrs r0, psp
    isb

    add r0, r0, #0x20                      /* Move r0 to location where s0 is saved. */
    tst lr, #0x10
    ittt eq
    vstmiaeq r1!, {s16-s31}                /* Store s16-s31. */
    vldmiaeq r0, {s0-s16}                  /* Copy hardware saved FP context into s0-s16. */
    vstmiaeq r1!, {s0-s16}                 /* Store hardware saved FP context. */
    sub r0, r0, #0x20                      /* Set r0 back to the location of hardware saved context. */

    stmia r1!, {r3-r11, lr}                /* Store CONTROL register, r4-r11 and LR. */
    ldmia r0, {r4-r11}                     /* Copy hardware saved context into r4-r11. */
    stmia r1!, {r0, r4-r11}                /* Store original PSP (after hardware has saved context) and the hardware saved context. */
    str r1, [r2]                           /* Save the location from where the context should be restored as the first member of TCB. */

    /*---------- Select next task. --------- */
    mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
#if ( configENABLE_ERRATA_837070_WORKAROUND == 1 )
    cpsid i                                /* ARM Cortex-M7 r0p1 Errata 837070 workaround. */
#endif
    msr basepri, r0
    dsb
    isb
#if ( configENABLE_ERRATA_837070_WORKAROUND == 1 )
    cpsie i                                /* ARM Cortex-M7 r0p1 Errata 837070 workaround. */
#endif
    bl vTaskSwitchContext
    mov r0, #0
    msr basepri, r0

    /*------------ Program MPU. ------------ */
    ldr r3, =pxCurrentTCB
    ldr r2, [r3]                           /* r2 = pxCurrentTCB. */
    add r2, r2, #4                         /* r2 = Second item in the TCB which is xMPUSettings. */

    dmb                                    /* Complete outstanding transfers before disabling MPU. */
    ldr r0, =0xe000ed94                    /* MPU_CTRL register. */
    ldr r3, [r0]                           /* Read the value of MPU_CTRL. */
    bic r3, #1                             /* r3 = r3 & ~1 i.e. Clear the bit 0 in r3. */
    str r3, [r0]                           /* Disable MPU. */

    ldr r0, =0xe000ed9c                    /* Region Base Address register. */
    ldmia r2!, {r4-r11}                    /* Read 4 sets of MPU registers [MPU Region # 0 - 3]. */
    stmia r0, {r4-r11}                     /* Write 4 sets of MPU registers [MPU Region # 0 - 3]. */

#ifdef configTOTAL_MPU_REGIONS
    #if ( configTOTAL_MPU_REGIONS == 16 )
        ldmia r2!, {r4-r11}                 /* Read 4 sets of MPU registers [MPU Region # 4 - 7]. */
        stmia r0, {r4-r11}                  /* Write 4 sets of MPU registers. [MPU Region # 4 - 7]. */
        ldmia r2!, {r4-r11}                 /* Read 4 sets of MPU registers [MPU Region # 8 - 11]. */
        stmia r0, {r4-r11}                  /* Write 4 sets of MPU registers. [MPU Region # 8 - 11]. */
    #endif /* configTOTAL_MPU_REGIONS == 16. */
#endif

    ldr r0, =0xe000ed94                    /* MPU_CTRL register. */
    ldr r3, [r0]                           /* Read the value of MPU_CTRL. */
    orr r3, #1                             /* r3 = r3 | 1 i.e. Set the bit 0 in r3. */
    str r3, [r0]                           /* Enable MPU. */
    dsb                                    /* Force memory writes before continuing. */

    /*---------- Restore Context. ---------- */
    ldr r3, =pxCurrentTCB
    ldr r2, [r3]                           /* r2 = pxCurrentTCB. */
    ldr r1, [r2]                           /* r1 = Location of saved context in TCB. */

    ldmdb r1!, {r0, r4-r11}                /* r0 contains PSP after the hardware had saved context. r4-r11 contain hardware saved context. */
    msr psp, r0
    stmia r0!, {r4-r11}                    /* Copy the hardware saved context on the task stack. */
    ldmdb r1!, {r3-r11, lr}                /* r3 contains CONTROL register. r4-r11 and LR restored. */
    msr control, r3

    tst lr, #0x10
    ittt eq
    vldmdbeq r1!, {s0-s16}                 /* s0-s16 contain hardware saved FP context. */
    vstmiaeq r0!, {s0-s16}                 /* Copy hardware saved FP context on the task stack. */
    vldmdbeq r1!, {s16-s31}                /* Restore s16-s31. */

    str r1, [r2]                           /* Save the location where the context should be saved next as the first member of TCB. */
    bx lr

/*-----------------------------------------------------------*/

#if ( configUSE_MPU_WRAPPERS_V1 == 0 )

vPortSVCHandler:
    tst lr, #4
    ite eq
    mrseq r0, msp
    mrsne r0, psp

    ldr r1, [r0, #24]
    ldrb r2, [r1, #-2]
    cmp r2, #NUM_SYSTEM_CALLS
    blt syscall_enter
    cmp r2, #portSVC_SYSTEM_CALL_EXIT
    beq syscall_exit
    b vPortSVCHandler_C

    syscall_enter:
        mov r1, lr
        b vSystemCallEnter

    syscall_exit:
        mov r1, lr
        b vSystemCallExit

#else /* #if ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

vPortSVCHandler:
    #ifndef USE_PROCESS_STACK
        tst lr, #4
        ite eq
        mrseq r0, msp
        mrsne r0, psp
    #else
        mrs r0, psp
    #endif
        b vPortSVCHandler_C

#endif /* #if ( configUSE_MPU_WRAPPERS_V1 == 0 ) */
/*-----------------------------------------------------------*/

vPortStartFirstTask:
    /* Use the NVIC offset register to locate the stack. */
    ldr r0, =0xE000ED08
    ldr r0, [r0]
    ldr r0, [r0]
    /* Set the msp back to the start of the stack. */
    msr msp, r0
    /* Clear the bit that indicates the FPU is in use in case the FPU was used
    before the scheduler was started - which would otherwise result in the
    unnecessary leaving of space in the SVC stack for lazy saving of FPU
    registers. */
    mov r0, #0
    msr control, r0
    /* Call SVC to start the first task. */
    cpsie i
    cpsie f
    dsb
    isb
    svc #portSVC_START_SCHEDULER

/*-----------------------------------------------------------*/

vPortRestoreContextOfFirstTask:
    ldr r0, =0xE000ED08                    /* Use the NVIC offset register to locate the stack. */
    ldr r0, [r0]
    ldr r0, [r0]
    msr msp, r0                            /* Set the msp back to the start of the stack. */

    /*------------ Program MPU. ------------ */
    ldr r3, =pxCurrentTCB
    ldr r2, [r3]                           /* r2 = pxCurrentTCB. */
    add r2, r2, #4                         /* r2 = Second item in the TCB which is xMPUSettings. */

    dmb                                    /* Complete outstanding transfers before disabling MPU. */
    ldr r0, =0xe000ed94                    /* MPU_CTRL register. */
    ldr r3, [r0]                           /* Read the value of MPU_CTRL. */
    bic r3, #1                             /* r3 = r3 & ~1 i.e. Clear the bit 0 in r3. */
    str r3, [r0]                           /* Disable MPU. */

    ldr r0, =0xe000ed9c                    /* Region Base Address register. */
    ldmia r2!, {r4-r11}                    /* Read 4 sets of MPU registers [MPU Region # 0 - 3]. */
    stmia r0, {r4-r11}                     /* Write 4 sets of MPU registers [MPU Region # 0 - 3]. */

#ifdef configTOTAL_MPU_REGIONS
    #if ( configTOTAL_MPU_REGIONS == 16 )
        ldmia r2!, {r4-r11}                /* Read 4 sets of MPU registers [MPU Region # 4 - 7]. */
        stmia r0, {r4-r11}                 /* Write 4 sets of MPU registers. [MPU Region # 4 - 7]. */
        ldmia r2!, {r4-r11}                /* Read 4 sets of MPU registers [MPU Region # 8 - 11]. */
        stmia r0, {r4-r11}                 /* Write 4 sets of MPU registers. [MPU Region # 8 - 11]. */
    #endif /* configTOTAL_MPU_REGIONS == 16. */
#endif

    ldr r0, =0xe000ed94                    /* MPU_CTRL register. */
    ldr r3, [r0]                           /* Read the value of MPU_CTRL. */
    orr r3, #1                             /* r3 = r3 | 1 i.e. Set the bit 0 in r3. */
    str r3, [r0]                           /* Enable MPU. */
    dsb                                    /* Force memory writes before continuing. */

    /*---------- Restore Context. ---------- */
    ldr r3, =pxCurrentTCB
    ldr r2, [r3]                           /* r2 = pxCurrentTCB. */
    ldr r1, [r2]                           /* r1 = Location of saved context in TCB. */

    ldmdb r1!, {r0, r4-r11}                /* r0 contains PSP after the hardware had saved context. r4-r11 contain hardware saved context. */
    msr psp, r0
    stmia r0, {r4-r11}                     /* Copy the hardware saved context on the task stack. */
    ldmdb r1!, {r3-r11, lr}                /* r3 contains CONTROL register. r4-r11 and LR restored. */
    msr control, r3
    str r1, [r2]                           /* Save the location where the context should be saved next as the first member of TCB. */

    mov r0, #0
    msr basepri, r0
    bx lr

/*-----------------------------------------------------------*/

vPortEnableVFP:
    /* The FPU enable bits are in the CPACR. */
    ldr.w r0, =0xE000ED88
    ldr r1, [r0]

    /* Enable CP10 and CP11 coprocessors, then save back. */
    orr r1, r1, #( 0xf << 20 )
    str r1, [r0]
    bx  r14

/*-----------------------------------------------------------*/

xIsPrivileged:
    mrs r0, control     /* r0 = CONTROL. */
    tst r0, #1          /* Perform r0 & 1 (bitwise AND) and update the conditions flag. */
    ite ne
    movne r0, #0        /* CONTROL[0]!=0. Return false to indicate that the processor is not privileged. */
    moveq r0, #1        /* CONTROL[0]==0. Return true to indicate that the processor is privileged. */
    bx lr               /* Return. */
/*-----------------------------------------------------------*/

vResetPrivilege:
    mrs r0, control     /* r0 = CONTROL. */
    orr r0, r0, #1      /* r0 = r0 | 1. */
    msr control, r0     /* CONTROL = r0. */
    bx lr               /* Return to the caller. */
/*-----------------------------------------------------------*/

    END
