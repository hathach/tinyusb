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
* File Name    : bsp_register_protection.c
* Description  : This module provides APIs for modifying register write protection.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "bsp_api.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/* Key code for writing PRCR register. */
#define BSP_PRV_PRCR_KEY        (0xA500U)

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
 
/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/
/** Used for holding reference counters for protection bits. */
static volatile uint16_t g_protect_counters[BSP_REG_PROTECT_TOTAL_ITEMS] = {0,0,0};
/** Masks for setting or clearing the PRCR register. Use -1 for size because PWPR in MPC is used differently. */
static const    uint16_t g_prcr_masks[BSP_REG_PROTECT_TOTAL_ITEMS] =
{
    0x0001U,         /* PRC0. */
    0x0002U,         /* PRC1. */
    0x0008U,         /* PRC3. */
};

/*******************************************************************************************************************//**
 * @addtogroup BSP_MCU_REG_PROTECT
 *
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Enable register protection. Registers that are protected cannot be written to. Register protection is
 *          enabled by using the Protect Register (PRCR) and the MPC's Write-Protect Register (PWPR).
 *
 * @param[in] regs_to_protect Registers which have write protection enabled.
 **********************************************************************************************************************/
void R_BSP_RegisterProtectEnable (bsp_reg_protect_t regs_to_protect)
{
    /** Get/save the current state of interrupts */
    SSP_CRITICAL_SECTION_DEFINE;
    SSP_CRITICAL_SECTION_ENTER;

    /* Is it safe to disable write access? */
    if (0 != g_protect_counters[regs_to_protect])
    {
        /* Decrement the protect counter */
        g_protect_counters[regs_to_protect]--;
    }

    /* Is it safe to disable write access? */
    if (0 == g_protect_counters[regs_to_protect])
    {
        /** Enable protection using PRCR register. */
        /** When writing to the PRCR register the upper 8-bits must be the correct key. Set lower bits to 0 to
           disable writes. */
        R_SYSTEM->PRCR = (uint16_t)((R_SYSTEM->PRCR | (uint16_t)BSP_PRV_PRCR_KEY) & (uint16_t)(~g_prcr_masks[regs_to_protect]));

    }

    /** Restore the interrupt state */
    SSP_CRITICAL_SECTION_EXIT;
}

/*******************************************************************************************************************//**
 * @brief Disable register protection. Registers that are protected cannot be written to. Register protection is
 *          disabled by using the Protect Register (PRCR) and the MPC's Write-Protect Register (PWPR).
 *
 * @param[in] regs_to_unprotect Registers which have write protection disabled.
 **********************************************************************************************************************/
void R_BSP_RegisterProtectDisable (bsp_reg_protect_t regs_to_unprotect)
{
    /** Get/save the current state of interrupts */
    SSP_CRITICAL_SECTION_DEFINE;
    SSP_CRITICAL_SECTION_ENTER;

    /* If this is first entry then disable protection. */
    if (0 == g_protect_counters[regs_to_unprotect])
    {
        /** Disable protection using PRCR register. */
        /** When writing to the PRCR register the upper 8-bits must be the correct key. Set lower bits to 0 to
           disable writes. */
        R_SYSTEM->PRCR = (uint16_t)((R_SYSTEM->PRCR | (uint16_t)BSP_PRV_PRCR_KEY) | (uint16_t)g_prcr_masks[regs_to_unprotect]);

    }

    /** Increment the protect counter */
    g_protect_counters[regs_to_unprotect]++;

    /** Restore the interrupt state */
    SSP_CRITICAL_SECTION_EXIT;
}



/*******************************************************************************************************************//**
 * @brief Initializes variables needed for register protection functionality.
 **********************************************************************************************************************/
void bsp_register_protect_open (void)
{
    uint32_t i;

    /** Initialize reference counters to 0. */
    for (i = 0U; i < BSP_REG_PROTECT_TOTAL_ITEMS; i++)
    {
        g_protect_counters[i] = (uint16_t)0;
    }
}

/** @} (end addtogroup BSP_MCU_REG_PROTECT) */
