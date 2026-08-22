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
* File Name    : bsp_api.h
* Description  : BSP API header file. This is the only file you need to include for BSP use.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @defgroup BSP_Interface Board Support Package
 * @brief Common BSP includes.
 *
 * The BSP is responsible for getting the MCU from reset to the user application (i.e. the main function). Before
 * reaching the user application the BSP sets up the stacks, heap, clocks, interrupts, and C
 * runtime environment. The BSP is configurable to allow users to modify the process to meet design requirements.
 *
 * @{
***********************************************************************************************************************/

#ifndef BSP_API_H
#define BSP_API_H

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
/* SSP Common Includes. */
#include "../../inc/ssp_common_api.h"

/* Gets MCU configuration information. */
#include "bsp_cfg.h"

/* BSP Common Includes. */
#include "../../src/bsp/mcu/all/bsp_common.h"

/* BSP MCU Specific Includes. */
#include "../../src/bsp/mcu/all/bsp_register_protection.h"
#include "../../src/bsp/mcu/all/bsp_locking.h"
#include "../../src/bsp/mcu/all/bsp_irq.h"
#include "../../src/bsp/mcu/all/bsp_group_irq.h"
#include "../../src/bsp/mcu/all/bsp_clocks.h"

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

/* Build time error checking. */
#include "../../src/bsp/mcu/all/bsp_error_checking.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @defgroup BSP_MCUs Supported MCUs
 * @brief Supported MCUs in this version of the BSP
 *
 * The BSP has code specific to a MCU and a board. The code that is specific to a MCU can be shared amongst
 * boards that use the MCU.
 *
 * @{
***********************************************************************************************************************/
ssp_err_t   R_SSP_VersionGet(ssp_pack_version_t * const p_version);             /// Common to all MCU's

/** @} (end defgroup BSP_MCUs) */

/** @} (end defgroup BSP_Interface) */

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* BSP_API_H */
