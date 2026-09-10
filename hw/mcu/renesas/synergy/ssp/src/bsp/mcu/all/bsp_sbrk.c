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
* File Name    : bsp_sbrk.c
* Description  : Implements _sbrk() for the GCC compiler.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#if defined(__GNUC__)
#include "bsp_api.h"
#include <sys/types.h>
#include <errno.h>

/***********************************************************************************************************************
* Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
* Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
* Private function prototypes
***********************************************************************************************************************/

/***********************************************************************************************************************
* Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
/*LDRA_NOANALYSIS LDRA_INSPECTED below not working. */
/*LDRA_INSPECTED 90 S - This is an override of the standard library _sbrk() and it's prototype must match exactly.*/
caddr_t _sbrk(int incr);
/*LDRA_ANALYSIS */
/***********************************************************************************************************************
* Private global variables and functions
***********************************************************************************************************************/


/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_COMMON
 * @defgroup BSP_MCU_SBRK
 *
 * @{
***********************************************************************************************************************/

/*******************************************************************************************************************//**
* @brief       SSP implementation of the standard library _sbrk() function.
* @param[in]   incr  The number of bytes being asked for by malloc().
*
* @note This function overrides the _sbrk version that exists in the newlib library that is linked with.
*       That version improperly relies on the SP as part of it's allocation strategy. This is bad in general and
*       worse in an RTOS environment. This version insures that we allocate the byte pool requested by malloc()
*       only from our allocated HEAP area. Also note that newlib is pre-built and forces the pagesize used by
*       malloc() to be 4096. That requires that we have a HEAP of at least 4096 if we are to support malloc().
* @retval        Address of allocated area if successful, -1 otherwise.
***********************************************************************************************************************/

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
/*LDRA_INSPECTED 90 S - This is an override of the standard library _sbrk() and it's prototype must match exactly.*/
caddr_t _sbrk(int incr)
{
    /*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
    extern char _Heap_Begin __asm ("__HeapBase");      ///< Defined by the linker.
    /*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
    extern char _Heap_Limit __asm ("__HeapLimit");     ///< Defined by the linker.

    uint32_t bytes = (uint32_t)incr;
    static char* current_heap_end = 0;
    char* current_block_address;

    if (current_heap_end == 0)
    {
        current_heap_end = &_Heap_Begin;
    }

    current_block_address = current_heap_end;

    /** Need to align heap to word boundary, else will get */
    /** hard faults on Cortex-M0. So we assume that heap starts on */
    /** word boundary, hence make sure we always add a multiple of */
    /** 4 to it. */
    bytes = (bytes + 3U) & (~3U);      ///< align value to 4
    if (current_heap_end + bytes > &_Heap_Limit)
    {
        /** Heap has overflowed */
        errno = ENOMEM;
        return (caddr_t) - 1;
    }

    current_heap_end += bytes;
    return (caddr_t) current_block_address;
}

#endif
/******************************************************************************************************************//**
 * @} (end defgroup BSP_MCU_SBRK)
 *********************************************************************************************************************/
