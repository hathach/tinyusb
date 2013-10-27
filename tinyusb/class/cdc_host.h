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

/** \addtogroup ClassDriver_CDC Communication Device Class (CDC)
 *  @{ */

#ifndef _TUSB_CDC_HOST_H_
#define _TUSB_CDC_HOST_H_

#include "common/common.h"
#include "host/usbh.h"
#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef enum {
  CDC_PIPE_ERROR = 0,
  CDC_PIPE_NOTIFICATION,
  CDC_PIPE_DATA_IN,
  CDC_PIPE_DATA_OUT
}cdc_pipeid_t;

//--------------------------------------------------------------------+
// APPLICATION PUBLIC API
//--------------------------------------------------------------------+
/** \defgroup CDC_ACM Abtract Control Model (ACM)
 *  @{ */

bool tusbh_cdc_serial_is_mounted(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;
tusb_error_t tusbh_cdc_send(uint8_t dev_addr, void const * p_data, uint32_t length, bool is_notify);
tusb_error_t tusbh_cdc_receive(uint8_t dev_addr, void * p_buffer, uint32_t length, bool is_notify);

//------------- CDC Application Callback -------------//
/** \brief 			Callback function that will be invoked when a device with CDC Abstract Control Model interface is mounted
 * \param[in]	  dev_addr Address of newly mounted device
 * \note        This callback should be used by Application to set-up interface-related data
 */
void tusbh_cdc_mounted_cb(uint8_t dev_addr);

/** \brief 			Callback function that will be invoked when a device with CDC Abstract Control Model interface is unmounted
 * \param[in] 	dev_addr Address of newly unmounted device
 * \note        This callback should be used by Application to tear-down interface-related data
 */
void tusbh_cdc_unmounted_cb(uint8_t dev_addr);

void tusbh_cdc_xfer_isr(uint8_t dev_addr, tusb_event_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes);

/// @}

//--------------------------------------------------------------------+
// RNDIS APPLICATION API
//--------------------------------------------------------------------+
/** \addtogroup CDC_RNDIS Remote Network Driver Interface Specification (RNDIS)
 * @{
 * \addtogroup CDC_RNSID_Host Host
 *  @{ */

bool tusbh_cdc_rndis_is_mounted(uint8_t dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;
tusb_error_t tusbh_cdc_rndis_get_mac_addr(uint8_t dev_addr, uint8_t mac_address[6]);

//------------- RNDIS Application Callback (overshadow CDC callbacks) -------------//
/** \brief 			Callback function that will be invoked when a device with RNDIS interface is mounted
 * \param[in]	  dev_addr Address of newly mounted device
 * \note        This callback should be used by Application to set-up interface-related data
 */
void tusbh_cdc_rndis_mounted_cb(uint8_t dev_addr);

/** \brief 			Callback function that will be invoked when a device with RNDIS interface is unmounted
 * \param[in] 	dev_addr Address of newly unmounted device
 * \note        This callback should be used by Application to tear-down interface-related data
 */
void tusbh_cdc_rndis_unmounted_cb(uint8_t dev_addr);

void tusbh_cdc_rndis_xfer_isr(uint8_t dev_addr, tusb_event_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes);

/// @}
/// @}

//--------------------------------------------------------------------+
// USBH-CLASS API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

typedef struct {
  uint8_t interface_number;
  uint8_t interface_protocol;
  bool is_rndis;
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

/** @} */
