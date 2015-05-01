/**************************************************************************/
/*!
    @file     msc_host.h
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

#ifndef _TUSB_MSC_HOST_H_
#define _TUSB_MSC_HOST_H_

#include "common/common.h"
#include "host/usbh.h"
#include "msc.h"

#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup ClassDriver_MSC
 *  @{
 * \defgroup MSC_Host Host
 *  The interface API includes status checking function, data transferring function and callback functions
 *  @{ */
//--------------------------------------------------------------------+
// MASS STORAGE Application API
//--------------------------------------------------------------------+
/** \brief      Check if device supports MassStorage interface or not
 * \param[in]   dev_addr    device address
 * \retval      true if device supports
 * \retval      false if device does not support or is not mounted
 */
bool          tuh_msc_is_mounted(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief      Check if the interface is currently busy or not
 * \param[in]   dev_addr device address
 * \retval      true if the interface is busy meaning the stack is still transferring/waiting data from/to device
 * \retval      false if the interface is not busy meaning the stack successfully transferred data from/to device
 * \note        This function is used to check if previous transfer is complete (success or error), so that the next transfer
 *              can be scheduled. User needs to make sure the corresponding interface is mounted (by \ref tuh_msc_is_mounted)
 *              before calling this function
 */
bool          tuh_msc_is_busy(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;

/** \brief      Get SCSI vendor's name of MassStorage device
 * \param[in]   dev_addr device address
 * \return      pointer to vendor's name or NULL if specified device does not support MassStorage
 * \note        SCSI vendor's name is 8-byte length field in \ref scsi_inquiry_data_t. During enumeration, the stack has already
 *              retrieved (via SCSI INQUIRY) and store this information internally. There is no need for application to re-send SCSI INQUIRY
 *              command or allocate buffer for this.
 */
uint8_t const* tuh_msc_get_vendor_name(uint8_t dev_addr);

/** \brief      Get SCSI product's name of MassStorage device
 * \param[in]   dev_addr device address
 * \return      pointer to product's name or NULL if specified device does not support MassStorage
 * \note        SCSI product's name is 16-byte length field in \ref scsi_inquiry_data_t. During enumeration, the stack has already
 *              retrieved (via SCSI INQUIRY) and store this information internally. There is no need for application to re-send SCSI INQUIRY
 *              command or allocate buffer for this.
 */
uint8_t const* tuh_msc_get_product_name(uint8_t dev_addr);

/** \brief      Get SCSI Capacity of MassStorage device
 * \param[in]   dev_addr device address
 * \param[out]  p_last_lba Last Logical Block Address of device
 * \param[out]  p_block_size Block Size of device in bytes
 * \retval      pointer to product's name or NULL if specified device does not support MassStorage
 * \note        MassStorage's capacity can be computed by last LBA x block size (in bytes). During enumeration, the stack has already
 *              retrieved (via SCSI READ CAPACITY 10) and store this information internally. There is no need for application
 *              to re-send SCSI READ CAPACITY 10 command
 */
tusb_error_t tuh_msc_get_capacity(uint8_t dev_addr, uint32_t* p_last_lba, uint32_t* p_block_size);

/** \brief 			Perform SCSI READ 10 command to read data from MassStorage device
 * \param[in]		dev_addr	device address
 * \param[in]		lun       Targeted Logical Unit
 * \param[out]	p_buffer  Buffer used to store data read from device. Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \param[in]		lba       Starting Logical Block Address to be read
 * \param[in]		block_count Number of Block to be read
 * \retval      TUSB_ERROR_NONE on success
 * \retval      TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval      TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval      TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note        This function is non-blocking and returns immediately. The result of USB transfer will be reported by the interface's callback function
 */
tusb_error_t tuh_msc_read10 (uint8_t dev_addr, uint8_t lun, void * p_buffer, uint32_t lba, uint16_t block_count) ATTR_WARN_UNUSED_RESULT;

/** \brief 			Perform SCSI WRITE 10 command to write data to MassStorage device
 * \param[in]		dev_addr	device address
 * \param[in]		lun       Targeted Logical Unit
 * \param[in]	  p_buffer  Buffer containing data. Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \param[in]		lba       Starting Logical Block Address to be written
 * \param[in]		block_count Number of Block to be written
 * \retval      TUSB_ERROR_NONE on success
 * \retval      TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval      TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval      TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note        This function is non-blocking and returns immediately. The result of USB transfer will be reported by the interface's callback function
 */
tusb_error_t tuh_msc_write10(uint8_t dev_addr, uint8_t lun, void const * p_buffer, uint32_t lba, uint16_t block_count) ATTR_WARN_UNUSED_RESULT;

/** \brief 			Perform SCSI REQUEST SENSE command, used to retrieve sense data from MassStorage device
 * \param[in]		dev_addr	device address
 * \param[in]		lun       Targeted Logical Unit
 * \param[in]	  p_data    Buffer to store response's data from device. Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \retval      TUSB_ERROR_NONE on success
 * \retval      TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval      TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval      TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note        This function is non-blocking and returns immediately. The result of USB transfer will be reported by the interface's callback function
 */
tusb_error_t tuh_msc_request_sense(uint8_t dev_addr, uint8_t lun, uint8_t *p_data);

/** \brief 			Perform SCSI TEST UNIT READY command to test if MassStorage device is ready
 * \param[in]		dev_addr	device address
 * \param[in]		lun       Targeted Logical Unit
 * \retval      TUSB_ERROR_NONE on success
 * \retval      TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval      TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval      TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note        This function is non-blocking and returns immediately. The result of USB transfer will be reported by the interface's callback function
 */
tusb_error_t tuh_msc_test_unit_ready(uint8_t dev_addr, uint8_t lun, msc_cmd_status_wrapper_t * p_csw) ATTR_WARN_UNUSED_RESULT; // TODO to be refractor

//tusb_error_t  tusbh_msc_scsi_send(uint8_t dev_addr, uint8_t lun, bool is_direction_in,
//                                  uint8_t const * p_command, uint8_t cmd_len,
//                                  uint8_t * p_response, uint32_t resp_len) ATTR_WARN_UNUSED_RESULT;

//------------- Application Callback -------------//
/** \brief 			Callback function that will be invoked when a device with MassStorage interface is mounted
 * \param[in]	  dev_addr Address of newly mounted device
 * \note        This callback should be used by Application to set-up interface-related data
 */
void tuh_msc_mounted_cb(uint8_t dev_addr);

/** \brief 			Callback function that will be invoked when a device with MassStorage interface is unmounted
 * \param[in] 	dev_addr Address of newly unmounted device
 * \note        This callback should be used by Application to tear-down interface-related data
 */
void tuh_msc_unmounted_cb(uint8_t dev_addr);

/** \brief      Callback function that is invoked when an transferring event occurred
 * \param[in]		dev_addr	Address of device
 * \param[in]   event an value from \ref tusb_event_t
 * \param[in]   xferred_bytes Number of bytes transferred via USB bus
 * \note        event can be one of following
 *              - TUSB_EVENT_XFER_COMPLETE : previously scheduled transfer completes successfully.
 *              - TUSB_EVENT_XFER_ERROR   : previously scheduled transfer encountered a transaction error.
 *              - TUSB_EVENT_XFER_STALLED : previously scheduled transfer is stalled by device.
 * \note
 */
void tuh_msc_isr(uint8_t dev_addr, tusb_event_t event, uint32_t xferred_bytes);


//--------------------------------------------------------------------+
// USBH-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

typedef struct {
  pipe_handle_t bulk_in, bulk_out;
  uint8_t  interface_number;

  uint8_t  max_lun;
  uint16_t block_size;
  uint32_t last_lba; // last logical block address

  volatile bool is_initialized;
  uint8_t vendor_id[8];
  uint8_t product_id[16];

  msc_cmd_block_wrapper_t cbw;
  msc_cmd_status_wrapper_t csw;
}msch_interface_t;

void         msch_init(void);
tusb_error_t msch_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length) ATTR_WARN_UNUSED_RESULT;
void         msch_isr(pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes);
void         msch_close(uint8_t dev_addr);
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSC_HOST_H_ */

/// @}
/// @}
