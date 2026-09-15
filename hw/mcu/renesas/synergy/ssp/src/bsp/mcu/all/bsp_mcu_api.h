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
* File Name    : bsp_mcu_api.h
* Description  : BSP API function prototypes.
***********************************************************************************************************************/


#ifndef BSP_MCU_API_H_
#define BSP_MCU_API_H_

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

void        R_BSP_RegisterProtectEnable(bsp_reg_protect_t regs_to_protect);
void        R_BSP_RegisterProtectDisable(bsp_reg_protect_t regs_to_protect);
void        R_BSP_SoftwareLockInit(bsp_lock_t * p_lock);
ssp_err_t   R_BSP_SoftwareLock(bsp_lock_t * p_lock);
void        R_BSP_SoftwareUnlock(bsp_lock_t * p_lock);
ssp_err_t   R_BSP_HardwareLock(ssp_feature_t const * const p_feature);
void        R_BSP_HardwareUnlock(ssp_feature_t const * const p_feature);
ssp_err_t   R_BSP_GroupIrqWrite(bsp_grp_irq_t irq,  void (* p_callback)(bsp_grp_irq_t irq));
void        R_BSP_IrqStatusClear(IRQn_Type irq);
ssp_err_t   R_BSP_LedsGet(bsp_leds_t * p_leds);
void        R_BSP_SoftwareDelay(uint32_t delay, bsp_delay_units_t units);
ssp_err_t   R_BSP_ModuleStop(ssp_feature_t const * const feature);
ssp_err_t   R_BSP_ModuleStopAlways(ssp_feature_t const * const p_feature);
ssp_err_t   R_BSP_ModuleStart(ssp_feature_t const * const feature);
ssp_err_t   R_BSP_ModuleStateGet(ssp_feature_t const * const p_feature, bool * const p_stop);
ssp_err_t   R_BSP_VersionGet(ssp_version_t * p_version);
ssp_err_t   R_BSP_CacheOff(bsp_cache_state_t * p_state);
ssp_err_t   R_BSP_CacheSet(bsp_cache_state_t state);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* BSP_MCU_API_H_ */
