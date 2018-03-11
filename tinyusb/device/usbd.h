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
#include "common/common.h"
#include "osal/osal.h"
#include "dcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
// LPC11uxx and LPC13uxx requires each buffer has to be 64-byte alignment
#if TUSB_CFG_MCU == MCU_LPC11UXX || TUSB_CFG_MCU == MCU_LPC13UXX
 #define ATTR_USB_MIN_ALIGNMENT   ATTR_ALIGNED(64)
#else
 #define ATTR_USB_MIN_ALIGNMENT
#endif

/// \brief Descriptor pointer collector to all the needed.
typedef struct {
  uint8_t const * p_device;              ///< pointer to device descritpor \ref tusb_descriptor_device_t
  uint8_t const * p_configuration;       ///< pointer to the whole configuration descriptor, starting by \ref tusb_descriptor_configuration_t
  uint8_t const** p_string_arr;          ///< a array of pointers to string descriptors

  uint8_t const * p_hid_keyboard_report; ///< pointer to HID report descriptor of Keybaord interface. Only needed if TUSB_CFG_DEVICE_HID_KEYBOARD is enabled
  uint8_t const * p_hid_mouse_report;    ///< pointer to HID report descriptor of Mouse interface. Only needed if TUSB_CFG_DEVICE_HID_MOUSE is enabled
}tusbd_descriptor_pointer_t;

// define by application
extern tusbd_descriptor_pointer_t tusbd_descriptor_pointers;

typedef struct {
  void (* init) (void);
  tusb_error_t (* open)(uint8_t, tusb_descriptor_interface_t const *, uint16_t*);
  tusb_error_t (* control_request_subtask) (uint8_t port, tusb_control_request_t const *);
  tusb_error_t (* xfer_cb) (edpt_hdl_t, tusb_event_t, uint32_t);
//  void (* routine)(void);
  void (* sof)(uint8_t port);
  void (* close) (uint8_t);
} usbd_class_driver_t;


enum {
  USBD_INTERFACE_NUM_MAX = 16 // USB specs specify up to 16 endpoints per device
};


typedef struct {
  volatile uint8_t state;
  uint8_t interface2class[USBD_INTERFACE_NUM_MAX]; // determine interface number belongs to which class
}usbd_device_info_t;

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_mounted(uint8_t port);

/*------------- Callback -------------*/
/** \brief 			Callback function that will be invoked device is mounted (configured) by USB host
 * \param[in] 	port USB Controller ID of the interface
 * \note        This callback should be used by Application to \b set-up application data
 */
void tud_mount_cb(uint8_t port);

/** \brief 			Callback function that will be invoked when device is unmounted (bus reset/unplugged)
 * \param[in] 	port USB Controller ID of the interface
 * \note        This callback should be used by Application to \b tear-down application data
 */
void tud_umount_cb(uint8_t port);

//void tud_device_suspended_cb(uint8_t port);

//--------------------------------------------------------------------+
// CLASS-USBD & INTERNAL API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

extern osal_semaphore_t usbd_control_xfer_sem_hdl;

tusb_error_t usbd_init(void);
void usbd_task( void* param);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBD_H_ */

/** @} */
