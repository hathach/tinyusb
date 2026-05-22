/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
  #define BOARD_TUD_RHPORT 0
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
  #define BOARD_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
  #error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
  #define CFG_TUSB_OS OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
  #define CFG_TUSB_DEBUG 0
#endif

// Enable Device stack
#define CFG_TUD_ENABLED 1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED BOARD_TUD_MAX_SPEED

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
  #define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
  #define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

// Use different configurations to test all net devices (also due to resource limitations)
#ifndef USE_ECM
#if TU_CHECK_MCU(OPT_MCU_LPC15XX, OPT_MCU_LPC40XX, OPT_MCU_LPC51UXX, OPT_MCU_LPC54)
  #define USE_ECM 1
#elif TU_CHECK_MCU(OPT_MCU_SAMD21, OPT_MCU_SAML2X)
  #define USE_ECM 1
#elif TU_CHECK_MCU(OPT_MCU_STM32F0, OPT_MCU_STM32F1)
  #define USE_ECM 1
#else
  #define USE_ECM 0
#endif
#endif

// MCU SRAM tier — drives the bigger lwIP buffers in lwipopts.h, the larger
// NCM OUT NTB size below, and whether iperf is built. Small-RAM MCUs
// (stm32c0/f1/wb, lpc11/13, samd11) keep modest defaults to fit.
#ifndef LWIP_HIGH_THROUGHPUT
  #if TU_CHECK_MCU(OPT_MCU_MAX32650, OPT_MCU_MAX32666, OPT_MCU_MAX32690, OPT_MCU_MAX78002) || \
      TU_CHECK_MCU(OPT_MCU_STM32F2,  OPT_MCU_STM32F4,  OPT_MCU_STM32F7) || \
      TU_CHECK_MCU(OPT_MCU_STM32H5,  OPT_MCU_STM32H7,  OPT_MCU_STM32H7RS) || \
      TU_CHECK_MCU(OPT_MCU_STM32U5,  OPT_MCU_STM32N6) || \
      TU_CHECK_MCU(OPT_MCU_RP2040) || \
      TU_CHECK_MCU(OPT_MCU_MIMXRT1XXX) || \
      TU_CHECK_MCU(OPT_MCU_NRF5X)
    #define LWIP_HIGH_THROUGHPUT 1
  #else
    #define LWIP_HIGH_THROUGHPUT 0
  #endif
#endif

#if LWIP_HIGH_THROUGHPUT && !defined(INCLUDE_IPERF)
  #define INCLUDE_IPERF
#endif

//--------------------------------------------------------------------
// NCM CLASS CONFIGURATION, SEE "ncm.h" FOR PERFORMANCE TUNING
//--------------------------------------------------------------------

// CDC-NCM 1.0 Table 6-4 defines 2048 as the minimum required NTB size
#define CFG_TUD_NCM_IN_NTB_MAX_SIZE 2048

#if LWIP_HIGH_THROUGHPUT
  #define CFG_TUD_NCM_OUT_NTB_MAX_SIZE 4096
#else
  #define CFG_TUD_NCM_OUT_NTB_MAX_SIZE 2048
#endif

// Number of NCM transfer blocks for reception side
#ifndef CFG_TUD_NCM_OUT_NTB_N
  #define CFG_TUD_NCM_OUT_NTB_N 1
#endif

#ifndef CFG_TUD_NCM_IN_NTB_N
  #define CFG_TUD_NCM_IN_NTB_N 1
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
  #define CFG_TUD_ENDPOINT0_SIZE 64
#endif

//------------- CLASS -------------//

// Network class has 2 drivers: ECM/RNDIS and NCM.
// Only one of the drivers can be enabled
#define CFG_TUD_ECM_RNDIS     USE_ECM
#define CFG_TUD_NCM           (1 - CFG_TUD_ECM_RNDIS)

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
