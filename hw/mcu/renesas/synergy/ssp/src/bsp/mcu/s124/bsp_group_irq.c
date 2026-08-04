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
* File Name    : bsp_group_irq.c
* Description  : This module allows callbacks to be registered for sources inside a grouped interrupt. For example, the
*                NMI pin, oscillation stop detect, and WDT underflow interrupts are all mapped to the NMI exception.
*                Since multiple modules cannot define the same handler, the BSP provides a mechanism to be alerted when
*                a certain exception occurs.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "bsp_api.h"

#if defined(BSP_MCU_GROUP_S124)

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
 
/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/
/** This array holds callback functions. */
static bsp_grp_irq_cb_t g_bsp_group_irq_sources[BSP_GRP_IRQ_TOTAL_ITEMS];

void NMI_Handler(void);

/*******************************************************************************************************************//**
 * @brief      Initialize callback function array.
 **********************************************************************************************************************/
void bsp_group_interrupt_open (void)
{
    uint32_t i;

    for (i = (uint32_t)0; i < BSP_GRP_IRQ_TOTAL_ITEMS; i++)
    {
        g_bsp_group_irq_sources[i] = NULL;
    }
}

/*******************************************************************************************************************//**
 * @brief      Calls the callback function for an interrupt if a callback has been registered.
 *
 * @param[in]   irq         Which interrupt to check and possibly call.
 *
 * @retval SSP_SUCCESS              Callback was called.
 * @retval SSP_ERR_INVALID_ARGUMENT No valid callback has been registered for this interrupt source.
 *
 * @warning This function is called from within an interrupt
 **********************************************************************************************************************/
ssp_err_t bsp_group_irq_call (bsp_grp_irq_t irq)
{
    ssp_err_t err;

    err = SSP_SUCCESS;

    /** Check for valid callback */
    if ((uint32_t)g_bsp_group_irq_sources[irq] == (uint32_t)NULL)
    {
        err = SSP_ERR_INVALID_ARGUMENT;
    }
    else
    {
        /** Callback has been found. Call it. */
        g_bsp_group_irq_sources[irq](irq);
    }

    return err;
}

/*******************************************************************************************************************//**
 * @addtogroup BSP_MCU_GROUP_IRQ
 *
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Register a callback function for supported interrupts. If NULL is passed for the callback argument
 *          then any previously registered callbacks are unregistered.
 *
 * @param[in] irq        Interrupt for which  to register a callback.
 * @param[in] p_callback Pointer to function to call when interrupt occurs.
 *
 * @retval SSP_SUCCESS          Callback registered
 **********************************************************************************************************************/
ssp_err_t R_BSP_GroupIrqWrite (bsp_grp_irq_t irq,  void (* p_callback)(bsp_grp_irq_t irq))
{
    ssp_err_t err;

    err = SSP_SUCCESS;

    /** Check for valid address. */
    if ((uint32_t)p_callback == (uint32_t)NULL)
    {
        /** Callback was NULL. De-register callback. */
        g_bsp_group_irq_sources[irq] = NULL;
    }
    else
    {
        /** Register callback. */
        g_bsp_group_irq_sources[irq] = p_callback;
    }

    return err;
}



/*******************************************************************************************************************//**
 * @brief Non-maskable interrupt handler. This exception is defined by the BSP, unlike other system exceptions,
 *        because there are many sources that map to the NMI exception.
 **********************************************************************************************************************/
void NMI_Handler (void)
{
    /** Determine what is the cause of this interrupt. */
    if (1 == R_ICU->NMISR_b.IWDTST)
    {
        /** IWDT underflow/refresh error interrupt is requested. */
        bsp_group_irq_call(BSP_GRP_IRQ_IWDT_ERROR);

        /** Clear IWDT flag. */
        R_ICU->NMICLR_b.IWDTCLR = 1U;
    }

    if (1 == R_ICU->NMISR_b.WDTST)
    {
        /** WDT underflow/refresh error interrupt is requested. */
        bsp_group_irq_call(BSP_GRP_IRQ_WDT_ERROR);

        /** Clear WDT flag. */
        R_ICU->NMICLR_b.WDTCLR = 1U;
    }

    if (1 == R_ICU->NMISR_b.LVD1ST)
    {
        /** Voltage monitoring 1 interrupt is requested. */
        bsp_group_irq_call(BSP_GRP_IRQ_LVD1);

        /** Clear LVD1 flag. */
        R_ICU->NMICLR_b.LVD1CLR = 1U;
    }

    if (1 == R_ICU->NMISR_b.LVD2ST)
    {
        /** Voltage monitoring 2 interrupt is requested. */
        bsp_group_irq_call(BSP_GRP_IRQ_LVD2);

        /** Clear LVD2 flag. */
        R_ICU->NMICLR_b.LVD2CLR = 1U;
    }

    if (1 == R_ICU->NMISR_b.OSTST)
    {
        /** Oscillation stop detection interrupt is requested. */
        bsp_group_irq_call(BSP_GRP_IRQ_OSC_STOP_DETECT);

        /** Clear oscillation stop detect flag. */
        R_ICU->NMICLR_b.OSTCLR = 1U;
    }

    if (1 == R_ICU->NMISR_b.NMIST)
    {
        /** NMI pin interrupt is requested. */
        bsp_group_irq_call(BSP_GRP_IRQ_NMI_PIN);

        /** Clear NMI pin interrupt flag. */
        R_ICU->NMICLR_b.NMICLR = 1U;
    }

    if (1 == R_ICU->NMISR_b.RPEST)
    {
        /** RAM Parity Error interrupt is requested. */
        bsp_group_irq_call(BSP_GRP_IRQ_RAM_PARITY);

        /** Clear RAM parity error flag. */
        R_ICU->NMICLR_b.RPECLR = 1U;
    }

    if (1 == R_ICU->NMISR_b.RECCST)
    {
        /** RAM ECC Error interrupt is requested. */
        bsp_group_irq_call(BSP_GRP_IRQ_RAM_ECC);

        /** Clear RAM ECC error flag. */
        R_ICU->NMICLR_b.RECCCLR = 1U;
    }
}

#endif

/** @} (end addtogroup BSP_MCU_GROUP_IRQ) */
