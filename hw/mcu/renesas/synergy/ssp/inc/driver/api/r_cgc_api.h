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
 * File Name    : r_cgc_api.h
 * Description  : API for the Clock Generation Circuit (CGC) module.
 **********************************************************************************************************************/

#ifndef DRV_CGC_API_H
#define DRV_CGC_API_H

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @defgroup CGC_API CGC Interface
 * @brief Interface for clock generation.
 *
 * @section CGC_API_SUMMARY Summary
 *
 * The CGC interface provides the ability to configure and use all of the CGC module's capabilities. Among the
 * capabilities is the selection of several clock sources to use as the system clock source. Additionally, the
 * system clocks can be divided down to provide a wide range of frequencies for various system and peripheral needs.
 *
 * Clock stability can be checked and clocks may also be stopped to save power when not needed. The API has a function
 * to return the frequency of the system and system peripheral clocks at run time. There is also a feature to detect
 * when the main oscillator has stopped, with the option of calling a user provided callback function.
 *
 * Related SSP architecture topics:
 *  - @ref ssp-interfaces
 *  - @ref ssp-predefined-layers
 *  - @ref using-ssp-modules
 *
 * CGC Interface description: @ref ModuleCGConCGC
 *
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/* Includes board and MCU related header files. */
#include "bsp_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/* Version Number of API. */
#define CGC_API_VERSION_MAJOR (2U)
#define CGC_API_VERSION_MINOR (0U)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Events that can trigger a callback function */
typedef enum e_cgc_event
{
    CGC_EVENT_OSC_STOP_DETECT       ///< Oscillator stop detection has caused the event.
} cgc_event_t;

/** Callback function parameter data */
typedef struct st_cgc_callback_args
{
    cgc_event_t     event;          ///< The event can be used to identify what caused the callback.
    void const    * p_context;      ///< Placeholder for user data.
} cgc_callback_args_t;


/** System clock source identifiers -  The source of ICLK, BCLK, FCLK, PCLKS A-D and UCLK prior to the system clock
 * divider */
typedef enum e_cgc_clock
{
    CGC_CLOCK_HOCO      = 0x00,     ///< The high speed on chip oscillator.
    CGC_CLOCK_MOCO      = 0x01,     ///< The middle speed on chip oscillator.
    CGC_CLOCK_LOCO      = 0x02,     ///< The low speed on chip oscillator.
    CGC_CLOCK_MAIN_OSC  = 0x03,     ///< The main oscillator.
    CGC_CLOCK_SUBCLOCK  = 0x04,     ///< The subclock oscillator.
    CGC_CLOCK_PLL       = 0x05,     ///< The PLL oscillator.
} cgc_clock_t;

/** PLL divider values */
typedef enum e_cgc_pll_div
{
    CGC_PLL_DIV_1       = 0x00,     ///< PLL divider of 1.
    CGC_PLL_DIV_2       = 0x01,     ///< PLL divider of 2.
    CGC_PLL_DIV_3       = 0x02,     ///< PLL divider of 3 (S7G2 only).
    CGC_PLL_DIV_4       = 0x03      ///< PLL divider of 4 (S3A7 only).
} cgc_pll_div_t;

/** System clock divider values - The individually selectable divider of each of the system clocks, ICLK, BCLK, FCLK,
 * PCLKS A-D  */
typedef enum e_cgc_sys_clock_div
{
    CGC_SYS_CLOCK_DIV_1     = 0x00,   ///< System clock divided by 1.
    CGC_SYS_CLOCK_DIV_2     = 0x01,   ///< System clock divided by 2.
    CGC_SYS_CLOCK_DIV_4     = 0x02,   ///< System clock divided by 4.
    CGC_SYS_CLOCK_DIV_8     = 0x03,   ///< System clock divided by 8.
    CGC_SYS_CLOCK_DIV_16    = 0x04,   ///< System clock divided by 16.
    CGC_SYS_CLOCK_DIV_32    = 0x05,   ///< System clock divided by 32.
    CGC_SYS_CLOCK_DIV_64    = 0x06,   ///< System clock divided by 64.
} cgc_sys_clock_div_t;

/** Clock configuration structure - Used as an input parameter to the cgc_api_t::clockStart function for the PLL clock. */
typedef struct st_cgc_clock_cfg
{
    cgc_clock_t     source_clock;   ///< PLL source clock (S7G2 only).
    cgc_pll_div_t   divider;        ///< PLL divider.
    /*LDRA_INSPECTED 90 s float used because float32_t is not part of the C99 standard integer definitions. */
    float           multiplier;     ///< PLL multiplier.
} cgc_clock_cfg_t;

/** System clock identifiers - Used as an input parameter to the cgc_api_t::systemClockFreqGet function. */
typedef enum e_cgc_system_clocks
{
    CGC_SYSTEM_CLOCKS_PCLKA,        ///< PCLKA - Peripheral module clock A.
    CGC_SYSTEM_CLOCKS_PCLKB,        ///< PCLKB - Peripheral module clock B.
    CGC_SYSTEM_CLOCKS_PCLKC,        ///< PCLKC - Peripheral module clock C.
    CGC_SYSTEM_CLOCKS_PCLKD,        ///< PCLKD - Peripheral module clock D.
    CGC_SYSTEM_CLOCKS_BCLK,         ///< BCLK  - External bus Clock.
    CGC_SYSTEM_CLOCKS_FCLK,         ///< FCLK  - FlashIF clock.
    CGC_SYSTEM_CLOCKS_ICLK          ///< ICLK  - System clock.
} cgc_system_clocks_t;

/** Divider values for the CLKOUT output. */
typedef enum e_cgc_clockout_dividers
{
    CGC_CLOCKOUT_DIV_1      = 0x00,     ///< Clockout source is divided by 1.
    CGC_CLOCKOUT_DIV_2      = 0x01,     ///< Clockout source is divided by 2.
    CGC_CLOCKOUT_DIV_4      = 0x02,     ///< Clockout source is divided by 4.
    CGC_CLOCKOUT_DIV_8      = 0x03,     ///< Clockout source is divided by 8.
    CGC_CLOCKOUT_DIV_16     = 0x04,     ///< Clockout source is divided by 16.
    CGC_CLOCKOUT_DIV_32     = 0x05,     ///< Clockout source is divided by 32.
    CGC_CLOCKOUT_DIV_64     = 0x06,     ///< Clockout source is divided by 64.
    CGC_CLOCKOUT_DIV_128    = 0x07,     ///< Clockout source is divided by 128.
} cgc_clockout_dividers_t;

/** Divider values for the external bus clock output. */
typedef enum e_cgc_bclockout_dividers
{
    CGC_BCLOCKOUT_DIV_1     = 0x00,     ///< External bus clock source is divided by 1.
    CGC_BCLOCKOUT_DIV_2     = 0x01,     ///< External bus clock source is divided by 2.
} cgc_bclockout_dividers_t;

/** Clock configuration structure - Used as an input parameter to the cgc_api_t::systemClockSet and cgc_api_t::systemClockGet
 * functions. */
typedef struct st_cgc_system_clock_cfg
{
    cgc_sys_clock_div_t     pclka_div;    ///< Divider value for PCLKA
    cgc_sys_clock_div_t     pclkb_div;    ///< Divider value for PCLKB
    cgc_sys_clock_div_t     pclkc_div;    ///< Divider value for PCLKC
    cgc_sys_clock_div_t     pclkd_div;    ///< Divider value for PCLKD
    cgc_sys_clock_div_t     bclk_div;     ///< Divider value for BCLK
    cgc_sys_clock_div_t     fclk_div;     ///< Divider value for FCLK
    cgc_sys_clock_div_t     iclk_div;     ///< Divider value for ICLK
} cgc_system_clock_cfg_t;

/** USB clock divider values */
typedef enum e_cgc_usb_clock_div
{
    CGC_USB_CLOCK_DIV_3     = 0x02,     ///< Divide USB source clock by 3
    CGC_USB_CLOCK_DIV_4     = 0x03,     ///< Divide USB source clock by 4
    CGC_USB_CLOCK_DIV_5     = 0x04,     ///< Divide USB source clock by 5
} cgc_usb_clock_div_t;


/** Available period units for R_CGC_SystickUpdate() */
typedef enum
{
    CGC_SYSTICK_PERIOD_UNITS_MILLISECONDS = 1000,           ///< Requested period in milliseconds
    CGC_SYSTICK_PERIOD_UNITS_MICROSECONDS = 1000000         ///< Requested period in microseconds
} cgc_systick_period_units_t;

/** Clock options */
typedef enum e_cgc_clock_change
{
    CGC_CLOCK_CHANGE_NONE,    ///< No change to the clock
    CGC_CLOCK_CHANGE_STOP,         ///< Stop the clock
    CGC_CLOCK_CHANGE_START,        ///< Start the clock
} cgc_clock_change_t;

/** Clock configuration */
typedef struct st_cgc_clocks_cfg
{
    cgc_clock_t               system_clock;   ///< System clock source enumeration
    cgc_clock_cfg_t           pll_cfg;        ///< PLL configuration structure
    cgc_system_clock_cfg_t    sys_cfg;        ///< Clock dividers structure
    cgc_clock_change_t        loco_state;     ///< State of LOCO
    cgc_clock_change_t        moco_state;     ///< State of MOCO
    cgc_clock_change_t        hoco_state;     ///< State of HOCO
    cgc_clock_change_t        subosc_state;   ///< State of Sub-oscillator
    cgc_clock_change_t        mainosc_state;  ///< State of Main oscillator
    cgc_clock_change_t        pll_state;      ///< State of PLL
} cgc_clocks_cfg_t;

/** CGC functions implemented at the HAL layer follow this API. */
typedef struct
{
    /** Initial configuration
     * @par Implemented as
     * - R_CGC_Init()
     *  @note The BSP module calls this function at startup. No further initialization is necessary.
     */
    ssp_err_t (* init)(void);

    /** Configure all system clocks.
     * @par Implemented as
     * - R_CGC_ClocksCfg()
     *  @note The BSP module calls this function at startup, but it can also be called from the application to change
     *  clocks at runtime.
     *  @param[in]   p_clock_cfg     Pointer to a structure that contains the dividers or multipliers to be used when
     *                              configuring the PLL.
     */
    ssp_err_t (* clocksCfg)(cgc_clocks_cfg_t const * const p_clock_cfg);

    /** Start a clock.
     * @par Implemented as
     * - R_CGC_ClockStart()
     * @pre Clock to be started must not be running prior to calling this function or an error will be returned.
     * @param[in]   clock_source    Clock source to initialize.
     * @param[in]   p_clock_cfg     Pointer to a structure that contains the dividers or multipliers to be used when
     *                              configuring the PLL.
     */
    ssp_err_t (* clockStart)(cgc_clock_t clock_source, cgc_clock_cfg_t * p_clock_cfg);

    /** Stop a clock.
     * @par Implemented as
     * - R_CGC_ClockStop()
     * @pre Clock to be stopped must not be stopped prior to calling this function or an error will be returned.
     * @param[in]  clock_source     The clock source to stop.
     */
    ssp_err_t (* clockStop)(cgc_clock_t clock_source);

    /** Set the system clock.
     * @par Implemented as
     * - R_CGC_SystemClockSet()
     * @pre The clock to be set as the system clock must be running prior to calling this function.
     * @param[in]  clock_source     Clock source to set as system clock
     * @param[in]  p_clock_cfg      Pointer to the clock dividers configuration passed by the caller.
     */
    ssp_err_t (* systemClockSet)(cgc_clock_t clock_source, cgc_system_clock_cfg_t const * const p_clock_cfg);

    /** Get the system clock information.
     * @par Implemented as
     * - R_CGC_SystemClockGet()
     * @param[in]   p_set_clock_cfg Pointer to clock configuration structure
     * @param[out]  clock_source    Returns the current system clock.
     * @param[out]  p_clock_cfg     Returns the current system clock dividers.
     */
    ssp_err_t (* systemClockGet)(cgc_clock_t * p_clock_source, cgc_system_clock_cfg_t * p_set_clock_cfg);

    /** Return the frequency of the selected clock.
     * @par Implemented as
     * - R_CGC_SystemClockFreqGet()
     * @param[in]   clock       Specifies the internal clock whose frequency is returned.
     * @param[out]  p_freq_hz   Returns the frequency in Hz referenced by this pointer.
     */
    ssp_err_t (* systemClockFreqGet)(cgc_system_clocks_t clock, uint32_t * p_freq_hz);

    /** Check the stability of the selected clock.
     * @par Implemented as
     * - R_CGC_ClockCheck()
     * @param[in]   clock_source    Which clock source to check for stability.
     */
    ssp_err_t (* clockCheck)(cgc_clock_t clock_source);

    /** Configure the Main Oscillator stop detection.
     * @par Implemented as
     * - R_CGC_OscStopDetect()
     * @param[in]  p_callback   Callback function that will be called by the NMI interrupt when an oscillation stop is
     *                          detected. If the second argument is "false", then this argument can be NULL.
     * @param[in]  enable       Enable/disable Oscillation Stop Detection.
     */
    ssp_err_t (* oscStopDetect)(void (* p_callback)(cgc_callback_args_t * p_args), bool enable);

    /** Clear the oscillator stop detection flag.
     * @par Implemented as
     * - R_CGC_OscStopStatusClear()
     */
    ssp_err_t (* oscStopStatusClear)(void);

    /** Configure the bus clock output secondary divider. The primary divider is set using the
     * bsp clock configuration and the cgc_api_t::systemClockSet function (S7G2 and S3A7 only).
     *
     * @par Implemented as
     * - R_CGC_BusClockOutCfg()
     * @param[in]   divider     The divider of 1 or 2 of the clock source.
     */
    ssp_err_t (* busClockOutCfg)(cgc_bclockout_dividers_t divider);

    /** Enable the bus clock output (S7G2 and S3A7 only).
     * @par Implemented as
     * - R_CGC_BusClockOutEnable()
     */
    ssp_err_t (* busClockOutEnable)(void);

    /** Disable the bus clock output (S7G2 and S3A7 only).
     * @par Implemented as
     * - R_CGC_BusClockOutDisable()
     */
    ssp_err_t (* busClockOutDisable)(void);

    /** Configure clockOut.
     * @par Implemented as
     * - R_CGC_ClockOutCfg()
     * @param[in]   clock       Clock source.
     * @param[in]   divider     Divider of between 1 and 128 of the clock source.
     */
    ssp_err_t (* clockOutCfg)(cgc_clock_t clock, cgc_clockout_dividers_t divider);

    /** Enable clock output on the CLKOUT pin. The source of the clock is controlled by cgc_api_t::clockOutCfg.
     * @par Implemented as
     * - R_CGC_ClockOutEnable()
     */
    ssp_err_t (* clockOutEnable)(void);

    /** Disable clock output on the CLKOUT pin. The source of the clock is controlled by cgc_api_t::clockOutCfg.
     * @par Implemented as
     * - R_CGC_ClockOutDisable()
     */
    ssp_err_t (* clockOutDisable)(void);

    /** Configure the segment LCD Clock (S3A7 and S124 only).
     * @par Implemented as
     * - R_CGC_LCDClockCfg()
     * @param[in]   clock   Segment LCD clock source.
     */
    ssp_err_t (* lcdClockCfg)(cgc_clock_t clock);

    /** Enable the LCD clock (S3A7 and S124 only).
     * @par Implemented as
     * - R_CGC_LCDClockEnable()
     */
    ssp_err_t (* lcdClockEnable)(void);

    /** Disables the LCD clock (S3A7 and S124 only).
     * @par Implemented as
     * - R_CGC_LCDClockDisable()
     */
    ssp_err_t (* lcdClockDisable)(void);

    /** Configure the 24-bit Sigma-Delta A/D Converter Clock (S1JA only).
     * @par Implemented as
     * - R_CGC_SDADCClockCfg()
     * @param[in]   clock   SDADC clock source.
     */
    ssp_err_t (* sdadcClockCfg)(cgc_clock_t clock);

    /** Enable the SDADC clock (S1JA only).
     * @par Implemented as
     * - R_CGC_SDADCClockEnable()
     */
    ssp_err_t (* sdadcClockEnable)(void);

    /** Disables the SDADC clock (S1JA only).
     * @par Implemented as
     * - R_CGC_SDADCClockDisable()
     */
    ssp_err_t (* sdadcClockDisable)(void);

    /** Enables the SDRAM clock output (S7G2 only).
     * @par Implemented as
     * - R_CGC_SDRAMClockOutEnable()
     */
    ssp_err_t (* sdramClockOutEnable)(void);

    /** Disables the SDRAM clock (S7G2 only).
     * @par Implemented as
     * - R_CGC_SDRAMClockOutDisable()
     *  */
    ssp_err_t (* sdramClockOutDisable)(void);

    /** Configures the USB clock (S7G2 only).
     * @par Implemented as
     * - R_CGC_USBClockCfg()
     * @param[in]   divider     The divider of 3, 4 or 5, of the clock source.
     */
    ssp_err_t (* usbClockCfg)(cgc_usb_clock_div_t divider);

    /** Update the Systick timer.
     * @par Implemented as
     * - R_CGC_SystickUpdate()
     * @param[in]   period_count       The duration for the systick period.
     * @param[in]   units              The units for the provided period.
     */
    ssp_err_t (* systickUpdate)(uint32_t period_count, cgc_systick_period_units_t units);

    /** Gets the CGC driver version.
     * @par Implemented as
     * - R_CGC_VersionGet()
     * @param[out]  p_version   Code and API version used.
     */
    ssp_err_t (* versionGet)(ssp_version_t * p_version);
} cgc_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_cgc_instance
{
    cgc_clock_cfg_t const * p_cfg;     ///< Pointer to the configuration structure for this instance
    cgc_api_t       const * p_api;     ///< Pointer to the API structure for this instance
} cgc_instance_t;

/*******************************************************************************************************************//**
 * @} (end defgroup CGC_API)
 **********************************************************************************************************************/

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif // ifndef DRV_CGC_API_H
