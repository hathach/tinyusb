/**************************************************************************/
/*!
    @file     tusb_config.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// CONTROLLER CONFIGURATION
//--------------------------------------------------------------------+
//#define CFG_TUSB_MCU will be passed from IDE/command line for easy board/mcu switching

#define CFG_TUSB_CONTROLLER_0_MODE  (TUSB_MODE_DEVICE)
//#define CFG_TUSB_CONTROLLER_1_MODE  (TUSB_MODE_DEVICE)

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+
#define CFG_TUSB_DEVICE_ENDOINT0_SIZE     64

//------------- CLASS -------------//
#define CFG_TUSB_DEVICE_HID_KEYBOARD      1
#define CFG_TUSB_DEVICE_HID_MOUSE         1
#define CFG_TUSB_DEVICE_HID_GENERIC       0 // not supported yet
#define CFG_TUSB_DEVICE_MSC               1
#define CFG_TUSB_DEVICE_CDC               1

//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+
#define CFG_TUSB_DEBUG                    2

//#define CFG_TUSB_OS                   OPT_OS_NONE // be passed from IDE/command line for easy project switching
//#define CFG_TUSB_OS_TASK_PRIO         0            // be passed from IDE/command line for easy project switching

//--------------------------------------------------------------------+
// USB RAM PLACEMENT
//--------------------------------------------------------------------+
#ifdef __CODE_RED // compiled with lpcxpresso

  #if (CFG_TUSB_MCU == MCU_LPC11UXX) || (CFG_TUSB_MCU == MCU_LPC13UXX)
    #define CFG_TUSB_ATTR_USBRAM  ATTR_SECTION(.data.$RAM2) ATTR_ALIGNED(64) // lp11u & lp13u requires data to be 64 byte aligned
  #elif CFG_TUSB_MCU == MCU_LPC175X_6X
    #define CFG_TUSB_ATTR_USBRAM // LPC17xx USB DMA can access all
  #elif  (CFG_TUSB_MCU == MCU_LPC43XX)
    #define CFG_TUSB_ATTR_USBRAM  ATTR_SECTION(.data.$RAM3)
  #endif

#elif defined  __CC_ARM // Compiled with Keil armcc, USBRAM_SECTION is defined in scatter files

  #if (CFG_TUSB_MCU == MCU_LPC11UXX) || (CFG_TUSB_MCU == MCU_LPC13UXX)
    #define CFG_TUSB_ATTR_USBRAM  ATTR_SECTION(USBRAM_SECTION) ATTR_ALIGNED(64) // lp11u & lp13u requires data to be 64 byte aligned
  #elif (CFG_TUSB_MCU == MCU_LPC175X_6X)
    #define CFG_TUSB_ATTR_USBRAM  // LPC17xx USB DMA can access all address
  #elif  (CFG_TUSB_MCU == MCU_LPC43XX)
    #define CFG_TUSB_ATTR_USBRAM // Use keil tool configure to have AHB SRAM as default memory
  #endif

#elif defined __ICCARM__ // compiled with IAR

  #if (CFG_TUSB_MCU == MCU_LPC11UXX) || (CFG_TUSB_MCU == MCU_LPC13UXX)
    #define CFG_TUSB_ATTR_USBRAM _Pragma("location=\"USB_PACKET_MEMORY\"") ATTR_ALIGNED(64)
  #elif (CFG_TUSB_MCU == MCU_LPC175X_6X)
    #define CFG_TUSB_ATTR_USBRAM
  #elif  (CFG_TUSB_MCU == MCU_LPC43XX)
    #define CFG_TUSB_ATTR_USBRAM _Pragma("location=\".ahb_sram1\"")
  #endif

#else

  #error compiler not specified

#endif

// LPC11uxx and LPC13uxx requires each buffer has to be 64-byte alignment
#if CFG_TUSB_MCU == MCU_LPC11UXX || CFG_TUSB_MCU == MCU_LPC13UXX
 #define CFG_TUSB_MEM_ALIGN   ATTR_ALIGNED(64)
#elif defined NRF52840_XXAA
 #define CFG_TUSB_MEM_ALIGN   ATTR_ALIGNED(4)
#else
 #define CFG_TUSB_MEM_ALIGN
#endif


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
