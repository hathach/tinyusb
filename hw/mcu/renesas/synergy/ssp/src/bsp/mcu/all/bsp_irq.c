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
* File Name    : bsp_irq.c
* Description  : This module configures certain ELC events so that they can trigger NVIC interrupts.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "bsp_api.h"

/** ELC event definitions. */
#include "r_elc_api.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/
/* Used for accessing bitfields in IELSR registers. */
typedef struct
{
    __IO uint32_t  IELS       :  9;               /* [0..8] Event selection to NVIC */
         uint32_t  res0       :  7;
    __IO uint32_t  IR         :  1;               /* [16..16] Interrupt Status Flag */
         uint32_t  res1       :  7;
    __IO uint32_t  DTCE       :  1;               /* [24..24] DTC Activation Enable */
         uint32_t  res2       :  7;
} bsp_prv_ielsr_t;

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/

/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/
#if defined(__GNUC__)
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
extern uint32_t __Vector_Info_End;
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
extern uint32_t __Vector_Info_Start;
/*LDRA_INSPECTED 461 S This rule should not apply. This is not a reference to a variable declared both static and extern. */
ssp_vector_info_t * const gp_vector_information = (ssp_vector_info_t * const) &__Vector_Info_Start;
#endif
#if defined(__ICCARM__)               /* IAR Compiler */
#pragma section="VECT_INFO"
extern ssp_vector_info_t VECT_INFO$$Base;
ssp_vector_info_t * const gp_vector_information = &VECT_INFO$$Base;
#endif

/*LDRA_INSPECTED 27 D This structure must be accessible in user code. It cannot be static. */
uint32_t g_vector_information_size = 0;

/*******************************************************************************************************************//**
 * @addtogroup BSP_MCU_IRQ
 *
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Clear the interrupt status flag (IR) for a given interrupt. When an interrupt is triggered the IR bit
 *        is set. If it is not cleared in the ISR then the interrupt will trigger again immediately.
 *
 * @param[in] irq Interrupt for which to clear the IR bit. Note that the enums listed for IRQn_Type are
 * only those for the Cortex Processor Exceptions Numbers. In prior releases this list contained all of the
 * interrupts enabled for the specfic MCU but enabled interrupts are now configured using the SSP_VECTOR_DEFINE macro.
 *
 * @note This does not work for system exceptions where the IRQn_Type value is < 0.
 **********************************************************************************************************************/
void R_BSP_IrqStatusClear (IRQn_Type irq)
{
    /* This does not work for system exceptions where the IRQn_Type value is < 0 */
    if (((int32_t)irq) >= 0)
    {
        /* Clear the IR bit in the selected IELSR register. */
        ((bsp_prv_ielsr_t *)&R_ICU->IELSRn)[(uint32_t)irq].IR = 0U;
    }
}



/*******************************************************************************************************************//**
 * @brief Using the vector table information section that has been built by the linker and placed into ROM in the
 * .vector_info. section, this function will initialize the ICU so that configured ELC events will trigger interrupts
 * in the NVIC.
 *
 **********************************************************************************************************************/
void bsp_irq_cfg (void)
{
    uint32_t * base_addr = (uint32_t *)&R_ICU->IELSRn;


#if defined(__GNUC__)
    g_vector_information_size = ((uint32_t) &__Vector_Info_End - (uint32_t) &__Vector_Info_Start) / sizeof(ssp_vector_info_t);
#endif
#if defined(__ICCARM__)               /* IAR Compiler */
    g_vector_information_size = __section_size("VECT_INFO") / sizeof(ssp_vector_info_t);
#endif

    for (uint32_t i = 0U; i < g_vector_information_size; i++)
    {
        *(base_addr + i) = gp_vector_information[i].event_number;
    }
}
/** @} (end addtogroup BSP_MCU_IRQ) */
