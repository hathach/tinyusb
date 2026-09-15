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
 * File Name    : hw_elc_common.h
 * Description  : ELC LLD API
 **********************************************************************************************************************/


/*******************************************************************************************************************//**
 * @addtogroup ELC
 * @{
 **********************************************************************************************************************/

#ifndef HW_ELC_COMMON_H
#define HW_ELC_COMMON_H

/**********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"


/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER


/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
#define ELC_ELSEGRN_STEP1 (0U)     /* WI = 0, WE = 0, SEG = 0 */
#define ELC_ELSEGRN_STEP2 (0x40U)  /* WI = 0, WE = 1, SEG = 0 */
#define ELC_ELSEGRN_STEP3 (0x41U)  /* WI = 0, WE = 1, SEG = 1 */

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Generate a software event 0
 **********************************************************************************************************************/
__STATIC_INLINE void HW_ELC_SoftwareEvent0Generate (R_ELC_Type * p_elc_reg)
{
    /* Step 1. enable the ELSEGR0 register for writing */
    p_elc_reg->ELSEGRnRC0[0].ELSEGRn = ELC_ELSEGRN_STEP1;

    /* Step 2. Enable the SEG bit for writing */
    p_elc_reg->ELSEGRnRC0[0].ELSEGRn = ELC_ELSEGRN_STEP2;

    /* Step 3. Set the SEG bit which causes the software event generation */
    p_elc_reg->ELSEGRnRC0[0].ELSEGRn = ELC_ELSEGRN_STEP3;
}

/*******************************************************************************************************************//**
 * Generate a software event 1
 **********************************************************************************************************************/
__STATIC_INLINE void HW_ELC_SoftwareEvent1Generate (R_ELC_Type * p_elc_reg)
{
    /* Step 1. enable the ELSEGR0 register for writing */
    p_elc_reg->ELSEGRnRC0[1].ELSEGRn = ELC_ELSEGRN_STEP1;

    /* Step 2. Enable the SEG bit for writing */
    p_elc_reg->ELSEGRnRC0[1].ELSEGRn = ELC_ELSEGRN_STEP2;

    /* Step 3. Set the SEG bit which causes the software event generation */
    p_elc_reg->ELSEGRnRC0[1].ELSEGRn = ELC_ELSEGRN_STEP3;
}

/*******************************************************************************************************************//**
 * Enable the ELC block
 **********************************************************************************************************************/
__STATIC_INLINE void HW_ELC_Enable (R_ELC_Type * p_elc_reg)
{
    p_elc_reg->ELCR_b.ELCON = 1U;
}

/*******************************************************************************************************************//**
 * Disable the ELC block
 **********************************************************************************************************************/
__STATIC_INLINE void HW_ELC_Disable (R_ELC_Type * p_elc_reg)
{
    p_elc_reg->ELCR_b.ELCON = 0U;
}

/*******************************************************************************************************************//**
 * Make a link between a signal and a peripheral
 **********************************************************************************************************************/
__STATIC_INLINE void HW_ELC_LinkSet (R_ELC_Type * p_elc_reg, uint32_t peripheral, uint16_t signal)
{
    p_elc_reg->ELSRnRC0[peripheral].ELSRn = signal;
}

/*******************************************************************************************************************//**
 * Break a link between a signal and a peripheral
 **********************************************************************************************************************/
__STATIC_INLINE void HW_ELC_LinkBreak (R_ELC_Type * p_elc_reg, uint32_t peripheral)
{
    p_elc_reg->ELSRnRC0[peripheral].ELSRn = 0U;
}



/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER


#endif /* HW_ELC_COMMON_H */
