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
 * Title:       Exception handlers (C functions)
 *
 * -----------------------------------------------------------------------------
 */
#include "RTE_Components.h"
#include CMSIS_device_header


//Fault Status Register (IFSR/DFSR) definitions
#define FSR_ALIGNMENT_FAULT                  0x01   //DFSR only. Fault on first lookup
#define FSR_INSTRUCTION_CACHE_MAINTENANCE    0x04   //DFSR only - async/external
#define FSR_SYNC_EXT_TTB_WALK_FIRST          0x0c   //sync/external
#define FSR_SYNC_EXT_TTB_WALK_SECOND         0x0e   //sync/external
#define FSR_SYNC_PARITY_TTB_WALK_FIRST       0x1c   //sync/external
#define FSR_SYNC_PARITY_TTB_WALK_SECOND      0x1e   //sync/external
#define FSR_TRANSLATION_FAULT_FIRST          0x05   //MMU Fault - internal
#define FSR_TRANSLATION_FAULT_SECOND         0x07   //MMU Fault - internal
#define FSR_ACCESS_FLAG_FAULT_FIRST          0x03   //MMU Fault - internal
#define FSR_ACCESS_FLAG_FAULT_SECOND         0x06   //MMU Fault - internal
#define FSR_DOMAIN_FAULT_FIRST               0x09   //MMU Fault - internal
#define FSR_DOMAIN_FAULT_SECOND              0x0b   //MMU Fault - internal
#define FSR_PERMISSION_FAULT_FIRST           0x0f   //MMU Fault - internal
#define FSR_PERMISSION_FAULT_SECOND          0x0d   //MMU Fault - internal
#define FSR_DEBUG_EVENT                      0x02   //internal
#define FSR_SYNC_EXT_ABORT                   0x08   //sync/external
#define FSR_TLB_CONFLICT_ABORT               0x10   //sync/external
#define FSR_LOCKDOWN                         0x14   //internal
#define FSR_COPROCESSOR_ABORT                0x1a   //internal
#define FSR_SYNC_PARITY_ERROR                0x19   //sync/external
#define FSR_ASYNC_EXTERNAL_ABORT             0x16   //DFSR only - async/external
#define FSR_ASYNC_PARITY_ERROR               0x18   //DFSR only - async/external

void CDAbtHandler(uint32_t DFSR, uint32_t DFAR, uint32_t LR) {
    uint32_t FS = (DFSR & (1U << 10U)) >> 6U | (DFSR & 0x0FU); //Store Fault Status
    (void)DFAR;
    (void)LR;
  
    switch(FS) {
        //Synchronous parity errors - retry
        case FSR_SYNC_PARITY_ERROR:
        case FSR_SYNC_PARITY_TTB_WALK_FIRST:
        case FSR_SYNC_PARITY_TTB_WALK_SECOND:
            return;

        //Your code here. Value in DFAR is invalid for some fault statuses.
        case FSR_ALIGNMENT_FAULT:
        case FSR_INSTRUCTION_CACHE_MAINTENANCE:
        case FSR_SYNC_EXT_TTB_WALK_FIRST:
        case FSR_SYNC_EXT_TTB_WALK_SECOND:
        case FSR_TRANSLATION_FAULT_FIRST:
        case FSR_TRANSLATION_FAULT_SECOND:
        case FSR_ACCESS_FLAG_FAULT_FIRST:
        case FSR_ACCESS_FLAG_FAULT_SECOND:
        case FSR_DOMAIN_FAULT_FIRST:
        case FSR_DOMAIN_FAULT_SECOND:
        case FSR_PERMISSION_FAULT_FIRST:
        case FSR_PERMISSION_FAULT_SECOND:
        case FSR_DEBUG_EVENT:
        case FSR_SYNC_EXT_ABORT:
        case FSR_TLB_CONFLICT_ABORT:
        case FSR_LOCKDOWN:
        case FSR_COPROCESSOR_ABORT:
        case FSR_ASYNC_EXTERNAL_ABORT: //DFAR invalid
        case FSR_ASYNC_PARITY_ERROR:   //DFAR invalid
        default:
            while(1);
    }
}

void CPAbtHandler(uint32_t IFSR, uint32_t IFAR, uint32_t LR) {
    uint32_t FS = (IFSR & (1U << 10U)) >> 6U | (IFSR & 0x0FU); //Store Fault Status
    (void)IFAR;
    (void)LR;
    
    switch(FS) {
        //Synchronous parity errors - retry
        case FSR_SYNC_PARITY_ERROR:
        case FSR_SYNC_PARITY_TTB_WALK_FIRST:
        case FSR_SYNC_PARITY_TTB_WALK_SECOND:
            return;

        //Your code here. Value in IFAR is invalid for some fault statuses.
        case FSR_SYNC_EXT_TTB_WALK_FIRST:
        case FSR_SYNC_EXT_TTB_WALK_SECOND:
        case FSR_TRANSLATION_FAULT_FIRST:
        case FSR_TRANSLATION_FAULT_SECOND:
        case FSR_ACCESS_FLAG_FAULT_FIRST:
        case FSR_ACCESS_FLAG_FAULT_SECOND:
        case FSR_DOMAIN_FAULT_FIRST:
        case FSR_DOMAIN_FAULT_SECOND:
        case FSR_PERMISSION_FAULT_FIRST:
        case FSR_PERMISSION_FAULT_SECOND:
        case FSR_DEBUG_EVENT: //IFAR invalid
        case FSR_SYNC_EXT_ABORT:
        case FSR_TLB_CONFLICT_ABORT:
        case FSR_LOCKDOWN:
        case FSR_COPROCESSOR_ABORT:
        default:
            while(1);
    }
}


//returns amount to decrement lr by
//this will be 0 when we have emulated the instruction and want to execute the next instruction
//this will be 2 when we have performed some maintenance and want to retry the instruction in Thumb (state == 2)
//this will be 4 when we have performed some maintenance and want to retry the instruction in Arm   (state == 4)
uint32_t CUndefHandler(uint32_t opcode, uint32_t state, uint32_t LR) {
    const uint32_t THUMB = 2U;
    const uint32_t ARM = 4U;
    (void)LR;
    //Lazy VFP/NEON initialisation and switching

    // (Arm Architecture Reference Manual section A7.5) VFP data processing instruction?
    // (Arm Architecture Reference Manual section A7.6) VFP/NEON register load/store instruction?
    // (Arm Architecture Reference Manual section A7.8) VFP/NEON register data transfer instruction?
    // (Arm Architecture Reference Manual section A7.9) VFP/NEON 64-bit register data transfer instruction?
    if ((state == ARM   && ((opcode & 0x0C000000U) >> 26U == 0x03U)) ||
        (state == THUMB && ((opcode & 0xEC000000U) >> 26U == 0x3BU))) {
        if (((opcode & 0x00000E00U) >> 9U) == 5U) {
            __FPU_Enable();
            return state;
        }
    }

    // (Arm Architecture Reference Manual section A7.4) NEON data processing instruction?
    if ((state == ARM   && ((opcode & 0xFE000000U) >> 24U == 0xF2U)) ||
        (state == THUMB && ((opcode & 0xEF000000U) >> 24U == 0xEFU)) ||
    // (Arm Architecture Reference Manual section A7.7) NEON load/store instruction?
        (state == ARM   && ((opcode >> 24U) == 0xF4U)) ||
        (state == THUMB && ((opcode >> 24U) == 0xF9U))) {
            __FPU_Enable();
            return state;
    }

    //Add code here for other Undef cases
    while(1);
}
