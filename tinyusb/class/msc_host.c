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
TUSB_CFG_ATTR_USBRAM STATIC_VAR msch_interface_t msch_data[TUSB_CFG_HOST_DEVICE_MAX];

//------------- Initalization Data -------------//
OSAL_SEM_DEF(msch_semaphore);
static osal_semaphore_handle_t msch_sem_hdl;

// buffer used to read scsi information when mounted, largest response data currently is inquiry
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4) STATIC_VAR uint8_t msch_buffer[sizeof(scsi_inquiry_data_t)];

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// PUBLIC API
//--------------------------------------------------------------------+
bool tuh_msc_is_mounted(uint8_t dev_addr)
{
  return  tuh_device_is_configured(dev_addr) && // is configured can be omitted
          msch_data[dev_addr-1].is_initialized;
}

bool tuh_msc_is_busy(uint8_t dev_addr)
{
  return  msch_data[dev_addr-1].is_initialized &&
          hcd_pipe_is_busy(msch_data[dev_addr-1].bulk_in);
}

uint8_t const* tuh_msc_get_vendor_name(uint8_t dev_addr)
{
  return msch_data[dev_addr-1].is_initialized ? msch_data[dev_addr-1].vendor_id : NULL;
}

uint8_t const* tuh_msc_get_product_name(uint8_t dev_addr)
{
  return msch_data[dev_addr-1].is_initialized ? msch_data[dev_addr-1].product_id : NULL;
}

tusb_error_t tuh_msc_get_capacity(uint8_t dev_addr, uint32_t* p_last_lba, uint32_t* p_block_size)
{
  if ( !msch_data[dev_addr-1].is_initialized )   return TUSB_ERROR_MSCH_DEVICE_NOT_MOUNTED;
  ASSERT(p_last_lba != NULL && p_block_size != NULL, TUSB_ERROR_INVALID_PARA);

  (*p_last_lba)   = msch_data[dev_addr-1].last_lba;
  (*p_block_size) = (uint32_t) msch_data[dev_addr-1].block_size;

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PUBLIC API: SCSI COMMAND
//--------------------------------------------------------------------+
static inline void msc_cbw_add_signature(msc_cmd_block_wrapper_t *p_cbw, uint8_t lun) ATTR_ALWAYS_INLINE;
static inline void msc_cbw_add_signature(msc_cmd_block_wrapper_t *p_cbw, uint8_t lun)
{
  p_cbw->signature  = MSC_CBW_SIGNATURE;
  p_cbw->tag        = 0xCAFECAFE;
  p_cbw->lun        = lun;
}

static tusb_error_t msch_command_xfer(msch_interface_t * p_msch, void* p_buffer) ATTR_WARN_UNUSED_RESULT;
static tusb_error_t msch_command_xfer(msch_interface_t * p_msch, void* p_buffer)
{
  if ( NULL != p_buffer)
  { // there is data phase
    if (p_msch->cbw.dir & TUSB_DIR_DEV_TO_HOST_MASK)
    {
      ASSERT_STATUS( hcd_pipe_xfer(p_msch->bulk_out, (uint8_t*) &p_msch->cbw, sizeof(msc_cmd_block_wrapper_t), false) );
      ASSERT_STATUS( hcd_pipe_queue_xfer(p_msch->bulk_in , p_buffer, p_msch->cbw.xfer_bytes) );
    }else
    {
      ASSERT_STATUS( hcd_pipe_queue_xfer(p_msch->bulk_out, (uint8_t*) &p_msch->cbw, sizeof(msc_cmd_block_wrapper_t)) );
      ASSERT_STATUS( hcd_pipe_xfer(p_msch->bulk_out , p_buffer, p_msch->cbw.xfer_bytes, false) );
    }
  }

  ASSERT_STATUS( hcd_pipe_xfer(p_msch->bulk_in , (uint8_t*) &p_msch->csw, sizeof(msc_cmd_status_wrapper_t), true) );

  return TUSB_ERROR_NONE;
}

tusb_error_t tusbh_msc_inquiry(uint8_t dev_addr, uint8_t lun, uint8_t *p_data)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);
  p_msch->cbw.xfer_bytes = sizeof(scsi_inquiry_data_t);
  p_msch->cbw.dir        = TUSB_DIR_DEV_TO_HOST_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_inquiry_t);

  //------------- SCSI command -------------//
  scsi_inquiry_t cmd_inquiry =
  {
      .cmd_code     = SCSI_CMD_INQUIRY,
      .alloc_length = sizeof(scsi_inquiry_data_t)
  };

  memcpy(p_msch->cbw.command, &cmd_inquiry, p_msch->cbw.cmd_len);

  ASSERT_STATUS ( msch_command_xfer(p_msch, p_data) );

  return TUSB_ERROR_NONE;
}

tusb_error_t tusbh_msc_read_capacity10(uint8_t dev_addr, uint8_t lun, uint8_t *p_data)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);
  p_msch->cbw.xfer_bytes = sizeof(scsi_read_capacity10_data_t);
  p_msch->cbw.dir        = TUSB_DIR_DEV_TO_HOST_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_read_capacity10_t);

  //------------- SCSI command -------------//
  scsi_read_capacity10_t cmd_read_capacity10 =
  {
      .cmd_code                 = SCSI_CMD_READ_CAPACITY_10,
      .lba                      = 0,
      .partial_medium_indicator = 0
  };

  memcpy(p_msch->cbw.command, &cmd_read_capacity10, p_msch->cbw.cmd_len);

  ASSERT_STATUS ( msch_command_xfer(p_msch, p_data) );

  return TUSB_ERROR_NONE;
}

tusb_error_t tuh_msc_request_sense(uint8_t dev_addr, uint8_t lun, uint8_t *p_data)
{
  (void) lun; // TODO [MSCH] multiple lun support

  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  p_msch->cbw.xfer_bytes = 18;
  p_msch->cbw.dir        = TUSB_DIR_DEV_TO_HOST_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_request_sense_t);

  //------------- SCSI command -------------//
  scsi_request_sense_t cmd_request_sense =
  {
      .cmd_code     = SCSI_CMD_REQUEST_SENSE,
      .alloc_length = 18
  };

  memcpy(p_msch->cbw.command, &cmd_request_sense, p_msch->cbw.cmd_len);

  ASSERT_STATUS ( msch_command_xfer(p_msch, p_data) );

  return TUSB_ERROR_NONE;
}

tusb_error_t tuh_msc_test_unit_ready(uint8_t dev_addr, uint8_t lun,  msc_cmd_status_wrapper_t * p_csw)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);

  p_msch->cbw.xfer_bytes = 0; // Number of bytes
  p_msch->cbw.dir        = TUSB_DIR_HOST_TO_DEV;
  p_msch->cbw.cmd_len    = sizeof(scsi_test_unit_ready_t);

  //------------- SCSI command -------------//
  scsi_test_unit_ready_t cmd_test_unit_ready =
  {
      .cmd_code = SCSI_CMD_TEST_UNIT_READY,
      .lun      = lun // according to wiki
  };

  memcpy(p_msch->cbw.command, &cmd_test_unit_ready, p_msch->cbw.cmd_len);

  // TODO MSCH refractor test uinit ready
  ASSERT_STATUS( hcd_pipe_xfer(p_msch->bulk_out, (uint8_t*) &p_msch->cbw, sizeof(msc_cmd_block_wrapper_t), false) );
  ASSERT_STATUS( hcd_pipe_xfer(p_msch->bulk_in , (uint8_t*) p_csw, sizeof(msc_cmd_status_wrapper_t), true) );

  return TUSB_ERROR_NONE;
}

tusb_error_t  tuh_msc_read10(uint8_t dev_addr, uint8_t lun, void * p_buffer, uint32_t lba, uint16_t block_count)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);

  p_msch->cbw.xfer_bytes = p_msch->block_size*block_count; // Number of bytes
  p_msch->cbw.dir        = TUSB_DIR_DEV_TO_HOST_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_read10_t);

  //------------- SCSI command -------------//
  scsi_read10_t cmd_read10 =
  {
      .cmd_code    = SCSI_CMD_READ_10,
      .lba         = __n2be(lba),
      .block_count = u16_le2be(block_count)
  };

  memcpy(p_msch->cbw.command, &cmd_read10, p_msch->cbw.cmd_len);

  ASSERT_STATUS ( msch_command_xfer(p_msch, p_buffer));

  return TUSB_ERROR_NONE;
}

tusb_error_t tuh_msc_write10(uint8_t dev_addr, uint8_t lun, void const * p_buffer, uint32_t lba, uint16_t block_count)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);

  p_msch->cbw.xfer_bytes = p_msch->block_size*block_count; // Number of bytes
  p_msch->cbw.dir        = TUSB_DIR_HOST_TO_DEV;
  p_msch->cbw.cmd_len    = sizeof(scsi_write10_t);

  //------------- SCSI command -------------//
  scsi_write10_t cmd_write10 =
  {
      .cmd_code    = SCSI_CMD_WRITE_10,
      .lba         = __n2be(lba),
      .block_count = u16_le2be(block_count)
  };

  memcpy(p_msch->cbw.command, &cmd_write10, p_msch->cbw.cmd_len);

  ASSERT_STATUS ( msch_command_xfer(p_msch, (void*) p_buffer));

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
void msch_init(void)
{
  memclr_(msch_data, sizeof(msch_interface_t)*TUSB_CFG_HOST_DEVICE_MAX);
  msch_sem_hdl = osal_semaphore_create( OSAL_SEM_REF(msch_semaphore) );
}

tusb_error_t msch_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length)
{
  tusb_error_t error;

  OSAL_SUBTASK_BEGIN

  if (! ( MSC_SUBCLASS_SCSI == p_interface_desc->bInterfaceSubClass &&
          MSC_PROTOCOL_BOT  == p_interface_desc->bInterfaceProtocol ) )
  {
    return TUSB_ERROR_MSC_UNSUPPORTED_PROTOCOL;
  }

  //------------- Open Data Pipe -------------//
  tusb_descriptor_endpoint_t const *p_endpoint;
  p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*) p_interface_desc );

  for(uint32_t i=0; i<2; i++)
  {
    SUBTASK_ASSERT(TUSB_DESC_TYPE_ENDPOINT == p_endpoint->bDescriptorType);
    SUBTASK_ASSERT(TUSB_XFER_BULK == p_endpoint->bmAttributes.xfer);

    pipe_handle_t * p_pipe_hdl =  ( p_endpoint->bEndpointAddress &  TUSB_DIR_DEV_TO_HOST_MASK ) ?
        &msch_data[dev_addr-1].bulk_in : &msch_data[dev_addr-1].bulk_out;

    (*p_pipe_hdl) = hcd_pipe_open(dev_addr, p_endpoint, TUSB_CLASS_MSC);
    SUBTASK_ASSERT( pipehandle_is_valid(*p_pipe_hdl) );

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

  enum { SCSI_XFER_TIMEOUT = 2000 };
  //------------- SCSI Inquiry -------------//
  tusbh_msc_inquiry(dev_addr, 0, msch_buffer);
  osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT, &error);
  SUBTASK_ASSERT_STATUS(error);

  memcpy(msch_data[dev_addr-1].vendor_id , ((scsi_inquiry_data_t*) msch_buffer)->vendor_id , 8);
  memcpy(msch_data[dev_addr-1].product_id, ((scsi_inquiry_data_t*) msch_buffer)->product_id, 16);

  //------------- SCSI Read Capacity 10 -------------//
  tusbh_msc_read_capacity10(dev_addr, 0, msch_buffer);
  osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT, &error);
  SUBTASK_ASSERT_STATUS(error);

  // NOTE: my toshiba thumb-drive stall the first Read Capacity and require the sequence
  // Read Capacity --> Stalled --> Clear Stall --> Request Sense --> Read Capacity (2) to work
  if ( hcd_pipe_is_stalled(msch_data[dev_addr-1].bulk_in) )
  { // clear stall TODO abstract clear stall function
    OSAL_SUBTASK_INVOKED_AND_WAIT(
      usbh_control_xfer_subtask( dev_addr, bm_request_type(TUSB_DIR_HOST_TO_DEV, TUSB_REQUEST_TYPE_STANDARD, TUSB_REQUEST_RECIPIENT_ENDPOINT),
                                 TUSB_REQUEST_CLEAR_FEATURE, 0, hcd_pipe_get_endpoint_addr(msch_data[dev_addr-1].bulk_in),
                                 0, NULL ),
      error
    );
    SUBTASK_ASSERT_STATUS(error);

    hcd_pipe_clear_stall(msch_data[dev_addr-1].bulk_in);
    osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT, &error); // wait for SCSI status
    SUBTASK_ASSERT_STATUS(error);

    //------------- SCSI Request Sense -------------//
    (void) tuh_msc_request_sense(dev_addr, 0, msch_buffer);
    osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT, &error);
    SUBTASK_ASSERT_STATUS(error);

    //------------- Re-read SCSI Read Capactity -------------//
    tusbh_msc_read_capacity10(dev_addr, 0, msch_buffer);
    osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT, &error);
    SUBTASK_ASSERT_STATUS(error);
  }

  msch_data[dev_addr-1].last_lba   = __be2n( ((scsi_read_capacity10_data_t*)msch_buffer)->last_lba );
  msch_data[dev_addr-1].block_size = (uint16_t) __be2n( ((scsi_read_capacity10_data_t*)msch_buffer)->block_size );

  msch_data[dev_addr-1].is_initialized = true;
  tuh_msc_mounted_cb(dev_addr);

  OSAL_SUBTASK_END
}

void msch_isr(pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
  if ( pipehandle_is_equal(pipe_hdl, msch_data[pipe_hdl.dev_addr-1].bulk_in) )
  {
    if (msch_data[pipe_hdl.dev_addr-1].is_initialized)
    {
      tuh_msc_isr(pipe_hdl.dev_addr, event, xferred_bytes);
    }else
    { // still initializing under open subtask
      ASSERT( TUSB_ERROR_NONE == osal_semaphore_post(msch_sem_hdl), VOID_RETURN );
    }
  }
}

void msch_close(uint8_t dev_addr)
{
  (void) hcd_pipe_close(msch_data[dev_addr-1].bulk_in);
  (void) hcd_pipe_close(msch_data[dev_addr-1].bulk_out);

  memclr_(&msch_data[dev_addr-1], sizeof(msch_interface_t));
  osal_semaphore_reset(msch_sem_hdl);

  tuh_msc_unmounted_cb(dev_addr); // invoke Application Callback
}

//--------------------------------------------------------------------+
// INTERNAL & HELPER
//--------------------------------------------------------------------+


#endif
