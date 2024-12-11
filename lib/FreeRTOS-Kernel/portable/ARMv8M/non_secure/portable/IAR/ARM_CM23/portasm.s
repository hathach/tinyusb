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


    EXTERN pxCurrentTCB
    EXTERN xSecureContext
    EXTERN vTaskSwitchContext
    EXTERN vPortSVCHandler_C
    EXTERN SecureContext_SaveContext
    EXTERN SecureContext_LoadContext
#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )
    EXTERN vSystemCallEnter
    EXTERN vSystemCallExit
#endif

    PUBLIC xIsPrivileged
    PUBLIC vResetPrivilege
    PUBLIC vPortAllocateSecureContext
    PUBLIC vRestoreContextOfFirstTask
    PUBLIC vRaisePrivilege
    PUBLIC vStartFirstTask
    PUBLIC ulSetInterruptMask
    PUBLIC vClearInterruptMask
    PUBLIC PendSV_Handler
    PUBLIC SVC_Handler
    PUBLIC vPortFreeSecureContext

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

vPortAllocateSecureContext:
    svc 100                                 /* Secure context is allocated in the supervisor call. portSVC_ALLOCATE_SECURE_CONTEXT = 100. */
    bx lr                                   /* Return. */
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
        ldr r3, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r1, [r3]                        /* r1 = pxCurrentTCB.*/
        ldr r2, [r1]                        /* r2 = Location of saved context in TCB. */

    restore_special_regs_first_task:
        subs r2, #20
        ldmia r2!, {r0, r3-r6}              /* r0 = xSecureContext, r3 = original PSP, r4 = PSPLIM, r5 = CONTROL, r6 = LR. */
        subs r2, #20
        msr psp, r3
        msr control, r5
        mov lr, r6
        ldr r4, =xSecureContext             /* Read the location of xSecureContext i.e. &( xSecureContext ). */
        str r0, [r4]                        /* Restore xSecureContext. */

    restore_general_regs_first_task:
        subs r2, #32
        ldmia r2!, {r4-r7}                  /* r4-r7 contain half of the hardware saved context. */
        stmia r3!, {r4-r7}                  /* Copy half of the the hardware saved context on the task stack. */
        ldmia r2!, {r4-r7}                  /* r4-r7 contain rest half of the hardware saved context. */
        stmia r3!, {r4-r7}                  /* Copy rest half of the the hardware saved context on the task stack. */
        subs r2, #48
        ldmia r2!, {r4-r7}                  /* Restore r8-r11. */
        mov r8, r4                          /* r8 = r4. */
        mov r9, r5                          /* r9 = r5. */
        mov r10, r6                         /* r10 = r6. */
        mov r11, r7                         /* r11 = r7. */
        subs r2, #32
        ldmia r2!, {r4-r7}                  /* Restore r4-r7. */
        subs r2, #16

    restore_context_done_first_task:
       str r2, [r1]                         /* Save the location where the context should be saved next as the first member of TCB. */
       bx lr

#else /* configENABLE_MPU */

vRestoreContextOfFirstTask:
    ldr  r2, =pxCurrentTCB                  /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
    ldr  r3, [r2]                           /* Read pxCurrentTCB. */
    ldr  r0, [r3]                           /* Read top of stack from TCB - The first item in pxCurrentTCB is the task top of stack. */

    ldm  r0!, {r1-r3}                       /* Read from stack - r1 = xSecureContext, r2 = PSPLIM and r3 = EXC_RETURN. */
    ldr  r4, =xSecureContext
    str  r1, [r4]                           /* Set xSecureContext to this task's value for the same. */
    movs r1, #2                             /* r1 = 2. */
    msr  CONTROL, r1                        /* Switch to use PSP in the thread mode. */
    adds r0, #32                            /* Discard everything up to r0. */
    msr  psp, r0                            /* This is now the new top of stack to use in the task. */
    isb
    bx   r3                                 /* Finally, branch to EXC_RETURN. */

#endif /* configENABLE_MPU */
/*-----------------------------------------------------------*/

vRaisePrivilege:
    mrs r0, control                         /* Read the CONTROL register. */
    movs r1, #1                             /* r1 = 1. */
    bics r0, r1                             /* Clear the bit 0. */
    msr control, r0                         /* Write back the new CONTROL value. */
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
    ldr r3, =xSecureContext                 /* Read the location of xSecureContext i.e. &( xSecureContext ). */
    ldr r0, [r3]                            /* Read xSecureContext - Value of xSecureContext must be in r0 as it is used as a parameter later. */
    ldr r3, =pxCurrentTCB                   /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
    ldr r1, [r3]                            /* Read pxCurrentTCB - Value of pxCurrentTCB must be in r1 as it is used as a parameter later.*/
    ldr r2, [r1]                            /* r2 = Location in TCB where the context should be saved. */

    cbz r0, save_ns_context                 /* No secure context to save. */
    save_s_context:
        push {r0-r2, lr}
        bl SecureContext_SaveContext        /* Params are in r0 and r1. r0 = xSecureContext and r1 = pxCurrentTCB. */
        pop {r0-r3}                         /* LR is now in r3. */
        mov lr, r3                          /* Restore LR. */

    save_ns_context:
        mov r3, lr                          /* r3 = LR (EXC_RETURN). */
        lsls r3, r3, #25                    /* r3 = r3 << 25. Bit[6] of EXC_RETURN is 1 if secure stack was used, 0 if non-secure stack was used to store stack frame. */
        bmi save_special_regs               /* r3 < 0 ==> Bit[6] in EXC_RETURN is 1 ==> secure stack was used to store the stack frame. */

    save_general_regs:
        mrs r3, psp
        stmia r2!, {r4-r7}                  /* Store r4-r7. */
        mov r4, r8                          /* r4 = r8. */
        mov r5, r9                          /* r5 = r9. */
        mov r6, r10                         /* r6 = r10. */
        mov r7, r11                         /* r7 = r11. */
        stmia r2!, {r4-r7}                  /* Store r8-r11. */
        ldmia r3!, {r4-r7}                  /* Copy half of the  hardware saved context into r4-r7. */
        stmia r2!, {r4-r7}                  /* Store the hardware saved context. */
        ldmia r3!, {r4-r7}                  /* Copy rest half of the  hardware saved context into r4-r7. */
        stmia r2!, {r4-r7}                  /* Store the hardware saved context. */

    save_special_regs:
        mrs r3, psp                         /* r3 = PSP. */
        movs r4, #0                         /* r4 = 0. 0 is stored in the PSPLIM slot. */
        mrs r5, control                     /* r5 = CONTROL. */
        mov r6, lr                          /* r6 = LR. */
        stmia r2!, {r0, r3-r6}              /* Store xSecureContext, original PSP (after hardware has saved context), PSPLIM, CONTROL and LR. */
        str r2, [r1]                        /* Save the location from where the context should be restored as the first member of TCB. */

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
        ldr r3, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r1, [r3]                        /* r1 = pxCurrentTCB.*/
        ldr r2, [r1]                        /* r2 = Location of saved context in TCB. */

    restore_special_regs:
        subs r2, #20
        ldmia r2!, {r0, r3-r6}              /* r0 = xSecureContext, r3 = original PSP, r4 = PSPLIM, r5 = CONTROL, r6 = LR. */
        subs r2, #20
        msr psp, r3
        msr control, r5
        mov lr, r6
        ldr r4, =xSecureContext             /* Read the location of xSecureContext i.e. &( xSecureContext ). */
        str r0, [r4]                        /* Restore xSecureContext. */
        cbz r0, restore_ns_context          /* No secure context to restore. */

    restore_s_context:
        push {r1-r3, lr}
        bl SecureContext_LoadContext        /* Params are in r0 and r1. r0 = xSecureContext and r1 = pxCurrentTCB. */
        pop {r1-r4}                         /* LR is now in r4. */
        mov lr, r4

    restore_ns_context:
        mov r0, lr                          /* r0 = LR (EXC_RETURN).  */
        lsls r0, r0, #25                    /* r0 = r0 << 25. Bit[6] of EXC_RETURN is 1 if secure stack was used, 0 if non-secure stack was used to store stack frame.  */
        bmi restore_context_done            /* r0 < 0 ==> Bit[6] in EXC_RETURN is 1 ==> secure stack was used to store the stack frame. */

    restore_general_regs:
        subs r2, #32
        ldmia r2!, {r4-r7}                  /* r4-r7 contain half of the hardware saved context. */
        stmia r3!, {r4-r7}                  /* Copy half of the the hardware saved context on the task stack. */
        ldmia r2!, {r4-r7}                  /* r4-r7 contain rest half of the hardware saved context. */
        stmia r3!, {r4-r7}                  /* Copy rest half of the the hardware saved context on the task stack. */
        subs r2, #48
        ldmia r2!, {r4-r7}                  /* Restore r8-r11. */
        mov r8, r4                          /* r8 = r4. */
        mov r9, r5                          /* r9 = r5. */
        mov r10, r6                         /* r10 = r6. */
        mov r11, r7                         /* r11 = r7. */
        subs r2, #32
        ldmia r2!, {r4-r7}                  /* Restore r4-r7. */
        subs r2, #16

    restore_context_done:
        str r2, [r1]                        /* Save the location where the context should be saved next as the first member of TCB.  */
        bx lr

#else /* configENABLE_MPU */

PendSV_Handler:
    ldr r3, =xSecureContext                 /* Read the location of xSecureContext i.e. &( xSecureContext ). */
    ldr r0, [r3]                            /* Read xSecureContext - Value of xSecureContext must be in r0 as it is used as a parameter later. */
    ldr r3, =pxCurrentTCB                   /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
    ldr r1, [r3]                            /* Read pxCurrentTCB - Value of pxCurrentTCB must be in r1 as it is used as a parameter later. */
    mrs r2, psp                             /* Read PSP in r2. */

    cbz r0, save_ns_context                 /* No secure context to save. */
    push {r0-r2, r14}
    bl SecureContext_SaveContext            /* Params are in r0 and r1. r0 = xSecureContext and r1 = pxCurrentTCB. */
    pop {r0-r3}                             /* LR is now in r3. */
    mov lr, r3                              /* LR = r3. */
    lsls r1, r3, #25                        /* r1 = r3 << 25. Bit[6] of EXC_RETURN is 1 if secure stack was used, 0 if non-secure stack was used to store stack frame. */
    bpl save_ns_context                     /* bpl - branch if positive or zero. If r1 >= 0 ==> Bit[6] in EXC_RETURN is 0 i.e. non-secure stack was used. */
    ldr r3, =pxCurrentTCB                   /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
    ldr r1, [r3]                            /* Read pxCurrentTCB. */

    subs r2, r2, #12                        /* Make space for xSecureContext, PSPLIM and LR on the stack. */
    str r2, [r1]                            /* Save the new top of stack in TCB. */
    movs r1, #0                             /* r1 = 0. 0 is stored in the PSPLIM slot. */
    mov r3, lr                              /* r3 = LR/EXC_RETURN. */
    stmia r2!, {r0, r1, r3}                 /* Store xSecureContext, PSPLIM and LR on the stack. */

    b select_next_task

    save_ns_context:
        ldr r3, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r1, [r3]                        /* Read pxCurrentTCB. */
        subs r2, r2, #44                    /* Make space for xSecureContext, PSPLIM, LR and the remaining registers on the stack. */
        str r2, [r1]                        /* Save the new top of stack in TCB. */
        movs r1, #0                         /* r1 = 0. 0 is stored in the PSPLIM slot. */
        mov r3, lr                          /* r3 = LR/EXC_RETURN. */
        stmia r2!, {r0, r1, r3-r7}          /* Store xSecureContext, PSPLIM, LR and the low registers that are not saved automatically. */
        mov r4, r8                          /* r4 = r8. */
        mov r5, r9                          /* r5 = r9. */
        mov r6, r10                         /* r6 = r10. */
        mov r7, r11                         /* r7 = r11. */
        stmia r2!, {r4-r7}                  /* Store the high registers that are not saved automatically. */

    select_next_task:
        cpsid i
        bl vTaskSwitchContext
        cpsie i

        ldr r3, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r1, [r3]                        /* Read pxCurrentTCB. */
        ldr r2, [r1]                        /* The first item in pxCurrentTCB is the task top of stack. r2 now points to the top of stack. */

        ldmia r2!, {r0, r1, r4}             /* Read from stack - r0 = xSecureContext, r1 = PSPLIM and r4 = LR. */
        mov lr, r4                          /* LR = r4. */
        ldr r3, =xSecureContext             /* Read the location of xSecureContext i.e. &( xSecureContext ). */
        str r0, [r3]                        /* Restore the task's xSecureContext. */
        cbz r0, restore_ns_context          /* If there is no secure context for the task, restore the non-secure context. */
        ldr r3, =pxCurrentTCB               /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
        ldr r1, [r3]                        /* Read pxCurrentTCB. */
        push {r2, r4}
        bl SecureContext_LoadContext        /* Restore the secure context. Params are in r0 and r1. r0 = xSecureContext and r1 = pxCurrentTCB. */
        pop {r2, r4}
        mov lr, r4                          /* LR = r4. */
        lsls r1, r4, #25                    /* r1 = r4 << 25. Bit[6] of EXC_RETURN is 1 if secure stack was used, 0 if non-secure stack was used to store stack frame. */
        bpl restore_ns_context              /* bpl - branch if positive or zero. If r1 >= 0 ==> Bit[6] in EXC_RETURN is 0 i.e. non-secure stack was used. */
        msr psp, r2                         /* Remember the new top of stack for the task. */
        bx lr

    restore_ns_context:
        adds r2, r2, #16                    /* Move to the high registers. */
        ldmia r2!, {r4-r7}                  /* Restore the high registers that are not automatically restored. */
        mov r8, r4                          /* r8 = r4. */
        mov r9, r5                          /* r9 = r5. */
        mov r10, r6                         /* r10 = r6. */
        mov r11, r7                         /* r11 = r7. */
        msr psp, r2                         /* Remember the new top of stack for the task. */
        subs r2, r2, #32                    /* Go back to the low registers. */
        ldmia r2!, {r4-r7}                  /* Restore the low registers that are not automatically restored. */
        bx lr

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

vPortFreeSecureContext:
    ldr r2, [r0]                            /* The first item in the TCB is the top of the stack. */
    ldr r1, [r2]                            /* The first item on the stack is the task's xSecureContext. */
    cmp r1, #0                              /* Raise svc if task's xSecureContext is not NULL. */
    bne free_secure_context                 /* Branch if r1 != 0. */
    bx lr                                   /* There is no secure context (xSecureContext is NULL). */
    free_secure_context:
        svc 101                             /* Secure context is freed in the supervisor call. portSVC_FREE_SECURE_CONTEXT = 101. */
        bx lr                               /* Return. */
/*-----------------------------------------------------------*/

    END
