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

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
/** \brief      Check if the interface is configured and ready to use
 * \param[in]   coreid USB Controller ID
 * \retval      true if the interface is configured
 * \retval      false if the interface is not configured (e.g device is not attached)
 * \note        This function should be call frequently or before any xfer attempt to check if device is still attached
 */
bool tusbd_msc_is_configured(uint8_t coreid);

//--------------------------------------------------------------------+
// APPLICATION CALLBACK API
//--------------------------------------------------------------------+
void tusbd_msc_mounted_cb(uint8_t coreid);
void tusbd_msc_unmounted_cb(uint8_t coreid);

// p_length [in,out] allocated/maximum length, application update with actual length
msc_csw_status_t tusbd_msc_scsi_received_isr (uint8_t coreid, uint8_t lun, uint8_t scsi_cmd[16], void ** pp_buffer, uint16_t* p_length);

tusb_error_t tusbd_msc_read10 (uint8_t dev_addr, uint8_t lun, void * p_buffer, uint32_t lba, uint16_t block_count) ATTR_WARN_UNUSED_RESULT;
tusb_error_t tusbh_msc_write10(uint8_t dev_addr, uint8_t lun, void const * p_buffer, uint32_t lba, uint16_t block_count) ATTR_WARN_UNUSED_RESULT;

//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void mscd_init(void);
tusb_error_t mscd_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length);
tusb_error_t mscd_control_request(uint8_t coreid, tusb_control_request_t const * p_request);
void mscd_isr(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes);
void mscd_close(uint8_t coreid);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSC_DEVICE_H_ */

/** @} */
