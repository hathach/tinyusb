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

/* Standard includes. */
#include <stdint.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE ensures that PRIVILEGED_FUNCTION
 * is defined correctly and privileged functions are placed in correct sections. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* Portasm includes. */
#include "portasm.h"

/* System call numbers includes. */
#include "mpu_syscall_numbers.h"

/* MPU_WRAPPERS_INCLUDED_FROM_API_FILE is needed to be defined only for the
 * header files. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if ( configENABLE_MPU == 1 )

    void vRestoreContextOfFirstTask( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            " .extern pxCurrentTCB                            \n"
            " .syntax unified                                 \n"
            "                                                 \n"
            " program_mpu_first_task:                         \n"
            "                                                 \n"
            "    ldr r3, =pxCurrentTCB                        \n" /* r3 = &pxCurrentTCB. */
            "    ldr r0, [r3]                                 \n" /* r0 = pxCurrentTCB.*/
            "    adds r0, #4                                  \n" /* r0 = Second item in the TCB which is xMPUSettings. */
            "                                                 \n"
            "    dmb                                          \n" /* Complete outstanding transfers before disabling MPU. */
            "    ldr r1, =0xe000ed94                          \n" /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
            "    ldr r2, [r1]                                 \n" /* Read the value of MPU_CTRL. */
            "    movs r3, #1                                  \n" /* r3 = 1. */
            "    bics r2, r3                                  \n" /* r2 = r2 & ~r3 i.e. Clear the bit 0 in r2. */
            "    str r2, [r1]                                 \n" /* Disable MPU. */
            "                                                 \n"
            "    ldr r1, =0xe000ed9c                          \n" /* r1 = 0xe000ed9c [Location of RBAR]. */
            "    ldr r2, =0xe000eda0                          \n" /* r2 = 0xe000eda0 [Location of RASR]. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read first set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read second set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read third set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read fourth set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read fifth set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldr r1, =0xe000ed94                          \n" /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
            "    ldr r2, [r1]                                 \n" /* Read the value of MPU_CTRL. */
            "    movs r3, #1                                  \n" /* r3 = 1. */
            "    orrs r2, r3                                  \n" /* r2 = r2 | r3 i.e. Set the bit 0 in r2. */
            "    str r2, [r1]                                 \n" /* Enable MPU. */
            "    dsb                                          \n" /* Force memory writes before continuing. */
            "                                                 \n"
            " restore_context_first_task:                     \n"
            "    ldr r2, =pxCurrentTCB                        \n" /* r2 = &pxCurrentTCB. */
            "    ldr r0, [r2]                                 \n" /* r0 = pxCurrentTCB.*/
            "    ldr r1, [r0]                                 \n" /* r1 = Location of saved context in TCB. */
            "                                                 \n"
            " restore_special_regs_first_task:                \n"
            "    subs r1, #12                                 \n"
            "    ldmia r1!, {r2-r4}                           \n" /* r2 = original PSP, r3 = CONTROL, r4 = LR. */
            "    subs r1, #12                                 \n"
            "    msr psp, r2                                  \n"
            "    msr control, r3                              \n"
            "    mov lr, r4                                   \n"
            "                                                 \n"
            " restore_general_regs_first_task:                \n"
            "    subs r1, #32                                 \n"
            "    ldmia r1!, {r4-r7}                           \n" /* r4-r7 contain half of the hardware saved context. */
            "    stmia r2!, {r4-r7}                           \n" /* Copy half of the the hardware saved context on the task stack. */
            "    ldmia r1!, {r4-r7}                           \n" /* r4-r7 contain rest half of the hardware saved context. */
            "    stmia r2!, {r4-r7}                           \n" /* Copy rest half of the the hardware saved context on the task stack. */
            "    subs r1, #48                                 \n"
            "    ldmia r1!, {r4-r7}                           \n" /* Restore r8-r11. */
            "    mov r8, r4                                   \n" /* r8 = r4. */
            "    mov r9, r5                                   \n" /* r9 = r5. */
            "    mov r10, r6                                  \n" /* r10 = r6. */
            "    mov r11, r7                                  \n" /* r11 = r7. */
            "    subs r1, #32                                 \n"
            "    ldmia r1!, {r4-r7}                           \n" /* Restore r4-r7. */
            "    subs r1, #16                                 \n"
            "                                                 \n"
            " restore_context_done_first_task:                \n"
            "    str r1, [r0]                                 \n" /* Save the location where the context should be saved next as the first member of TCB. */
            "    bx lr                                        \n"
            "                                                 \n"
            " .align 4                                        \n"
            ::"i" ( portSVC_START_SCHEDULER ) : "memory"
        );
    }

#else /* configENABLE_MPU */

    void vRestoreContextOfFirstTask( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            " .extern pxCurrentTCB                            \n"
            " .syntax unified                                 \n"
            "                                                 \n"
            " ldr  r2, =pxCurrentTCB                          \n" /* r2 = &pxCurrentTCB. */
            " ldr  r1, [r2]                                   \n" /* r1 = pxCurrentTCB.*/
            " ldr  r0, [r1]                                   \n" /* Read top of stack from TCB - The first item in pxCurrentTCB is the task top of stack. */
            "                                                 \n"
            " ldm  r0!, {r2}                                  \n" /* Read from stack - r2 = EXC_RETURN. */
            " movs r1, #2                                     \n" /* r1 = 2. */
            " msr  CONTROL, r1                                \n" /* Switch to use PSP in the thread mode. */
            " adds r0, #32                                    \n" /* Discard everything up to r0. */
            " msr  psp, r0                                    \n" /* This is now the new top of stack to use in the task. */
            " isb                                             \n"
            " bx   r2                                         \n" /* Finally, branch to EXC_RETURN. */
            "                                                 \n"
            " .align 4                                        \n"
        );
    }

#endif /* configENABLE_MPU */

/*-----------------------------------------------------------*/

BaseType_t xIsPrivileged( void ) /* __attribute__ (( naked )) */
{
    __asm volatile
    (
        " .syntax unified                                 \n"
        "                                                 \n"
        " mrs r0, control                                 \n" /* r0 = CONTROL. */
        " movs r1, #1                                     \n" /* r1 = 1. */
        " tst r0, r1                                      \n" /* Perform r0 & r1 (bitwise AND) and update the conditions flag. */
        " beq running_privileged                          \n" /* If the result of previous AND operation was 0, branch. */
        " movs r0, #0                                     \n" /* CONTROL[0]!=0. Return false to indicate that the processor is not privileged. */
        " bx lr                                           \n" /* Return. */
        " running_privileged:                             \n"
        "    movs r0, #1                                  \n" /* CONTROL[0]==0. Return true to indicate that the processor is privileged. */
        "    bx lr                                        \n" /* Return. */
        "                                                 \n"
        " .align 4                                        \n"
        ::: "r0", "r1", "memory"
    );
}

/*-----------------------------------------------------------*/

void vRaisePrivilege( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
{
    __asm volatile
    (
        " .syntax unified                                 \n"
        "                                                 \n"
        " mrs  r0, control                                \n" /* Read the CONTROL register. */
        " movs r1, #1                                     \n" /* r1 = 1. */
        " bics r0, r1                                     \n" /* Clear the bit 0. */
        " msr  control, r0                                \n" /* Write back the new CONTROL value. */
        " bx lr                                           \n" /* Return to the caller. */
        ::: "r0", "r1", "memory"
    );
}

/*-----------------------------------------------------------*/

void vResetPrivilege( void ) /* __attribute__ (( naked )) */
{
    __asm volatile
    (
        " .syntax unified                                 \n"
        "                                                 \n"
        " mrs r0, control                                 \n" /* r0 = CONTROL. */
        " movs r1, #1                                     \n" /* r1 = 1. */
        " orrs r0, r1                                     \n" /* r0 = r0 | r1. */
        " msr control, r0                                 \n" /* CONTROL = r0. */
        " bx lr                                           \n" /* Return to the caller. */
        ::: "r0", "r1", "memory"
    );
}

/*-----------------------------------------------------------*/

void vStartFirstTask( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
{
    /* Don't reset the MSP stack as is done on CM3/4 devices. The reason is that
     * the Vector Table Offset Register (VTOR) is optional in CM0+ architecture
     * and therefore, may not be available on all the devices. */
    __asm volatile
    (
        " .syntax unified                                 \n"
        " cpsie i                                         \n" /* Globally enable interrupts. */
        " dsb                                             \n"
        " isb                                             \n"
        " svc %0                                          \n" /* System call to start the first task. */
        " nop                                             \n"
        "                                                 \n"
        " .align 4                                        \n"
        ::"i" ( portSVC_START_SCHEDULER ) : "memory"
    );
}

/*-----------------------------------------------------------*/

uint32_t ulSetInterruptMask( void ) /* __attribute__(( naked )) PRIVILEGED_FUNCTION */
{
    __asm volatile
    (
        " .syntax unified                                 \n"
        "                                                 \n"
        " mrs r0, PRIMASK                                 \n"
        " cpsid i                                         \n"
        " bx lr                                           \n"
        ::: "memory"
    );
}

/*-----------------------------------------------------------*/

void vClearInterruptMask( __attribute__( ( unused ) ) uint32_t ulMask ) /* __attribute__(( naked )) PRIVILEGED_FUNCTION */
{
    __asm volatile
    (
        " .syntax unified                                 \n"
        "                                                 \n"
        " msr PRIMASK, r0                                 \n"
        " bx lr                                           \n"
        ::: "memory"
    );
}

/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

    void PendSV_Handler( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            " .extern pxCurrentTCB                            \n"
            " .syntax unified                                 \n"
            "                                                 \n"
            " ldr r2, =pxCurrentTCB                           \n" /* r2 = &( pxCurrentTCB ). */
            " ldr r0, [r2]                                    \n" /* r0 = pxCurrentTCB. */
            " ldr r1, [r0]                                    \n" /* r1 = Location in TCB where the context should be saved. */
            " mrs r2, psp                                     \n" /* r2 = PSP. */
            "                                                 \n"
            " save_general_regs:                              \n"
            "    stmia r1!, {r4-r7}                           \n" /* Store r4-r7. */
            "    mov r4, r8                                   \n" /* r4 = r8. */
            "    mov r5, r9                                   \n" /* r5 = r9. */
            "    mov r6, r10                                  \n" /* r6 = r10. */
            "    mov r7, r11                                  \n" /* r7 = r11. */
            "    stmia r1!, {r4-r7}                           \n" /* Store r8-r11. */
            "    ldmia r2!, {r4-r7}                           \n" /* Copy half of the  hardware saved context into r4-r7. */
            "    stmia r1!, {r4-r7}                           \n" /* Store the hardware saved context. */
            "    ldmia r2!, {r4-r7}                           \n" /* Copy rest half of the  hardware saved context into r4-r7. */
            "    stmia r1!, {r4-r7}                           \n" /* Store the hardware saved context. */
            "                                                 \n"
            " save_special_regs:                              \n"
            "    mrs r2, psp                                  \n" /* r2 = PSP. */
            "    mrs r3, control                              \n" /* r3 = CONTROL. */
            "    mov r4, lr                                   \n" /* r4 = LR. */
            "    stmia r1!, {r2-r4}                           \n" /* Store original PSP (after hardware has saved context), CONTROL and LR. */
            "    str r1, [r0]                                 \n" /* Save the location from where the context should be restored as the first member of TCB. */
            "                                                 \n"
            " select_next_task:                               \n"
            "    cpsid i                                      \n"
            "    bl vTaskSwitchContext                        \n"
            "    cpsie i                                      \n"
            "                                                 \n"
            " program_mpu:                                    \n"
            "                                                 \n"
            "    ldr r2, =pxCurrentTCB                        \n" /* r2 = &( pxCurrentTCB ). */
            "    ldr r0, [r2]                                 \n" /* r0 = pxCurrentTCB. */
            "    adds r0, #4                                  \n" /* r0 = Second item in the TCB which is xMPUSettings. */
            "                                                 \n"
            "    dmb                                          \n" /* Complete outstanding transfers before disabling MPU. */
            "    ldr r1, =0xe000ed94                          \n" /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
            "    ldr r2, [r1]                                 \n" /* Read the value of MPU_CTRL. */
            "    movs r3, #1                                  \n" /* r3 = 1. */
            "    bics r2, r3                                  \n" /* r2 = r2 & ~r3 i.e. Clear the bit 0 in r2. */
            "    str r2, [r1]                                 \n" /* Disable MPU */
            "                                                 \n"
            "    ldr r1, =0xe000ed9c                          \n" /* r1 = 0xe000ed9c [Location of RBAR]. */
            "    ldr r2, =0xe000eda0                          \n" /* r2 = 0xe000eda0 [Location of RASR]. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read first set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read second set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read third set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read fourth set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldmia r0!, {r3-r4}                           \n" /* Read fifth set of RBAR/RASR registers from TCB. */
            "    str r3, [r1]                                 \n" /* Program RBAR. */
            "    str r4, [r2]                                 \n" /* Program RASR. */
            "                                                 \n"
            "    ldr r1, =0xe000ed94                          \n" /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
            "    ldr r2, [r1]                                 \n" /* Read the value of MPU_CTRL. */
            "    movs r3, #1                                  \n" /* r3 = 1. */
            "    orrs r2, r3                                  \n" /* r2 = r2 | r3 i.e. Set the bit 0 in r2. */
            "    str r2, [r1]                                 \n" /* Enable MPU. */
            "    dsb                                          \n" /* Force memory writes before continuing. */
            "                                                 \n"
            " restore_context:                                \n"
            "    ldr r2, =pxCurrentTCB                        \n" /* r2 = &pxCurrentTCB. */
            "    ldr r0, [r2]                                 \n" /* r0 = pxCurrentTCB.*/
            "    ldr r1, [r0]                                 \n" /* r1 = Location of saved context in TCB. */
            "                                                 \n"
            " restore_special_regs:                           \n"
            "    subs r1, #12                                 \n"
            "    ldmia r1!, {r2-r4}                           \n" /* r2 = original PSP, r3 = CONTROL, r4 = LR. */
            "    subs r1, #12                                 \n"
            "    msr psp, r2                                  \n"
            "    msr control, r3                              \n"
            "    mov lr, r4                                   \n"
            "                                                 \n"
            " restore_general_regs:                           \n"
            "    subs r1, #32                                 \n"
            "    ldmia r1!, {r4-r7}                           \n" /* r4-r7 contain half of the hardware saved context. */
            "    stmia r2!, {r4-r7}                           \n" /* Copy half of the the hardware saved context on the task stack. */
            "    ldmia r1!, {r4-r7}                           \n" /* r4-r7 contain rest half of the hardware saved context. */
            "    stmia r2!, {r4-r7}                           \n" /* Copy rest half of the the hardware saved context on the task stack. */
            "    subs r1, #48                                 \n"
            "    ldmia r1!, {r4-r7}                           \n" /* Restore r8-r11. */
            "    mov r8, r4                                   \n" /* r8 = r4. */
            "    mov r9, r5                                   \n" /* r9 = r5. */
            "    mov r10, r6                                  \n" /* r10 = r6. */
            "    mov r11, r7                                  \n" /* r11 = r7. */
            "    subs r1, #32                                 \n"
            "    ldmia r1!, {r4-r7}                           \n" /* Restore r4-r7. */
            "    subs r1, #16                                 \n"
            "                                                 \n"
            " restore_context_done:                           \n"
            "    str r1, [r0]                                 \n" /* Save the location where the context should be saved next as the first member of TCB. */
            "    bx lr                                        \n"
            "                                                 \n"
            " .align 4                                        \n"
        );
    }

#else /* configENABLE_MPU */

    void PendSV_Handler( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            " .extern pxCurrentTCB                            \n"
            " .syntax unified                                 \n"
            "                                                 \n"
            " mrs r0, psp                                     \n" /* Read PSP in r0. */
            " ldr r2, =pxCurrentTCB                           \n" /* r2 = &( pxCurrentTCB ). */
            " ldr r1, [r2]                                    \n" /* r1 = pxCurrentTCB. */
            " subs r0, r0, #36                                \n" /* Make space for LR and the remaining registers on the stack. */
            " str r0, [r1]                                    \n" /* Save the new top of stack in TCB. */
            "                                                 \n"
            " mov r3, lr                                      \n" /* r3 = LR/EXC_RETURN. */
            " stmia r0!, {r3-r7}                              \n" /* Store on the stack - LR and low registers that are not automatically saved. */
            " mov r4, r8                                      \n" /* r4 = r8. */
            " mov r5, r9                                      \n" /* r5 = r9. */
            " mov r6, r10                                     \n" /* r6 = r10. */
            " mov r7, r11                                     \n" /* r7 = r11. */
            " stmia r0!, {r4-r7}                              \n" /* Store the high registers that are not saved automatically. */
            "                                                 \n"
            " cpsid i                                         \n"
            " bl vTaskSwitchContext                           \n"
            " cpsie i                                         \n"
            "                                                 \n"
            " ldr r2, =pxCurrentTCB                           \n" /* r2 = &( pxCurrentTCB ). */
            " ldr r1, [r2]                                    \n" /* r1 = pxCurrentTCB. */
            " ldr r0, [r1]                                    \n" /* The first item in pxCurrentTCB is the task top of stack. r0 now points to the top of stack. */
            "                                                 \n"
            " adds r0, r0, #20                                \n" /* Move to the high registers. */
            " ldmia r0!, {r4-r7}                              \n" /* Restore the high registers that are not automatically restored. */
            " mov r8, r4                                      \n" /* r8 = r4. */
            " mov r9, r5                                      \n" /* r9 = r5. */
            " mov r10, r6                                     \n" /* r10 = r6. */
            " mov r11, r7                                     \n" /* r11 = r7. */
            " msr psp, r0                                     \n" /* Remember the new top of stack for the task. */
            " subs r0, r0, #36                                \n" /* Move to the starting of the saved context. */
            " ldmia r0!, {r3-r7}                              \n" /* Read from stack - r3 = LR and r4-r7 restored. */
            " bx r3                                           \n"
            "                                                 \n"
            " .align 4                                        \n"
        );
    }

#endif /* configENABLE_MPU */

/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    void SVC_Handler( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            " .syntax unified                \n"
            " .extern vPortSVCHandler_C      \n"
            " .extern vSystemCallEnter       \n"
            " .extern vSystemCallExit        \n"
            " .extern pxCurrentTCB           \n"
            "                                \n"
            " movs r0, #4                    \n"
            " mov r1, lr                     \n"
            " tst r0, r1                     \n"
            " beq stack_on_msp               \n"
            "                                \n"
            " stack_on_psp:                  \n"
            "     mrs r0, psp                \n"
            "     b route_svc                \n"
            "                                \n"
            " stack_on_msp:                  \n"
            "     mrs r0, msp                \n"
            "     b route_svc                \n"
            "                                \n"
            " route_svc:                     \n"
            "     ldr r3, [r0, #24]          \n"
            "     subs r3, #2                \n"
            "     ldrb r2, [r3, #0]          \n"
            "     ldr r3, =%0                \n"
            "     cmp r2, r3                 \n"
            "     blt system_call_enter      \n"
            "     ldr r3, =%1                \n"
            "     cmp r2, r3                 \n"
            "     beq system_call_exit       \n"
            "     ldr r3, =vPortSVCHandler_C \n"
            "     bx r3                      \n"
            "                                \n"
            " system_call_enter:             \n"
            "    push {lr}                   \n"
            "    bl vSystemCallEnter         \n"
            "    pop {pc}                    \n"
            "                                \n"
            " system_call_exit:              \n"
            "    push {lr}                   \n"
            "    bl vSystemCallExit          \n"
            "    pop {pc}                    \n"
            "                                \n"
            " .align 4                       \n"
            "                                \n"
            : /* No outputs. */
            : "i" ( NUM_SYSTEM_CALLS ), "i" ( portSVC_SYSTEM_CALL_EXIT )
            : "r0", "r1", "r2", "r3", "memory"
        );
    }

#else /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

    void SVC_Handler( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            " .syntax unified                \n"
            " .extern vPortSVCHandler_C      \n"
            "                                \n"
            " movs r0, #4                    \n"
            " mov r1, lr                     \n"
            " tst r0, r1                     \n"
            " beq stacking_used_msp          \n"
            "                                \n"
            " stacking_used_psp:             \n"
            "    mrs r0, psp                 \n"
            "    ldr r3, =vPortSVCHandler_C  \n"
            "    bx r3                       \n"
            "                                \n"
            " stacking_used_msp:             \n"
            "    mrs r0, msp                 \n"
            "    ldr r3, =vPortSVCHandler_C  \n"
            "    bx r3                       \n"
            "                                \n"
            " .align 4                       \n"
        );
    }

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

/*-----------------------------------------------------------*/
