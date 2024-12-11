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

/* FreeRTOS includes. */
#include "FreeRTOSConfig.h"

/* Xilinx library includes. */
#include "microblaze_exceptions_g.h"
#include "xparameters.h"

#include "microblaze_instructions.h"
/* The context is oversized to allow functions called from the ISR to write
back into the caller stack. */
#if defined (__arch64__)
#if( XPAR_MICROBLAZE_USE_FPU != 0 )
	#define portCONTEXT_SIZE 272
	#define portMINUS_CONTEXT_SIZE -272
#else
	#define portCONTEXT_SIZE 264
	#define portMINUS_CONTEXT_SIZE -264
#endif
#else
#if( XPAR_MICROBLAZE_USE_FPU != 0 )
	#define portCONTEXT_SIZE 136
	#define portMINUS_CONTEXT_SIZE -136
#else
	#define portCONTEXT_SIZE 132
	#define portMINUS_CONTEXT_SIZE -132
#endif
#endif

/* Offsets from the stack pointer at which saved registers are placed. */
#if defined (__arch64__)
#define portR31_OFFSET	8
#define portR30_OFFSET	16
#define portR29_OFFSET	24
#define portR28_OFFSET	32
#define portR27_OFFSET	40
#define portR26_OFFSET	48
#define portR25_OFFSET	56
#define portR24_OFFSET	64
#define portR23_OFFSET	72
#define portR22_OFFSET	80
#define portR21_OFFSET	88
#define portR20_OFFSET	96
#define portR19_OFFSET	104
#define portR18_OFFSET	112
#define portR17_OFFSET	120
#define portR16_OFFSET	128
#define portR15_OFFSET	136
#define portR14_OFFSET	144
#define portR13_OFFSET	152
#define portR12_OFFSET	160
#define portR11_OFFSET	168
#define portR10_OFFSET	176
#define portR9_OFFSET	184
#define portR8_OFFSET	192
#define portR7_OFFSET	200
#define portR6_OFFSET	208
#define portR5_OFFSET	216
#define portR4_OFFSET	224
#define portR3_OFFSET	232
#define portR2_OFFSET	240
#define portCRITICAL_NESTING_OFFSET 248
#define portMSR_OFFSET 256
#define portFSR_OFFSET 264
#else
#define portR31_OFFSET	4
#define portR30_OFFSET	8
#define portR29_OFFSET	12
#define portR28_OFFSET	16
#define portR27_OFFSET	20
#define portR26_OFFSET	24
#define portR25_OFFSET	28
#define portR24_OFFSET	32
#define portR23_OFFSET	36
#define portR22_OFFSET	40
#define portR21_OFFSET	44
#define portR20_OFFSET	48
#define portR19_OFFSET	52
#define portR18_OFFSET	56
#define portR17_OFFSET	60
#define portR16_OFFSET	64
#define portR15_OFFSET	68
#define portR14_OFFSET	72
#define portR13_OFFSET	76
#define portR12_OFFSET	80
#define portR11_OFFSET	84
#define portR10_OFFSET	88
#define portR9_OFFSET	92
#define portR8_OFFSET	96
#define portR7_OFFSET	100
#define portR6_OFFSET	104
#define portR5_OFFSET	108
#define portR4_OFFSET	112
#define portR3_OFFSET	116
#define portR2_OFFSET	120
#define portCRITICAL_NESTING_OFFSET 124
#define portMSR_OFFSET 128
#define portFSR_OFFSET 132

#endif

    .extern pxCurrentTCB
    .extern XIntc_DeviceInterruptHandler
    .extern vTaskSwitchContext
    .extern uxCriticalNesting
    .extern pulISRStack
    .extern ulTaskSwitchRequested
    .extern vPortExceptionHandler
    .extern pulStackPointerOnFunctionEntry

    .global _interrupt_handler
    .global VPortYieldASM
    .global vPortStartFirstTask
    .global vPortExceptionHandlerEntry


.macro portSAVE_CONTEXT

	/* Make room for the context on the stack. */
	ADDLIK r1, r1, portMINUS_CONTEXT_SIZE

	/* Stack general registers. */
	SI r31, r1, portR31_OFFSET
	SI r30, r1, portR30_OFFSET
	SI r29, r1, portR29_OFFSET
	SI r28, r1, portR28_OFFSET
	SI r27, r1, portR27_OFFSET
	SI r26, r1, portR26_OFFSET
	SI r25, r1, portR25_OFFSET
	SI r24, r1, portR24_OFFSET
	SI r23, r1, portR23_OFFSET
	SI r22, r1, portR22_OFFSET
	SI r21, r1, portR21_OFFSET
	SI r20, r1, portR20_OFFSET
	SI r19, r1, portR19_OFFSET
	SI r18, r1, portR18_OFFSET
	SI r17, r1, portR17_OFFSET
	SI r16, r1, portR16_OFFSET
	SI r15, r1, portR15_OFFSET
	/* R14 is saved later as it needs adjustment if a yield is performed. */
	SI r13, r1, portR13_OFFSET
	SI r12, r1, portR12_OFFSET
	SI r11, r1, portR11_OFFSET
	SI r10, r1, portR10_OFFSET
	SI r9, r1, portR9_OFFSET
	SI r8, r1, portR8_OFFSET
	SI r7, r1, portR7_OFFSET
	SI r6, r1, portR6_OFFSET
	SI r5, r1, portR5_OFFSET
	SI r4, r1, portR4_OFFSET
	SI r3, r1, portR3_OFFSET
	SI r2, r1, portR2_OFFSET

	/* Stack the critical section nesting value. */
	LI r18, r0, uxCriticalNesting
	SI r18, r1, portCRITICAL_NESTING_OFFSET

	/* Stack MSR. */
	mfs r18, rmsr
	SI r18, r1, portMSR_OFFSET

	#if( XPAR_MICROBLAZE_USE_FPU != 0 )
		/* Stack FSR. */
		mfs r18, rfsr
		SI r18, r1, portFSR_OFFSET
	#endif

#if( XPAR_MICROBLAZE_USE_STACK_PROTECTION )
    /* Save the stack limits */
    mfs r18, rslr
    swi r18, r1, portSLR_OFFSET
    mfs r18, rshr
    swi r18, r1, portSHR_OFFSET
#endif

	/* Save the top of stack value to the TCB. */
	LI r3, r0, pxCurrentTCB
	STORE	r1, r0, r3

    .endm

.macro portRESTORE_CONTEXT

	/* Load the top of stack value from the TCB. */
	LI r18, r0, pxCurrentTCB
	LOAD	r1, r0, r18

#if( XPAR_MICROBLAZE_USE_STACK_PROTECTION )
    /* Restore the stack limits -- must not load from r1 (Stack Pointer)
    because if the address of load or store instruction is out of range,
    it will trigger Stack Protection Violation exception. */
    or  r18, r0, r1
    lwi r12, r18, portSLR_OFFSET
    mts rslr, r12
    lwi r12, r18, portSHR_OFFSET
    mts rshr, r12
#endif

	/* Restore the general registers. */
	LI r31, r1, portR31_OFFSET
	LI r30, r1, portR30_OFFSET
	LI r29, r1, portR29_OFFSET
	LI r28, r1, portR28_OFFSET
	LI r27, r1, portR27_OFFSET
	LI r26, r1, portR26_OFFSET
	LI r25, r1, portR25_OFFSET
	LI r24, r1, portR24_OFFSET
	LI r23, r1, portR23_OFFSET
	LI r22, r1, portR22_OFFSET
	LI r21, r1, portR21_OFFSET
	LI r20, r1, portR20_OFFSET
	LI r19, r1, portR19_OFFSET
	LI r17, r1, portR17_OFFSET
	LI r16, r1, portR16_OFFSET
	LI r15, r1, portR15_OFFSET
	LI r14, r1, portR14_OFFSET
	LI r13, r1, portR13_OFFSET
	LI r12, r1, portR12_OFFSET
	LI r11, r1, portR11_OFFSET
	LI r10, r1, portR10_OFFSET
	LI r9, r1, portR9_OFFSET
	LI r8, r1, portR8_OFFSET
	LI r7, r1, portR7_OFFSET
	LI r6, r1, portR6_OFFSET
	LI r5, r1, portR5_OFFSET
	LI r4, r1, portR4_OFFSET
	LI r3, r1, portR3_OFFSET
	LI r2, r1, portR2_OFFSET

	/* Reload the rmsr from the stack. */
	LI r18, r1, portMSR_OFFSET
	mts rmsr, r18

	#if( XPAR_MICROBLAZE_USE_FPU != 0 )
		/* Reload the FSR from the stack. */
		LI r18, r1, portFSR_OFFSET
		mts rfsr, r18
	#endif

	/* Load the critical nesting value. */
	LI r18, r1, portCRITICAL_NESTING_OFFSET
	SI r18, r0, uxCriticalNesting

	/* Test the critical nesting value.  If it is non zero then the task last
	exited the running state using a yield.  If it is zero, then the task
	last exited the running state through an interrupt. */
	XORI r18, r18, 0
	BNEI r18, exit_from_yield

	/* r18 was being used as a temporary.  Now restore its true value from the
	stack. */
	LI r18, r1, portR18_OFFSET

	/* Remove the stack frame. */
	ADDLIK r1, r1, portCONTEXT_SIZE

	/* Return using rtid so interrupts are re-enabled as this function is
	exited. */
	rtid r14, 0
	OR r0, r0, r0

	.endm

/* This function is used to exit portRESTORE_CONTEXT() if the task being
returned to last left the Running state by calling taskYIELD() (rather than
being preempted by an interrupt). */
	.text
#ifdef __arch64__
	.align  8
#else
        .align  4
#endif

exit_from_yield:

	/* r18 was being used as a temporary.  Now restore its true value from the
	stack. */
	LI r18, r1, portR18_OFFSET

	/* Remove the stack frame. */
	ADDLIK r1, r1, portCONTEXT_SIZE

	/* Return to the task. */
	rtsd r14, 0
	OR r0, r0, r0


	.text

#ifdef __arch64__
	.align  8
#else
        .align  4
#endif

_interrupt_handler:

    portSAVE_CONTEXT

	/* Stack the return address. */
	SI r14, r1, portR14_OFFSET

	/* Switch to the ISR stack. */
	LI r1, r0, pulISRStack

#if( XPAR_MICROBLAZE_USE_STACK_PROTECTION )
    ori r18, r0, _stack_end
    mts rslr, r18
    ori r18, r0, _stack
    mts rshr, r18
#endif

	/* The parameter to the interrupt handler. */
	ORI r5, r0, configINTERRUPT_CONTROLLER_TO_USE

	/* Execute any pending interrupts. */
	BRALID r15, XIntc_DeviceInterruptHandler
	OR r0, r0, r0

	/* See if a new task should be selected to execute. */
	LI r18, r0, ulTaskSwitchRequested
	OR r18, r18, r0

	/* If ulTaskSwitchRequested is already zero, then jump straight to
	restoring the task that is already in the Running state. */
	BEQI r18, task_switch_not_requested

	/* Set ulTaskSwitchRequested back to zero as a task switch is about to be
	performed. */
	SI r0, r0, ulTaskSwitchRequested

	/* ulTaskSwitchRequested was not 0 when tested.  Select the next task to
	execute. */
	BRALID r15, vTaskSwitchContext
	OR r0, r0, r0

task_switch_not_requested:

    /* Restore the context of the next task scheduled to execute. */
    portRESTORE_CONTEXT


	.text
#ifdef __arch64__
	.align  8
#else
        .align  4
#endif

VPortYieldASM:

    portSAVE_CONTEXT

	/* Modify the return address so a return is done to the instruction after
	the call to VPortYieldASM. */
	ADDI r14, r14, 8
	SI r14, r1, portR14_OFFSET

	/* Switch to use the ISR stack. */
	LI r1, r0, pulISRStack

#if( XPAR_MICROBLAZE_USE_STACK_PROTECTION )
    ori r18, r0, _stack_end
    mts rslr, r18
    ori r18, r0, _stack
    mts rshr, r18
#endif

	/* Select the next task to execute. */
	BRALID r15, vTaskSwitchContext
	OR r0, r0, r0

    /* Restore the context of the next task scheduled to execute. */
    portRESTORE_CONTEXT

	.text
#ifdef __arch64__
	.align  8
#else
        .align  4
#endif

vPortStartFirstTask:

    portRESTORE_CONTEXT



#if ( MICROBLAZE_EXCEPTIONS_ENABLED == 1 ) && ( configINSTALL_EXCEPTION_HANDLERS == 1 )

	.text
#ifdef __arch64__
	.align 8
#else
        .align  4
#endif

vPortExceptionHandlerEntry:

	/* Take a copy of the stack pointer before vPortExecptionHandler is called,
	storing its value prior to the function stack frame being created. */
	SI r1, r0, pulStackPointerOnFunctionEntry
	BRALID r15, vPortExceptionHandler
	OR r0, r0, r0

#endif /* ( MICROBLAZE_EXCEPTIONS_ENABLED == 1 ) && ( configINSTALL_EXCEPTION_HANDLERS == 1 ) */
