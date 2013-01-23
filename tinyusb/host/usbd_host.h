/*
 * usbd_host.h
 *
 *  Created on: Jan 19, 2013
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

#ifndef _TUSB_USBD_HOST_H_
#define _TUSB_USBD_HOST_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "hcd.h"

typedef struct {
  pipe_handle_t pipe_in;
  osal_queue_id_t qid;
}hid_info_t;

typedef struct {
  uint8_t interface_num;
  uint8_t attributes;

  struct {
    hid_info_t hid_keyboard;
    //      hid_info_t hid_mouse;
    //      hid_info_t hid_generic;
  } classes;
} usbh_configure_info_t;

typedef struct {
  uint8_t core_id;
#if 0 // TODO allow configure for vendor/product
  uint16_t vendor_id;
  uint16_t product_id;
#endif

  usbh_configure_info_t configuration;
} usbh_device_info_t;



//--------------------------------------------------------------------+
// Structures & Types
//--------------------------------------------------------------------+
typedef uint32_t tusb_handle_device_t;

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
void         tusbh_device_mounted_cb (tusb_error_t error, tusb_handle_device_t device_hdl, uint32_t *configure_flags, uint8_t number_of_configure);
tusb_error_t tusbh_configuration_set     (tusb_handle_device_t const device_hdl, uint8_t const configure_number);


// TODO hiding from application include
//--------------------------------------------------------------------+
// CLASS API
//--------------------------------------------------------------------+
usbh_device_info_t* usbh_device_info_get (tusb_handle_device_t device_hdl);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBD_HOST_H_ */

/** @} */
