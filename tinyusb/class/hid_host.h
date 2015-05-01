/**************************************************************************/
/*!
    @file     hid_host.h
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

/** \addtogroup ClassDriver_HID
 *  @{ */

#ifndef _TUSB_HID_HOST_H_
#define _TUSB_HID_HOST_H_

#include "common/common.h"
#include "host/usbh.h"
#include "hid.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// KEYBOARD Application API
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Keyboard Keyboard
 *  @{ */

/** \defgroup Keyboard_Host Host
 *  The interface API includes status checking function, data transferring function and callback functions
 *  @{ */

extern uint8_t const hid_keycode_to_ascii_tbl[2][128]; // TODO used weak attr if build failed without KEYBOARD enabled

/** \brief      Check if device supports Keyboard interface or not
 * \param[in]   dev_addr    device address
 * \retval      true if device supports Keyboard interface
 * \retval      false if device does not support Keyboard interface or is not mounted
 */
bool          tuh_hid_keyboard_is_mounted(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief      Check if the interface is currently busy or not
 * \param[in]   dev_addr device address
 * \retval      true if the interface is busy meaning the stack is still transferring/waiting data from/to device
 * \retval      false if the interface is not busy meaning the stack successfully transferred data from/to device
 * \note        This function is primarily used for polling/waiting result after \ref tuh_hid_keyboard_get_report.
 *              Alternatively, asynchronous event API can be used
 */
bool          tuh_hid_keyboard_is_busy(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief        Perform a get report from Keyboard interface
 * \param[in]		  dev_addr device address
 * \param[in,out] p_report address that is used to store data from device. Must be accessible by usb controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \returns       \ref tusb_error_t type to indicate success or error condition.
 * \retval        TUSB_ERROR_NONE on success
 * \retval        TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval        TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval        TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note          This function is non-blocking and returns immediately. The result of usb transfer will be reported by the interface's callback function
 */
tusb_error_t  tuh_hid_keyboard_get_report(uint8_t dev_addr, void * p_report) /*ATTR_WARN_UNUSED_RESULT*/;

//------------- Application Callback -------------//
/** \brief      Callback function that is invoked when an transferring event occurred
 * \param[in]		dev_addr	Address of device
 * \param[in]   event an value from \ref tusb_event_t
 * \note        event can be one of following
 *              - TUSB_EVENT_XFER_COMPLETE : previously scheduled transfer completes successfully.
 *              - TUSB_EVENT_XFER_ERROR   : previously scheduled transfer encountered a transaction error.
 *              - TUSB_EVENT_XFER_STALLED : previously scheduled transfer is stalled by device.
 * \note        Application should schedule the next report by calling \ref tuh_hid_keyboard_get_report within this callback
 */
void tuh_hid_keyboard_isr(uint8_t dev_addr, tusb_event_t event);

/** \brief 			Callback function that will be invoked when a device with Keyboard interface is mounted
 * \param[in] 	dev_addr Address of newly mounted device
 * \note        This callback should be used by Application to set-up interface-related data
 */
void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr);

/** \brief 			Callback function that will be invoked when a device with Keyboard interface is unmounted
 * \param[in] 	dev_addr Address of newly unmounted device
 * \note        This callback should be used by Application to tear-down interface-related data
 */
void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr);

/** @} */ // Keyboard_Host
/** @} */ // ClassDriver_HID_Keyboard

//--------------------------------------------------------------------+
// MOUSE Application API
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Mouse Mouse
 *  @{ */

/** \defgroup Mouse_Host Host
 *  The interface API includes status checking function, data transferring function and callback functions
 *  @{ */

/** \brief      Check if device supports Mouse interface or not
 * \param[in]   dev_addr    device address
 * \retval      true if device supports Mouse interface
 * \retval      false if device does not support Mouse interface or is not mounted
 */
bool          tuh_hid_mouse_is_mounted(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief      Check if the interface is currently busy or not
 * \param[in]   dev_addr device address
 * \retval      true if the interface is busy meaning the stack is still transferring/waiting data from/to device
 * \retval      false if the interface is not busy meaning the stack successfully transferred data from/to device
 * \note        This function is primarily used for polling/waiting result after \ref tuh_hid_mouse_get_report.
 *              Alternatively, asynchronous event API can be used
 */
bool          tuh_hid_mouse_is_busy(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief        Perform a get report from Mouse interface
 * \param[in]		  dev_addr device address
 * \param[in,out] p_report address that is used to store data from device. Must be accessible by usb controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \returns       \ref tusb_error_t type to indicate success or error condition.
 * \retval        TUSB_ERROR_NONE on success
 * \retval        TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval        TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval        TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note          This function is non-blocking and returns immediately. The result of usb transfer will be reported by the interface's callback function
 */
tusb_error_t  tuh_hid_mouse_get_report(uint8_t dev_addr, void* p_report) /*ATTR_WARN_UNUSED_RESULT*/;

//------------- Application Callback -------------//
/** \brief      Callback function that is invoked when an transferring event occurred
 * \param[in]		dev_addr	Address of device
 * \param[in]   event an value from \ref tusb_event_t
 * \note        event can be one of following
 *              - TUSB_EVENT_XFER_COMPLETE : previously scheduled transfer completes successfully.
 *              - TUSB_EVENT_XFER_ERROR   : previously scheduled transfer encountered a transaction error.
 *              - TUSB_EVENT_XFER_STALLED : previously scheduled transfer is stalled by device.
 * \note        Application should schedule the next report by calling \ref tuh_hid_mouse_get_report within this callback
 */
void tuh_hid_mouse_isr(uint8_t dev_addr, tusb_event_t event);

/** \brief 			Callback function that will be invoked when a device with Mouse interface is mounted
 * \param[in]	  dev_addr Address of newly mounted device
 * \note        This callback should be used by Application to set-up interface-related data
 */
void tuh_hid_mouse_mounted_cb(uint8_t dev_addr);

/** \brief 			Callback function that will be invoked when a device with Mouse interface is unmounted
 * \param[in] 	dev_addr Address of newly unmounted device
 * \note        This callback should be used by Application to tear-down interface-related data
 */
void tuh_hid_mouse_unmounted_cb(uint8_t dev_addr);

/** @} */ // Mouse_Host
/** @} */ // ClassDriver_HID_Mouse

//--------------------------------------------------------------------+
// GENERIC Application API
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Generic Generic (not supported yet)
 *  @{ */

/** \defgroup Generic_Host Host
 *  The interface API includes status checking function, data transferring function and callback functions
 *  @{ */

bool          tuh_hid_generic_is_mounted(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;
tusb_error_t  tuh_hid_generic_get_report(uint8_t dev_addr, void* p_report, bool int_on_complete) ATTR_WARN_UNUSED_RESULT;
tusb_error_t  tuh_hid_generic_set_report(uint8_t dev_addr, void* p_report, bool int_on_complete) ATTR_WARN_UNUSED_RESULT;
tusb_interface_status_t tuh_hid_generic_get_status(uint8_t dev_addr) ATTR_WARN_UNUSED_RESULT;
tusb_interface_status_t tuh_hid_generic_set_status(uint8_t dev_addr) ATTR_WARN_UNUSED_RESULT;

//------------- Application Callback -------------//
void tuh_hid_generic_isr(uint8_t dev_addr, tusb_event_t event);

/** @} */ // Generic_Host
/** @} */ // ClassDriver_HID_Generic

//--------------------------------------------------------------------+
// USBH-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

typedef struct {
  pipe_handle_t pipe_hdl;
  uint16_t report_size;
  uint8_t interface_number;
}hidh_interface_info_t;

void         hidh_init(void);
tusb_error_t hidh_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length) ATTR_WARN_UNUSED_RESULT;
void         hidh_isr(pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes);
void         hidh_close(uint8_t dev_addr);

#endif

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_HID_HOST_H_ */

/** @} */ // ClassDriver_HID
