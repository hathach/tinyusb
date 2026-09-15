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
 * File Name    : r_elc.c
 * Description  : HAL API code for the Event Link Controller module
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_elc.h"
#include "r_elc_private.h"
#include "r_elc_private_api.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** Macro for error logger. */
#ifndef ELC_ERROR_RETURN
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define ELC_ERROR_RETURN(a, err) SSP_ERROR_RETURN((a), (err), &g_module_name[0], &s_elc_version)
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
#if defined(__GNUC__)
/* This structure is affected by warnings from the GCC compiler bug gcc.gnu.org/bugzilla/show_bug.cgi?id=60784
 * This pragma suppresses the warnings in this structure only, and will be removed when the SSP compiler is updated to
 * v5.3.*/
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
/** Version data structure used by error logger macro. */
static const ssp_version_t s_elc_version =
{
    .api_version_minor  = ELC_API_VERSION_MINOR,
    .api_version_major  = ELC_API_VERSION_MAJOR,
    .code_version_major = ELC_CODE_VERSION_MAJOR,
    .code_version_minor = ELC_CODE_VERSION_MINOR
};
#if defined(__GNUC__)
/* Restore warning settings for 'missing-field-initializers' to as specified on command line. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic pop
#endif

static R_ELC_Type * gp_elc_reg = NULL;

/** Name of module used by error logger macro */
#if BSP_CFG_ERROR_LOG != 0
static const char          g_module_name[] = "elc";
#endif

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

/** ELC API structure.  */
/*LDRA_INSPECTED 27 D This structure must be accessible in user code. It cannot be static. */
const elc_api_t g_elc_on_elc =
{
    .init                  = R_ELC_Init,
    .softwareEventGenerate = R_ELC_SoftwareEventGenerate,
    .linkSet               = R_ELC_LinkSet,
    .linkBreak             = R_ELC_LinkBreak,
    .enable                = R_ELC_Enable,
    .disable               = R_ELC_Disable,
    .versionGet            = R_ELC_VersionGet
};

/*******************************************************************************************************************//**
 * @addtogroup ELC
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief  Initialize all the links in the Event Link Controller.
 *
 *  Implements elc_api_t::init
 *
 * The configuration structure passed in to this function includes
 * links for every event source included in the ELC and sets them
 * all at once. To set an individual link use R_ELC_LinkSet()
 *
 * @retval SSP_SUCCESS             Initialization was successful
 * @retval SSP_ERR_ASSERTION       p_config was NULL
 * @return                         See @ref Common_Error_Codes or functions called by this function for other possible
 *                                 return codes. This function calls:
 *                                     * fmi_api_t::productFeatureGet
 **********************************************************************************************************************/
ssp_err_t R_ELC_Init (elc_cfg_t const * const p_cfg)
{
    ssp_err_t err;
    uint32_t  i;

#if ELC_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(p_cfg);
#endif

    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0U}};
    ssp_feature.channel = 0U;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_ELC;
    fmi_feature_info_t info = {0U};
    err = g_fmi_on_fmi.productFeatureGet(&ssp_feature, &info);
    ELC_ERROR_RETURN(SSP_SUCCESS == err, err);
    gp_elc_reg = (R_ELC_Type *) info.ptr;

    /** Power on ELC */
    R_BSP_ModuleStart(&ssp_feature);

    /* Loop through all links in the config structure and set them in the ELC block */
    for (i = (uint32_t)0; i < p_cfg->link_count; i++)
    {
        R_ELC_LinkSet(p_cfg->link_list[i].peripheral, p_cfg->link_list[i].event);
    }

    /** Enable the operation of the Event Link Controller */
    if (p_cfg->autostart)
    {
        R_ELC_Enable();
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Generate a software event in the Event Link Controller.
 *
 *  Implements elc_api_t::softwareEventGenerate
 *
 * @retval SSP_SUCCESS             Initialization was successful
 * @retval SSP_ERR_ASSERTION       Invalid event number
 **********************************************************************************************************************/
ssp_err_t R_ELC_SoftwareEventGenerate (elc_software_event_t event_number)
{
    /** Generate a software event in the Event Link Controller. */
    switch (event_number)
    {
        case ELC_SOFTWARE_EVENT_0:
            HW_ELC_SoftwareEvent0Generate(gp_elc_reg);
            break;

        case ELC_SOFTWARE_EVENT_1:
            HW_ELC_SoftwareEvent1Generate(gp_elc_reg);
            break;

        default:
            SSP_ASSERT(0);
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Create a single event link.
 *
 *  Implements elc_api_t::linkSet
 *
 * @retval SSP_SUCCESS             Initialization was successful
 *
 **********************************************************************************************************************/
ssp_err_t R_ELC_LinkSet (elc_peripheral_t peripheral, elc_event_t signal)
{
    /** Make a link between a signal and a peripheral. */
    HW_ELC_LinkSet(gp_elc_reg, (uint32_t) peripheral, (uint16_t) signal);
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Break an event link.
 *
 *  Implements elc_api_t::linkBreak
 *
 * @retval SSP_SUCCESS             Event link broken
 *
 **********************************************************************************************************************/
ssp_err_t R_ELC_LinkBreak (elc_peripheral_t peripheral)
{
    /** Break a link between a signal and a peripheral. */
    HW_ELC_LinkBreak(gp_elc_reg, (uint32_t) peripheral);
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Enable the operation of the Event Link Controller.
 *
 *  Implements elc_api_t::enable
 *
 * @retval SSP_SUCCESS           ELC enabled.
 *
 **********************************************************************************************************************/
ssp_err_t R_ELC_Enable (void)
{
    /** Enable the Event Link Controller block. */
    HW_ELC_Enable(gp_elc_reg);
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Disable the operation of the Event Link Controller.
 *
 *  Implements elc_api_t::disable
 *
 * @retval SSP_SUCCESS           ELC disabled.
 *
 **********************************************************************************************************************/
ssp_err_t R_ELC_Disable (void)
{
    /** Disable the Event Link Controller block. */
    HW_ELC_Disable(gp_elc_reg);
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief      Get the driver version based on compile time macros.
 *
 *  Implements elc_api_t::versionGet
 *
 * @retval     SSP_SUCCESS          Successful close.
 * @retval     SSP_ERR_ASSERTION    p_version is NULL.
 *
 **********************************************************************************************************************/
ssp_err_t R_ELC_VersionGet (ssp_version_t * const p_version)
{
#if ELC_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(p_version);
#endif

    p_version->version_id = s_elc_version.version_id;

    return SSP_SUCCESS;
}

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @} (end addtogroup ELC)
 **********************************************************************************************************************/
