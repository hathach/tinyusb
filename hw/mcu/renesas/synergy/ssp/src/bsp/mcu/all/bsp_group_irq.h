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
* File Name    : bsp_group_irq.h
* Description  : This module allows callbacks to be registered for sources inside a grouped interrupt. For example, the
*                NMI pin, oscillation stop detect, and WDT underflow interrupts are all mapped to the NMI exception.
*                Since multiple modules cannot define the same handler, the BSP provides a mechanism to be alerted when
*                a certain exception occurs.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_COMMON
 * @defgroup BSP_MCU_GROUP_IRQ Grouped Interrupt Support
 *
 * Support for grouped interrupts. Grouped interrupts occur when multiple interrupt events trigger the same interrupt
 * vector. When this common vector is triggered the activation source must be discovered. The functions in this file
 * allow users to register a callback function for a single interrupt source in an interrupt group.
 *
 * @{
 **********************************************************************************************************************/

/** @} (end defgroup BSP_MCU_GROUP_IRQ) */

#ifndef BSP_GROUP_IRQ_H_
#define BSP_GROUP_IRQ_H_

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/
/** Which interrupts can have callbacks registered. */
typedef enum e_bsp_grp_irq
{
    BSP_GRP_IRQ_NMI_PIN,                ///< NMI Pin interrupt
    BSP_GRP_IRQ_OSC_STOP_DETECT,        ///< Oscillation stop is detected
    BSP_GRP_IRQ_WDT_ERROR,              ///< WDT underflow/refresh error has occurred
    BSP_GRP_IRQ_IWDT_ERROR,             ///< IWDT underflow/refresh error has occurred
    BSP_GRP_IRQ_LVD1,                   ///< Voltage monitoring 1 interrupt
    BSP_GRP_IRQ_LVD2,                   ///< Voltage monitoring 2 interrupt
    BSP_GRP_IRQ_VBATT,                  ///< VBATT monitor interrupt
    BSP_GRP_IRQ_RAM_PARITY,             ///< RAM Parity Error
    BSP_GRP_IRQ_RAM_ECC,                ///< RAM ECC Error
    BSP_GRP_IRQ_MPU_BUS_SLAVE,          ///< MPU Bus Slave Error
    BSP_GRP_IRQ_MPU_BUS_MASTER,         ///< MPU Bus Master Error
    BSP_GRP_IRQ_MPU_STACK,              ///< MPU Stack Error
    BSP_GRP_IRQ_TOTAL_ITEMS             //   DO NOT MODIFY! This is used for sizing internal callback array.
} bsp_grp_irq_t;

/* Callback type. */
typedef void (* bsp_grp_irq_cb_t)(bsp_grp_irq_t irq);

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/
/* Public functions defined in bsp.h */
void      bsp_group_interrupt_open(void);           // Used internally by BSP
ssp_err_t bsp_group_irq_call(bsp_grp_irq_t irq);    // Used internally by BSP

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* BSP_GROUP_IRQ_H_ */

