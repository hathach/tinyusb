/*
 * Copyright (c) 2016-2020 Arm Limited. All rights reserved.
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
 * Title:       ARMv8M Mainline Exception handlers
 *
 * -----------------------------------------------------------------------------
 */


        .syntax  unified

        #ifdef   _RTE_
        #include "RTE_Components.h"
        #ifdef   RTE_CMSIS_RTOS2_RTX5_ARMV8M_NS
        #define  DOMAIN_NS    1
        #endif
        #endif

        #ifndef  DOMAIN_NS
        #define  DOMAIN_NS    0
        #endif

        #if (defined(__ARM_FP) && (__ARM_FP > 0))
        .equ     FPU_USED,    1
        #else
        .equ     FPU_USED,    0
        #endif

        #if (defined(__ARM_FEATURE_MVE) && (__ARM_FEATURE_MVE > 0))
        .equ     MVE_USED,    1
        #else
        .equ     MVE_USED,    0
        #endif

        .equ     I_T_RUN_OFS, 20        // osRtxInfo.thread.run offset
        .equ     TCB_SM_OFS,  48        // TCB.stack_mem offset
        .equ     TCB_SP_OFS,  56        // TCB.SP offset
        .equ     TCB_SF_OFS,  34        // TCB.stack_frame offset
        .equ     TCB_TZM_OFS, 64        // TCB.tz_memory offset

        .section ".rodata"
        .global  irqRtxLib              // Non weak library reference
irqRtxLib:
        .byte    0


        .thumb
        .section ".text"
        .align   2


        .thumb_func
        .type    SVC_Handler, %function
        .global  SVC_Handler
        .fnstart
        .cantunwind
SVC_Handler:

        TST      LR,#0x04               // Determine return stack from EXC_RETURN bit 2
        ITE      EQ
        MRSEQ    R0,MSP                 // Get MSP if return stack is MSP
        MRSNE    R0,PSP                 // Get PSP if return stack is PSP

        LDR      R1,[R0,#24]            // Load saved PC from stack
        LDRB     R1,[R1,#-2]            // Load SVC number
        CMP      R1,#0
        BNE      SVC_User               // Branch if not SVC 0

        PUSH     {R0,LR}                // Save SP and EXC_RETURN
        LDM      R0,{R0-R3,R12}         // Load function parameters and address from stack
        BLX      R12                    // Call service function
        POP      {R12,LR}               // Restore SP and EXC_RETURN
        STM      R12,{R0-R1}            // Store function return values

SVC_Context:
        LDR      R3,=osRtxInfo+I_T_RUN_OFS // Load address of osRtxInfo.run
        LDM      R3,{R1,R2}             // Load osRtxInfo.thread.run: curr & next
        CMP      R1,R2                  // Check if thread switch is required
        IT       EQ
        BXEQ     LR                     // Exit when threads are the same

        .if      (FPU_USED == 1) || (MVE_USED == 1)
        CBNZ     R1,SVC_ContextSave     // Branch if running thread is not deleted
        TST      LR,#0x10               // Check if extended stack frame
        BNE      SVC_ContextSwitch
        LDR      R1,=0xE000EF34         // FPCCR Address
        LDR      R0,[R1]                // Load FPCCR
        BIC      R0,R0,#1               // Clear LSPACT (Lazy state)
        STR      R0,[R1]                // Store FPCCR
        B        SVC_ContextSwitch
        .else
        CBZ      R1,SVC_ContextSwitch   // Branch if running thread is deleted
        .endif

SVC_ContextSave:
        #if      (DOMAIN_NS == 1)
        LDR      R0,[R1,#TCB_TZM_OFS]   // Load TrustZone memory identifier
        CBZ      R0,SVC_ContextSave1    // Branch if there is no secure context
        PUSH     {R1,R2,R3,LR}          // Save registers and EXC_RETURN
        BL       TZ_StoreContext_S      // Store secure context
        POP      {R1,R2,R3,LR}          // Restore registers and EXC_RETURN
        #endif

SVC_ContextSave1:
        MRS      R0,PSP                 // Get PSP
        STMDB    R0!,{R4-R11}           // Save R4..R11
        .if      (FPU_USED == 1) || (MVE_USED == 1)
        TST      LR,#0x10               // Check if extended stack frame
        IT       EQ
        VSTMDBEQ R0!,{S16-S31}          //  Save VFP S16.S31
        .endif

SVC_ContextSave2:
        STR      R0,[R1,#TCB_SP_OFS]    // Store SP
        STRB     LR,[R1,#TCB_SF_OFS]    // Store stack frame information

SVC_ContextSwitch:
        STR      R2,[R3]                // osRtxInfo.thread.run: curr = next

SVC_ContextRestore:
        #if      (DOMAIN_NS == 1)
        LDR      R0,[R2,#TCB_TZM_OFS]   // Load TrustZone memory identifier
        CBZ      R0,SVC_ContextRestore1 // Branch if there is no secure context
        PUSH     {R2,R3}                // Save registers
        BL       TZ_LoadContext_S       // Load secure context
        POP      {R2,R3}                // Restore registers
        #endif

SVC_ContextRestore1:
        LDR      R0,[R2,#TCB_SM_OFS]    // Load stack memory base
        LDRB     R1,[R2,#TCB_SF_OFS]    // Load stack frame information
        MSR      PSPLIM,R0              // Set PSPLIM
        LDR      R0,[R2,#TCB_SP_OFS]    // Load SP
        ORR      LR,R1,#0xFFFFFF00      // Set EXC_RETURN

        #if      (DOMAIN_NS == 1)
        TST      LR,#0x40               // Check domain of interrupted thread
        BNE      SVC_ContextRestore2    // Branch if secure
        #endif

        .if      (FPU_USED == 1) || (MVE_USED == 1)
        TST      LR,#0x10               // Check if extended stack frame
        IT       EQ
        VLDMIAEQ R0!,{S16-S31}          //  Restore VFP S16..S31
        .endif
        LDMIA    R0!,{R4-R11}           // Restore R4..R11

SVC_ContextRestore2:
        MSR      PSP,R0                 // Set PSP

SVC_Exit:
        BX       LR                     // Exit from handler

SVC_User:
        LDR      R2,=osRtxUserSVC       // Load address of SVC table
        LDR      R3,[R2]                // Load SVC maximum number
        CMP      R1,R3                  // Check SVC number range
        BHI      SVC_Exit               // Branch if out of range

        PUSH     {R0,LR}                // Save SP and EXC_RETURN
        LDR      R12,[R2,R1,LSL #2]     // Load address of SVC function
        LDM      R0,{R0-R3}             // Load function parameters from stack
        BLX      R12                    // Call service function
        POP      {R12,LR}               // Restore SP and EXC_RETURN
        STR      R0,[R12]               // Store function return value

        BX       LR                     // Return from handler

        .fnend
        .size    SVC_Handler, .-SVC_Handler


        .thumb_func
        .type    PendSV_Handler, %function
        .global  PendSV_Handler
        .fnstart
        .cantunwind
PendSV_Handler:

        PUSH     {R0,LR}                // Save EXC_RETURN
        BL       osRtxPendSV_Handler    // Call osRtxPendSV_Handler
        POP      {R0,LR}                // Restore EXC_RETURN
        B        Sys_Context

        .fnend
        .size    PendSV_Handler, .-PendSV_Handler


        .thumb_func
        .type    SysTick_Handler, %function
        .global  SysTick_Handler
        .fnstart
        .cantunwind
SysTick_Handler:

        PUSH     {R0,LR}                // Save EXC_RETURN
        BL       osRtxTick_Handler      // Call osRtxTick_Handler
        POP      {R0,LR}                // Restore EXC_RETURN
        B        Sys_Context

        .fnend
        .size   SysTick_Handler, .-SysTick_Handler


        .thumb_func
        .type    Sys_Context, %function
        .global  Sys_Context
        .fnstart
        .cantunwind
Sys_Context:

        LDR      R3,=osRtxInfo+I_T_RUN_OFS // Load address of osRtxInfo.run
        LDM      R3,{R1,R2}             // Load osRtxInfo.thread.run: curr & next
        CMP      R1,R2                  // Check if thread switch is required
        IT       EQ
        BXEQ     LR                     // Exit when threads are the same

Sys_ContextSave:
        #if      (DOMAIN_NS == 1)
        LDR      R0,[R1,#TCB_TZM_OFS]   // Load TrustZone memory identifier
        CBZ      R0,Sys_ContextSave1    // Branch if there is no secure context
        PUSH     {R1,R2,R3,LR}          // Save registers and EXC_RETURN
        BL       TZ_StoreContext_S      // Store secure context
        POP      {R1,R2,R3,LR}          // Restore registers and EXC_RETURN

Sys_ContextSave1:
        TST      LR,#0x40               // Check domain of interrupted thread
        IT       NE
        MRSNE    R0,PSP                 // Get PSP
        BNE      Sys_ContextSave3       // Branch if secure
        #endif

Sys_ContextSave2:
        MRS      R0,PSP                 // Get PSP
        STMDB    R0!,{R4-R11}           // Save R4..R11
        .if      (FPU_USED == 1) || (MVE_USED == 1)
        TST      LR,#0x10               // Check if extended stack frame
        IT       EQ
        VSTMDBEQ R0!,{S16-S31}          //  Save VFP S16.S31
        .endif

Sys_ContextSave3:
        STR      R0,[R1,#TCB_SP_OFS]    // Store SP
        STRB     LR,[R1,#TCB_SF_OFS]    // Store stack frame information

Sys_ContextSwitch:
        STR      R2,[R3]                // osRtxInfo.run: curr = next

Sys_ContextRestore:
        #if      (DOMAIN_NS == 1)
        LDR      R0,[R2,#TCB_TZM_OFS]   // Load TrustZone memory identifier
        CBZ      R0,Sys_ContextRestore1 // Branch if there is no secure context
        PUSH     {R2,R3}                // Save registers
        BL       TZ_LoadContext_S       // Load secure context
        POP      {R2,R3}                // Restore registers
        #endif

Sys_ContextRestore1:
        LDR      R0,[R2,#TCB_SM_OFS]    // Load stack memory base
        LDRB     R1,[R2,#TCB_SF_OFS]    // Load stack frame information
        MSR      PSPLIM,R0              // Set PSPLIM
        LDR      R0,[R2,#TCB_SP_OFS]    // Load SP
        ORR      LR,R1,#0xFFFFFF00      // Set EXC_RETURN

        #if      (DOMAIN_NS == 1)
        TST      LR,#0x40               // Check domain of interrupted thread
        BNE      Sys_ContextRestore2    // Branch if secure
        #endif

        .if      (FPU_USED == 1) || (MVE_USED == 1)
        TST      LR,#0x10               // Check if extended stack frame
        IT       EQ
        VLDMIAEQ R0!,{S16-S31}          //  Restore VFP S16..S31
        .endif
        LDMIA    R0!,{R4-R11}           // Restore R4..R11

Sys_ContextRestore2:
        MSR      PSP,R0                 // Set PSP

Sys_ContextExit:
        BX       LR                     // Exit from handler

        .fnend
        .size    Sys_Context, .-Sys_Context


        .end
