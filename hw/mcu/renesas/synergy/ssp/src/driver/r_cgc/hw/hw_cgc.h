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
 * File Name    : hw_cgc.h
 * Description  : Header file for hw_cgc.c
 **********************************************************************************************************************/
#ifndef HW_CGC_H
#define HW_CGC_H

#include "bsp_cfg.h"
#include "bsp_clock_cfg.h"
#include "hw_cgc_private.h"
#include "../r_cgc_private.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER


/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#if BSP_FEATURE_HAS_CGC_PLL
#define CGC_CLOCK_NUM_CLOCKS    ((uint8_t) CGC_CLOCK_PLL + 1U)
#else
#define CGC_CLOCK_NUM_CLOCKS    ((uint8_t) CGC_CLOCK_SUBCLOCK + 1U)
#endif

/** PRC0 mask */
#define CGC_PRC0_MASK             ((uint16_t) 0x0001)
/** PRC1 mask */
#define CGC_PRC1_MASK             ((uint16_t) 0x0002)
/** Key code for writing PRCR register. */
#define CGC_PRCR_KEY              ((uint16_t) 0xA500)
#define CGC_PLL_MAIN_OSC          (0x00U)            ///< PLL clock source register value for the main oscillator
#define CGC_PLL_HOCO              (0x01U)            ///< PLL clock source register value for the HOCO

#define CGC_LOCO_FREQ             (32768U)           ///< LOCO frequency is fixed at 32768 Hz
#define CGC_MOCO_FREQ             (8000000U)         ///< MOCO frequency is fixed at 8 MHz
#define CGC_SUBCLOCK_FREQ         (32768U)           ///< Subclock frequency is 32768 Hz
#define CGC_PLL_FREQ              (0U)               ///< The PLL frequency must be calculated.
#define CGC_IWDT_FREQ             (15000U)           ///< The IWDT frequency is fixed at 15 kHz
#define CGC_S124_LOW_V_MODE_FREQ  (4000000U)         ///< Max ICLK frequency while in Low Voltage Mode for S124

#define CGC_MAX_ZERO_WAIT_FREQ    (32000000U)
#define CGC_MAX_MIDDLE_SPEED_FREQ (12000000U)        ///< Maximum frequency for Middle Speed mode

#define    CGC_CLOCKOUT_MAX       (CGC_CLOCK_SUBCLOCK)     ///< Highest enum value for CLKOUT clock source

#define    CGC_SYSTEMS_CLOCKS_MAX (CGC_SYSTEM_CLOCKS_ICLK) ///< Highest enum value for system clock

#define CGC_HW_MAX_REGISTER_WAIT_COUNT   (0xFFFFU)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

typedef union
{
    __IO uint32_t  sckdivcr_w; /*!< (@ 0x4001E020) System clock Division control register
                                *                */
    /*LDRA_INSPECTED 381 S Anonymous structures and unions are allowed in SSP code. */
    struct
    {
        __IO uint32_t  pckd : 3; /*!< [0..2] Peripheral Module Clock D (PCLKD) Select
                                  *                      */
        uint32_t            : 1;
        __IO uint32_t  pckc : 3; /*!< [4..6] Peripheral Module Clock C (PCLKC) Select
                                  *                      */
        uint32_t            : 1;
        __IO uint32_t  pckb : 3; /*!< [8..10] Peripheral Module Clock B (PCLKB) Select
                                  *                     */
        uint32_t            : 1;
        __IO uint32_t  pcka : 3; /*!< [12..14] Peripheral Module Clock A (PCLKA) Select
                                  *                    */
        uint32_t            : 1;
        __IO uint32_t  bck  : 3; /*!< [16..18] External Bus Clock (BCLK) Select
                                  *                            */
        uint32_t            : 5;
        __IO uint32_t  ick  : 3; /*!< [24..26] System Clock (ICLK) Select
                                  *                                  */
        uint32_t            : 1;
        __IO uint32_t  fck  : 3; /*!< [28..30] Flash IF Clock (FCLK) Select
                                  *                                */
    }  sckdivcr_b;               /*!< [31] BitSize
                                  *                                                         */
} sckdivcr_clone_t;


/**********************************************************************************************************************
* Function Prototypes
 **********************************************************************************************************************/
/*******************************************************************************************************************//**
@addtogroup CGC

@{
 **********************************************************************************************************************/

/**********************************************************************************************************************
* Inline Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief      This function places the MCU in High Speed Mode
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     None
 **********************************************************************************************************************/
__STATIC_INLINE void HW_CGC_SetHighSpeedMode (R_SYSTEM_Type * p_system_reg)
{
    r_cgc_operating_hw_modeset(p_system_reg, CGC_HIGH_SPEED_MODE);
}

/*******************************************************************************************************************//**
 * @brief      This function places the MCU in Middle Speed Mode
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     None
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_SetMiddleSpeedMode (R_SYSTEM_Type * p_system_reg)
{
    r_cgc_operating_hw_modeset(p_system_reg, CGC_MIDDLE_SPEED_MODE);
}

/*******************************************************************************************************************//**
 * @brief      This function places the MCU in Low Voltage Mode
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     None
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_SetLowVoltageMode (R_SYSTEM_Type * p_system_reg)
{
    r_cgc_operating_hw_modeset(p_system_reg, CGC_LOW_VOLTAGE_MODE);
}

/*******************************************************************************************************************//**
 * @brief      This function places the MCU in Low Speed Mode
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     None
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_SetLowSpeedMode (R_SYSTEM_Type * p_system_reg)
{
    r_cgc_operating_hw_modeset(p_system_reg, CGC_LOW_SPEED_MODE);
}

/*******************************************************************************************************************//**
 * @brief      This function places the MCU in Sub-Osc Speed Mode
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     None
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_SetSubOscSpeedMode (R_SYSTEM_Type * p_system_reg)
{
    r_cgc_operating_hw_modeset(p_system_reg, CGC_SUBOSC_SPEED_MODE);
}


#if BSP_FEATURE_HAS_CGC_LCD_CLK
/*******************************************************************************************************************//**
 * @brief      This function configures the Segment LCD clock
 * @param[in]  p_system_reg pointer to system register structure
 * @param[in]  clock is the LCD clock source
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_LCDClockCfg (R_SYSTEM_Type * p_system_reg, uint8_t clock)
{
    p_system_reg->SLCDSCKCR_b.LCDSCKSEL = clock & 0xFU;
}

/*******************************************************************************************************************//**
 * @brief      This function returns the Segment LCD clock configuration
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     LCD clock configuration
 **********************************************************************************************************************/

__STATIC_INLINE uint8_t HW_CGC_LCDClockCfgGet (R_SYSTEM_Type * p_system_reg)
{
    return p_system_reg->SLCDSCKCR_b.LCDSCKSEL;
}

/*******************************************************************************************************************//**
 * @brief      This function enables LCD Clock Out
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_LCDClockEnable (R_SYSTEM_Type * p_system_reg)
{
    p_system_reg->SLCDSCKCR_b.LCDSCKEN = 1U;
}

/*******************************************************************************************************************//**
 * @brief      This function disables LCD Clock Out
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_LCDClockDisable (R_SYSTEM_Type * p_system_reg)
{
    p_system_reg->SLCDSCKCR_b.LCDSCKEN = 0U;
}

/*******************************************************************************************************************//**
 * @brief      This function queries LCD clock to see if it is enabled.
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     true if enabled, false if disabled
 **********************************************************************************************************************/

__STATIC_INLINE bool HW_CGC_LCDClockIsEnabled (R_SYSTEM_Type * p_system_reg)
{
    if (p_system_reg->SLCDSCKCR_b.LCDSCKEN == 1U)
    {
        return true;
    }
    else
    {
        return false;
    }
}
#endif /* #if BSP_FEATURE_HAS_CGC_LCD_CLK */

#if BSP_FEATURE_HAS_CGC_SDADC_CLK
/*******************************************************************************************************************//**
 * @brief      This function configures the SDADC clock
 * @param[in]  p_system_reg pointer to system register structure
 * @param[in]  clock is the SDADC clock source
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_SDADCClockCfg (R_SYSTEM_Type * p_system_reg, uint8_t clock)
{
    if (clock == CGC_CLOCK_HOCO)
    {
        p_system_reg->SDADCCKCR_b.SDADCCKSEL = 1U;
    }
    else
    {
        p_system_reg->SDADCCKCR_b.SDADCCKSEL = 0U;
    }
}

/*******************************************************************************************************************//**
 * @brief      This function returns the SDADC clock configuration
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     SDADC clock configuration
 **********************************************************************************************************************/

__STATIC_INLINE uint8_t HW_CGC_SDADCClockCfgGet (R_SYSTEM_Type * p_system_reg)
{
    return p_system_reg->SDADCCKCR_b.SDADCCKSEL;
}

/*******************************************************************************************************************//**
 * @brief      This function enables SDADC Clock Out
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_SDADCClockEnable (R_SYSTEM_Type * p_system_reg)
{
    p_system_reg->SDADCCKCR_b.SDADCCKEN = 1U;
}

/*******************************************************************************************************************//**
 * @brief      This function disables SDADC Clock Out
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_SDADCClockDisable (R_SYSTEM_Type * p_system_reg)
{
    p_system_reg->SDADCCKCR_b.SDADCCKEN = 0U;
}

/*******************************************************************************************************************//**
 * @brief      This function queries SDADC clock to see if it is enabled.
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     true if enabled, false if disabled
 **********************************************************************************************************************/

__STATIC_INLINE bool HW_CGC_SDADCClockIsEnabled (R_SYSTEM_Type * p_system_reg)
{
    if (p_system_reg->SDADCCKCR_b.SDADCCKEN == 1U)
    {
        return true;
    }
    else
    {
        return false;
    }
}
#endif /* BSP_FEATURE_HAS_CGC_SDADC_CLK */


/*******************************************************************************************************************//**
 * @brief      This function locks CGC registers
 * @retval     none
 **********************************************************************************************************************/
__STATIC_INLINE void HW_CGC_HardwareLock (void)
{
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_CGC);
}

/*******************************************************************************************************************//**
 * @brief      This function unlocks CGC registers
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_HardwareUnLock (void)
{
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_CGC);
}

/*******************************************************************************************************************//**
 * @brief      This function locks CGC registers
 * @retval     none
 **********************************************************************************************************************/
__STATIC_INLINE void HW_CGC_LPMHardwareLock (void)
{
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_OM_LPC_BATT);
}

/*******************************************************************************************************************//**
 * @brief      This function unlocks CGC registers
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_LPMHardwareUnLock (void)
{
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_OM_LPC_BATT);
}


/*******************************************************************************************************************//**
 * @brief      This function checks that the clock is available in the hardware
 * @param[in]  clock Check clock to see if it is valid
 * @retval     true if clock available, false if not
 **********************************************************************************************************************/

__STATIC_INLINE bool HW_CGC_ClockSourceValidCheck (cgc_clock_t clock)
{
    return (clock < CGC_CLOCK_NUM_CLOCKS);
}

/*******************************************************************************************************************//**
 * @brief      This function returns the system clock source
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     the enum of the current system clock
 **********************************************************************************************************************/

__STATIC_INLINE cgc_clock_t HW_CGC_ClockSourceGet (R_SYSTEM_Type * p_system_reg)
{
    /* Get the system clock source */
    return (cgc_clock_t) p_system_reg->SCKSCR_b.CKSEL;
}


/*******************************************************************************************************************//**
 * @brief      This function returns the status of the stop detection function
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     true if detected, false if not detected
 **********************************************************************************************************************/
__STATIC_INLINE bool HW_CGC_OscStopDetectEnabledGet (R_SYSTEM_Type * p_system_reg)
{
    if (p_system_reg->OSTDCR_b.OSTDE == 1U)          // is stop detection enabled?
    {
        return true;                       // Function enabled.
    }
    return false;                          // Function not enabled.

}
/*******************************************************************************************************************//**
 * @brief      This function returns the status of the stop detection function
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     true if detected, false if not detected
 **********************************************************************************************************************/

__STATIC_INLINE bool HW_CGC_OscStopDetectGet (R_SYSTEM_Type * p_system_reg)
{
    /* oscillator stop detected */
    if (p_system_reg->OSTDSR_b.OSTDF == 1U)
    {
        return true;            // stop detected
    }

    return false;               // no stop detected
}

#if BSP_FEATURE_HAS_CGC_EXTERNAL_BUS
/*******************************************************************************************************************//**
 * @brief      This function configures the BusClock Out divider
 * @param[in]  p_system_reg pointer to system register structure
 * @param[in]  divider is the bus clock out divider
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_BusClockOutCfg (R_SYSTEM_Type * p_system_reg, cgc_bclockout_dividers_t divider)
{
    /*  */
    HW_CGC_HardwareUnLock();
    p_system_reg->BCKCR_b.BCLKDIV = divider;      // set external bus clock output divider
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function enables BusClockOut
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_BusClockOutEnable (R_SYSTEM_Type * p_system_reg)
{
    /*  */
    HW_CGC_HardwareUnLock();
    p_system_reg->EBCKOCR_b.EBCKOEN = 1U;           // enable bus clock output
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function disables BusClockOut
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_BusClockOutDisable (R_SYSTEM_Type * p_system_reg)
{
    /*  */
    HW_CGC_HardwareUnLock();
    p_system_reg->EBCKOCR_b.EBCKOEN = 0U;           // disable bus clock output (fixed high level)
    HW_CGC_HardwareLock();
}
#endif /* BSP_FEATURE_HAS_CGC_EXTERNAL_BUS */

#if BSP_FEATURE_HAS_CGC_SDRAM_CLK
/*******************************************************************************************************************//**
 * @brief      This function enables SDRAM ClockOut
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/
__STATIC_INLINE void HW_CGC_SDRAMClockOutEnable (R_SYSTEM_Type * p_system_reg)
{
    HW_CGC_HardwareUnLock();
    p_system_reg->SDCKOCR_b.SDCKOEN = 1U;          // enable SDRAM output
    HW_CGC_HardwareLock();
}

/*******************************************************************************************************************//**
 * @brief      This function disables SDRAM ClockOut
 * @param[in]  p_system_reg pointer to system register structure
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_SDRAMClockOutDisable (R_SYSTEM_Type * p_system_reg)
{
    HW_CGC_HardwareUnLock();
    p_system_reg->SDCKOCR_b.SDCKOEN = 0U;          // disable SDRAM output (fixed high level)
    HW_CGC_HardwareLock();
}
#endif /* BSP_FEATURE_HAS_CGC_SDRAM_CLK */



#if BSP_FEATURE_HAS_CGC_USB_CLK
/*******************************************************************************************************************//**
 * @brief      This function configures the USB clock divider
 * @param[in]  p_system_reg pointer to system register structure
 * @param[in]  divider is the USB clock divider
 * @retval     none
 **********************************************************************************************************************/

__STATIC_INLINE void HW_CGC_USBClockCfg (R_SYSTEM_Type * p_system_reg, cgc_usb_clock_div_t divider)
{
    HW_CGC_HardwareUnLock();
    p_system_reg->SCKDIVCR2_b.UCK = divider;      // set USB divider
    HW_CGC_HardwareLock();
}
#endif /* BSP_FEATURE_HAS_CGC_USB_CLK */
/*******************************************************************************************************************//**
 * @} (end addtogroup CGC)
 **********************************************************************************************************************/

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER
#endif // ifndef HW_CGC_H
