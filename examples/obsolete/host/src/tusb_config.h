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

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// CONTROLLER CONFIGURATION
//--------------------------------------------------------------------+
//#define CFG_TUSB_MCU will be passed from IDE for easy board/mcu switching
#define CFG_TUSB_RHPORT0_MODE      (OPT_MODE_HOST)

//--------------------------------------------------------------------+
// HOST CONFIGURATION
//--------------------------------------------------------------------+

//------------- CLASS -------------//
#define CFG_TUH_HUB               1
#define CFG_TUH_HID_KEYBOARD      1
#define CFG_TUH_HID_MOUSE         1
#define CFG_TUSB_HOST_HID_GENERIC       0 // (not yet supported)
#define CFG_TUH_MSC               1
#define CFG_TUH_CDC               1

#define CFG_TUSB_HOST_DEVICE_MAX        (CFG_TUH_HUB ? 5 : 1) // normal hub has 4 ports

//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+
#define CFG_TUSB_DEBUG                  1

//#define CFG_TUSB_OS                   OPT_OS_NONE // defined using eclipse build
//#define CFG_TUD_TASK_PRIO         0            // defined using eclipse build

//--------------------------------------------------------------------+
// USB RAM PLACEMENT
//--------------------------------------------------------------------+
#ifdef __CODE_RED // make use of code red's support for ram region macros

  #if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X
    #define CFG_TUSB_MEM_SECTION // LPC17xx USB DMA can access all address
  #elif  (CFG_TUSB_MCU == OPT_MCU_LPC43XX)
    #define CFG_TUSB_MEM_SECTION  TU_ATTR_SECTION(.data.$RAM3)
  #endif

#elif defined __CC_ARM // Compiled with Keil armcc

  #if (CFG_TUSB_MCU == OPT_MCU_LPC175X_6X)
    #define CFG_TUSB_MEM_SECTION  // LPC17xx USB DMA can access all address
  #elif  (CFG_TUSB_MCU == OPT_MCU_LPC43XX)
    #define CFG_TUSB_MEM_SECTION // Use keil tool configure to have AHB SRAM as default memory
  #endif

#elif defined __ICCARM__ // compiled with IAR

  #if (CFG_TUSB_MCU == OPT_MCU_LPC175X_6X)
    // LP175x_6x can access all but CMSIS-RTX causes overflow in 32KB SRAM --> move to AHB ram
    #define CFG_TUSB_MEM_SECTION _Pragma("location=\".sram\"")
  #elif  (CFG_TUSB_MCU == OPT_MCU_LPC43XX)
    #define CFG_TUSB_MEM_SECTION _Pragma("location=\".ahb_sram1\"")
  #endif

#else

  #error compiler not specified

#endif


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
