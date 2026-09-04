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
* File Name    : bsp_common.h
* Description  : Contains common includes, typedefs, and macros used by the BSP.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_Interface
 * @defgroup BSP_MCU_COMMON Common BSP Code
 * @brief Code common to all BSPs.
 *
 * Implements functions that are common to all BSPs.
 *
 * @{
***********************************************************************************************************************/

#ifndef BSP_COMMON_H_
#define BSP_COMMON_H_

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
/* C99 includes. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
/* Different compiler support. */
#include "../../inc/ssp_common_api.h"
#include "bsp_compiler_support.h"
#include "bsp_cfg.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/** Used to signify that an ELC event is not able to be used as an interrupt. */
#define BSP_IRQ_DISABLED            (0xFFU)

/* Version of this module's code and API. */
#define BSP_CODE_VERSION_MAJOR      (2U)
#define BSP_CODE_VERSION_MINOR      (0U)
#define BSP_API_VERSION_MAJOR       (2U)
#define BSP_API_VERSION_MINOR       (0U)

#if 1 == BSP_CFG_RTOS
#define SF_CONTEXT_SAVE    tx_isr_start(__get_IPSR());
#define SF_CONTEXT_RESTORE tx_isr_end(__get_IPSR());
void  tx_isr_start(unsigned long isr_id);
void  tx_isr_end(unsigned long isr_id);
#else
#define SF_CONTEXT_SAVE
#define SF_CONTEXT_RESTORE
#endif

/** Function call to insert before returning assertion error. */
#if (1 == BSP_CFG_ASSERT)
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define SSP_ASSERT_FAIL                         SSP_ERROR_LOG(SSP_ERR_ASSERTION, __FILE__, __LINE__);
#else
#define SSP_ASSERT_FAIL
#endif

/** This function is called before returning an error code. To stop on a runtime error, define ssp_error_log in
 * user code and do required debugging (breakpoints, stack dump, etc) in this function.*/
#if (1 == BSP_CFG_ERROR_LOG)
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
/*LDRA_INSPECTED 340 S. Function-like macro used here to allow usage of a standard error logger which extracts module name and line number in a module without adding "#if SSP_ERROR_LOG". */
#ifndef SSP_ERROR_LOG
#define SSP_ERROR_LOG(err, module, version)     SSP_PARAMETER_NOT_USED((version));          \
                                                ssp_error_log((err), (module), __LINE__);
#endif
#else
/*LDRA_INSPECTED 340 S. Function-like macro used here to allow usage of a standard error logger which extracts module name and line number in a module without adding "#if SSP_ERROR_LOG". */
#define SSP_ERROR_LOG(err, module, version)
#endif

/** Default assertion calls ::SSP_ASSERT_FAIL if condition "a" is false. Used to identify incorrect use of API's in SSP
 * functions. */
#if (2 == BSP_CFG_ASSERT)
#define SSP_ASSERT(a)   {assert(a);}
#else
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define SSP_ASSERT(a)                 \
    {                                 \
        if ((a))                      \
        {                             \
            (void) 0;/* Do nothing */ \
        }                             \
        else                          \
        {                             \
            SSP_ASSERT_FAIL;          \
            return SSP_ERR_ASSERTION; \
        }                             \
    }
#endif // ifndef SSP_ASSERT

/** All SSP error codes are returned using this macro. Calls ::SSP_ERROR_LOG function if condition "a" is false. Used
 * to identify runtime errors in SSP functions. */
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define SSP_ERROR_RETURN(a, err, module, version) \
    {                                             \
        if ((a))                                  \
        {                                         \
            (void) 0; /* Do nothing */            \
        }                                         \
        else                                      \
        {                                         \
            SSP_ERROR_LOG((err), (module), (version));  \
            return (err);                         \
        }                                         \
    }

/** This check is performed to select suitable ASM API with respect to core */
/* The macros __CORE__ , __ARM7EM__ and __ARM_ARCH_8M_BASE__ are undefined for GCC, but defined(__IAR_SYSTEMS_ICC__) is false for GCC, so
 * the left half of the || expression evaluates to false for GCC regardless of the values of these macros. */
/*LDRA_INSPECTED 337 S *//*LDRA_INSPECTED 337 S *//*LDRA_INSPECTED 337 S *//*LDRA_INSPECTED 337 S */
#if (defined(__IAR_SYSTEMS_ICC__) && ((__CORE__ == __ARM7EM__) || (__CORE__ == __ARM_ARCH_8M_BASE__) ) ) || defined(__ARM_ARCH_7EM__) // CM4
#ifndef BSP_CFG_IRQ_MASK_LEVEL_FOR_CRITICAL_SECTION
#define BSP_CFG_IRQ_MASK_LEVEL_FOR_CRITICAL_SECTION (0U)
#endif
#else // CM0+
#ifdef BSP_CFG_IRQ_MASK_LEVEL_FOR_CRITICAL_SECTION
#undef BSP_CFG_IRQ_MASK_LEVEL_FOR_CRITICAL_SECTION
#define BSP_CFG_IRQ_MASK_LEVEL_FOR_CRITICAL_SECTION (0U)
#endif
#endif

/** This macro defines a variable for saving previous mask value */
#ifndef SSP_CRITICAL_SECTION_DEFINE
/*LDRA_INSPECTED 77 S This macro does need to be surrounded by parentheses. */
#define SSP_CRITICAL_SECTION_DEFINE uint32_t old_mask_level = 0U
#endif

/** This macro defined to get the mask value */
/*LDRA_INSPECTED 337 S *//*LDRA_INSPECTED 337 S */
#ifndef SSP_CRITICAL_SECTION_ENTER
#if(0 == BSP_CFG_IRQ_MASK_LEVEL_FOR_CRITICAL_SECTION)
#define SSP_CRITICAL_SECTION_ENTER  old_mask_level = __get_PRIMASK(); \
        __set_PRIMASK(1U)
#else
#define SSP_CRITICAL_SECTION_ENTER  old_mask_level = (uint32_t) __get_BASEPRI(); \
        __set_BASEPRI((uint8_t) (BSP_CFG_IRQ_MASK_LEVEL_FOR_CRITICAL_SECTION << (8U - __NVIC_PRIO_BITS)))
#endif
#endif

/** This macro defined to restore the old mask value */
/*LDRA_INSPECTED 337 S *//*LDRA_INSPECTED 337 S */
#ifndef SSP_CRITICAL_SECTION_EXIT
#if(0 == BSP_CFG_IRQ_MASK_LEVEL_FOR_CRITICAL_SECTION)
#define SSP_CRITICAL_SECTION_EXIT  __set_PRIMASK(old_mask_level)
#else
#define SSP_CRITICAL_SECTION_EXIT  __set_BASEPRI(old_mask_level)
#endif
#endif

/** @} (end defgroup BSP_MCU_COMMON) */

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/** Different warm start entry locations in the BSP. */
typedef enum e_bsp_warm_start_event
{
    BSP_WARM_START_PRE_C = 0,   ///< Called almost immediately after reset. No C runtime environment, clocks, or IRQs.
    BSP_WARM_START_POST_C       ///< Called after clocks and C runtime environment have been setup
} bsp_warm_start_event_t;

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/
#if ((1 == BSP_CFG_ERROR_LOG) || (1 == BSP_CFG_ASSERT))
/** Prototype of function called before errors are returned in SSP code if BSP_CFG_LOG_ERRORS is set to 1. */
void ssp_error_log(ssp_err_t err, const char * module, int32_t line);
#endif

/** In the event of an unrecoverable error the BSP will by default call the __BKPT() intrinsic function which will
 *  alert the user of the error. The user can override this default behavior by defining their own
 *  BSP_CFG_HANDLE_UNRECOVERABLE_ERROR macro.
 */
#if !defined(BSP_CFG_HANDLE_UNRECOVERABLE_ERROR)
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(x)    __BKPT((x))
#endif


/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* BSP_COMMON_H_ */
