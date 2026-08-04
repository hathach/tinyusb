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
* File Name    : bsp_common_analog.h
* Description  : Common definitions for analog pin connections.
***********************************************************************************************************************/

#ifndef BSP_COMMON_ANALOG_H_
#define BSP_COMMON_ANALOG_H_

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
#define ANALOG_CONNECT_PRIV_MODULE_START_BIT      (24)
#define ANALOG_CONNECT_PRIV_FLAG_BIT              (20)
#define ANALOG_CONNECT_PRIV_CHANNEL_START_BIT     (16)
#define ANALOG_CONNECT_PRIV_CONNECTION_START_BIT  (8)
#define ANALOG_CONNECT_PRIV_OPTION_START_BIT      (0)

#define ANALOG_CONNECT_PRIV_FLAG_MASK             (1U << ANALOG_CONNECT_PRIV_FLAG_BIT)

#define ANALOG_CONNECT_DEFINE(module, channel, connection, option, flag) \
                          ((((((uint32_t) ANALOG_CONNECT_PRIV_MODULE_##module                            << ANALOG_CONNECT_PRIV_MODULE_START_BIT)      |  \
                              ((uint32_t) ANALOG_CONNECT_PRIV_CHANNEL_##channel                          << ANALOG_CONNECT_PRIV_CHANNEL_START_BIT))    |  \
                              ((uint32_t) ANALOG_CONNECT_PRIV_REG_##module##_##connection                << ANALOG_CONNECT_PRIV_CONNECTION_START_BIT)) |  \
                              ((uint32_t) ANALOG_CONNECT_PRIV_OPTION_##module##_##connection##_##option  << ANALOG_CONNECT_PRIV_OPTION_START_BIT))     |  \
                              ((uint32_t) ANALOG_CONNECT_PRIV_##flag                                     << ANALOG_CONNECT_PRIV_FLAG_BIT))

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/
typedef enum e_analog_connect_priv_module
{
    ANALOG_CONNECT_PRIV_MODULE_ACMPHS  = 1U,
    ANALOG_CONNECT_PRIV_MODULE_ACMPLP  = 2U,
    ANALOG_CONNECT_PRIV_MODULE_OPAMP   = 3U,
} analog_connect_priv_module_t;

typedef enum e_analog_connect_priv_channel
{
    ANALOG_CONNECT_PRIV_CHANNEL_0  = 0U,
    ANALOG_CONNECT_PRIV_CHANNEL_1  = 1U,
    ANALOG_CONNECT_PRIV_CHANNEL_2  = 2U,
    ANALOG_CONNECT_PRIV_CHANNEL_3  = 3U,
    ANALOG_CONNECT_PRIV_CHANNEL_4  = 4U,
    ANALOG_CONNECT_PRIV_CHANNEL_5  = 5U,
    ANALOG_CONNECT_PRIV_CHANNEL_6  = 6U,
    ANALOG_CONNECT_PRIV_CHANNEL_7  = 7U,
    ANALOG_CONNECT_PRIV_CHANNEL_8  = 8U,
    ANALOG_CONNECT_PRIV_CHANNEL_9  = 9U,
    ANALOG_CONNECT_PRIV_CHANNEL_10 = 10U,
    ANALOG_CONNECT_PRIV_CHANNEL_11 = 11U,
    ANALOG_CONNECT_PRIV_CHANNEL_12 = 12U,
    ANALOG_CONNECT_PRIV_CHANNEL_13 = 13U,
    ANALOG_CONNECT_PRIV_CHANNEL_14 = 14U,
    ANALOG_CONNECT_PRIV_CHANNEL_15 = 15U,
} analog_connect_priv_channel_t;

typedef enum e_analog_connect_priv_flag
{
    ANALOG_CONNECT_PRIV_FLAG_CLEAR  = 0U,
    ANALOG_CONNECT_PRIV_FLAG_SET    = 1U,
} analog_connect_priv_flag_t;

typedef enum e_analog_connect_priv_reg_acmphs
{
    ANALOG_CONNECT_PRIV_REG_ACMPHS_CMPSEL0 = 0x4U,  // CMPSEL0 offset from ACMPHS base
    ANALOG_CONNECT_PRIV_REG_ACMPHS_CMPSEL1 = 0x8U,  // CMPSEL1 offset from ACMPHS base
} analog_connect_priv_reg_acmphs_t;

typedef enum e_analog_connect_priv_reg_acmplp
{
    ANALOG_CONNECT_PRIV_REG_ACMPLP_COMPMDR  = 0x0U,  // COMPMDR offset from ACMPLP base
    ANALOG_CONNECT_PRIV_REG_ACMPLP_COMPSEL0 = 0x4U,  // COMPSEL0 offset from ACMPLP base
    ANALOG_CONNECT_PRIV_REG_ACMPLP_COMPSEL1 = 0x5U,  // COMPSEL1 offset from ACMPLP base
} analog_connect_priv_reg_acmplp_t;

typedef enum e_analog_connect_priv_reg_opamp
{
    ANALOG_CONNECT_PRIV_REG_OPAMP_AMP0OS = 0xEU,   // AMP0OS offset from OPAMP base
    ANALOG_CONNECT_PRIV_REG_OPAMP_AMP0MS = 0xFU,   // AMP0MS offset from OPAMP base
    ANALOG_CONNECT_PRIV_REG_OPAMP_AMP0PS = 0x10U,  // AMP0PS offset from OPAMP base
    ANALOG_CONNECT_PRIV_REG_OPAMP_AMP1MS = 0x12U,  // AMP1MS offset from OPAMP base
    ANALOG_CONNECT_PRIV_REG_OPAMP_AMP1PS = 0x13U,  // AMP1PS offset from OPAMP base
    ANALOG_CONNECT_PRIV_REG_OPAMP_AMP2MS = 0x15U,  // AMP2MS offset from OPAMP base
    ANALOG_CONNECT_PRIV_REG_OPAMP_AMP2PS = 0x16U,  // AMP2PS offset from OPAMP base
} analog_connect_priv_reg_opamp_t;

typedef enum e_analog_connect_priv_option_acmphs_cmpsel0
{
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL0_IVCMP0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL0_IVCMP1 = 1U << 1,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL0_IVCMP2 = 1U << 2,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL0_IVCMP3 = 1U << 3,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL0_IVCMP4 = 1U << 4,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL0_IVCMP5 = 1U << 5,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL0_IVCMP6 = 1U << 6,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL0_IVCMP7 = 1U << 7,
} analog_connect_priv_option_acmphs_cmpsel0_t;

typedef enum e_analog_connect_priv_option_acmphs_cmpsel1
{
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL1_IVREF0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL1_IVREF1 = 1U << 1,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL1_IVREF2 = 1U << 2,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL1_IVREF3 = 1U << 3,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL1_IVREF4 = 1U << 4,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL1_IVREF5 = 1U << 5,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL1_IVREF6 = 1U << 6,
    ANALOG_CONNECT_PRIV_OPTION_ACMPHS_CMPSEL1_IVREF7 = 1U << 7,
} analog_connect_priv_option_acmphs_cmpsel1_t;

typedef enum e_analog_connect_priv_option_acmplp_compmdr
{
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPMDR_C0VRF        = 0x40,                                   // Set bit 2, do not clear anything
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPMDR_CLEAR_C0VRF  = 0x04,                                   // Do not set anything, clear bit 2
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPMDR_C1VRF        = 0x40 | ANALOG_CONNECT_PRIV_FLAG_MASK,   // Set bit 6, do not clear anything
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPMDR_CLEAR_C1VRF  = 0x04 | ANALOG_CONNECT_PRIV_FLAG_MASK,   // Do not set anything, clear bit 6
} analog_connect_priv_option_acmplp_compmdr_t;

typedef enum e_analog_connect_priv_option_acmplp_compsel0
{
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL0_CMPSEL0      = 0x16,                                  // Set bit 0, clear bits 1 and 2
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL0_CMPSEL1      = 0x25,                                  // Set bit 1, clear bits 0 and 2
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL0_CMPSEL2      = 0x43,                                  // Set bit 2, clear bits 0 and 1
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL0_CMPSEL4      = 0x16 | ANALOG_CONNECT_PRIV_FLAG_MASK,  // Set bit 4, clear bits 5 and 6
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL0_CMPSEL5      = 0x25 | ANALOG_CONNECT_PRIV_FLAG_MASK,  // Set bit 5, clear bits 4 and 6
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL0_CMPSEL6      = 0x43 | ANALOG_CONNECT_PRIV_FLAG_MASK,  // Set bit 6, clear bits 4 and 5
} analog_connect_priv_option_acmplp_compsel0_t;

typedef enum e_analog_connect_priv_option_acmplp_compsel1
{
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL1_CRVS0        = 0x16,                                  // Set bit 0, clear bits 1 and 2
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL1_CRVS1        = 0x25,                                  // Set bit 1, clear bits 0 and 2
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL1_CRVS2        = 0x43,                                  // Set bit 2, clear bits 0 and 1
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL1_CRVS4        = 0x16 | ANALOG_CONNECT_PRIV_FLAG_MASK,  // Set bit 4, clear bits 5 and 6
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL1_CRVS5        = 0x25 | ANALOG_CONNECT_PRIV_FLAG_MASK,  // Set bit 5, clear bits 4 and 6
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL1_CRVS6        = 0x43 | ANALOG_CONNECT_PRIV_FLAG_MASK,  // Set bit 6, clear bits 4 and 5
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL1_C1VRF2       = 0x80 | ANALOG_CONNECT_PRIV_FLAG_MASK,  // Set bit 7, do not clear anything
    ANALOG_CONNECT_PRIV_OPTION_ACMPLP_COMPSEL1_CLEAR_C1VRF2 = 0x08 | ANALOG_CONNECT_PRIV_FLAG_MASK,  // Do not set anything, clear bit 7
} analog_connect_priv_option_acmplp_compsel1_t;

typedef enum e_analog_connect_priv_option_opamp_amp0os
{
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0OS_BREAK  = 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0OS_AMPOS0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0OS_AMPOS1 = 1U << 1,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0OS_AMPOS2 = 1U << 2,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0OS_AMPOS3 = 1U << 3,
} analog_connect_priv_option_opamp_amp0os_t;

typedef enum e_analog_connect_priv_option_opamp_amp0ms
{
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0MS_BREAK  = 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0MS_AMPMS0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0MS_AMPMS1 = 1U << 1,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0MS_AMPMS2 = 1U << 2,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0MS_AMPMS3 = 1U << 3,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0MS_AMPMS4 = 1U << 4,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0MS_AMPMS7 = 1U << 7,
} analog_connect_priv_option_opamp_amp0ms_t;

typedef enum e_analog_connect_priv_option_opamp_amp0ps
{
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0PS_BREAK  = 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0PS_AMPPS0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0PS_AMPPS1 = 1U << 1,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0PS_AMPPS2 = 1U << 2,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0PS_AMPPS3 = 1U << 3,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP0PS_AMPPS7 = 1U << 7,
} analog_connect_priv_option_opamp_amp0ps_t;

typedef enum e_analog_connect_priv_option_opamp_amp1ms
{
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1MS_BREAK  = 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1MS_AMPMS0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1MS_AMPMS7 = 1U << 7,
} analog_connect_priv_option_opamp_amp1ms_t;

typedef enum e_analog_connect_priv_option_opamp_amp1ps
{
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1PS_BREAK  = 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1PS_AMPPS0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1PS_AMPPS1 = 1U << 1,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1PS_AMPPS2 = 1U << 2,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1PS_AMPPS3 = 1U << 3,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP1PS_AMPPS7 = 1U << 7,
} analog_connect_priv_option_opamp_amp1ps_t;

typedef enum e_analog_connect_priv_option_opamp_amp2ms
{
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP2MS_BREAK  = 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP2MS_AMPMS0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP2MS_AMPMS7 = 1U << 7,
} analog_connect_priv_option_opamp_amp2ms_t;

typedef enum e_analog_connect_priv_option_opamp_amp2ps
{
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP2PS_BREAK  = 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP2PS_AMPPS0 = 1U << 0,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP2PS_AMPPS1 = 1U << 1,
    ANALOG_CONNECT_PRIV_OPTION_OPAMP_AMP2PS_AMPPS7 = 1U << 7,
} analog_connect_priv_option_opamp_amp2ps_t;

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/

#endif /* BSP_COMMON_ANALOG_H_ */
