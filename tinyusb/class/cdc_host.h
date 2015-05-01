/**************************************************************************/
/*!
    @file     cdc_host.h
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


#ifndef _TUSB_CDC_HOST_H_
#define _TUSB_CDC_HOST_H_

#include "common/common.h"
#include "host/usbh.h"
#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// CDC APPLICATION PUBLIC API
//--------------------------------------------------------------------+
/** \ingroup ClassDriver_CDC Communication Device Class (CDC)
 * \addtogroup CDC_Serial Serial
 * @{
 * \defgroup   CDC_Serial_Host Host
 * @{ */

/** \brief 			Check if device support CDC Serial interface or not
 * \param[in]		dev_addr	device address
 * \retval      true if device supports
 * \retval      false if device does not support or is not mounted
 */
bool tuh_cdc_serial_is_mounted(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief      Check if the interface is currently busy or not
 * \param[in]   dev_addr device address
 * \param[in]   pipeid value from \ref cdc_pipeid_t to indicate target pipe.
 * \retval      true if the interface is busy, meaning the stack is still transferring/waiting data from/to device
 * \retval      false if the interface is not busy, meaning the stack successfully transferred data from/to device
 * \note        This function is used to check if previous transfer is complete (success or error), so that the next transfer
 *              can be scheduled. User needs to make sure the corresponding interface is mounted
 *              (by \ref tuh_cdc_serial_is_mounted) before calling this function.
 */
bool tuh_cdc_is_busy(uint8_t dev_addr, cdc_pipeid_t pipeid)  ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief 			Perform USB OUT transfer to device
 * \param[in]		dev_addr	device address
 * \param[in]	  p_data    Buffer containing data. Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \param[in]		length    Number of bytes to be transferred via USB bus
 * \retval      TUSB_ERROR_NONE on success
 * \retval      TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval      TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval      TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note        This function is non-blocking and returns immediately. The result of USB transfer will be reported by the
 *              interface's callback function. \a p_data must be declared with \ref TUSB_CFG_ATTR_USBRAM.
 */
tusb_error_t tuh_cdc_send(uint8_t dev_addr, void const * p_data, uint32_t length, bool is_notify);

/** \brief 			Perform USB IN transfer to get data from device
 * \param[in]		dev_addr	device address
 * \param[in]	  p_buffer  Buffer containing received data. Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \param[in]		length    Number of bytes to be transferred via USB bus
 * \retval      TUSB_ERROR_NONE on success
 * \retval      TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval      TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval      TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note        This function is non-blocking and returns immediately. The result of USB transfer will be reported by the
 *              interface's callback function. \a p_data must be declared with \ref TUSB_CFG_ATTR_USBRAM.
 */
tusb_error_t tuh_cdc_receive(uint8_t dev_addr, void * p_buffer, uint32_t length, bool is_notify);

//--------------------------------------------------------------------+
// CDC APPLICATION CALLBACKS
//--------------------------------------------------------------------+
/** \brief 			Callback function that will be invoked when a device with CDC Abstract Control Model interface is mounted
 * \param[in]	  dev_addr Address of newly mounted device
 * \note        This callback should be used by Application to set-up interface-related data
 */
void tuh_cdc_mounted_cb(uint8_t dev_addr);

/** \brief 			Callback function that will be invoked when a device with CDC Abstract Control Model interface is unmounted
 * \param[in] 	dev_addr Address of newly unmounted device
 * \note        This callback should be used by Application to tear-down interface-related data
 */
void tuh_cdc_unmounted_cb(uint8_t dev_addr);

/** \brief      Callback function that is invoked when an transferring event occurred
 * \param[in]		dev_addr	Address of device
 * \param[in]   event an value from \ref tusb_event_t
 * \param[in]   pipe_id value from \ref cdc_pipeid_t indicate the pipe
 * \param[in]   xferred_bytes Number of bytes transferred via USB bus
 * \note        event can be one of following
 *              - TUSB_EVENT_XFER_COMPLETE : previously scheduled transfer completes successfully.
 *              - TUSB_EVENT_XFER_ERROR   : previously scheduled transfer encountered a transaction error.
 *              - TUSB_EVENT_XFER_STALLED : previously scheduled transfer is stalled by device.
 * \note
 */
void tuh_cdc_xfer_isr(uint8_t dev_addr, tusb_event_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes);

/// @} // group CDC_Serial_Host
/// @}

//--------------------------------------------------------------------+
// USBH-CLASS API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

typedef struct {
  uint8_t interface_number;
  uint8_t interface_protocol;

  cdc_acm_capability_t acm_capability;

  pipe_handle_t pipe_notification, pipe_out, pipe_in;

} cdch_data_t;

extern cdch_data_t cdch_data[TUSB_CFG_HOST_DEVICE_MAX]; // TODO consider to move to cdch internal header file

void         cdch_init(void);
tusb_error_t cdch_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length) ATTR_WARN_UNUSED_RESULT;
void         cdch_isr(pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes);
void         cdch_close(uint8_t dev_addr);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_HOST_H_ */
