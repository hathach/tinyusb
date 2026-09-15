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
* File Name    : bsp_feature.h
* Description  : Provides feature information that varies by MCU and is not available in the factory flash.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_COMMON
 * @defgroup BSP_FEATURE Module specific feature overrides
 *
 * This group contains lookup functions that provide MCU specific feature information that is not available in the
 * factory flash.
 *
 * @{
 **********************************************************************************************************************/

#ifndef BSP_FEATURE_H_
#define BSP_FEATURE_H_

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include <stdint.h>

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/
/** SCI MCU specific features. */
typedef struct st_bsp_feature_sci
{
    uint8_t     clock;   ///< Which clock the SCI is connected to
} bsp_feature_sci_t;

/** RSPI MCU specific features. */
typedef struct st_bsp_feature_rspi
{
    uint8_t     clock;   ///< Which clock the RSPI is connected to
    uint8_t     has_ssl_level_keep        : 1;
    uint8_t     swap                      : 1;
} bsp_feature_rspi_t;

/** LVD MCU specific features. */
typedef struct st_bsp_feature_lvd
{
    uint8_t     monitor_1_low_threshold; ///< Monitor 1 lowest valid voltage threshold
    uint8_t     monitor_1_hi_threshold;  ///< Monitor 1 highest valid voltage threshold
    uint8_t     monitor_2_low_threshold; ///< Monitor 2 lowest valid voltage threshold
    uint8_t     monitor_2_hi_threshold;  ///< Monitor 2 highest valid voltage threshold
    uint32_t    has_digital_filter : 1;  ///< Whether or not LVD has a digital filter
    uint32_t    negation_delay_clock;    ///< Clock required for LVD signal negation delay after reset
} bsp_feature_lvd_t;

/** ACMPHS MCU specific features. */
typedef struct st_bsp_feature_acmphs
{
    uint32_t     min_wait_time_us;        ///< Minimum stabilization wait time in microseconds
} bsp_feature_acmphs_t;

/** ADC MCU specific features. */
typedef struct st_bsp_feature_adc
{
    uint8_t     has_sample_hold_reg       : 1;   ///< Whether or not sample and hold registers are present
    uint8_t     group_b_sensors_allowed   : 1;   ///< Whether or not sensors are allowed on group b
    uint8_t     sensors_exclusive         : 1;   ///< Whether or not sensors can be used with other sensors/channels
    uint8_t     tsn_calibration_available : 1;   ///< Identify if the TSN calibration data is available
    uint8_t     tsn_control_available     : 1;   ///< Identify if the TSN control register is available
    int16_t     tsn_slope;                     ///< TSN slope in micro-volts/°C
    uint32_t    sensor_min_sampling_time;      ///< The minimum sampling time required by the on-chip temperature and voltage sensor in nsec
    uint32_t    clock_source;                  ///< The conversion clock used by the ADC peripheral
    uint8_t     addition_supported        : 1; ///< Whether addition is supported or not in this MCU
    uint8_t     calibration_reg_available : 1; ///< Whether CALEXE register is available
    uint8_t     reference_voltage         : 1 ;///< Whether external or internal ref voltage
} bsp_feature_adc_t;

/** CAN MCU specific features. */
typedef struct st_bsp_feature_can
{
    uint8_t     mclock_only             : 1;   ///< Whether or not MCLK is the only valid clock
    uint8_t     check_pclkb_ratio       : 1;   ///< Whether clock:PCLKB must be 2:1
    uint8_t     clock;                         ///< Which clock to compare PCLKB to
} bsp_feature_can_t;


/** DAC MCU specific features. */
typedef struct st_bsp_feature_dac
{
    uint8_t     has_davrefcr            : 1;   ///< Whether or not DAC has DAVREFCR register
    uint8_t     has_chargepump          : 1;   ///< Whether or not DAC has DAPC register
} bsp_feature_dac_t;


/** FLASH LP MCU specific features. */
typedef struct st_bsp_feature_flash_lp
{
    uint8_t     flash_clock_src;              ///< Source clock for Flash (ie. FCLK or ICLK)
    uint8_t     flash_cf_macros;              ///< Number of implemented code flash hardware macros
    uint32_t    cf_macro_size;                ///< The size of the implemented Code Flash macro
} bsp_feature_flash_lp;

/** FLASH HP MCU specific features. */
typedef struct st_bsp_feature_flash_hp
{
    uint8_t     cf_block_size_write;          ///< Code Flash Block size write.
} bsp_feature_flash_hp;


/** CTSU MCU specific features. */
typedef struct st_bsp_feature_ctsu
{
    uint8_t ctsucr0_mask;                      ///< Mask of valid bits in CTSUCR0
    uint8_t ctsucr1_mask;                      ///< Mask of valid bits in CTSUCR1
    uint8_t ctsumch0_mask;                     ///< Mask of valid bits in CTSUMCH0
    uint8_t ctsumch1_mask;                     ///< Mask of valid bits in CTSUMCH1
    uint8_t ctsuchac_register_count;           ///< Number of CTSUCHAC registers
    uint8_t ctsuchtrc_register_count;          ///< Number of CTSUCHTRC registers
} bsp_feature_ctsu_t;

/** I/O port MCU specific features. */
typedef struct st_bsp_feature_ioport
{
    uint8_t     has_ethernet            : 1;   ///< Whether or not MCU has Ethernet port configurations
    uint8_t     has_vbatt_pins          : 1;   ///< Whether or not MCU has pins on vbatt domain
} bsp_feature_ioport_t;

/** CGC MCU specific features. */
typedef struct st_bsp_feature_cgc
{
    uint32_t    high_speed_freq_hz;            ///< Frequency above which high speed mode must be used
    uint32_t    middle_speed_max_freq_hz;      ///< Max frequency for middle speed, 0 indicates not available
    uint32_t    low_speed_max_freq_hz;         ///< Max frequency for low speed, 0 indicates not available
    uint32_t    low_voltage_max_freq_hz;       ///< Max frequency for low voltage, 0 indicates not available
    uint32_t    low_speed_pclk_div_min;        ///< Minimum divisor for peripheral clocks when using oscillator stop detect
    uint32_t    low_voltage_pclk_div_min;       ///< Minimum divisor for peripheral clocks when using oscillator stop detect
    uint32_t    hoco_freq_hz;                  ///< HOCO frequency
    uint32_t    main_osc_freq_hz;              ///< Main oscillator frequency
    uint8_t     modrv_mask;                    ///< Mask for MODRV in MOMCR
    uint8_t     modrv_shift;                   ///< Offset of lowest bit of MODRV in MOMCR
    uint8_t     sodrv_mask;                    ///< Mask for SODRV in SOMCR
    uint8_t     sodrv_shift;                   ///< Offset of lowest bit of SODRV in SOMCR
    uint8_t     pll_div_max;                   ///< Maximum PLL divisor
    uint8_t     pll_mul_min;                   ///< Minimum PLL multiplier
    uint8_t     pll_mul_max;                   ///< Maximum PLL multiplier
    uint8_t     mainclock_drive;               ///< Main clock drive capacity
    uint32_t    iclk_div                : 4;   ///< ICLK divisor
    uint32_t    pllccr_type             : 2;   ///< 0: No PLL, 1: PLLCCR, 2: PLLCCR2
    uint32_t    pll_src_configurable    : 1;   ///< Whether or not PLL clock source is configurable
    uint32_t    has_subosc_speed        : 1;   ///< Whether or not MCU has subosc speed mode
    uint32_t    has_lcd_clock           : 1;   ///< Whether or not MCU has LCD clock
    uint32_t    has_sdram_clock         : 1;   ///< Whether or not MCU has SDRAM clock
    uint32_t    has_usb_clock_div       : 1;   ///< Whether or not MCU has USB clock divisor
    uint32_t    has_pclka               : 1;   ///< Whether or not MCU has PCLKA clock
    uint32_t    has_pclkb               : 1;   ///< Whether or not MCU has PCLKB clock
    uint32_t    has_pclkc               : 1;   ///< Whether or not MCU has PCLKC clock
    uint32_t    has_pclkd               : 1;   ///< Whether or not MCU has PCLKD clock
    uint32_t    has_fclk                : 1;   ///< Whether or not MCU has FCLK clock
    uint32_t    has_bclk                : 1;   ///< Whether or not MCU has BCLK clock
    uint32_t    has_sdadc_clock         : 1;   ///< Whether or not MCU has SDADC clock
    uint32_t    set_bck_with_pckb       : 1;   ///< Whether or not BCK bits should be set with PCKB bits
} bsp_feature_cgc_t;

/** OPAMP MCU specific features. */
typedef struct st_bsp_feature_opamp
{
    uint16_t    min_wait_time_lp_us;           ///< Minimum wait time in low power mode
    uint16_t    min_wait_time_ms_us;           ///< Minimum wait time in middle speed mode
    uint16_t    min_wait_time_hs_us;           ///< Minimum wait time in high speed mode
} bsp_feature_opamp_t;

/** SDHI MCU specific features. */
typedef struct st_bsp_feature_sdhi
{
    uint32_t    has_card_detection      : 1;   ///< Whether or not MCU has card detection
    uint32_t    supports_8_bit_mmc      : 1;   ///< Whether or not MCU supports 8-bit MMC
    uint32_t    max_clock_frequency;           ///< Maximum clock rate supported by the peripheral
} bsp_feature_sdhi_t;

/** SSI MCU specific features. */
typedef struct st_bsp_feature_ssi
{
    uint8_t     fifo_num_stages;               ///< Number of FIFO stages on this MCU
} bsp_feature_ssi_t;

/** DMAC MCU specific features. */
typedef struct st_bsp_feature_icu
{
    uint8_t     has_ir_flag            : 1;   ///< Whether or not MCU has IR flag in DELSRn register
} bsp_feature_icu_t;

/** LPMV2 MCU specific features. */
typedef struct st_bsp_feature_lpmv2
{
    uint8_t     has_dssby              : 1;   ///< Whether or not MCU has Deep software standby mode
}bsp_feature_lpmv2_t;

/** RIIC MCU specific features. */
typedef struct st_bsp_feature_riic
{
    uint32_t    riic_std_fast_rise_time;      ///< Input rise time for the riic peripheral for standard and fast mode
    uint32_t    riic_fastplus_rise_time;      ///< Input rise time for the riic peripheral for the fast plus mode
} bsp_feature_riic_t;

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
/*******************************************************************************************************************//**
 * Get MCU specific SCI features.
 * @param[out] p_sci_feature  Pointer to structure to store SCI features.
 **********************************************************************************************************************/
void R_BSP_FeatureSciGet(bsp_feature_sci_t * p_sci_feature);

/*******************************************************************************************************************//**
 * Get MCU specific RSPI features.
 * @param[out] p_rspi_feature  Pointer to structure to store RSPI features.
 **********************************************************************************************************************/
void R_BSP_FeatureRspiGet(bsp_feature_rspi_t * p_rspi_feature);

/*******************************************************************************************************************//**
 * Get MCU specific LVD features.
 * @param[out] p_lvd_feature  Pointer to structure to store LVD features.
 **********************************************************************************************************************/
void R_BSP_FeatureLvdGet(bsp_feature_lvd_t * p_lvd_feature);

/*******************************************************************************************************************//**
 * Get MCU specific ACMPHS features
 * @param[out] p_acmphs_feature  Pointer to structure to store ACMPHS features.
 **********************************************************************************************************************/
void R_BSP_FeatureAcmphsGet(bsp_feature_acmphs_t * p_acmphs_feature);

/*******************************************************************************************************************//**
 * Get MCU specific ADC features
 * @param[out] p_adc_feature  Pointer to structure to store ADC features.
 **********************************************************************************************************************/
void R_BSP_FeatureAdcGet(bsp_feature_adc_t * p_adc_feature);

/*******************************************************************************************************************//**
 * Get MCU specific CTSU features
 * @param[out] p_ctsu_feature  Pointer to structure to store CTSU features.
 **********************************************************************************************************************/
void R_BSP_FeatureCtsuGet(bsp_feature_ctsu_t * p_ctsu_feature);

/*******************************************************************************************************************//**
 * Get MCU specific I/O port features
 * @param[out] p_ioport_feature  Pointer to structure to store I/O port features.
 **********************************************************************************************************************/
void R_BSP_FeatureIoportGet(bsp_feature_ioport_t * p_ioport_feature);

/*******************************************************************************************************************//**
 * Get MCU specific CGC features
 * @param[out] pp_cgc_feature  Pointer to structure to store pointer to CGC features.
 **********************************************************************************************************************/
void R_BSP_FeatureCgcGet(bsp_feature_cgc_t const ** pp_cgc_feature);

/*******************************************************************************************************************//**
 * Get MCU specific CAN features
 * @param[out] p_can_feature  Pointer to structure to store CAN features.
 **********************************************************************************************************************/
void R_BSP_FeatureCanGet(bsp_feature_can_t * p_can_feature);

/*******************************************************************************************************************//**
 * Get MCU specific DAC features
 * @param[out] p_dac_feature  Pointer to structure to store DAC features.
 **********************************************************************************************************************/
void R_BSP_FeatureDacGet(bsp_feature_dac_t * p_dac_feature);

/*******************************************************************************************************************//**
 * Get MCU specific FLASH LP features
 * @param[out] p_flash_lp_feature  Pointer to structure to store Flash LP features.
 **********************************************************************************************************************/
void R_BSP_FeatureFlashLpGet(bsp_feature_flash_lp * p_flash_lp_feature);

/*******************************************************************************************************************//**
 * Get MCU specific OPAMP features
 * @param[out] p_opamp_feature  Pointer to structure to store OPAMP features.
 **********************************************************************************************************************/
void R_BSP_FeatureOpampGet(bsp_feature_opamp_t * const p_opamp_feature);

/*******************************************************************************************************************//**
 * Get MCU specific SDHI features
 * @param[out] p_sdhi_feature  Pointer to structure to store SDHI features.
 **********************************************************************************************************************/
void R_BSP_FeatureSdhiGet(bsp_feature_sdhi_t * p_sdhi_feature);

/*******************************************************************************************************************//**
 * Get MCU specific SSI features
 * @param[out] p_ssi_feature  Pointer to structure to store SSI features.
 **********************************************************************************************************************/
void R_BSP_FeatureSsiGet(bsp_feature_ssi_t * p_ssi_feature);

/*******************************************************************************************************************//**
 * Get MCU specific DMAC features
 * @param[out] p_icu_feature  Pointer to structure to store DMAC features.
 **********************************************************************************************************************/
void R_BSP_FeatureICUGet(bsp_feature_icu_t * p_icu_feature);

/*******************************************************************************************************************//**
 * Get MCU specific LPMV2 features
 * @param[out] p_lpmv2_feature  Pointer to structure to store LPMV2 features.
 **********************************************************************************************************************/
void R_BSP_FeatureLPMV2Get(bsp_feature_lpmv2_t * p_lpmv2_feature);

/*******************************************************************************************************************//**
 * Get MCU specific RIIC features
 * @param[out] p_riic_feature  Pointer to structure to store RIIC features.
 **********************************************************************************************************************/
void R_BSP_FeatureRIICGet(bsp_feature_riic_t * p_riic_feature);

#endif // BSP_FEATURE_H_

/** @} (end defgroup BSP_FEATURE) */

