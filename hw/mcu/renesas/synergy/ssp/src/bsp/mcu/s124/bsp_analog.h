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
* File Name    : bsp_analog.h
* Description  : Analog pin connections available on this MCU.
***********************************************************************************************************************/

#ifndef BSP_ANALOG_H_
#define BSP_ANALOG_H_

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_S124
 * @defgroup BSP_MCU_ANALOG_S124 Analog Connections
 *
 * This group contains a list of enumerations that can be used with the @ref ANALOG_CONNECT_API.
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "../all/bsp_common_analog.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/
/** List of analog connections that can be made on S124
 * @note This list may change based on device. This list is for S124.
 * */
typedef enum e_analog_connect
{
    /* Connections for ACMPLP channel 0 VREF input. */
    /* ANALOG0_VREF is the internal reference voltage. */
    /** Connect ACMPLP0 IVREF to ANALOG0 VREF. */
    ANALOG_CONNECT_ACMPLP0_IVREF_TO_ANALOG0_VREF    = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPMDR, C0VRF, FLAG_CLEAR),
    /* CMPREF0 = P101 */
    /** Connect ACMPLP0 IVREF to PORT1 P101. */
    ANALOG_CONNECT_ACMPLP0_IVREF_TO_PORT1_P101      = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPMDR, CLEAR_C0VRF, FLAG_CLEAR),

    /* Connections for ACMPLP channel 1 VREF input. */
    /* ANALOG0_VREF is the internal reference voltage. */
    /** Connect ACMPLP1 IVREF to ANALOG0 VREF. */
    ANALOG_CONNECT_ACMPLP1_IVREF_TO_ANALOG0_VREF    = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPMDR, C1VRF, FLAG_CLEAR),
    /* CMPREF1 = P103 */
    /** Connect ACMPLP1 IVREF to PORT1 P103. */
    ANALOG_CONNECT_ACMPLP1_IVREF_TO_PORT1_P103      = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPMDR, CLEAR_C1VRF, FLAG_CLEAR),

} analog_connect_t;

/** @} (end defgroup BSP_MCU_ANALOG_S124) */

#endif /* BSP_ANALOG_H_ */
