/*
 * tusb_config.h
 *
 *  Created on: Jan 11, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

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
// HOST CONFIGURATION
//--------------------------------------------------------------------+
#define TUSB_CFG_HOST

//------------- CORE/CONTROLLER -------------//
#define TUSB_CFG_HOST_CONTROLLER_NUM             2
#define TUSB_CFG_HOST_CONTROLLER_START_INDEX     1

#define TUSB_CFG_HOST_DEVICE_MAX                 2
#define TUSB_CFG_CONFIGURATION_MAX               2

#define TUSB_CFG_HOST_ENUM_BUFFER_SIZE           256
//------------- CLASS -------------//
#define TUSB_CFG_HOST_HID_KEYBOARD               1
#define TUSB_CFG_HOST_HID_KEYBOARD_ENDPOINT_SIZE 64

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+
//#define TUSB_CFG_DEVICE

//------------- CORE/CONTROLLER -------------//

//------------- CLASS -------------//
//#define TUSB_CFG_DEVICE_CDC
#define TUSB_CFG_DEVICE_HID_KEYBOARD
//#define TUSB_CFG_DEVICE_HID_MOUSE

//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+

#define TUSB_CFG_DEBUG 3

#define TUSB_CFG_OS TUSB_OS_NONE
#define TUSB_CFG_OS_TICKS_PER_SECOND 1000 // 1 ms tick
#define TUSB_CFG_ATTR_USBRAM

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TUSB_CONFIG_H_ */

/** @} */
