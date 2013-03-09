/*
 * usbh_hcd.h
 *
 *  Created on: Feb 4, 2013
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

#ifndef _TUSB_USBH_HCD_H_
#define _TUSB_USBH_HCD_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"

//--------------------------------------------------------------------+
// USBH
//--------------------------------------------------------------------+
typedef struct ATTR_ALIGNED(4){
  uint8_t core_id;
  uint8_t hub_addr;
  uint8_t hub_port;
  uint8_t speed;
} usbh_enumerate_t;

typedef struct { // TODO internal structure, re-order members
  //------------- port info -------------//
  uint8_t core_id;
  uint8_t hub_addr;
  uint8_t hub_port;
  uint8_t speed;

  //------------- device descriptor info -------------//
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t  configure_count; // bNumConfigurations alias

  //------------- configuration descriptor info -------------//
  uint8_t interface_count; // bNumInterfaces alias

  uint8_t status; // value from enum tusbh_device_status_

//  pipe_handle_t pipe_control; NOTE: use device address/handle instead
  tusb_std_request_t control_request;
  OSAL_SEM_DEF(semaphore);
  osal_semaphore_handle_t sem_hdl;

#if 0 // TODO allow configure for vendor/product
  struct {
    uint8_t interface_count;
    uint8_t attributes;
  } configuration;
#endif

} usbh_device_info_t;

extern usbh_device_info_t usbh_device_info_pool[TUSB_CFG_HOST_DEVICE_MAX+1]; // including zero-address

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBH_HCD_H_ */

/** @} */
