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
VERIFY_STATIC(CFG_TUD_MSC_BUFSIZE < UINT16_MAX, "Size is not correct");

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

//--------------------------------------------------------------------+
// APPLICATION CALLBACK API (WEAK is optional)
//--------------------------------------------------------------------+
/** \brief Callback invoked when received \ref SCSI_CMD_READ_10 command
 * \param[in]		lun         Targeted Logical Unit
 *                          Must be accessible by USB controller (see \ref CFG_TUSB_ATTR_USBRAM)
 * \param[in]		lba         Starting Logical Block Address to be read
 * \param[in]		offset      Byte offset from LBA for reading
 * \param[out]	buffer      Buffer which application need to update with the response data.
 * \param[in]   bufsize     Max bytes can be copied. Application must not return more than this value.
 *
 * \retval      positive    Actual bytes returned, must not be more than \a \b bufsize.
 *                          If value less than bufsize is returned, Tinyusb will transfer this amount and afterwards
 *                          invoke this callback again with adjusted offset.
 *
 * \retval      zero        Indicate application is not ready yet to response e.g disk I/O is not complete.
 *                          Tinyusb will invoke this callback with the same params again some time later.
 *
 * \retval      negative    Indicate error reading disk I/O. Tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 *
 * \note        Host can request dozens of Kbytes in one command e.g \a \b block_count = 128, it is insufficient to require
 *              application to have that much of buffer. Instead, application can return the number of blocks it can processed,
 *              the stack after transferred that amount of data will continue to invoke this callback with adjusted \a \b lba and \a \b block_count.
 *              \n\n Although this callback is called by tinyusb device task (non-isr context), however as all the classes share
 *              the same task (to save resource), any delay in this callback will cause delay in response on other classes.
 */
int32_t tud_msc_read10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);

/** \brief 			Callback invoked when received \ref SCSI_CMD_WRITE_10 command
 * \param[in]		lun         Targeted Logical Unit
 *                          Must be accessible by USB controller (see \ref CFG_TUSB_ATTR_USBRAM)
 * \param[in]		lba         Starting Logical Block Address to be write
 * \param[out]	buffer      Buffer which holds written data.
 * \param[in]   bufsize     Max bytes in buffer. Application must not return more than this value.
 *
 * \retval      positive    Actual bytes written, must not be more than \a \b bufsize.
 *                          If value less than bufsize is returned, Tinyusb will trim off this amount and
 *                          invoke this callback again with adjusted offset some time later.
 *
 * \retval      zero        Indicate application is not ready yet e.g disk I/O is not complete.
 *                          Tinyusb will invoke this callback with the same params again some time later.
 *
 * \retval      negative    Indicate error writing disk I/O. Tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 *
 * \note        Host can request dozens of Kbytes in one command e.g \a \b block_count = 128, it is insufficient to require
 *              application to have that much of buffer. Instead, application can return the number of blocks it can processed,
 *              the stack after transferred that amount of data will continue to invoke this callback with adjusted \a \b lba and \a \b block_count.
 *              \n\n Although this callback is called by tinyusb device task (non-isr context), however as all the classes share
 *              the same task (to save resource), any delay in this callback will cause delay in response on other classes.
 */
int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);


/** \brief 			Callback invoked when received an SCSI command other than \ref SCSI_CMD_WRITE_10 and \ref SCSI_CMD_READ_10
 * \param[in]		lun         Targeted Logical Unit
 * \param[in]		scsi_cmd    SCSI command contents, application should examine this command block to know which command host requested
 * \param[out]  buffer      Buffer for SCSI Data Stage.
 *                            - For INPUT: application must fill this with response.
 *                            - For OUTPUT it holds the Data from host
 * \param[in]   bufsize     Buffer's length.
 *
 * \retval      negative    Indicate error e.g accessing disk I/O. Tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 * \retval      otherwise   Actual bytes processed, must not be more than \a \b bufsize. Can be zero for no-data command.
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

