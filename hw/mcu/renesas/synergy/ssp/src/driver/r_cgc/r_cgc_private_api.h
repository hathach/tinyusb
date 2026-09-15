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

#ifndef R_CGC_R_CGC_PRIVATE_API_H_
#define R_CGC_R_CGC_PRIVATE_API_H_

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Private Instance API Functions. DO NOT USE! Use functions through Interface API structure instead.
 **********************************************************************************************************************/
ssp_err_t R_CGC_Init (void);
ssp_err_t R_CGC_ClocksCfg(cgc_clocks_cfg_t const * const p_clock_cfg);
ssp_err_t R_CGC_ClockStart (cgc_clock_t clock_source, cgc_clock_cfg_t * pclock_cfg);
ssp_err_t R_CGC_ClockStop (cgc_clock_t clock_source);
ssp_err_t R_CGC_SystemClockSet (cgc_clock_t clock_source, cgc_system_clock_cfg_t const * const pclock_cfg);
ssp_err_t R_CGC_SystemClockGet (cgc_clock_t * clock_source, cgc_system_clock_cfg_t * pSet_clock_cfg);
ssp_err_t R_CGC_SystemClockFreqGet (cgc_system_clocks_t clock, uint32_t * freq_hz);
ssp_err_t R_CGC_ClockCheck (cgc_clock_t clock_source);
ssp_err_t R_CGC_OscStopDetect (void (* pcallback) (cgc_callback_args_t * p_args), bool enable);
ssp_err_t R_CGC_OscStopStatusClear (void);
ssp_err_t R_CGC_BusClockOutCfg (cgc_bclockout_dividers_t divider);
ssp_err_t R_CGC_BusClockOutEnable (void);
ssp_err_t R_CGC_BusClockOutDisable (void);
ssp_err_t R_CGC_ClockOutCfg (cgc_clock_t clock, cgc_clockout_dividers_t divider);
ssp_err_t R_CGC_ClockOutEnable (void);
ssp_err_t R_CGC_ClockOutDisable (void);
ssp_err_t R_CGC_LCDClockCfg (cgc_clock_t clock);
ssp_err_t R_CGC_LCDClockEnable (void);
ssp_err_t R_CGC_LCDClockDisable (void);
ssp_err_t R_CGC_SDADCClockCfg (cgc_clock_t clock);
ssp_err_t R_CGC_SDADCClockEnable (void);
ssp_err_t R_CGC_SDADCClockDisable (void);
ssp_err_t R_CGC_SDRAMClockOutEnable (void);
ssp_err_t R_CGC_SDRAMClockOutDisable (void);
ssp_err_t R_CGC_USBClockCfg (cgc_usb_clock_div_t divider);
ssp_err_t R_CGC_SystickUpdate(uint32_t period_count, cgc_systick_period_units_t units);
ssp_err_t R_CGC_VersionGet (ssp_version_t * version);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_CGC_R_CGC_PRIVATE_API_H_ */
