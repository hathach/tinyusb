/*
 * tusb_types.h
 *
 *  Created on: Dec 3, 2012
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
 */

/** \ingroup Group_Core
 *  \defgroup Group_USBTypes USB Types
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_TUSB_TYPES_H_
#define _TUSB_TUSB_TYPES_H_

#ifdef __cplusplus
 extern "C" {
#endif

/// defined base on EHCI specs value for Endpoint Speed
typedef enum {
  FULL_SPEED =0,
  LOWS_PEED,
  HIGH_SPEED
}USB_Speed_t;

/// defined base on USB Specs Endpoint's bmAttributes
typedef enum {
  CONTROL_TYPE = 0,
  ISOCHRONOUS_TYPE,
  BULK_TYPE,
  INTERRUPT_TYPE
}USB_TransferType_t;

/// TBD
typedef enum {
  SETUP_TOKEN,
  IN_TOKEN,
  OUT_TOKEN
}USB_PID_t;

/// USB Descriptor Types (section 9.4 table 9-5)
typedef enum {
  DEVICE_DESC=1 								 , ///< 1
  CONFIGURATIONT_DESC 					 , ///< 2
  STRING_DESC 									 , ///< 3
  INTERFACE_DESC								 , ///< 4
  ENDPOINT_DESC 								 , ///< 5
  DEVICE_QUALIFIER_DESC 				 , ///< 6
  OTHER_SPEED_CONFIGURATION_DESC , ///< 7
  INTERFACE_POWER_DESC					 , ///< 8
  OTG_DESC											 , ///< 9
  DEBUG_DESCRIPTOR							 , ///< 10
  INTERFACE_ASSOCIATION_DESC			 ///< 11
}USB_DescriptorType_t;

typedef enum {
  REQUEST_GET_STATUS =0 		, ///< 0
  REQUEST_CLEAR_FEATURE 		, ///< 1
  REQUEST_RESERVED					, ///< 2
  REQUEST_SET_FEATURE 			, ///< 3
  REQUEST_RESERVED2 				, ///< 4
  REQUEST_SET_ADDRESS 			, ///< 5
  REQUEST_GET_DESCRIPTOR		, ///< 6
  REQUEST_SET_DESCRIPTOR		, ///< 7
  REQUEST_GET_CONFIGURATION , ///< 8
  REQUEST_SET_CONFIGURATION , ///< 9
  REQUEST_GET_INTERFACE 		, ///< 10
  REQUEST_SET_INTERFACE 		, ///< 11
  REQUEST_SYNCH_FRAME 				///< 12
}USB_RequestCode_t;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TUSB_TYPES_H_ */

/** @} */
