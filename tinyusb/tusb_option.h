/**************************************************************************/
/*!
    @file     tusb_option.h
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

#ifndef _TUSB_TUSB_OPTION_H_
#define _TUSB_TUSB_OPTION_H_

#define TUSB_VERSION_YEAR   00
#define TUSB_VERSION_MONTH  00
#define TUSB_VERSION_WEEK   0
#define TUSB_VERSION_NAME   "alpha"
#define TUSB_VERSION        XSTRING_(TUSB_VERSION_YEAR) "." XSTRING_(TUSB_VERSION_MONTH)

/** \defgroup group_mcu Supported MCU
 * \ref TUSB_CFG_MCU must be defined to one of these
 *  @{ */
#define MCU_LPC11UXX       1 ///< NXP LPC11Uxx family
#define MCU_LPC13XX        2 ///< NXP LPC13xx (not supported yet)
#define MCU_LPC13UXX       3 ///< NXP LPC13xx 12 bit ADC family
#define MCU_LPC175X_6X     4 ///< NXP LPC175x, LPC176x family
#define MCU_LPC177X_8X     5 ///< NXP LPC177x, LPC178x family (not supported yet)
#define MCU_LPC18XX        6 ///< NXP LPC18xx family (not supported yet)
#define MCU_LPC43XX        7 ///< NXP LPC43xx family
/** @} */

#include "mcu_capacity.h"

#ifndef TUSB_CFG_CONFIG_FILE
  #define TUSB_CFG_CONFIG_FILE "tusb_config.h"
#endif

#include TUSB_CFG_CONFIG_FILE

/** \addtogroup group_configuration
 *  @{ */

//--------------------------------------------------------------------+
// CONTROLLER
//--------------------------------------------------------------------+
/** \defgroup group_mode Controller Mode Selection
 * \brief TUSB_CFG_CONTROLLER_N_MODE must be defined with these
 *  @{ */
#define TUSB_MODE_HOST    0x02 ///< Host Mode
#define TUSB_MODE_DEVICE  0x01 ///< Device Mode
#define TUSB_MODE_NONE    0x00 ///< Disabled
/** @} */

#ifndef TUSB_CFG_CONTROLLER_0_MODE
  #define TUSB_CFG_CONTROLLER_0_MODE TUSB_MODE_NONE
#endif

#ifndef TUSB_CFG_CONTROLLER_1_MODE
  #define TUSB_CFG_CONTROLLER_1_MODE TUSB_MODE_NONE
#endif

#define CONTROLLER_HOST_NUMBER (\
    ((TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_HOST) ? 1 : 0) + \
    ((TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_HOST) ? 1 : 0))

#define CONTROLLER_DEVICE_NUMBER (\
    ((TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_DEVICE) ? 1 : 0) + \
    ((TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_DEVICE) ? 1 : 0))

#define MODE_HOST_SUPPORTED   (CONTROLLER_HOST_NUMBER > 0)
#define MODE_DEVICE_SUPPORTED (CONTROLLER_DEVICE_NUMBER > 0)

#if !MODE_HOST_SUPPORTED && !MODE_DEVICE_SUPPORTED
  #error please configure at least 1 TUSB_CFG_CONTROLLER_N_MODE to TUSB_MODE_HOST and/or TUSB_MODE_DEVICE
#endif

//--------------------------------------------------------------------+
// COMMON OPTIONS
//--------------------------------------------------------------------+
/**
  determines the debug level for the stack
  - Level 3: TBD
  - Level 2: ATTR_ALWAYS_INLINE is null --> no function is inline
  - Level 1: Print out if Assert failed. STATIC_VAR is NULL --> accessible when debugging
  - Level 0: no debug information is generated
*/
#ifndef TUSB_CFG_DEBUG
  #define TUSB_CFG_DEBUG 0
  #warning TUSB_CFG_DEBUG is not defined, default value is 0
#endif

#ifndef TUSB_CFG_ATTR_USBRAM
 #error TUSB_CFG_ATTR_USBRAM is not defined, please help me know how to place data in accessible RAM for usb controller
#endif


#if (TUSB_CFG_OS != TUSB_OS_NONE) && !defined (TUSB_CFG_OS_TASK_PRIO)
  #error TUSB_CFG_OS_TASK_PRIO need to be defined (hint: use the highest if possible)
#endif

//#ifndef TUSB_CFG_CONFIGURATION_MAX
//  #define TUSB_CFG_CONFIGURATION_MAX 1
//  #warning TUSB_CFG_CONFIGURATION_MAX is not defined, default value is 1
//#endif

//--------------------------------------------------------------------+
// HOST OPTIONS
//--------------------------------------------------------------------+
#if MODE_HOST_SUPPORTED
  #ifndef TUSB_CFG_HOST_DEVICE_MAX
    #define TUSB_CFG_HOST_DEVICE_MAX 1
    #warning TUSB_CFG_HOST_DEVICE_MAX is not defined, default value is 1
  #endif

  //------------- HUB CLASS -------------//
  #if TUSB_CFG_HOST_HUB && (TUSB_CFG_HOST_DEVICE_MAX == 1)
    #error there is no benefit enable hub with max device is 1. Please disable hub or increase TUSB_CFG_HOST_DEVICE_MAX
  #endif

  //------------- HID CLASS -------------//
  #define HOST_CLASS_HID   ( TUSB_CFG_HOST_HID_KEYBOARD + TUSB_CFG_HOST_HID_MOUSE + TUSB_CFG_HOST_HID_GENERIC )
//  #if HOST_CLASS_HID
//    #define HOST_HCD_XFER_INTERRUPT
//  #endif

  #ifndef TUSB_CFG_HOST_ENUM_BUFFER_SIZE
    #define TUSB_CFG_HOST_ENUM_BUFFER_SIZE 256
  #endif

  //------------- CLASS -------------//
#endif // MODE_HOST_SUPPORTED

//--------------------------------------------------------------------+
// DEVICE OPTIONS
//--------------------------------------------------------------------+
#if MODE_DEVICE_SUPPORTED

 #define DEVICE_CLASS_HID ( TUSB_CFG_DEVICE_HID_KEYBOARD + TUSB_CFG_DEVICE_HID_MOUSE + TUSB_CFG_DEVICE_HID_GENERIC )

 #if TUSB_CFG_DEVICE_CONTROL_ENDOINT_SIZE > 64
  #error Control Endpoint Max Package Size cannot larger than 64
 #endif

 #ifndef TUSB_CFG_DEVICE_ENUM_BUFFER_SIZE
   #define TUSB_CFG_DEVICE_ENUM_BUFFER_SIZE 256
 #endif

#endif // MODE_DEVICE_SUPPORTED

#endif /* _TUSB_TUSB_OPTION_H_ */

/** @} */
