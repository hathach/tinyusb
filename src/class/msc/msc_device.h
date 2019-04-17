/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_MSC_DEVICE_H_
#define _TUSB_MSC_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "msc.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+
TU_VERIFY_STATIC(CFG_TUD_MSC_BUFSIZE < UINT16_MAX, "Size is not correct");

#ifndef CFG_TUD_MSC_MAXLUN
  #define CFG_TUD_MSC_MAXLUN 1
#elif CFG_TUD_MSC_MAXLUN == 0 || CFG_TUD_MSC_MAXLUN > 16
  #error MSC Device: Incorrect setting of MAX LUN
#endif

#ifndef CFG_TUD_MSC_BUFSIZE
  #error CFG_TUD_MSC_BUFSIZE must be defined, value of a block size should work well, the more the better
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


//--------------------------------------------------------------------+
// Interface Descriptor Template
//--------------------------------------------------------------------+

// Length of template descriptor: 23 bytes
#define TUD_MSC_DESC_LEN    (9 + 7 + 7)

// Interface Number, EP Out & EP In address
#define TUD_MSC_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
  /* Interface */\
  0x09, TUSB_DESC_INTERFACE, _itfnum, 0x00, 0x02, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI, MSC_PROTOCOL_BOT, _stridx,\
  /* Endpoint Out */\
  0x07, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0x00,\
  /* Endpoint In */\
  0x07, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0x00


/** \addtogroup ClassDriver_MSC
 *  @{
 * \defgroup MSC_Device Device
 *  @{ */

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
int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize);

// Invoked to determine the disk size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size);

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

// Invoked when Read10 command is complete
ATTR_WEAK void tud_msc_read10_complete_cb(uint8_t lun);

// Invoke when Write10 command is complete
ATTR_WEAK void tud_msc_write10_complete_cb(uint8_t lun);

// Invoked when command in tud_msc_scsi_cb is complete
ATTR_WEAK void tud_msc_scsi_complete_cb(uint8_t lun, uint8_t const scsi_cmd[16]);

// Hook to make a mass storage device read-only. TODO remove
ATTR_WEAK bool tud_msc_is_writable_cb(uint8_t lun);

/** @} */
/** @} */

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void mscd_init(void);
bool mscd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t *p_length);
bool mscd_control_request(uint8_t rhport, tusb_control_request_t const * p_request);
bool mscd_control_request_complete (uint8_t rhport, tusb_control_request_t const * p_request);
bool mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void mscd_reset(uint8_t rhport);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSC_DEVICE_H_ */
