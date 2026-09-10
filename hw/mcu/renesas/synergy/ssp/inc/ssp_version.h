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
 * File Name    : ssp_version.h
 * Description  : SSP package version information.
 **********************************************************************************************************************/

#ifndef SSP_VERSION_H
#define SSP_VERSION_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/* Includes board and MCU related header files. */
#include "bsp_api.h"

/*******************************************************************************************************************//**
 * @ingroup Common_Version
 * @{
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** SSP pack major version. */
#define SSP_VERSION_MAJOR                   (2U)

/** SSP pack minor version. */
#define SSP_VERSION_MINOR                   (7U)

/** SSP pack patch version. */
#define SSP_VERSION_PATCH                   (0U)

/** SSP pack version build number (currently unused). */
#define SSP_VERSION_BUILD                   (0U)

/** Public SSP version name. */
#define SSP_VERSION_STRING                  ("2.7.0")

/** Unique SSP version ID. */
#define SSP_VERSION_BUILD_STRING            ("Built with Renesas Synergy (TM) Software Package version 2.7.0+build.0dc2a0521bad0495c1279b0532d8726b3ff9429b")

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** SSP Pack version structure */
typedef union st_ssp_pack_version
{
    /** Version id */
    uint32_t version_id;
    /** Code version parameters, little endian order. */
    /*LDRA_INSPECTED 381 S Anonymous structures and unions are allowed in SSP code. */
    struct
    {
        uint8_t build;     ///< Build version of SSP Pack
        uint8_t patch;     ///< Patch version of SSP Pack
        uint8_t minor;     ///< Minor version of SSP Pack
        uint8_t major;     ///< Major version of SSP Pack
    };
} ssp_pack_version_t;

/** @} (end ingroup Common_Error_Codes) */

#endif /* SSP_VERSION_H */
