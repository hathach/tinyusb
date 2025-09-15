/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   name: i.MX RT1070 Evaluation Kit
   url: https://www.nxp.com/design/design-center/development-boards-and-designs/i-mx-evaluation-and-development-boards/i-mx-rt1170-evaluation-kit:MIMXRT1170-EVKB
*/

#ifndef BOARD_MIMXRT1170_EVKB_H_
#define BOARD_MIMXRT1170_EVKB_H_

// required since iMXRT MCUX-SDK include this file for board size
#define BOARD_FLASH_SIZE (0x1000000U)

// LED: IOMUXC_GPIO_AD_04_GPIO9_IO03
#define LED_PORT              BOARD_INITPINS_USER_LED_PERIPHERAL
#define LED_PIN               BOARD_INITPINS_USER_LED_CHANNEL
#define LED_STATE_ON          0

// SW8 button: IOMUXC_WAKEUP_DIG_GPIO13_IO00
#define BUTTON_PORT           BOARD_INITPINS_USER_BUTTON_PERIPHERAL
#define BUTTON_PIN            BOARD_INITPINS_USER_BUTTON_CHANNEL
#define BUTTON_STATE_ACTIVE   0

// UART: IOMUXC_GPIO_AD_B0_13_LPUART1_RX, IOMUXC_GPIO_AD_B0_12_LPUART1_TX
#define UART_PORT             LPUART1
#define UART_CLK_ROOT         BOARD_BOOTCLOCKRUN_LPUART10_CLK_ROOT

//--------------------------------------------------------------------
// MPU configuration
//--------------------------------------------------------------------
#if __CORTEX_M == 7
static inline void BOARD_ConfigMPU(void) {
  #if defined(__CC_ARM) || defined(__ARMCC_VERSION)
  extern uint32_t Image$$RW_m_ncache$$Base[];
  /* RW_m_ncache_unused is a auxiliary region which is used to get the whole size of noncache section */
  extern uint32_t Image$$RW_m_ncache_unused$$Base[];
  extern uint32_t Image$$RW_m_ncache_unused$$ZI$$Limit[];
  uint32_t nonCacheStart = (uint32_t) Image$$RW_m_ncache$$Base;
  uint32_t size = ((uint32_t) Image$$RW_m_ncache_unused$$Base == nonCacheStart) ? 0 : ((uint32_t) Image$$RW_m_ncache_unused$$ZI$$Limit - nonCacheStart);
  #elif defined(__MCUXPRESSO)
    #if defined(__USE_SHMEM)
  extern uint32_t __base_rpmsg_sh_mem;
  extern uint32_t __top_rpmsg_sh_mem;
  uint32_t nonCacheStart = (uint32_t) (&__base_rpmsg_sh_mem);
  uint32_t size = (uint32_t) (&__top_rpmsg_sh_mem) - nonCacheStart;
    #else
  extern uint32_t __base_NCACHE_REGION;
  extern uint32_t __top_NCACHE_REGION;
  uint32_t nonCacheStart = (uint32_t) (&__base_NCACHE_REGION);
  uint32_t size = (uint32_t) (&__top_NCACHE_REGION) - nonCacheStart;
    #endif
  #elif defined(__ICCARM__) || defined(__GNUC__)
  extern uint32_t __NCACHE_REGION_START[];
  extern uint32_t __NCACHE_REGION_SIZE[];
  uint32_t nonCacheStart = (uint32_t) __NCACHE_REGION_START;
  uint32_t size = (uint32_t) __NCACHE_REGION_SIZE;
  #endif
  volatile uint32_t i = 0;

  #if defined(__ICACHE_PRESENT) && __ICACHE_PRESENT
  /* Disable I cache and D cache */
  if (SCB_CCR_IC_Msk == (SCB_CCR_IC_Msk & SCB->CCR)) {
    SCB_DisableICache();
  }
  #endif
  #if defined(__DCACHE_PRESENT) && __DCACHE_PRESENT
  if (SCB_CCR_DC_Msk == (SCB_CCR_DC_Msk & SCB->CCR)) {
    SCB_DisableDCache();
  }
  #endif

  /* Disable MPU */
  ARM_MPU_Disable();

  /* MPU configure:
     * Use ARM_MPU_RASR(DisableExec, AccessPermission, TypeExtField, IsShareable, IsCacheable, IsBufferable,
     * SubRegionDisable, Size)
     * API in mpu_armv7.h.
     * param DisableExec       Instruction access (XN) disable bit,0=instruction fetches enabled, 1=instruction fetches
     * disabled.
     * param AccessPermission  Data access permissions, allows you to configure read/write access for User and
     * Privileged mode.
     *      Use MACROS defined in mpu_armv7.h:
     * ARM_MPU_AP_NONE/ARM_MPU_AP_PRIV/ARM_MPU_AP_URO/ARM_MPU_AP_FULL/ARM_MPU_AP_PRO/ARM_MPU_AP_RO
     * Combine TypeExtField/IsShareable/IsCacheable/IsBufferable to configure MPU memory access attributes.
     *  TypeExtField  IsShareable  IsCacheable  IsBufferable   Memory Attribute    Shareability        Cache
     *     0             x           0           0             Strongly Ordered    shareable
     *     0             x           0           1              Device             shareable
     *     0             0           1           0              Normal             not shareable   Outer and inner write
     * through no write allocate
     *     0             0           1           1              Normal             not shareable   Outer and inner write
     * back no write allocate
     *     0             1           1           0              Normal             shareable       Outer and inner write
     * through no write allocate
     *     0             1           1           1              Normal             shareable       Outer and inner write
     * back no write allocate
     *     1             0           0           0              Normal             not shareable   outer and inner
     * noncache
     *     1             1           0           0              Normal             shareable       outer and inner
     * noncache
     *     1             0           1           1              Normal             not shareable   outer and inner write
     * back write/read acllocate
     *     1             1           1           1              Normal             shareable       outer and inner write
     * back write/read acllocate
     *     2             x           0           0              Device              not shareable
     *  Above are normal use settings, if your want to see more details or want to config different inner/outer cache
     * policy.
     *  please refer to Table 4-55 /4-56 in arm cortex-M7 generic user guide <dui0646b_cortex_m7_dgug.pdf>
     * param SubRegionDisable  Sub-region disable field. 0=sub-region is enabled, 1=sub-region is disabled.
     * param Size              Region size of the region to be configured. use ARM_MPU_REGION_SIZE_xxx MACRO in
     * mpu_armv7.h.
     */

  /*
     * Add default region to deny access to whole address space to workaround speculative prefetch.
     * Refer to Arm errata 1013783-B for more details.
     *
     */
  /* Region 0 setting: Instruction access disabled, No data access permission. */
  MPU->RBAR = ARM_MPU_RBAR(0, 0x00000000U);
  MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_NONE, 0, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_4GB);

  /* Region 1 setting: Memory with Device type, not shareable, non-cacheable. */
  MPU->RBAR = ARM_MPU_RBAR(1, 0x80000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_512MB);

  /* Region 2 setting: Memory with Device type, not shareable,  non-cacheable. */
  MPU->RBAR = ARM_MPU_RBAR(2, 0x60000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_512MB);

  /* Region 3 setting: Memory with Device type, not shareable, non-cacheable. */
  MPU->RBAR = ARM_MPU_RBAR(3, 0x00000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_1GB);

  /* Region 4 setting: Memory with Normal type, not shareable, outer/inner write back */
  MPU->RBAR = ARM_MPU_RBAR(4, 0x00000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_256KB);

  /* Region 5 setting: Memory with Normal type, not shareable, outer/inner write back */
  MPU->RBAR = ARM_MPU_RBAR(5, 0x20000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_256KB);

  #if defined(CACHE_MODE_WRITE_THROUGH) && CACHE_MODE_WRITE_THROUGH
  /* Region 6 setting: Memory with Normal type, not shareable, write through */
  MPU->RBAR = ARM_MPU_RBAR(6, 0x20200000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 0, 0, ARM_MPU_REGION_SIZE_1MB);

  /* Region 7 setting: Memory with Normal type, not shareable, write through */
  MPU->RBAR = ARM_MPU_RBAR(7, 0x20300000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 0, 0, ARM_MPU_REGION_SIZE_512KB);
  #else
  /* Region 6 setting: Memory with Normal type, not shareable, outer/inner write back */
  MPU->RBAR = ARM_MPU_RBAR(6, 0x20200000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_1MB);

  /* Region 7 setting: Memory with Normal type, not shareable, outer/inner write back */
  MPU->RBAR = ARM_MPU_RBAR(7, 0x20300000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_512KB);
  #endif

  #if defined(XIP_EXTERNAL_FLASH) && (XIP_EXTERNAL_FLASH == 1)
  /* Region 8 setting: Memory with Normal type, not shareable, outer/inner write back. */
  MPU->RBAR = ARM_MPU_RBAR(8, 0x30000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_RO, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_16MB);
  #endif

  #ifdef USE_SDRAM
    #if defined(CACHE_MODE_WRITE_THROUGH) && CACHE_MODE_WRITE_THROUGH
  /* Region 9 setting: Memory with Normal type, not shareable, write through */
  MPU->RBAR = ARM_MPU_RBAR(9, 0x80000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 0, 0, ARM_MPU_REGION_SIZE_64MB);
    #else
  /* Region 9 setting: Memory with Normal type, not shareable, outer/inner write back */
  MPU->RBAR = ARM_MPU_RBAR(9, 0x80000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_64MB);
    #endif
  #endif

  while ((size >> i) > 0x1U) {
    i++;
  }

  if (i != 0) {
    /* The MPU region size should be 2^N, 5<=N<=32, region base should be multiples of size. */
    assert(!(nonCacheStart % size));
    assert(size == (uint32_t) (1 << i));
    assert(i >= 5);

    /* Region 10 setting: Memory with Normal type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(10, nonCacheStart);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 1, 0, 0, 0, 0, i - 1);
  }

  /* Region 11 setting: Memory with Device type, not shareable, non-cacheable */
  MPU->RBAR = ARM_MPU_RBAR(11, 0x40000000);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_16MB);

  /* Region 12 setting: Memory with Device type, not shareable, non-cacheable */
  MPU->RBAR = ARM_MPU_RBAR(12, 0x41000000);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_2MB);

  /* Region 13 setting: Memory with Device type, not shareable, non-cacheable */
  MPU->RBAR = ARM_MPU_RBAR(13, 0x41400000);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_1MB);

  /* Region 14 setting: Memory with Device type, not shareable, non-cacheable */
  MPU->RBAR = ARM_MPU_RBAR(14, 0x41800000);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_2MB);

  /* Region 15 setting: Memory with Device type, not shareable, non-cacheable */
  MPU->RBAR = ARM_MPU_RBAR(15, 0x42000000);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_1MB);

  /* Enable MPU */
  ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_HFNMIENA_Msk);

  /* Enable I cache and D cache */
  #if defined(__DCACHE_PRESENT) && __DCACHE_PRESENT
  SCB_EnableDCache();
  #endif
  #if defined(__ICACHE_PRESENT) && __ICACHE_PRESENT
  SCB_EnableICache();
  #endif
}

#elif __CORTEX_M == 4

static void BOARD_ConfigMPU(void) {
  #if defined(__CC_ARM) || defined(__ARMCC_VERSION)
  extern uint32_t Image$$RW_m_ncache$$Base[];
  /* RW_m_ncache_unused is a auxiliary region which is used to get the whole size of noncache section */
  extern uint32_t Image$$RW_m_ncache_unused$$Base[];
  extern uint32_t Image$$RW_m_ncache_unused$$ZI$$Limit[];
  uint32_t nonCacheStart = (uint32_t) Image$$RW_m_ncache$$Base;
  uint32_t nonCacheSize = ((uint32_t) Image$$RW_m_ncache_unused$$Base == nonCacheStart) ? 0 : ((uint32_t) Image$$RW_m_ncache_unused$$ZI$$Limit - nonCacheStart);
  #elif defined(__MCUXPRESSO)
  extern uint32_t __base_NCACHE_REGION;
  extern uint32_t __top_NCACHE_REGION;
  uint32_t nonCacheStart = (uint32_t) (&__base_NCACHE_REGION);
  uint32_t nonCacheSize = (uint32_t) (&__top_NCACHE_REGION) - nonCacheStart;
  #elif defined(__ICCARM__) || defined(__GNUC__)
  extern uint32_t __NCACHE_REGION_START[];
  extern uint32_t __NCACHE_REGION_SIZE[];
  uint32_t nonCacheStart = (uint32_t) __NCACHE_REGION_START;
  uint32_t nonCacheSize = (uint32_t) __NCACHE_REGION_SIZE;
  #endif
  #if defined(__USE_SHMEM)
    #if defined(__CC_ARM) || defined(__ARMCC_VERSION)
  extern uint32_t Image$$RPMSG_SH_MEM$$Base[];
  /* RPMSG_SH_MEM_unused is a auxiliary region which is used to get the whole size of RPMSG_SH_MEM section */
  extern uint32_t Image$$RPMSG_SH_MEM_unused$$Base[];
  extern uint32_t Image$$RPMSG_SH_MEM_unused$$ZI$$Limit[];
  uint32_t rpmsgShmemStart = (uint32_t) Image$$RPMSG_SH_MEM$$Base;
  uint32_t rpmsgShmemSize = (uint32_t) Image$$RPMSG_SH_MEM_unused$$ZI$$Limit - rpmsgShmemStart;
    #elif defined(__MCUXPRESSO)
  extern uint32_t __base_rpmsg_sh_mem;
  extern uint32_t __top_rpmsg_sh_mem;
  uint32_t rpmsgShmemStart = (uint32_t) (&__base_rpmsg_sh_mem);
  uint32_t rpmsgShmemSize = (uint32_t) (&__top_rpmsg_sh_mem) - rpmsgShmemStart;
    #elif defined(__ICCARM__) || defined(__GNUC__)
  extern uint32_t __RPMSG_SH_MEM_START[];
  extern uint32_t __RPMSG_SH_MEM_SIZE[];
  uint32_t rpmsgShmemStart = (uint32_t) __RPMSG_SH_MEM_START;
  uint32_t rpmsgShmemSize = (uint32_t) __RPMSG_SH_MEM_SIZE;
    #endif
  #endif
  uint32_t i = 0;

  /* Only config non-cacheable region on system bus */
  assert(nonCacheStart >= 0x20000000);

  /* Disable code bus cache */
  if (LMEM_PCCCR_ENCACHE_MASK == (LMEM_PCCCR_ENCACHE_MASK & LMEM->PCCCR)) {
    /* Enable the processor code bus to push all modified lines. */
    LMEM->PCCCR |= LMEM_PCCCR_PUSHW0_MASK | LMEM_PCCCR_PUSHW1_MASK | LMEM_PCCCR_GO_MASK;
    /* Wait until the cache command completes. */
    while ((LMEM->PCCCR & LMEM_PCCCR_GO_MASK) != 0U) {
    }
    /* As a precaution clear the bits to avoid inadvertently re-running this command. */
    LMEM->PCCCR &= ~(LMEM_PCCCR_PUSHW0_MASK | LMEM_PCCCR_PUSHW1_MASK);
    /* Now disable the cache. */
    LMEM->PCCCR &= ~LMEM_PCCCR_ENCACHE_MASK;
  }

  /* Disable system bus cache */
  if (LMEM_PSCCR_ENCACHE_MASK == (LMEM_PSCCR_ENCACHE_MASK & LMEM->PSCCR)) {
    /* Enable the processor system bus to push all modified lines. */
    LMEM->PSCCR |= LMEM_PSCCR_PUSHW0_MASK | LMEM_PSCCR_PUSHW1_MASK | LMEM_PSCCR_GO_MASK;
    /* Wait until the cache command completes. */
    while ((LMEM->PSCCR & LMEM_PSCCR_GO_MASK) != 0U) {
    }
    /* As a precaution clear the bits to avoid inadvertently re-running this command. */
    LMEM->PSCCR &= ~(LMEM_PSCCR_PUSHW0_MASK | LMEM_PSCCR_PUSHW1_MASK);
    /* Now disable the cache. */
    LMEM->PSCCR &= ~LMEM_PSCCR_ENCACHE_MASK;
  }

  /* Disable MPU */
  ARM_MPU_Disable();

  #if defined(CACHE_MODE_WRITE_THROUGH) && CACHE_MODE_WRITE_THROUGH
  /* Region 0 setting: Memory with Normal type, not shareable, write through */
  MPU->RBAR = ARM_MPU_RBAR(0, 0x20200000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 0, 0, ARM_MPU_REGION_SIZE_1MB);

  /* Region 1 setting: Memory with Normal type, not shareable, write through */
  MPU->RBAR = ARM_MPU_RBAR(1, 0x20300000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 0, 0, ARM_MPU_REGION_SIZE_512KB);

  /* Region 2 setting: Memory with Normal type, not shareable, write through */
  MPU->RBAR = ARM_MPU_RBAR(2, 0x80000000U);
  MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 0, 0, ARM_MPU_REGION_SIZE_64MB);

  while ((nonCacheSize >> i) > 0x1U) {
    i++;
  }

  if (i != 0) {
    /* The MPU region size should be 2^N, 5<=N<=32, region base should be multiples of size. */
    assert(!(nonCacheStart % nonCacheSize));
    assert(nonCacheSize == (uint32_t) (1 << i));
    assert(i >= 5);

    /* Region 3 setting: Memory with device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(3, nonCacheStart);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, i - 1);
  }

    #if defined(__USE_SHMEM)
  i = 0;

  while ((rpmsgShmemSize >> i) > 0x1U) {
    i++;
  }

  if (i != 0) {
    /* The MPU region size should be 2^N, 5<=N<=32, region base should be multiples of size. */
    assert(!(rpmsgShmemStart % rpmsgShmemSize));
    assert(rpmsgShmemSize == (uint32_t) (1 << i));
    assert(i >= 5);

    /* Region 4 setting: Memory with device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(4, rpmsgShmemStart);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, i - 1);
  }
    #endif
  #else
  while ((nonCacheSize >> i) > 0x1U) {
    i++;
  }

  if (i != 0) {
    /* The MPU region size should be 2^N, 5<=N<=32, region base should be multiples of size. */
    assert(!(nonCacheStart % nonCacheSize));
    assert(nonCacheSize == (uint32_t) (1 << i));
    assert(i >= 5);

    /* Region 0 setting: Memory with device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(0, nonCacheStart);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, i - 1);
  }

    #if defined(__USE_SHMEM)
  i = 0;

  while ((rpmsgShmemSize >> i) > 0x1U) {
    i++;
  }

  if (i != 0) {
    /* The MPU region size should be 2^N, 5<=N<=32, region base should be multiples of size. */
    assert(!(rpmsgShmemStart % rpmsgShmemSize));
    assert(rpmsgShmemSize == (uint32_t) (1 << i));
    assert(i >= 5);

    /* Region 1 setting: Memory with device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(1, rpmsgShmemStart);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, i - 1);
  }
    #endif
  #endif

  /* Enable MPU */
  ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_HFNMIENA_Msk);

  /* Enables the processor system bus to invalidate all lines in both ways.
    and Initiate the processor system bus cache command. */
  LMEM->PSCCR |= LMEM_PSCCR_INVW0_MASK | LMEM_PSCCR_INVW1_MASK | LMEM_PSCCR_GO_MASK;
  /* Wait until the cache command completes */
  while ((LMEM->PSCCR & LMEM_PSCCR_GO_MASK) != 0U) {
  }
  /* As a precaution clear the bits to avoid inadvertently re-running this command. */
  LMEM->PSCCR &= ~(LMEM_PSCCR_INVW0_MASK | LMEM_PSCCR_INVW1_MASK);
  /* Now enable the system bus cache. */
  LMEM->PSCCR |= LMEM_PSCCR_ENCACHE_MASK;

  /* Enables the processor code bus to invalidate all lines in both ways.
    and Initiate the processor code bus code cache command. */
  LMEM->PCCCR |= LMEM_PCCCR_INVW0_MASK | LMEM_PCCCR_INVW1_MASK | LMEM_PCCCR_GO_MASK;
  /* Wait until the cache command completes. */
  while ((LMEM->PCCCR & LMEM_PCCCR_GO_MASK) != 0U) {
  }
  /* As a precaution clear the bits to avoid inadvertently re-running this command. */
  LMEM->PCCCR &= ~(LMEM_PCCCR_INVW0_MASK | LMEM_PCCCR_INVW1_MASK);
  /* Now enable the code bus cache. */
  LMEM->PCCCR |= LMEM_PCCCR_ENCACHE_MASK;
}
#endif


#endif
