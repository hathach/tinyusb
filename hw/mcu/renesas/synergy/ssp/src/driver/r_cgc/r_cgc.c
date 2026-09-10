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
 * File Name    : r_cgc.c
 * Description  : HAL driver for the Clock Generation circuit.
 **********************************************************************************************************************/


/***********************************************************************************************************************
* Includes
 **********************************************************************************************************************/
#include "r_cgc.h"
#include "r_cgc_private.h"
#include "r_cgc_private_api.h"
#include "./hw/hw_cgc_private.h"
/* Configuration for this package. */
#include "r_cgc_cfg.h"
#include "./hw/hw_cgc.h"
#include "bsp_cfg.h"


#if (1 == BSP_CFG_RTOS)
#include "tx_api.h"
#endif


/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** Macro for error logger. */
#ifndef CGC_ERROR_RETURN
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define CGC_ERROR_RETURN(a, err) SSP_ERROR_RETURN((a), (err), g_module_name, &g_cgc_version)
#endif

#ifndef CGC_ERROR_LOG
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define CGC_ERROR_LOG(err) SSP_ERROR_LOG((err), g_module_name, &g_cgc_version)
#endif

#define CGC_LDC_INVALID_CLOCK   (0xFFU)

#define CGC_LCD_CFG_TIMEOUT     (0xFFFFFU)
#define CGC_SDADC_CFG_TIMEOUT   (0xFFFFFU)

/* From user's manual and discussions with hardware group, 
 * using the maximum is safe for all MCUs, will be updated and restored in LPMV2 when entering
 * low power mode on S7 and S5 MCUs (lowPowerModeEnter())
 * For S1 and S3 series, When clock source is HOCO and its frequency is 32MHz or lower,
 * the HSTS value "05" is used to account for low power wakeup time, and as the S5 and S7 series
 * doesn't support 24 MHz and 32 MHz frequency they are automatically excluded */
#if((BSP_HOCO_HZ == 24000000) || (BSP_HOCO_HZ == 32000000))
  #define MAXIMUM_HOCOWTR_HSTS    ((uint8_t)0x5U)
#else
  #define MAXIMUM_HOCOWTR_HSTS    ((uint8_t)0x6U)
#endif

#define CGC_PLL_DIV_1_SETTING 0
#define CGC_PLL_DIV_2_SETTING 1
#define CGC_PLL_DIV_4_SETTING 2

#if BSP_FEATURE_HAS_CGC_PLL
#define CGC_CLOCK_NUM_CLOCKS    ((uint8_t) CGC_CLOCK_PLL + 1U)
#else
#define CGC_CLOCK_NUM_CLOCKS    ((uint8_t) CGC_CLOCK_SUBCLOCK + 1U)
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static ssp_err_t r_cgc_clock_start_stop(cgc_clock_change_t clock_state, cgc_clock_t clock_to_change, cgc_clocks_cfg_t const *p_clk_cfg);
static ssp_err_t r_cgc_stabilization_wait(cgc_clock_t clock);

#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
static ssp_err_t r_cgc_check_peripheral_clocks(cgc_system_clocks_t clock);
static ssp_err_t r_cgc_check_dividers(cgc_system_clock_cfg_t const * const p_clock_cfg, uint32_t min_div);
static ssp_err_t r_cgc_check_config_dividers(cgc_system_clock_cfg_t const * const p_clock_cfg);
#if BSP_FEATURE_HAS_CGC_PLL
static bool r_cgc_clockcfg_valid_check (cgc_clock_cfg_t * cfg);
#endif
#endif

static bool r_cgc_subosc_mode_possible(void);
static bool r_cgc_low_speed_mode_possible(void);
static void r_cgc_operating_mode_set(cgc_clock_t const clock_source, uint32_t const current_system_clock);
static ssp_err_t r_cgc_wait_to_complete(cgc_clock_t const clock_source, cgc_clock_change_t option);
static ssp_err_t r_cgc_clock_running_status_check(cgc_clock_t const clock_source);
static void r_cgc_adjust_subosc_speed_mode(void);
static ssp_err_t r_cgc_apply_start_stop_options(cgc_clocks_cfg_t const * const p_clock_cfg, cgc_clock_change_t const * const options);
static void r_cgc_hw_init (void);
static void r_cgc_main_clock_drive_set (R_SYSTEM_Type * p_system_reg, uint8_t val);
static void r_cgc_subclock_drive_set (R_SYSTEM_Type * p_system_reg, uint8_t val);
static void r_cgc_clock_start (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock);
static void r_cgc_clock_stop (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock);
static bool r_cgc_clock_check (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock);
static void r_cgc_mainosc_source_set (R_SYSTEM_Type * p_system_reg, cgc_osc_source_t osc);
static void r_cgc_clock_wait_set (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock, uint8_t time);
static void r_cgc_clock_source_set (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock);
static void r_cgc_system_dividers_get (R_SYSTEM_Type * p_system_reg, cgc_system_clock_cfg_t * cfg);
static uint32_t r_cgc_clock_divider_get (R_SYSTEM_Type * p_system_reg, cgc_system_clocks_t clock);
static uint32_t r_cgc_clock_hzget (R_SYSTEM_Type * p_system_reg, cgc_system_clocks_t clock);
static uint32_t r_cgc_clockhz_calculate (cgc_clock_t source_clock,  cgc_sys_clock_div_t divider);
static void r_cgc_oscstop_detect_enable (R_SYSTEM_Type * p_system_reg);
static void r_cgc_oscstop_detect_disable (R_SYSTEM_Type * p_system_reg);
static bool r_cgc_oscstop_status_clear (R_SYSTEM_Type * p_system_reg);
static void r_cgc_clockout_cfg (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock, cgc_clockout_dividers_t divider);
static void r_cgc_clockout_enable (R_SYSTEM_Type * p_system_reg);
static void r_cgc_clockout_disable (R_SYSTEM_Type * p_system_reg);
static bool r_cgc_systick_update(uint32_t ticks_per_second);
static void r_cgc_system_dividers_set (R_SYSTEM_Type * p_system_reg, cgc_system_clock_cfg_t const * const cfg);

#if BSP_FEATURE_HAS_CGC_PLL
static ssp_err_t r_cgc_prepare_pll_clock(cgc_clock_cfg_t * p_clock_cfg);
static void r_cgc_init_pllfreq (R_SYSTEM_Type * p_system_reg);
static cgc_clock_t r_cgc_pll_clocksource_get (R_SYSTEM_Type * p_system_reg);
#if BSP_FEATURE_HAS_CGC_PLL_SRC_CFG
static void r_cgc_pll_clocksource_set (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock);
#endif
/*LDRA_INSPECTED 90 s float used because float32_t is not part of the C99 standard integer definitions. */
static void r_cgc_pll_multiplier_set (R_SYSTEM_Type * p_system_reg, float multiplier);
/*LDRA_INSPECTED 90 s float used because float32_t is not part of the C99 standard integer definitions. */
static float r_cgc_pll_multiplier_get (R_SYSTEM_Type * p_system_reg);
static void r_cgc_pll_divider_set (R_SYSTEM_Type * p_system_reg, cgc_pll_div_t divider);
static uint16_t r_cgc_pll_divider_get (R_SYSTEM_Type * p_system_reg);
#endif /* #if BSP_FEATURE_HAS_CGC_PLL */

#if (CGC_CFG_SUBCLOCK_AT_RESET_ENABLE == 1)
static void r_cgc_delay_cycles (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock, uint16_t cycles);
#endif

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
#if defined(__GNUC__)
/* This structure is affected by warnings from a GCC compiler bug. This pragma suppresses the warnings in this
 * structure only.*/
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
/** Version data structure used by error logger macro. */
static const ssp_version_t g_cgc_version =
{
    .api_version_minor  = CGC_API_VERSION_MINOR,
    .api_version_major  = CGC_API_VERSION_MAJOR,
    .code_version_major = CGC_CODE_VERSION_MAJOR,
    .code_version_minor = CGC_CODE_VERSION_MINOR
};
#if defined(__GNUC__)
/* Restore warning settings for 'missing-field-initializers' to as specified on command line. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic pop
#endif

/** Name of module used by error logger macro */
#if BSP_CFG_ERROR_LOG != 0
static const char g_module_name[] = "cgc";
#endif
/*LDRA_NOANALYSIS LDRA_INSPECTED below not working. */
/* This is initialized in cgc_api_t::init, which is called before the C runtime environment is initialized. */
/*LDRA_INSPECTED 219 S */
bsp_feature_cgc_t const * gp_cgc_feature BSP_PLACE_IN_SECTION_V2(".noinit");
/*LDRA_ANALYSIS */

/** Pointer to CGC base register. */
/* This variable is not initialized at declaration because it is initialized and used before C runtime initialization. */
/*LDRA_INSPECTED 57 D *//*LDRA_INSPECTED 57 D */
static R_SYSTEM_Type * gp_system_reg BSP_PLACE_IN_SECTION_V2(".noinit");

#if BSP_FEATURE_HAS_CGC_LCD_CLK
/** LCD clock selection register values. */
static const uint8_t g_lcd_clock_settings[] =
{
    [CGC_CLOCK_HOCO]      = 0x04U,     ///< The high speed on chip oscillator.
    [CGC_CLOCK_MOCO]      = CGC_LDC_INVALID_CLOCK,
    [CGC_CLOCK_LOCO]      = 0x00U,     ///< The low speed on chip oscillator.
    [CGC_CLOCK_MAIN_OSC]  = 0x02U,     ///< The main oscillator.
    [CGC_CLOCK_SUBCLOCK]  = 0x01U,     ///< The subclock oscillator.
#if BSP_FEATURE_HAS_CGC_PLL
    [CGC_CLOCK_PLL]       = CGC_LDC_INVALID_CLOCK,
#endif /* #if BSP_FEATURE_HAS_CGC_PLL */
};
#endif /* #if BSP_FEATURE_HAS_CGC_LCD_CLK */


/** This section of RAM should not be initialized by the C runtime environment */
/*LDRA_NOANALYSIS LDRA_INSPECTED below not working. */
/* This is initialized in cgc_api_t::init, which is called before the C runtime environment is initialized. */
/*LDRA_INSPECTED 219 S */
static uint32_t           g_clock_freq[CGC_CLOCK_NUM_CLOCKS]  BSP_PLACE_IN_SECTION_V2(".noinit");
/*LDRA_ANALYSIS */

#if BSP_FEATURE_HAS_CGC_PLL
/** These are the divisor values to use when calculating the system clock frequency, using the CGC_PLL_DIV enum type */
static const uint16_t g_pllccr_div_value[] =
{
    [CGC_PLL_DIV_1] = 0x01U,
    [CGC_PLL_DIV_2] = 0x02U,
    [CGC_PLL_DIV_3] = 0x03U
};
/** These are the values to use to set the PLL divider register according to the CGC_PLL_DIV enum type */
static const uint16_t g_pllccr_div_setting[] =
{
    [CGC_PLL_DIV_1] = 0x00U,
    [CGC_PLL_DIV_2] = 0x01U,
    [CGC_PLL_DIV_3] = 0x02U
};
/** These are the divisor values to use when calculating the system clock frequency, using the CGC_PLL_DIV enum type */
static const uint16_t g_pllccr2_div_value[] =
{
    [CGC_PLL_DIV_1_SETTING]         = 0x01U,
    [CGC_PLL_DIV_2_SETTING]         = 0x02U,
    [CGC_PLL_DIV_4_SETTING]         = 0x04U
};

/** These are the values to use to set the PLL divider register according to the CGC_PLL_DIV enum type */
static const uint16_t g_pllccr2_div_setting[] =
{
    [CGC_PLL_DIV_1] = 0x00U,
    [CGC_PLL_DIV_2] = 0x01U,
    [CGC_PLL_DIV_4] = 0x02U
};
#endif



/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
/*LDRA_INSPECTED 27 D This structure must be accessible in user code. It cannot be static. */
const cgc_api_t g_cgc_on_cgc =
{
    .init                 = R_CGC_Init,
    .clocksCfg            = R_CGC_ClocksCfg,
    .clockStart           = R_CGC_ClockStart,
    .clockStop            = R_CGC_ClockStop,
    .systemClockSet       = R_CGC_SystemClockSet,
    .systemClockGet       = R_CGC_SystemClockGet,
    .systemClockFreqGet   = R_CGC_SystemClockFreqGet,
    .clockCheck           = R_CGC_ClockCheck,
    .oscStopDetect        = R_CGC_OscStopDetect,
    .oscStopStatusClear   = R_CGC_OscStopStatusClear,
    .busClockOutCfg       = R_CGC_BusClockOutCfg,
    .busClockOutEnable    = R_CGC_BusClockOutEnable,
    .busClockOutDisable   = R_CGC_BusClockOutDisable,
    .clockOutCfg          = R_CGC_ClockOutCfg,
    .clockOutEnable       = R_CGC_ClockOutEnable,
    .clockOutDisable      = R_CGC_ClockOutDisable,
    .lcdClockCfg          = R_CGC_LCDClockCfg,
    .lcdClockEnable       = R_CGC_LCDClockEnable,
    .lcdClockDisable      = R_CGC_LCDClockDisable,
    .sdadcClockCfg        = R_CGC_SDADCClockCfg,
    .sdadcClockEnable     = R_CGC_SDADCClockEnable,
    .sdadcClockDisable    = R_CGC_SDADCClockDisable,
    .sdramClockOutEnable  = R_CGC_SDRAMClockOutEnable,
    .sdramClockOutDisable = R_CGC_SDRAMClockOutDisable,
    .usbClockCfg          = R_CGC_USBClockCfg,
    .systickUpdate        = R_CGC_SystickUpdate,
    .versionGet           = R_CGC_VersionGet
};

/*******************************************************************************************************************//**
 * @ingroup HAL_Library
 * @addtogroup CGC
 * @brief Clock Generation Circuit Hardware Functions
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief  Initialize the CGC API.
 *
 *                Configures the following for the clock generator module
 *                   -If CGC_CFG_SUBCLOCK_AT_RESET_ENABLE is set to true:
 *                      - SubClock drive capacity (Compile time configurable: CGC_CFG_SUBCLOCK_DRIVE)
 *                      - Initial setting for the SubClock
 *
 *                THIS FUNCTION MUST BE EXECUTED ONCE AT STARTUP BEFORE ANY OF THE OTHER CGC FUNCTIONS
 *                CAN BE USED OR THE CLOCK SOURCE IS CHANGED FROM THE MOCO.
 * @retval SSP_SUCCESS                  Clock initialized successfully.
 * @retval SSP_ERR_HARDWARE_TIMEOUT     Hardware timed out.
 **********************************************************************************************************************/
ssp_err_t R_CGC_Init (void)
{
    /* Initialize MCU specific feature pointer. */
    R_BSP_FeatureCgcGet(&gp_cgc_feature);

    ssp_feature_t ssp_feature= {{(ssp_ip_t) 0U}};
    fmi_feature_info_t info = {0U};
    ssp_feature.channel = 0U;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_CGC;
    g_fmi_on_fmi.productFeatureGet(&ssp_feature, &info);
    gp_system_reg = (R_SYSTEM_Type *) info.ptr;

    volatile uint32_t timeout;
    timeout = MAX_REGISTER_WAIT_COUNT;
    /* Update HOCOWTCR_b.HSTS */
    if(true == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_HOCO))
    {
        /* Make sure the HOCO is stable before changing wait control register */
        while ((false == r_cgc_clock_check(gp_system_reg, CGC_CLOCK_HOCO)) && (0U != timeout))
        {
            /* wait until the clock state is stable */
            timeout--;
        }
        CGC_ERROR_RETURN(timeout, SSP_ERR_HARDWARE_TIMEOUT);
    }

    /* Set HOCOWTCR_b.HSTS */
    r_cgc_hoco_wait_control_set(gp_system_reg, MAXIMUM_HOCOWTR_HSTS);

    timeout = MAX_REGISTER_WAIT_COUNT;
    r_cgc_hw_init(); // initialize hardware functions

    /** SubClock will stop only if configurable setting is Enabled */
#if (CGC_CFG_SUBCLOCK_AT_RESET_ENABLE == 1)
    r_cgc_clock_stop(gp_system_reg, CGC_CLOCK_SUBCLOCK);  // stop SubClock
    CGC_ERROR_RETURN((SSP_SUCCESS == r_cgc_wait_to_complete(CGC_CLOCK_SUBCLOCK, CGC_CLOCK_CHANGE_STOP)), SSP_ERR_HARDWARE_TIMEOUT);
    r_cgc_delay_cycles(gp_system_reg, CGC_CLOCK_SUBCLOCK, SUBCLOCK_DELAY); // Delay for 5 SubClock cycles.
    r_cgc_subclock_drive_set(gp_system_reg, CGC_CFG_SUBCLOCK_DRIVE);        // set the SubClock drive according to the configuration
#endif
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Reconfigure all main system clocks.
 *
 * @retval SSP_SUCCESS                  Clock initialized successfully.
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid argument used.
 * @retval SSP_ERR_MAIN_OSC_INACTIVE    PLL Initialization attempted with Main OCO turned off/unstable.
 * @retval SSP_ERR_CLOCK_ACTIVE         Active clock source specified for modification. This applies specifically to the
 *                                      PLL dividers/multipliers which cannot be modified if the PLL is active. It has
 *                                      to be stopped first before modification.
 * @retval SSP_ERR_NOT_STABILIZED       The Clock source is not stabilized after being turned off.
 * @retval SSP_ERR_CLKOUT_EXCEEDED      The main oscillator can be only 8 or 16 MHz.
 * @retval SSP_ERR_ASSERTION            A NULL is passed for configuration data when PLL is the clock_source.
 * @retval SSP_ERR_INVALID_MODE         Attempt to start a clock in a restricted operating power control mode.
 *
 **********************************************************************************************************************/
ssp_err_t R_CGC_ClocksCfg(cgc_clocks_cfg_t const * const p_clock_cfg)
{
#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
    SSP_ASSERT(NULL != p_clock_cfg);
#endif /* CGC_CFG_PARAM_CHECKING_ENABLE */
    ssp_err_t err = SSP_SUCCESS;
    cgc_clock_t requested_system_clock = p_clock_cfg->system_clock;
#if CGC_CFG_PARAM_CHECKING_ENABLE
#if BSP_FEATURE_HAS_CGC_PLL
    cgc_clock_cfg_t * p_pll_cfg = (cgc_clock_cfg_t *)&(p_clock_cfg->pll_cfg);
#endif /* BSP_FEATURE_HAS_CGC_PLL */
#endif
    cgc_system_clock_cfg_t sys_cfg = {
        .pclka_div = CGC_SYS_CLOCK_DIV_1,
        .pclkb_div = CGC_SYS_CLOCK_DIV_1,
        .pclkc_div = CGC_SYS_CLOCK_DIV_1,
        .pclkd_div = CGC_SYS_CLOCK_DIV_1,
        .bclk_div = CGC_SYS_CLOCK_DIV_1,
        .fclk_div = CGC_SYS_CLOCK_DIV_1,
        .iclk_div = CGC_SYS_CLOCK_DIV_1,
    };
    bool    power_mode_adjustment = false;
    cgc_clock_t current_system_clock = CGC_CLOCK_HOCO;
    R_CGC_SystemClockGet(&current_system_clock, &sys_cfg);

    cgc_clock_change_t options[CGC_CLOCK_NUM_CLOCKS];
    options[CGC_CLOCK_HOCO] = p_clock_cfg->hoco_state;
    options[CGC_CLOCK_LOCO] = p_clock_cfg->loco_state;
    options[CGC_CLOCK_MOCO] = p_clock_cfg->moco_state;
    options[CGC_CLOCK_MAIN_OSC] = p_clock_cfg->mainosc_state;
    options[CGC_CLOCK_SUBCLOCK] = p_clock_cfg->subosc_state;
#if BSP_FEATURE_HAS_CGC_PLL
    options[CGC_CLOCK_PLL] = p_clock_cfg->pll_state;
#endif /* BSP_FEATURE_HAS_CGC_PLL */

#if CGC_CFG_PARAM_CHECKING_ENABLE
    CGC_ERROR_RETURN(HW_CGC_ClockSourceValidCheck(requested_system_clock), SSP_ERR_INVALID_ARGUMENT);
    CGC_ERROR_RETURN(CGC_CLOCK_CHANGE_STOP != options[p_clock_cfg->system_clock], SSP_ERR_INVALID_ARGUMENT);
#if BSP_FEATURE_HAS_CGC_PLL
    if (CGC_CLOCK_CHANGE_START == options[CGC_CLOCK_PLL])
    {
        CGC_ERROR_RETURN(HW_CGC_ClockSourceValidCheck(p_pll_cfg->source_clock), SSP_ERR_INVALID_ARGUMENT);
        CGC_ERROR_RETURN(CGC_CLOCK_CHANGE_STOP != options[p_pll_cfg->source_clock], SSP_ERR_INVALID_ARGUMENT);
    }
#endif /* BSP_FEATURE_HAS_CGC_PLL */
#endif /* CGC_CFG_PARAM_CHECKING_ENABLE */

    err = r_cgc_apply_start_stop_options(p_clock_cfg, &options[0]);
    CGC_ERROR_RETURN(SSP_SUCCESS == err, err);

    err = r_cgc_stabilization_wait(requested_system_clock);
    CGC_ERROR_RETURN(SSP_SUCCESS == err, err);

    /* Check if clocks PLL, MOSC, HOCO and MOCO are stopped as a precondition to enter Subosc Speed Mode */
    /* If not, it will require to review the power mode after setting the clock, because the precondition can change */
    power_mode_adjustment = (CGC_CLOCK_SUBCLOCK == requested_system_clock) && (!r_cgc_subosc_mode_possible());

    /* Set which clock to use for system clock and divisors for all system clocks. */
    err = R_CGC_SystemClockSet(requested_system_clock, &(p_clock_cfg->sys_cfg));
    CGC_ERROR_RETURN(SSP_SUCCESS == err, err);

    /* If the system clock has changed, stop previous system clock if requested. */
    if (requested_system_clock != current_system_clock)
    {
        if (CGC_CLOCK_CHANGE_STOP == options[current_system_clock])
        {
            err = r_cgc_clock_start_stop(CGC_CLOCK_CHANGE_STOP, current_system_clock, p_clock_cfg);
            CGC_ERROR_RETURN(SSP_SUCCESS == err, err);
        }
    }
    if (power_mode_adjustment)
    {
        r_cgc_operating_mode_set(requested_system_clock, bsp_cpu_clock_get());
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Start the specified clock if it is not currently active.
 *
 *                Configures the following when starting the Main Clock Oscillator:
 *                - MainClock drive capacity (Configured based on external clock frequency)
 *                - MainClock stabilization wait time (Compile time configurable: CGC_CFG_MAIN_OSC_WAIT)
 *                - To update the subclock driven capacity, stop the subclock first before calling this function.
**
 * @retval SSP_SUCCESS                  Clock initialized successfully.
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid argument used.
 * @retval SSP_ERR_MAIN_OSC_INACTIVE    PLL Initialization attempted with Main OCO turned off/unstable.
 * @retval SSP_ERR_CLOCK_ACTIVE         Active clock source specified for modification. This applies specifically to the
 *                                      PLL dividers/multipliers which cannot be modified if the PLL is active. It has
 *                                      to be stopped first before modification.
 * @retval SSP_ERR_NOT_STABILIZED       The Clock source is not stabilized after being turned off.
 * @retval SSP_ERR_CLKOUT_EXCEEDED      The main oscillator can be only 8 or 16 MHz.
 * @retval SSP_ERR_ASSERTION            A NULL is passed for configuration data when PLL is the clock_source.
 * @retval SSP_ERR_INVALID_MODE         Attempt to start a clock in a restricted operating power control mode.
 * @retval SSP_ERR_HARDWARE_TIMEOUT		Hardware timed out.
 **********************************************************************************************************************/

ssp_err_t R_CGC_ClockStart (cgc_clock_t clock_source, cgc_clock_cfg_t * p_clock_cfg)
{


#if !BSP_FEATURE_HAS_CGC_PLL
    SSP_PARAMETER_NOT_USED(p_clock_cfg);
#endif

    /* return error if invalid clock source or not supported by hardware */
    CGC_ERROR_RETURN((HW_CGC_ClockSourceValidCheck(clock_source)), SSP_ERR_INVALID_ARGUMENT);

    if (true == r_cgc_clock_run_state_get(gp_system_reg, clock_source))
    {
#if BSP_FEATURE_HAS_CGC_PLL
        r_cgc_init_pllfreq(gp_system_reg);                                /* calculate  PLL clock frequency and save it */
#endif /* BSP_FEATURE_HAS_CGC_PLL */
        CGC_ERROR_LOG(SSP_ERR_CLOCK_ACTIVE);
        return SSP_ERR_CLOCK_ACTIVE;
    }

    /* some clocks (other than LOCO and MOCO require some additional work before starting them */
    if (CGC_CLOCK_SUBCLOCK == clock_source)
    {
        /* Set SubClockDrive only if clock is not running */
        r_cgc_subclock_drive_set(gp_system_reg, CGC_CFG_SUBCLOCK_DRIVE);      //Set the SubClock Drive
    }
    else if (CGC_CLOCK_MOCO == clock_source)
    {
        r_cgc_adjust_subosc_speed_mode();
    }
    else if (CGC_CLOCK_HOCO == clock_source)
    {
        /* make sure the oscillator has stopped before starting again */
        CGC_ERROR_RETURN(!(r_cgc_clock_check(gp_system_reg, CGC_CLOCK_HOCO)), SSP_ERR_NOT_STABILIZED);
        r_cgc_adjust_subosc_speed_mode();
    }
    else if (CGC_CLOCK_MAIN_OSC == clock_source)
    {
        /* make sure the oscillator has stopped before starting again */
        CGC_ERROR_RETURN(!(r_cgc_clock_check(gp_system_reg, CGC_CLOCK_MAIN_OSC)), SSP_ERR_NOT_STABILIZED);

        r_cgc_adjust_subosc_speed_mode();
        r_cgc_main_clock_drive_set(gp_system_reg, gp_cgc_feature->mainclock_drive);          /* set the Main Clock drive according to
                                                                     * the configuration */
        r_cgc_mainosc_source_set(gp_system_reg, (cgc_osc_source_t) CGC_CFG_MAIN_OSC_CLOCK_SOURCE); /* set the main osc source
                                                                                        * to resonator or
                                                                                        * external osc. */
        r_cgc_clock_wait_set(gp_system_reg, CGC_CLOCK_MAIN_OSC, CGC_CFG_MAIN_OSC_WAIT);            /* set the main osc wait time */
    }

#if BSP_FEATURE_HAS_CGC_PLL
    /*  if clock source is PLL */
    else if (CGC_CLOCK_PLL == clock_source)
    {
        ssp_err_t err;
        err = r_cgc_prepare_pll_clock(p_clock_cfg);
        CGC_ERROR_RETURN(SSP_SUCCESS == err, err);

#if BSP_FEATURE_HAS_CGC_MIDDLE_SPEED
        /** See if we need to switch to Middle or High Speed mode before starting the PLL. */
        cgc_system_clock_cfg_t  current_clock_cfg1;
        r_cgc_system_dividers_get(gp_system_reg, &current_clock_cfg1); // Get the current iclk divider value.
        uint32_t requested_frequency_hz = r_cgc_clockhz_calculate(CGC_CLOCK_PLL, current_clock_cfg1.iclk_div);
        if (requested_frequency_hz > gp_cgc_feature->middle_speed_max_freq_hz)
        {
            HW_CGC_SetHighSpeedMode(gp_system_reg);
        }
        else
        {
            HW_CGC_SetMiddleSpeedMode(gp_system_reg);    // PLL will only run in High or Middle Speed modes.
        }
#else
        HW_CGC_SetHighSpeedMode(gp_system_reg);
#endif /* BSP_FEATURE_HAS_CGC_MIDDLE_SPEED */

    }
#endif /* BSP_FEATURE_HAS_CGC_PLL */

    else
    {
        /* statement here to follow coding standard */
    }

    r_cgc_clock_start(gp_system_reg, clock_source);       // start the clock
    CGC_ERROR_RETURN((SSP_SUCCESS == r_cgc_wait_to_complete(clock_source, CGC_CLOCK_CHANGE_START)), SSP_ERR_HARDWARE_TIMEOUT);

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Stop the specified clock if it is active and not configured as the system clock.
 * @retval SSP_SUCCESS              		Clock stopped successfully.
 * @retval SSP_ERR_CLOCK_ACTIVE     		Current System clock source specified for stopping. This is not allowed.
 * @retval SSP_ERR_OSC_STOP_DET_ENABLED  	Illegal attempt to stop MOCO when Oscillation stop is enabled.
 * @retval SSP_ERR_NOT_STABILIZED   		Clock not stabilized after starting. A finite stabilization time after starting the
 *                                  		clock has to elapse before it can be stopped.
 * @retval SSP_ERR_INVALID_ARGUMENT 		Invalid argument used.
 * @retval SSP_ERR_HARDWARE_TIMEOUT			Hardware timed out.
 **********************************************************************************************************************/

ssp_err_t R_CGC_ClockStop (cgc_clock_t clock_source)
{
    cgc_clock_t current_clock;

    /*  return error if invalid clock source or not supported by hardware */
    CGC_ERROR_RETURN(HW_CGC_ClockSourceValidCheck(clock_source), SSP_ERR_INVALID_ARGUMENT);

    current_clock = HW_CGC_ClockSourceGet(gp_system_reg);     // The currently active system clock source cannot be stopped

    /* if clock source is the current system clock, return error */
    CGC_ERROR_RETURN((clock_source != current_clock), SSP_ERR_CLOCK_ACTIVE);

#if BSP_FEATURE_HAS_CGC_PLL
    /* If either PLL is the current system clock, or if PLL is not current system clock but it is operating, and the clock source of PLL is same as
     * requested clock_source to stop, return an error.
     */

    CGC_ERROR_RETURN(!(((CGC_CLOCK_PLL == current_clock) || (r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_PLL)))
            && (r_cgc_pll_clocksource_get(gp_system_reg) == clock_source)), SSP_ERR_CLOCK_ACTIVE);
#endif /* BSP_FEATURE_HAS_CGC_PLL */

    /* MOCO cannot be stopped if OSC Stop Detect is enabled */
    CGC_ERROR_RETURN(!((clock_source == CGC_CLOCK_MOCO) && ((HW_CGC_OscStopDetectEnabledGet(gp_system_reg)))), SSP_ERR_OSC_STOP_DET_ENABLED);

    if (!r_cgc_clock_run_state_get(gp_system_reg, clock_source))
    {
        return SSP_SUCCESS;    // if clock is already inactive, return success
    }

    /*  make sure the oscillator is stable before stopping */
    CGC_ERROR_RETURN(r_cgc_clock_check(gp_system_reg, clock_source), SSP_ERR_NOT_STABILIZED);

    r_cgc_clock_stop(gp_system_reg, clock_source);         // stop the clock
    /* return error if timed out */
    CGC_ERROR_RETURN((SSP_SUCCESS == r_cgc_wait_to_complete(clock_source, CGC_CLOCK_CHANGE_STOP)), SSP_ERR_HARDWARE_TIMEOUT);

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Set the specified clock as the system clock and configure the internal dividers for
 *              ICLK, PCLKA, PCLKB, PCLKC, PCLKD and FCLK.
 *
 *              THIS FUNCTION DOES NOT CHECK TO SEE IF THE OPERATING MODE SUPPORTS THE SPECIFIED CLOCK SOURCE
 *              AND DIVIDER VALUES. SETTING A CLOCK SOURCE AND DVIDER OUTSIDE THE RANGE SUPPORTED BY THE
 *              CURRENT OPERATING MODE WILL RESULT IN UNDEFINED OPERATION.
 *
 *              IF THE LOCO MOCO OR SUBCLOCK ARE CHOSEN AS THE SYSTEM CLOCK, THIS FUNCTION WILL SET THOSE AS THE
 *              SYSTEM CLOCK WITHOUT CHECKING FOR STABILIZATION. IT IS UP TO THE USER TO ENSURE THAT LOCO, MOCO
 *              OR SUBCLOCK ARE STABLE BEFORE USING THEM AS THE SYSTEM CLOCK.
 *
 *              Additionally this function sets the RAM and ROM wait states for the MCU.
 *              For the S7 MCU the ROMWT register controls ROM wait states.
 *              For the S3 MCU the MEMWAIT register controls ROM wait states.
 *
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_CLOCK_INACTIVE       The specified clock source is inactive.
 * @retval SSP_ERR_ASSERTION            The p_clock_cfg parameter is NULL.
 * @retval SSP_ERR_NOT_STABILIZED       The clock source has not stabilized
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid argument used. ICLK is not set as the fastest clock.
 * @retval SSP_ERR_INVALID_MODE         Peripheral divisions are not valid in sub-osc mode
 * @retval SSP_ERR_INVALID_MODE         Oscillator stop detect not allowed in sub-osc mode
 **********************************************************************************************************************/

ssp_err_t R_CGC_SystemClockSet (cgc_clock_t clock_source, cgc_system_clock_cfg_t const * const p_clock_cfg)
{
    cgc_clock_t current_clock;
    bsp_clock_set_callback_args_t args;
    ssp_err_t err;

    /* return error if invalid clock source or not supported by hardware */
    CGC_ERROR_RETURN((HW_CGC_ClockSourceValidCheck(clock_source)), SSP_ERR_INVALID_ARGUMENT);

#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
    SSP_ASSERT(NULL != p_clock_cfg);

    err = r_cgc_check_config_dividers(p_clock_cfg);
    CGC_ERROR_RETURN((SSP_SUCCESS == err), err);

    /* Check if clock source is sub-oscillator clock */
    if(CGC_CLOCK_SUBCLOCK == clock_source)
    {
        /* check if ICLK & FCLK divider is not 0 */
        /* Peripheral divisions are not valid in sub-osc mode */
        CGC_ERROR_RETURN(!(0U != p_clock_cfg->iclk_div), SSP_ERR_INVALID_MODE);
#if BSP_FEATURE_HAS_CGC_FLASH_CLOCK
        CGC_ERROR_RETURN(!(0U != p_clock_cfg->fclk_div), SSP_ERR_INVALID_MODE);
#endif /* BSP_FEATURE_HAS_CGC_FLASH_CLOCK */

        /* Oscillator stop detect not allowed in sub-osc mode */
        CGC_ERROR_RETURN(!(true == HW_CGC_OscStopDetectEnabledGet(gp_system_reg)), SSP_ERR_INVALID_MODE);
    }
#endif /* CGC_CFG_PARAM_CHECKING_ENABLE */

    /** In order to correctly set the ROM and RAM wait state registers we need to know the current (S3A7 only) and
     * requested iclk frequencies.
     */

    current_clock = HW_CGC_ClockSourceGet(gp_system_reg);
    if (clock_source != current_clock)    // if clock_source is not the current system clock, check stabilization
    {
        err = r_cgc_clock_running_status_check(clock_source);
        CGC_ERROR_RETURN(SSP_SUCCESS == err, err);
    }

    /* In order to avoid a system clock (momentarily) higher than expected, the order of switching the clock and
     * dividers must be so that the frequency of the clock goes lower, instead of higher, before being correct.
     */

    cgc_system_clock_cfg_t  current_clock_cfg;
    r_cgc_system_dividers_get(gp_system_reg, &current_clock_cfg);

   /* Switch to high-speed to prevent any issues with the subsequent system clock change. */
    HW_CGC_SetHighSpeedMode(gp_system_reg);

    /* Adjust the MCU specific wait state right before the system clock is set, if the system clock frequency to be set is higher than previous */
    args.event = BSP_CLOCK_SET_EVENT_PRE_CHANGE;
    args.new_rom_wait_state = 0U;
    args.requested_freq_hz = r_cgc_clockhz_calculate(clock_source, p_clock_cfg->iclk_div);
    args.current_freq_hz = r_cgc_clock_hzget(gp_system_reg, CGC_SYSTEM_CLOCKS_ICLK);
    err = bsp_clock_set_callback(&args);
    CGC_ERROR_RETURN(SSP_SUCCESS == err, err);

    /* If the current ICLK divider is less (higher frequency) than the requested ICLK divider,
     *  set the divider first.
     */
    if (current_clock_cfg.iclk_div < p_clock_cfg->iclk_div )
    {
        r_cgc_system_dividers_set(gp_system_reg, p_clock_cfg);
        r_cgc_clock_source_set(gp_system_reg, clock_source);
    }
    /* The current ICLK divider is greater (lower frequency) than the requested ICLK divider, so
     * set the clock source first.
     */
    else
    {
        r_cgc_clock_source_set(gp_system_reg, clock_source);
        r_cgc_system_dividers_set(gp_system_reg, p_clock_cfg);
    }

    /* Clock is now at requested frequency. */

    /* Adjust the MCU specific wait state soon after the system clock is set, if the system clock frequency to be set is lower than previous. */
    args.current_freq_hz = r_cgc_clock_hzget(gp_system_reg, CGC_SYSTEM_CLOCKS_ICLK);
    args.event = BSP_CLOCK_SET_EVENT_POST_CHANGE;
    err = bsp_clock_set_callback(&args);
    CGC_ERROR_RETURN(SSP_SUCCESS == err, err);

    /* Update the CMSIS core clock variable so that it reflects the new Iclk freq */
    SystemCoreClock = bsp_cpu_clock_get();

    /* Make sure SystemCoreClock frequency should not be zero */
    CGC_ERROR_RETURN((0U != SystemCoreClock), SSP_ERR_CLOCK_INACTIVE);

    /* Set the Operating speed mode based on the new system clock source */
    r_cgc_operating_mode_set(clock_source, SystemCoreClock);

#if (1 == BSP_CFG_RTOS)
    /* If an RTOS is in use, Update the Systick period based on the new frequency, using the ThreadX systick period in Microsecs */
    R_CGC_SystickUpdate((1000000 / TX_TIMER_TICKS_PER_SECOND), CGC_SYSTICK_PERIOD_UNITS_MICROSECONDS);
#endif

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Return the current system clock source and configuration.
 * @retval SSP_SUCCESS          Parameters returned successfully.
 * @retval SSP_ERR_ASSERTION    A NULL is passed for configuration data.
 * @retval SSP_ERR_ASSERTION    A NULL is passed for clock source.
 **********************************************************************************************************************/

ssp_err_t R_CGC_SystemClockGet (cgc_clock_t * clock_source, cgc_system_clock_cfg_t * p_set_clock_cfg)
{
#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
    SSP_ASSERT(NULL != clock_source);
    SSP_ASSERT(NULL != p_set_clock_cfg);
#endif /* CGC_CFG_PARAM_CHECKING_ENABLE */
    *clock_source = HW_CGC_ClockSourceGet(gp_system_reg);
    r_cgc_system_dividers_get(gp_system_reg, p_set_clock_cfg);

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Return the requested internal clock frequency in Hz.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid clock specified.
 * @retval SSP_ERR_ASSERTION            A NULL is passed for frequency data.
 **********************************************************************************************************************/
ssp_err_t R_CGC_SystemClockFreqGet (cgc_system_clocks_t clock, uint32_t * p_freq_hz)
{
#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
    SSP_ASSERT(NULL != p_freq_hz);

    /* Return error if invalid system clock or not supported by hardware */
    ssp_err_t err;
    if (CGC_SYSTEM_CLOCKS_ICLK == clock)
    {
        /* Valid clock for all MCUs. Do nothing. */
    }
    else if (CGC_SYSTEM_CLOCKS_FCLK == clock)
    {
#if !BSP_FEATURE_HAS_CGC_FLASH_CLOCK
        CGC_ERROR_LOG(SSP_ERR_INVALID_ARGUMENT);
        return SSP_ERR_INVALID_ARGUMENT;
#endif /* if !BSP_FEATURE_HAS_CGC_FLASH_CLOCK */
    }
    else if (CGC_SYSTEM_CLOCKS_BCLK == clock)
    {
#if !BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
        CGC_ERROR_LOG(SSP_ERR_INVALID_ARGUMENT);
        return SSP_ERR_INVALID_ARGUMENT;
#endif /* if BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */
    }
    else
    {
        err = r_cgc_check_peripheral_clocks(clock);
        CGC_ERROR_RETURN((SSP_SUCCESS == err), SSP_ERR_INVALID_ARGUMENT);
    }

#endif /* CGC_CFG_PARAM_CHECKING_ENABLE */

    *p_freq_hz = 0x00U;
    *p_freq_hz = r_cgc_clock_hzget(gp_system_reg, clock);
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Check the specified clock for stability.
 * @retval SSP_SUCCESS           		Operation performed successfully.
 * @retval SSP_ERR_NOT_STABILIZED       Clock not stabilized.
 * @retval SSP_ERR_CLOCK_ACTIVE         Clock active but not able to check for stability.
 * @retval SSP_ERR_CLOCK_INACTIVE       Clock not turned on.
 * @retval SSP_ERR_INVALID_ARGUMENT     Illegal parameter passed.
 * @retval SSP_ERR_STABILIZED			Clock stabilized.
 **********************************************************************************************************************/
ssp_err_t R_CGC_ClockCheck (cgc_clock_t clock_source)
{
    /* return error if invalid clock source or not supported by hardware */
    CGC_ERROR_RETURN((HW_CGC_ClockSourceValidCheck(clock_source)), SSP_ERR_INVALID_ARGUMENT);

    /*  There is no function to check for LOCO, MOCO or SUBCLOCK stability */
    if (((clock_source == CGC_CLOCK_LOCO) || (clock_source == CGC_CLOCK_MOCO)) || (clock_source == CGC_CLOCK_SUBCLOCK))
    {
        if (true == r_cgc_clock_run_state_get(gp_system_reg, clock_source))
        {
            return SSP_ERR_CLOCK_ACTIVE;      // There is no hardware to check for stability so just check for state.
        }
        else
        {
            return SSP_ERR_CLOCK_INACTIVE;    // There is no hardware to check for stability so just check for state.
        }
    }

    /*  There is a function to check for HOCO, MAIN_OSC and PLL stability */
#if BSP_FEATURE_HAS_CGC_PLL
    else if (((clock_source == CGC_CLOCK_HOCO) || (clock_source == CGC_CLOCK_MAIN_OSC))
             || (clock_source == CGC_CLOCK_PLL))
#else
    else if (((clock_source == CGC_CLOCK_HOCO) || (clock_source == CGC_CLOCK_MAIN_OSC)))
#endif /* BSP_FEATURE_HAS_CGC_PLL */
    {
        /* if clock is not active, can't check for stability, return error */
        CGC_ERROR_RETURN(r_cgc_clock_run_state_get(gp_system_reg, clock_source), SSP_ERR_CLOCK_INACTIVE);

        /*  check oscillator for stability, return error if not stable */
        CGC_ERROR_RETURN(r_cgc_clock_check(gp_system_reg, clock_source), SSP_ERR_NOT_STABILIZED);
        return SSP_ERR_STABILIZED;              // otherwise, return affirmative, not really an error
    }
    else
    {
        /* statement here to follow coding standard */
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Enable or disable the oscillation stop detection for the main clock.
 *  The MCU will automatically switch the system clock to MOCO when a stop is detected if Main Clock is
 *  the system clock. If the system clock is the PLL, then the clock source will not be changed and the
 *  PLL free running frequency will be the system clock frequency.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_OSC_STOP_DETECTED    The Oscillation stop detect status flag is set. Under this condition it is not
 *                                      possible to disable the Oscillation stop detection function.
 * @retval SSP_ERR_ASSERTION            Null pointer passed for callback function when the second argument is "true".
 * @retval SSP_ERR_ASSERTION            Cannot enable oscillator stop detect in sub-osc speed mode
 * @retval SSP_ERR_ASSERTION            Invalid peripheral clock divisions for oscillator stop detect
 * @retval SSP_ERR_INVALID_MODE         Invalid peripheral clock divider setting. Frequencies of peripherals should
 *                                      follow certain conditions.
 **********************************************************************************************************************/
ssp_err_t R_CGC_OscStopDetect (void (* p_callback) (cgc_callback_args_t * p_args), bool enable)
{
    if (true == enable)
    {
#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
        SSP_ASSERT(NULL != p_callback);
        cgc_operating_modes_t operating_mode = r_cgc_operating_mode_get(gp_system_reg);
        SSP_ASSERT(CGC_SUBOSC_SPEED_MODE != operating_mode);
        cgc_system_clock_cfg_t clock_cfg;
        r_cgc_system_dividers_get(gp_system_reg, &clock_cfg);
        if((0U != gp_cgc_feature->low_speed_max_freq_hz) && (CGC_LOW_SPEED_MODE == operating_mode))
        {
            CGC_ERROR_RETURN((SSP_SUCCESS == r_cgc_check_dividers(&clock_cfg, gp_cgc_feature->low_speed_pclk_div_min)), SSP_ERR_INVALID_MODE);
        }
        else if((0U != gp_cgc_feature->low_voltage_max_freq_hz) && (CGC_LOW_VOLTAGE_MODE == operating_mode))
        {
            CGC_ERROR_RETURN((SSP_SUCCESS == r_cgc_check_dividers(&clock_cfg, gp_cgc_feature->low_voltage_pclk_div_min)), SSP_ERR_INVALID_MODE);
        }
        else
        {
            /* Do nothing */
        }
#endif
        /** add callback function to BSP */
        bsp_grp_irq_cb_t * p_cgc_callback = (bsp_grp_irq_cb_t *) p_callback;
        R_BSP_GroupIrqWrite(BSP_GRP_IRQ_OSC_STOP_DETECT, (bsp_grp_irq_cb_t) p_cgc_callback);
        r_cgc_oscstop_detect_enable(gp_system_reg);          // enable hardware oscillator stop detect
    }
    else
    {
        /* if oscillator stop detected, return error */
        CGC_ERROR_RETURN(!(HW_CGC_OscStopDetectGet(gp_system_reg)), SSP_ERR_OSC_STOP_DETECTED);
        r_cgc_oscstop_detect_disable(gp_system_reg);          // disable hardware oscillator stop detect
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Clear the Oscillation Stop Detection Status register.
 *
 *                This register is not cleared automatically if the stopped clock is restarted.
 *                This function blocks for about 3 ICLK cycles until the status register is cleared.
 * @retval SSP_SUCCESS 						Operation performed successfully.
 * @retval SSP_ERR_OSC_STOP_CLOCK_ACTIVE    The Oscillation Detect Status flag cannot be cleared if the
 *                                          Main Osc or PLL is set as the system clock. Change the
 *                                          system clock before attempting to clear this bit.
 **********************************************************************************************************************/

ssp_err_t R_CGC_OscStopStatusClear (void)
{
    cgc_clock_t current_clock;

    if (HW_CGC_OscStopDetectGet(gp_system_reg))               // if oscillator stop detected
    {
        current_clock = HW_CGC_ClockSourceGet(gp_system_reg); // The currently active system clock source

#if BSP_FEATURE_HAS_CGC_PLL
        /* MCU has PLL. */
        if (gp_cgc_feature->pll_src_configurable)
        {
            cgc_clock_t alt_clock;
            alt_clock = r_cgc_pll_clocksource_get(gp_system_reg);
            /* cannot clear oscillator stop status if Main Osc is source of PLL */
            CGC_ERROR_RETURN(!((CGC_CLOCK_PLL == current_clock) && (CGC_CLOCK_MAIN_OSC == alt_clock)), SSP_ERR_OSC_STOP_CLOCK_ACTIVE);
        }
        else
        {
            /* cannot clear oscillator stop status if PLL is current clock */
            CGC_ERROR_RETURN(!(CGC_CLOCK_PLL == current_clock), SSP_ERR_OSC_STOP_CLOCK_ACTIVE);
        }
#endif /* BSP_FEATURE_HAS_CGC_PLL */

        /* cannot clear oscillator stop status if Main Osc is current clock */
        CGC_ERROR_RETURN(!(CGC_CLOCK_MAIN_OSC == current_clock), SSP_ERR_OSC_STOP_CLOCK_ACTIVE);
    }

    r_cgc_oscstop_status_clear(gp_system_reg);          // clear hardware oscillator stop detect status

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Configure the secondary dividers for BCLKOUT. The primary divider is set using the
 *           bsp clock configuration and the R_CGC_SystemClockSet function.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 **********************************************************************************************************************/

ssp_err_t R_CGC_BusClockOutCfg (cgc_bclockout_dividers_t divider)
{
    /* The application must set up the PFS register so the BCLK pin is an output */

#if !BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
    SSP_PARAMETER_NOT_USED(divider);
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    HW_CGC_BusClockOutCfg(gp_system_reg, divider);
    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */
}

/*******************************************************************************************************************//**
 * @brief  Enable the BCLKOUT output.
 * @retval SSP_SUCCESS  Operation performed successfully.
 **********************************************************************************************************************/

ssp_err_t R_CGC_BusClockOutEnable (void)
{
#if !BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    HW_CGC_BusClockOutEnable(gp_system_reg);
    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */
}

/*******************************************************************************************************************//**
 * @brief Disable the BCLKOUT output.
 * @retval SSP_SUCCESS  Operation performed successfully.
 **********************************************************************************************************************/

ssp_err_t R_CGC_BusClockOutDisable (void)
{
#if !BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    HW_CGC_BusClockOutDisable(gp_system_reg);
    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */
}

/*******************************************************************************************************************//**
 * @brief  Configure the dividers for CLKOUT.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_INVALID_ARGUMENT     return error if PLL is used as source for clock out
 * @retval SSP_ERR_CLOCK_INACTIVE       return error if sub clock is not started prior to using it for clock out
 **********************************************************************************************************************/
ssp_err_t R_CGC_ClockOutCfg (cgc_clock_t clock, cgc_clockout_dividers_t divider)
{
    /* The application must set up the PFS register so the CLKOUT pin is an output */
    CGC_ERROR_RETURN(CGC_CLOCK_PLL != clock, SSP_ERR_INVALID_ARGUMENT); // return error if PLL is selected as source
    /* Subclock needs to be started before using it for clock out return error if subclock is stopped */
    CGC_ERROR_RETURN(((CGC_CLOCK_SUBCLOCK != clock) || ((CGC_CLOCK_SUBCLOCK == clock)
                      &&(true == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_SUBCLOCK)))) , SSP_ERR_CLOCK_INACTIVE)
    r_cgc_clockout_cfg(gp_system_reg, clock, divider);
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Enable the CLKOUT output.
 * @retval SSP_SUCCESS  Operation performed successfully.
 **********************************************************************************************************************/

ssp_err_t R_CGC_ClockOutEnable (void)
{
    r_cgc_clockout_enable(gp_system_reg);
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Disable the CLKOUT output.
 * @retval SSP_SUCCESS  Operation performed successfully.
 **********************************************************************************************************************/

ssp_err_t R_CGC_ClockOutDisable (void)
{
    r_cgc_clockout_disable(gp_system_reg);
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Configure the source for the segment LCDCLK.
 * @retval SSP_SUCCESS                     Operation performed successfully.
 * @retval SSP_ERR_TIMEOUT                 Timed out.
 * @retval SSP_ERR_INVALID_ARGUMENT        lcd_clock settings are invalid
 * @retval SSP_ERR_UNSUPPORTED             lcd_clock configuration is not supported on this device
 **********************************************************************************************************************/

ssp_err_t R_CGC_LCDClockCfg (cgc_clock_t clock)
{
#if !BSP_FEATURE_HAS_CGC_LCD_CLK
    SSP_PARAMETER_NOT_USED(clock);
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    /* The application must set up the PFS register so the LCDCLKOUT pin is an output */
    CGC_ERROR_RETURN(CGC_LDC_INVALID_CLOCK != g_lcd_clock_settings[clock], SSP_ERR_INVALID_ARGUMENT);

    /* Protect OFF for CGC. */
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_CGC);

    bool lcd_clock_was_enabled = HW_CGC_LCDClockIsEnabled(gp_system_reg);
    R_CGC_LCDClockDisable();
    HW_CGC_LCDClockCfg(gp_system_reg, g_lcd_clock_settings[clock]);
    uint32_t timeout = CGC_LCD_CFG_TIMEOUT;
    while ((g_lcd_clock_settings[clock] != HW_CGC_LCDClockCfgGet(gp_system_reg)) && (0U != timeout))  /* wait for the bit to set */
    {
        timeout--;
    }

    if (lcd_clock_was_enabled)
    {
        R_CGC_LCDClockEnable();
    }

    /* Protect ON for CGC. */
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_CGC);

    CGC_ERROR_RETURN(timeout > 0U, SSP_ERR_TIMEOUT);
    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_LCD_CLK */
}

/*******************************************************************************************************************//**
 * @brief  Enable the segment LCDCLK output.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_TIMEOUT              Timed out.
 * @retval SSP_ERR_UNSUPPORTED          lcd_clock is not supported on this device
 **********************************************************************************************************************/

ssp_err_t R_CGC_LCDClockEnable (void)
{
#if !BSP_FEATURE_HAS_CGC_LCD_CLK
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    /* Protect OFF for CGC. */
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_CGC);

    HW_CGC_LCDClockEnable(gp_system_reg);
    uint32_t timeout = CGC_LCD_CFG_TIMEOUT;
    while ((true != HW_CGC_LCDClockIsEnabled(gp_system_reg)) && (0U != timeout))  /* wait for the bit to set */
    {
        timeout--;
    }

    /* Protect ON for CGC. */
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_CGC);

    CGC_ERROR_RETURN(timeout > 0U, SSP_ERR_TIMEOUT);
    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_LCD_CLK */
}

/*******************************************************************************************************************//**
 * @brief  Disable the segment LCDCLK output.
 * @retval SSP_SUCCESS                 Operation performed successfully.
 * @retval SSP_ERR_TIMEOUT             Timed out.
 * @retval SSP_ERR_UNSUPPORTED         lcd_clock is not supported on this device
 **********************************************************************************************************************/

ssp_err_t R_CGC_LCDClockDisable (void)
{
#if !BSP_FEATURE_HAS_CGC_LCD_CLK
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    /* Protect OFF for CGC. */
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_CGC);

    HW_CGC_LCDClockDisable(gp_system_reg);
    uint32_t timeout = CGC_LCD_CFG_TIMEOUT;
    while ((false != HW_CGC_LCDClockIsEnabled(gp_system_reg)) && (0U != timeout))  /* wait for the bit to set */
    {
        timeout--;
    }

    /* Protect ON for CGC. */
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_CGC);

    CGC_ERROR_RETURN(timeout > 0U, SSP_ERR_TIMEOUT);
    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_LCD_CLK */
}



/*******************************************************************************************************************//**
 * @brief  Configure the source for the SDADCCLK.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_UNSUPPORTED          sdadc_clock configuration is not supported on this device
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid clock used
  **********************************************************************************************************************/

ssp_err_t R_CGC_SDADCClockCfg (cgc_clock_t clock)
{
#if !BSP_FEATURE_HAS_CGC_SDADC_CLK
    SSP_PARAMETER_NOT_USED(clock);
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    /* check for valid clock source */
    CGC_ERROR_RETURN(!((CGC_CLOCK_MAIN_OSC != clock) && (CGC_CLOCK_HOCO != clock)), SSP_ERR_INVALID_ARGUMENT);

    /* Protect OFF for CGC. */
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_CGC);

    bool sdadc_clock_was_enabled = HW_CGC_SDADCClockIsEnabled(gp_system_reg);
    R_CGC_SDADCClockDisable();

    HW_CGC_SDADCClockCfg(gp_system_reg, clock);

    if (sdadc_clock_was_enabled)
    {
        R_CGC_SDADCClockEnable();
    }

    /* Protect ON for CGC. */
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_CGC);

    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_SDADC_CLK */
}

/*******************************************************************************************************************//**
 * @brief  Enable the SDADCCLK output.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_UNSUPPORTED          sdadc_clock is not supported on this device
 **********************************************************************************************************************/

ssp_err_t R_CGC_SDADCClockEnable (void)
{
#if !BSP_FEATURE_HAS_CGC_SDADC_CLK
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    /* Protect OFF for CGC. */
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_CGC);

    HW_CGC_SDADCClockEnable(gp_system_reg);

    /* Protect ON for CGC. */
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_CGC);
    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_SDADC_CLK */
}

/*******************************************************************************************************************//**
 * @brief  Disable the SDADCCLK output.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_UNSUPPORTED          sdadc_clock is not supported on this device
 **********************************************************************************************************************/

ssp_err_t R_CGC_SDADCClockDisable (void)
{
#if !BSP_FEATURE_HAS_CGC_SDADC_CLK
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    /* Protect OFF for CGC. */
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_CGC);
    HW_CGC_SDADCClockDisable(gp_system_reg);
    /* Protect ON for CGC. */
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_CGC);
    return SSP_SUCCESS;
#endif /* !BSP_FEATURE_HAS_CGC_SDADC_CLK */
}


/*******************************************************************************************************************//**
 * @brief  Enable the SDCLK output.
 * @retval SSP_SUCCESS                    Operation performed successfully.
 * @retval SSP_ERR_UNSUPPORTED            sdram_clock is not supported on this device
 **********************************************************************************************************************/

ssp_err_t R_CGC_SDRAMClockOutEnable (void)
{
#if !BSP_FEATURE_HAS_CGC_SDRAM_CLK
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    HW_CGC_SDRAMClockOutEnable(gp_system_reg);
    return SSP_SUCCESS;
#endif
}

/*******************************************************************************************************************//**
 * @brief  Disable the SDCLK output.
 * @retval SSP_SUCCESS                    Operation performed successfully.
 * @retval SSP_ERR_UNSUPPORTED            sdram_clock is not supported on this device
 **********************************************************************************************************************/

ssp_err_t R_CGC_SDRAMClockOutDisable (void)
{
#if !BSP_FEATURE_HAS_CGC_SDRAM_CLK
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    HW_CGC_SDRAMClockOutDisable(gp_system_reg);
    return SSP_SUCCESS;
#endif
}

/*******************************************************************************************************************//**
 * @brief  Configure the dividers for UCLK.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid usb_clock divider specified
 ***********************************************************************************************************************/

ssp_err_t R_CGC_USBClockCfg (cgc_usb_clock_div_t divider)
{
#if !BSP_FEATURE_HAS_CGC_USB_CLK
    SSP_PARAMETER_NOT_USED(divider);
    CGC_ERROR_LOG(SSP_ERR_UNSUPPORTED);
    return SSP_ERR_UNSUPPORTED;
#else
    /* The application must set up the PFS register so the USBCLKOUT pin is an output */
    HW_CGC_USBClockCfg(gp_system_reg, divider);
    return SSP_SUCCESS;
#endif
}

/*******************************************************************************************************************//**
 * @brief  Re-Configure the systick based on the provided period and current system clock frequency.
 * @param[in]   period_count       The duration for the systick period.
 * @param[in]   units              The units for the provided period.
 * @retval SSP_SUCCESS                  Operation performed successfully.
 * @retval SSP_ERR_INVALID_ARGUMENT     Invalid period specified.
 * @retval SSP_ERR_ABORTED              Attempt to update systick timer failed.
 **********************************************************************************************************************/
ssp_err_t R_CGC_SystickUpdate(uint32_t period_count, cgc_systick_period_units_t units)
{
    uint32_t requested_period_count = period_count;
    uint32_t reload_value;
    uint32_t freq;
    cgc_systick_period_units_t period_units = units;
#if (__FPU_PRESENT != 0)
    /*LDRA_INSPECTED 90 s float used because float32_t is not part of the C99 standard integer definitions. */
    float period = 0.0f;
#endif

#if (CGC_CFG_PARAM_CHECKING_ENABLE)
    if (0 == period_count)
    {
        SSP_ERROR_LOG((SSP_ERR_INVALID_ARGUMENT), (g_module_name), (g_cgc_version));   // Invalid period provided
        return (SSP_ERR_INVALID_ARGUMENT);
    }
#endif

    freq = bsp_cpu_clock_get();		                // Get the current ICLK frequency

    /* If an RTOS is in use then we want to set the Systick period to that defined by the RTOS. So we'll convert the macro
     * settings use the existing code and calculate the reload value
     */
#if (1 == BSP_CFG_RTOS)
    period_units = CGC_SYSTICK_PERIOD_UNITS_MICROSECONDS;
    requested_period_count = (RELOAD_COUNT_FOR_1US) / TX_TIMER_TICKS_PER_SECOND;        // Convert ticks per sec to ticks per us
#endif

#if (__FPU_PRESENT == 0)
    reload_value = (uint32_t)(((uint64_t)(requested_period_count) * (uint64_t)(freq) + (uint64_t)(period_units/2)) / period_units);
#else
    /*LDRA_INSPECTED 90 s *//*LDRA_INSPECTED 90 s */
    period = ((1.0f)/(float)freq) * (float)period_units;           // This is the period in the provided units
    /*LDRA_INSPECTED 90 s */
    reload_value = (uint32_t)((float)requested_period_count/period);
#endif

    // Configure the systick period as requested
    CGC_ERROR_RETURN(r_cgc_systick_update(reload_value), SSP_ERR_ABORTED);
    return SSP_SUCCESS;
}


/*******************************************************************************************************************//**
 * @brief  Return the driver version.
 * @retval SSP_SUCCESS      	  Operation performed successfully.
 * @retval SSP_ERR_ASSERTION      The parameter p_version is NULL..
 **********************************************************************************************************************/

ssp_err_t R_CGC_VersionGet (ssp_version_t * const p_version)
{
#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
    SSP_ASSERT(NULL != p_version);
#endif /* CGC_CFG_PARAM_CHECKING_ENABLE */
    p_version->version_id = g_cgc_version.version_id;
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Return the Stabilization Status.
 * @param[in]  clock                    clock to be checked
 * @retval     SSP_SUCCESS              Operation performed successfully.
 * @retval     SSP_ERR_STABILIZED       Clock stabilized
 * @retval     SSP_ERR_NOT_STABILIZED   CLock not stabilized
 * @retval     SPP_ERR_CLOCK_ACTIVE     Specified clock source is already stabilized
 **********************************************************************************************************************/

static ssp_err_t r_cgc_stabilization_wait(cgc_clock_t clock)
{
    ssp_err_t err = SSP_ERR_NOT_STABILIZED;

    int32_t timeout = MAX_REGISTER_WAIT_COUNT;
    while (SSP_ERR_NOT_STABILIZED == err)
    {
        /* Wait for clock source to stabilize */
        timeout--;
        CGC_ERROR_RETURN(0 < timeout, SSP_ERR_NOT_STABILIZED);
        err = R_CGC_ClockCheck(clock);
    }

    CGC_ERROR_RETURN((SSP_SUCCESS == err) || (SSP_ERR_STABILIZED == err) ||
        (SSP_ERR_CLOCK_ACTIVE == err), err);

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Functionality of this function is to change the clock state and modifies the clock source
 *        as per input parameters
 * @param[in]   clock_state                     required clock state
 * @param[in]   clock_to_change                 required clock source
 * @param[in]   p_clk_cfg                       Pointer to the clock configuration structure
 * @retval      SSP_SUCCESS                     Operation performed successfully.
 * @retval      SPP_ERR_CLOCK_ACTIVE            Specified clock is already running
 * @retval      SSP_ERR_OSC_STOP_DET_ENABLED  	Illegal attempt to stop MOCO when Oscillation stop is enabled.
 * @retval      SSP_ERR_NOT_STABILIZED   		Clock not stabilized after starting. A finite stabilization time after starting the
 *                                  		    clock has to elapse before it can be stopped.
 * @retval      SSP_ERR_INVALID_ARGUMENT 		Invalid argument used.
 * @retval      SSP_ERR_HARDWARE_TIMEOUT		Hardware timed out.
 **********************************************************************************************************************/
static ssp_err_t r_cgc_clock_start_stop(cgc_clock_change_t clock_state, cgc_clock_t clock_to_change, cgc_clocks_cfg_t const *p_clk_cfg)
{
    cgc_clock_cfg_t * p_pll_cfg = (cgc_clock_cfg_t *)&(p_clk_cfg->pll_cfg);
    ssp_err_t err = SSP_SUCCESS;

    if(CGC_CLOCK_CHANGE_STOP == clock_state)
    {
        err = R_CGC_ClockStop(clock_to_change);
        CGC_ERROR_RETURN(SSP_SUCCESS == err, err);
    }
    else if(CGC_CLOCK_CHANGE_START == clock_state)
    {
#if BSP_FEATURE_HAS_CGC_PLL
        if (CGC_CLOCK_PLL == clock_to_change)
        {
            /* Need to start PLL source clock and let it stabilize before starting PLL */
            err = R_CGC_ClockStart(p_pll_cfg->source_clock, (cgc_clock_cfg_t *)p_clk_cfg);
            CGC_ERROR_RETURN((SSP_SUCCESS == err) || (SSP_ERR_CLOCK_ACTIVE == err), err);

            err = r_cgc_stabilization_wait(p_pll_cfg->source_clock);
            CGC_ERROR_RETURN(SSP_SUCCESS == err, err);
        }
#endif /* BSP_FEATURE_HAS_CGC_PLL */
        err = R_CGC_ClockStart(clock_to_change, p_pll_cfg);
        CGC_ERROR_RETURN((SSP_SUCCESS == err) || (SSP_ERR_CLOCK_ACTIVE == err), err);
    }
    else /* CGC_CLOCK_OPTION_NO_CHANGE */
    {
        /* Do nothing */
    }

    return SSP_SUCCESS;
}

#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
/*******************************************************************************************************************//**
 * @brief  Check if the given peripheral clock is valid for the current MCU.
 * @param[in] clock                     current system clock
 * @retval SSP_SUCCESS                  Given peripheral clock is valid for the current MCU.
 * @retval SSP_ERR_INVALID_ARGUMENT     Given peripheral clock is invalid for the current MCU.
 **********************************************************************************************************************/
static ssp_err_t r_cgc_check_peripheral_clocks (cgc_system_clocks_t clock)
{
    if (CGC_SYSTEM_CLOCKS_PCLKA == clock)
    {
#if BSP_FEATURE_HAS_CGC_PCKA
        return SSP_SUCCESS;
#else
        CGC_ERROR_LOG(SSP_ERR_INVALID_ARGUMENT);
        return SSP_ERR_INVALID_ARGUMENT;
#endif /* BSP_FEATURE_HAS_CGC_PCKA */
    }
    else if (CGC_SYSTEM_CLOCKS_PCLKB == clock)
    {
#if BSP_FEATURE_HAS_CGC_PCKB
        return SSP_SUCCESS;
#else
        CGC_ERROR_LOG(SSP_ERR_INVALID_ARGUMENT);
        return SSP_ERR_INVALID_ARGUMENT;
#endif /* BSP_FEATURE_HAS_CGC_PCKB */
    }
    else if (CGC_SYSTEM_CLOCKS_PCLKC == clock)
    {
#if BSP_FEATURE_HAS_CGC_PCKC
        return SSP_SUCCESS;
#else
        CGC_ERROR_LOG(SSP_ERR_INVALID_ARGUMENT);
        return SSP_ERR_INVALID_ARGUMENT;
#endif /* BSP_FEATURE_HAS_CGC_PCKC */
    }
    else if (CGC_SYSTEM_CLOCKS_PCLKD == clock)
    {
#if BSP_FEATURE_HAS_CGC_PCKD
        return SSP_SUCCESS;
#else
        CGC_ERROR_LOG(SSP_ERR_INVALID_ARGUMENT);
        return SSP_ERR_INVALID_ARGUMENT;
#endif /* BSP_FEATURE_HAS_CGC_PCKD */
    }
    else
    {
        CGC_ERROR_LOG(SSP_ERR_INVALID_ARGUMENT);
        return SSP_ERR_INVALID_ARGUMENT;
    }
}
#endif

/*******************************************************************************************************************//**
 * @brief  Verifies if Sub-osc Mode is possible
 * @retval  true    Sub-osc mode is possible or feature does not exist (hence no reason to try it later)
 * @retval  false   Sub-osc mode feature is available but could not be used at this time (not meeting conditions)
 **********************************************************************************************************************/
static bool r_cgc_subosc_mode_possible(void)
{
#if BSP_FEATURE_HAS_CGC_PLL
    if ((false == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_PLL)) && (false == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_MAIN_OSC)) &&
        (false == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_HOCO)) && (false == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_MOCO)))
    {
        return true;
    }
#else
    if ((false == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_MAIN_OSC)) &&
        (false == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_HOCO)) && (false == r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_MOCO)))
    {
        return true;
    }
#endif
    return false;
}

/*******************************************************************************************************************//**
 * @brief  Verifies if Low-speed Mode is possible
 * @retval  true    Low-speed mode is possible
 * @retval  false   Low-speed mode is not possible (not meeting conditions)
 **********************************************************************************************************************/
static bool r_cgc_low_speed_mode_possible(void)
{
#if BSP_FEATURE_HAS_CGC_PLL
    return !r_cgc_clock_run_state_get(gp_system_reg, CGC_CLOCK_PLL);
#else /* BSP_FEATURE_HAS_CGC_PLL */
    return true;
#endif /* BSP_FEATURE_HAS_CGC_PLL */
}


/*******************************************************************************************************************//**
 * @brief  Set the optimum operating speed mode based on new system clock
 * @param[in] clock_source    		Clock that will have the operating mode prepared for
 * @param[in] current_system_clock	Current clock, as before changing the speed mode
  **********************************************************************************************************************/
static void r_cgc_operating_mode_set(cgc_clock_t const clock_source, uint32_t const current_system_clock)
{
#if BSP_FEATURE_HAS_CGC_PLL && BSP_FEATURE_HAS_CGC_MIDDLE_SPEED
    /* Checks if clock source is PLL */
    if (CGC_CLOCK_PLL == clock_source)
    {
        /* PLL will only run in High or Middle Speed modes. */
        if (current_system_clock <= gp_cgc_feature->middle_speed_max_freq_hz)
        {
            /* Switch to middle speed mode */
            HW_CGC_SetMiddleSpeedMode(gp_system_reg);
        }
        else
        {
            /* Nothing to do, stay in high speed mode */
        }
        return;
    }
#endif /* BSP_FEATURE_HAS_CGC_PLL */
    /* For all other remaining clock sources i.e., HOCO, MOCO, LOCO, SUBCLOCK, MOSC */
#if CGC_CFG_USE_LOW_VOLTAGE_MODE
    /* System clock should be less than or equal to Low voltage max frequency */
    if (current_system_clock <= (gp_cgc_feature->low_voltage_max_freq_hz))
    {
        /* Switch to low voltage mode */
        HW_CGC_SetLowVoltageMode(gp_system_reg);

        /* Low voltage mode is conditionally compiled. If the Low voltage mode is executed,
         * no other operating power mode needs to be checked for execution, hence the function returns from here.
         * If the low voltage mode feature was not conditionally compiled, the next statement would be an 'else if()'
         * instead of an 'if()' would be added to choose the optimum power control mode */
        return;
    }
#endif

    /* Checks if MCU supports sub oscillator speed mode and also clock source should be subclock or LOCO */
#if BSP_FEATURE_HAS_CGC_SUBOSC_SPEED
    if ((CGC_CLOCK_SUBCLOCK == clock_source) || (CGC_CLOCK_LOCO == clock_source))
    {
        /* Verify that clocks PLL, MOSC, HOCO and MOCO are stopped as a precondition to enter Subosc Speed Mode */
        if (true == r_cgc_subosc_mode_possible())
        {
            /* Switch to sub oscillator mode */
            HW_CGC_SetSubOscSpeedMode(gp_system_reg);
            return;
        }
        /* If sub-osc mode was not available will try the lowest available */
    }
#endif

    /* System clock should be less than or equal to Low speed max frequency */
    if (current_system_clock <= gp_cgc_feature->low_speed_max_freq_hz)
    {
        if(true == r_cgc_low_speed_mode_possible())
        {
            /* Switch to low speed mode */
            HW_CGC_SetLowSpeedMode(gp_system_reg);
            return;
        }
    }

#if BSP_FEATURE_HAS_CGC_MIDDLE_SPEED
    /* System clock should be less than or equal to Middle speed max frequency */
    if (current_system_clock <= gp_cgc_feature->middle_speed_max_freq_hz)
    {
        /* Switch to middle speed mode */
        HW_CGC_SetMiddleSpeedMode(gp_system_reg);
    }
    else
    {
        /* Nothing to do, stay in high speed mode */
    }
#endif /* BSP_FEATURE_HAS_CGC_MIDDLE_SPEED */
}

#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
/*******************************************************************************************************************//**
 * @brief  Check dividers values
 * @param[in] p_clock_cfg       pointer to the clock configuration structure
 * @param[in] min_div           minimum low speed clock divider value
 * @retval      SSP_SUCCESS     Operation performed successfully
 * @retval SSP_ERR_INVALID_MODE clock divider is divider is greater than minimum low speed clock divider and is invalid
  **********************************************************************************************************************/
static ssp_err_t r_cgc_check_dividers(cgc_system_clock_cfg_t const * const p_clock_cfg, uint32_t min_div)
{
#if BSP_FEATURE_HAS_CGC_PCKA
    /* check if the MCU has PCLKA and PCLKA divider is greater than minimum low speed clock divider*/
    CGC_ERROR_RETURN(!(p_clock_cfg->pclka_div < min_div), SSP_ERR_INVALID_MODE);
#endif /* BSP_FEATURE_HAS_CGC_PCKA */

#if BSP_FEATURE_HAS_CGC_PCKB
    /* check if the MCU has PCLKB and PCLKB divider is greater than minimum low speed clock divider*/
    CGC_ERROR_RETURN(!(p_clock_cfg->pclkb_div < min_div), SSP_ERR_INVALID_MODE);
#endif /* BSP_FEATURE_HAS_CGC_PCKB */

#if BSP_FEATURE_HAS_CGC_PCKC
    /* check if the MCU has PCLKC and PCLKC divider is greater than minimum low speed clock divider*/
    CGC_ERROR_RETURN(!(p_clock_cfg->pclkc_div < min_div), SSP_ERR_INVALID_MODE);
#endif /* BSP_FEATURE_HAS_CGC_PCKC */

#if BSP_FEATURE_HAS_CGC_PCKD
    /* check if the MCU has PCLKD and PCLKD divider is greater than minimum low speed clock divider*/
    CGC_ERROR_RETURN(!(p_clock_cfg->pclkd_div < min_div), SSP_ERR_INVALID_MODE);
#endif /* BSP_FEATURE_HAS_CGC_PCKD */

#if BSP_FEATURE_HAS_CGC_FLASH_CLOCK
    /* check if the MCU has FCLK and FCLK divider is greater than minimum low speed clock divider*/
    CGC_ERROR_RETURN(!(p_clock_cfg->fclk_div < min_div), SSP_ERR_INVALID_MODE);
#endif /* BSP_FEATURE_HAS_CGC_FLASH_CLOCK */

#if BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
    /* check if the MCU has BCLK and BCLK divider is greater than minimum low speed clock divider*/
    CGC_ERROR_RETURN(!(p_clock_cfg->bclk_div < min_div), SSP_ERR_INVALID_MODE);
#endif /* BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */

    /* check if the ICLK  is greater than minimum low speed clock divider*/
    CGC_ERROR_RETURN(p_clock_cfg->iclk_div >= min_div, SSP_ERR_INVALID_MODE);

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Check config dividers values
 * @param[in]   p_clock_cfg              pointer to clock system configuration
 * @retval      SSP_SUCCESS              Operation performed successfully.
 * @retval      SSP_ERR_INVALID_ARGUMENT One or more divider values are not satisfying the conditions
 *                                       as per User Manual
 **********************************************************************************************************************/
static ssp_err_t r_cgc_check_config_dividers(cgc_system_clock_cfg_t const * const p_clock_cfg)
{
    /* Check if MCU supports PCLKA */
#if BSP_FEATURE_HAS_CGC_PCKA
    /* Note: Below conditions are based on divider checking, frequency is inversely proportional */
    /* Check if ICLK is greater than PCLKA */
    CGC_ERROR_RETURN(!(p_clock_cfg->pclka_div < p_clock_cfg->iclk_div), SSP_ERR_INVALID_ARGUMENT);

    /* check if PCLKA is greater than PCLKB */
    CGC_ERROR_RETURN(!(p_clock_cfg->pclkb_div < p_clock_cfg->pclka_div), SSP_ERR_INVALID_ARGUMENT);

    /* check if PCLKD is greater than PCLKA */
    CGC_ERROR_RETURN(!(p_clock_cfg->pclka_div < p_clock_cfg->pclkd_div), SSP_ERR_INVALID_ARGUMENT);
#endif /* BSP_FEATURE_HAS_CGC_PCKA */

#if BSP_FEATURE_HAS_CGC_PCKB && BSP_FEATURE_HAS_CGC_PCKD
    /* Check if PCLKD is greater than PCLKB */
    CGC_ERROR_RETURN(!(p_clock_cfg->pclkb_div < p_clock_cfg->pclkd_div), SSP_ERR_INVALID_ARGUMENT);
#endif /* BSP_FEATURE_HAS_CGC_PCKB && BSP_FEATURE_HAS_CGC_PCKD */

#if BSP_FEATURE_HAS_CGC_PCKB
    /* Check if ICLK is greater than PCLKB */
    CGC_ERROR_RETURN(!(p_clock_cfg->pclkb_div < p_clock_cfg->iclk_div), SSP_ERR_INVALID_ARGUMENT);
#endif /* defined(BSP_FEATURE_HAS_CGC_PCKB) */

#if BSP_FEATURE_HAS_CGC_FLASH_CLOCK
    /* Check if MCU supports FCLK, then check if ICLK is greater than FCLK */
    CGC_ERROR_RETURN(!(p_clock_cfg->fclk_div < p_clock_cfg->iclk_div), SSP_ERR_INVALID_ARGUMENT);
#endif /* BSP_FEATURE_HAS_CGC_FLASH_CLOCK */

#if BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
    /* Check if MCU has BCLK, then check if ICLK > BCLK */
    CGC_ERROR_RETURN(!(p_clock_cfg->bclk_div < p_clock_cfg->iclk_div), SSP_ERR_INVALID_ARGUMENT);
#endif /* BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */

    return SSP_SUCCESS;
}
#endif

/*******************************************************************************************************************//**
 * @brief  Wait for the specified clock to complete the start or stop operation
 * @param[in]   clock_source             clock source that is waited for to complete operation
 * @param[in]   option           	     clock status value
 * @retval      SSP_SUCCESS              Operation performed successfully.
 * @retval      SSP_ERR_HARDWARE_TIMEOUT Operation failed to complete
 **********************************************************************************************************************/
static ssp_err_t r_cgc_wait_to_complete(cgc_clock_t const clock_source, cgc_clock_change_t option)
{
    volatile uint32_t timeout;
    bool start_stop;

    timeout = MAX_REGISTER_WAIT_COUNT;

    if((option != CGC_CLOCK_CHANGE_NONE))
    {
        start_stop = (CGC_CLOCK_CHANGE_START == option);
        while((r_cgc_clock_run_state_get(gp_system_reg, clock_source) != start_stop) && (0U != timeout))
        {
            timeout--;
        }
        CGC_ERROR_RETURN(timeout, SSP_ERR_HARDWARE_TIMEOUT);    // return error if timed out
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Check if the specified clock is running and stable
 * @param[in]   clock_source                    clock to be verified
 * @retval      SSP_SUCCESS                     Operation performed successfully.
 * @retval      SSP_ERR_CLOCK_INACTIVE          Specified clock is not running
 * @retval      SSP_ERR_NOT_STABILIZED   		Clock not stabilized after starting. A finite stabilization time after starting the
 **********************************************************************************************************************/
static ssp_err_t r_cgc_clock_running_status_check(cgc_clock_t const clock_source)
{
#if defined(BSP_FEATURE_HAS_CGC_PLL)
    if (((clock_source == CGC_CLOCK_HOCO) || (clock_source == CGC_CLOCK_MAIN_OSC))
        || (clock_source == CGC_CLOCK_PLL))
#else
    if (((clock_source == CGC_CLOCK_HOCO) || (clock_source == CGC_CLOCK_MAIN_OSC)))
#endif /* defined(BSP_FEATURE_HAS_CGC_PLL) */
    {
        /* make sure the oscillator is stable before setting as system clock */
        CGC_ERROR_RETURN(r_cgc_clock_check(gp_system_reg, clock_source), SSP_ERR_NOT_STABILIZED);
    }
    else  /* for CGC_CLOCK_MOCO, CGC_CLOCK_LOCO or CGC_CLOCK_SUBCLOCK */
    {
        /* Make sure the clock is not stopped before setting as system clock */
        /* There is no way to check stability for this clocks, we just check they are active */
        CGC_ERROR_RETURN(r_cgc_clock_run_state_get(gp_system_reg, clock_source), SSP_ERR_CLOCK_INACTIVE);
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief  Check if the system needs to go out from sub-osc speed mode
  **********************************************************************************************************************/
static void r_cgc_adjust_subosc_speed_mode(void)
{
    /* If system was in subosc speed mode and we turn on other higher speed clocks, subosc speed mode needs to be turned off */
    /* Since running speed is still under 32KHz, we can go to low-speed mode */
    if (CGC_SUBOSC_SPEED_MODE == r_cgc_operating_mode_get(gp_system_reg))
    {
        HW_CGC_SetLowSpeedMode(gp_system_reg);
    }
}

/*******************************************************************************************************************//**
 * @brief Apply options set in configuration structure to all clocks
 * @param[in]   p_clock_cfg                     pointer to clock system configuration
 * @param[in]   options         		        pointer to array of clock status values for all clocks
 * @retval      SSP_SUCCESS                     Operation performed successfully.
 * @retval      SSP_ERR_OSC_STOP_DET_ENABLED  	Illegal attempt to stop MOCO when Oscillation stop is enabled.
 * @retval      SSP_ERR_NOT_STABILIZED   		Clock not stabilized after starting. A finite stabilization time after starting the
 *                                  		    clock has to elapse before it can be stopped.
 * @retval      SSP_ERR_INVALID_ARGUMENT 		Invalid argument used.
 * @retval      SSP_ERR_HARDWARE_TIMEOUT		Hardware timed out.
 **********************************************************************************************************************/
static ssp_err_t r_cgc_apply_start_stop_options(cgc_clocks_cfg_t const * const p_clock_cfg, cgc_clock_change_t const * const options)
{
    ssp_err_t err = SSP_SUCCESS;

    /* start with PLL clock, so that we may stop it before trying to stop the PLL source clk */
#if BSP_FEATURE_HAS_CGC_PLL
    err = r_cgc_clock_start_stop(options[CGC_CLOCK_PLL], CGC_CLOCK_PLL, p_clock_cfg);
    CGC_ERROR_RETURN((SSP_SUCCESS == err) || (SSP_ERR_CLOCK_ACTIVE == err), err);
#endif /* BSP_FEATURE_HAS_CGC_PLL */
    for (uint32_t i = 0U; i < CGC_CLOCK_PLL; i++)
    {
        err = r_cgc_clock_start_stop(options[i], (cgc_clock_t) i, p_clock_cfg);
        CGC_ERROR_RETURN((SSP_SUCCESS == err) || (SSP_ERR_CLOCK_ACTIVE == err), err);
    }

    return SSP_SUCCESS;
}

#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief Check and prepare PLL registers for PLL clock start
 * @param[in]   p_clock_cfg             pointer to clock system configuration
 * @retval      SSP_SUCCESS             Operation performed successfully.
 * @retval      SSP_ERR_INVALID_ARGUMENT  configuration contains illegal parameters
 * @retval      SSP_ERR_MAIN_OSC_INACTIVE  PLL clock source is inactive
 * @retval      SSP_ERR_NOT_STABILIZED     PLL clock is not stabilized
 **********************************************************************************************************************/
static ssp_err_t r_cgc_prepare_pll_clock(cgc_clock_cfg_t * p_clock_cfg)
{
#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
    SSP_ASSERT(NULL != p_clock_cfg);          // return error if NULL pointer to configuration

    /* return error if configuration contains illegal parameters */
    CGC_ERROR_RETURN((r_cgc_clockcfg_valid_check(p_clock_cfg)), SSP_ERR_INVALID_ARGUMENT);
    /* if the PLL source clock isn't running, PLL cannot be turned on, return error */
    CGC_ERROR_RETURN(r_cgc_clock_run_state_get(gp_system_reg, p_clock_cfg->source_clock), SSP_ERR_MAIN_OSC_INACTIVE);
    /*  make sure the PLL has stopped before starting again */
    CGC_ERROR_RETURN(!(r_cgc_clock_check(gp_system_reg, CGC_CLOCK_PLL)), SSP_ERR_NOT_STABILIZED);
#endif /* CGC_CFG_PARAM_CHECKING_ENABLE */

    r_cgc_adjust_subosc_speed_mode();

#if BSP_FEATURE_HAS_CGC_PLL_SRC_CFG
    r_cgc_pll_clocksource_set(gp_system_reg, p_clock_cfg->source_clock); // configure PLL source clock when needed
#endif /* BSP_FEATURE_HAS_CGC_PLL_SRC_CFG */

    r_cgc_pll_divider_set(gp_system_reg, p_clock_cfg->divider);          // configure PLL divider
    r_cgc_pll_multiplier_set(gp_system_reg, p_clock_cfg->multiplier);    // configure PLL multiplier
    r_cgc_init_pllfreq(gp_system_reg);                                // calculate  PLL clock frequency

    return SSP_SUCCESS;
}
#endif /* BSP_FEATURE_HAS_CGC_PLL */




/*******************************************************************************************************************//**
 * @brief      This function initializes CGC variables independent of the C runtime environment.
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_hw_init (void)
{
    /** initialize the clock frequency array */
    g_clock_freq[CGC_CLOCK_HOCO]     = gp_cgc_feature->hoco_freq_hz;  // Initialize the HOCO value.
    g_clock_freq[CGC_CLOCK_MOCO]     = CGC_MOCO_FREQ;       // Initialize the MOCO value.
    g_clock_freq[CGC_CLOCK_LOCO]     = CGC_LOCO_FREQ;       // Initialize the LOCO value.
    g_clock_freq[CGC_CLOCK_MAIN_OSC] = gp_cgc_feature->main_osc_freq_hz;   // Initialize the Main oscillator value.
    g_clock_freq[CGC_CLOCK_SUBCLOCK] = CGC_SUBCLOCK_FREQ;   // Initialize the subclock value.
#if BSP_FEATURE_HAS_CGC_PLL
    g_clock_freq[CGC_CLOCK_PLL]      = CGC_PLL_FREQ;        // The PLL value will be calculated at initialization.
#endif /* BSP_FEATURE_HAS_CGC_PLL */
}

#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief      This function initializes PLL frequency value
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/
static void r_cgc_init_pllfreq (R_SYSTEM_Type * p_system_reg)
{
    uint32_t divider = 0U;
    divider = r_cgc_pll_divider_get(p_system_reg);
    if (divider != 0U) /* prevent divide by 0 */
    {
        uint32_t clock_freq = 0U;
        if (1U == gp_cgc_feature->pllccr_type)
        {
            clock_freq = g_clock_freq[r_cgc_pll_clocksource_get(p_system_reg)];
        }
        if (2U == gp_cgc_feature->pllccr_type)
        {
            clock_freq = g_clock_freq[CGC_CLOCK_MAIN_OSC];
        }
        /* This casts the float result back to an integer.  The multiplier is always a multiple of 0.5, and the clock
         * frequency is always a multiple of 2, so casting to an integer is safe. */
        /* Float used because float32_t is not part of the C99 standard integer definitions. */
        /*LDRA_INSPECTED 90 s *//*LDRA_INSPECTED 90 s */
        g_clock_freq[CGC_CLOCK_PLL] = (uint32_t)
            (((float) clock_freq / (float)divider) * r_cgc_pll_multiplier_get(p_system_reg));
    }
}
#endif /* BSP_FEATURE_HAS_CGC_PLL */

/*******************************************************************************************************************//**
 * @brief      This function sets the main clock drive
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  setting  - clock drive setting
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_main_clock_drive_set (R_SYSTEM_Type * p_system_reg, uint8_t setting)
{
    /*  Set the drive capacity for the main clock. */
    uint8_t modrv_mask = gp_cgc_feature->modrv_mask;
    uint8_t modrv_shift = gp_cgc_feature->modrv_shift;
    HW_CGC_HardwareUnLock();
    p_system_reg->MOMCR =
        (uint8_t) ((p_system_reg->MOMCR & (~modrv_mask)) | ((setting << modrv_shift) & modrv_mask));
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function sets the subclock drive
 * 
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  setting   clock drive setting
 * 
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_subclock_drive_set (R_SYSTEM_Type * p_system_reg, uint8_t setting)
{
    /*  Set the drive capacity for the subclock. */
    uint8_t sodrv_mask = gp_cgc_feature->sodrv_mask;
    uint8_t sodrv_shift = gp_cgc_feature->sodrv_shift;
    HW_CGC_HardwareUnLock();
    /* Sub-Clock Drive Capability can be changed only if Sub-Clock is stopped */
    if (p_system_reg->SOSCCR_b.SOSTP)
    {
        p_system_reg->SOMCR =
            (uint8_t) ((p_system_reg->SOMCR & (~sodrv_mask)) | ((setting << sodrv_shift) & sodrv_mask));
    }
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function starts the selected clock
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock  the clock to start
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_clock_start (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock)
{
    /* Start the selected clock. */
    HW_CGC_HardwareUnLock();

    switch (clock)
    {
        case CGC_CLOCK_HOCO:
            p_system_reg->HOCOCR_b.HCSTP = 0U; /* Start the HOCO clock. */
            break;

        case CGC_CLOCK_MOCO:
            p_system_reg->MOCOCR_b.MCSTP = 0U; /* Start the MOCO clock.*/
            break;

        case CGC_CLOCK_LOCO:
            p_system_reg->LOCOCR_b.LCSTP = 0U; /* Start the LOCO clock.*/
            break;

        case CGC_CLOCK_MAIN_OSC:
            p_system_reg->MOSCCR_b.MOSTP = 0U; /* Start the Main oscillator.*/
            break;

        case CGC_CLOCK_SUBCLOCK:
            p_system_reg->SOSCCR_b.SOSTP = 0U; /* Start the subClock.*/
            break;

#if BSP_FEATURE_HAS_CGC_PLL
        case CGC_CLOCK_PLL:
            p_system_reg->PLLCR_b.PLLSTP = 0U; /* Start the PLL clock.*/
            break;
#endif /* BSP_FEATURE_HAS_CGC_PLL */

        default:
            break;
    }

    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function stops the selected clock
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock the clock to stop
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_clock_stop (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock)
{
    /*  Stop the selected clock. */
    HW_CGC_HardwareUnLock();
    switch (clock)
    {
        case CGC_CLOCK_HOCO:
            p_system_reg->HOCOCR_b.HCSTP = 1U; /* Stop the HOCO clock.*/
            break;

        case CGC_CLOCK_MOCO:
            p_system_reg->MOCOCR_b.MCSTP = 1U; /* Stop the MOCO clock.*/
            break;

        case CGC_CLOCK_LOCO:
            p_system_reg->LOCOCR_b.LCSTP = 1U; /* Stop the LOCO clock.*/
            break;

        case CGC_CLOCK_MAIN_OSC:
            p_system_reg->MOSCCR_b.MOSTP = 1U; /* Stop the  main oscillator.*/
            break;

        case CGC_CLOCK_SUBCLOCK:
            p_system_reg->SOSCCR_b.SOSTP = 1U; /* Stop the subClock.*/
            break;

#if BSP_FEATURE_HAS_CGC_PLL
        case CGC_CLOCK_PLL:
            p_system_reg->PLLCR_b.PLLSTP = 1U; /* Stop PLL clock.*/
            break;
#endif /* BSP_FEATURE_HAS_CGC_PLL */

        default:
            break;
    }

    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function checks the selected clock for stability
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock the clock to check
 * @retval     bool   true if stable, false if not stable or stopped
 **********************************************************************************************************************/

static bool r_cgc_clock_check (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock)
{
    /*  Check for stability of selected clock. */
    switch (clock)
    {
        case CGC_CLOCK_HOCO:
            if (p_system_reg->OSCSF_b.HOCOSF)
            {
                return true; /* HOCO is stable */
            }

            break;

        case CGC_CLOCK_MAIN_OSC:
            if (p_system_reg->OSCSF_b.MOSCSF)
            {
                return true; /* Main Osc is stable */
            }

            break;

#if BSP_FEATURE_HAS_CGC_PLL
        case CGC_CLOCK_PLL:
            if (p_system_reg->OSCSF_b.PLLSF)
            {
                return true; /* PLL is stable */
            }

            break;
#endif /* BSP_FEATURE_HAS_CGC_PLL */

        default:
            /* All other clocks don't have a stability flag, return true */
            return true;
            break;
    }

    return false;
}

#if (CGC_CFG_SUBCLOCK_AT_RESET_ENABLE == 1)
/*******************************************************************************************************************//**
 * @brief      This function delays for a specified number of clock cycles, of the selected clock
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock the clock to time
 * @param[in]  cycles the number of cycles to delay
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_delay_cycles (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock, uint16_t cycles)
{
    /* delay for number of clock cycles specified */

    uint32_t               delay_count;
    uint32_t               clock_freq_in;
    uint32_t               system_clock_freq;

    system_clock_freq = r_cgc_clock_hzget(p_system_reg, CGC_SYSTEM_CLOCKS_ICLK);
    clock_freq_in     = g_clock_freq[clock];
    if (clock_freq_in != 0U)             // ensure divide by zero doesn't happen
    {
        delay_count = ((system_clock_freq / clock_freq_in) * cycles);

        while (delay_count > 0U)
        {
            delay_count--;
        }
    }
}
#endif

/*******************************************************************************************************************//**
 * @brief      This function sets the source of the main oscillator
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  osc the source of the main clock oscillator
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_mainosc_source_set (R_SYSTEM_Type * p_system_reg, cgc_osc_source_t osc)
{
    /* Set the source to resonator or external osc. */
    HW_CGC_HardwareUnLock();
    p_system_reg->MOMCR_b.MOSEL = osc;
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function sets the clock wait time
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock  the clock to set the wait time for
 * @param[in]  setting is wait time
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_clock_wait_set (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock, uint8_t setting)
{
    SSP_PARAMETER_NOT_USED(clock);
    /* Set the clock wait time */
    HW_CGC_HardwareUnLock();
    p_system_reg->MOSCWTCR_b.MSTS = (uint8_t)(setting & 0x0F);
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function sets the system clock source
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock         the clock to use for the system clock
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_clock_source_set (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock)
{
    /* Set the system clock source */
    HW_CGC_HardwareUnLock();
    p_system_reg->SCKSCR_b.CKSEL = clock;    //set the system clock source
    HW_CGC_HardwareLock();
}

#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief      This function returns the PLL clock source
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     clock type   PLL clock source
 **********************************************************************************************************************/

static cgc_clock_t r_cgc_pll_clocksource_get (R_SYSTEM_Type * p_system_reg)
{
    /*  PLL source selection only available on S7G2 */
    if (gp_cgc_feature->pll_src_configurable)
    {
        /* Get the PLL clock source */
        if (p_system_reg->PLLCCR_b.PLLSRCSEL == 1U)
        {
            return CGC_CLOCK_HOCO;
        }
    }

    return CGC_CLOCK_MAIN_OSC;
}
#endif /* BSP_FEATURE_HAS_CGC_PLL */

#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief      This function sets the PLL clock source
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock  current system clock
 * @retval     none
 **********************************************************************************************************************/
#if BSP_FEATURE_HAS_CGC_PLL_SRC_CFG
static void r_cgc_pll_clocksource_set (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock)
{
    /* Set the PLL clock source */
    HW_CGC_HardwareUnLock();
    if (CGC_CLOCK_MAIN_OSC == clock)
    {
        p_system_reg->PLLCCR_b.PLLSRCSEL = CGC_PLL_MAIN_OSC;
    }
    else
    {
        /* The default value is HOCO. */
        p_system_reg->PLLCCR_b.PLLSRCSEL = CGC_PLL_HOCO;
    }
    HW_CGC_HardwareLock();
}
#endif
#endif /* BSP_FEATURE_HAS_CGC_PLL */
#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief      This function sets the PLL multiplier
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  multiplier    multiplier value
 * @retval     none
 **********************************************************************************************************************/

/*LDRA_INSPECTED 90 s float used because float32_t is not part of the C99 standard integer definitions. */
static void r_cgc_pll_multiplier_set (R_SYSTEM_Type * p_system_reg, float multiplier)
{
    /* Set the PLL multiplier */
    if (1U == gp_cgc_feature->pllccr_type)
    {
        uint32_t write_val                  = (uint32_t) (multiplier * 2) - 1;
        HW_CGC_HardwareUnLock();
        p_system_reg->PLLCCR_b.PLLMUL  = (uint8_t)(write_val & 0x3F);
        HW_CGC_HardwareLock();
    }
    if (2U == gp_cgc_feature->pllccr_type)
    {
        uint32_t write_val                  = (uint32_t) multiplier - 1;
        HW_CGC_HardwareUnLock();
        p_system_reg->PLLCCR2_b.PLLMUL = (uint8_t)(write_val & 0x1F);
        HW_CGC_HardwareLock();
    }
}
#endif /* BSP_FEATURE_HAS_CGC_PLL */


#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief      This function gets the PLL multiplier
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     float multiplier value
 **********************************************************************************************************************/

/*LDRA_INSPECTED 90 s float used because float32_t is not part of the C99 standard integer definitions. */
static float r_cgc_pll_multiplier_get (R_SYSTEM_Type * p_system_reg)
{
    /* Get the PLL multiplier */
    /*LDRA_INSPECTED 90 s */
    float pll_mul = 0.0f;

    if (1U == gp_cgc_feature->pllccr_type)
    {
        /* This cast is used for compatibility with the S7 implementation. */
        /*LDRA_INSPECTED 90 s */
        pll_mul = ((float)(p_system_reg->PLLCCR_b.PLLMUL + 1U)) / 2.0f;
    }
    if (2U == gp_cgc_feature->pllccr_type)
    {
        /* This cast is used for compatibility with the S1 and S3 implementation. */
        /*LDRA_INSPECTED 90 s */
        pll_mul = (float) p_system_reg->PLLCCR2_b.PLLMUL + 1.0f;
    }

    return pll_mul;
}
#endif /* BSP_FEATURE_HAS_CGC_PLL */

#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief      This function sets the PLL divider
 * @param[in] p_system_reg  pointer to system register structure
 * @param[in] divider  divider value
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_pll_divider_set (R_SYSTEM_Type * p_system_reg, cgc_pll_div_t divider)
{
    /* Set the PLL divider */
    if (1U == gp_cgc_feature->pllccr_type)
    {
        uint16_t register_value = g_pllccr_div_setting[divider];
        HW_CGC_HardwareUnLock();
        p_system_reg->PLLCCR_b.PLIDIV  = (uint16_t)(register_value & 0x3);
        HW_CGC_HardwareLock();
    }
    if (2U == gp_cgc_feature->pllccr_type)
    {
        uint16_t register_value = g_pllccr2_div_setting[divider];
        HW_CGC_HardwareUnLock();
        p_system_reg->PLLCCR2_b.PLODIV = (uint8_t)(register_value & 0x3);
        HW_CGC_HardwareLock();
    }
}
#endif /* BSP_FEATURE_HAS_CGC_PLL */

#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief      This function gets the PLL divider
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     divider
 **********************************************************************************************************************/

static uint16_t r_cgc_pll_divider_get (R_SYSTEM_Type * p_system_reg)
{
    /* Get the PLL divider */
    uint16_t ret = 1U;
    if (1U == gp_cgc_feature->pllccr_type)
    {
        /* This cast maps the register value to an enumerated list. */
        ret = g_pllccr_div_value[p_system_reg->PLLCCR_b.PLIDIV];
    }
    if (2U == gp_cgc_feature->pllccr_type)
    {
        /* This cast maps the register value to an enumerated list. */
        ret = g_pllccr2_div_value[p_system_reg->PLLCCR2_b.PLODIV];
    }
    return ret;
}
#endif /* BSP_FEATURE_HAS_CGC_PLL */

/*******************************************************************************************************************//**
 * @brief      This function gets the system dividers
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  cfg           a pointer to a cgc_system_clock_cfg_t struct
 * @param[out] cfg           a pointer to a cgc_system_clock_cfg_t struct
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_system_dividers_get (R_SYSTEM_Type * p_system_reg, cgc_system_clock_cfg_t * cfg)
{
    /* Get the system dividers */
    /* The cgc_sys_clock_div_t defines all valid values (3 bits each) for these registers as per the hardware manual,
     * and each of the elements of SCKDIVCR_b are 3 bits of a 32-bit value, so these casts are safe. */
#if BSP_FEATURE_HAS_CGC_PCKA
    cfg->pclka_div = (cgc_sys_clock_div_t) p_system_reg->SCKDIVCR_b.PCKA;
#else
    /* Fill the structure field with 0 which is the value of corresponding reserved bits in register */
    cfg->pclka_div = CGC_SYS_CLOCK_DIV_1;
#endif /* BSP_FEATURE_HAS_CGC_PCKA */
#if BSP_FEATURE_HAS_CGC_PCKB
    cfg->pclkb_div = (cgc_sys_clock_div_t) p_system_reg->SCKDIVCR_b.PCKB;
#else
    /* Fill the structure field with 0 which is the value of corresponding reserved bits in register */
    cfg->pclkb_div = CGC_SYS_CLOCK_DIV_1;
#endif /* BSP_FEATURE_HAS_CGC_PCKB */
#if BSP_FEATURE_HAS_CGC_PCKC
    cfg->pclkc_div = (cgc_sys_clock_div_t) p_system_reg->SCKDIVCR_b.PCKC;
#else
    /* Fill the structure field with 0 which is the value of corresponding reserved bits in register */
    cfg->pclkc_div = CGC_SYS_CLOCK_DIV_1;
#endif /* BSP_FEATURE_HAS_CGC_PCKC */
#if BSP_FEATURE_HAS_CGC_PCKD
    cfg->pclkd_div = (cgc_sys_clock_div_t) p_system_reg->SCKDIVCR_b.PCKD;
#else
    /* Fill the structure field with 0 which is the value of corresponding reserved bits in register */
    cfg->pclkd_div = CGC_SYS_CLOCK_DIV_1;
#endif /* BSP_FEATURE_HAS_CGC_PCKD */
    cfg->bclk_div  = (cgc_sys_clock_div_t) p_system_reg->SCKDIVCR_b.BCK;
    cfg->fclk_div  = (cgc_sys_clock_div_t) p_system_reg->SCKDIVCR_b.FCK;
    cfg->iclk_div  = (cgc_sys_clock_div_t) p_system_reg->SCKDIVCR_b.ICK;
}

#if (CGC_CFG_PARAM_CHECKING_ENABLE == 1)
#if BSP_FEATURE_HAS_CGC_PLL
/*******************************************************************************************************************//**
 * @brief      This function tests the clock configuration for validity (only for systems with PLL)
 * @param[in]  cfg  a pointer to a cgc_clock_cfg_t struct
 * @retval     bool  true if configuration is valid
 **********************************************************************************************************************/

static bool r_cgc_clockcfg_valid_check (cgc_clock_cfg_t * cfg)
{
#if !BSP_FEATURE_HAS_CGC_PLL
    SSP_PARAMETER_NOT_USED(cfg);
#else
    /* check for valid configuration */

    /* Check maximum and minimum divider values */
    if (gp_cgc_feature->pll_div_max < cfg->divider)
    {
        return false;    // Value is out of range.
    }

    /* Check maximum and minimum multiplier values */
    /* Float used because float32_t is not part of the C99 standard integer definitions. */
    /*LDRA_INSPECTED 90 s *//*LDRA_INSPECTED 90 s */
    if (((float)gp_cgc_feature->pll_mul_max < cfg->multiplier) || ((float)gp_cgc_feature->pll_mul_min > cfg->multiplier))
    {
        return false;   // Value is out of range.
    }

    if ((CGC_CLOCK_MAIN_OSC != cfg->source_clock) && (CGC_CLOCK_HOCO != cfg->source_clock))
    {
        return false;   // Value is out of range.
    }
#endif /* #if BSP_FEATURE_HAS_CGC_PLL */

    return true;
}
#endif
#endif

/*******************************************************************************************************************//**
 * @brief      This function returns the divider of the selected clock
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock  the system clock to get the divider for
 * @retval     divider value
 **********************************************************************************************************************/

static uint32_t r_cgc_clock_divider_get (R_SYSTEM_Type * p_system_reg, cgc_system_clocks_t clock)
{
    /*  get divider of selected clock */
    uint32_t divider;
    divider = 0U;

    switch (clock)
    {
#if BSP_FEATURE_HAS_CGC_PCKA
        case CGC_SYSTEM_CLOCKS_PCLKA:
            divider = p_system_reg->SCKDIVCR_b.PCKA;
            break;
#endif /* #if BSP_FEATURE_HAS_CGC_PCKA */

#if BSP_FEATURE_HAS_CGC_PCKB
        case CGC_SYSTEM_CLOCKS_PCLKB:
            divider = p_system_reg->SCKDIVCR_b.PCKB;
            break;
#endif /* #if BSP_FEATURE_HAS_CGC_PCKB */

#if BSP_FEATURE_HAS_CGC_PCKC
        case CGC_SYSTEM_CLOCKS_PCLKC:
            divider = p_system_reg->SCKDIVCR_b.PCKC;
            break;
#endif /* #if BSP_FEATURE_HAS_CGC_PCKC */

#if BSP_FEATURE_HAS_CGC_PCKD
        case CGC_SYSTEM_CLOCKS_PCLKD:
            divider = p_system_reg->SCKDIVCR_b.PCKD;
            break;
#endif /* #if BSP_FEATURE_HAS_CGC_PCKD */

#if BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
        case CGC_SYSTEM_CLOCKS_BCLK:
            divider = p_system_reg->SCKDIVCR_b.BCK;
            break;
#endif /* #if BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */

#if BSP_FEATURE_HAS_CGC_FLASH_CLOCK
        case CGC_SYSTEM_CLOCKS_FCLK:
            divider = p_system_reg->SCKDIVCR_b.FCK;
            break;
#endif /* #if BSP_FEATURE_HAS_CGC_FLASH_CLOCK */

        case CGC_SYSTEM_CLOCKS_ICLK:
            divider = p_system_reg->SCKDIVCR_b.ICK;
            break;
        default:
            break;
    }

    return (divider);
}

/*******************************************************************************************************************//**
 * @brief      This function returns the frequency of the selected clock
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock  the system clock to get the freq for
 * @retval     frequency
 **********************************************************************************************************************/

static uint32_t r_cgc_clock_hzget (R_SYSTEM_Type * p_system_reg, cgc_system_clocks_t clock)
{
    /*  get frequency of selected clock */
    uint32_t divider;
    divider =  r_cgc_clock_divider_get(p_system_reg, clock);
    return (uint32_t) ((g_clock_freq[HW_CGC_ClockSourceGet(p_system_reg)]) >> divider);
}


/*******************************************************************************************************************//**
 * @brief      This function returns the frequency of the selected clock
 * @param[in]  source_clock  the source clock, such as the main osc or PLL
 * @param[in]  divider divider value
 * @retval     frequency
 **********************************************************************************************************************/

static uint32_t r_cgc_clockhz_calculate (cgc_clock_t source_clock,  cgc_sys_clock_div_t divider)
{
    /*  calculate frequency of selected system clock, given the source clock and divider */
    return (uint32_t) ((g_clock_freq[source_clock]) >> (uint32_t)divider);
}

/*******************************************************************************************************************//**
 * @brief      This function enables the osc stop detection function and interrupt
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/
static void r_cgc_oscstop_detect_enable (R_SYSTEM_Type * p_system_reg)
{
    /* enable hardware oscillator stop detect function */
    HW_CGC_HardwareUnLock();
    p_system_reg->OSTDCR_b.OSTDE  = 1U;        // enable osc stop detection
    p_system_reg->OSTDCR_b.OSTDIE = 1U;        // enable osc stop detection interrupt
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function disables the osc stop detection function and interrupt
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/
static void r_cgc_oscstop_detect_disable (R_SYSTEM_Type * p_system_reg)
{
    /* disable hardware oscillator stop detect function */
    HW_CGC_HardwareUnLock();
    p_system_reg->OSTDCR_b.OSTDIE = 0U;        // disable osc stop detection interrupt
    p_system_reg->OSTDCR_b.OSTDE  = 0U;        // disable osc stop detection
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function clear the status of the stop detection function
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     bool  true if oscillator stop detection flag cleared, false if no oscillator stop detected
 **********************************************************************************************************************/

static bool r_cgc_oscstop_status_clear (R_SYSTEM_Type * p_system_reg)
{
    /* clear hardware oscillator stop detect status */
    if (p_system_reg->OSTDSR_b.OSTDF == 1U)
    {
        HW_CGC_HardwareUnLock();
        p_system_reg->OSTDSR_b.OSTDF = 0U;
        HW_CGC_HardwareLock();
        return true;            // stop detection cleared
    }

    return false;               // can't clear flag because no stop detected
}


/*******************************************************************************************************************//**
 * @brief      This function configures the ClockOut divider and clock source
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock  the clock out source
 * @param[in]  divider  the clock out divider
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_clockout_cfg (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock, cgc_clockout_dividers_t divider)
{
    /*  */
    HW_CGC_HardwareUnLock();
    p_system_reg->CKOCR_b.CKOEN  = 0U;            // disable CLKOUT to change configuration
    p_system_reg->CKOCR_b.CKODIV = divider;       // set CLKOUT dividers
    p_system_reg->CKOCR_b.CKOSEL = clock;         // set CLKOUT clock source
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function enables ClockOut
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_clockout_enable (R_SYSTEM_Type * p_system_reg)
{
    /* Enable CLKOUT output  */
    HW_CGC_HardwareUnLock();
    p_system_reg->CKOCR_b.CKOEN = 1U;
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function disables ClockOut
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_clockout_disable (R_SYSTEM_Type * p_system_reg)
{
    /* Disable CLKOUT output */
    HW_CGC_HardwareUnLock();
    p_system_reg->CKOCR_b.CKOEN = 0U;
    HW_CGC_HardwareLock();
}


/*******************************************************************************************************************//**
 * @brief      This updates the Systick timer period
 * @param[in]  reload_value Reload value, calculated by caller
 * @retval     bool  true if reload is successful
 **********************************************************************************************************************/
static bool r_cgc_systick_update(uint32_t reload_value)
{
    bool result = false;
    /* SysTick is defined by CMSIS and will not be modified. */
    /*LDRA_INSPECTED 93 S *//*LDRA_INSPECTED 96 S *//*LDRA_INSPECTED 330 S *//*LDRA_INSPECTED 330 S */
    uint32_t systick_ctrl = SysTick->CTRL;

    /** If there is an RTOS in place AND the Systick interrupt is not yet enabled, just return and do nothing */
#if (1 == BSP_CFG_RTOS)
    /*LDRA_INSPECTED 93 S *//*LDRA_INSPECTED 96 S *//*LDRA_INSPECTED 330 S *//*LDRA_INSPECTED 330 S */
    if ((SysTick->CTRL & SysTick_CTRL_TICKINT_Msk) == 0)
    {
        return(true);           ///< Not really an error.
    }
#endif
    /*LDRA_INSPECTED 93 S *//*LDRA_INSPECTED 96 S *//*LDRA_INSPECTED 330 S *//*LDRA_INSPECTED 330 S */
    SysTick->CTRL = 0U;          ///< Disable systick while we reset the counter

    // Number of ticks between two interrupts.
    /* Save the Priority for Systick Interrupt, will need to be restored after SysTick_Config() */
    uint32_t systick_priority = NVIC_GetPriority (SysTick_IRQn);
    if (0U == SysTick_Config(reload_value))
    {
        result = true;
        NVIC_SetPriority (SysTick_IRQn, systick_priority); ///< Restore systick priority
    }
    else
    {
        /*LDRA_INSPECTED 93 S *//*LDRA_INSPECTED 96 S *//*LDRA_INSPECTED 330 S *//*LDRA_INSPECTED 330 S */
        SysTick->CTRL = systick_ctrl;          ///< If we were provided an invalid (ie too large) reload value,
                                               ///< keep using prior value.
    }
    return(result);
}


/*******************************************************************************************************************//**
 * @brief      This function sets the system dividers
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  cfg pointer to a cgc_system_clock_cfg_t struct
 * @retval     none
 **********************************************************************************************************************/

static void r_cgc_system_dividers_set (R_SYSTEM_Type * p_system_reg, cgc_system_clock_cfg_t const * const cfg)
{
    sckdivcr_clone_t sckdivcr;
    /* The cgc_sys_clock_div_t fits in the 3 bits, and each of the elements of sckdivcr_b are 3 bits of a 32-bit value,
     * so these casts are safe. */
    sckdivcr.sckdivcr_w = (uint32_t) 0x00;

#if BSP_FEATURE_HAS_CGC_PCKA
    sckdivcr.sckdivcr_b.pcka = (uint32_t)(cfg->pclka_div & 0x7);
#endif /* BSP_FEATURE_HAS_CGC_PCKA */
#if BSP_FEATURE_HAS_CGC_PCKB
    sckdivcr.sckdivcr_b.pckb = (uint32_t)(cfg->pclkb_div & 0x7);
#endif /* BSP_FEATURE_HAS_CGC_PCKB */
#if BSP_FEATURE_HAS_CGC_PCKC
    sckdivcr.sckdivcr_b.pckc = (uint32_t)(cfg->pclkc_div & 0x7);
#endif /* BSP_FEATURE_HAS_CGC_PCKC */
#if BSP_FEATURE_HAS_CGC_PCKD
    sckdivcr.sckdivcr_b.pckd = (uint32_t)(cfg->pclkd_div & 0x7);
#endif /* BSP_FEATURE_HAS_CGC_PCKD */
#if BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
    sckdivcr.sckdivcr_b.bck  = (uint32_t)(cfg->bclk_div  & 0x7);
#endif /* BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */
#if BSP_FEATURE_HAS_CGC_FLASH_CLOCK
    sckdivcr.sckdivcr_b.fck  = (uint32_t)(cfg->fclk_div  & 0x7);
#endif /* BSP_FEATURE_HAS_CGC_FLASH_CLOCK */

    /* All MCUs have ICLK. */
    sckdivcr.sckdivcr_b.ick  = (uint32_t)(cfg->iclk_div  & 0x7);

    /** If the MCU requires, set bck divider with pckb divider bits */
    if(1U == gp_cgc_feature->set_bck_with_pckb)
    {
        sckdivcr.sckdivcr_b.bck = (uint32_t)(cfg->pclkb_div & 0x7);
    }
    /* Set the system dividers */
    HW_CGC_HardwareUnLock();
    p_system_reg->SCKDIVCR       = sckdivcr.sckdivcr_w;
    HW_CGC_HardwareLock();
}

/**********************************************************************************************************************
* Private Global Functions
 **********************************************************************************************************************/
/*******************************************************************************************************************//**
 * @brief      This function returns the run state of the selected clock
 * @param[in]  clock  the clock to check
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  clock - the clock to check
 * @retval     bool  true if clock is running, false if stopped
 **********************************************************************************************************************/

bool r_cgc_clock_run_state_get (R_SYSTEM_Type * p_system_reg, cgc_clock_t clock)
{
    /* Get clock run state. */

    switch (clock)
    {
        case CGC_CLOCK_HOCO:
            if (!(p_system_reg->HOCOCR_b.HCSTP))
            {
                return true; /* HOCO clock is running */
            }

            break;

        case CGC_CLOCK_MOCO:
            if (!(p_system_reg->MOCOCR_b.MCSTP))
            {
                return true; /* MOCO clock is running */
            }

            break;

        case CGC_CLOCK_LOCO:
            if (!(p_system_reg->LOCOCR_b.LCSTP))
            {
                return true; /* LOCO clock is running */
            }

            break;

        case CGC_CLOCK_MAIN_OSC:
            if (!(p_system_reg->MOSCCR_b.MOSTP))
            {
                return true; /* main osc clock is running */
            }

            break;

        case CGC_CLOCK_SUBCLOCK:
            if (!(p_system_reg->SOSCCR_b.SOSTP))
            {
                return true; /* Subclock is running */
            }

            break;

#if BSP_FEATURE_HAS_CGC_PLL
        case CGC_CLOCK_PLL:
            if (!(p_system_reg->PLLCR_b.PLLSTP))
            {
                return true; /* PLL clock is running */
            }

            break;
#endif /* BSP_FEATURE_HAS_CGC_PLL */

        default:
            break;
    }

    return false;
}


/*******************************************************************************************************************//**
 * @brief      This function checks the MCU for High Speed Mode
 * @param[in]  p_system_reg  pointer to system register structure
 * @retval     operating_mode  current mode of operation read from OPCCR register
 **********************************************************************************************************************/

cgc_operating_modes_t r_cgc_operating_mode_get (R_SYSTEM_Type * p_system_reg)
{
    cgc_operating_modes_t operating_mode = CGC_HIGH_SPEED_MODE;
    if (1U == p_system_reg->SOPCCR_b.SOPCM)
    {
        operating_mode = CGC_SUBOSC_SPEED_MODE;
    }
    else
    {
        operating_mode = (cgc_operating_modes_t)p_system_reg->OPCCR_b.OPCM;
    }
    return operating_mode;
}

/*******************************************************************************************************************//**
 * @brief      This function changes the operating power control mode
 * @param[in]  p_system_reg  pointer to system register structure
 * @param[in]  operating_mode Operating power control mode
 **********************************************************************************************************************/

void r_cgc_operating_hw_modeset (R_SYSTEM_Type * p_system_reg, cgc_operating_modes_t operating_mode)
{
    bsp_cache_state_t cache_state;
    volatile uint32_t timeout;
    timeout = CGC_HW_MAX_REGISTER_WAIT_COUNT;

    /** Enable writing to OPCCR and SOPCCR registers. */
    HW_CGC_LPMHardwareUnLock();

    cache_state = BSP_CACHE_STATE_OFF;
    R_BSP_CacheOff(&cache_state);                           // Turn the cache off.

    while ((0U != p_system_reg->SOPCCR_b.SOPCMTSF) || (0U != p_system_reg->OPCCR_b.OPCMTSF))
    {
        /** Wait for transition to complete. */
        timeout--;
        if(0U == timeout)
        {
            /** Disable writing to OPCCR and SOPCCR registers. */
            HW_CGC_LPMHardwareLock();
            return;
        }
    }

    /** The Sub-osc bit has to be cleared first. */
    p_system_reg->SOPCCR_b.SOPCM = CGC_SOPCCR_CLEAR_SUBOSC_SPEED_MODE;
    timeout = CGC_HW_MAX_REGISTER_WAIT_COUNT;
    while (((0U != p_system_reg->SOPCCR_b.SOPCMTSF) || (0U != p_system_reg->OPCCR_b.OPCMTSF)) && (0U != timeout))
    {
        /** Wait for transition to complete. */
        timeout--;
    }

    /** Set OPCCR. */
    if(CGC_SUBOSC_SPEED_MODE == operating_mode)
    {
        p_system_reg->OPCCR_b.OPCM = CGC_OPPCR_OPCM_MASK & CGC_OPCCR_LOW_SPEED_MODE;
    }
    else
    {
        p_system_reg->OPCCR_b.OPCM = CGC_OPPCR_OPCM_MASK & (uint32_t)operating_mode;
    }
    timeout = CGC_HW_MAX_REGISTER_WAIT_COUNT;
    while (((0U != p_system_reg->SOPCCR_b.SOPCMTSF) || (0U != p_system_reg->OPCCR_b.OPCMTSF)) && (0U != timeout))
    {
        /** Wait for transition to complete. */
        timeout--;
    }

    /** Set SOPCCR. */
    if(CGC_SUBOSC_SPEED_MODE == operating_mode)
    {
        p_system_reg->SOPCCR_b.SOPCM = CGC_SOPPCR_SOPCM_MASK & CGC_SOPCCR_SET_SUBOSC_SPEED_MODE;
    }
    else
    {
        p_system_reg->SOPCCR_b.SOPCM = CGC_SOPPCR_SOPCM_MASK & CGC_SOPCCR_CLEAR_SUBOSC_SPEED_MODE;
    }
    timeout = CGC_HW_MAX_REGISTER_WAIT_COUNT;
    while (((0U != p_system_reg->SOPCCR_b.SOPCMTSF) || (0U != p_system_reg->OPCCR_b.OPCMTSF)) && (0U != timeout))
    {
        /** Wait for transition to complete. */
        timeout--;
    }

    R_BSP_CacheSet(cache_state);                            // Restore cache to previous state.

    /** Disable writing to OPCCR and SOPCCR registers. */
    HW_CGC_LPMHardwareLock();
}


/*******************************************************************************************************************//**
 * @brief      This function sets the HOCO wait time register
 * @param[in]  hoco_wait  HOCOWTCR HSTS setting
 * @param[in]  p_system_reg  pointer to system register structure 
 * @retval     none
 **********************************************************************************************************************/

void r_cgc_hoco_wait_control_set (R_SYSTEM_Type * p_system_reg, uint8_t hoco_wait)
{
    /* Set the system clock source */
    HW_CGC_HardwareUnLock();
    /* Set HOCOWTCR_b.HSTS */
    p_system_reg->HOCOWTCR_b.HSTS = 0x7U & hoco_wait;
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @} (end addtogroup CGC)
 **********************************************************************************************************************/
