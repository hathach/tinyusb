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
* File Name    : bsp_mcu_info.h
* Description  : Information about the MCU on this board
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_MCUs
 * @defgroup BSP_MCU_S124 S124
 * @brief Code that is common to S124 MCUs.
 *
 * Implements functions that are common to S124 MCUs.
 *
 * @{
***********************************************************************************************************************/

#ifndef BSP_MCU_INFO_H_
#define BSP_MCU_INFO_H_

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/

/* BSP Common Includes. */
#include "../../inc/ssp_common_api.h"

#if defined(__GNUC__)
/* CMSIS-CORE currently generates 2 warnings when compiling with GCC. One in core_cmInstr.h and one in core_cm4_simd.h.
 * We are not modifying these files so we will ignore these warnings temporarily. */
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

/* The following files are provided by CMSIS, so violations are not reported during static analysis. */
/*LDRA_NOANALYSIS */

/* CMSIS-CORE Renesas Device Files. */
#include "../../src/bsp/cmsis/Device/RENESAS/S124/Include/S124.h"
#include "../../src/bsp/cmsis/Device/RENESAS/S124/Include/system_S124.h"

/* Static analysis resumes as normal after CMSIS files. */
/*LDRA_ANALYSIS */

#if defined(__GNUC__)
/* Restore warning settings for 'conversion' and 'sign-conversion' to as specified on command line. */
#pragma GCC diagnostic pop
#endif

/* BSP MCU Specific Includes. */
#include "../../src/bsp/mcu/all/bsp_register_protection.h"
#include "../../src/bsp/mcu/all/bsp_locking.h"
#include "../../src/bsp/mcu/all/bsp_irq.h"
#include "../../src/bsp/mcu/all/bsp_group_irq.h"
#include "../../src/bsp/mcu/all/bsp_clocks.h"
#include "../../src/bsp/mcu/s124/bsp_elc.h"
#include "../../src/bsp/mcu/s124/bsp_cache.h"
#include "../../src/bsp/mcu/s124/bsp_analog.h"

/* I/O port pin definitions. */
#include "r_ioport_api.h"

/* Factory MCU information. */
#include "../../inc/ssp_features.h"
#include "r_fmi.h"

/* BSP Common Includes (Other than bsp_common.h) */
#include "../../src/bsp/mcu/all/bsp_common_leds.h"
#include "../../src/bsp/mcu/all/bsp_delay.h"
#include "../../src/bsp/mcu/all/bsp_feature.h"

#include "../../src/bsp/mcu/all/bsp_mcu_api.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/* Naming convention for the following defines is BSP_FEATURE_HAS_<PERIPHERAL>_<FEATURE>. */
#define BSP_FEATURE_HAS_CGC_PLL                 (0)
#define BSP_FEATURE_HAS_CGC_PLL_SRC_CFG         (0)
#define BSP_FEATURE_HAS_CGC_USB_CLK             (0)
#define BSP_FEATURE_HAS_CGC_MIDDLE_SPEED        (1)
#define BSP_FEATURE_HAS_CGC_LOW_VOLTAGE         (1)
#define BSP_FEATURE_HAS_CGC_SUBOSC_SPEED        (1)
#define BSP_FEATURE_HAS_CGC_SDADC_CLK           (0)
#define BSP_FEATURE_HAS_CGC_LCD_CLK             (0)
#define BSP_FEATURE_HAS_CGC_EXTERNAL_BUS        (0)
#define BSP_FEATURE_HAS_CGC_SDRAM_CLK           (0)
#define BSP_FEATURE_HAS_CGC_PCKA                (0)
#define BSP_FEATURE_HAS_CGC_PCKB                (1)
#define BSP_FEATURE_HAS_CGC_PCKC                (0)
#define BSP_FEATURE_HAS_CGC_PCKD                (1)
#define BSP_FEATURE_HAS_CGC_FLASH_CLOCK         (0)

#define BSP_FEATURE_HAS_SCE_ON_S1   /* Crypto functionality available on S1 series MCUs */

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/

#endif /* BSP_MCU_INFO_H_ */

/** @} (end defgroup BSP_MCU_S124) */
