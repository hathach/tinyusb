/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2024 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

    .arm
    .syntax unified
    .section privileged_functions

#define FREERTOS_ASSEMBLY
    #include "portmacro_asm.h"
    #include "mpu_syscall_numbers.h"
#undef FREERTOS_ASSEMBLY

    /* External FreeRTOS-Kernel variables. */
    .extern pxCurrentTCB
    .extern uxSystemCallImplementations
    .extern ulPortInterruptNesting
    .extern ulPortYieldRequired

    /* External Llnker script variables. */
    .extern __syscalls_flash_start__
    .extern __syscalls_flash_end__

    /* External FreeRTOS-Kernel functions. */
    .extern vTaskSwitchContext
    .extern vApplicationIRQHandler

/* ----------------------------------------------------------------------------------- */

/* Save the context of a FreeRTOS Task. */
.macro portSAVE_CONTEXT
    DSB
    ISB
    /* Push R0 and LR to the stack for current mode. */
    PUSH    { R0, LR }

    LDR     LR, =pxCurrentTCB   /* LR = &( pxCurrentTCB ). */
    LDR     LR, [LR]            /* LR = pxCurrentTCB. */
    LDR     LR, [LR]            /* LR = pxTopOfStack i.e. the address where to store the task context. */

    LDR     R0, =ulCriticalNesting  /* R0 = &( ulCriticalNesting ). */
    LDR     R0, [R0]                /* R0 = ulCriticalNesting. */
    STM     LR!, { R0 }             /* Store ulCriticalNesting. ! increments LR after storing. */

#if ( portENABLE_FPU == 1 )
    VMRS    R0, FPSCR       /* R0 = FPSCR. */
    STM     LR!, { R0 }     /* Store FPSCR. */
    VSTM    LR!, { D0-D15 } /* Store D0-D15. */
#endif /* ( portENABLE_FPU == 1 ) */

    POP     { R0 }  /* Restore R0 to pre-exception value. */
    /* STM (user registers) - In a PL1 mode other than System mode, STM (user
     * registers) instruction stores multiple User mode registers to
     * consecutive memory locations using an address from a base register. The
     * processor reads the base register value normally, using the current mode
     * to determine the correct Banked version of the register. This instruction
     * cannot writeback to the base register.
     *
     * The following can be derived from the above description:
     * - The macro portSAVE_CONTEXT MUST be called from a PL1 mode other than
     *   the System mode.
     * - Base register LR of the current mode will be used which contains the
     *   location to store the context.
     * - It will store R0-R14 of User mode i.e. pre-exception SP(R13) and LR(R14)
     *   will be stored. */
    STM     LR, { R0-R14 }^
    ADD     LR, LR, #60 /* R0-R14 - Total 155 register, each 4 byte wide. */

    POP     { R0 }          /* Pre-exception PC is in R0. */
    MRS     R1, SPSR        /* R1 = Pre-exception CPSR. */
    STM     LR!, { R0-R1 }  /* Store pre-exception PC and CPSR. */

.endm

/* ----------------------------------------------------------------------------------- */

/* Restore the context of a FreeRTOS Task. */
.macro portRESTORE_CONTEXT
    /* Load the pointer to the current task's Task Control Block (TCB). */
    LDR     LR, =pxCurrentTCB   /* LR = &( pxCurrentTCB ). */
    LDR     LR, [LR]            /* LR = pxCurrentTCB. */
    ADD     R1, LR, #0x4        /* R1 now points to the xMPUSettings in TCB. */
    LDR     LR, [LR]            /* LR = pxTopOfStack i.e. the address where to restore the task context from. */

    /* When creating a loop label in a macro it has to be a numeric label.
     * for( R5 = portFIRST_CONFIGURABLE_REGION ; R5 <= portNUM_CONFIGURABLE_REGIONS ; R5++ ) */
    MOV     R5, #portFIRST_CONFIGURABLE_REGION
    123:
        LDMIA   R1!, { R2-R4 }  /* R2 = ulRegionSize, R3 = ulRegionAttribute, R4 = ulRegionBaseAddress. */

        MCR     p15, #0, R5, c6, c2, #0 /* MPU Region Number Register. */
        MCR     p15, #0, R4, c6, c1, #0 /* MPU Region Base Address Register. */
        MCR     p15, #0, R3, c6, c1, #4 /* MPU Region Access Control Register. */
        MCR     p15, #0, R2, c6, c1, #2 /* MPU Region Size and Enable Register. */

        ADD     R5, R5, #1
        CMP     R5, #portNUM_CONFIGURABLE_REGIONS
        BLE     123b

    LDR     R1, =ulCriticalNesting /* R1 = &( ulCriticalNesting ). */
    LDM     LR!, { R2 }            /* R2 = Stored ulCriticalNesting. */
    STR     R2, [R1]               /* Restore ulCriticalNesting. */

#if ( portENABLE_FPU == 1 )
    LDM     LR!, { R1 }     /* R1 = Stored FPSCR.  */
    VMSR    FPSCR, R1       /* Restore FPSCR. */
    VLDM   LR!, { D0-D15 }  /* Restore D0-D15. */
#endif /* portENABLE_FPU*/

    /* LDM (User registers) - In a PL1 mode other than System mode, LDM (User
     * registers) loads multiple User mode registers from consecutive memory
     * locations using an address from a base register. The registers loaded
     * cannot include the PC. The processor reads the base register value
     * normally, using the current mode to determine the correct Banked version
     * of the register. This instruction cannot writeback to the base register.
     *
     *  The following can be derived from the above description:
     * - The macro portRESTORE_CONTEXT MUST be called from a PL1 mode other than
     *   the System mode.
     * - Base register LR of the current mode will be used which contains the
     *   location to restore the context from.
     * - It will restore R0-R14 of User mode i.e. SP(R13) and LR(R14) of User
     *   mode will be restored.
     */
    LDM     LR, { R0-R14 }^
    ADD     LR, LR, #60 /* R0-R14 - Total 155 register, each 4 byte wide. */

    RFE     LR  /* Restore PC and CPSR from the context. */

.endm

/* ----------------------------------------------------------------------------------- */

/*
 * void vPortStartFirstTask( void );
 */
.align 4
.global vPortStartFirstTask
.type vPortStartFirstTask, %function
vPortStartFirstTask:
    /* This function is called from System Mode to start the FreeRTOS-Kernel.
     * As described in the portRESTORE_CONTEXT macro, portRESTORE_CONTEXT cannot
     * be called from the System mode. We, therefore, switch to the Supervisor
     * mode before calling portRESTORE_CONTEXT. */
    CPS #SVC_MODE
    portRESTORE_CONTEXT

/* ----------------------------------------------------------------------------------- */

.align 4
.global FreeRTOS_SVC_Handler
.type FreeRTOS_SVC_Handler, %function
FreeRTOS_SVC_Handler:
    PUSH    { R11-R12 }

    /* ------------------------- Caller Flash Location Check ------------------------- */

    LDR     R11, =__syscalls_flash_start__
    LDR     R12, =__syscalls_flash_end__
    CMP     LR, R11 /* If SVC instruction address is less than __syscalls_flash_start__, exit. */
    BLT     svcHandlerExit
    CMP     LR, R12 /* If SVC instruction address is greater than __syscalls_flash_end__, exit. */
    BGT     svcHandlerExit

    /* ---------------------------- Get Caller SVC Number ---------------------------- */

    MRS     R11, SPSR               /* LR = CPSR at the time of SVC. */
    TST     R11, #0x20              /* Check Thumb bit (5) in CPSR. */
    LDRHNE  R11, [LR, #-0x2]        /* If Thumb, load halfword. */
    BICNE   R11, R11, #0xFF00       /* And extract immidiate field (i.e. SVC number). */
    LDREQ   R11, [LR, #-0x4]        /* If ARM, load word. */
    BICEQ   R11, R11, #0xFF000000   /* And extract immidiate field (i.e. SVC number). */

    /* --------------------------------- SVC Routing --------------------------------- */

    /* If SVC Number < #NUM_SYSTEM_CALLS, go to svcSystemCallEnter. */
    CMP     R11, #NUM_SYSTEM_CALLS
    BLT     svcSystemCallEnter

    /* If SVC Number == #portSVC_SYSTEM_CALL_EXIT, go to svcSystemCallExit. */
    CMP     R11, #portSVC_SYSTEM_CALL_EXIT
    BEQ     svcSystemCallExit

    /* If SVC Number == #portSVC_YIELD, go to svcPortYield. */
    CMP     R11, #portSVC_YIELD
    BEQ     svcPortYield

svcHandlerExit:
    POP     { R11-R12 }
    MOVS    PC, LR /* Copies the SPSR into the CPSR, performing the mode swap. */

svcPortYield:
    POP     { R11-R12 }
    portSAVE_CONTEXT
    BL      vTaskSwitchContext
    portRESTORE_CONTEXT

svcSystemCallExit:
    LDR     R11, =pxCurrentTCB /* R11 = &( pxCurrentTCB ). */
    LDR     R11, [R11]         /* R11 = pxCurrentTCB. */
    ADD     R11, R11, #portSYSTEM_CALL_INFO_OFFSET /* R11 now points to xSystemCallStackInfo in TCB. */

    /* Restore the user mode SP and LR. */
    LDM   R11, { R13-R14 }^

    AND     R12, R12, #0x0      /* R12 = 0. */
    STR     R12, [R11]          /* xSystemCallStackInfo.pulTaskStackPointer = NULL. */
    STR     R12, [R11, #0x4]    /* xSystemCallStackInfo.pulLinkRegisterAtSystemCallEntry = NULL. */

    LDMDB   R11, { R12 }        /* R12 = ulTaskFlags. */

    TST     R12, #portTASK_IS_PRIVILEGED_FLAG
    /* If the task is privileged, we can exit now. */
    BNE     svcHandlerExit
    /* Otherwise, we need to switch back to User mode. */
    MRS     R12, SPSR
    BIC     R12, R12, #0x0F
    MSR     SPSR_cxsf, R12

    B   svcHandlerExit

svcSystemCallEnter:
    LDR     R12, =uxSystemCallImplementations /* R12 = uxSystemCallImplementations. */
    /* R12 = uxSystemCallImplementations[ R12 + ( R11 << 2 ) ].
     * R12 now contains the address of the system call impl function. */
    LDR     R12, [R12, R11, lsl #2]

    /* If R12 == NULL, exit. */
    CMP     R12, #0x0
    BEQ     svcHandlerExit

    /* It is okay to clobber LR here because we do not need to return to the
     * SVC enter location anymore. LR now contains the address of the system
     * call impl function. */
    MOV     LR, R12

    LDR     R11, =pxCurrentTCB  /* R11 = &( pxCurrentTCB ). */
    LDR     R11, [R11]          /* R11 = pxCurrentTCB. */
    ADD     R11, R11, #portSYSTEM_CALL_INFO_OFFSET  /* R11 now points to xSystemCallStackInfo in TCB. */

    /* Store User mode SP and LR in xSystemCallStackInfo.pulTaskStackPointer and
     * xSystemCallStackInfo.pulLinkRegisterAtSystemCallEntry. */
    STM     R11, { R13-R14 }^
    ADD     R11, R11, 0x8

    /* Load User mode SP an LR with xSystemCallStackInfo.pulSystemCallStackPointer
     * and xSystemCallStackInfo.pulSystemCallExitAddress. */
    LDM     R11, { R13-R14 }^

    /* Change to SYS_MODE for the System Call. */
    MRS     R12, SPSR
    ORR     R12, R12, #SYS_MODE
    MSR     SPSR_cxsf, R12

    B       svcHandlerExit

/* ----------------------------------------------------------------------------------- */

/*
 * void vPortDisableInterrupts( void );
 */
.align 4
.global vPortDisableInterrupts
.type vPortDisableInterrupts, %function
vPortDisableInterrupts:
    CPSID    I
    BX      LR

/* ----------------------------------------------------------------------------------- */

/*
 * void vPortEnableInterrupts( void );
 */
.align 4
.global vPortEnableInterrupts
.type vPortEnableInterrupts, %function
vPortEnableInterrupts:
    CPSIE   I
    BX      LR

/* ----------------------------------------------------------------------------------- */

/*
 * void vMPUSetRegion( uint32_t ulRegionNumber,
 *                     uint32_t ulBaseAddress,
 *                     uint32_t ulRegionSize,
 *                     uint32_t ulRegionPermissions );
 *
 * According to the Procedure Call Standard for the ARM Architecture (AAPCS),
 * paramters are passed in the following registers:
 * R0 = ulRegionNumber.
 * R1 = ulBaseAddress.
 * R2 = ulRegionSize.
 * R3 = ulRegionPermissions.
 */
.align 4
.global vMPUSetRegion
.type vMPUSetRegion, %function
vMPUSetRegion:
    AND     R0,  R0, #0x0F    /* R0 = R0 & 0x0F. Max possible region number is 15. */

    MCR     p15, #0, R0, c6, c2, #0 /* MPU Region Number Register. */
    MCR     p15, #0, R1, c6, c1, #0 /* MPU Region Base Address Register. */
    MCR     p15, #0, R3, c6, c1, #4 /* MPU Region Access Control Register. */
    MCR     p15, #0, R2, c6, c1, #2 /* MPU Region Size and Enable Register. */

    BX      LR

/* ----------------------------------------------------------------------------------- */

/*
 * void vMPUEnable( void );
 */
.align 4
.global vMPUEnable
.type vMPUEnable, %function
vMPUEnable:
    PUSH    { R0 }

    MRC     p15, #0, R0, c1, c0, #0 /* R0 = System Control Register (SCTLR). */
    ORR     R0,  R0, #0x1 /* R0 = R0 | 0x1. Set the M bit in SCTLR. */
    DSB
    MCR     p15, #0, R0, c1, c0, #0 /* SCTLR = R0. */
    ISB

    POP     { R0 }
    BX      LR

/* ----------------------------------------------------------------------------------- */

/*
 * void vMPUDisable( void );
 */
.align 4
.global vMPUDisable
.type vMPUDisable, %function
vMPUDisable:
    PUSH    { R0 }

    MRC     p15, #0, R0, c1, c0, #0 /* R0 = System Control Register (SCTLR). */
    BIC     R0,  R0, #1 /* R0 = R0 & ~0x1. Clear the M bit in SCTLR. */
    /* Wait for all pending data accesses to complete. */
    DSB
    MCR     p15, #0, R0, c1, c0, #0 /* SCTLR = R0. */
    /* Flush the pipeline and prefetch buffer(s) in the processor to ensure that
    *  all following instructions are fetched from cache or memory. */
    ISB

    POP     { R0 }
    BX      LR

/* ----------------------------------------------------------------------------------- */

/*
 * void vMPUEnableBackgroundRegion( void );
 */
.align 4
.global vMPUEnableBackgroundRegion
.type vMPUEnableBackgroundRegion, %function
vMPUEnableBackgroundRegion:
    PUSH    { R0 }

    MRC     p15, #0, R0, c1, c0, #0 /* R0 = System Control Register (SCTLR). */
    ORR     R0, R0, #0x20000 /* R0 = R0 | 0x20000. Set the BR bit in SCTLR. */
    MCR     p15, #0, R0, c1, c0, #0 /* SCTLR = R0. */

    POP     { R0 }
    BX      LR

/* ----------------------------------------------------------------------------------- */

/*
 * void vMPUDisableBackgroundRegion( void );
 */
.align 4
.global vMPUDisableBackgroundRegion
.type vMPUDisableBackgroundRegion, %function
vMPUDisableBackgroundRegion:
    PUSH    { R0 }

    MRC     p15, 0, R0, c1, c0, 0 /* R0 = System Control Register (SCTLR). */
    BIC     R0, R0, #0x20000 /* R0 = R0 & ~0x20000. Clear the BR bit in SCTLR. */
    MCR     p15, 0, R0, c1, c0, 0 /* SCTLR = R0. */

    POP     { R0 }
    BX      LR

/* ----------------------------------------------------------------------------------- */

.align 4
.global FreeRTOS_IRQ_Handler
.type FreeRTOS_IRQ_Handler, %function
FreeRTOS_IRQ_Handler:
    SUB     LR, LR, #4 /* Return to the interrupted instruction. */
    SRSDB   SP!, #IRQ_MODE /* Save return state (i.e. SPSR_irq and LR_irq) to the IRQ stack. */

    /* Change to supervisor mode to allow reentry. It is necessary to ensure
     * that a BL instruction within the interrupt handler code does not
     * overwrite LR_irq. */
    CPS     #SVC_MODE

    PUSH    { R0-R3, R12 } /* Push AAPCS callee saved registers. */

    /* Update interrupt nesting count. */
    LDR     R0, =ulPortInterruptNesting /* R0 = &( ulPortInterruptNesting ). */
    LDR     R1, [R0] /* R1 = ulPortInterruptNesting. */
    ADD     R2, R1, #1 /* R2 = R1 + 1. */
    STR     R2, [R0] /* Store the updated nesting count. */

    /* Call the application provided IRQ handler. */
    PUSH    { R0-R3, LR }
    BL      vApplicationIRQHandler
    POP     { R0-R3, LR }

    /* Disable IRQs incase vApplicationIRQHandler enabled them for re-entry. */
    CPSID   I
    DSB
    ISB

    /* Restore the old interrupt nesting count. R0 holds the address of
     * ulPortInterruptNesting and R1 holds original value of
     * ulPortInterruptNesting. */
    STR     R1, [R0]

    /* Context switch is only performed when interrupt nesting count is 0. */
    CMP     R1, #0
    BNE     exit_without_switch

    /* Check ulPortInterruptNesting to see if the interrupt requested a context
     * switch. */
    LDR     R1, =ulPortYieldRequired /* R1 = &( ulPortYieldRequired ). */
    LDR     R0, [R1] /* R0 = ulPortYieldRequired. */
    /* If ulPortYieldRequired != 0, goto switch_before_exit. */
    CMP     R0, #0
    BNE     switch_before_exit

exit_without_switch:
    POP     { R0-R3, R12 } /* Restore AAPCS callee saved registers. */
    CPS     #IRQ_MODE
    RFE     SP!

switch_before_exit:
    /* A context switch is to be performed. Clear ulPortYieldRequired. R1 holds
     * the address of ulPortYieldRequired. */
    MOV     R0, #0
    STR     R0, [R1]

    /* Restore AAPCS callee saved registers, SPSR_irq and LR_irq before saving
     * the task context. */
    POP     { R0-R3, R12 }
    CPS     #IRQ_MODE
    /* The contents of the IRQ stack at this point is the following:
     *       +----------+
     *  SP+4 | SPSR_irq |
     *       +----------+
     *    SP |  LR_irq  |
     *       +----------+
     */
    LDMIB   SP!, { LR }
    MSR     SPSR_cxsf, LR
    LDMDB   SP, { LR }
    ADD     SP, SP, 0x4
    portSAVE_CONTEXT

    /* Call the function that selects the new task to execute. */
    BLX     vTaskSwitchContext

    /* Restore the context of, and branch to, the task selected to execute
     * next. */
    portRESTORE_CONTEXT

/* ----------------------------------------------------------------------------------- */

.end
