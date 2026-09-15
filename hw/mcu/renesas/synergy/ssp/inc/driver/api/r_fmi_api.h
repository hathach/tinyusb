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
 * File Name    : r_fmi_api.h
 * Description  : FMI interface
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @defgroup FMI_API FMI Interface
 *
 * @brief Interface for reading on-chip factory information.
 *
 * @section FMI_API_SUMMARY Summary
 * The FMI (Factory MCU Information) module provides a function for reading the Product Information record.
 *
 * Related SSP architecture topics:
 *  - @ref ssp-interfaces
 *  - @ref ssp-predefined-layers
 *  - @ref using-ssp-modules
 *
 * FMI Interface description: @ref HALFMIInterface
 * @{
 **********************************************************************************************************************/
#ifndef DRV_FMI_API_H
#define DRV_FMI_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/** Register definitions, common services and error codes. */
#include "bsp_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define FMI_API_VERSION_MAJOR (2U)
#define FMI_API_VERSION_MINOR (0U)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef struct st_fmi_header
{
    uint32_t contents :8; // [0:7]
    uint32_t variant :8;  // [8:15]
    uint32_t count :8;    // [16:23]
    uint32_t minor :4;    // [24:27]
    uint32_t major :4;    // [28:31]
} fmi_header_t;

typedef struct st_fmi_product_info
{
    fmi_header_t       header;
    uint8_t            unique_id[16];       ///< DEPRECATED, use uniqueIdGet instead
    uint8_t            product_name[16];    /* No guarantee of null terminator. */
    uint8_t            product_marking[16]; /* No guarantee of null terminator. */
    /*LDRA_INSPECTED 381 S Anonymous structures and unions are allowed in SSP code. */
    struct
    {
        uint32_t mask_revision :8; /*  [0:7]  */
        uint32_t pin_count :10;    /*  [8:17] */
        uint32_t pkg_type :3;      /* [18:20] */
        uint32_t temp_range :3;    /* [21:23] */
        uint32_t quality_code :3;  /* [24:26] */
        uint32_t reserved :5;      /* [27:32] */
    };
    /*LDRA_INSPECTED 381 S Anonymous structures and unions are allowed in SSP code. */
    struct
    {
        uint32_t max_freq :8;       /* [0:7] */
        uint32_t reserved1;
    };
} fmi_product_info_t;

typedef struct st_fmi_unique_id
{
    uint32_t        unique_id[4];
} fmi_unique_id_t;

typedef struct st_fmi_feature_info
{
    void * ptr;
    /*LDRA_INSPECTED 381 S Anonymous structures and unions are allowed in SSP code. */
    struct
    {
        uint32_t channel_count:16; /* this is highest possible...not populated */
        uint32_t variant_data: 16;
        uint32_t extended_data_count:8;  /* the number of variant words for channel */
        uint32_t version_major:8;
        uint32_t version_minor:8;
    };
    void * ptr_extended_data;
} fmi_feature_info_t;

typedef struct st_fmi_event_info
{
    IRQn_Type   irq;
    elc_event_t event;
} fmi_event_info_t;

/** fmi driver structure. General fmi functions implemented at the HAL layer will follow this API. */
typedef struct st_fmi_api
{
    /** Initialize the FMI base pointer.
     * @par Implemented as
     * - R_FMI_Init()
     **/
    ssp_err_t (* init)(void);

    /** Get product information record address into caller's pointer.
     * @warning fmi_product_info_t::unique_id is deprecated and will not contain a unique ID if the factory
     * MCU information is linked in by the application code.  Use fmi_api_t::uniqueIdGet for the unique ID.
     * @param[in,out]  pp_product_info  Pointer to store pointer to product info.
     *
     * @par Implemented as
     * - R_FMI_ProductInfoGet()
     **/
    ssp_err_t (* productInfoGet)(fmi_product_info_t ** pp_product_info);

    /** Copy unique ID into p_unique_id.
     * @param[out]  p_unique_id  Pointer to unique ID.
     *
     * @par Implemented as
     * - R_FMI_UniqueIdGet()
     **/
    ssp_err_t (* uniqueIdGet)(fmi_unique_id_t * p_unique_id);

    /** Get feature information and store it in p_info.
     * @param[in]  p_feature  Definition of SSP feature.
     * @param[out] p_info     Feature specific information.
     *
     * @par Implemented as
     * - R_FMI_ProductFeatureGet()
     **/
    ssp_err_t (* productFeatureGet)(ssp_feature_t const * const p_feature, fmi_feature_info_t * const p_info);

    /** Get event information and store it in p_info.
     * @param[in]  p_feature  Definition of SSP feature.
     * @param[in]  signal     Feature signal ID.
     * @param[out] p_info     Event information for feature signal.
     *
     * @par Implemented as
     * - R_FMI_EventInfoGet()
     **/
    ssp_err_t (* eventInfoGet)(ssp_feature_t const * const p_feature, ssp_signal_t signal, fmi_event_info_t * const p_info);

    /** Get the driver version based on compile time macros.
     * @par Implemented as
     * - R_FMI_VersionGet()
     **/
    ssp_err_t (* versionGet)(ssp_version_t * const p_version);
} fmi_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_fmi_instance
{
    fmi_api_t const * p_api;     ///< Pointer to the API structure for this instance
} fmi_instance_t;

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* DRV_FMI_API_H */

/*******************************************************************************************************************//**
 * @} (end addtogroup FMI_API)
 **********************************************************************************************************************/
