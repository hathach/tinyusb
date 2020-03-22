/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include "tusb_option.h"

#if TUSB_OPT_HOST_ENABLED & CFG_TUH_MSC

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/tusb_common.h"
#include "msc_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION static msch_interface_t msch_data[CFG_TUSB_HOST_DEVICE_MAX];

//------------- Initalization Data -------------//
static osal_semaphore_def_t msch_sem_def;
static osal_semaphore_t msch_sem_hdl;

// buffer used to read scsi information when mounted, largest response data currently is inquiry
CFG_TUSB_MEM_SECTION TU_ATTR_ALIGNED(4) static uint8_t msch_buffer[sizeof(scsi_inquiry_resp_t)];

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
          hcd_edpt_busy(dev_addr, msch_data[dev_addr-1].ep_in);
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
  TU_ASSERT(p_last_lba != NULL && p_block_size != NULL, TUSB_ERROR_INVALID_PARA);

  (*p_last_lba)   = msch_data[dev_addr-1].last_lba;
  (*p_block_size) = (uint32_t) msch_data[dev_addr-1].block_size;

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PUBLIC API: SCSI COMMAND
//--------------------------------------------------------------------+
static inline void msc_cbw_add_signature(msc_cbw_t *p_cbw, uint8_t lun)
{
  p_cbw->signature  = MSC_CBW_SIGNATURE;
  p_cbw->tag        = 0xCAFECAFE;
  p_cbw->lun        = lun;
}

static tusb_error_t msch_command_xfer(uint8_t dev_addr, msch_interface_t * p_msch, void* p_buffer)
{
  if ( NULL != p_buffer)
  { // there is data phase
    if (p_msch->cbw.dir & TUSB_DIR_IN_MASK)
    {
      TU_ASSERT( hcd_pipe_xfer(dev_addr, p_msch->ep_out, (uint8_t*) &p_msch->cbw, sizeof(msc_cbw_t), false), TUSB_ERROR_FAILED );
      TU_ASSERT( hcd_pipe_queue_xfer(dev_addr, p_msch->ep_in , p_buffer, p_msch->cbw.total_bytes), TUSB_ERROR_FAILED );
    }else
    {
      TU_ASSERT( hcd_pipe_queue_xfer(dev_addr, p_msch->ep_out, (uint8_t*) &p_msch->cbw, sizeof(msc_cbw_t)), TUSB_ERROR_FAILED );
      TU_ASSERT( hcd_pipe_xfer(dev_addr, p_msch->ep_out , p_buffer, p_msch->cbw.total_bytes, false), TUSB_ERROR_FAILED );
    }
  }

  TU_ASSERT( hcd_pipe_xfer(dev_addr, p_msch->ep_in , (uint8_t*) &p_msch->csw, sizeof(msc_csw_t), true), TUSB_ERROR_FAILED);

  return TUSB_ERROR_NONE;
}

tusb_error_t tusbh_msc_inquiry(uint8_t dev_addr, uint8_t lun, uint8_t *p_data)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);
  p_msch->cbw.total_bytes = sizeof(scsi_inquiry_resp_t);
  p_msch->cbw.dir        = TUSB_DIR_IN_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_inquiry_t);

  //------------- SCSI command -------------//
  scsi_inquiry_t cmd_inquiry =
  {
      .cmd_code     = SCSI_CMD_INQUIRY,
      .alloc_length = sizeof(scsi_inquiry_resp_t)
  };

  memcpy(p_msch->cbw.command, &cmd_inquiry, p_msch->cbw.cmd_len);

  TU_ASSERT_ERR ( msch_command_xfer(dev_addr, p_msch, p_data) );

  return TUSB_ERROR_NONE;
}

tusb_error_t tusbh_msc_read_capacity10(uint8_t dev_addr, uint8_t lun, uint8_t *p_data)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);
  p_msch->cbw.total_bytes = sizeof(scsi_read_capacity10_resp_t);
  p_msch->cbw.dir        = TUSB_DIR_IN_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_read_capacity10_t);

  //------------- SCSI command -------------//
  scsi_read_capacity10_t cmd_read_capacity10 =
  {
      .cmd_code                 = SCSI_CMD_READ_CAPACITY_10,
      .lba                      = 0,
      .partial_medium_indicator = 0
  };

  memcpy(p_msch->cbw.command, &cmd_read_capacity10, p_msch->cbw.cmd_len);

  TU_ASSERT_ERR ( msch_command_xfer(dev_addr, p_msch, p_data) );

  return TUSB_ERROR_NONE;
}

tusb_error_t tuh_msc_request_sense(uint8_t dev_addr, uint8_t lun, uint8_t *p_data)
{
  (void) lun; // TODO [MSCH] multiple lun support

  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  p_msch->cbw.total_bytes = 18;
  p_msch->cbw.dir        = TUSB_DIR_IN_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_request_sense_t);

  //------------- SCSI command -------------//
  scsi_request_sense_t cmd_request_sense =
  {
      .cmd_code     = SCSI_CMD_REQUEST_SENSE,
      .alloc_length = 18
  };

  memcpy(p_msch->cbw.command, &cmd_request_sense, p_msch->cbw.cmd_len);

  TU_ASSERT_ERR ( msch_command_xfer(dev_addr, p_msch, p_data) );

  return TUSB_ERROR_NONE;
}

tusb_error_t tuh_msc_test_unit_ready(uint8_t dev_addr, uint8_t lun,  msc_csw_t * p_csw)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);

  p_msch->cbw.total_bytes = 0; // Number of bytes
  p_msch->cbw.dir        = TUSB_DIR_OUT;
  p_msch->cbw.cmd_len    = sizeof(scsi_test_unit_ready_t);

  //------------- SCSI command -------------//
  scsi_test_unit_ready_t cmd_test_unit_ready =
  {
      .cmd_code = SCSI_CMD_TEST_UNIT_READY,
      .lun      = lun // according to wiki
  };

  memcpy(p_msch->cbw.command, &cmd_test_unit_ready, p_msch->cbw.cmd_len);

  // TODO MSCH refractor test uinit ready
  TU_ASSERT( hcd_pipe_xfer(dev_addr, p_msch->ep_out, (uint8_t*) &p_msch->cbw, sizeof(msc_cbw_t), false), TUSB_ERROR_FAILED );
  TU_ASSERT( hcd_pipe_xfer(dev_addr, p_msch->ep_in , (uint8_t*) p_csw, sizeof(msc_csw_t), true), TUSB_ERROR_FAILED );

  return TUSB_ERROR_NONE;
}

tusb_error_t  tuh_msc_read10(uint8_t dev_addr, uint8_t lun, void * p_buffer, uint32_t lba, uint16_t block_count)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);

  p_msch->cbw.total_bytes = p_msch->block_size*block_count; // Number of bytes
  p_msch->cbw.dir        = TUSB_DIR_IN_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_read10_t);

  //------------- SCSI command -------------//
  scsi_read10_t cmd_read10 =
  {
      .cmd_code    = SCSI_CMD_READ_10,
      .lba         = tu_htonl(lba),
      .block_count = tu_htons(block_count)
  };

  memcpy(p_msch->cbw.command, &cmd_read10, p_msch->cbw.cmd_len);

  TU_ASSERT_ERR ( msch_command_xfer(dev_addr, p_msch, p_buffer));

  return TUSB_ERROR_NONE;
}

tusb_error_t tuh_msc_write10(uint8_t dev_addr, uint8_t lun, void const * p_buffer, uint32_t lba, uint16_t block_count)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);

  p_msch->cbw.total_bytes = p_msch->block_size*block_count; // Number of bytes
  p_msch->cbw.dir        = TUSB_DIR_OUT;
  p_msch->cbw.cmd_len    = sizeof(scsi_write10_t);

  //------------- SCSI command -------------//
  scsi_write10_t cmd_write10 =
  {
      .cmd_code    = SCSI_CMD_WRITE_10,
      .lba         = tu_htonl(lba),
      .block_count = tu_htons(block_count)
  };

  memcpy(p_msch->cbw.command, &cmd_write10, p_msch->cbw.cmd_len);

  TU_ASSERT_ERR ( msch_command_xfer(dev_addr, p_msch, (void*) p_buffer));

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
void msch_init(void)
{
  tu_memclr(msch_data, sizeof(msch_interface_t)*CFG_TUSB_HOST_DEVICE_MAX);
  msch_sem_hdl = osal_semaphore_create(&msch_sem_def);
}

bool msch_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t *p_length)
{
  TU_VERIFY (MSC_SUBCLASS_SCSI == itf_desc->bInterfaceSubClass &&
             MSC_PROTOCOL_BOT  == itf_desc->bInterfaceProtocol);

  msch_interface_t* p_msc = &msch_data[dev_addr-1];

  //------------- Open Data Pipe -------------//
  tusb_desc_endpoint_t const * ep_desc = (tusb_desc_endpoint_t const *) tu_desc_next(itf_desc);

  for(uint32_t i=0; i<2; i++)
  {
    TU_ASSERT(TUSB_DESC_ENDPOINT == ep_desc->bDescriptorType);
    TU_ASSERT(TUSB_XFER_BULK == ep_desc->bmAttributes.xfer);

    TU_ASSERT(hcd_edpt_open(rhport, dev_addr, ep_desc));

    if ( tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN )
    {
      p_msc->ep_in = ep_desc->bEndpointAddress;
    }else
    {
      p_msc->ep_out = ep_desc->bEndpointAddress;
    }

    ep_desc = (tusb_desc_endpoint_t const *) tu_desc_next(ep_desc);
  }

  p_msc->itf_numr = itf_desc->bInterfaceNumber;
  (*p_length) += sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);

  //------------- Get Max Lun -------------//
  tusb_control_request_t request = {
        .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_IN },
        .bRequest = MSC_REQ_GET_MAX_LUN,
        .wValue = 0,
        .wIndex = p_msc->itf_numr,
        .wLength = 1
  };
  // TODO STALL means zero
  TU_ASSERT( usbh_control_xfer( dev_addr, &request, msch_buffer ) );
  p_msc->max_lun = msch_buffer[0];

#if 0
  //------------- Reset -------------//
  request = (tusb_control_request_t) {
        .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_OUT },
        .bRequest = MSC_REQ_RESET,
        .wValue = 0,
        .wIndex = p_msc->itf_numr,
        .wLength = 0
  };
  TU_ASSERT( usbh_control_xfer( dev_addr, &request, NULL ) );
#endif

  enum { SCSI_XFER_TIMEOUT = 2000 };
  //------------- SCSI Inquiry -------------//
  tusbh_msc_inquiry(dev_addr, 0, msch_buffer);
  TU_ASSERT( osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT) );

  memcpy(p_msc->vendor_id , ((scsi_inquiry_resp_t*) msch_buffer)->vendor_id , 8);
  memcpy(p_msc->product_id, ((scsi_inquiry_resp_t*) msch_buffer)->product_id, 16);

  //------------- SCSI Read Capacity 10 -------------//
  tusbh_msc_read_capacity10(dev_addr, 0, msch_buffer);
  TU_ASSERT( osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT));

  // NOTE: my toshiba thumb-drive stall the first Read Capacity and require the sequence
  // Read Capacity --> Stalled --> Clear Stall --> Request Sense --> Read Capacity (2) to work
  if ( hcd_edpt_stalled(dev_addr, p_msc->ep_in) )
  {
    // clear stall TODO abstract clear stall function
    request = (tusb_control_request_t) {
      .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_ENDPOINT, .type = TUSB_REQ_TYPE_STANDARD, .direction = TUSB_DIR_OUT },
          .bRequest = TUSB_REQ_CLEAR_FEATURE,
          .wValue = 0,
          .wIndex = p_msc->ep_in,
          .wLength = 0
    };

    TU_ASSERT(usbh_control_xfer( dev_addr, &request, NULL ));

    hcd_edpt_clear_stall(dev_addr, p_msc->ep_in);
    TU_ASSERT( osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT) ); // wait for SCSI status

    //------------- SCSI Request Sense -------------//
    (void) tuh_msc_request_sense(dev_addr, 0, msch_buffer);
    TU_ASSERT(osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT));

    //------------- Re-read SCSI Read Capactity -------------//
    tusbh_msc_read_capacity10(dev_addr, 0, msch_buffer);
    TU_ASSERT(osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT));
  }

  p_msc->last_lba   = tu_ntohl( ((scsi_read_capacity10_resp_t*)msch_buffer)->last_lba );
  p_msc->block_size = (uint16_t) tu_ntohl( ((scsi_read_capacity10_resp_t*)msch_buffer)->block_size );

  p_msc->is_initialized = true;

  tuh_msc_mounted_cb(dev_addr);

  return true;
}

void msch_isr(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  msch_interface_t* p_msc = &msch_data[dev_addr-1];
  if ( ep_addr == p_msc->ep_in )
  {
    if (p_msc->is_initialized)
    {
      tuh_msc_isr(dev_addr, event, xferred_bytes);
    }else
    { // still initializing under open subtask
      osal_semaphore_post(msch_sem_hdl, true);
    }
  }
}

void msch_close(uint8_t dev_addr)
{
  tu_memclr(&msch_data[dev_addr-1], sizeof(msch_interface_t));
  osal_semaphore_reset(msch_sem_hdl);

  tuh_msc_unmounted_cb(dev_addr); // invoke Application Callback
}

//--------------------------------------------------------------------+
// INTERNAL & HELPER
//--------------------------------------------------------------------+


#endif
