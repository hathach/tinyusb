/**************************************************************************/
/*!
    @file     msc_device.h
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

#ifndef _TUSB_MSC_DEVICE_H_
#define _TUSB_MSC_DEVICE_H_

#include "common/common.h"
#include "device/usbd.h"
#include "msc.h"

#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup ClassDriver_MSC
 *  @{
 * \defgroup MSC_Device Device
 *  @{ */

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// APPLICATION CALLBACK API
//--------------------------------------------------------------------+
/** \brief 			Callback function that will be invoked when this interface is mounted (configured) by USB host
 * \param[in] 	coreid USB Controller ID of the interface
 * \note        This callback should be used by Application to \b set-up interface-related data
 */
void tusbd_msc_mounted_cb(uint8_t coreid);

/** \brief 			Callback function that will be invoked when this interface is unmounted (bus reset/unplugged)
 * \param[in] 	coreid USB Controller ID of the interface
 * \note        This callback should be used by Application to \b tear-down interface-related data
 */
void tusbd_msc_unmounted_cb(uint8_t coreid);

/** \brief 			Callback that is invoked when tinyusb stack received \ref SCSI_CMD_READ_10 command from host
 * \param[in]		coreid	    USB Controller ID
 * \param[in]		lun         Targeted Logical Unit
 * \param[out]	pp_buffer   Pointer to buffer which application need to update with the response data's address.
 *                          Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \param[in]		lba         Starting Logical Block Address to be read
 * \param[in]		block_count Number of requested block
 * \retval      non-zero    Actual number of block that application processed, must be less than or equal to \a \b block_count.
 * \retval      zero        Indicate error in retrieving data from application. Tinyusb device stack will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 * \note        Host can request dozens of Kbytes in one command e.g \a \b block_count = 128, it is insufficient to require
 *              application to have that much of buffer. Instead, application can return the number of blocks it can processed,
 *              the stack after transferred that amount of data will continue to invoke this callback with adjusted \a \b lba and \a \b block_count.
 *              \n\n Although this callback is called by tinyusb device task (non-isr context), however as all the classes share
 *              the same task (to save resource), any delay in this callback will cause delay in reponse on other classes.
 */
uint16_t tusbd_msc_read10_cb (uint8_t coreid, uint8_t lun, void** pp_buffer, uint32_t lba, uint16_t block_count);

/** \brief 			Callback that is invoked when tinyusb stack received \ref SCSI_CMD_WRITE_10 command from host
 * \param[in]		coreid	    USB Controller ID
 * \param[in]		lun         Targeted Logical Unit
 * \param[out]	pp_buffer   Pointer to buffer which application need to update with the address to hold data from host
 *                          Must be accessible by USB controller (see \ref TUSB_CFG_ATTR_USBRAM)
 * \param[in]		lba         Starting Logical Block Address to be write
 * \param[in]		block_count Number of requested block
 * \retval      non-zero    Actual number of block that application can receive and must be less than or equal to \a \b block_count.
 * \retval      zero        Indicate error in retrieving data from application. Tinyusb device stack will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 * \note        Host can request dozens of Kbytes in one command e.g \a \b block_count = 128, it is insufficient to require
 *              application to have that much of buffer. Instead, application can return the number of blocks it can processed,
 *              the stack after transferred that amount of data will continue to invoke this callback with adjusted \a \b lba and \a \b block_count.
 *              \n\n Although this callback is called by tinyusb device task (non-isr context), however as all the classes share
 *              the same task (to save resource), any delay in this callback will cause delay in reponse on other classes.
 */
uint16_t tusbd_msc_write10_cb(uint8_t coreid, uint8_t lun, void** pp_buffer, uint32_t lba, uint16_t block_count);

// p_length [in,out] allocated/maximum length, application update with actual length
/** \brief 			Callback that is invoked when tinyusb stack received an SCSI command other than \ref SCSI_CMD_WRITE_10 and
 *              \ref SCSI_CMD_READ_10 command from host
 * \param[in]		coreid	    USB Controller ID
 * \param[in]		lun         Targeted Logical Unit
 * \param[in]		scsi_cmd    SCSI command contents, application should examine this command block to know which command host requested
 * \param[out]	pp_buffer   Pointer to buffer which application need to update with the address to transfer data with host.
 *                          The buffer address can be anywhere since the stack will copy its contents to a internal USB-accessible buffer.
 * \param[in]		p_length    length
 * \retval      non-zero    Actual number of block that application can receive and must be less than or equal to \a \b block_count.
 * \retval      zero        Indicate error in retrieving data from application. Tinyusb device stack will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 * \note        Although this callback is called by tinyusb device task (non-isr context), however as all the classes share
 *              the same task (to save resource), any delay in this callback will cause delay in reponse on other classes.
 */
msc_csw_status_t tusbd_msc_scsi_cb (uint8_t coreid, uint8_t lun, uint8_t scsi_cmd[16], void const ** pp_buffer, uint16_t* p_length);

/** @} */
/** @} */

//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void mscd_init(void);
tusb_error_t mscd_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length);
tusb_error_t mscd_control_request_subtask(uint8_t coreid, tusb_control_request_t const * p_request);
tusb_error_t mscd_xfer_cb(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes);
void mscd_close(uint8_t coreid);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSC_DEVICE_H_ */

