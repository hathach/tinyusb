/**************************************************************************/
/*!
    @file     msc_device_app.c
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

#include "msc_device_app.h"

#if CFG_TUD_MSC
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
static scsi_inquiry_data_t const mscd_inquiry_data =
{
    .is_removable         = 1,
    .version              = 2,
    .response_data_format = 2,
    .vendor_id            = "tinyusb",
    .product_id           = "MSC Example",
    .product_revision     = "0.01"
};

static scsi_read_capacity10_data_t const mscd_read_capacity10_data =
{
    .last_lba   = ENDIAN_BE(DISK_BLOCK_NUM-1), // read capacity
    .block_size = ENDIAN_BE(DISK_BLOCK_SIZE)
};

scsi_sense_fixed_data_t mscd_sense_data =
{
    .response_code        = 0x70,
    .sense_key            = 0, // no errors
    .additional_sense_len = sizeof(scsi_sense_fixed_data_t) - 8
};

static scsi_read_format_capacity_data_t const mscd_format_capacity_data =
{
    .list_length     = 8,
    .block_num       = ENDIAN_BE(DISK_BLOCK_NUM), // write capacity
    .descriptor_type = 2, // TODO formatted media, refractor to const
    .block_size_u16  = ENDIAN_BE16(DISK_BLOCK_SIZE)
};

static scsi_mode_parameters_t const msc_dev_mode_para =
{
    .mode_data_length        = 3,
    .medium_type             = 0,
    .device_specific_para    = 0,
    .block_descriptor_length = 0
};

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void msc_app_mount(uint8_t rhport)
{

}

void msc_app_umount(uint8_t rhport)
{

}

bool tud_msc_scsi_cb (uint8_t rhport, uint8_t lun, uint8_t scsi_cmd[16], void* buffer, uint16_t* p_len)
{
  // read10 & write10 has their own callback and MUST not be handled here

  void* bufptr = NULL;
  uint16_t buflen = 0;

  switch (scsi_cmd[0])
  {
    case SCSI_CMD_INQUIRY:
      bufptr = &mscd_inquiry_data;
      buflen = sizeof(scsi_inquiry_data_t);
    break;

    case SCSI_CMD_READ_CAPACITY_10:
      bufptr = &mscd_read_capacity10_data;
      buflen = sizeof(scsi_read_capacity10_data_t);
    break;

    case SCSI_CMD_REQUEST_SENSE:
      bufptr = &mscd_sense_data;
      buflen = sizeof(scsi_sense_fixed_data_t);
    break;

    case SCSI_CMD_READ_FORMAT_CAPACITY:
      bufptr = &mscd_format_capacity_data;
      buflen = sizeof(scsi_read_format_capacity_data_t);
    break;

    case SCSI_CMD_MODE_SENSE_6:
      bufptr = &msc_dev_mode_para;
      buflen = sizeof(msc_dev_mode_para);
    break;

    case SCSI_CMD_TEST_UNIT_READY:
      bufptr = NULL;
      buflen= 0;
    break;

    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      bufptr = NULL;
      buflen= 0;
    break;

    default:
      (*p_len) = 0;
      return false;
  }

  if ( bufptr && buflen )
  {
    // Response len must not larger than expected from host
    TU_ASSERT( (*p_len) >= buflen );

    memcpy(buffer, bufptr, buflen);
    (*p_len) = buflen;
  }

  //------------- clear sense data if it is not request sense command -------------//
  if ( SCSI_CMD_REQUEST_SENSE != scsi_cmd[0] )
  {
    mscd_sense_data.sense_key                  = SCSI_SENSEKEY_NONE;
    mscd_sense_data.additional_sense_code      = 0;
    mscd_sense_data.additional_sense_qualifier = 0;
  }

  return true;
}

//--------------------------------------------------------------------+
// APPLICATION CODE
//--------------------------------------------------------------------+
void msc_app_task(void* param)
{ // no need to implement the task yet
  (void) param;

  OSAL_TASK_BEGIN

  OSAL_TASK_END
}

void msc_app_init (void)
{

}


#endif
