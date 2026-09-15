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
 * File Name    : hw_cgc_private.h
 * Description  : hw_cgc Private header file.
 **********************************************************************************************************************/

#ifndef HW_CGC_PRIVATE_H
#define HW_CGC_PRIVATE_H
#include "r_cgc_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER


/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define CGC_OPCCR_HIGH_SPEED_MODE   (0U)
#define CGC_OPCCR_MIDDLE_SPEED_MODE (1U)
#define CGC_OPCCR_LOW_VOLTAGE_MODE (2U)
#define CGC_OPCCR_LOW_SPEED_MODE (3U)
#define CGC_SOPCCR_SET_SUBOSC_SPEED_MODE (1U)
#define CGC_SOPCCR_CLEAR_SUBOSC_SPEED_MODE (0U)
#define CGC_OPPCR_OPCM_MASK (0x3U)
#define CGC_SOPPCR_SOPCM_MASK (0x1U)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Main oscillator source identifier. */
typedef enum e_cgc_osc_source
{
    CGC_OSC_SOURCE_RESONATOR,       ///< A resonator is used as the source of the main oscillator.
    CGC_OSC_SOURCE_EXTERNAL_OSC     ///< An external oscillator is used as the source of the main oscillator.
} cgc_osc_source_t;


/*******************************************************************************************************************//**
 * @} (end addtogroup CGC)
 **********************************************************************************************************************/

/**********************************************************************************************************************
* Functions
 **********************************************************************************************************************/
void          HW_CGC_MemWaitSet (R_SYSTEM_Type * p_system_reg, uint32_t setting);

uint32_t      HW_CGC_MemWaitGet (R_SYSTEM_Type * p_system_reg);

void          HW_CGC_OscStopDetectEnable (R_SYSTEM_Type * p_system_reg);            // enable hardware oscillator stop detect function

void          HW_CGC_SRAM_ProtectLock (R_SYSTEM_Type * p_system_reg);

void          HW_CGC_SRAM_ProtectUnLock (R_SYSTEM_Type * p_system_reg);

void          HW_CGC_SRAM_RAMWaitSet (R_SYSTEM_Type * p_system_reg, uint32_t setting);

void          HW_CGC_SRAM_HSRAMWaitSet (R_SYSTEM_Type * p_system_reg, uint32_t setting);

void          HW_CGC_ROMWaitSet (R_SYSTEM_Type * p_system_reg, uint32_t setting);

uint32_t      HW_CGC_ROMWaitGet (R_SYSTEM_Type * p_system_reg);

bool          HW_CGC_SystickUpdate(uint32_t ticks_per_second);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* HW_CGC_PRIVATE_H */
