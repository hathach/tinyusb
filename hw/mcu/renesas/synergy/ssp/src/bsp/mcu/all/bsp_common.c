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
* File Name    : bsp_common.c
* Description  : Implements functions common to all BSPs. Example: VersionGet() function.
***********************************************************************************************************************/


/***********************************************************************************************************************
 *
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"


/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
#if defined(__ICCARM__)
#define WEAK_ERROR_ATTRIBUTE
#define WEAK_INIT_ATTRIBUTE
#pragma weak ssp_error_log                            = ssp_error_log_internal
#pragma weak bsp_init                                 = bsp_init_internal
#elif defined(__GNUC__)
/*LDRA_INSPECTED 293 S */
#define WEAK_ERROR_ATTRIBUTE       __attribute__ ((weak, alias("ssp_error_log_internal")))
/*LDRA_INSPECTED 293 S */
#define WEAK_INIT_ATTRIBUTE        __attribute__ ((weak, alias("bsp_init_internal")))
#endif

#define SSP_SECTION_VERSION ".version"

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Private function prototypes
***********************************************************************************************************************/
/** Prototype of initialization function called before main.  This prototype sets the weak association of this
 * function to an internal example implementation. If this function is defined in the application code, the
 * application code version is used. */

/*LDRA_NOANALYSIS LDRA_INSPECTED below not working. */
/*LDRA_INSPECTED 110 D This function is weakly defined as a customer may choose to provide their own. */
void bsp_init(void * p_args) WEAK_INIT_ATTRIBUTE;
/*LDRA_ANALYSIS */
void bsp_init_internal(void * p_args);  /// Default initialization function

#if ((1 == BSP_CFG_ERROR_LOG) || (1 == BSP_CFG_ASSERT))
/** Prototype of function called before errors are returned in SSP code if BSP_CFG_ERROR_LOG is set to 1.  This
 * prototype sets the weak association of this function to an internal example implementation. */

/*LDRA_NOANALYSIS LDRA_INSPECTED below not working. */
/*LDRA_INSPECTED 110 D This function is weakly defined as a customer may choose to provide their own. */
void ssp_error_log(ssp_err_t err, const char * module, int32_t line) WEAK_ERROR_ATTRIBUTE;
/*LDRA_ANALYSIS */

void ssp_error_log_internal(ssp_err_t err, const char * module, int32_t line);  /// Default error logger function
#endif

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
#if defined(__GNUC__)
/* This pragma suppresses the warnings in this structure only, and will be removed when the SSP compiler is updated to v5.3.*/

/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
/* SSP pack version structure. */
static BSP_DONT_REMOVE const ssp_pack_version_t g_ssp_version BSP_PLACE_IN_SECTION_V2(SSP_SECTION_VERSION) =
{
    .minor = SSP_VERSION_MINOR,
    .major = SSP_VERSION_MAJOR,
    .build = SSP_VERSION_BUILD,
    .patch = SSP_VERSION_PATCH
};
#if defined(__GNUC__)
/* Restore warning settings for 'missing-field-initializers' to as specified on command line. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic pop
#endif

/* Public SSP version name. */
/*LDRA_INSPECTED 57 D - This global is being initialized at it's declaration below. */
static BSP_DONT_REMOVE const uint8_t g_ssp_version_string[] BSP_PLACE_IN_SECTION_V2(SSP_SECTION_VERSION) =
        SSP_VERSION_STRING;

/* Unique SSP version ID. */
/*LDRA_INSPECTED 57 D - This global is being initialized at it's declaration below. */
static BSP_DONT_REMOVE const uint8_t g_ssp_version_build_string[] BSP_PLACE_IN_SECTION_V2(SSP_SECTION_VERSION) =
        SSP_VERSION_BUILD_STRING;

/*******************************************************************************************************************//**
 * @addtogroup BSP_MCU_COMMON
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief      Set BSP version based on compile time macros.
 *
 * @param[out] p_version Memory address to return version information to.
 *
 * @retval SSP_SUCCESS          Version information stored.
 * @retval SSP_ERR_ASSERTION    The parameter p_version is NULL.
 **********************************************************************************************************************/
ssp_err_t R_BSP_VersionGet (ssp_version_t * p_version)
{
#if BSP_CFG_PARAM_CHECKING_ENABLE
    /** Verify parameters are valid */
    SSP_ASSERT(NULL != p_version);
#endif

    p_version->api_version_major  = BSP_API_VERSION_MAJOR;
    p_version->api_version_minor  = BSP_API_VERSION_MINOR;
    p_version->code_version_major = BSP_CODE_VERSION_MAJOR;
    p_version->code_version_minor = BSP_CODE_VERSION_MINOR;

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief      Set SSP version based on compile time macros.
 *
 * @param[out] p_version Memory address to return version information to.
 *
 * @retval SSP_SUCCESS          Version information stored.
 * @retval SSP_ERR_ASSERTION    The parameter p_version is NULL.
 **********************************************************************************************************************/
ssp_err_t R_SSP_VersionGet (ssp_pack_version_t * const p_version)
{
#if BSP_CFG_PARAM_CHECKING_ENABLE
    /** Verify parameters are valid */
    SSP_ASSERT(NULL != p_version);
#endif

    *p_version = g_ssp_version;

    return SSP_SUCCESS;
}

#if ((1 == BSP_CFG_ERROR_LOG) || (1 == BSP_CFG_ASSERT))
/*******************************************************************************************************************//**
 * @brief      Default error logger function, used only if ssp_error_log is not defined in the user application.
 *
 * @param[in]  err     The error code encountered.
 * @param[in]  module  The module name in which the error code was encountered.
 * @param[in]  line    The line number at which the error code was encountered.
 **********************************************************************************************************************/
void ssp_error_log_internal(ssp_err_t err, const char * module, int32_t line)
{
    /** Do nothing. Do not generate any 'unused' warnings...*/
    SSP_PARAMETER_NOT_USED(err);
    SSP_PARAMETER_NOT_USED(module);
    SSP_PARAMETER_NOT_USED(line);

}
#endif

/*******************************************************************************************************************//**
 * @brief      Default initialization function, used only if bsp_init is not defined in the user application.
 **********************************************************************************************************************/
void bsp_init_internal(void * p_args)
{
    /* Do nothing. */
    SSP_PARAMETER_NOT_USED(p_args);
}

/** @} (end addtogroup BSP_MCU_COMMON) */
