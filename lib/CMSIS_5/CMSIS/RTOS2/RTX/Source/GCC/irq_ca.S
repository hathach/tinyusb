/*
 * Copyright (c) 2013-2018 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------------
 *
 * Project:     CMSIS-RTOS RTX
 * Title:       Cortex-A Exception handlers
 *
 * -----------------------------------------------------------------------------
 */

                .syntax  unified

                .equ   MODE_FIQ,        0x11
                .equ   MODE_IRQ,        0x12
                .equ   MODE_SVC,        0x13
                .equ   MODE_ABT,        0x17
                .equ   MODE_UND,        0x1B

                .equ   CPSR_BIT_T,      0x20

                .equ   K_STATE_RUNNING, 2           // osKernelState_t::osKernelRunning
                .equ   I_K_STATE_OFS,   8           // osRtxInfo.kernel.state offset
                .equ   I_TICK_IRQN_OFS, 16          // osRtxInfo.tick_irqn offset
                .equ   I_T_RUN_OFS,     20          // osRtxInfo.thread.run offset
                .equ   TCB_SP_FRAME,    34          // osRtxThread_t.stack_frame offset
                .equ   TCB_SP_OFS,      56          // osRtxThread_t.sp offset


                .section ".rodata"
                .global  irqRtxLib                  // Non weak library reference
irqRtxLib:
                .byte    0

                .section ".data"
                .global  IRQ_PendSV
IRQ_NestLevel:
                .word    0                          // IRQ nesting level counter
IRQ_PendSV:
                .byte    0                          // Pending SVC flag

                .arm
                .section ".text"
                .align   4


                .type    Undef_Handler, %function
                .global  Undef_Handler
                .fnstart
                .cantunwind
Undef_Handler:

                SRSFD   SP!, #MODE_UND
                PUSH    {R0-R4, R12}                // Save APCS corruptible registers to UND mode stack

                MRS     R0, SPSR
                TST     R0, #CPSR_BIT_T             // Check mode
                MOVEQ   R1, #4                      // R1 = 4 ARM mode
                MOVNE   R1, #2                      // R1 = 2 Thumb mode
                SUB     R0, LR, R1
                LDREQ   R0, [R0]                    // ARM mode - R0 points to offending instruction
                BEQ     Undef_Cont

                // Thumb instruction
                // Determine if it is a 32-bit Thumb instruction
                LDRH    R0, [R0]
                MOV     R2, #0x1C
                CMP     R2, R0, LSR #11
                BHS     Undef_Cont                  // 16-bit Thumb instruction

                // 32-bit Thumb instruction. Unaligned - reconstruct the offending instruction
                LDRH    R2, [LR]
                ORR     R0, R2, R0, LSL #16
Undef_Cont:
                MOV     R2, LR                      // Set LR to third argument

                AND     R12, SP, #4                 // Ensure stack is 8-byte aligned
                SUB     SP, SP, R12                 // Adjust stack
                PUSH    {R12, LR}                   // Store stack adjustment and dummy LR

                // R0 =Offending instruction, R1 =2(Thumb) or =4(ARM)
                BL      CUndefHandler

                POP     {R12, LR}                   // Get stack adjustment & discard dummy LR
                ADD     SP, SP, R12                 // Unadjust stack

                LDR     LR, [SP, #24]               // Restore stacked LR and possibly adjust for retry
                SUB     LR, LR, R0
                LDR     R0, [SP, #28]               // Restore stacked SPSR
                MSR     SPSR_cxsf, R0
                CLREX                               // Clear exclusive monitor
                POP     {R0-R4, R12}                // Restore stacked APCS registers
                ADD     SP, SP, #8                  // Adjust SP for already-restored banked registers
                MOVS    PC, LR

                .fnend
                .size    Undef_Handler, .-Undef_Handler


                .type    PAbt_Handler, %function
                .global  PAbt_Handler
                .fnstart
                .cantunwind
PAbt_Handler:

                SUB     LR, LR, #4                  // Pre-adjust LR
                SRSFD   SP!, #MODE_ABT              // Save LR and SPRS to ABT mode stack
                PUSH    {R0-R4, R12}                // Save APCS corruptible registers to ABT mode stack
                MRC     p15, 0, R0, c5, c0, 1       // IFSR
                MRC     p15, 0, R1, c6, c0, 2       // IFAR

                MOV     R2, LR                      // Set LR to third argument

                AND     R12, SP, #4                 // Ensure stack is 8-byte aligned
                SUB     SP, SP, R12                 // Adjust stack
                PUSH    {R12, LR}                   // Store stack adjustment and dummy LR

                BL      CPAbtHandler

                POP     {R12, LR}                   // Get stack adjustment & discard dummy LR
                ADD     SP, SP, R12                 // Unadjust stack

                CLREX                               // Clear exclusive monitor
                POP     {R0-R4, R12}                // Restore stack APCS registers
                RFEFD   SP!                         // Return from exception

                .fnend
                .size    PAbt_Handler, .-PAbt_Handler


                .type    DAbt_Handler, %function
                .global  DAbt_Handler
                .fnstart
                .cantunwind
DAbt_Handler:
                SUB     LR, LR, #8                  // Pre-adjust LR
                SRSFD   SP!, #MODE_ABT              // Save LR and SPRS to ABT mode stack
                PUSH    {R0-R4, R12}                // Save APCS corruptible registers to ABT mode stack
                MRC     p15, 0, R0, c5, c0, 0       // DFSR
                MRC     p15, 0, R1, c6, c0, 0       // DFAR

                MOV     R2, LR                      // Set LR to third argument

                AND     R12, SP, #4                 // Ensure stack is 8-byte aligned
                SUB     SP, SP, R12                 // Adjust stack
                PUSH    {R12, LR}                   // Store stack adjustment and dummy LR

                BL      CDAbtHandler

                POP     {R12, LR}                   // Get stack adjustment & discard dummy LR
                ADD     SP, SP, R12                 // Unadjust stack

                CLREX                               // Clear exclusive monitor
                POP     {R0-R4, R12}                // Restore stacked APCS registers
                RFEFD   SP!                         // Return from exception

                .fnend
                .size    DAbt_Handler, .-DAbt_Handler


                .type    IRQ_Handler, %function
                .global  IRQ_Handler
                .fnstart
                .cantunwind
IRQ_Handler:

                SUB     LR, LR, #4                  // Pre-adjust LR
                SRSFD   SP!, #MODE_SVC              // Save LR_irq and SPSR_irq on to the SVC stack
                CPS     #MODE_SVC                   // Change to SVC mode
                PUSH    {R0-R3, R12, LR}            // Save APCS corruptible registers

                LDR     R0, =IRQ_NestLevel
                LDR     R1, [R0]
                ADD     R1, R1, #1                  // Increment IRQ nesting level
                STR     R1, [R0]

                MOV     R3, SP                      // Move SP into R3
                AND     R3, R3, #4                  // Get stack adjustment to ensure 8-byte alignment
                SUB     SP, SP, R3                  // Adjust stack
                PUSH    {R3, R4}                    // Store stack adjustment(R3) and user data(R4)

                BLX     IRQ_GetActiveIRQ            // Retrieve interrupt ID into R0
                MOV     R4, R0                      // Move interrupt ID to R4

                BLX     IRQ_GetHandler              // Retrieve interrupt handler address for current ID
                CMP     R0, #0                      // Check if handler address is 0
                BEQ     IRQ_End                     // If 0, end interrupt and return

                CPSIE   i                           // Re-enable interrupts
                BLX     R0                          // Call IRQ handler
                CPSID   i                           // Disable interrupts

IRQ_End:
                MOV     R0, R4                      // Move interrupt ID to R0
                BLX     IRQ_EndOfInterrupt          // Signal end of interrupt

                POP     {R3, R4}                    // Restore stack adjustment(R3) and user data(R4)
                ADD     SP, SP, R3                  // Unadjust stack

                BL      osRtxContextSwitch          // Continue in context switcher

                LDR     R0, =IRQ_NestLevel
                LDR     R1, [R0]
                SUBS    R1, R1, #1                  // Decrement IRQ nesting level
                STR     R1, [R0]

                CLREX                               // Clear exclusive monitor for interrupted code
                POP     {R0-R3, R12, LR}            // Restore stacked APCS registers
                RFEFD   SP!                         // Return from IRQ handler

                .fnend
                .size    IRQ_Handler, .-IRQ_Handler


                .type    SVC_Handler, %function
                .global  SVC_Handler
                .fnstart
                .cantunwind
SVC_Handler:

                SRSFD   SP!, #MODE_SVC              // Store SPSR_svc and LR_svc onto SVC stack
                PUSH    {R12, LR}

                MRS     R12, SPSR                   // Load SPSR
                TST     R12, #CPSR_BIT_T            // Thumb bit set?
                LDRHNE  R12, [LR,#-2]               // Thumb: load halfword
                BICNE   R12, R12, #0xFF00           //        extract SVC number
                LDREQ   R12, [LR,#-4]               // ARM:   load word
                BICEQ   R12, R12, #0xFF000000       //        extract SVC number
                CMP     R12, #0                     // Compare SVC number
                BNE     SVC_User                    // Branch if User SVC

                PUSH    {R0-R3}

                LDR     R0, =IRQ_NestLevel
                LDR     R1, [R0]
                ADD     R1, R1, #1                  // Increment IRQ nesting level
                STR     R1, [R0]

                LDR     R0, =osRtxInfo
                LDR     R1, [R0, #I_K_STATE_OFS]    // Load RTX5 kernel state
                CMP     R1, #K_STATE_RUNNING        // Check osKernelRunning
                BLT     SVC_FuncCall                // Continue if kernel is not running
                LDR     R0, [R0, #I_TICK_IRQN_OFS]  // Load OS Tick irqn
                BLX     IRQ_Disable                 // Disable OS Tick interrupt
SVC_FuncCall:
                POP     {R0-R3}

                LDR     R12, [SP]                   // Reload R12 from stack

                CPSIE   i                           // Re-enable interrupts
                BLX     R12                         // Branch to SVC function
                CPSID   i                           // Disable interrupts

                SUB     SP, SP, #4
                STM     SP, {SP}^                   // Store SP_usr onto stack
                POP     {R12}                       // Pop SP_usr into R12
                SUB     R12, R12, #16               // Adjust pointer to SP_usr
                LDMDB   R12, {R2,R3}                // Load return values from SVC function
                PUSH    {R0-R3}                     // Push return values to stack

                LDR     R0, =osRtxInfo
                LDR     R1, [R0, #I_K_STATE_OFS]    // Load RTX5 kernel state
                CMP     R1, #K_STATE_RUNNING        // Check osKernelRunning
                BLT     SVC_ContextCheck            // Continue if kernel is not running
                LDR     R0, [R0, #I_TICK_IRQN_OFS]  // Load OS Tick irqn
                BLX     IRQ_Enable                  // Enable OS Tick interrupt

SVC_ContextCheck:
                BL      osRtxContextSwitch          // Continue in context switcher

                LDR     R0, =IRQ_NestLevel
                LDR     R1, [R0]
                SUB     R1, R1, #1                  // Decrement IRQ nesting level
                STR     R1, [R0]

                CLREX                               // Clear exclusive monitor
                POP     {R0-R3, R12, LR}            // Restore stacked APCS registers
                RFEFD   SP!                         // Return from exception

SVC_User:
                PUSH    {R4, R5}
                LDR     R5,=osRtxUserSVC            // Load address of SVC table
                LDR     R4,[R5]                     // Load SVC maximum number
                CMP     R12,R4                      // Check SVC number range
                BHI     SVC_Done                    // Branch if out of range

                LDR     R12,[R5,R12,LSL #2]         // Load SVC Function Address
                BLX     R12                         // Call SVC Function

SVC_Done:
                CLREX                               // Clear exclusive monitor
                POP     {R4, R5, R12, LR}
                RFEFD   SP!                         // Return from exception

                .fnend
                .size    SVC_Handler, .-SVC_Handler


                .type    osRtxContextSwitch, %function
                .global  osRtxContextSwitch
                .fnstart
                .cantunwind
osRtxContextSwitch:

                PUSH    {LR}

                // Check interrupt nesting level
                LDR     R0, =IRQ_NestLevel
                LDR     R1, [R0]                    // Load IRQ nest level
                CMP     R1, #1
                BNE     osRtxContextExit            // Nesting interrupts, exit context switcher

                LDR     R12, =osRtxInfo+I_T_RUN_OFS // Load address of osRtxInfo.run
                LDM     R12, {R0, R1}               // Load osRtxInfo.thread.run: curr & next
                LDR     R2, =IRQ_PendSV             // Load address of IRQ_PendSV flag
                LDRB    R3, [R2]                    // Load PendSV flag

                CMP     R0, R1                      // Check if context switch is required
                BNE     osRtxContextCheck           // Not equal, check if context save required
                CMP     R3, #1                      // Compare IRQ_PendSV value
                BNE     osRtxContextExit            // No post processing (and no context switch requested)

osRtxContextCheck:
                STR     R1, [R12]                   // Store run.next as run.curr
                // R0 = curr, R1 = next, R2 = &IRQ_PendSV, R3 = IRQ_PendSV, R12 = &osRtxInfo.thread.run
                PUSH    {R1-R3, R12}

                CMP     R0, #0                      // Is osRtxInfo.thread.run.curr == 0
                BEQ     osRtxPostProcess            // Current deleted, skip context save

osRtxContextSave:
                MOV     LR, R0                      // Move &osRtxInfo.thread.run.curr to LR
                MOV     R0, SP                      // Move SP_svc into R0
                ADD     R0, R0, #20                 // Adjust SP_svc to R0 of the basic frame
                SUB     SP, SP, #4
                STM     SP, {SP}^                   // Save SP_usr to current stack
                POP     {R1}                        // Pop SP_usr into R1

                SUB     R1, R1, #64                 // Adjust SP_usr to R4 of the basic frame
                STMIA   R1!, {R4-R11}               // Save R4-R11 to user stack
                LDMIA   R0!, {R4-R8}                // Load stacked R0-R3,R12 into R4-R8
                STMIA   R1!, {R4-R8}                // Store them to user stack
                STM     R1, {LR}^                   // Store LR_usr directly
                ADD     R1, R1, #4                  // Adjust user sp to PC
                LDMIB   R0!, {R5-R6}                // Load current PC, CPSR
                STMIA   R1!, {R5-R6}                // Restore user PC and CPSR

                SUB     R1, R1, #64                 // Adjust SP_usr to stacked R4

                // Check if VFP state need to be saved
                MRC     p15, 0, R2, c1, c0, 2       // VFP/NEON access enabled? (CPACR)
                AND     R2, R2, #0x00F00000
                CMP     R2, #0x00F00000
                BNE     osRtxContextSave1           // Continue, no VFP

                VMRS    R2, FPSCR
                STMDB   R1!, {R2,R12}               // Push FPSCR, maintain 8-byte alignment

                VSTMDB  R1!, {D0-D15}               // Save D0-D15
                #if     __ARM_NEON == 1
                VSTMDB  R1!, {D16-D31}              // Save D16-D31
                #endif

                LDRB    R2, [LR, #TCB_SP_FRAME]     // Load osRtxInfo.thread.run.curr frame info
                #if     __ARM_NEON == 1
                ORR     R2, R2, #4                  // NEON state
                #else
                ORR     R2, R2, #2                  // VFP state
                #endif
                STRB    R2, [LR, #TCB_SP_FRAME]     // Store VFP/NEON state

osRtxContextSave1:
                STR     R1, [LR, #TCB_SP_OFS]       // Store user sp to osRtxInfo.thread.run.curr

osRtxPostProcess:
                // RTX IRQ post processing check
                POP     {R8-R11}                    // Pop R8 = run.next, R9 = &IRQ_PendSV, R10 = IRQ_PendSV, R11 = &osRtxInfo.thread.run
                CMP     R10, #1                     // Compare PendSV value
                BNE     osRtxContextRestore         // Skip post processing if not pending

                MOV     R4, SP                      // Move SP_svc into R4
                AND     R4, R4, #4                  // Get stack adjustment to ensure 8-byte alignment
                SUB     SP, SP, R4                  // Adjust stack

                // Disable OS Tick
                LDR     R5, =osRtxInfo              // Load address of osRtxInfo
                LDR     R5, [R5, #I_TICK_IRQN_OFS]  // Load OS Tick irqn
                MOV     R0, R5                      // Set it as function parameter
                BLX     IRQ_Disable                 // Disable OS Tick interrupt
                MOV     R6, #0                      // Set PendSV clear value
                B       osRtxPendCheck
osRtxPendExec:
                STRB    R6, [R9]                    // Clear PendSV flag
                CPSIE   i                           // Re-enable interrupts
                BLX     osRtxPendSV_Handler         // Post process pending objects
                CPSID   i                           // Disable interrupts
osRtxPendCheck:
                LDR     R8, [R11, #4]               // Load osRtxInfo.thread.run.next
                STR     R8, [R11]                   // Store run.next as run.curr
                LDRB    R0, [R9]                    // Load PendSV flag
                CMP     R0, #1                      // Compare PendSV value
                BEQ     osRtxPendExec               // Branch to PendExec if PendSV is set

                // Re-enable OS Tick
                MOV     R0, R5                      // Restore irqn as function parameter
                BLX     IRQ_Enable                  // Enable OS Tick interrupt

                ADD     SP, SP, R4                  // Restore stack adjustment

osRtxContextRestore:
                LDR     LR, [R8, #TCB_SP_OFS]       // Load next osRtxThread_t.sp
                LDRB    R2, [R8, #TCB_SP_FRAME]     // Load next osRtxThread_t.stack_frame

                ANDS    R2, R2, #0x6                // Check stack frame for VFP context
                MRC     p15, 0, R2, c1, c0, 2       // Read CPACR
                ANDEQ   R2, R2, #0xFF0FFFFF         // VFP/NEON state not stacked, disable VFP/NEON
                ORRNE   R2, R2, #0x00F00000         // VFP/NEON state is stacked, enable VFP/NEON
                MCR     p15, 0, R2, c1, c0, 2       // Write CPACR
                BEQ     osRtxContextRestore1        // No VFP
                ISB                                 // Sync if VFP was enabled
                #if     __ARM_NEON == 1
                VLDMIA  LR!, {D16-D31}              // Restore D16-D31
                #endif
                VLDMIA  LR!, {D0-D15}               // Restore D0-D15
                LDR     R2, [LR]
                VMSR    FPSCR, R2                   // Restore FPSCR
                ADD     LR, LR, #8                  // Adjust sp pointer to R4

osRtxContextRestore1:
                LDMIA   LR!, {R4-R11}               // Restore R4-R11
                ADD     R12, LR, #32                // Adjust sp and save it into R12
                PUSH    {R12}                       // Push sp onto stack
                LDM     SP, {SP}^                   // Restore SP_usr directly
                ADD     SP, SP, #4                  // Adjust SP_svc
                LDMIA   LR!, {R0-R3, R12}           // Load user registers R0-R3,R12
                STMIB   SP!, {R0-R3, R12}           // Store them to SP_svc
                LDM     LR, {LR}^                   // Restore LR_usr directly
                LDMIB   LR!, {R0-R1}                // Load user registers PC,CPSR
                ADD     SP, SP, #4
                STMIB   SP!, {R0-R1}                // Store them to SP_svc
                SUB     SP, SP, #32                 // Adjust SP_svc to stacked LR

osRtxContextExit:
                POP     {PC}                        // Return

                .fnend
                .size    osRtxContextSwitch, .-osRtxContextSwitch

                .end
