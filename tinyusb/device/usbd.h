/**************************************************************************/
/*!
    @file     usbd.h
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup group_usbd
 *  @{ */

#ifndef _TUSB_USBD_H_
#define _TUSB_USBD_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include <common/tusb_common.h>
#include "osal/osal.h"
#include "device/dcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

/// \brief Descriptor pointer collector to all the needed.
typedef struct {
  uint8_t const * p_device;              ///< pointer to device descritpor \ref tusb_desc_device_t
  uint8_t const * p_configuration;       ///< pointer to the whole configuration descriptor, starting by \ref tusb_desc_configuration_t
  uint8_t const** p_string_arr;          ///< a array of pointers to string descriptors

  uint8_t const * p_hid_keyboard_report; ///< pointer to HID report descriptor of Keybaord interface. Only needed if CFG_TUSB_DEVICE_HID_KEYBOARD is enabled
  uint8_t const * p_hid_mouse_report;    ///< pointer to HID report descriptor of Mouse interface. Only needed if CFG_TUSB_DEVICE_HID_MOUSE is enabled
}tusbd_descriptor_pointer_t;

// define by application
extern tusbd_descriptor_pointer_t tusbd_descriptor_pointers;

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_n_mounted(uint8_t rhport);

static inline bool tud_mounted(void)
{
  return tud_n_mounted(0);
}


/*------------- Callback -------------*/
/** \brief 			Callback function that will be invoked device is mounted (configured) by USB host
 * \param[in] 	rhport USB Controller ID of the interface
 * \note        This callback should be used by Application to \b set-up application data
 */
void tud_mount_cb(uint8_t rhport);

/** \brief 			Callback function that will be invoked when device is unmounted (bus reset/unplugged)
 * \param[in] 	rhport USB Controller ID of the interface
 * \note        This callback should be used by Application to \b tear-down application data
 */
void tud_umount_cb(uint8_t rhport);

//void tud_device_suspended_cb(uint8_t rhport);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBD_H_ */

/** @} */
