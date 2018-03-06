/**************************************************************************/
/*!
    @file     cdc_rndis_host.h
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

/** \ingroup CDC_RNDIS
 * \defgroup CDC_RNSID_Host Host
 *  @{ */

#ifndef _TUSB_CDC_RNDIS_HOST_H_
#define _TUSB_CDC_RNDIS_HOST_H_

#include "common/common.h"
#include "host/usbh.h"
#include "cdc_rndis.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// RNDIS-CDC Driver API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

typedef struct {
  OSAL_SEM_DEF(semaphore_notification);
  osal_semaphore_handle_t sem_notification_hdl;  // used to wait on notification pipe
  uint32_t max_xfer_size; // got from device's msg initialize complete
  uint8_t mac_address[6];
}rndish_data_t;

void rndish_init(void);
tusb_error_t rndish_open_subtask(uint8_t dev_addr, cdch_data_t *p_cdc) ATTR_WARN_UNUSED_RESULT;
void rndish_xfer_isr(cdch_data_t *p_cdc, pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes);
void rndish_close(uint8_t dev_addr);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_RNDIS_HOST_H_ */

/** @} */
