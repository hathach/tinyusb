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
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __RTTHREAD__
#include <rtconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

#if   defined(SOC_SERIES_STM32F0)
#define CFG_TUSB_MCU    OPT_MCU_STM32F0
#elif defined(SOC_SERIES_STM32F1)
#define CFG_TUSB_MCU    OPT_MCU_STM32F1
#elif defined(SOC_SERIES_STM32F2)
#define CFG_TUSB_MCU    OPT_MCU_STM32F2
#elif defined(SOC_SERIES_STM32F3)
#define CFG_TUSB_MCU    OPT_MCU_STM32F3
#elif defined(SOC_SERIES_STM32F4)
#define CFG_TUSB_MCU    OPT_MCU_STM32F4
#elif defined(SOC_SERIES_STM32F7)
#define CFG_TUSB_MCU    OPT_MCU_STM32F7
#elif defined(SOC_SERIES_STM32H7)
#define CFG_TUSB_MCU    OPT_MCU_STM32H7
#elif defined(SOC_SERIES_STM32L0)
#define CFG_TUSB_MCU    OPT_MCU_STM32L0
#elif defined(SOC_SERIES_STM32L1)
#define CFG_TUSB_MCU    OPT_MCU_STM32L1
#elif defined(SOC_SERIES_STM32L4)
#define CFG_TUSB_MCU    OPT_MCU_STM32L4
#elif defined(SOC_NRF52840)
#define CFG_TUSB_MCU    OPT_MCU_NRF5X
#elif defined(SOC_HPM6000)
#define CFG_TUSB_MCU    OPT_MCU_HPM
#elif defined(SOC_RP2040)
#define CFG_TUSB_MCU    OPT_MCU_RP2040
#elif defined(SOC_FAMILY_RENESAS)
#define CFG_TUSB_MCU    OPT_MCU_RAXXX
#else
#error "Not support for current MCU"
#endif

#define CFG_TUSB_OS OPT_OS_RTTHREAD

//--------------------------------------------------------------------
// DEBUG CONFIGURATION
//--------------------------------------------------------------------
#ifdef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG_PRINTF rt_kprintf
#endif /* CFG_TUSB_DEBUG */

#ifndef BOARD_DEVICE_RHPORT_NUM
#define BOARD_DEVICE_RHPORT_NUM     PKG_TINYUSB_RHPORT_NUM
#endif

#ifndef BOARD_DEVICE_RHPORT_SPEED
#define BOARD_DEVICE_RHPORT_SPEED   PKG_TINYUSB_DEVICE_PORT_SPEED
#endif

#if   BOARD_DEVICE_RHPORT_NUM == 0
#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)
#elif BOARD_DEVICE_RHPORT_NUM == 1
#define CFG_TUSB_RHPORT1_MODE     (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)
#else
  #error "Incorrect RHPort configuration"
#endif

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION        rt_section(PKG_TINYUSB_MEM_SECTION)
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          rt_align(PKG_TINYUSB_MEM_ALIGN)
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE        PKG_TINYUSB_EDPT0_SIZE
#endif

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE        PKG_TINYUSB_DEVICE_CDC_RX_BUFSIZE
#define CFG_TUD_CDC_TX_BUFSIZE        PKG_TINYUSB_DEVICE_CDC_TX_BUFSIZE

#define CFG_TUD_MSC_EP_BUFSIZE        PKG_TINYUSB_DEVICE_MSC_EP_BUFSIZE

#define CFG_TUD_HID_EP_BUFSIZE        PKG_TINYUSB_DEVICE_HID_EP_BUFSIZE

#ifndef PKG_TINYUSB_DEVICE_CDC_STRING
#define PKG_TINYUSB_DEVICE_CDC_STRING ""
#endif

#ifndef PKG_TINYUSB_DEVICE_MSC_STRING
#define PKG_TINYUSB_DEVICE_MSC_STRING ""
#endif

#ifndef PKG_TINYUSB_DEVICE_HID_STRING
#define PKG_TINYUSB_DEVICE_HID_STRING ""
#endif


#ifdef __cplusplus
}
#endif
#endif /*__RTTHREAD__*/

#endif /* _TUSB_CONFIG_H_ */
