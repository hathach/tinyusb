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
 * File Name    : bsp_common_leds.c
 * Description  : Support for LEDs on a board.
 **********************************************************************************************************************/

#ifndef BSP_LEDS_H
#define BSP_LEDS_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/

/*********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/** Structure with LED information. */
/** @cond INC_HEADER_DEFS_SEC */
extern const bsp_leds_t g_bsp_leds;
/** @endcond */

/*******************************************************************************************************************//**
 * @ingroup BSP_COMMON_LEDS
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Public Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief      Return information about the LEDs on the current board.
 *
 * @param[out] p_leds Pointer to structure where LED info is stored.
 **********************************************************************************************************************/
ssp_err_t R_BSP_LedsGet(bsp_leds_t * p_leds)
{
#if BSP_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(p_leds);
#endif

    *p_leds = g_bsp_leds;            /* Copy over information */
    return SSP_SUCCESS;
}

#endif /* BSP_COMMON_LEDS */
/******************************************************************************************************************//**
 * @} (end @ingroup BSP_COMMON_LEDS)
 *********************************************************************************************************************/
