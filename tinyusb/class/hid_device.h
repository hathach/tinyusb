/**************************************************************************/
/*!
    @file     hid_device.h
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

#ifndef _TUSB_HID_DEVICE_H_
#define _TUSB_HID_DEVICE_H_

#include "common/common.h"
#include "device/usbd.h"
#include "hid.h"

#ifdef __cplusplus
 extern "C" {
#endif


//--------------------------------------------------------------------+
// KEYBOARD Application API
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Keyboard Keyboard
 *  @{ */
/** \defgroup Keyboard_Device Device
 *  @{ */

/** \brief      Check if the interface is currently busy or not
 * \param[in]   coreid USB Controller ID
 * \retval      true if the interface is busy meaning the stack is still transferring/waiting data from/to host
 * \retval      false if the interface is not busy meaning the stack successfully transferred data from/to host
 * \note        This function is primarily used for polling/waiting result after \ref tusbd_hid_keyboard_send.
 */
bool tusbd_hid_keyboard_is_busy(uint8_t coreid);

/** \brief        Perform transfer queuing
 * \param[in]		  coreid USB Controller ID
 * \param[in,out] p_report address that is used to store data from device. Must be accessible by usb controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \returns       \ref tusb_error_t type to indicate success or error condition.
 * \retval        TUSB_ERROR_NONE on success
 * \retval        TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval        TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval        TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note          This function is non-blocking and returns immediately. Data will be transferred when USB Host work with this interface.
 *                The result of usb transfer will be reported by the interface's callback function
 */
tusb_error_t tusbd_hid_keyboard_send(uint8_t coreid, hid_keyboard_report_t const *p_report);

//------------- Application Callback -------------//
/** \brief      Callback function that is invoked when an transferring event occurred
 * \param[in]		coreid	USB Controller ID
 * \param[in]   event an value from \ref tusb_event_t
 * \note        event can be one of following
 *              - TUSB_EVENT_XFER_COMPLETE : previously scheduled transfer completes successfully.
 *              - TUSB_EVENT_XFER_ERROR   : previously scheduled transfer encountered a transaction error.
 *              - TUSB_EVENT_XFER_STALLED : previously scheduled transfer is stalled by device.
 * \note        Application should schedule the next report by calling \ref tusbh_hid_keyboard_get_report within this callback
 */
void tusbd_hid_keyboard_isr(uint8_t coreid, tusb_event_t event);

/** @} */
/** @} */

//--------------------------------------------------------------------+
// MOUSE APPLICATION API
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Mouse Mouse
 *  @{ */
/** \defgroup Mouse_Device Device
 *  @{ */

bool tusbd_hid_mouse_is_busy(uint8_t coreid);

tusb_error_t tusbd_hid_mouse_send(uint8_t coreid, hid_mouse_report_t const *p_report);

/** @} */
/** @} */



//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

tusb_error_t hidd_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length);
tusb_error_t hidd_control_request(uint8_t coreid, tusb_control_request_t const * p_request);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HID_DEVICE_H_ */


