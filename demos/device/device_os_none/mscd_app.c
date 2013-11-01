/**************************************************************************/
/*!
    @file     mscd_app.c
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

#include "mscd_app.h"

#if TUSB_CFG_DEVICE_MSC
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
enum
{
  DISK_CAPACITY   = 16 * 1024 * 1024,
  DISK_BLOCK_SIZE = 512
};

static scsi_inquiry_data_t mscd_inquiry_data TUSB_CFG_ATTR_USBRAM =
{
    .is_removable         = 1,
    .version              = 2,
    .response_data_format = 2,
    .vendor_id            = "tinyusb",
    .product_id           = "MSC Example",
    .product_revision     = "0.01"
};

static scsi_read_capacity10_data_t mscd_read_capacity10_data TUSB_CFG_ATTR_USBRAM =
{
    .last_lba   = DISK_CAPACITY / DISK_BLOCK_SIZE, // read capacity
    .block_size = DISK_BLOCK_SIZE
};

static scsi_sense_fixed_data_t mscd_sense_data TUSB_CFG_ATTR_USBRAM =
{
    .response_code        = 0x70,
    .sense_key            = 0, // no errors
    .additional_sense_len = sizeof(scsi_sense_fixed_data_t) - 8
};

static scsi_read_format_capacity_data_t mscd_format_capacity_data TUSB_CFG_ATTR_USBRAM =
{
    .list_length     = 8,
    .block_num       = DISK_CAPACITY / DISK_BLOCK_SIZE, // write capacity
    .descriptor_type = 2, // TODO formatted media, refractor to const
    .block_size      = DISK_BLOCK_SIZE
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// tinyusb callback (ISR context)
//--------------------------------------------------------------------+
msc_csw_status_t tusbd_msc_scsi_received_isr (uint8_t coreid, uint8_t lun, uint8_t scsi_cmd[16], void ** pp_buffer, uint16_t* p_length)
{
  switch (scsi_cmd[0])
  {
    case SCSI_CMD_INQUIRY:
      (*pp_buffer) = &mscd_inquiry_data;
      (*p_length)  = sizeof(scsi_inquiry_data_t);
    break;

    case SCSI_CMD_READ_CAPACITY_10:
      (*pp_buffer) = &mscd_read_capacity10_data;
      (*p_length)  = sizeof(scsi_read_capacity10_data_t);
    break;

    case SCSI_CMD_REQUEST_SENSE:
      (*pp_buffer) = &mscd_sense_data;
      (*p_length)  = sizeof(scsi_sense_fixed_data_t);
    break;

    case  SCSI_CMD_READ_FORMAT_CAPACITY:
      (*pp_buffer) = &mscd_format_capacity_data;
      (*p_length)  = sizeof(scsi_read_format_capacity_data_t);
    break;

    default: return MSC_CSW_STATUS_FAILED;
  }

  return MSC_CSW_STATUS_PASSED;
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+


#endif
