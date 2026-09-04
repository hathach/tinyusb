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
* File Name    : bsp_clocks.c
* Description  : Calls the CGC module to setup the system clocks. Settings for clocks are based on macros in
*                bsp_clock_cfg.h.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_S124
 * @defgroup BSP_MCU_CLOCKS_S124 Clock Initialization
 *
 * Functions in this file configure the system clocks based upon the macros in bsp_clock_cfg.h.
 *
 * @{
 **********************************************************************************************************************/



/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "bsp_api.h"

#if defined(BSP_MCU_GROUP_S124)

#include "r_cgc_api.h"
#include "r_cgc.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
 
/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief      Sets up system clocks.
 **********************************************************************************************************************/
void bsp_clock_init (void)
{
    ssp_err_t err = SSP_SUCCESS;
    cgc_system_clock_cfg_t sys_cfg;
    cgc_clock_t clock;

    g_cgc_on_cgc.init();

    /** MOCO is default clock out of reset. Enable new clock if chosen. S124 has no PLL. */
    clock = BSP_CFG_CLOCK_SOURCE;
    err = g_cgc_on_cgc.clockStart(clock, NULL);

    /** If the system clock has failed to start call the unrecoverable error handler. */
    if((SSP_SUCCESS != err) && (SSP_ERR_CLOCK_ACTIVE != err))
    {
        BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0);
    }

    /** MOCO, LOCO, and subclock do not have stabilization flags that can be checked. */
    if ((CGC_CLOCK_MOCO != clock) && (CGC_CLOCK_LOCO != clock) && (CGC_CLOCK_SUBCLOCK != clock))
    {
        while (SSP_ERR_STABILIZED != g_cgc_on_cgc.clockCheck(clock))
        {
            /** Wait for clock source to stabilize */
        }
    }

    sys_cfg.iclk_div  = BSP_CFG_ICK_DIV;
    sys_cfg.pclkb_div = BSP_CFG_PCKB_DIV;
    sys_cfg.pclkd_div = BSP_CFG_PCKD_DIV;

    /** Set which clock to use for system clock and divisors for all system clocks. */
    err = g_cgc_on_cgc.systemClockSet(clock, &sys_cfg);

    /** If the system clock has failed to be configured properly call the unrecoverable error handler. */
    if(SSP_SUCCESS != err)
    {
        BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0);
    }
}

/*******************************************************************************************************************//**
 * @brief      Returns frequency of CPU clock in Hz.
 *
 * @retval     Frequency of the CPU in Hertz
 **********************************************************************************************************************/
uint32_t bsp_cpu_clock_get (void)
{
    uint32_t freq = (uint32_t)0;

    g_cgc_on_cgc.systemClockFreqGet(CGC_SYSTEM_CLOCKS_ICLK, &freq);

    return freq;
}

ssp_err_t bsp_clock_set_callback(bsp_clock_set_callback_args_t * p_args)
{
    SSP_PARAMETER_NOT_USED(p_args);

    /* Do nothing. */
    return SSP_SUCCESS;
}

#endif


/** @} (end defgroup BSP_MCU_CLOCKS_S124) */
