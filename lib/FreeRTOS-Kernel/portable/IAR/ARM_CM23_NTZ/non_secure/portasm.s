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
#include "FreeRTOSConfig.h"

/* System call numbers includes. */
#include "mpu_syscall_numbers.h"

#ifndef configUSE_MPU_WRAPPERS_V1
    #define configUSE_MPU_WRAPPERS_V1 0
#endif

#ifndef configRUN_FREERTOS_SECURE_ONLY
    #define configRUN_FREERTOS_SECURE_ONLY 0
#endif

    EXTERN pxCurrentTCB
    EXTERN vTaskSwitchContext
    EXTERN vPortSVCHandler_C
#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )
    EXTERN vSystemCallEnter
    EXTERN vSystemCallExit
#endif

    PUBLIC xIsPrivileged
    PUBLIC vResetPrivilege
    PUBLIC vRestoreContextOfFirstTask
    PUBLIC vRaisePrivilege
    PUBLIC vStartFirstTask
    PUBLIC ulSetInterruptMask
    PUBLIC vClearInterruptMask
    PUBLIC PendSV_Handler
    PUBLIC SVC_Handler

#if ( configENABLE_FPU == 1 )
    #error Cortex-M23 does not have a Floating Point Unit (FPU) and therefore configENABLE_FPU must be set to 0.
#endif
/*-----------------------------------------------------------*/

/*---------------- Unprivileged Functions -------------------*/

/*-----------------------------------------------------------*/

    SECTION .text:CODE:NOROOT(2)
    THUMB
/*-----------------------------------------------------------*/

xIsPrivileged:
    mrs r0, control                         /* r0 = CONTROL. */
    movs r1, #1                             /* r1 = 1. */
    tst r0, r1                              /* Perform r0 & r1 (bitwise AND) and update the conditions flag. */
    beq running_privileged                  /* If the result of previous AND operation was 0, branch. */
    movs r0, #0                             /* CONTROL[0]!=0. Return false to indicate that the processor is not privileged. */
    bx lr                                   /* Return. */
    running_privileged:
        movs r0, #1                         /* CONTROL[0]==0. Return true to indicate that the processor is privileged. */
        bx lr                               /* Return. */

/*-----------------------------------------------------------*/

vResetPrivilege:
    mrs r0, control                         /* r0 = CONTROL. */
    movs r1, #1                             /* r1 = 1. */
    orrs r0, r1                             /* r0 = r0 | r1. */
    msr control, r0                         /* CONTROL = r0. */
    bx lr                                   /* Return to the caller. */
/*-----------------------------------------------------------*/

/*----------------- Privileged Functions --------------------*/

/*-----------------------------------------------------------*/

    SECTION privileged_functions:CODE:NOROOT(2)
    THUMB
/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

vRestoreContextOfFirstTask:
    program_mpu_first_task:
        ldr r3, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r0, [r3]                        /* r0 = pxCurrentTCB.*/

        dmb                                 /* Complete outstanding transfers before disabling MPU. */
        ldr r1, =0xe000ed94                 /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
        ldr r2, [r1]                        /* Read the value of MPU_CTRL. */
        movs r3, #1                         /* r3 = 1. */
        bics r2, r3                         /* r2 = r2 & ~r3 i.e. Clear the bit 0 in r2. */
        str r2, [r1]                        /* Disable MPU. */

        adds r0, #4                         /* r0 = r0 + 4. r0 now points to MAIR0 in TCB. */
        ldr r1, [r0]                        /* r1 = *r0 i.e. r1 = MAIR0. */
        ldr r2, =0xe000edc0                 /* r2 = 0xe000edc0 [Location of MAIR0]. */
        str r1, [r2]                        /* Program MAIR0. */

        adds r0, #4                         /* r0 = r0 + 4. r0 now points to first RBAR in TCB. */
        ldr r1, =0xe000ed98                 /* r1 = 0xe000ed98 [Location of RNR]. */

        movs r3, #4                         /* r3 = 4. */
        str r3, [r1]                        /* Program RNR = 4. */
        ldmia r0!, {r4-r5}                  /* Read first set of RBAR/RLAR registers from TCB. */
        ldr r2, =0xe000ed9c                 /* r2 = 0xe000ed9c [Location of RBAR]. */
        stmia r2!, {r4-r5}                  /* Write first set of RBAR/RLAR registers. */
        movs r3, #5                         /* r3 = 5. */
        str r3, [r1]                        /* Program RNR = 5. */
        ldmia r0!, {r4-r5}                  /* Read second set of RBAR/RLAR registers from TCB. */
        ldr r2, =0xe000ed9c                 /* r2 = 0xe000ed9c [Location of RBAR]. */
        stmia r2!, {r4-r5}                  /* Write second set of RBAR/RLAR registers. */
        movs r3, #6                         /* r3 = 6. */
        str r3, [r1]                        /* Program RNR = 6. */
        ldmia r0!, {r4-r5}                  /* Read third set of RBAR/RLAR registers from TCB. */
        ldr r2, =0xe000ed9c                 /* r2 = 0xe000ed9c [Location of RBAR]. */
        stmia r2!, {r4-r5}                  /* Write third set of RBAR/RLAR registers. */
        movs r3, #7                         /* r3 = 6. */
        str r3, [r1]                        /* Program RNR = 7. */
        ldmia r0!, {r4-r5}                  /* Read fourth set of RBAR/RLAR registers from TCB. */
        ldr r2, =0xe000ed9c                 /* r2 = 0xe000ed9c [Location of RBAR]. */
        stmia r2!, {r4-r5}                  /* Write fourth set of RBAR/RLAR registers. */

        ldr r1, =0xe000ed94                 /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
        ldr r2, [r1]                        /* Read the value of MPU_CTRL. */
        movs r3, #1                         /* r3 = 1. */
        orrs r2, r3                         /* r2 = r2 | r3 i.e. Set the bit 0 in r2. */
        str r2, [r1]                        /* Enable MPU. */
        dsb                                 /* Force memory writes before continuing. */

    restore_context_first_task:
        ldr r2, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r0, [r2]                        /* r0 = pxCurrentTCB.*/
        ldr r1, [r0]                        /* r1 = Location of saved context in TCB. */

    restore_special_regs_first_task:
        subs r1, #16
        ldmia r1!, {r2-r5}                  /* r2 = original PSP, r3 = PSPLIM, r4 = CONTROL, r5 = LR. */
        subs r1, #16
        msr psp, r2
    #if ( configRUN_FREERTOS_SECURE_ONLY == 1 )
        msr psplim, r3
    #endif
        msr control, r4
        mov lr, r5

    restore_general_regs_first_task:
        subs r1, #32
        ldmia r1!, {r4-r7}                  /* r4-r7 contain half of the hardware saved context. */
        stmia r2!, {r4-r7}                  /* Copy half of the the hardware saved context on the task stack. */
        ldmia r1!, {r4-r7}                  /* r4-r7 contain rest half of the hardware saved context. */
        stmia r2!, {r4-r7}                  /* Copy rest half of the the hardware saved context on the task stack. */
        subs r1, #48
        ldmia r1!, {r4-r7}                  /* Restore r8-r11. */
        mov r8, r4                          /* r8 = r4. */
        mov r9, r5                          /* r9 = r5. */
        mov r10, r6                         /* r10 = r6. */
        mov r11, r7                         /* r11 = r7. */
        subs r1, #32
        ldmia r1!, {r4-r7}                  /* Restore r4-r7. */
        subs r1, #16

    restore_context_done_first_task:
        str r1, [r0]                        /* Save the location where the context should be saved next as the first member of TCB. */
        bx lr

#else /* configENABLE_MPU */

vRestoreContextOfFirstTask:
    ldr  r2, =pxCurrentTCB                  /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
    ldr  r1, [r2]                           /* Read pxCurrentTCB. */
    ldr  r0, [r1]                           /* Read top of stack from TCB - The first item in pxCurrentTCB is the task top of stack. */

    ldm  r0!, {r1-r2}                       /* Read from stack - r1 = PSPLIM and r2 = EXC_RETURN. */
#if ( configRUN_FREERTOS_SECURE_ONLY == 1 )
    msr  psplim, r1                         /* Set this task's PSPLIM value. */
#endif
    movs r1, #2                             /* r1 = 2. */
    msr  CONTROL, r1                        /* Switch to use PSP in the thread mode. */
    adds r0, #32                            /* Discard everything up to r0. */
    msr  psp, r0                            /* This is now the new top of stack to use in the task. */
    isb
    bx   r2                                 /* Finally, branch to EXC_RETURN. */

#endif /* configENABLE_MPU */
/*-----------------------------------------------------------*/

vRaisePrivilege:
    mrs  r0, control                        /* Read the CONTROL register. */
    movs r1, #1                             /* r1 = 1. */
    bics r0, r1                             /* Clear the bit 0. */
    msr  control, r0                        /* Write back the new CONTROL value. */
    bx lr                                   /* Return to the caller. */
/*-----------------------------------------------------------*/

vStartFirstTask:
    ldr r0, =0xe000ed08                     /* Use the NVIC offset register to locate the stack. */
    ldr r0, [r0]                            /* Read the VTOR register which gives the address of vector table. */
    ldr r0, [r0]                            /* The first entry in vector table is stack pointer. */
    msr msp, r0                             /* Set the MSP back to the start of the stack. */
    cpsie i                                 /* Globally enable interrupts. */
    dsb
    isb
    svc 102                                 /* System call to start the first task. portSVC_START_SCHEDULER = 102. */
    nop
/*-----------------------------------------------------------*/

ulSetInterruptMask:
    mrs r0, PRIMASK
    cpsid i
    bx lr
/*-----------------------------------------------------------*/

vClearInterruptMask:
    msr PRIMASK, r0
    bx lr
/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

PendSV_Handler:
    ldr r2, =pxCurrentTCB                   /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
    ldr r0, [r2]                            /* r0 = pxCurrentTCB. */
    ldr r1, [r0]                            /* r1 = Location in TCB where the context should be saved. */
    mrs r2, psp                             /* r2 = PSP. */

    save_general_regs:
        stmia r1!, {r4-r7}                  /* Store r4-r7. */
        mov r4, r8                          /* r4 = r8. */
        mov r5, r9                          /* r5 = r9. */
        mov r6, r10                         /* r6 = r10. */
        mov r7, r11                         /* r7 = r11. */
        stmia r1!, {r4-r7}                  /* Store r8-r11. */
        ldmia r2!, {r4-r7}                  /* Copy half of the  hardware saved context into r4-r7. */
        stmia r1!, {r4-r7}                  /* Store the hardware saved context. */
        ldmia r2!, {r4-r7}                  /* Copy rest half of the  hardware saved context into r4-r7. */
        stmia r1!, {r4-r7}                  /* Store the hardware saved context. */

    save_special_regs:
        mrs r2, psp                         /* r2 = PSP. */
    #if ( configRUN_FREERTOS_SECURE_ONLY == 1 )
        mrs r3, psplim                      /* r3 = PSPLIM. */
    #else
        movs r3, #0                         /* r3 = 0. 0 is stored in the PSPLIM slot. */
    #endif
        mrs r4, control                     /* r4 = CONTROL. */
        mov r5, lr                          /* r5 = LR. */
        stmia r1!, {r2-r5}                  /* Store original PSP (after hardware has saved context), PSPLIM, CONTROL and LR. */
        str r1, [r0]                        /* Save the location from where the context should be restored as the first member of TCB. */

    select_next_task:
        cpsid i
        bl vTaskSwitchContext
        cpsie i

    program_mpu:
        ldr r3, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r0, [r3]                        /* r0 = pxCurrentTCB.*/

        dmb                                 /* Complete outstanding transfers before disabling MPU. */
        ldr r1, =0xe000ed94                 /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
        ldr r2, [r1]                        /* Read the value of MPU_CTRL. */
        movs r3, #1                         /* r3 = 1. */
        bics r2, r3                         /* r2 = r2 & ~r3 i.e. Clear the bit 0 in r2. */
        str r2, [r1]                        /* Disable MPU. */

        adds r0, #4                         /* r0 = r0 + 4. r0 now points to MAIR0 in TCB. */
        ldr r1, [r0]                        /* r1 = *r0 i.e. r1 = MAIR0. */
        ldr r2, =0xe000edc0                 /* r2 = 0xe000edc0 [Location of MAIR0]. */
        str r1, [r2]                        /* Program MAIR0. */

        adds r0, #4                         /* r0 = r0 + 4. r0 now points to first RBAR in TCB. */
        ldr r1, =0xe000ed98                 /* r1 = 0xe000ed98 [Location of RNR]. */

        movs r3, #4                         /* r3 = 4. */
        str r3, [r1]                        /* Program RNR = 4. */
        ldmia r0!, {r4-r5}                  /* Read first set of RBAR/RLAR registers from TCB. */
        ldr r2, =0xe000ed9c                 /* r2 = 0xe000ed9c [Location of RBAR]. */
        stmia r2!, {r4-r5}                  /* Write first set of RBAR/RLAR registers. */
        movs r3, #5                         /* r3 = 5. */
        str r3, [r1]                        /* Program RNR = 5. */
        ldmia r0!, {r4-r5}                  /* Read second set of RBAR/RLAR registers from TCB. */
        ldr r2, =0xe000ed9c                 /* r2 = 0xe000ed9c [Location of RBAR]. */
        stmia r2!, {r4-r5}                  /* Write second set of RBAR/RLAR registers. */
        movs r3, #6                         /* r3 = 6. */
        str r3, [r1]                        /* Program RNR = 6. */
        ldmia r0!, {r4-r5}                  /* Read third set of RBAR/RLAR registers from TCB. */
        ldr r2, =0xe000ed9c                 /* r2 = 0xe000ed9c [Location of RBAR]. */
        stmia r2!, {r4-r5}                  /* Write third set of RBAR/RLAR registers. */
        movs r3, #7                         /* r3 = 6. */
        str r3, [r1]                        /* Program RNR = 7. */
        ldmia r0!, {r4-r5}                  /* Read fourth set of RBAR/RLAR registers from TCB. */
        ldr r2, =0xe000ed9c                 /* r2 = 0xe000ed9c [Location of RBAR]. */
        stmia r2!, {r4-r5}                  /* Write fourth set of RBAR/RLAR registers. */

        ldr r1, =0xe000ed94                 /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
        ldr r2, [r1]                        /* Read the value of MPU_CTRL. */
        movs r3, #1                         /* r3 = 1. */
        orrs r2, r3                         /* r2 = r2 | r3 i.e. Set the bit 0 in r2. */
        str r2, [r1]                        /* Enable MPU. */
        dsb                                 /* Force memory writes before continuing. */

    restore_context:
        ldr r2, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r0, [r2]                        /* r0 = pxCurrentTCB.*/
        ldr r1, [r0]                        /* r1 = Location of saved context in TCB. */

    restore_special_regs:
        subs r1, #16
        ldmia r1!, {r2-r5}                  /* r2 = original PSP, r3 = PSPLIM, r4 = CONTROL, r5 = LR. */
        subs r1, #16
        msr psp, r2
    #if ( configRUN_FREERTOS_SECURE_ONLY == 1 )
        msr psplim, r3
    #endif
        msr control, r4
        mov lr, r5

    restore_general_regs:
        subs r1, #32
        ldmia r1!, {r4-r7}                  /* r4-r7 contain half of the hardware saved context. */
        stmia r2!, {r4-r7}                  /* Copy half of the the hardware saved context on the task stack. */
        ldmia r1!, {r4-r7}                  /* r4-r7 contain rest half of the hardware saved context. */
        stmia r2!, {r4-r7}                  /* Copy rest half of the the hardware saved context on the task stack. */
        subs r1, #48
        ldmia r1!, {r4-r7}                  /* Restore r8-r11. */
        mov r8, r4                          /* r8 = r4. */
        mov r9, r5                          /* r9 = r5. */
        mov r10, r6                         /* r10 = r6. */
        mov r11, r7                         /* r11 = r7. */
        subs r1, #32
        ldmia r1!, {r4-r7}                  /* Restore r4-r7. */
        subs r1, #16

    restore_context_done:
        str r1, [r0]                        /* Save the location where the context should be saved next as the first member of TCB. */
        bx lr

#else /* configENABLE_MPU */

PendSV_Handler:
    mrs r0, psp                             /* Read PSP in r0. */
    ldr r2, =pxCurrentTCB                   /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
    ldr r1, [r2]                            /* Read pxCurrentTCB. */

    subs r0, r0, #40                        /* Make space for PSPLIM, LR and the remaining registers on the stack. */
    str r0, [r1]                            /* Save the new top of stack in TCB. */
#if ( configRUN_FREERTOS_SECURE_ONLY == 1 )
    mrs r2, psplim                          /* r2 = PSPLIM. */
#else
    movs r2, #0                             /* r0 = 0. 0 is stored in the PSPLIM slot. */
#endif
    mov r3, lr                              /* r3 = LR/EXC_RETURN. */
    stmia r0!, {r2-r7}                      /* Store on the stack - PSPLIM, LR and low registers that are not automatically saved. */
    mov r4, r8                              /* r4 = r8. */
    mov r5, r9                              /* r5 = r9. */
    mov r6, r10                             /* r6 = r10. */
    mov r7, r11                             /* r7 = r11. */
    stmia r0!, {r4-r7}                      /* Store the high registers that are not saved automatically. */

    cpsid i
    bl vTaskSwitchContext
    cpsie i

    ldr r2, =pxCurrentTCB                   /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
    ldr r1, [r2]                            /* Read pxCurrentTCB. */
    ldr r0, [r1]                            /* The first item in pxCurrentTCB is the task top of stack. r0 now points to the top of stack. */

    adds r0, r0, #24                        /* Move to the high registers. */
    ldmia r0!, {r4-r7}                      /* Restore the high registers that are not automatically restored. */
    mov r8, r4                              /* r8 = r4. */
    mov r9, r5                              /* r9 = r5. */
    mov r10, r6                             /* r10 = r6. */
    mov r11, r7                             /* r11 = r7. */
    msr psp, r0                             /* Remember the new top of stack for the task. */
    subs r0, r0, #40                        /* Move to the starting of the saved context. */
    ldmia r0!, {r2-r7}                      /* Read from stack - r2 = PSPLIM, r3 = LR and r4-r7 restored. */
#if ( configRUN_FREERTOS_SECURE_ONLY == 1 )
    msr psplim, r2                          /* Restore the PSPLIM register value for the task. */
#endif
    bx r3

#endif /* configENABLE_MPU */
/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

SVC_Handler:
    movs r0, #4
    mov r1, lr
    tst r0, r1
    beq stack_on_msp
    stack_on_psp:
        mrs r0, psp
        b route_svc
    stack_on_msp:
        mrs r0, msp
        b route_svc

    route_svc:
        ldr r3, [r0, #24]
        subs r3, #2
        ldrb r2, [r3, #0]
        cmp r2, #NUM_SYSTEM_CALLS
        blt system_call_enter
        cmp r2, #104        /* portSVC_SYSTEM_CALL_EXIT. */
        beq system_call_exit
        b vPortSVCHandler_C

    system_call_enter:
        b vSystemCallEnter
    system_call_exit:
        b vSystemCallExit

#else /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

SVC_Handler:
    movs r0, #4
    mov r1, lr
    tst r0, r1
    beq stacking_used_msp
    mrs r0, psp
    b vPortSVCHandler_C
    stacking_used_msp:
        mrs r0, msp
        b vPortSVCHandler_C

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */
/*-----------------------------------------------------------*/

    END
