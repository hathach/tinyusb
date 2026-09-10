/***********************************************************************************************************************
 * Copyright [2015-2025] Renesas Electronics Corporation and/or its licensors. All Rights Reserved.
 * 
 * This file is part of Renesas SynergyTM Software Package (SSP)
 *
 * The contents of this file (the "contents") are proprietary and confidential to Renesas Electronics Corporation
 * and/or its licensors ("Renesas") and subject to statutory and contractual protections.
 *
 * This file is subject to a Renesas SSP license agreement. Unless otherwise agreed in an SSP license agreement with
 * Renesas: 1) you may not use, copy, modify, distribute, display, or perform the contents; 2) you may not use any name
 * or mark of Renesas for advertising or publicity purposes or in connection with your use of the contents; 3) RENESAS
 * MAKES NO WARRANTY OR REPRESENTATIONS ABOUT THE SUITABILITY OF THE CONTENTS FOR ANY PURPOSE; THE CONTENTS ARE PROVIDED
 * "AS IS" WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, AND NON-INFRINGEMENT; AND 4) RENESAS SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, OR
 * CONSEQUENTIAL DAMAGES, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA, OR PROJECTS, WHETHER IN AN ACTION OF
 * CONTRACT OR TORT, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE CONTENTS. Third-party contents
 * included in this file may be subject to different terms.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : bsp_compiler_support.h
* Description  : Contains macros to help support multiple compilers
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_COMMON
 * @defgroup BSP_COMPILER_SUPPORT Compiler Support
 *
 * The macros in this file are defined based upon which compiler is being used. The macros abstract common section
 * names and gives a common way to place code in a particular section. Some macros have a version that ends in V2.
 * These were created to unify the usage between compilers while retaining backwards compatibility in the existing
 * macros and should be preferred in new code.
 *
 * Description of macros:
 * - BSP_SECTION_STACK          - Name of section where stack(s) are stored
 * - BSP_SECTION_HEAP           - Name of section where heap(s) are stored
 * - BSP_SECTION_VECTOR         - Name of section where vector table is stored
 * - BSP_SECTION_ROM_REGISTERS  - Name of section where ROM registers are located
 * - BSP_PLACE_IN_SECTION       - Macro for placing code in a particular section
 * - BSP_ALIGN_VARIABLE         - Macro for specifiying a minimum alignment in bytes
 * - BSP_PACKED                 - Macro for setting a 1 byte alignment to remove padding
 * - BSP_DONT_REMOVE            - Keyword to tell linker/compiler to not optimize out a variable or function
 *
 * @note Currently supported compilers are GCC and IAR
 *
 * @{
 **********************************************************************************************************************/

#ifndef BSP_COMPILER_SUPPORT_H_
#define BSP_COMPILER_SUPPORT_H_

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
#if   defined(__GNUC__)                 /* GCC Compiler */
#define BSP_SECTION_STACK  ".stack"
#define BSP_SECTION_HEAP   ".heap"
#define BSP_SECTION_VECTOR ".vectors"
#define BSP_SECTION_ROM_REGISTERS ".rom_registers"
#define BSP_SECTION_ID_CODE_1 ".id_code_1"
#define BSP_SECTION_ID_CODE_2 ".id_code_2"
#define BSP_SECTION_ID_CODE_3 ".id_code_3"
#define BSP_SECTION_ID_CODE_4 ".id_code_4"
/*LDRA_INSPECTED 293 s SSP requires section support and there is no better option than to use an attribute in GCC. */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define BSP_PLACE_IN_SECTION(x) __attribute__ ((section(x))) __attribute__ ((__used__))
#define BSP_DONT_REMOVE
/*LDRA_INSPECTED 293 s SSP requires alignment support and there is no better option than to use an attribute in GCC. */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define BSP_ALIGN_VARIABLE(x)  __attribute__ ((aligned (x)))
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define BSP_PACKED  __attribute__ ((aligned(1)))
#elif defined(__ICCARM__)               /* IAR Compiler */
#define BSP_SECTION_STACK  ".stack"
#define BSP_SECTION_HEAP   "HEAP"
#define BSP_SECTION_VECTOR ".vectors"
#define BSP_SECTION_ROM_REGISTERS ".rom_registers"
#define BSP_SECTION_ID_CODE_1 ".id_code_1"
#define BSP_SECTION_ID_CODE_2 ".id_code_2"
#define BSP_SECTION_ID_CODE_3 ".id_code_3"
#define BSP_SECTION_ID_CODE_4 ".id_code_4"
#define BSP_PLACE_IN_SECTION(x) @ x
#define BSP_DONT_REMOVE __root
#define BSP_ALIGN_VARIABLE(x)
#define BSP_PACKED
#endif

/* Compiler neutral macros. Older versions kept for backwards compatibility. */
/*LDRA_INSPECTED 293 s SSP requires section support and there is no better option than to use an attribute in GCC. */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define BSP_PLACE_IN_SECTION_V2(x) __attribute__ ((section(x))) __attribute__ ((__used__))
/*LDRA_INSPECTED 293 s SSP requires section support and there is no better option than to use an attribute in GCC. */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define BSP_ALIGN_VARIABLE_V2(x)  __attribute__ ((aligned (x)))
/*LDRA_INSPECTED 293 s SSP requires section support and there is no better option than to use an attribute in GCC. */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define BSP_PACKED_V2  __attribute__ ((aligned(1))) 

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/

#endif /* BSP_COMPILER_SUPPORT_H_ */

/** @} (end of defgroup BSP_COMPILER_SUPPORT) */
