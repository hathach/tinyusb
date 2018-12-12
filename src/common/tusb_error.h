/**************************************************************************/
/*!
    @file     tusb_error.h
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

/** \ingroup Group_Common
 *  \defgroup Group_Error Error Codes
 *  @{ */

#ifndef _TUSB_ERRORS_H_
#define _TUSB_ERRORS_H_

#include "tusb_option.h"

#ifdef __cplusplus
 extern "C" {
#endif

#define ERROR_ENUM(x) x,
#define ERROR_STRING(x) #x,

#define ERROR_TABLE(ENTRY) \
    ENTRY(TUSB_ERROR_NONE                            )\
    ENTRY(TUSB_ERROR_INVALID_PARA                    )\
    ENTRY(TUSB_ERROR_DEVICE_NOT_READY                )\
    ENTRY(TUSB_ERROR_INTERFACE_IS_BUSY               )\
    ENTRY(TUSB_ERROR_HCD_FAILED                      )\
    ENTRY(TUSB_ERROR_HCD_OPEN_PIPE_FAILED            )\
    ENTRY(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND   )\
    ENTRY(TUSB_ERROR_USBH_MOUNT_CONFIG_DESC_TOO_LONG )\
    ENTRY(TUSB_ERROR_USBH_DESCRIPTOR_CORRUPTED       )\
    ENTRY(TUSB_ERROR_USBH_XFER_STALLED               )\
    ENTRY(TUSB_ERROR_USBH_XFER_FAILED                )\
    ENTRY(TUSB_ERROR_OSAL_TIMEOUT                    )\
    ENTRY(TUSB_ERROR_OSAL_WAITING                    ) /* only used by OSAL_NONE in the subtask */ \
    ENTRY(TUSB_ERROR_OSAL_TASK_FAILED                )\
    ENTRY(TUSB_ERROR_OSAL_QUEUE_FAILED               )\
    ENTRY(TUSB_ERROR_OSAL_SEMAPHORE_FAILED           )\
    ENTRY(TUSB_ERROR_OSAL_MUTEX_FAILED               )\
    ENTRY(TUSB_ERROR_EHCI_NOT_ENOUGH_QTD             )\
    ENTRY(TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE       )\
    ENTRY(TUSB_ERROR_HIDH_NOT_SUPPORTED_PROTOCOL     )\
    ENTRY(TUSB_ERROR_HIDH_NOT_SUPPORTED_SUBCLASS     )\
    ENTRY(TUSB_ERROR_CDC_UNSUPPORTED_SUBCLASS       )\
    ENTRY(TUSB_ERROR_CDC_UNSUPPORTED_PROTOCOL       )\
    ENTRY(TUSB_ERROR_CDCH_DEVICE_NOT_MOUNTED         )\
    ENTRY(TUSB_ERROR_MSC_UNSUPPORTED_PROTOCOL       )\
    ENTRY(TUSB_ERROR_MSCH_UNKNOWN_SCSI_COMMAND       )\
    ENTRY(TUSB_ERROR_MSCH_DEVICE_NOT_MOUNTED         )\
    ENTRY(TUSB_ERROR_HUB_FEATURE_NOT_SUPPORTED       )\
    ENTRY(TUSB_ERROR_DESCRIPTOR_CORRUPTED            )\
    ENTRY(TUSB_ERROR_DCD_FAILED                      )\
    ENTRY(TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT )\
    ENTRY(TUSB_ERROR_DCD_NOT_ENOUGH_QTD              )\
    ENTRY(TUSB_ERROR_DCD_OPEN_PIPE_FAILED            )\
    ENTRY(TUSB_ERROR_DCD_EDPT_XFER                   )\
    ENTRY(TUSB_ERROR_NOT_SUPPORTED_YET               )\
    ENTRY(TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED      )\
    ENTRY(TUSB_ERROR_NOT_ENOUGH_MEMORY               )\
    ENTRY(TUSB_ERROR_FAILED                          )\
    \
    ENTRY(ERR_TUD_INVALID_DESCRIPTOR) \
    ENTRY(ERR_TUD_EDPT_OPEN_FAILED) \


/// \brief Error Code returned
typedef enum
{
  ERROR_TABLE(ERROR_ENUM)
  TUSB_ERROR_COUNT
}tusb_error_t;

#if CFG_TUSB_DEBUG
/// Enum to String for debugging purposes. Only available if \ref CFG_TUSB_DEBUG > 0
extern char const* const tusb_strerr[TUSB_ERROR_COUNT];
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_ERRORS_H_ */

/**  @} */
