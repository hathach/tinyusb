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
 * File Name    : r_elc_api.h
 * Description  : ELC Interface
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @defgroup ELC_API events and peripheral definitions
 * @brief Interface for the Event Link Controller.
 *
 * Related SSP architecture topics:
 *  - @ref ssp-interfaces
 *  - @ref ssp-predefined-layers
 *  - @ref using-ssp-modules
 *
 * Event Link Controller Interface description: @ref HALELCInterface
 *
 * @{
 **********************************************************************************************************************/

#ifndef DRV_ELC_API_H
#define DRV_ELC_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/* Register definitions, common services and error codes. */
#include "bsp_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ELC_API_VERSION_MAJOR (2U)
#define ELC_API_VERSION_MINOR (0U)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Individual event link. The actual peripheral definitions can be found in the MCU specific (ie. /mcu/S124/bsp_elc.h)
 * bsp_elc.h files.*/
typedef struct st_elc_link
{
    elc_peripheral_t  peripheral;     ///< Peripheral to receive the signal
    elc_event_t       event;          ///< Signal that gets sent to the Peripheral
} elc_link_t;

/** Main configuration structure for the Event Link Controller */
typedef struct st_elc_cfg
{
    bool                autostart;   ///< Start operation and enable interrupts during open().
    uint32_t            link_count;  ///< Number of event links
    elc_link_t const  * link_list;   ///< Event links
} elc_cfg_t;

/** Software event number */
typedef enum e_elc_software_event
{
    ELC_SOFTWARE_EVENT_0,       ///< Software event 0
    ELC_SOFTWARE_EVENT_1,       ///< Software event 1
} elc_software_event_t;

/** ELC driver structure. General ELC functions implemented at the HAL layer follow this API. */
typedef struct st_elc_api
{
    /** Initialize all links in the Event Link Controller.
     * @par Implemented as
     * - R_ELC_Init()
     *
     * @param[in]   p_cfg   Pointer to configuration structure.
     **/
    ssp_err_t (* init)(elc_cfg_t const * const p_cfg);

    /** Generate a software event in the Event Link Controller.
     * @par Implemented as
     * - R_ELC_SoftwareEventGenerate()
     *
     * @param[in]   eventNum           Software event number to be generated.
     **/
    ssp_err_t (* softwareEventGenerate)(elc_software_event_t event_num);

    /** Create a single event link.
     * @par Implemented as
     * - R_ELC_LinkSet()
     *
     * @param[in]   peripheral The peripheral block that will receive the event signal.
     * @param[in]   signal     The event signal.
     **/
    ssp_err_t (* linkSet)(elc_peripheral_t peripheral, elc_event_t signal);

    /** Break an event link.
     * @par Implemented as
     * - R_ELC_LinkBreak()
     *
     * @param[in]   peripheral   The peripheral that should no longer be linked.
     **/
    ssp_err_t (* linkBreak)(elc_peripheral_t peripheral);

    /** Enable the operation of the Event Link Controller.
     * @par Implemented as
     * - R_ELC_Enable()
     **/
    ssp_err_t (* enable)(void);

    /** Disable the operation of the Event Link Controller.
     * @par Implemented as
     * - R_ELC_Disable()
     **/
    ssp_err_t (* disable)(void);

    /** Get the driver version based on compile time macros.
     * @par Implemented as
     * - R_ELC_VersionGet()
     *
     * @param[out]  p_version is value returned.
     **/
    ssp_err_t (* versionGet)(ssp_version_t * const p_version);
} elc_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_elc_instance
{
    elc_cfg_t const * p_cfg;     ///< Pointer to the configuration structure for this instance
    elc_api_t const * p_api;     ///< Pointer to the API structure for this instance
} elc_instance_t;

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* DRV_ELC_API_H */

/*******************************************************************************************************************//**
 * @} (end addtogroup ELC_API)
 **********************************************************************************************************************/
