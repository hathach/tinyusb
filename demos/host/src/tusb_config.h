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

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

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
#define TUSB_CFG_CONTROLLER0_MODE  (TUSB_MODE_HOST)
#define TUSB_CFG_CONTROLLER1_MODE  (TUSB_MODE_NONE)

//--------------------------------------------------------------------+
// HOST CONFIGURATION
//--------------------------------------------------------------------+
#define TUSB_CFG_HOST_DEVICE_MAX     1
#define TUSB_CFG_CONFIGURATION_MAX   1

//------------- USBD -------------//
#define TUSB_CFG_HOST_ENUM_BUFFER_SIZE 255

//------------- CLASS -------------//
#define TUSB_CFG_HOST_HUB           0
#define TUSB_CFG_HOST_HID_KEYBOARD  0
#define TUSB_CFG_HOST_HID_MOUSE     0
#define TUSB_CFG_HOST_HID_GENERIC   0
#define TUSB_CFG_HOST_MSC           1
#define TUSB_CFG_HOST_CDC           0
#define TUSB_CFG_HOST_CDC_RNDIS     0

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+
//#define TUSB_CFG_DEVICE

//------------- CORE/CONTROLLER -------------//

//------------- CLASS -------------//
//#define TUSB_CFG_DEVICE_CDC
//#define TUSB_CFG_DEVICE_HID_KEYBOARD  1
//#define TUSB_CFG_DEVICE_HID_MOUSE     1

//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+

#define TUSB_CFG_DEBUG                2

//#define TUSB_CFG_OS                   TUSB_OS_NONE // defined using eclipse build
//#define TUSB_CFG_OS_TASK_PRIO

#define TUSB_CFG_OS_TICKS_PER_SECOND  1000

#ifdef __CODE_RED // make use of code red's support for ram region macros
  #if (MCU == MCU_LPC11UXX) || (MCU == MCU_LPC13UXX)
    #define TUSB_RAM_SECTION  ".data.$RAM2"
  #elif  (MCU == MCU_LPC43XX)
    #define TUSB_RAM_SECTION  ".data.$RAM3"
  #endif

  #define TUSB_CFG_ATTR_USBRAM   __attribute__ ((section(TUSB_RAM_SECTION)))
#elif defined  __CC_ARM // Compiled with Keil armcc
  #define TUSB_CFG_ATTR_USBRAM
#elif __ICCARM__ // compiled with IAR
  #define TUSB_CFG_ATTR_USBRAM  @ ".ahb_sram1"
#else
  #error compiler not specified
#endif


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TUSB_CONFIG_H_ */

/** @} */
