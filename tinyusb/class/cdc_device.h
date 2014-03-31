/**************************************************************************/
/*!
    @file     cdc_device.h
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

#ifndef _TUSB_CDC_DEVICE_H_
#define _TUSB_CDC_DEVICE_H_

#include "common/common.h"
#include "device/usbd.h"
#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup CDC_Serial Serial
 *  @{
 *  \defgroup   CDC_Serial_Device Device
 *  @{ */

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
/** \brief      Check if the interface is currently busy or not
 * \param[in]   coreid USB Controller ID
 * \retval      true if the interface is busy meaning the stack is still transferring/waiting data from/to host
 * \retval      false if the interface is not busy meaning the stack successfully transferred data from/to host
 * \note        This function is primarily used for polling/waiting result after \ref tusbd_hid_keyboard_send.
 */
bool tusbd_cdc_is_busy(uint8_t coreid, cdc_pipeid_t pipeid)  ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief        Submit USB transfer
 * \param[in]		  coreid USB Controller ID
 * \param[in]     p_data  buffer containing data from application. Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \param[in]     length number of bytes in \a p_data.
 * \param[in]     is_notify indicates whether the hardware completion (data transferred through USB bus) will be notified
 *                to Application (via \ref tusbd_cdc_xfer_cb)
 * \returns       \ref tusb_error_t type to indicate success or error condition.
 * \retval        TUSB_ERROR_NONE on success
 * \retval        TUSB_ERROR_INTERFACE_IS_BUSY if the interface is busy transferring previous data.
 * \retval        TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval        TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note          This function is non-blocking and returns immediately. Data will be transferred when USB Host work with this interface.
 *                The result of usb transfer will be reported by the interface's callback function if \a is_notify is true
 */
tusb_error_t tusbd_cdc_send(uint8_t coreid, void * p_data, uint32_t length, bool is_notify);

/** \brief        Submit USB transfer
 * \param[in]		  coreid USB Controller ID
 * \param[in]     p_buffer  application's buffer to receive data. Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \param[in]     length number of bytes in \a p_buffer.
 * \param[in]     is_notify indicates whether the hardware completion (data transferred through USB bus) will be notified
 *                to Application (via \ref tusbd_cdc_xfer_cb)
 * \returns       \ref tusb_error_t type to indicate success or error condition.
 * \retval        TUSB_ERROR_NONE on success
 * \retval        TUSB_ERROR_INTERFACE_IS_BUSY if the interface is busy transferring previous data.
 * \retval        TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval        TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note          This function is non-blocking and returns immediately. Data will be transferred when USB Host work with this interface.
 *                The result of usb transfer will be reported by the interface's callback function if \a is_notify is true
 */
tusb_error_t tusbd_cdc_receive(uint8_t coreid, void * p_buffer, uint32_t length, bool is_notify);

//--------------------------------------------------------------------+
// APPLICATION CALLBACK API
//--------------------------------------------------------------------+
/** \brief 			Callback function that will be invoked when this interface is mounted (configured) by USB host
 * \param[in] 	coreid USB Controller ID of the interface
 * \note        This callback should be used by Application to \b set-up interface-related data
 */
void tusbd_cdc_mounted_cb(uint8_t coreid);

/** \brief 			Callback function that will be invoked when this interface is unmounted (bus reset/unplugged)
 * \param[in] 	coreid USB Controller ID of the interface
 * \note        This callback should be used by Application to \b tear-down interface-related data
 */
void tusbd_cdc_unmounted_cb(uint8_t coreid);

/** \brief      Callback function that is invoked when an completion (error or success) of an USB transfer previously submitted
 *              by application (e.g \ref tusbd_cdc_send or \ref tusbd_cdc_send) with \a is_notify set to true.
 * \param[in]		coreid	USB Controller ID
 * \param[in]   event an value from \ref tusb_event_t
 * \param[in]   pipe_id indicates which pipe of this interface the event occured.
 * \param[in]   xferred_bytes is actual number of bytes transferred via USB bus. This value in general can be different to
 *              the one that previously submitted by application.
 */
void tusbd_cdc_xfer_cb(uint8_t coreid, tusb_event_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes);
//void tusbd_cdc_line_coding_changed_cb(uint8_t coreid, cdc_line_coding_t* p_line_coding);

//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void cdcd_init(void);
tusb_error_t cdcd_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length);
tusb_error_t cdcd_control_request_subtask(uint8_t coreid, tusb_control_request_t const * p_request);
tusb_error_t cdcd_xfer_cb(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes);
void cdcd_close(uint8_t coreid);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_DEVICE_H_ */

/** @} */
/** @} */
