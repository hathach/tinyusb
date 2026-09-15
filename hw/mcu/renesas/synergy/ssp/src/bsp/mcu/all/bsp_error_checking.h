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
* File Name    : bsp_error_checking.h
* Description  : Contains build time error checking
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_COMMON
 * @defgroup BSP_ERROR_CHECKING Error Checking
 *
 * This file performs build time error checking where possible.
 *
 * @{
 **********************************************************************************************************************/

#ifndef BSP_ERROR_CHECKING_H_
#define BSP_ERROR_CHECKING_H_

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "bsp_cfg.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/** Stacks (and heap) must be sized and aligned to an integer multiple of this number. */
#define BSP_STACK_ALIGNMENT     (8)     

/***********************************************************************************************************************
Error checking
***********************************************************************************************************************/
/** Verify that stack and heap sizes are integer multiples of 8. Stacks and heaps are aligned on 8-byte boundaries so
    if the size is an integer multiple of 8 then the start of the stack will be 8-byte aligned. This is done because
    the ARM EABI requires an 8-byte aligned stack. */
#if (BSP_CFG_STACK_MAIN_BYTES % BSP_STACK_ALIGNMENT) != 0
    #error "Main Stack size is not integer multiple of 8. Please update BSP_CFG_STACK_MAIN_BYTES"
#endif

#if (BSP_CFG_STACK_PROCESS_BYTES % BSP_STACK_ALIGNMENT) > 0
    #error "Process Stack size is not integer multiple of 8. Please update BSP_CFG_STACK_PROCESS_BYTES"
#endif

#if (BSP_CFG_HEAP_BYTES % BSP_STACK_ALIGNMENT) > 0
    #error "Heap size is not integer multiple of 8. Please update BSP_CFG_HEAP_BYTES"
#endif

#endif /* BSP_ERROR_CHECKING_H_ */

/** @} (end of defgroup BSP_ERROR_CHECKING) */
