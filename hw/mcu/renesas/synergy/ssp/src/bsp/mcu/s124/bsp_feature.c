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
* File Name    : bsp_feature.c
* Description  : Provides query functions for MCU specific features.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "../all/bsp_feature.h"
#include "r_cgc.h"

#if defined(BSP_MCU_GROUP_S124)

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/** The main oscillator drive value is based upon the oscillator frequency selected in the configuration */
#if (BSP_CFG_XTAL_HZ > (9999999))
#define CGC_MAINCLOCK_DRIVE (0x00U)
#else
#define CGC_MAINCLOCK_DRIVE (0x01U)
#endif

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
 
/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/
static const bsp_feature_cgc_t g_cgc_feature =
{
    .hoco_freq_hz           = (uint32_t)BSP_HOCO_HZ,
    .main_osc_freq_hz       = (uint32_t)BSP_CFG_XTAL_HZ,
    .high_speed_freq_hz     = 4000000U,  ///< Max ICLK frequency while in Low Voltage Mode
    .modrv_mask             = 0x08U,
    .modrv_shift            = 0x3U,
    .sodrv_mask             = 0x03U,
    .sodrv_shift            = 0x0U,
    .pll_div_max            = CGC_PLL_DIV_1, // No PLL
    .pll_mul_min            = 0xFFU,         // No PLL
    .pll_mul_max            = 0x0U,          // No PLL
    .mainclock_drive        = CGC_MAINCLOCK_DRIVE,
    .pll_src_configurable   = 0U,
    .pllccr_type            = 0U,
    .iclk_div               = BSP_CFG_ICK_DIV,
    .has_lcd_clock          = 0U,
    .has_sdram_clock        = 0U,
    .has_usb_clock_div      = 0U,
    .has_pclka              = 0U,
    .has_pclkb              = 1U,
    .has_pclkc              = 0U,
    .has_pclkd              = 1U,
    .has_fclk               = 0U,
    .has_bclk               = 0U,
    .has_sdadc_clock        = 0U,
    .set_bck_with_pckb      = 0U,           ///< This MCU does not have to set bck bits with pckb bits
    .middle_speed_max_freq_hz = 8000000U,   ///< This MCU does have Middle Speed Mode, up to 8MHz
    .low_speed_max_freq_hz    = 1000000U,   ///< This MCU does have Low Speed Mode, up to 1MHz
    .low_voltage_max_freq_hz  = 4000000U,   ///< This MCU does have Low Voltage Mode, up to 4MHz
    .has_subosc_speed         = 1U,         ///< This MCU does have Subosc Speed Mode
    .low_speed_pclk_div_min   = 0x04U, ///< Minimum divisor for peripheral clocks when using oscillator stop detect
    .low_voltage_pclk_div_min = 0x02U, ///< Minimum divisor for peripheral clocks when using oscillator stop detect
};

void R_BSP_FeatureSciGet(bsp_feature_sci_t * p_sci_feature)
{
    p_sci_feature->clock = (uint8_t) CGC_SYSTEM_CLOCKS_PCLKB;
}

void R_BSP_FeatureRspiGet(bsp_feature_rspi_t * p_rspi_feature)
{
    p_rspi_feature->clock = (uint8_t) CGC_SYSTEM_CLOCKS_PCLKB;
    p_rspi_feature->has_ssl_level_keep = 0U;
    p_rspi_feature->swap = 0U;
}

void R_BSP_FeatureLvdGet(bsp_feature_lvd_t * p_lvd_feature)
{
    p_lvd_feature->has_digital_filter = 0U;
    p_lvd_feature->monitor_1_low_threshold = 0x0FU;  // LVD_THRESHOLD_MONITOR_1_LEVEL_F, 1.65V (Vdet1_F)
    p_lvd_feature->monitor_1_hi_threshold  = 0x00U;  // LVD_THRESHOLD_MONITOR_1_LEVEL_0, 4.29V (Vdet1_0)
    p_lvd_feature->monitor_2_low_threshold = 0x03U;  // LVD_THRESHOLD_MONITOR_2_LEVEL_3, 3.84V (Vdet2_3)
    p_lvd_feature->monitor_2_hi_threshold  = 0x00U;  // LVD_THRESHOLD_MONITOR_2_LEVEL_0, 4.29V (Vdet1_0)
    p_lvd_feature->negation_delay_clock    = CGC_CLOCK_MOCO;  // MOCO required for LVD signal negation delay after reset
}

void R_BSP_FeatureAdcGet(bsp_feature_adc_t * p_adc_feature)
{
    p_adc_feature->has_sample_hold_reg = 0U;
    p_adc_feature->group_b_sensors_allowed = 0U;
    p_adc_feature->sensors_exclusive = 1U;
    p_adc_feature->sensor_min_sampling_time = 5000U;
    p_adc_feature->clock_source = CGC_SYSTEM_CLOCKS_PCLKD;
    p_adc_feature->tsn_calibration_available = 1U;
    p_adc_feature->tsn_control_available = 0U;
    p_adc_feature->tsn_slope = -3650;
    p_adc_feature->addition_supported = 1U;
    p_adc_feature->calibration_reg_available = 0U;
    p_adc_feature->reference_voltage = 0U;
}

void R_BSP_FeatureCanGet(bsp_feature_can_t * p_can_feature)
{
    p_can_feature->mclock_only = 1U;
    p_can_feature->check_pclkb_ratio = 1U;
    p_can_feature->clock = CGC_SYSTEM_CLOCKS_ICLK;
}

void R_BSP_FeatureDacGet(bsp_feature_dac_t * p_dac_feature)
{
    p_dac_feature->has_davrefcr = 1U;
    p_dac_feature->has_chargepump = 0U;
}

void R_BSP_FeatureFlashLpGet(bsp_feature_flash_lp * p_flash_lp_feature)
{
    p_flash_lp_feature->flash_clock_src = (uint8_t)CGC_SYSTEM_CLOCKS_ICLK; // S124 Flash uses ICLK
    /** S124 uses 1 macro of 128K and single access for Code Flash. It can therefore access 128K as a single macro
     *  and it's Code Flash memory is effectively organized as a single macro of 128K, yielding a total of 128K
     *  Code Flash.
     */
    p_flash_lp_feature->flash_cf_macros = 1U;     // S124 has 1 code flash HW macro
    p_flash_lp_feature->cf_macro_size = 0x20000U; // S124 uses single access and 1 Code Flash macro of 128K for a total of 128K
}

void R_BSP_FeatureCtsuGet(bsp_feature_ctsu_t * p_ctsu_feature)
{
    p_ctsu_feature->ctsucr0_mask = 0x17U;
    p_ctsu_feature->ctsucr1_mask = 0xFFU;
    p_ctsu_feature->ctsumch0_mask = 0x3FU;
    p_ctsu_feature->ctsumch1_mask = 0x3FU;
    p_ctsu_feature->ctsuchac_register_count = 4U;
    p_ctsu_feature->ctsuchtrc_register_count = 4U;
}

void R_BSP_FeatureIoportGet(bsp_feature_ioport_t * p_ioport_feature)
{
    p_ioport_feature->has_ethernet = 0U;
    p_ioport_feature->has_vbatt_pins = 0U;
}

void R_BSP_FeatureCgcGet(bsp_feature_cgc_t const ** pp_cgc_feature)
{
    *pp_cgc_feature = &g_cgc_feature;
}

void R_BSP_FeatureLPMV2Get(bsp_feature_lpmv2_t * p_lpmv2_feature)
{
   p_lpmv2_feature->has_dssby = 0U;
}

void R_BSP_FeatureRIICGet(bsp_feature_riic_t * p_riic_feature)
{
    p_riic_feature->riic_std_fast_rise_time = 40U;  ///< Initialize the input rise time for standard and fast mode
    p_riic_feature->riic_fastplus_rise_time = 70U; ///< Initialize the input rise time for fastplus mode
}

#endif
