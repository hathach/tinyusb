/**************************************************************************/
/*!
    @file     msc_host.c
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
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"

#if MODE_HOST_SUPPORTED & TUSB_CFG_HOST_MSC

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "msc_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  pipe_handle_t bulk_in, bulk_out;
  uint8_t  interface_number;

  uint8_t  max_lun;
  uint16_t block_size;
  uint32_t last_lba; // last logical block address

  uint8_t vendor_id[8];
  uint8_t product_id[16];

  msc_cmd_block_wrapper_t cbw;
  msc_cmd_status_wrapper_t csw;
  ATTR_ALIGNED(4) uint8_t buffer[100];
}msch_interface_t;

STATIC_VAR msch_interface_t msch_data[TUSB_CFG_HOST_DEVICE_MAX] TUSB_CFG_ATTR_USBRAM; // TODO to be static

// TODO rename this
STATIC_VAR uint8_t msch_buffer[10] TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// PUBLIC API
//--------------------------------------------------------------------+
bool tusbh_msc_is_mounted(uint8_t dev_addr)
{
  return  tusbh_device_is_configured(dev_addr) &&
          pipehandle_is_valid(msch_data[dev_addr-1].bulk_in) &&
          pipehandle_is_valid(msch_data[dev_addr-1].bulk_out);
}

uint8_t const* tusbh_msc_get_vendor_name(uint8_t dev_addr)
{
  return tusbh_msc_is_mounted(dev_addr) ? msch_data[dev_addr-1].vendor_id : NULL;
}

uint8_t const* tusbh_msc_get_product_name(uint8_t dev_addr)
{
  return tusbh_msc_is_mounted(dev_addr) ? msch_data[dev_addr-1].product_id : NULL;
}

tusb_error_t tusbh_msc_get_capacity(uint8_t dev_addr, uint32_t* p_last_lba, uint32_t* p_block_size)
{
  if ( !tusbh_msc_is_mounted(dev_addr) )   return TUSB_ERROR_MSCH_DEVICE_NOT_MOUNTED;
  ASSERT(p_last_lba != NULL && p_block_size != NULL, TUSB_ERROR_INVALID_PARA);

  (*p_last_lba)   = msch_data[dev_addr-1].last_lba;
  (*p_block_size) = (uint32_t) msch_data[dev_addr-1].block_size;
  
  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
static tusb_error_t scsi_command_send(msch_interface_t * p_msch, scsi_cmd_type_t cmd_code, uint8_t lun)
{
  p_msch->cbw.signature = 0x43425355;
  p_msch->cbw.tag       = 0xCAFECAFE;
  p_msch->cbw.lun       = lun;

  switch ( cmd_code )
  {
    case SCSI_CMD_INQUIRY:
      p_msch->cbw.xfer_bytes = sizeof(scsi_inquiry_data_t);
      p_msch->cbw.flags      = TUSB_DIR_DEV_TO_HOST_MASK;
      p_msch->cbw.cmd_len    = sizeof(scsi_inquiry_t);

      scsi_inquiry_t cmd_inquiry =
      {
          .cmd_code     = SCSI_CMD_INQUIRY,
          .alloc_length = sizeof(scsi_inquiry_data_t)
      };

      memcpy(p_msch->cbw.command, &cmd_inquiry, sizeof(scsi_inquiry_t));
    break;

    case SCSI_CMD_READ_CAPACITY_10:
      p_msch->cbw.xfer_bytes = sizeof(scsi_read_capacity10_data_t);
      p_msch->cbw.flags      = TUSB_DIR_DEV_TO_HOST_MASK;
      p_msch->cbw.cmd_len    = sizeof(scsi_read_capacity10_t);

      scsi_read_capacity10_t cmd_read_capacity10 =
      {
          .cmd_code                 = SCSI_CMD_READ_CAPACITY_10,
          .logical_block_addr       = 0,
          .partial_medium_indicator = 0
      };

      memcpy(p_msch->cbw.command, &cmd_read_capacity10, sizeof(scsi_read_capacity10_t));
    break;

    case SCSI_CMD_TEST_UNIT_READY:
    break;

    case SCSI_CMD_READ_10:
    break;

    case SCSI_CMD_WRITE_10:
    break;

    case SCSI_CMD_REQUEST_SENSE:
      p_msch->cbw.xfer_bytes = 18;
      p_msch->cbw.flags      = TUSB_DIR_DEV_TO_HOST_MASK;
      p_msch->cbw.cmd_len    = sizeof(scsi_request_sense_t);

      scsi_request_sense_t cmd_request_sense =
      {
          .cmd_code     = SCSI_CMD_REQUEST_SENSE,
          .alloc_length = 18
      };

      memcpy(p_msch->cbw.command, &cmd_request_sense, sizeof(scsi_request_sense_t));
    break;

    default:
      return TUSB_ERROR_MSCH_UNKNOWN_SCSI_COMMAND;
  }

  ASSERT_STATUS( hcd_pipe_xfer(p_msch->bulk_out, (uint8_t*) &p_msch->cbw, sizeof(msc_cmd_block_wrapper_t), false) );
  ASSERT_STATUS( hcd_pipe_queue_xfer(p_msch->bulk_in , p_msch->buffer, p_msch->cbw.xfer_bytes) );
  ASSERT_STATUS( hcd_pipe_xfer(p_msch->bulk_in , &p_msch->csw, sizeof(msc_cmd_status_wrapper_t), true) );
}

void msch_init(void)
{
  memclr_(msch_data, sizeof(msch_interface_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

tusb_error_t msch_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length)
{
  tusb_error_t error;

  OSAL_SUBTASK_BEGIN

  if (! ( MSC_SUBCLASS_SCSI == p_interface_desc->bInterfaceSubClass &&
          MSC_PROTOCOL_BOT  == p_interface_desc->bInterfaceProtocol ) )
  {
    return TUSB_ERROR_MSCH_UNSUPPORTED_PROTOCOL;
  }

  //------------- Open Data Pipe -------------//
  tusb_descriptor_endpoint_t const *p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*) p_interface_desc );
  for(uint32_t i=0; i<2; i++)
  {
    ASSERT_INT(TUSB_DESC_TYPE_ENDPOINT, p_endpoint->bDescriptorType, TUSB_ERROR_USBH_DESCRIPTOR_CORRUPTED);

    pipe_handle_t * p_pipe_hdl =  ( p_endpoint->bEndpointAddress &  TUSB_DIR_DEV_TO_HOST_MASK ) ?
        &msch_data[dev_addr-1].bulk_in : &msch_data[dev_addr-1].bulk_out;

    (*p_pipe_hdl) = hcd_pipe_open(dev_addr, p_endpoint, TUSB_CLASS_MSC);
    ASSERT ( pipehandle_is_valid(*p_pipe_hdl), TUSB_ERROR_HCD_OPEN_PIPE_FAILED );

    p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*)  p_endpoint );
  }

  msch_data[dev_addr-1].interface_number = p_interface_desc->bInterfaceNumber;
  (*p_length) += sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);

  //------------- Get Max Lun -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask( dev_addr, bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_CLASS, TUSB_REQUEST_RECIPIENT_INTERFACE),
                               MSC_REQUEST_GET_MAX_LUN, 0, msch_data[dev_addr-1].interface_number,
                               1, msch_buffer ),
    error
  );

  SUBTASK_ASSERT( TUSB_ERROR_NONE == error /* && TODO STALL means zero */);
  msch_data[dev_addr-1].max_lun = msch_buffer[0];

#if 0
  //------------- Reset -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask( dev_addr, bm_request_type(TUSB_DIR_HOST_TO_DEV, TUSB_REQUEST_TYPE_CLASS, TUSB_REQUEST_RECIPIENT_INTERFACE),
                               MSC_REQUEST_RESET, 0, msch_data[dev_addr-1].interface_number,
                               0, NULL ),
    error
  );
#endif


  // TODO timeout required, a proper synchronization
//  while( !hcd_pipe_is_idle(msch_data[dev_addr-1].bulk_in) )

  //------------- SCSI Inquiry -------------//
  scsi_command_send(&msch_data[dev_addr-1], SCSI_CMD_INQUIRY, 0);
  osal_task_delay(2);

  memcpy(msch_data[dev_addr-1].vendor_id,
         ((scsi_inquiry_data_t*)msch_data[dev_addr-1].buffer)->vendor_id,
         8);
  memcpy(msch_data[dev_addr-1].product_id,
         ((scsi_inquiry_data_t*)msch_data[dev_addr-1].buffer)->product_id,
         16);

#if 0
  //------------- SCSI Request Sense -------------//
  scsi_command_send(&msch_data[dev_addr-1], SCSI_CMD_REQUEST_SENSE, 0);
  osal_task_delay(2);
#endif

  //------------- SCSI Read Capacity 10 -------------//
  scsi_command_send(&msch_data[dev_addr-1], SCSI_CMD_READ_CAPACITY_10, 0);
  osal_task_delay(2);

  msch_data[dev_addr-1].last_lba   = __be2le( ((scsi_read_capacity10_data_t*)msch_data[dev_addr-1].buffer)->last_lba );
  msch_data[dev_addr-1].block_size = (uint16_t) __be2le( ((scsi_read_capacity10_data_t*)msch_data[dev_addr-1].buffer)->block_size );

  tusbh_msc_mounted_cb(dev_addr);

  OSAL_SUBTASK_END

  return TUSB_ERROR_NONE;
}

void msch_isr(pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes)
{

}

void msch_close(uint8_t dev_addr)
{
  (void) hcd_pipe_close(msch_data[dev_addr-1].bulk_in);
  (void) hcd_pipe_close(msch_data[dev_addr-1].bulk_out);

  memclr_(&msch_data[dev_addr-1], sizeof(msch_interface_t));
}

//--------------------------------------------------------------------+
// INTERNAL & HELPER
//--------------------------------------------------------------------+


#endif
