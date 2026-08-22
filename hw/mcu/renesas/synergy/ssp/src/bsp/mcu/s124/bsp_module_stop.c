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
 * File Name    : bsp_module_stop.c
 * Description  : Provides a function to set or clear module stop bits.
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_S124
 * @defgroup BSP_MCU_MODULE_STOP_S124 Module Start and Stop
 *
 * Module start and stop functions are provided to enable or disable peripherals.
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "bsp_api.h"

#if defined(BSP_MCU_GROUP_S124)

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define BSP_MSTP_DATA_INVALID (0xFFU)

#define BSP_MSTP_EXCEPTION    (1U << 5)

/*LDRA_INSPECTED 340 S Using function like macro because inline function cannot be used to initialize an array. */
#define BSP_MSTP_REG(x)       ((x) << 6)
/*LDRA_INSPECTED 340 S Using function like macro because inline function cannot be used to initialize an array. */
#define BSP_MSTP_BIT(x)       (x)

#define BSP_MSTPCRA (0U)
#define BSP_MSTPCRB (1U)
#define BSP_MSTPCRC (2U)
#define BSP_MSTPCRD (3U)

/** Largest channel number associated with lower MSTP bit for GPT on this MCU. */
#define HW_GPT_MSTPD5_MAX_CH (0U)

/** Used to generate a compiler error (divided by 0 error) if the assertion fails.  This is used in place of "#error"
 * for expressions that cannot be evaluated by the preprocessor like sizeof(). */
/*LDRA_INSPECTED 340 S Using function like macro because inline function does not generate divided by 0 error. */
#define BSP_COMPILE_TIME_ASSERT(e) ((void) sizeof(char[1 - 2 * !(e)]))

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Definition of BSP module stop bit. */
typedef struct u_bsp_mstp_bit_t
{
    uint8_t  bit       : 5; ///< Which bit in MSTP register
    uint8_t  exception : 1; ///< Special processing required
    uint8_t  reg       : 2; ///< Which MSTP register
} bsp_mstp_bit_t;


/** Definition of BSP module stop bit. */
typedef union u_bsp_mstp_data
{
    uint8_t  byte;              ///< Byte access for comparison
    bsp_mstp_bit_t bit;
} bsp_mstp_data_t;

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
/** Module stop bit definitions for each ssp_ip_t enum value. */
static const uint8_t g_bsp_module_stop[] =
{
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_CFLASH=0,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_DFLASH=1,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_RAM=2,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_SYSTEM=3,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_FCU=4,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_DEBUG=5,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_ICU=6,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_DMAC=7,
    BSP_MSTP_REG(BSP_MSTPCRA) | BSP_MSTP_BIT(22U),                      // SSP_IP_DTC=8,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_IOPORT=9,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_PFC=10,
    BSP_MSTP_REG(BSP_MSTPCRC) | BSP_MSTP_BIT(14U),                      // SSP_IP_ELC=11,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_BSC=12,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_MPU=13,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_MSTP=14,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_MMF=15,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_KEY=16,
    BSP_MSTP_REG(BSP_MSTPCRC) | BSP_MSTP_BIT(0U),                       // SSP_IP_CAC=17,
    BSP_MSTP_REG(BSP_MSTPCRC) | BSP_MSTP_BIT(13U),                      // SSP_IP_DOC=18,
    BSP_MSTP_REG(BSP_MSTPCRC) | BSP_MSTP_BIT(1U),                       // SSP_IP_CRC=19,
    BSP_MSTP_REG(BSP_MSTPCRB) | BSP_MSTP_BIT(31U),                      // SSP_IP_SCI=20,
    BSP_MSTP_REG(BSP_MSTPCRB) | BSP_MSTP_BIT(9U),                       // SSP_IP_IIC=21,
    BSP_MSTP_REG(BSP_MSTPCRB) | BSP_MSTP_BIT(19U),                      // SSP_IP_RSPI=22,
    BSP_MSTP_REG(BSP_MSTPCRC) | BSP_MSTP_BIT(3U),                       // SSP_IP_CTSU=23,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_SCE=24,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_SLCDC=25,
    BSP_MSTP_REG(BSP_MSTPCRC) | BSP_MSTP_BIT(31U),                      // SSP_IP_AES=26,
    BSP_MSTP_REG(BSP_MSTPCRC) | BSP_MSTP_BIT(28U),                      // SSP_IP_TRNG=27,
    BSP_MSTP_DATA_INVALID,                                             // 28,
    BSP_MSTP_DATA_INVALID,                                             // 29,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_ROMC=30,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_SRAM=31,
    BSP_MSTP_REG(BSP_MSTPCRD) | BSP_MSTP_BIT(16U),                      // SSP_IP_ADC=32,
    BSP_MSTP_REG(BSP_MSTPCRD) | BSP_MSTP_BIT(20U),                      // SSP_IP_DAC=33,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_TSN=34,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_DAAD=35,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_COMP_HS=36,
    BSP_MSTP_REG(BSP_MSTPCRD) | BSP_MSTP_BIT(29U) | BSP_MSTP_EXCEPTION, // SSP_IP_COMP_LP=37,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_OPAMP=38,
    BSP_MSTP_DATA_INVALID,                                             // 39,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_RTC=40,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_WDT=41,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_IWDT=42,
    BSP_MSTP_REG(BSP_MSTPCRD) | BSP_MSTP_BIT(5U) | BSP_MSTP_EXCEPTION,  // SSP_IP_GPT=43,
    BSP_MSTP_REG(BSP_MSTPCRD) | BSP_MSTP_BIT(14U) | BSP_MSTP_EXCEPTION, // SSP_IP_POEG=44,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_OPS=45,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_PSD=46,
    BSP_MSTP_REG(BSP_MSTPCRD) | BSP_MSTP_BIT(3U),                       // SSP_IP_AGT=47,
    BSP_MSTP_REG(BSP_MSTPCRB) | BSP_MSTP_BIT(2U),                       // SSP_IP_CAN=48,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_IRDA=49,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_QSPI=50,
    BSP_MSTP_REG(BSP_MSTPCRB) | BSP_MSTP_BIT(11) | BSP_MSTP_EXCEPTION, // SSP_IP_USB=51,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_SDHIMMC=52,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_SRC=53,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_SSI=54,
    BSP_MSTP_DATA_INVALID,                                             // 55,
    BSP_MSTP_DATA_INVALID,                                             // 56,
    BSP_MSTP_DATA_INVALID,                                             // 57,
    BSP_MSTP_DATA_INVALID,                                             // 58,
    BSP_MSTP_DATA_INVALID,                                             // 59,
    BSP_MSTP_DATA_INVALID,                                             // 60,
    BSP_MSTP_DATA_INVALID,                                             // 61,
    BSP_MSTP_DATA_INVALID,                                             // 62,
    BSP_MSTP_DATA_INVALID,                                             // 63,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_ETHER=64,    SSP_IP_EDMAC=64,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_EPTPC=65,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_PDC=66,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_GLCDC=67,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_DRW=68,
    BSP_MSTP_DATA_INVALID,                                             // SSP_IP_JPEG=69,
};

/** Module stop register addresses. */
static volatile uint32_t * const gp_mstp[] =
{
    &(R_SYSTEM->MSTPCRA),
    &(R_MSTP->MSTPCRB),
    &(R_MSTP->MSTPCRC),
    &(R_MSTP->MSTPCRD),
};

static ssp_err_t r_bsp_module_start_stop (ssp_feature_t const * const p_feature, bool stop, bool force);
static uint32_t r_bsp_get_mstp_mask (ssp_feature_t const * const p_feature);

/*******************************************************************************************************************//**
 * @brief Stop module (enter module stop).  Stopping a module disables clocks to the peripheral to save power.
 *
 * @note Some module stop bits are shared between peripherals. Modules with shared module stop bits cannot be stopped
 * to prevent unintentionally stopping related modules.
 *
 * @param[in] p_feature  Pointer to definition of the feature, defined by ssp_feature_t.
 *
 * @retval SSP_SUCCESS               Module is stopped
 * @retval SSP_ERR_ASSERTION         p_feature::id is invalid
 * @retval SSP_ERR_INVALID_ARGUMENT  Module has no module stop bit, or module stop bit is shared and entering module
 *                                   stop is not supported because it could affect other modules.
 **********************************************************************************************************************/
ssp_err_t R_BSP_ModuleStop(ssp_feature_t const * const p_feature)
{
    return r_bsp_module_start_stop(p_feature, true, false);
}

/******************************************************************************************************************//**
 * @brief Stop module (enter module stop) even if the module is used for multiple channels.
 *
 * @param[in] p_feature Pointer to definition of the feature, defined by ssp_feature_t.
 *
 * @retval SSP_SUCCESS          Module is stopped
 * @retval SSP_ERR_ASSERTION    p_feature::id is invalid
 **********************************************************************************************************************/
ssp_err_t R_BSP_ModuleStopAlways(ssp_feature_t const * const p_feature)
{
    return r_bsp_module_start_stop(p_feature, true, true);
}

/*******************************************************************************************************************//**
 * @brief Start module (cancel module stop).  Starting a module enables clocks to the peripheral and allows registers
 * to be set.
 *
 * @param[in] p_feature  Pointer to definition of the feature, defined by ssp_feature_t.
 *
 * @retval SSP_SUCCESS               Module is started
 * @retval SSP_ERR_ASSERTION         p_feature::id is invalid
 * @retval SSP_ERR_INVALID_ARGUMENT  Module has no module stop bit.
 **********************************************************************************************************************/
ssp_err_t R_BSP_ModuleStart(ssp_feature_t const * const p_feature)
{
    return r_bsp_module_start_stop(p_feature, false, false);
}

/***********************************************************************************************************************
 * @brief Get the module state.
 *
 * @param[in]  p_feature  Pointer to definition of the feature, defined by ssp_feature_t.
 * @param[out] p_stop     Pointer to a boolean that will contain the module stopped state
 *
 * @retval SSP_SUCCESS               Module is started
 * @retval SSP_ERR_ASSERTION         p_feature::id is invalid
 *                                   p_stop is invalid
 * @retval SSP_ERR_INVALID_ARGUMENT  Module has no module stop bit.
 **********************************************************************************************************************/
ssp_err_t R_BSP_ModuleStateGet(ssp_feature_t const * const p_feature, bool * const p_stop)
{
    bsp_mstp_data_t data = {0};

#if BSP_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(p_feature->id < SSP_IP_MAX);
    SSP_ASSERT(p_stop != NULL);
#endif

    /** The g_bsp_module_stop array must have entries for each ssp_ip_t enum value. */
    /* LDRA will complain about "Statement with no side effect" w/o SSP_PARAMETER_NOT_USED */
    SSP_PARAMETER_NOT_USED(BSP_COMPILE_TIME_ASSERT(SSP_IP_MAX == sizeof(g_bsp_module_stop)));

    data.byte = g_bsp_module_stop[p_feature->id];
    if (BSP_MSTP_DATA_INVALID == data.byte)
    {
        /* Module has no module stop bit, clocks are enabled by default. */
        return SSP_ERR_INVALID_ARGUMENT;
    }

    uint32_t mask = r_bsp_get_mstp_mask(p_feature);
    volatile uint32_t * p_mstp_base = gp_mstp[data.bit.reg];

    /** Save the current module state */
    *p_stop = ((*p_mstp_base & mask) > 0);

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Start module (cancel module stop) or stop module (enter module stop).
 *
 * @note Some module stop bits are shared between peripherals.  Entering module stop is not permitted for these
 * peripherals unless force is true.
 *
 * @param[in] p_feature  Pointer to definition of the feature, defined by ssp_feature_t.
 * @param[in] stop       true to stop module, false to start module
 * @param[in] force      Set to true if the module should be stopped even if it is shared.
 *
 * @retval SSP_SUCCESS               Module is stopped or started based on stop parameter
 * @retval SSP_ERR_ASSERTION         p_feature::id is invalid
 * @retval SSP_ERR_INVALID_ARGUMENT  Module has no module stop bit, or module stop bit is shared and entering module
 *                                   stop is not supported because it could affect other modules.
 **********************************************************************************************************************/
static ssp_err_t r_bsp_module_start_stop(ssp_feature_t const * const p_feature, bool stop, bool force)
{
    bsp_mstp_data_t data = {0};

#if BSP_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(p_feature->id < SSP_IP_MAX);
#endif

    /** The g_bsp_module_stop array must have entries for each ssp_ip_t enum value. */
    /* LDRA will complain about "Statement with no side effect" w/o SSP_PARAMETER_NOT_USED */
    SSP_PARAMETER_NOT_USED(BSP_COMPILE_TIME_ASSERT(SSP_IP_MAX == sizeof(g_bsp_module_stop)));

    data.byte = g_bsp_module_stop[p_feature->id];
    if (BSP_MSTP_DATA_INVALID == data.byte)
    {
        /* Module has no module stop bit, clocks are enabled by default. */
        return SSP_ERR_INVALID_ARGUMENT;
    }

    uint32_t mask = r_bsp_get_mstp_mask(p_feature);

    volatile uint32_t * p_mstp_base = gp_mstp[data.bit.reg];

    if (stop)
    {
        if (!data.bit.exception || force)
        {
            SSP_CRITICAL_SECTION_DEFINE;
            SSP_CRITICAL_SECTION_ENTER;

            /** Set MSTP bit if stop parameter is true and the bit is not shared with other channels. */
            *p_mstp_base |= mask;

            SSP_CRITICAL_SECTION_EXIT;
        }
        else
        {
            /* Module stop bit is shared, so module is not stopped because this could affect other modules. */
            return SSP_ERR_INVALID_ARGUMENT;
        }
    }
    else
    {
        /** Clear MSTP bit if stop parameter is false. */
        mask          = (uint32_t) (~mask);

        SSP_CRITICAL_SECTION_DEFINE;
        SSP_CRITICAL_SECTION_ENTER;

        *p_mstp_base &= mask;

        SSP_CRITICAL_SECTION_EXIT;
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Get the correct MSTP mask for a peripheral
 *
 * @param[in] p_feature  Pointer to definition of the feature, defined by ssp_feature_t.
 *
 * @retval Mask for MSTP bit for peripheral
 **********************************************************************************************************************/
static uint32_t r_bsp_get_mstp_mask(ssp_feature_t const * const p_feature)
{
    bsp_mstp_data_t data = {0};
    data.byte = g_bsp_module_stop[p_feature->id];

    uint32_t mask = (1U << (data.bit.bit - (p_feature->channel + p_feature->unit)));
    if (data.bit.exception)
    {
        mask = (1U << (data.bit.bit));
        switch (p_feature->id)
        {
            case SSP_IP_GPT:
                if (p_feature->channel > HW_GPT_MSTPD5_MAX_CH)
                {
                    mask = (1U << (data.bit.bit + 1U));
                }

            break;
            default:
            break;
        }
    }
    return mask;
}

#endif /* if defined(BSP_MCU_GROUP_S124) */


/** @} (end defgroup BSP_MCU_MODULE_STOP_S124) */
