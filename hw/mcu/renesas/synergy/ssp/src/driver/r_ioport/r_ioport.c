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
 * File Name    : r_ioport.c
 * Description  : IOPORT HAL API implementation.
 **********************************************************************************************************************/




/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdint.h>
#include "r_ioport.h"
#include "r_ioport_private.h"
#include "r_ioport_api.h"
#include "./hw/hw_ioport_private.h"
#include "r_ioport_private_api.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** Macro for error logger. */
#ifndef IOPORT_ERROR_RETURN
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define IOPORT_ERROR_RETURN(a, err) SSP_ERROR_RETURN((a), (err), &g_module_name[0], &g_ioport_version)
#endif

/** Mask to get PSEL bitfield from PFS register. */
#define BSP_PRV_PFS_PSEL_MASK         (0x1F000000uL)

/** Shift to get pin 0 on a package in extended data. */
#define IOPORT_PRV_EXISTS_B0_SHIFT    (16UL)

/** Mask to determine if any pins on port exist on this package. */
#define IOPORT_PRV_PORT_EXISTS_MASK   (0xFFFF0000U)

/** Shift to get port in ioport_port_t and ioport_port_pin_t enums. */
#define IOPORT_PRV_PORT_OFFSET        (8U)

#ifndef BSP_MCU_VBATT_SUPPORT
#define BSP_MCU_VBATT_SUPPORT         (0U)
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
static ssp_err_t r_ioport_pin_exists(ioport_port_pin_t pin);
static ssp_err_t r_ioport_port_exists(ioport_port_t port);
#endif

#if BSP_MCU_VBATT_SUPPORT
static void bsp_vbatt_init(ioport_cfg_t const * const p_pin_cfg);   // Used internally by BSP
#endif

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/** Name of module used by error logger macro */
#if BSP_CFG_ERROR_LOG != 0
static const char g_module_name[] = "ioport";
#endif

#if defined(__GNUC__)
/* This structure is affected by warnings from a GCC compiler bug. This pragma suppresses the warnings in this 
 * structure only. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
/** Version data structure used by error logger macro. */
static const ssp_version_t g_ioport_version =
{
    .api_version_minor  = IOPORT_API_VERSION_MINOR,
    .api_version_major  = IOPORT_API_VERSION_MAJOR,
    .code_version_major = IOPORT_CODE_VERSION_MAJOR,
    .code_version_minor = IOPORT_CODE_VERSION_MINOR
};
#if defined(__GNUC__)
/* Restore warning settings for 'missing-field-initializers' to as specified on command line. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic pop
#endif

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
/** IOPort Implementation of IOPort Driver  */
/*LDRA_INSPECTED 27 D This structure must be accessible in user code. It cannot be static. */
const ioport_api_t g_ioport_on_ioport =
{
    .init                 = R_IOPORT_Init,
    .pinCfg               = R_IOPORT_PinCfg,
    .pinsCfg              = R_IOPORT_PinsCfg,
    .pinDirectionSet      = R_IOPORT_PinDirectionSet,
    .pinEventInputRead    = R_IOPORT_PinEventInputRead,
    .pinEventOutputWrite  = R_IOPORT_PinEventOutputWrite,
    .pinEthernetModeCfg   = R_IOPORT_EthernetModeCfg,
    .pinRead              = R_IOPORT_PinRead,
    .pinWrite             = R_IOPORT_PinWrite,
    .portDirectionSet     = R_IOPORT_PortDirectionSet,
    .portEventInputRead   = R_IOPORT_PortEventInputRead,
    .portEventOutputWrite = R_IOPORT_PortEventOutputWrite,
    .portRead             = R_IOPORT_PortRead,
    .portWrite            = R_IOPORT_PortWrite,
    .versionGet           = R_IOPORT_VersionGet,
};

/** Pointer to IOPORT base register. */
/*LDRA_INSPECTED 219 S In the GCC compiler, section placement requires a GCC attribute, which starts with underscore. */
/* This variable is not initialized at declaration because it is initialized and used before C runtime initialization */
/*LDRA_INSPECTED 57 D *//*LDRA_INSPECTED 57 D */
static R_IOPORT1_Type * gp_ioport_reg BSP_PLACE_IN_SECTION_V2(".noinit");

/** Pointer to PFS base register. */
/*LDRA_INSPECTED 219 S In the GCC compiler, section placement requires a GCC attribute, which starts with underscore. */
/* This variable is not initialized at declaration because it is initialized and used before C runtime initialization */
/*LDRA_INSPECTED 57 D *//*LDRA_INSPECTED 57 D */
static R_PFS_Type * gp_pfs_reg BSP_PLACE_IN_SECTION_V2(".noinit");

/** Pointer to PMISC base register. */
/*LDRA_INSPECTED 219 S In the GCC compiler, section placement requires a GCC attribute, which starts with underscore. */
/* This variable is not initialized at declaration because it is initialized and used before C runtime initialization */
/*LDRA_INSPECTED 57 D *//*LDRA_INSPECTED 57 D */
static R_PMISC_Type * gp_pmisc_reg BSP_PLACE_IN_SECTION_V2(".noinit");

/** Pointer to IOPORT extended variant data, used to determine if pins exist on this package. */
/*LDRA_INSPECTED 219 S In the GCC compiler, section placement requires a GCC attribute, which starts with underscore. */
/* This variable is not initialized at declaration because it is initialized and used before C runtime initialization */
/*LDRA_INSPECTED 57 D *//*LDRA_INSPECTED 57 D */
static uint32_t const * gp_ioport_exists BSP_PLACE_IN_SECTION_V2(".noinit");

/** Number of I/O ports in this package. */
/*LDRA_INSPECTED 219 S In the GCC compiler, section placement requires a GCC attribute, which starts with underscore. */
/* This variable is not initialized at declaration because it is initialized and used before C runtime initialization */
/*LDRA_INSPECTED 57 D *//*LDRA_INSPECTED 57 D */
static uint32_t g_ioport_num_ports BSP_PLACE_IN_SECTION_V2(".noinit");

#if BSP_MCU_VBATT_SUPPORT
static const ioport_port_pin_t g_vbatt_pins_input[] =
{
    IOPORT_PORT_04_PIN_02,  ///< Associated with VBTICTLR->VCH0INEN
    IOPORT_PORT_04_PIN_03,  ///< Associated with VBTICTLR->VCH1INEN
    IOPORT_PORT_04_PIN_04   ///< Associated with VBTICTLR->VCH2INEN
};
#endif

/** Used for holding reference counters for PFSWE. */
volatile uint16_t g_protect_pfswe_counter = 0U;

/*******************************************************************************************************************//**
 * @addtogroup IOPORT
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief  Initializes internal driver data, then calls R_IOPORT_PinsCfg to configure pins.
 *
 * @retval SSP_SUCCESS                  Pin configuration data written to PFS register(s)
 * @retval SSP_ERR_ASSERTION            NULL pointer
 * @return                              See @ref Common_Error_Codes or functions called by this function for other possible
 *                                      return codes. This function calls:
 *                                        * fmi_api_t::productFeatureGet
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_Init (const ioport_cfg_t * p_cfg)
{
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    SSP_ASSERT(NULL != p_cfg);
    SSP_ASSERT(NULL != p_cfg->p_pin_cfg_data);
#endif

    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0U}};
    fmi_feature_info_t info = {0U};
    ssp_feature.channel = 0U;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_IOPORT;
    g_fmi_on_fmi.productFeatureGet(&ssp_feature, &info);
    gp_ioport_reg = (R_IOPORT1_Type *) info.ptr;
    gp_ioport_exists = info.ptr_extended_data;
    g_ioport_num_ports = info.channel_count;
    ssp_feature.id = SSP_IP_PFS;
    g_fmi_on_fmi.productFeatureGet(&ssp_feature, &info);
    gp_pfs_reg = (R_PFS_Type *) info.ptr;
    ssp_feature.unit = 1U;
    g_fmi_on_fmi.productFeatureGet(&ssp_feature, &info);
    gp_pmisc_reg = (R_PMISC_Type *) info.ptr;

    return R_IOPORT_PinsCfg(p_cfg);
} /* End of function R_IOPORT_Init */

/*******************************************************************************************************************//**
 * @brief  Configures the functions of multiple pins by loading configuration data into pin PFS registers.
 * Implements ioport_api_t::pinsCfg.
 *
 * This function initializes the supplied list of PmnPFS registers with the supplied values.
 * This data can be generated by the ISDE pin configurator or manually by the developer. Different pin configurations
 * can be loaded for different situations such as low power modes and test.*
 *
 * @retval SSP_SUCCESS                  Pin configuration data written to PFS register(s)
 * @retval SSP_ERR_ASSERTION            NULL pointer
 *
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PinsCfg (const ioport_cfg_t * p_cfg)
{
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    SSP_ASSERT(NULL != p_cfg);
    SSP_ASSERT(NULL != p_cfg->p_pin_cfg_data);
#endif

#if BSP_MCU_VBATT_SUPPORT
    /** Handle any VBATT domain pin configuration. */
    bsp_vbatt_init(p_cfg);
#endif

    uint16_t     pin_count;
    ioport_cfg_t * p_pin_data;

    p_pin_data = (ioport_cfg_t *) p_cfg;

    for (pin_count = 0u; pin_count < p_pin_data->number_of_pins; pin_count++)
    {
        HW_IOPORT_PFSWrite(gp_pfs_reg, gp_pmisc_reg, p_pin_data->p_pin_cfg_data[pin_count].pin, p_pin_data->p_pin_cfg_data[pin_count].pin_cfg);
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Configures the settings of a pin. Implements ioport_api_t::pinCfg.
 *
 * @retval SSP_SUCCESS                    Pin configured.
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid pin
 *
 * @note This function is re-entrant for different pins.
 * This function will change the configuration of the pin with the new configuration. For example it is not possible
 * with this function to change the drive strength of a pin while leaving all the other pin settings unchanged. To
 * achieve this the original settings with the required change will need to be written using this function.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PinCfg (ioport_port_pin_t pin, uint32_t cfg)
{
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_pin_exists(pin);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

#if BSP_MCU_VBATT_SUPPORT
    /** Create temporary structure for handling VBATT pins. */
    ioport_cfg_t     temp_cfg;
    ioport_pin_cfg_t temp_pin_cfg;

    temp_pin_cfg.pin = pin;
    temp_pin_cfg.pin_cfg = cfg;

    temp_cfg.number_of_pins = 1U;
    temp_cfg.p_pin_cfg_data = &temp_pin_cfg;

    /** Handle any VBATT domain pin configuration. */
    bsp_vbatt_init(&temp_cfg);
#endif

    HW_IOPORT_PFSWrite(gp_pfs_reg, gp_pmisc_reg, pin, cfg);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PinCfg */

/*******************************************************************************************************************//**
 * @brief  Reads the level on a pin. Implements ioport_api_t::pinRead.
 *
 * @retval SSP_SUCCESS                    Pin read.
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid argument
 * @retval SSP_ERR_ASSERTION                NULL pointer
 *
 * @note This function is re-entrant for different pins.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PinRead (ioport_port_pin_t pin, ioport_level_t * p_pin_value)
{
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_pin_exists(pin);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
    SSP_ASSERT(NULL != p_pin_value);
#endif

    *p_pin_value = HW_IOPORT_PinRead(gp_pfs_reg, pin);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PinRead */

/*******************************************************************************************************************//**
 * @brief  Reads the value on an IO port. Implements ioport_api_t::portRead.
 *
 * The specified port will be read, and the levels for all the pins will be returned.
 * Each bit in the returned value corresponds to a pin on the port. For example, bit 7 corresponds
 * to pin 7, bit 6 to pin 6, and so on. *
 *
 * @retval SSP_SUCCESS              Port read.
 * @retval SSP_ERR_INVALID_ARGUMENT     Port not valid.
 * @retval SSP_ERR_ASSERTION            NULL pointer
 *
 * @note This function is re-entrant for different ports.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PortRead (ioport_port_t port, ioport_size_t * p_port_value)
{
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_port_exists(port);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
    SSP_ASSERT(NULL != p_port_value);
#endif

    *p_port_value = HW_IOPORT_PortRead(gp_ioport_reg, port);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PortRead */

/*******************************************************************************************************************//**
 * @brief  Writes to multiple pins on a port. Implements ioport_api_t::portWrite.
 *
 * The input value will be written to the specified port. Each bit in the value parameter corresponds to a bit
 * on the port. For example, bit 7 corresponds to pin 7, bit 6 to pin 6, and so on.
 * Each bit in the mask parameter corresponds to a pin on the port.
 *
 * Only the bits with the corresponding bit in the mask value set will be updated.
 * e.g. value = 0xFFFF, mask = 0x0003 results in only bits 0 and 1 being updated.
 *
 * @retval SSP_SUCCESS                  Port written to.
 * @retval SSP_ERR_INVALID_ARGUMENT     The port and/or mask not valid.
 *
 * @note This function is re-entrant for different ports. This function makes use of the PCNTR3 register to atomically
 * modify the levels on the specified pins on a port.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PortWrite (ioport_port_t port, ioport_size_t value, ioport_size_t mask)
{
    ioport_size_t setbits;
    ioport_size_t clrbits;

#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    /* Cast to ensure correct conversion of parameter. */
    IOPORT_ERROR_RETURN(mask > (ioport_size_t)0, SSP_ERR_INVALID_ARGUMENT);
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_port_exists(port);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

    /** High bits */
    setbits = value & mask;

    /** Low bits */
    /* Cast to ensure size */
    clrbits = (ioport_size_t)((~value) & mask);

    HW_IOPORT_PortWriteWithPCNTR3(gp_ioport_reg, port, setbits, clrbits);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PortWrite */

/*******************************************************************************************************************//**
 * @brief  Sets a pin's output either high or low. Implements ioport_api_t::pinWrite.
 *
 * @retval SSP_SUCCESS              Pin written to.
 * @retval SSP_ERR_INVALID_ARGUMENT     The pin and/or level not valid.
 *
 * @note This function is re-entrant for different pins. This function makes use of the PCNTR3 register to atomically
 * modify the level on the specified pin on a port.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PinWrite (ioport_port_pin_t pin, ioport_level_t level)
{
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    IOPORT_ERROR_RETURN(level <= IOPORT_LEVEL_HIGH, SSP_ERR_INVALID_ARGUMENT);
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_pin_exists(pin);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

    ioport_size_t setbits = 0U;
    ioport_size_t clrbits = 0U;
    ioport_port_t port = (ioport_port_t) (0xFF00U & (ioport_size_t) pin);

    ioport_size_t shift = 0x00FFU & (ioport_size_t) pin;
    ioport_size_t pin_mask = (ioport_size_t) (1U << shift);

    if (IOPORT_LEVEL_LOW == level)
    {
        clrbits = pin_mask;
    }
    else
    {
        setbits = pin_mask;
    }

    HW_IOPORT_PortWriteWithPCNTR3(gp_ioport_reg, port, setbits, clrbits);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PinWrite */

/*******************************************************************************************************************//**
 * @brief  Sets the direction of individual pins on a port. Implements ioport_api_t::portDirectionSet().
 *
 * Multiple pins on a port can be set to inputs or outputs at once.
 * Each bit in the mask parameter corresponds to a pin on the port. For example, bit 7 corresponds to
 * pin 7, bit 6 to pin 6, and so on. If a bit is set to 1 then the corresponding pin will be changed to
 * an input or an output as specified by the direction values. If a mask bit is set to 0 then the direction of
 * the pin will not be changed.
 *
 * @retval SSP_SUCCESS              Port direction updated.
 * @retval SSP_ERR_INVALID_ARGUMENT    The port and/or mask not valid.
 *
 * @note This function is re-entrant for different ports.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PortDirectionSet (ioport_port_t port, ioport_size_t direction_values, ioport_size_t mask)
{
    ioport_size_t orig_value;
    ioport_size_t set_bits;
    ioport_size_t clr_bits;
    ioport_size_t write_value;
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    /* Cast to ensure correct conversion of parameter. */
    IOPORT_ERROR_RETURN(mask > (ioport_size_t)0, SSP_ERR_INVALID_ARGUMENT);
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_port_exists(port);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

    orig_value = HW_IOPORT_PortDirectionRead(gp_ioport_reg, port);

    /** High bits */
    set_bits = direction_values & mask;

    /**  Low bits */
    /* Cast to ensure size */
    clr_bits = (ioport_size_t)((~direction_values) & mask);

    /** New value to write to port direction register */
    write_value  = orig_value;
    write_value |= set_bits;
    /* Cast to ensure size */
    write_value &= (ioport_size_t)(~clr_bits);

    HW_IOPORT_PortDirectionSet(gp_ioport_reg, port, write_value);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PortDirectionSet */

/*******************************************************************************************************************//**
 * @brief  Sets the direction of an individual pin on a port. Implements ioport_api_t::pinDirectionSet.
 *
 * @retval SSP_SUCCESS              Pin direction updated.
 * @retval SSP_ERR_INVALID_ARGUMENT    The pin and/or direction not valid.
 *
 * @note This function is re-entrant for different pins.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PinDirectionSet (ioport_port_pin_t pin, ioport_direction_t direction)
{
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    IOPORT_ERROR_RETURN((direction == IOPORT_DIRECTION_INPUT) || (direction == IOPORT_DIRECTION_OUTPUT),
                        SSP_ERR_INVALID_ARGUMENT);
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_pin_exists(pin);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

    HW_IOPORT_PinDirectionSet(gp_pfs_reg, gp_pmisc_reg, pin, direction);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PinDirectionSet */

/*******************************************************************************************************************//**
 * @brief  Reads the value of the event input data. Implements ioport_api_t::portEventInputRead().
 *
 * The event input data for the port will be read. Each bit in the returned value corresponds to a pin on the port.
 * For example, bit 7 corresponds to pin 7, bit 6 to pin 6, and so on.
 *
 * The port event data is captured in response to a trigger from the ELC. This function enables this data to be read.
 * Using the event system allows the captured data to be stored when it occurs and then read back at a later time.
 *
 * @retval SSP_SUCCESS              Port read.
 * @retval SSP_ERR_INVALID_ARGUMENT     Port not valid.
 * @retval SSP_ERR_ASSERTION            NULL pointer
 *
 * @note This function is re-entrant for different ports.
 *
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PortEventInputRead (ioport_port_t port, ioport_size_t * p_event_data)
{
#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_port_exists(port);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
    SSP_ASSERT(NULL != p_event_data);
#endif

    *p_event_data = HW_IOPORT_PortEventInputDataRead(gp_ioport_reg, port);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PortEventInputRead */

/*******************************************************************************************************************//**
 * @brief  Reads the value of the event input data of a specific pin. Implements ioport_api_t::pinEventInputRead.
 *
 * The pin event data is captured in response to a trigger from the ELC. This function enables this data to be read.
 * Using the event system allows the captured data to be stored when it occurs and then read back at a later time.
 *
 * @retval SSP_SUCCESS                  Pin read.
 * @retval SSP_ERR_INVALID_ARGUMENT  Pin not valid.
 * @retval SSP_ERR_ASSERTION            NULL pointer
 *
 * @note This function is re-entrant.
 *
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PinEventInputRead (ioport_port_pin_t pin, ioport_level_t * p_pin_event)
{
    ioport_size_t portvalue;
    ioport_size_t mask;
    ioport_port_t port;
    uint16_t      pin_to_port;

#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_pin_exists(pin);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
    SSP_ASSERT(NULL != p_pin_event);
#endif

    /* Cast to ensure correct conversion of parameter. */
    pin_to_port = (uint16_t)pin;
    pin_to_port = pin_to_port & (uint16_t)0xFF00;
    port = (ioport_port_t)pin_to_port;
    portvalue = HW_IOPORT_PortEventInputDataRead(gp_ioport_reg, port);
    mask      = (ioport_size_t) (1U << (0x00FFU & (ioport_port_t) pin));

    if ((portvalue & mask) == mask)
    {
        *p_pin_event = IOPORT_LEVEL_HIGH;
    }
    else
    {
        *p_pin_event = IOPORT_LEVEL_LOW;
    }

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PinEventInputRead */

/*******************************************************************************************************************//**
 * @brief  This function writes the set and reset event output data for a port. Implements
 *ioport_api_t::portEventOutputWrite.
 *
 * Using the event system enables a port state to be stored by this function in advance of being output on the port.
 * The output to the port will occur when the ELC event occurs.
 *
 * The input value will be written to the specified port when an ELC event configured for that port occurs.
 * Each bit in the value parameter corresponds to a bit on the port. For example, bit 7 corresponds to pin 7,
 * bit 6 to pin 6, and so on. Each bit in the mask parameter corresponds to a pin on the port.
 *
 * @retval SSP_SUCCESS              Port event data written.
 * @retval SSP_ERR_INVALID_ARGUMENT     Port and/or mask not valid.
 * *
 * @note This function is re-entrant for different ports.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PortEventOutputWrite (ioport_port_t port, ioport_size_t event_data, ioport_size_t mask_value)
{
    ioport_size_t set_bits;
    ioport_size_t reset_bits;

#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_port_exists(port);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
    IOPORT_ERROR_RETURN(mask_value > (ioport_size_t)0, SSP_ERR_INVALID_ARGUMENT);
#endif

    set_bits   = event_data & mask_value;
    /* Cast to ensure size */
    reset_bits = (ioport_size_t)((~event_data) & mask_value);

    HW_IOPORT_PortEventOutputDataWrite(gp_ioport_reg, port, set_bits, reset_bits);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PortEventOutputWrite */

/**********************************************************************************************************************//**
 * @brief  This function writes the event output data value to a pin. Implements ioport_api_t::pinEventOutputWrite.
 *
 * Using the event system enables a pin state to be stored by this function in advance of being output on the pin.
 * The output to the pin will occur when the ELC event occurs.
 *
 * @retval SSP_SUCCESS              Pin event data written.
 * @retval SSP_ERR_INVALID_ARGUMENT  Pin or value not valid.
 *
 * @note This function is re-entrant for different ports.
 *
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_PinEventOutputWrite (ioport_port_pin_t pin, ioport_level_t pin_value)
{
    ioport_size_t set_bits;
    ioport_size_t reset_bits;
    ioport_port_t port;
    uint16_t      pin_to_port;

#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    ssp_err_t err = SSP_SUCCESS;
    err = r_ioport_pin_exists(pin);
    IOPORT_ERROR_RETURN(SSP_SUCCESS == err, err);
    IOPORT_ERROR_RETURN((pin_value == IOPORT_LEVEL_HIGH) || (pin_value == IOPORT_LEVEL_LOW), SSP_ERR_INVALID_ARGUMENT);
#endif

    /* Cast to ensure correct conversion of parameter. */
    pin_to_port = (uint16_t)pin;
    pin_to_port = pin_to_port & (uint16_t)0xFF00;
    port = (ioport_port_t)pin_to_port;
    set_bits   = (ioport_size_t)0;
    reset_bits = (ioport_size_t)0;

    if (IOPORT_LEVEL_HIGH == pin_value)
    {
        /* Cast to ensure size */
        set_bits = (ioport_size_t)(1U << ((ioport_size_t) pin & 0x00FFU));
    }
    else
    {
        /* Cast to ensure size */
        reset_bits = (ioport_size_t)(1U << ((ioport_size_t) pin & 0x00FFU));
    }

    HW_IOPORT_PinEventOutputDataWrite(gp_ioport_reg, port, set_bits, reset_bits, pin_value);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_PinEventOutputWrite */

/*******************************************************************************************************************//**
 * @brief   Returns IOPort HAL driver version. Implements ioport_api_t::versionGet.
 *
 * @retval      SSP_SUCCESS    Version information read.
 * @retval      SSP_ERR_ASSERTION The parameter p_data is NULL.
 *
 * @note This function is reentrant.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_VersionGet (ssp_version_t * p_data)
{
#if (1 == IOPORT_CFG_PARAM_CHECKING_ENABLE)
    /** Verify parameters are valid */
    SSP_ASSERT(NULL != p_data);
#endif

    *p_data = g_ioport_version;

    return SSP_SUCCESS;
} /* End of function R_IOPORT_VersionGet */

/*******************************************************************************************************************//**
 * @brief  Configures Ethernet channel PHY mode. Implements ioport_api_t::ethModeCfg.
 *
 * @retval SSP_SUCCESS              Ethernet PHY mode set.
 * @retval SSP_ERR_INVALID_ARGUMENT Channel or mode not valid.
 * @retval SSP_ERR_UNSUPPORTED      Ethernet configuration not supported on this device.
 *
 * @note This function is not re-entrant.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_EthernetModeCfg (ioport_ethernet_channel_t channel, ioport_ethernet_mode_t mode)
{
    bsp_feature_ioport_t ioport_feature = {0U};
    R_BSP_FeatureIoportGet(&ioport_feature);

    IOPORT_ERROR_RETURN(1U == ioport_feature.has_ethernet, SSP_ERR_UNSUPPORTED);

#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
    IOPORT_ERROR_RETURN(channel < IOPORT_ETHERNET_CHANNEL_END, SSP_ERR_INVALID_ARGUMENT);
    IOPORT_ERROR_RETURN(mode < IOPORT_ETHERNET_MODE_END, SSP_ERR_INVALID_ARGUMENT);
#endif

    HW_IOPORT_EthernetModeCfg(gp_pmisc_reg, channel, mode);

    return SSP_SUCCESS;
} /* End of function R_IOPORT_EthModeCfg */

/*******************************************************************************************************************//**
 * @} (end addtogroup IOPORT)
 **********************************************************************************************************************/

#if ((1 == IOPORT_CFG_PARAM_CHECKING_ENABLE))
/******************************************************************************************************************//**
 * @brief  Checks the existence of the valid pin as per port.
 *
 * @param[in]   pin                  Pin number to be used.
 *
 * @retval SSP_SUCCESS               Pin number is valid.
 * @retval SSP_ERR_INVALID_ARGUMENT  An invalid pin number is used.
 *********************************************************************************************************************/
static ssp_err_t r_ioport_pin_exists(ioport_port_pin_t pin)
{
    uint32_t port_local = (uint32_t) pin >> IOPORT_PRV_PORT_OFFSET;
    uint32_t pin_local = (uint32_t) pin & 0xFFU;
    if (port_local < g_ioport_num_ports)
    {
        uint32_t pin_mask = 1U << (IOPORT_PRV_EXISTS_B0_SHIFT + pin_local);
        if (0U != (gp_ioport_exists[port_local] & pin_mask))
        {
            return SSP_SUCCESS;
        }
    }
    return SSP_ERR_INVALID_ARGUMENT;
}

/******************************************************************************************************************//**
* @brief  Checks the existence of the valid port.
*
* @param[in]   port                 Port number to be used.
*
* @retval SSP_SUCCESS               Port number is valid.
* @retval SSP_ERR_INVALID_ARGUMENT  An invalid port number is used.
*********************************************************************************************************************/
static ssp_err_t r_ioport_port_exists(ioport_port_t port)
{
    uint32_t port_local = (uint32_t) port >> IOPORT_PRV_PORT_OFFSET;
    if (port_local < g_ioport_num_ports)
    {
        return SSP_SUCCESS;
    }
    return SSP_ERR_INVALID_ARGUMENT;
}
#endif

#if BSP_MCU_VBATT_SUPPORT
/*******************************************************************************************************************//**
 * @brief Initializes VBTICTLR register based on pin configuration.
 *
 * @param[in,out]   p_pin_cfg       Pointer to pin configuration data.
 *
 * The VBTICTLR register may need to be modified based on the project's pin configuration. There is a set of pins that
 * needs to be checked. If one of these pins is found in the pin configuration table then it will be tested to see if
 * the appropriate VBTICTLR bit needs to be set or cleared. If one of the pins that is being searched for is not found
 * then the accompanying VBTICTLR bit is left as-is.
 **********************************************************************************************************************/
static void bsp_vbatt_init (ioport_cfg_t const * const p_pin_cfg)
{
    uint32_t pin_index;
    uint32_t vbatt_index;
    uint8_t  local_vbtictlr_set;    ///< Will hold bits to set in VBTICTLR
    uint8_t  local_vbtictlr_clear;  ///< Will hold bits to clear in VBTICTLR

    /** Make no changes unless required. */
    local_vbtictlr_set = 0U;
    local_vbtictlr_clear = 0U;

    /** Must loop over all pins as pin configuration table is unordered. */
    for (pin_index = 0U; pin_index < p_pin_cfg->number_of_pins; pin_index++)
    {
        /** Loop over VBATT input pins. */
        for (vbatt_index = 0U; vbatt_index < (sizeof(g_vbatt_pins_input)/sizeof(g_vbatt_pins_input[0])); vbatt_index++)
        {
            if (p_pin_cfg->p_pin_cfg_data[pin_index].pin == g_vbatt_pins_input[vbatt_index])
            {
                /** Get PSEL value for pin. */
                uint32_t pfs_psel_value = p_pin_cfg->p_pin_cfg_data[pin_index].pin_cfg & BSP_PRV_PFS_PSEL_MASK;

                /** Check if pin is being used for RTC or AGT use. */
                if ((IOPORT_PERIPHERAL_AGT == pfs_psel_value) || (IOPORT_PERIPHERAL_CLKOUT_COMP_RTC == pfs_psel_value))
                {
                    /** Bit should be set to 1. */
                    local_vbtictlr_set |= (uint8_t)(1U << vbatt_index);
                }
                else
                {
                    /** Bit should be cleared to 0. */
                    local_vbtictlr_clear |= (uint8_t)(1U << vbatt_index);
                }
            }
        }
    }

    /** Disable write protection on VBTICTLR. */
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_OM_LPC_BATT);

    /** Read value, set and clear bits as needed and write back. */
    uint8_t local_vbtictlr = R_SYSTEM->VBTICTLR;
    local_vbtictlr |= local_vbtictlr_set;               ///< Set appropriate bits
    local_vbtictlr &= (uint8_t)~local_vbtictlr_clear;   ///< Clear appropriate bits

    R_SYSTEM->VBTICTLR = local_vbtictlr;

    /** Enable write protection on VBTICTLR. */
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_OM_LPC_BATT);
}

#endif
