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

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "msc.h"


//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+
TU_VERIFY_STATIC(CFG_TUD_MSC_BUFSIZE < UINT16_MAX, "Size is not correct");

#ifndef CFG_TUD_MSC_MAXLUN
  #define CFG_TUD_MSC_MAXLUN 1
#elif CFG_TUD_MSC_MAXLUN == 0 || CFG_TUD_MSC_MAXLUN > 16
  #error MSC Device: Incorrect setting of MAX LUN
#endif

#ifndef CFG_TUD_MSC_BLOCK_NUM
  #error CFG_TUD_MSC_BLOCK_NUM must be defined
#endif

#ifndef CFG_TUD_MSC_BLOCK_SZ
  #error CFG_TUD_MSC_BLOCK_SZ must be defined
#endif

#ifndef CFG_TUD_MSC_BUFSIZE
  #error CFG_TUD_MSC_BUFSIZE must be defined, value of CFG_TUD_MSC_BLOCK_SZ should work well, the more the better
#endif

#ifndef CFG_TUD_MSC_VENDOR
  #error CFG_TUD_MSC_VENDOR 8-byte name must be defined
#endif

#ifndef CFG_TUD_MSC_PRODUCT
  #error CFG_TUD_MSC_PRODUCT 16-byte name must be defined
#endif

#ifndef CFG_TUD_MSC_PRODUCT_REV
  #error CFG_TUD_MSC_PRODUCT_REV 4-byte string must be defined
#endif

// TODO highspeed device is 512
#ifndef CFG_TUD_MSC_EPSIZE
#define CFG_TUD_MSC_EPSIZE 64
#endif


#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup ClassDriver_MSC
 *  @{
 * \defgroup MSC_Device Device
 *  @{ */


// Check if MSC interface is ready to use
bool tud_msc_ready(void);
bool tud_msc_set_sense(uint8_t lun, uint8_t sense_key, uint8_t add_sense_code, uint8_t add_sense_qualifier);

//--------------------------------------------------------------------+
// APPLICATION CALLBACK (WEAK is optional)
//--------------------------------------------------------------------+
/**
 * Callback invoked when received \ref SCSI_CMD_READ_10 command
 * \param[in]   lun         Logical unit number
 * \param[in]   lba         Logical Block Address to be read
 * \param[in]   offset      Byte offset from LBA
 * \param[out]  buffer      Buffer which application need to update with the response data.
 * \param[in]   bufsize     Requested bytes
 *
 * \return      Number of byte read, if it is less than requested bytes by \a \b bufsize. Tinyusb will transfer
 *              this amount first and invoked this again for remaining data.
 *
 * \retval      zero        Indicate application is not ready yet to response e.g disk I/O is not complete.
 *                          tinyusb will invoke this callback with the same parameters again some time later.
 *
 * \retval      negative    Indicate error e.g reading disk I/O. tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 */
int32_t tud_msc_read10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);

/**
 * Callback invoked when received \ref SCSI_CMD_WRITE_10 command
 * \param[in]   lun         Logical unit number
 * \param[in]   lba         Logical Block Address to be write
 * \param[in]   offset      Byte offset from LBA
 * \param[out]  buffer      Buffer which holds written data.
 * \param[in]   bufsize     Requested bytes
 *
 * \return      Number of byte written, if it is less than requested bytes by \a \b bufsize. Tinyusb will proceed with
 *              other work and invoked this again with adjusted parameters.
 *
 * \retval      zero        Indicate application is not ready yet e.g disk I/O is not complete.
 *                          Tinyusb will invoke this callback with the same parameters again some time later.
 *
 * \retval      negative    Indicate error writing disk I/O. Tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 */
int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);

/**
 * Callback invoked when received an SCSI command not in built-in list below.
 * \param[in]   lun         Logical unit number
 * \param[in]   scsi_cmd    SCSI command contents which application must examine to response accordingly
 * \param[out]  buffer      Buffer for SCSI Data Stage.
 *                            - For INPUT: application must fill this with response.
 *                            - For OUTPUT it holds the Data from host
 * \param[in]   bufsize     Buffer's length.
 *
 * \return      Actual bytes processed, can be zero for no-data command.
 * \retval      negative    Indicate error e.g unsupported command, tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 *
 * \note        Following command is automatically handled by tinyusb stack, callback should not be worried:
 *              - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
 *              - READ10 and WRITE10 has their own callbacks
 */
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize);

/*------------- Optional callbacks : Could be used by application to free up resources -------------*/
ATTR_WEAK void tud_msc_read10_complete_cb(uint8_t lun);
ATTR_WEAK void tud_msc_write10_complete_cb(uint8_t lun);
ATTR_WEAK void tud_msc_scsi_complete_cb(uint8_t lun, uint8_t const scsi_cmd[16]);

/** @} */
/** @} */

//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void mscd_init(void);
tusb_error_t mscd_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length);
tusb_error_t mscd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request);
tusb_error_t mscd_xfer_cb(uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes);
void mscd_reset(uint8_t rhport);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSC_DEVICE_H_ */

