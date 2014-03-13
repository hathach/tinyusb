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

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_TUSB_CONFIG_H_
#define _TUSB_TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// CONTROLLER CONFIGURATION
//--------------------------------------------------------------------+
//#define TUSB_CFG_MCU will be passed from IDE for easy board/mcu switching
#define TUSB_CFG_CONTROLLER_0_MODE  (TUSB_MODE_HOST)
#define TUSB_CFG_CONTROLLER_1_MODE  (TUSB_MODE_NONE) // TODO not yet tested

//--------------------------------------------------------------------+
// HOST CONFIGURATION
//--------------------------------------------------------------------+
#define TUSB_CFG_HOST_DEVICE_MAX                3 // TODO be a part of HUB config
#define TUSB_CFG_HOST_ENUM_BUFFER_SIZE          255

//------------- CLASS -------------//
#define TUSB_CFG_HOST_HUB                       1
#define TUSB_CFG_HOST_HID_KEYBOARD              1
#define TUSB_CFG_HOST_HID_MOUSE                 1
#define TUSB_CFG_HOST_HID_GENERIC               0
#define TUSB_CFG_HOST_MSC                       1
#define TUSB_CFG_HOST_CDC                       1
#define TUSB_CFG_HOST_CDC_RNDIS                 0

//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+
#define TUSB_CFG_DEBUG                3

//#define TUSB_CFG_OS                   TUSB_OS_NONE // defined using eclipse build
//#define TUSB_CFG_OS_TASK_PRIO

#define TUSB_CFG_OS_TICKS_PER_SECOND  1000

//--------------------------------------------------------------------+
// USB RAM PLACEMENT
//--------------------------------------------------------------------+
#ifdef __CODE_RED // make use of code red's support for ram region macros

  #if (TUSB_CFG_MCU == MCU_LPC11UXX) || (TUSB_CFG_MCU == MCU_LPC13UXX)
    #define TUSB_CFG_ATTR_USBRAM  ATTR_SECTION(.data.$RAM2)
  #elif TUSB_CFG_MCU == MCU_LPC175X_6X
    #define TUSB_CFG_ATTR_USBRAM // LPC17xx USB DMA can access all
  #elif  (TUSB_CFG_MCU == MCU_LPC43XX)
    #define TUSB_CFG_ATTR_USBRAM  ATTR_SECTION(.data.$RAM3)
  #endif

#elif defined __CC_ARM // Compiled with Keil armcc

  #define TUSB_CFG_ATTR_USBRAM

#elif defined __ICCARM__ // compiled with IAR

  #if  (TUSB_CFG_MCU == MCU_LPC43XX)
    #define TUSB_CFG_ATTR_USBRAM _Pragma("location=\".ahb_sram1\"")
  #elif (TUSB_CFG_MCU == MCU_LPC175X_6X)
    #define TUSB_CFG_ATTR_USBRAM
  #endif

#else

  #error compiler not specified

#endif


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TUSB_CONFIG_H_ */

/** @} */
