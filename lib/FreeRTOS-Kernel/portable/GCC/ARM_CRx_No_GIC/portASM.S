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

    .text
    .arm
    .syntax unified

    .set SYS_MODE,   0x1f
    .set SVC_MODE,   0x13
    .set IRQ_MODE,   0x12
    .set CPSR_I_BIT, 0x80

    /* Variables and functions. */
    .extern pxCurrentTCB
    .extern vTaskSwitchContext
    .extern vApplicationIRQHandler
    .extern vApplicationFPUSafeIRQHandler
    .extern ulPortInterruptNesting
    .extern ulPortTaskHasFPUContext
    .extern ulICCEOIR
    .extern ulPortYieldRequired

    .global FreeRTOS_IRQ_Handler
    .global FreeRTOS_SVC_Handler
    .global vPortRestoreTaskContext
    .global vPortInitialiseFPSCR
    .global ulReadAPSR
    .global vPortYield
    .global vPortEnableInterrupts
    .global vPortDisableInterrupts
    .global ulPortSetInterruptMaskFromISR
    .global ulPortCountLeadingZeros

    .weak   vApplicationSVCHandler
/*-----------------------------------------------------------*/

.macro portSAVE_CONTEXT

    /* Save the LR and SPSR onto the system mode stack before switching to
     * system mode to save the remaining system mode registers. */
    SRSDB   SP!, #SYS_MODE
    CPS     #SYS_MODE
    PUSH    {R0-R12, R14}

    /* Push the critical nesting count. */
    LDR     R2, =ulCriticalNesting
    LDR     R1, [R2]
    PUSH    {R1}

    /* Does the task have a floating point context that needs saving?  If
     * ulPortTaskHasFPUContext is 0 then no. */
    LDR     R2, =ulPortTaskHasFPUContext
    LDR     R3, [R2]
    CMP     R3, #0

    /* Save the floating point context, if any. */
    VMRSNE  R1,  FPSCR
    VPUSHNE {D0-D15}
#if configFPU_D32 == 1
    VPUSHNE {D16-D31}
#endif /* configFPU_D32 */
    PUSHNE  {R1}

    /* Save ulPortTaskHasFPUContext itself. */
    PUSH    {R3}

    /* Save the stack pointer in the TCB. */
    LDR     R0, =pxCurrentTCB
    LDR     R1, [R0]
    STR     SP, [R1]

    .endm

/*-----------------------------------------------------------*/

.macro portRESTORE_CONTEXT

    /* Set the SP to point to the stack of the task being restored. */
    LDR     R0, =pxCurrentTCB
    LDR     R1, [R0]
    LDR     SP, [R1]

    /* Is there a floating point context to restore?  If the restored
     * ulPortTaskHasFPUContext is zero then no. */
    LDR     R0, =ulPortTaskHasFPUContext
    POP     {R1}
    STR     R1, [R0]
    CMP     R1, #0

    /* Restore the floating point context, if any. */
    POPNE   {R0}
#if configFPU_D32 == 1
    VPOPNE  {D16-D31}
#endif /* configFPU_D32 */
    VPOPNE  {D0-D15}
    VMSRNE  FPSCR, R0

    /* Restore the critical section nesting depth. */
    LDR     R0, =ulCriticalNesting
    POP     {R1}
    STR     R1, [R0]

    /* Restore all system mode registers other than the SP (which is already
    being used). */
    POP     {R0-R12, R14}

    /* Return to the task code, loading CPSR on the way. */
    RFEIA   SP!

    .endm

/*-----------------------------------------------------------*/

/*
 * void vPortRestoreTaskContext( void );
 *
 * vPortRestoreTaskContext is used to start the scheduler.
 */
.align 4
.type vPortRestoreTaskContext, %function
vPortRestoreTaskContext:
    /* Switch to system mode. */
    CPS     #SYS_MODE
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

/*
 * void vPortInitialiseFPSCR( void );
 *
 * vPortInitialiseFPSCR is used to initialize the FPSCR register.
 */
.align 4
.type vPortInitialiseFPSCR, %function
vPortInitialiseFPSCR:
    MOV     R0, #0
    VMSR    FPSCR, R0
    BX      LR

/*-----------------------------------------------------------*/

/*
 * uint32_t ulReadAPSR( void );
 *
 * ulReadAPSR is used to read the value of APSR context.
 */
.align 4
.type ulReadAPSR, %function
ulReadAPSR:
    MRS R0, APSR
    BX  LR

/*-----------------------------------------------------------*/

/*
 * void vPortYield( void );
 */
.align 4
.type vPortYield, %function
vPortYield:
    SVC 0
    ISB
    BX  LR

/*-----------------------------------------------------------*/

/*
 * void vPortEnableInterrupts( void );
 */
.align 4
.type vPortEnableInterrupts, %function
vPortEnableInterrupts:
    CPSIE   I
    BX      LR

/*-----------------------------------------------------------*/

/*
 * void vPortDisableInterrupts( void );
 */
.align 4
.type vPortDisableInterrupts, %function
vPortDisableInterrupts:
    CPSID    I
    DSB
    ISB
    BX      LR

/*-----------------------------------------------------------*/

/*
 * uint32_t ulPortSetInterruptMaskFromISR( void );
 */
.align 4
.type ulPortSetInterruptMaskFromISR, %function
ulPortSetInterruptMaskFromISR:
    MRS     R0, CPSR
    AND     R0, R0, #CPSR_I_BIT
    CPSID   I
    DSB
    ISB
    BX      LR

/*-----------------------------------------------------------*/

/*
 * void vApplicationSVCHandler( uint32_t ulSvcNumber );
 */
.align 4
.type vApplicationSVCHandler, %function
vApplicationSVCHandler:
    B vApplicationSVCHandler

/*-----------------------------------------------------------*/

/* If the application provides an implementation of vApplicationIRQHandler(),
 * then it will get called directly without saving the FPU registers on
 * interrupt entry, and this weak implementation of vApplicationIRQHandler()
 * will not get called.
 *
 * If the application provides its own implementation of
 * vApplicationFPUSafeIRQHandler() then this implementation of
 * vApplicationIRQHandler() will be called, save the FPU registers, and then
 * call vApplicationFPUSafeIRQHandler().
 *
 * Therefore, if the application writer wants FPU registers to be saved on
 * interrupt entry, their IRQ handler must be called
 * vApplicationFPUSafeIRQHandler(), and if the application writer does not want
 * FPU registers to be saved on interrupt entry their IRQ handler must be
 * called vApplicationIRQHandler().
 */
.align 4
.weak vApplicationIRQHandler
.type vApplicationIRQHandler, %function
vApplicationIRQHandler:
    PUSH    {LR}

    VMRS    R1, FPSCR
    VPUSH   {D0-D7}
    PUSH    {R1}

    BLX     vApplicationFPUSafeIRQHandler

    POP     {R0}
    VPOP    {D0-D7}
    VMSR    FPSCR, R0

    POP     {PC}

/*-----------------------------------------------------------*/

.align 4
.weak vApplicationFPUSafeIRQHandler
.type vApplicationFPUSafeIRQHandler, %function
vApplicationFPUSafeIRQHandler:
    B       vApplicationFPUSafeIRQHandler

/*-----------------------------------------------------------*/

/*
 * UBaseType_t ulPortCountLeadingZeros( UBaseType_t ulBitmap );
 *
 * According to the Procedure Call Standard for the ARM Architecture (AAPCS):
 * - Parameter ulBitmap is passed in R0.
 * - Return value must be in R0.
 */
.align 4
.type ulPortCountLeadingZeros, %function
ulPortCountLeadingZeros:
    CLZ     R0, R0
    BX      LR

/*-----------------------------------------------------------*/

/*
 * SVC handler is used to yield.
 */
.align 4
.type FreeRTOS_SVC_Handler, %function
FreeRTOS_SVC_Handler:
    PUSH    { R0-R1 }

    /* ---------------------------- Get Caller SVC Number ---------------------------- */
    MRS     R0, SPSR               /* R0 = CPSR at the time of SVC. */
    TST     R0, #0x20              /* Check Thumb bit (5) in CPSR. */
    LDRHNE  R0, [LR, #-0x2]        /* If Thumb, load halfword. */
    BICNE   R0, R0, #0xFF00        /* And extract immidiate field (i.e. SVC number). */
    LDREQ   R0, [LR, #-0x4]        /* If ARM, load word. */
    BICEQ   R0, R0, #0xFF000000    /* And extract immidiate field (i.e. SVC number). */

    /* --------------------------------- SVC Routing --------------------------------- */
    CMP     R0, #0
    BEQ     svcPortYield
    BNE     svcApplicationCall

svcPortYield:
    POP     { R0-R1 }
    portSAVE_CONTEXT
    BLX     vTaskSwitchContext
    portRESTORE_CONTEXT

svcApplicationCall:
    POP     { R0-R1 }
    portSAVE_CONTEXT
    BLX     vApplicationSVCHandler
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

.align 4
.type FreeRTOS_IRQ_Handler, %function
FreeRTOS_IRQ_Handler:
    /* Return to the interrupted instruction. */
    SUB     LR, LR, #4

    /* Push the return address and SPSR. */
    PUSH    {LR}
    MRS     LR, SPSR
    PUSH    {LR}

    /* Change to supervisor mode to allow reentry. */
    CPS     #SVC_MODE

    /* Push used registers. */
    PUSH    {R0-R3, R12}

    /* Increment nesting count.  r3 holds the address of ulPortInterruptNesting
     * for future use.  r1 holds the original ulPortInterruptNesting value for
     * future use. */
    LDR     R3, =ulPortInterruptNesting
    LDR     R1, [R3]
    ADD     R0, R1, #1
    STR     R0, [R3]

    /* Ensure bit 2 of the stack pointer is clear.  r2 holds the bit 2 value for
     * future use. */
    MOV     R0, SP
    AND     R2, R0, #4
    SUB     SP, SP, R2

    /* Call the interrupt handler. */
    PUSH    {R0-R3, LR}
    BLX     vApplicationIRQHandler
    POP     {R0-R3, LR}
    ADD     SP, SP, R2

    /* Disable IRQs incase vApplicationIRQHandler enabled them for re-entry. */
    CPSID   i
    DSB
    ISB

    /* Write to the EOI register. */
    LDR     R0, =ulICCEOIR
    LDR     R2, [R0]
    STR     R0, [R2]

    /* Restore the old nesting count. */
    STR     R1, [R3]

    /* A context switch is never performed if the nesting count is not 0. */
    CMP     R1, #0
    BNE     exit_without_switch

    /* Did the interrupt request a context switch?  r1 holds the address of
     * ulPortYieldRequired and r0 the value of ulPortYieldRequired for future
     * use. */
    LDR     R1, =ulPortYieldRequired
    LDR     R0, [R1]
    CMP     R0, #0
    BNE     switch_before_exit

exit_without_switch:
    /* No context switch.  Restore used registers, LR_irq and SPSR before
     * returning. */
    POP     {R0-R3, R12}
    CPS     #IRQ_MODE
    POP     {LR}
    MSR     SPSR_cxsf, LR
    POP     {LR}
    MOVS    PC, LR

switch_before_exit:
    /* A context switch is to be performed.  Clear the context switch pending
     * flag. */
    MOV     R0, #0
    STR     R0, [R1]

    /* Restore used registers, LR-irq and SPSR before saving the context
     * to the task stack. */
    POP     {R0-R3, R12}
    CPS     #IRQ_MODE
    POP     {LR}
    MSR     SPSR_cxsf, LR
    POP     {LR}
    portSAVE_CONTEXT

    /* Call the function that selects the new task to execute.
     * vTaskSwitchContext() if vTaskSwitchContext() uses LDRD or STRD
     * instructions, or 8 byte aligned stack allocated data.  LR does not need
     * saving as a new LR will be loaded by portRESTORE_CONTEXT anyway. */
    BLX     vTaskSwitchContext

    /* Restore the context of, and branch to, the task selected to execute
     * next. */
    portRESTORE_CONTEXT

/*-----------------------------------------------------------*/

.end
