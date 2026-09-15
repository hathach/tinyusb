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
* File Name    : bsp_locking.c
* Description  : This module implements atomic locking
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "bsp_api.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/* Return options from store-exclusive instruction. */
#define BSP_PRV_STREX_SUCCESS           (0x00000000U)
#define BSP_PRV_STREX_FAILURE           (0x00000001U)

/* The macros __CORE__ , __ARM7EM__ and __ARM_ARCH_8M_BASE__ are undefined for GCC, but defined(__IAR_SYSTEMS_ICC__) is false for GCC, so
 * the left half of the || expression evaluates to false for GCC regardless of the values of these macros. */
/*LDRA_INSPECTED 337 S *//*LDRA_INSPECTED 337 S *//*LDRA_INSPECTED 337 S */
#if (defined(__IAR_SYSTEMS_ICC__) && (__CORE__ == __ARM7EM__)) || defined(__ARM_ARCH_7EM__) // CM4
#define BSP_PRIV_CORE_CM4
#elif(defined(__IAR_SYSTEMS_ICC__) && defined(__ARM_ARCH_8M_BASE__)) // CM23
#define BSP_PRIV_CORE_CM23
#else
#define BSP_PRIV_CORE_CM0PLUS
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
/** Array of HW locks. */
static bsp_lock_t * gp_bsp_locks = NULL;

/** Array of HW lock lookup data. */
static ssp_feature_t * gp_bsp_lock_lookup = NULL;

/** Size of HW lock lookup data. */
static uint32_t g_bsp_lock_lookup_size = 0;

#if defined(BSP_PRIV_CORE_CM4) || defined(BSP_PRIV_CORE_CM23)
static inline ssp_err_t r_bsp_software_lock_cm4(bsp_lock_t * p_lock);
#endif
#ifdef BSP_PRIV_CORE_CM0PLUS
static inline ssp_err_t r_bsp_software_lock_cm0plus(bsp_lock_t * p_lock);
#endif

/*******************************************************************************************************************//**
 * @addtogroup BSP_MCU_LOCKING
 *
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Attempt to acquire the lock that has been sent in.
 *
 * @param[in] p_lock Pointer to the structure which contains the lock to be acquired.
 *
 * @retval SSP_SUCCESS          Lock was acquired
 * @retval SSP_ERR_IN_USE       Lock was not acquired
 **********************************************************************************************************************/
ssp_err_t R_BSP_SoftwareLock(bsp_lock_t * p_lock)
{
    ssp_err_t err = SSP_SUCCESS;
#if defined(BSP_PRIV_CORE_CM4) || defined(BSP_PRIV_CORE_CM23)
    err = r_bsp_software_lock_cm4(p_lock);
#endif
#ifdef BSP_PRIV_CORE_CM0PLUS
    err = r_bsp_software_lock_cm0plus(p_lock);
#endif
    return err;
}

#if defined(BSP_PRIV_CORE_CM4) || defined(BSP_PRIV_CORE_CM23)
/*******************************************************************************************************************//**
 * @brief Attempt to acquire the lock that has been sent in (CM4 and CM23 implementation).
 *
 * @param[in] p_lock Pointer to the structure which contains the lock to be acquired.
 *
 * @retval SSP_SUCCESS          Lock was acquired
 * @retval SSP_ERR_IN_USE       Lock was not acquired
 **********************************************************************************************************************/
static inline ssp_err_t r_bsp_software_lock_cm4(bsp_lock_t * p_lock)
{
    ssp_err_t err;
    uint8_t   lock;
    bool      retry;

    err = SSP_ERR_IN_USE;

    /* The Load-Exclusive and Store-Exclusive instructions are being used to perform an exclusive read-modify-write
     * on the input lock. This process is:
     * 1) Use a load-exclusive to read the value of the lock
     * 2) If the lock is available, then modify the lock value so it is reserved. If not available, then issue CLREX.
     * 3) Use a store-exclusive to attempt to write the new value back to memory
     * 4) Test the returned status bit to see if the write was performed or not.
     */

    do
    {
        /* Only perform retry if needed. See why a retry would occur in the comments below. */
        retry = false;

        /* Issue load-exclusive on lock. */
        lock = __LDREXB(&p_lock->lock);

        /* Check if lock is available. */
        if (BSP_LOCK_UNLOCKED == lock)
        {
            /* Lock is available. Attempt to lock it. */
            if (BSP_PRV_STREX_SUCCESS == __STREXB(BSP_LOCK_LOCKED, &p_lock->lock))
            {
                err = SSP_SUCCESS;
            }
            else
            {
                /* If this path is taken it means that the lock was originally available but the STREX did not
                 * complete successfully. There are 2 reasons this could happen. A context switch could have occurred
                 * between the load and store and another task could have taken the lock. If this happened then we
                 * will retry one more time and on the next loop the lock will not be available so CLREX will be
                 * called and an error will be returned. The STREX will be cancelled on any context switch so there
                 * could also have been a context switch and the lock could still be available. If this is the case
                 * then the lock will be detected as available in the next loop iteration and a STREX will be
                 * attempted again.
                 */
                retry = true;
            }
        }
        else
        {
            /* Lock was already taken, clear exclusive hold. */
            __CLREX();
        }
    } while (retry == true);

    return err;
}
#endif

#ifdef BSP_PRIV_CORE_CM0PLUS
/*******************************************************************************************************************//**
 * @brief Attempt to acquire the lock that has been sent in (CM0+ implementation).
 *
 * @param[in] p_lock Pointer to the structure which contains the lock to be acquired.
 *
 * @retval SSP_SUCCESS          Lock was acquired
 * @retval SSP_ERR_IN_USE       Lock was not acquired
 **********************************************************************************************************************/
static inline ssp_err_t r_bsp_software_lock_cm0plus(bsp_lock_t * p_lock)
{
    ssp_err_t err = SSP_ERR_IN_USE;

    SSP_CRITICAL_SECTION_DEFINE;
    SSP_CRITICAL_SECTION_ENTER;

    /* Check if lock is available. */
    if (BSP_LOCK_UNLOCKED == p_lock->lock)
    {
        /* Lock is available. Lock it. */
        p_lock->lock = BSP_LOCK_LOCKED;
        err = SSP_SUCCESS;
    }

    SSP_CRITICAL_SECTION_EXIT;

    return err;
}
#endif

/*******************************************************************************************************************//**
 * @brief Release hold on lock.
 *
 * @param[in] p_lock Pointer to the structure which contains the lock to unlock.
 **********************************************************************************************************************/
void R_BSP_SoftwareUnlock(bsp_lock_t * p_lock)
{
    /* Set lock back to unlocked. */
    p_lock->lock = BSP_LOCK_UNLOCKED;
}

/*******************************************************************************************************************//**
 * @brief Attempt to reserve a hardware resource lock.
 *
 * @param[in] p_feature Pointer to the module specific feature information.
 *
 * @retval SSP_SUCCESS          Lock was acquired
 * @retval SSP_ERR_IN_USE       Lock was not acquired
 **********************************************************************************************************************/
ssp_err_t R_BSP_HardwareLock(ssp_feature_t const * const p_feature)
{
    for (uint32_t i = 0U; i < g_bsp_lock_lookup_size; i++)
    {
        if (p_feature->word == gp_bsp_lock_lookup[i].word)
        {
            /* Pass actual lock to software lock function. */
            return R_BSP_SoftwareLock(&gp_bsp_locks[i]);
        }
    }

    return SSP_ERR_INVALID_ARGUMENT;
}

/*******************************************************************************************************************//**
 * @brief Release hold on lock.
 *
 * @param[in] p_feature Pointer to the module specific feature information.
 **********************************************************************************************************************/
void R_BSP_HardwareUnlock(ssp_feature_t const * const p_feature)
{
    for (uint32_t i = 0U; i < g_bsp_lock_lookup_size; i++)
    {
        if (p_feature->word == gp_bsp_lock_lookup[i].word)
        {
            /* Pass actual lock to software unlock function. */
            R_BSP_SoftwareUnlock(&gp_bsp_locks[i]);
            return;
        }
    }
}



/*******************************************************************************************************************//**
 * @brief Initialize all of the hardware locks to BSP_LOCK_UNLOCKED.
 *
 **********************************************************************************************************************/
void bsp_init_hardware_locks(void)
{
#if defined(__GNUC__)
    /*LDRA_INSPECTED 219 S This is defined by the linker script, so the use of underscore is acceptable. */
    extern uint32_t __Lock_Start;
    gp_bsp_locks = (bsp_lock_t *) &__Lock_Start;

    /*LDRA_INSPECTED 219 S This is defined by the linker script, so the use of underscore is acceptable. */
    extern uint32_t __Lock_Lookup_Start;
    /*LDRA_INSPECTED 219 S This is defined by the linker script, so the use of underscore is acceptable. */
    extern uint32_t __Lock_Lookup_End;

    gp_bsp_lock_lookup = (ssp_feature_t *) &__Lock_Lookup_Start;
    g_bsp_lock_lookup_size = ((uint32_t) &__Lock_Lookup_End - (uint32_t) &__Lock_Lookup_Start) / sizeof(ssp_feature_t);

#endif
#if defined(__ICCARM__)               /* IAR Compiler */
#pragma section="HW_LOCK"
    gp_bsp_locks = __section_begin("HW_LOCK");

#pragma section="LOCK_LOOKUP"
    gp_bsp_lock_lookup = __section_begin("LOCK_LOOKUP");
    g_bsp_lock_lookup_size = __section_size("LOCK_LOOKUP") / sizeof(ssp_feature_t);
#endif

    for (uint32_t i = 0U; i < g_bsp_lock_lookup_size; i++)
    {
        /* Set each Lock to Unlocked. */
        R_BSP_HardwareUnlock(&gp_bsp_lock_lookup[i]);
    }
}

/*******************************************************************************************************************//**
 * @brief Initialize lock value to be unlocked.
 *
 * @param[in] p_lock Pointer to the structure which contains the lock to initialize.
 **********************************************************************************************************************/
void R_BSP_SoftwareLockInit(bsp_lock_t * p_lock)
{
    p_lock->lock = BSP_LOCK_UNLOCKED;
}


/** @} (end addtogroup BSP_MCU_LOCKING) */
