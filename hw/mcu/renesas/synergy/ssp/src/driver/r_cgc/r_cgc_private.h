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

/**********************************************************************************************************************
 * File Name    : r_cgc_private.h
 * Description  : Private header file for the CGC module.
 **********************************************************************************************************************/

#ifndef R_CGC_PRIVATE_H
#define R_CGC_PRIVATE_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_clock_cfg.h"


/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** Timeout to wait for register to be written */
#define MAX_REGISTER_WAIT_COUNT (0xFFFFU)
/** Number of subclock cycles to wait after subcclock has been stopped */
#define SUBCLOCK_DELAY          (5U)

/** Divisor to use to obtain time for 1 tick in us from iclk */
#define RELOAD_COUNT_FOR_1US    (1000000U)


/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Operating power control modes. */
typedef enum
{
    CGC_HIGH_SPEED_MODE     = 0U,   // Should match OPCCR OPCM high speed
    CGC_MIDDLE_SPEED_MODE   = 1U,   // Should match OPCCR OPCM middle speed
    CGC_LOW_VOLTAGE_MODE    = 2U,   // Should match OPCCR OPCM low voltage
    CGC_LOW_SPEED_MODE      = 3U,   // Should match OPCCR OPCM low speed
    CGC_SUBOSC_SPEED_MODE   = 4U,   // Can be any value not otherwise used
}cgc_operating_modes_t;

/*******************************************************************************************************************//**
@addtogroup CGC
@{
**********************************************************************************************************************/

/**********************************************************************************************************************
* Private Global Functions
 **********************************************************************************************************************/
bool r_cgc_clock_run_state_get (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock);
cgc_operating_modes_t r_cgc_operating_mode_get (R_SYSTEM_Type * p_system_reg);
void r_cgc_operating_hw_modeset (R_SYSTEM_Type * p_system_reg, cgc_operating_modes_t operating_mode);
void r_cgc_hoco_wait_control_set (R_SYSTEM_Type * p_system_reg, uint8_t hoco_wait);

/*******************************************************************************************************************//**
 * @} (end addtogroup CGC)
 **********************************************************************************************************************/
#endif /* R_CGC_PRIVATE_H */
