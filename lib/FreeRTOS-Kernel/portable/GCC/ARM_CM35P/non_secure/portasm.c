/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Copyright 2024 Arm Limited and/or its affiliates
 * <open-source-office@arm.com>
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
            " .syntax unified                                 \n"
            "                                                 \n"
            " program_mpu_first_task:                         \n"
            "    ldr r3, =pxCurrentTCB                        \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "    ldr r0, [r3]                                 \n" /* r0 = pxCurrentTCB. */
            "                                                 \n"
            "    dmb                                          \n" /* Complete outstanding transfers before disabling MPU. */
            "    ldr r1, =0xe000ed94                          \n" /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
            "    ldr r2, [r1]                                 \n" /* Read the value of MPU_CTRL. */
            "    bic r2, #1                                   \n" /* r2 = r2 & ~1 i.e. Clear the bit 0 in r2. */
            "    str r2, [r1]                                 \n" /* Disable MPU. */
            "                                                 \n"
            "    adds r0, #4                                  \n" /* r0 = r0 + 4. r0 now points to MAIR0 in TCB. */
            "    ldr r1, [r0]                                 \n" /* r1 = *r0 i.e. r1 = MAIR0. */
            "    ldr r2, =0xe000edc0                          \n" /* r2 = 0xe000edc0 [Location of MAIR0]. */
            "    str r1, [r2]                                 \n" /* Program MAIR0. */
            "                                                 \n"
            "    adds r0, #4                                  \n" /* r0 = r0 + 4. r0 now points to first RBAR in TCB. */
            "    ldr r1, =0xe000ed98                          \n" /* r1 = 0xe000ed98 [Location of RNR]. */
            "    ldr r2, =0xe000ed9c                          \n" /* r2 = 0xe000ed9c [Location of RBAR]. */
            "                                                 \n"
            "    movs r3, #4                                  \n" /* r3 = 4. */
            "    str r3, [r1]                                 \n" /* Program RNR = 4. */
            "    ldmia r0!, {r4-r11}                          \n" /* Read 4 set of RBAR/RLAR registers from TCB. */
            "    stmia r2, {r4-r11}                           \n" /* Write 4 set of RBAR/RLAR registers using alias registers. */
            "                                                 \n"
            #if ( configTOTAL_MPU_REGIONS == 16 )
                "    movs r3, #8                                  \n" /* r3 = 8. */
                "    str r3, [r1]                                 \n" /* Program RNR = 8. */
                "    ldmia r0!, {r4-r11}                          \n" /* Read 4 set of RBAR/RLAR registers from TCB. */
                "    stmia r2, {r4-r11}                           \n" /* Write 4 set of RBAR/RLAR registers using alias registers. */
                "    movs r3, #12                                 \n" /* r3 = 12. */
                "    str r3, [r1]                                 \n" /* Program RNR = 12. */
                "    ldmia r0!, {r4-r11}                          \n" /* Read 4 set of RBAR/RLAR registers from TCB. */
                "    stmia r2, {r4-r11}                           \n" /* Write 4 set of RBAR/RLAR registers using alias registers. */
            #endif /* configTOTAL_MPU_REGIONS == 16 */
            "                                                 \n"
            "    ldr r1, =0xe000ed94                          \n" /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
            "    ldr r2, [r1]                                 \n" /* Read the value of MPU_CTRL. */
            "    orr r2, #1                                   \n" /* r2 = r1 | 1 i.e. Set the bit 0 in r2. */
            "    str r2, [r1]                                 \n" /* Enable MPU. */
            "    dsb                                          \n" /* Force memory writes before continuing. */
            "                                                 \n"
            " restore_context_first_task:                     \n"
            "    ldr r3, =pxCurrentTCB                        \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "    ldr r1, [r3]                                 \n" /* r1 = pxCurrentTCB.*/
            "    ldr r2, [r1]                                 \n" /* r2 = Location of saved context in TCB. */
            "                                                 \n"
            " restore_special_regs_first_task:                \n"
            "    ldmdb r2!, {r0, r3-r5, lr}                   \n" /* r0 = xSecureContext, r3 = original PSP, r4 = PSPLIM, r5 = CONTROL, LR restored. */
            "    msr psp, r3                                  \n"
            "    msr psplim, r4                               \n"
            "    msr control, r5                              \n"
            "    ldr r4, =xSecureContext                      \n" /* Read the location of xSecureContext i.e. &( xSecureContext ). */
            "    str r0, [r4]                                 \n" /* Restore xSecureContext. */
            "                                                 \n"
            " restore_general_regs_first_task:                \n"
            "    ldmdb r2!, {r4-r11}                          \n" /* r4-r11 contain hardware saved context. */
            "    stmia r3!, {r4-r11}                          \n" /* Copy the hardware saved context on the task stack. */
            "    ldmdb r2!, {r4-r11}                          \n" /* r4-r11 restored. */
            "                                                 \n"
            " restore_context_done_first_task:                \n"
            "    str r2, [r1]                                 \n" /* Save the location where the context should be saved next as the first member of TCB. */
            "    mov r0, #0                                   \n"
            "    msr basepri, r0                              \n" /* Ensure that interrupts are enabled when the first task starts. */
            "    bx lr                                        \n"
        );
    }

#else /* configENABLE_MPU */

    void vRestoreContextOfFirstTask( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            "   .syntax unified                                 \n"
            "                                                   \n"
            "   ldr  r2, =pxCurrentTCB                          \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "   ldr  r3, [r2]                                   \n" /* Read pxCurrentTCB. */
            "   ldr  r0, [r3]                                   \n" /* Read top of stack from TCB - The first item in pxCurrentTCB is the task top of stack. */
            "                                                   \n"
            "   ldm  r0!, {r1-r3}                               \n" /* Read from stack - r1 = xSecureContext, r2 = PSPLIM and r3 = EXC_RETURN. */
            "   ldr  r4, =xSecureContext                        \n"
            "   str  r1, [r4]                                   \n" /* Set xSecureContext to this task's value for the same. */
            "   msr  psplim, r2                                 \n" /* Set this task's PSPLIM value. */
            "   mrs  r1, control                                \n" /* Obtain current control register value. */
            "   orrs r1, r1, #2                                 \n" /* r1 = r1 | 0x2 - Set the second bit to use the program stack pointer (PSP). */
            "   msr control, r1                                 \n" /* Write back the new control register value. */
            "   adds r0, #32                                    \n" /* Discard everything up to r0. */
            "   msr  psp, r0                                    \n" /* This is now the new top of stack to use in the task. */
            "   isb                                             \n"
            "   mov  r0, #0                                     \n"
            "   msr  basepri, r0                                \n" /* Ensure that interrupts are enabled when the first task starts. */
            "   bx   r3                                         \n" /* Finally, branch to EXC_RETURN. */
        );
    }

#endif /* configENABLE_MPU */
/*-----------------------------------------------------------*/

BaseType_t xIsPrivileged( void ) /* __attribute__ (( naked )) */
{
    __asm volatile
    (
        "   .syntax unified                                 \n"
        "                                                   \n"
        "   mrs r0, control                                 \n" /* r0 = CONTROL. */
        "   tst r0, #1                                      \n" /* Perform r0 & 1 (bitwise AND) and update the conditions flag. */
        "   ite ne                                          \n"
        "   movne r0, #0                                    \n" /* CONTROL[0]!=0. Return false to indicate that the processor is not privileged. */
        "   moveq r0, #1                                    \n" /* CONTROL[0]==0. Return true to indicate that the processor is privileged. */
        "   bx lr                                           \n" /* Return. */
        ::: "r0", "memory"
    );
}
/*-----------------------------------------------------------*/

void vRaisePrivilege( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
{
    __asm volatile
    (
        "   .syntax unified                                 \n"
        "                                                   \n"
        "   mrs r0, control                                 \n" /* Read the CONTROL register. */
        "   bic r0, #1                                      \n" /* Clear the bit 0. */
        "   msr control, r0                                 \n" /* Write back the new CONTROL value. */
        "   bx lr                                           \n" /* Return to the caller. */
        ::: "r0", "memory"
    );
}
/*-----------------------------------------------------------*/

void vResetPrivilege( void ) /* __attribute__ (( naked )) */
{
    __asm volatile
    (
        "   .syntax unified                                 \n"
        "                                                   \n"
        "   mrs r0, control                                 \n" /* r0 = CONTROL. */
        "   orr r0, #1                                      \n" /* r0 = r0 | 1. */
        "   msr control, r0                                 \n" /* CONTROL = r0. */
        "   bx lr                                           \n" /* Return to the caller. */
        ::: "r0", "memory"
    );
}
/*-----------------------------------------------------------*/

void vStartFirstTask( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
{
    __asm volatile
    (
        "   .syntax unified                                 \n"
        "                                                   \n"
        "   ldr r0, =0xe000ed08                             \n" /* Use the NVIC offset register to locate the stack. */
        "   ldr r0, [r0]                                    \n" /* Read the VTOR register which gives the address of vector table. */
        "   ldr r0, [r0]                                    \n" /* The first entry in vector table is stack pointer. */
        "   msr msp, r0                                     \n" /* Set the MSP back to the start of the stack. */
        "   cpsie i                                         \n" /* Globally enable interrupts. */
        "   cpsie f                                         \n"
        "   dsb                                             \n"
        "   isb                                             \n"
        "   svc %0                                          \n" /* System call to start the first task. */
        "   nop                                             \n"
        ::"i" ( portSVC_START_SCHEDULER ) : "memory"
    );
}
/*-----------------------------------------------------------*/

uint32_t ulSetInterruptMask( void ) /* __attribute__(( naked )) PRIVILEGED_FUNCTION */
{
    __asm volatile
    (
        "   .syntax unified                                 \n"
        "                                                   \n"
        "   mrs r0, basepri                                 \n" /* r0 = basepri. Return original basepri value. */
        "   mov r1, %0                                      \n" /* r1 = configMAX_SYSCALL_INTERRUPT_PRIORITY. */
        "   msr basepri, r1                                 \n" /* Disable interrupts up to configMAX_SYSCALL_INTERRUPT_PRIORITY. */
        "   dsb                                             \n"
        "   isb                                             \n"
        "   bx lr                                           \n" /* Return. */
        ::"i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY ) : "memory"
    );
}
/*-----------------------------------------------------------*/

void vClearInterruptMask( __attribute__( ( unused ) ) uint32_t ulMask ) /* __attribute__(( naked )) PRIVILEGED_FUNCTION */
{
    __asm volatile
    (
        "   .syntax unified                                 \n"
        "                                                   \n"
        "   msr basepri, r0                                 \n" /* basepri = ulMask. */
        "   dsb                                             \n"
        "   isb                                             \n"
        "   bx lr                                           \n" /* Return. */
        ::: "memory"
    );
}
/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )

    void PendSV_Handler( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            " .syntax unified                                 \n"
            " .extern SecureContext_SaveContext               \n"
            " .extern SecureContext_LoadContext               \n"
            "                                                 \n"
            " ldr r3, =xSecureContext                         \n" /* Read the location of xSecureContext i.e. &( xSecureContext ). */
            " ldr r0, [r3]                                    \n" /* Read xSecureContext - Value of xSecureContext must be in r0 as it is used as a parameter later. */
            " ldr r3, =pxCurrentTCB                           \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            " ldr r1, [r3]                                    \n" /* Read pxCurrentTCB - Value of pxCurrentTCB must be in r1 as it is used as a parameter later. */
            " ldr r2, [r1]                                    \n" /* r2 = Location in TCB where the context should be saved. */
            "                                                 \n"
            " cbz r0, save_ns_context                         \n" /* No secure context to save. */
            " save_s_context:                                 \n"
            "    push {r0-r2, lr}                             \n"
            "    bl SecureContext_SaveContext                 \n" /* Params are in r0 and r1. r0 = xSecureContext and r1 = pxCurrentTCB. */
            "    pop {r0-r2, lr}                              \n"
            "                                                 \n"
            " save_ns_context:                                \n"
            "    mov r3, lr                                   \n" /* r3 = LR (EXC_RETURN). */
            "    lsls r3, r3, #25                             \n" /* r3 = r3 << 25. Bit[6] of EXC_RETURN is 1 if secure stack was used, 0 if non-secure stack was used to store stack frame. */
            "    bmi save_special_regs                        \n" /* r3 < 0 ==> Bit[6] in EXC_RETURN is 1 ==> secure stack was used to store the stack frame. */
            "                                                 \n"
            " save_general_regs:                              \n"
            "    mrs r3, psp                                  \n"
            "                                                 \n"
            #if ( ( configENABLE_FPU == 1 ) || ( configENABLE_MVE == 1 ) )
                "    add r3, r3, #0x20                            \n" /* Move r3 to location where s0 is saved. */
                "    tst lr, #0x10                                \n"
                "    ittt eq                                      \n"
                "    vstmiaeq r2!, {s16-s31}                      \n" /* Store s16-s31. */
                "    vldmiaeq r3, {s0-s16}                        \n" /* Copy hardware saved FP context into s0-s16. */
                "    vstmiaeq r2!, {s0-s16}                       \n" /* Store hardware saved FP context. */
                "    sub r3, r3, #0x20                            \n" /* Set r3 back to the location of hardware saved context. */
            #endif /* configENABLE_FPU || configENABLE_MVE */
            "                                                 \n"
            "    stmia r2!, {r4-r11}                          \n" /* Store r4-r11. */
            "    ldmia r3, {r4-r11}                           \n" /* Copy the hardware saved context into r4-r11. */
            "    stmia r2!, {r4-r11}                          \n" /* Store the hardware saved context. */
            "                                                 \n"
            " save_special_regs:                              \n"
            "    mrs r3, psp                                  \n" /* r3 = PSP. */
            "    mrs r4, psplim                               \n" /* r4 = PSPLIM. */
            "    mrs r5, control                              \n" /* r5 = CONTROL. */
            "    stmia r2!, {r0, r3-r5, lr}                   \n" /* Store xSecureContext, original PSP (after hardware has saved context), PSPLIM, CONTROL and LR. */
            "    str r2, [r1]                                 \n" /* Save the location from where the context should be restored as the first member of TCB. */
            "                                                 \n"
            " select_next_task:                               \n"
            "    mov r0, %0                                   \n" /* r0 = configMAX_SYSCALL_INTERRUPT_PRIORITY */
            "    msr basepri, r0                              \n" /* Disable interrupts up to configMAX_SYSCALL_INTERRUPT_PRIORITY. */
            "    dsb                                          \n"
            "    isb                                          \n"
            "    bl vTaskSwitchContext                        \n"
            "    mov r0, #0                                   \n" /* r0 = 0. */
            "    msr basepri, r0                              \n" /* Enable interrupts. */
            "                                                 \n"
            " program_mpu:                                    \n"
            "    ldr r3, =pxCurrentTCB                        \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "    ldr r0, [r3]                                 \n" /* r0 = pxCurrentTCB.*/
            "                                                 \n"
            "    dmb                                          \n" /* Complete outstanding transfers before disabling MPU. */
            "    ldr r1, =0xe000ed94                          \n" /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
            "    ldr r2, [r1]                                 \n" /* Read the value of MPU_CTRL. */
            "    bic r2, #1                                   \n" /* r2 = r2 & ~1 i.e. Clear the bit 0 in r2. */
            "    str r2, [r1]                                 \n" /* Disable MPU. */
            "                                                 \n"
            "    adds r0, #4                                  \n" /* r0 = r0 + 4. r0 now points to MAIR0 in TCB. */
            "    ldr r1, [r0]                                 \n" /* r1 = *r0 i.e. r1 = MAIR0. */
            "    ldr r2, =0xe000edc0                          \n" /* r2 = 0xe000edc0 [Location of MAIR0]. */
            "    str r1, [r2]                                 \n" /* Program MAIR0. */
            "                                                 \n"
            "    adds r0, #4                                  \n" /* r0 = r0 + 4. r0 now points to first RBAR in TCB. */
            "    ldr r1, =0xe000ed98                          \n" /* r1 = 0xe000ed98 [Location of RNR]. */
            "    ldr r2, =0xe000ed9c                          \n" /* r2 = 0xe000ed9c [Location of RBAR]. */
            "                                                 \n"
            "    movs r3, #4                                  \n" /* r3 = 4. */
            "    str r3, [r1]                                 \n" /* Program RNR = 4. */
            "    ldmia r0!, {r4-r11}                          \n" /* Read 4 sets of RBAR/RLAR registers from TCB. */
            "    stmia r2, {r4-r11}                           \n" /* Write 4 set of RBAR/RLAR registers using alias registers. */
            "                                                 \n"
            #if ( configTOTAL_MPU_REGIONS == 16 )
                "    movs r3, #8                                  \n" /* r3 = 8. */
                "    str r3, [r1]                                 \n" /* Program RNR = 8. */
                "    ldmia r0!, {r4-r11}                          \n" /* Read 4 sets of RBAR/RLAR registers from TCB. */
                "    stmia r2, {r4-r11}                           \n" /* Write 4 set of RBAR/RLAR registers using alias registers. */
                "    movs r3, #12                                 \n" /* r3 = 12. */
                "    str r3, [r1]                                 \n" /* Program RNR = 12. */
                "    ldmia r0!, {r4-r11}                          \n" /* Read 4 sets of RBAR/RLAR registers from TCB. */
                "    stmia r2, {r4-r11}                           \n" /* Write 4 set of RBAR/RLAR registers using alias registers. */
            #endif /* configTOTAL_MPU_REGIONS == 16 */
            "                                                 \n"
            "   ldr r1, =0xe000ed94                           \n" /* r1 = 0xe000ed94 [Location of MPU_CTRL]. */
            "   ldr r2, [r1]                                  \n" /* Read the value of MPU_CTRL. */
            "   orr r2, #1                                    \n" /* r2 = r2 | 1 i.e. Set the bit 0 in r2. */
            "   str r2, [r1]                                  \n" /* Enable MPU. */
            "   dsb                                           \n" /* Force memory writes before continuing. */
            "                                                 \n"
            " restore_context:                                \n"
            "    ldr r3, =pxCurrentTCB                        \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "    ldr r1, [r3]                                 \n" /* r1 = pxCurrentTCB.*/
            "    ldr r2, [r1]                                 \n" /* r2 = Location of saved context in TCB. */
            "                                                 \n"
            " restore_special_regs:                           \n"
            "    ldmdb r2!, {r0, r3-r5, lr}                   \n" /* r0 = xSecureContext, r3 = original PSP, r4 = PSPLIM, r5 = CONTROL, LR restored. */
            "    msr psp, r3                                  \n"
            "    msr psplim, r4                               \n"
            "    msr control, r5                              \n"
            "    ldr r4, =xSecureContext                      \n" /* Read the location of xSecureContext i.e. &( xSecureContext ). */
            "    str r0, [r4]                                 \n" /* Restore xSecureContext. */
            "    cbz r0, restore_ns_context                   \n" /* No secure context to restore. */
            "                                                 \n"
            " restore_s_context:                              \n"
            "    push {r1-r3, lr}                             \n"
            "    bl SecureContext_LoadContext                 \n" /* Params are in r0 and r1. r0 = xSecureContext and r1 = pxCurrentTCB. */
            "    pop {r1-r3, lr}                              \n"
            "                                                 \n"
            " restore_ns_context:                             \n"
            "    mov r0, lr                                   \n" /* r0 = LR (EXC_RETURN). */
            "    lsls r0, r0, #25                             \n" /* r0 = r0 << 25. Bit[6] of EXC_RETURN is 1 if secure stack was used, 0 if non-secure stack was used to store stack frame. */
            "    bmi restore_context_done                     \n" /* r0 < 0 ==> Bit[6] in EXC_RETURN is 1 ==> secure stack was used to store the stack frame. */
            "                                                 \n"
            " restore_general_regs:                           \n"
            "    ldmdb r2!, {r4-r11}                          \n" /* r4-r11 contain hardware saved context. */
            "    stmia r3!, {r4-r11}                          \n" /* Copy the hardware saved context on the task stack. */
            "    ldmdb r2!, {r4-r11}                          \n" /* r4-r11 restored. */
            #if ( ( configENABLE_FPU == 1 ) || ( configENABLE_MVE == 1 ) )
                "    tst lr, #0x10                                \n"
                "    ittt eq                                      \n"
                "    vldmdbeq r2!, {s0-s16}                       \n" /* s0-s16 contain hardware saved FP context. */
                "    vstmiaeq r3!, {s0-s16}                       \n" /* Copy hardware saved FP context on the task stack. */
                "    vldmdbeq r2!, {s16-s31}                      \n" /* Restore s16-s31. */
            #endif /* configENABLE_FPU || configENABLE_MVE */
            "                                                 \n"
            " restore_context_done:                           \n"
            "    str r2, [r1]                                 \n" /* Save the location where the context should be saved next as the first member of TCB. */
            "    bx lr                                        \n"
            ::"i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY )
        );
    }

#else /* configENABLE_MPU */

    void PendSV_Handler( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            "   .syntax unified                                 \n"
            "   .extern SecureContext_SaveContext               \n"
            "   .extern SecureContext_LoadContext               \n"
            "                                                   \n"
            "   ldr r3, =xSecureContext                         \n" /* Read the location of xSecureContext i.e. &( xSecureContext ). */
            "   ldr r0, [r3]                                    \n" /* Read xSecureContext - Value of xSecureContext must be in r0 as it is used as a parameter later. */
            "   ldr r3, =pxCurrentTCB                           \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "   ldr r1, [r3]                                    \n" /* Read pxCurrentTCB - Value of pxCurrentTCB must be in r1 as it is used as a parameter later. */
            "   mrs r2, psp                                     \n" /* Read PSP in r2. */
            "                                                   \n"
            "   cbz r0, save_ns_context                         \n" /* No secure context to save. */
            "   push {r0-r2, r14}                               \n"
            "   bl SecureContext_SaveContext                    \n" /* Params are in r0 and r1. r0 = xSecureContext and r1 = pxCurrentTCB. */
            "   pop {r0-r3}                                     \n" /* LR is now in r3. */
            "   mov lr, r3                                      \n" /* LR = r3. */
            "   lsls r1, r3, #25                                \n" /* r1 = r3 << 25. Bit[6] of EXC_RETURN is 1 if secure stack was used, 0 if non-secure stack was used to store stack frame. */
            "   bpl save_ns_context                             \n" /* bpl - branch if positive or zero. If r1 >= 0 ==> Bit[6] in EXC_RETURN is 0 i.e. non-secure stack was used. */
            "                                                   \n"
            "   ldr r3, =pxCurrentTCB                           \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "   ldr r1, [r3]                                    \n" /* Read pxCurrentTCB.*/
            "   subs r2, r2, #12                                \n" /* Make space for xSecureContext, PSPLIM and LR on the stack. */
            "   str r2, [r1]                                    \n" /* Save the new top of stack in TCB. */
            "   mrs r1, psplim                                  \n" /* r1 = PSPLIM. */
            "   mov r3, lr                                      \n" /* r3 = LR/EXC_RETURN. */
            "   stmia r2!, {r0, r1, r3}                         \n" /* Store xSecureContext, PSPLIM and LR on the stack. */
            "   b select_next_task                              \n"
            "                                                   \n"
            " save_ns_context:                                  \n"
            "   ldr r3, =pxCurrentTCB                           \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "   ldr r1, [r3]                                    \n" /* Read pxCurrentTCB. */
            #if ( ( configENABLE_FPU == 1 ) || ( configENABLE_MVE == 1 ) )
                "   tst lr, #0x10                               \n" /* Test Bit[4] in LR. Bit[4] of EXC_RETURN is 0 if the Extended Stack Frame is in use. */
                "   it eq                                       \n"
                "   vstmdbeq r2!, {s16-s31}                     \n" /* Store the additional FP context registers which are not saved automatically. */
            #endif /* configENABLE_FPU || configENABLE_MVE */
            "   subs r2, r2, #44                                \n" /* Make space for xSecureContext, PSPLIM, LR and the remaining registers on the stack. */
            "   str r2, [r1]                                    \n" /* Save the new top of stack in TCB. */
            "   adds r2, r2, #12                                \n" /* r2 = r2 + 12. */
            "   stm r2, {r4-r11}                                \n" /* Store the registers that are not saved automatically. */
            "   mrs r1, psplim                                  \n" /* r1 = PSPLIM. */
            "   mov r3, lr                                      \n" /* r3 = LR/EXC_RETURN. */
            "   subs r2, r2, #12                                \n" /* r2 = r2 - 12. */
            "   stmia r2!, {r0, r1, r3}                         \n" /* Store xSecureContext, PSPLIM and LR on the stack. */
            "                                                   \n"
            " select_next_task:                                 \n"
            "   mov r0, %0                                      \n" /* r0 = configMAX_SYSCALL_INTERRUPT_PRIORITY */
            "   msr basepri, r0                                 \n" /* Disable interrupts up to configMAX_SYSCALL_INTERRUPT_PRIORITY. */
            "   dsb                                             \n"
            "   isb                                             \n"
            "   bl vTaskSwitchContext                           \n"
            "   mov r0, #0                                      \n" /* r0 = 0. */
            "   msr basepri, r0                                 \n" /* Enable interrupts. */
            "                                                   \n"
            "   ldr r3, =pxCurrentTCB                           \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "   ldr r1, [r3]                                    \n" /* Read pxCurrentTCB. */
            "   ldr r2, [r1]                                    \n" /* The first item in pxCurrentTCB is the task top of stack. r2 now points to the top of stack. */
            "                                                   \n"
            "   ldmia r2!, {r0, r1, r4}                         \n" /* Read from stack - r0 = xSecureContext, r1 = PSPLIM and r4 = LR. */
            "   msr psplim, r1                                  \n" /* Restore the PSPLIM register value for the task. */
            "   mov lr, r4                                      \n" /* LR = r4. */
            "   ldr r3, =xSecureContext                         \n" /* Read the location of xSecureContext i.e. &( xSecureContext ). */
            "   str r0, [r3]                                    \n" /* Restore the task's xSecureContext. */
            "   cbz r0, restore_ns_context                      \n" /* If there is no secure context for the task, restore the non-secure context. */
            "   ldr r3, =pxCurrentTCB                           \n" /* Read the location of pxCurrentTCB i.e. &( pxCurrentTCB ). */
            "   ldr r1, [r3]                                    \n" /* Read pxCurrentTCB. */
            "   push {r2, r4}                                   \n"
            "   bl SecureContext_LoadContext                    \n" /* Restore the secure context. Params are in r0 and r1. r0 = xSecureContext and r1 = pxCurrentTCB. */
            "   pop {r2, r4}                                    \n"
            "   mov lr, r4                                      \n" /* LR = r4. */
            "   lsls r1, r4, #25                                \n" /* r1 = r4 << 25. Bit[6] of EXC_RETURN is 1 if secure stack was used, 0 if non-secure stack was used to store stack frame. */
            "   bpl restore_ns_context                          \n" /* bpl - branch if positive or zero. If r1 >= 0 ==> Bit[6] in EXC_RETURN is 0 i.e. non-secure stack was used. */
            "   msr psp, r2                                     \n" /* Remember the new top of stack for the task. */
            "   bx lr                                           \n"
            "                                                   \n"
            " restore_ns_context:                               \n"
            "   ldmia r2!, {r4-r11}                             \n" /* Restore the registers that are not automatically restored. */
            #if ( ( configENABLE_FPU == 1 ) || ( configENABLE_MVE == 1 ) )
                "   tst lr, #0x10                               \n" /* Test Bit[4] in LR. Bit[4] of EXC_RETURN is 0 if the Extended Stack Frame is in use. */
                "   it eq                                       \n"
                "   vldmiaeq r2!, {s16-s31}                     \n" /* Restore the additional FP context registers which are not restored automatically. */
            #endif /* configENABLE_FPU || configENABLE_MVE */
            "   msr psp, r2                                     \n" /* Remember the new top of stack for the task. */
            "   bx lr                                           \n"
            ::"i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY )
        );
    }

#endif /* configENABLE_MPU */
/*-----------------------------------------------------------*/

#if ( ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    void SVC_Handler( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            ".syntax unified                \n"
            ".extern vPortSVCHandler_C      \n"
            ".extern vSystemCallEnter       \n"
            ".extern vSystemCallExit        \n"
            "                               \n"
            "tst lr, #4                     \n"
            "ite eq                         \n"
            "mrseq r0, msp                  \n"
            "mrsne r0, psp                  \n"
            "                               \n"
            "ldr r1, [r0, #24]              \n"
            "ldrb r2, [r1, #-2]             \n"
            "cmp r2, %0                     \n"
            "blt syscall_enter              \n"
            "cmp r2, %1                     \n"
            "beq syscall_exit               \n"
            "b vPortSVCHandler_C            \n"
            "                               \n"
            "syscall_enter:                 \n"
            "    mov r1, lr                 \n"
            "    b vSystemCallEnter         \n"
            "                               \n"
            "syscall_exit:                  \n"
            "    mov r1, lr                 \n"
            "    b vSystemCallExit          \n"
            "                               \n"
            : /* No outputs. */
            : "i" ( NUM_SYSTEM_CALLS ), "i" ( portSVC_SYSTEM_CALL_EXIT )
            : "r0", "r1", "r2", "memory"
        );
    }

#else /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */

    void SVC_Handler( void ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
    {
        __asm volatile
        (
            "   .syntax unified                                 \n"
            "                                                   \n"
            "   tst lr, #4                                      \n"
            "   ite eq                                          \n"
            "   mrseq r0, msp                                   \n"
            "   mrsne r0, psp                                   \n"
            "   ldr r1, =vPortSVCHandler_C                      \n"
            "   bx r1                                           \n"
        );
    }

#endif /* ( configENABLE_MPU == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) */
/*-----------------------------------------------------------*/

void vPortAllocateSecureContext( uint32_t ulSecureStackSize ) /* __attribute__ (( naked )) */
{
    __asm volatile
    (
        "   .syntax unified                                 \n"
        "                                                   \n"
        "   svc %0                                          \n" /* Secure context is allocated in the supervisor call. */
        "   bx lr                                           \n" /* Return. */
        ::"i" ( portSVC_ALLOCATE_SECURE_CONTEXT ) : "memory"
    );
}
/*-----------------------------------------------------------*/

void vPortFreeSecureContext( uint32_t * pulTCB ) /* __attribute__ (( naked )) PRIVILEGED_FUNCTION */
{
    __asm volatile
    (
        "   .syntax unified                                 \n"
        "                                                   \n"
        "   ldr r2, [r0]                                    \n" /* The first item in the TCB is the top of the stack. */
        "   ldr r1, [r2]                                    \n" /* The first item on the stack is the task's xSecureContext. */
        "   cmp r1, #0                                      \n" /* Raise svc if task's xSecureContext is not NULL. */
        "   it ne                                           \n"
        "   svcne %0                                        \n" /* Secure context is freed in the supervisor call. */
        "   bx lr                                           \n" /* Return. */
        ::"i" ( portSVC_FREE_SECURE_CONTEXT ) : "memory"
    );
}
/*-----------------------------------------------------------*/
