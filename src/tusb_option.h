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

#ifndef _TUSB_OPTION_H_
#define _TUSB_OPTION_H_

#define TUSB_VERSION_YEAR   00
#define TUSB_VERSION_MONTH  00
#define TUSB_VERSION_WEEK   0
#define TUSB_VERSION_NAME   "alpha"
#define TUSB_VERSION        XSTRING_(TUSB_VERSION_YEAR) "." XSTRING_(TUSB_VERSION_MONTH)

/** \defgroup group_mcu Supported MCU
 * \ref CFG_TUSB_MCU must be defined to one of these
 *  @{ */
#define OPT_MCU_LPC11UXX       1 ///< NXP LPC11Uxx series
#define OPT_MCU_LPC13XX        2 ///< NXP LPC13xx (not supported yet)
#define OPT_MCU_LPC13UXX       3 ///< NXP LPC13xx 12 bit ADC series
#define OPT_MCU_LPC175X_6X     4 ///< NXP LPC175x, LPC176x series
#define OPT_MCU_LPC177X_8X     5 ///< NXP LPC177x, LPC178x series (not supported yet)
#define OPT_MCU_LPC18XX        6 ///< NXP LPC18xx series (not supported yet)
#define OPT_MCU_LPC43XX        7 ///< NXP LPC43xx series

#define OPT_MCU_NRF5X        100 ///< Nordic nRF5x series
/** @} */

/** \defgroup group_supported_os Supported RTOS
 *  \ref CFG_TUSB_OS must be defined to one of these
 *  @{ */
#define OPT_OS_NONE       1 ///< No RTOS is used
#define OPT_OS_FREERTOS   2 ///< FreeRTOS is used
/** @} */


// Allow to use command line to change the config name/location
#ifndef CFG_TUSB_CONFIG_FILE
  #define CFG_TUSB_CONFIG_FILE "tusb_config.h"
#endif

#include CFG_TUSB_CONFIG_FILE

/** \addtogroup group_configuration
 *  @{ */

//--------------------------------------------------------------------
// CONTROLLER
//--------------------------------------------------------------------
/** \defgroup group_mode Controller Mode Selection
 * \brief CFG_TUSB_CONTROLLER_N_MODE must be defined with these
 *  @{ */
#define OPT_MODE_HOST    0x02 ///< Host Mode
#define OPT_MODE_DEVICE  0x01 ///< Device Mode
#define OPT_MODE_NONE    0x00 ///< Disabled
/** @} */

#ifndef CFG_TUSB_RHPORT0_MODE
  #define CFG_TUSB_RHPORT0_MODE OPT_MODE_NONE
#endif

#ifndef CFG_TUSB_RHPORT1_MODE
  #define CFG_TUSB_RHPORT1_MODE OPT_MODE_NONE
#endif

#define CONTROLLER_HOST_NUMBER (\
    ((CFG_TUSB_RHPORT0_MODE & OPT_MODE_HOST) ? 1 : 0) + \
    ((CFG_TUSB_RHPORT1_MODE & OPT_MODE_HOST) ? 1 : 0))

#define MODE_HOST_SUPPORTED     (CONTROLLER_HOST_NUMBER > 0)

#define TUH_OPT_RHPORT          ( (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HOST) ? 0 : ((CFG_TUSB_RHPORT1_MODE & OPT_MODE_HOST) ? 1 : -1) )
#define TUSB_OPT_HOST_ENABLED   ( TUH_OPT_RHPORT >= 0 )

#define TUD_OPT_RHPORT          ( (CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE) ? 0 : ((CFG_TUSB_RHPORT1_MODE & OPT_MODE_DEVICE) ? 1 : -1) )
#define TUSB_OPT_DEVICE_ENABLED ( TUD_OPT_RHPORT >= 0 )

#if ((CFG_TUSB_RHPORT0_MODE & OPT_MODE_HOST) && (CFG_TUSB_RHPORT1_MODE & OPT_MODE_HOST)) || ((CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE) && (CFG_TUSB_RHPORT1_MODE & OPT_MODE_DEVICE))
  #error "tinyusb does not support same modes on more than 1 roothub port"
#endif

//--------------------------------------------------------------------+
// COMMON OPTIONS
//--------------------------------------------------------------------+
/**
  determines the debug level for the stack
  - Level 3: TBD
  - Level 2: TBD
  - Level 1: Print out if Assert failed. STATIC_VAR is NULL --> accessible when debugging
  - Level 0: no debug information is generated
*/
#ifndef CFG_TUSB_DEBUG
  #define CFG_TUSB_DEBUG 0
  #warning CFG_TUSB_DEBUG is not defined, default value is 0
#endif

#ifndef CFG_TUSB_ATTR_USBRAM
 #error CFG_TUSB_ATTR_USBRAM is not defined, please help me know how to place data in accessible RAM for usb controller
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

//--------------------------------------------------------------------
// DEVICE OPTIONS
//--------------------------------------------------------------------
#if TUSB_OPT_DEVICE_ENABLED

  #ifndef CFG_TUD_ENDOINT0_SIZE
    #define CFG_TUD_ENDOINT0_SIZE    64
  #endif

  #ifndef CFG_TUD_ENUM_BUFFER_SIZE
    #define CFG_TUD_CTRL_BUFSIZE 256
  #endif

  #ifndef CFG_TUD_DESC_AUTO
    #define CFG_TUD_DESC_AUTO 0
  #endif

  #ifndef CFG_TUD_CDC
    #define CFG_TUD_CDC            0
  #endif

  #ifndef CFG_TUD_MSC
    #define CFG_TUD_MSC          0
  #endif

  #ifndef CFG_TUD_HID_KEYBOARD
  #define CFG_TUD_HID_KEYBOARD        0
  #endif

  #ifndef CFG_TUD_HID_MOUSE
  #define CFG_TUD_HID_MOUSE           0
  #endif

  #ifndef CFG_TUD_HID_KEYBOARD_BOOT
    #define CFG_TUD_HID_KEYBOARD_BOOT 0
  #endif

  #ifndef CFG_TUD_HID_MOUSE_BOOT
    #define CFG_TUD_HID_MOUSE_BOOT 0
  #endif

#endif // TUSB_OPT_DEVICE_ENABLED

//--------------------------------------------------------------------
// HOST OPTIONS
//--------------------------------------------------------------------
#if MODE_HOST_SUPPORTED
  #ifndef CFG_TUSB_HOST_DEVICE_MAX
    #define CFG_TUSB_HOST_DEVICE_MAX 1
    #warning CFG_TUSB_HOST_DEVICE_MAX is not defined, default value is 1
  #endif

  //------------- HUB CLASS -------------//
  #if CFG_TUSB_HOST_HUB && (CFG_TUSB_HOST_DEVICE_MAX == 1)
    #error there is no benefit enable hub with max device is 1. Please disable hub or increase CFG_TUSB_HOST_DEVICE_MAX
  #endif

  //------------- HID CLASS -------------//
  #define HOST_CLASS_HID   ( CFG_TUSB_HOST_HID_KEYBOARD + CFG_TUSB_HOST_HID_MOUSE + CFG_TUSB_HOST_HID_GENERIC )
//  #if HOST_CLASS_HID
//    #define HOST_HCD_XFER_INTERRUPT
//  #endif

  #ifndef CFG_TUSB_HOST_ENUM_BUFFER_SIZE
    #define CFG_TUSB_HOST_ENUM_BUFFER_SIZE 256
  #endif

  //------------- CLASS -------------//
#endif // MODE_HOST_SUPPORTED


//------------------------------------------------------------------
// Configuration Validation
//------------------------------------------------------------------

#if (CFG_TUSB_OS != OPT_OS_NONE) && !defined (CFG_TUD_TASK_PRIO)
  #error CFG_TUD_TASK_PRIO need to be defined (hint: use the highest if possible)
#endif

#if CFG_TUD_ENDOINT0_SIZE > 64
  #error Control Endpoint Max Package Size cannot larger than 64
#endif

#endif /* _TUSB_OPTION_H_ */

/** @} */
