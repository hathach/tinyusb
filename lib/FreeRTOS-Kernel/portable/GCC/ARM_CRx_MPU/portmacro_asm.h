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

#ifndef PORTMACRO_ASM_H
#define PORTMACRO_ASM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOSConfig.h"

#ifndef configTOTAL_MPU_REGIONS
    #error "Set configTOTAL_MPU_REGIONS to the number of MPU regions in FreeRTOSConfig.h"
#elif( configTOTAL_MPU_REGIONS == 12 )
    #define portMPU_TOTAL_REGIONS ( 12UL )
#elif( configTOTAL_MPU_REGIONS == 16 )
    #define portMPU_TOTAL_REGIONS ( 16UL )
#else
    #error "Set configTOTAL_MPU_REGIONS to the number of MPU regions in FreeRTOSConfig.h"
#endif /* configTOTAL_MPU_REGIONS */

/*
 * The application write can disable Floating Point Unit (FPU) support by
 * setting configENABLE_FPU to 0. Floating point context stored in TCB
 * comprises of 32 floating point registers (D0-D31) and FPSCR register.
 * Disabling FPU, therefore, reduces the per-task RAM usage by
 * ( 32 + 1 ) * 4 = 132 bytes per task.
 *
 * BE CAREFUL DISABLING THIS: Certain standard library APIs try to optimize
 * themselves by using the floating point registers. If the FPU support is
 * disabled, the use of such APIs may result in memory corruption.
 */
#ifndef configENABLE_FPU
    #define configENABLE_FPU    1
#endif /* configENABLE_FPU */

#define portENABLE_FPU configENABLE_FPU

/* On the ArmV7-R Architecture the Operating mode of the Processor is set
 * using the Current Program Status Register (CPSR) Mode bits, [4:0]. The only
 * unprivileged mode is User Mode.
 *
 * Additional information about the Processor Modes can be found here:
 * https://developer.arm.com/documentation/ddi0406/cb/System-Level-Architecture/The-System-Level-Programmers--Model/ARM-processor-modes-and-ARM-core-registers/ARM-processor-modes?lang=en
 *
 */

/**
 * @brief CPSR bits for various processor modes.
 *
 * @ingroup Port Privilege
 */
#define USER_MODE   0x10U
#define FIQ_MODE    0x11U
#define IRQ_MODE    0x12U
#define SVC_MODE    0x13U
#define MON_MODE    0x16U
#define ABT_MODE    0x17U
#define HYP_MODE    0x1AU
#define UND_MODE    0x1BU
#define SYS_MODE    0x1FU

/**
 * @brief Flag used to mark that a FreeRTOS Task is privileged.
 *
 * @ingroup Port Privilege
 */
#define portTASK_IS_PRIVILEGED_FLAG   ( 1UL << 1UL )

/**
 * @brief SVC numbers for various scheduler operations.
 *
 * @ingroup Scheduler
 *
 * @note These value must not be used in mpu_syscall_numbers.h.
 */
#define portSVC_YIELD                 0x0100U
#define portSVC_SYSTEM_CALL_EXIT      0x0104U

/**
 * @brief Macros required to manipulate MPU.
 *
 * Further information about MPU can be found in Arm's documentation
 * https://developer.arm.com/documentation/ddi0363/g/System-Control/Register-descriptions/c6--MPU-memory-region-programming-registers
 *
 */

/* MPU sub-region disable settings. This information is encoded in the MPU
 * Region Size and Enable Register. */
#define portMPU_SUBREGION_0_DISABLE   ( 0x1UL << 8UL )
#define portMPU_SUBREGION_1_DISABLE   ( 0x1UL << 9UL )
#define portMPU_SUBREGION_2_DISABLE   ( 0x1UL << 10UL )
#define portMPU_SUBREGION_3_DISABLE   ( 0x1UL << 11UL )
#define portMPU_SUBREGION_4_DISABLE   ( 0x1UL << 12UL )
#define portMPU_SUBREGION_5_DISABLE   ( 0x1UL << 13UL )
#define portMPU_SUBREGION_6_DISABLE   ( 0x1UL << 14UL )
#define portMPU_SUBREGION_7_DISABLE   ( 0x1UL << 15UL )

/* Default MPU regions. */
#define portFIRST_CONFIGURABLE_REGION ( 0 )
#define portLAST_CONFIGURABLE_REGION  ( portMPU_TOTAL_REGIONS - 5UL )
#define portSTACK_REGION              ( portMPU_TOTAL_REGIONS - 4UL )
#define portUNPRIVILEGED_FLASH_REGION ( portMPU_TOTAL_REGIONS - 3UL )
#define portPRIVILEGED_FLASH_REGION   ( portMPU_TOTAL_REGIONS - 2UL )
#define portPRIVILEGED_RAM_REGION     ( portMPU_TOTAL_REGIONS - 1UL )
#define portNUM_CONFIGURABLE_REGIONS \
    ( ( portLAST_CONFIGURABLE_REGION - portFIRST_CONFIGURABLE_REGION ) + 1UL )
/* Plus one to make space for the stack region. */
#define portTOTAL_NUM_REGIONS_IN_TCB        ( portNUM_CONFIGURABLE_REGIONS + 1UL )

/* MPU region sizes. This information is encoded in the MPU Region Size and
 * Enable Register. */
#define portMPU_REGION_SIZE_32B             ( 0x04UL << 1UL )
#define portMPU_REGION_SIZE_64B             ( 0x05UL << 1UL )
#define portMPU_REGION_SIZE_128B            ( 0x06UL << 1UL )
#define portMPU_REGION_SIZE_256B            ( 0x07UL << 1UL )
#define portMPU_REGION_SIZE_512B            ( 0x08UL << 1UL )
#define portMPU_REGION_SIZE_1KB             ( 0x09UL << 1UL )
#define portMPU_REGION_SIZE_2KB             ( 0x0AUL << 1UL )
#define portMPU_REGION_SIZE_4KB             ( 0x0BUL << 1UL )
#define portMPU_REGION_SIZE_8KB             ( 0x0CUL << 1UL )
#define portMPU_REGION_SIZE_16KB            ( 0x0DUL << 1UL )
#define portMPU_REGION_SIZE_32KB            ( 0x0EUL << 1UL )
#define portMPU_REGION_SIZE_64KB            ( 0x0FUL << 1UL )
#define portMPU_REGION_SIZE_128KB           ( 0x10UL << 1UL )
#define portMPU_REGION_SIZE_256KB           ( 0x11UL << 1UL )
#define portMPU_REGION_SIZE_512KB           ( 0x12UL << 1UL )
#define portMPU_REGION_SIZE_1MB             ( 0x13UL << 1UL )
#define portMPU_REGION_SIZE_2MB             ( 0x14UL << 1UL )
#define portMPU_REGION_SIZE_4MB             ( 0x15UL << 1UL )
#define portMPU_REGION_SIZE_8MB             ( 0x16UL << 1UL )
#define portMPU_REGION_SIZE_16MB            ( 0x17UL << 1UL )
#define portMPU_REGION_SIZE_32MB            ( 0x18UL << 1UL )
#define portMPU_REGION_SIZE_64MB            ( 0x19UL << 1UL )
#define portMPU_REGION_SIZE_128MB           ( 0x1AUL << 1UL )
#define portMPU_REGION_SIZE_256MB           ( 0x1BUL << 1UL )
#define portMPU_REGION_SIZE_512MB           ( 0x1CUL << 1UL )
#define portMPU_REGION_SIZE_1GB             ( 0x1DUL << 1UL )
#define portMPU_REGION_SIZE_2GB             ( 0x1EUL << 1UL )
#define portMPU_REGION_SIZE_4GB             ( 0x1FUL << 1UL )

/* MPU memory types. This information is encoded in the TEX, S, C and B bits
 * of the MPU Region Access Control Register. */
#define portMPU_REGION_STRONGLY_ORDERED_SHAREABLE  ( 0x00UL ) /* TEX=000, S=NA, C=0, B=0. */
#define portMPU_REGION_DEVICE_SHAREABLE            ( 0x01UL ) /* TEX=000, S=NA, C=0, B=1. */
#define portMPU_REGION_NORMAL_OIWTNOWA_NONSHARED   ( 0x02UL ) /* TEX=000, S=0, C=1, B=0. */
#define portMPU_REGION_NORMAL_OIWTNOWA_SHARED      ( 0x06UL ) /* TEX=000, S=1, C=1, B=0. */
#define portMPU_REGION_NORMAL_OIWBNOWA_NONSHARED   ( 0x03UL ) /* TEX=000, S=0, C=1, B=1. */
#define portMPU_REGION_NORMAL_OIWBNOWA_SHARED      ( 0x07UL ) /* TEX=000, S=1, C=1, B=1. */
#define portMPU_REGION_NORMAL_OINC_NONSHARED       ( 0x08UL ) /* TEX=001, S=0, C=0, B=0. */
#define portMPU_REGION_NORMAL_OINC_SHARED          ( 0x0CUL ) /* TEX=001, S=1, C=0, B=0. */
#define portMPU_REGION_NORMAL_OIWBWA_NONSHARED     ( 0x0BUL ) /* TEX=001, S=0, C=1, B=1. */
#define portMPU_REGION_NORMAL_OIWBWA_SHARED        ( 0x0FUL ) /* TEX=001, S=1, C=1, B=1. */
#define portMPU_REGION_DEVICE_NONSHAREABLE         ( 0x10UL ) /* TEX=010, S=NA, C=0, B=0. */

/* MPU access permissions. This information is encoded in the XN and AP bits of
 * the MPU Region Access Control Register. */
#define portMPU_REGION_AP_BITMASK                  ( 0x07UL << 8UL )
#define portMPU_REGION_XN_BITMASK                  ( 0x01UL << 12UL )

#define portMPU_REGION_PRIV_NA_USER_NA             ( 0x00UL << 8UL )
#define portMPU_REGION_PRIV_NA_USER_NA_EXEC        ( portMPU_REGION_PRIV_NA_USER_NA ) /* Priv: X, Unpriv: X. */
#define portMPU_REGION_PRIV_NA_USER_NA_NOEXEC      ( portMPU_REGION_PRIV_NA_USER_NA | \
                                                     portMPU_REGION_XN_BITMASK ) /* Priv: No Access, Unpriv: No Access. */

#define portMPU_REGION_PRIV_RW_USER_NA             ( 0x01UL << 8UL )
#define portMPU_REGION_PRIV_RW_USER_NA_EXEC        ( portMPU_REGION_PRIV_RW_USER_NA ) /* Priv: RWX, Unpriv: X. */
#define portMPU_REGION_PRIV_RW_USER_NA_NOEXEC      ( portMPU_REGION_PRIV_RW_USER_NA | \
                                                     portMPU_REGION_XN_BITMASK ) /* Priv: RW, Unpriv: No access. */

#define portMPU_REGION_PRIV_RW_USER_RO             ( 0x02UL << 8UL )
#define portMPU_REGION_PRIV_RW_USER_RO_EXEC        ( portMPU_REGION_PRIV_RW_USER_RO ) /* Priv: RWX, Unpriv: RX. */
#define portMPU_REGION_PRIV_RW_USER_RO_NOEXEC      ( portMPU_REGION_PRIV_RW_USER_RO | \
                                                     portMPU_REGION_XN_BITMASK ) /* Priv: RW, Unpriv: R. */

#define portMPU_REGION_PRIV_RW_USER_RW             ( 0x03UL << 8UL )
#define portMPU_REGION_PRIV_RW_USER_RW_EXEC        ( portMPU_REGION_PRIV_RW_USER_RW ) /* Priv: RWX, Unpriv: RWX. */
#define portMPU_REGION_PRIV_RW_USER_RW_NOEXEC      ( portMPU_REGION_PRIV_RW_USER_RW | \
                                                     portMPU_REGION_XN_BITMASK ) /* Priv: RW, Unpriv: RW. */

#define portMPU_REGION_PRIV_RO_USER_NA             ( 0x05UL << 8UL )
#define portMPU_REGION_PRIV_RO_USER_NA_EXEC        ( portMPU_REGION_PRIV_RO_USER_NA ) /* Priv: RX, Unpriv: X. */
#define portMPU_REGION_PRIV_RO_USER_NA_NOEXEC      ( portMPU_REGION_PRIV_RO_USER_NA | \
                                                     portMPU_REGION_XN_BITMASK ) /* Priv: R, Unpriv: No access. */

#define portMPU_REGION_PRIV_RO_USER_RO             ( 0x06UL << 8UL )
#define portMPU_REGION_PRIV_RO_USER_RO_EXEC        ( portMPU_REGION_PRIV_RO_USER_RO ) /* Priv: RX, Unpriv: RX. */
#define portMPU_REGION_PRIV_RO_USER_RO_NOEXEC      ( portMPU_REGION_PRIV_RO_USER_RO | \
                                                     portMPU_REGION_XN_BITMASK ) /* Priv: R, Unpriv: R. */

/* MPU region management. */
#define portMPU_REGION_EXECUTE_NEVER               ( 0x01UL << 12UL )
#define portMPU_REGION_ENABLE                      ( 0x01UL )

/**
 * @brief The size (in words) of a task context.
 *
 * An array of this size is allocated in TCB where a task's context is saved
 * when it is switched out.
 *
 * Information about Floating Point Unit (FPU):
 * https://developer.arm.com/documentation/den0042/a/Floating-Point
 *
 * Additional information related to the Cortex R4-F's FPU Implementation:
 * https://developer.arm.com/documentation/ddi0363/e/fpu-programmer-s-model
 *
 * Additional information related to the Cortex R5-F's FPU Implementation:
 * https://developer.arm.com/documentation/ddi0460/d/FPU-Programmers-Model
 *
 * Additional information related to the ArmV7-R CPSR:
 * https://developer.arm.com/documentation/ddi0406/cb/Application-Level-Architecture/Application-Level-Programmers--Model/The-Application-Program-Status-Register--APSR-?lang=en
 *
 * Additional information related to the GPRs:
 * https://developer.arm.com/documentation/ddi0406/cb/System-Level-Architecture/The-System-Level-Programmers--Model/ARM-processor-modes-and-ARM-core-registers/ARM-core-registers?lang=en
 *
 */

#if( portENABLE_FPU == 1 )
    /*
     * +-------------------+-------+----------+--------+----------+----------+----------+------+
     * | ulCriticalNesting | FPSCR |  S0-S31  | R0-R12 | SP (R13) | LR (R14) | PC (R15) | CPSR |
     * +-------------------+-------+----------+--------+----------+----------+----------+------+
     *
     * <------------------><------><---------><--------><---------><--------><----------><----->
     *           1            1        32         13         1         1          1        1
     */
    #define CONTEXT_SIZE 51U
#else
    /*
     * +-------------------+--------+----------+----------+----------+------+
     * | ulCriticalNesting | R0-R12 | SP (R13) | LR (R14) | PC (R15) | CPSR |
     * +-------------------+--------+----------+----------+----------+------+
     *
     * <------------------><--------><---------><--------><----------><----->
     *           1             13         1         1          1        1
     */
    #define CONTEXT_SIZE 18U
#endif /* CONTEXT_SIZE */

/**
 * @brief Offset of xSystemCallStackInfo from the start of a TCB.
 */
#define portSYSTEM_CALL_INFO_OFFSET             \
    ( ( 1U /* pxTopOfStack. */ +                \
        ( portTOTAL_NUM_REGIONS_IN_TCB * 3U ) + \
        1U /* ulTaskFlags. */                   \
      ) * 4U )

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* PORTMACRO_ASM_H */
