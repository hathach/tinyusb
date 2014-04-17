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

#ifndef _TUSB_TUSB_CONFIG_H_
#define _TUSB_TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// CONTROLLER CONFIGURATION
//--------------------------------------------------------------------+
//#define TUSB_CFG_MCU will be passed from IDE/command line for easy board/mcu switching

#define TUSB_CFG_CONTROLLER_0_MODE  (TUSB_MODE_DEVICE)

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+
#define TUSB_CFG_DEVICE_CONTROL_ENDOINT_SIZE    64

//------------- CLASS -------------//
#define TUSB_CFG_DEVICE_HID_KEYBOARD            1
#define TUSB_CFG_DEVICE_HID_MOUSE               0
#define TUSB_CFG_DEVICE_HID_GENERIC             0 // not supported yet
#define TUSB_CFG_DEVICE_MSC                     1
#define TUSB_CFG_DEVICE_CDC                     1

//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+
#define TUSB_CFG_DEBUG                2

//#define TUSB_CFG_OS                   TUSB_OS_NONE // be passed from IDE/command line for easy project switching
//#define TUSB_CFG_OS_TASK_PRIO         0            // be passed from IDE/command line for easy project switching
#define TUSB_CFG_TICKS_HZ             1000

//--------------------------------------------------------------------+
// USB RAM PLACEMENT
//--------------------------------------------------------------------+
#ifdef __CODE_RED // compiled with lpcxpresso

  #if (TUSB_CFG_MCU == MCU_LPC11UXX) || (TUSB_CFG_MCU == MCU_LPC13UXX)
    #define TUSB_CFG_ATTR_USBRAM  ATTR_SECTION(.data.$RAM2) ATTR_ALIGNED(64) // lp11u & lp13u requires data to be 64 byte aligned
  #elif TUSB_CFG_MCU == MCU_LPC175X_6X
    #define TUSB_CFG_ATTR_USBRAM // LPC17xx USB DMA can access all
  #elif  (TUSB_CFG_MCU == MCU_LPC43XX)
    #define TUSB_CFG_ATTR_USBRAM  ATTR_SECTION(.data.$RAM3)
  #endif

#elif defined  __CC_ARM // Compiled with Keil armcc, USBRAM_SECTION is defined in scatter files

  #if (TUSB_CFG_MCU == MCU_LPC11UXX) || (TUSB_CFG_MCU == MCU_LPC13UXX)
    #define TUSB_CFG_ATTR_USBRAM  ATTR_SECTION(USBRAM_SECTION) ATTR_ALIGNED(64) // lp11u & lp13u requires data to be 64 byte aligned
  #elif (TUSB_CFG_MCU == MCU_LPC175X_6X)
    #define TUSB_CFG_ATTR_USBRAM  // LPC17xx USB DMA can access all address
  #elif  (TUSB_CFG_MCU == MCU_LPC43XX)
    #define TUSB_CFG_ATTR_USBRAM // Use keil tool configure to have AHB SRAM as default memory
  #endif

#elif defined __ICCARM__ // compiled with IAR

  #if (TUSB_CFG_MCU == MCU_LPC11UXX) || (TUSB_CFG_MCU == MCU_LPC13UXX)
    #define TUSB_CFG_ATTR_USBRAM _Pragma("location=\"USB_PACKET_MEMORY\"") ATTR_ALIGNED(64)
  #elif (TUSB_CFG_MCU == MCU_LPC175X_6X)
    #define TUSB_CFG_ATTR_USBRAM
  #elif  (TUSB_CFG_MCU == MCU_LPC43XX)
    #define TUSB_CFG_ATTR_USBRAM _Pragma("location=\".ahb_sram1\"")
  #endif

#else

  #error compiler not specified

#endif


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TUSB_CONFIG_H_ */
