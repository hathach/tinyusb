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
enum
{
  MSC_STAGE_IDLE = 0,
  MSC_STAGE_CMD,
  MSC_STAGE_DATA,
  MSC_STAGE_STATUS,
};

typedef struct
{
  uint8_t  itf_num;
  uint8_t  ep_in;
  uint8_t  ep_out;

  uint8_t  max_lun;

  scsi_read_capacity10_resp_t capacity;

  volatile bool is_initialized;
  uint8_t vendor_id[8];
  uint8_t product_id[16];

  uint8_t stage;
  void*   buffer;
  tuh_msc_complete_cb_t complete_cb;

  msc_cbw_t cbw;
  msc_csw_t csw;
}msch_interface_t;

CFG_TUSB_MEM_SECTION static msch_interface_t msch_data[CFG_TUSB_HOST_DEVICE_MAX];

// buffer used to read scsi information when mounted, largest response data currently is inquiry
CFG_TUSB_MEM_SECTION TU_ATTR_ALIGNED(4) static uint8_t msch_buffer[sizeof(scsi_inquiry_resp_t)];

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

bool tuh_msc_get_capacity(uint8_t dev_addr, uint32_t* p_last_lba, uint32_t* p_block_size)
{
  msch_interface_t* p_msc = &msch_data[dev_addr-1];

  if ( !p_msc->is_initialized ) return false;
  TU_ASSERT(p_last_lba != NULL && p_block_size != NULL);

  (*p_last_lba)   = p_msc->capacity.last_lba;
  (*p_block_size) = p_msc->capacity.block_size;

  return true;
}

//--------------------------------------------------------------------+
// PUBLIC API: SCSI COMMAND
//--------------------------------------------------------------------+
static inline void msc_cbw_add_signature(msc_cbw_t *p_cbw, uint8_t lun)
{
  p_cbw->signature  = MSC_CBW_SIGNATURE;
  p_cbw->tag        = 0x54555342; // TUSB
  p_cbw->lun        = lun;
}

bool tuh_msc_scsi_command(uint8_t dev_addr, msc_cbw_t const* cbw, void* data, tuh_msc_complete_cb_t complete_cb)
{
  msch_interface_t* p_msc = &msch_data[dev_addr-1];

  // TODO claim endpoint

  p_msc->cbw = *cbw;
  p_msc->stage = MSC_STAGE_CMD;
  p_msc->buffer = data;
  p_msc->complete_cb = complete_cb;

  TU_ASSERT(usbh_edpt_xfer(dev_addr, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)));

  return true;
}

#if 0
tusb_error_t tusbh_msc_inquiry(uint8_t dev_addr, uint8_t lun, uint8_t *p_data)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);
  p_msch->cbw.total_bytes = sizeof(scsi_inquiry_resp_t);
  p_msch->cbw.dir        = TUSB_DIR_IN_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_inquiry_t);

  //------------- SCSI command -------------//
  scsi_inquiry_t const cmd_inquiry =
  {
      .cmd_code     = SCSI_CMD_INQUIRY,
      .alloc_length = sizeof(scsi_inquiry_resp_t)
  };

  memcpy(p_msch->cbw.command, &cmd_inquiry, p_msch->cbw.cmd_len);

  TU_ASSERT_ERR ( send_cbw(dev_addr, p_msch, p_data) );

  return TUSB_ERROR_NONE;
}
#endif

bool tuh_msc_test_unit_ready(uint8_t dev_addr, uint8_t lun,  tuh_msc_complete_cb_t complete_cb)
{
  msc_cbw_t cbw = { 0 };
  msc_cbw_add_signature(&cbw, lun);

  cbw.total_bytes = 0; // Number of bytes
  cbw.dir        = TUSB_DIR_OUT;
  cbw.cmd_len    = sizeof(scsi_test_unit_ready_t);
  cbw.command[0] = SCSI_CMD_TEST_UNIT_READY;
  cbw.command[1] = lun; // according to wiki TODO need verification

  TU_ASSERT(tuh_msc_scsi_command(dev_addr, &cbw, NULL, complete_cb));

  return true;
}

bool tuh_msc_request_sense(uint8_t dev_addr, uint8_t lun, void *resposne, tuh_msc_complete_cb_t complete_cb)
{
  msc_cbw_t cbw = { 0 };
  msc_cbw_add_signature(&cbw, lun);

  cbw.total_bytes = 18; // TODO sense response
  cbw.dir        = TUSB_DIR_IN_MASK;
  cbw.cmd_len    = sizeof(scsi_request_sense_t);

  scsi_request_sense_t const cmd_request_sense =
  {
    .cmd_code     = SCSI_CMD_REQUEST_SENSE,
    .alloc_length = 18
  };

  memcpy(cbw.command, &cmd_request_sense, cbw.cmd_len);

  TU_ASSERT(tuh_msc_scsi_command(dev_addr, &cbw, resposne, complete_cb));

  return true;
}

#if 0
tusb_error_t  tuh_msc_read10(uint8_t dev_addr, uint8_t lun, void * p_buffer, uint32_t lba, uint16_t block_count)
{
  msch_interface_t* p_msch = &msch_data[dev_addr-1];

  //------------- Command Block Wrapper -------------//
  msc_cbw_add_signature(&p_msch->cbw, lun);

  p_msch->cbw.total_bytes = p_msch->block_size*block_count; // Number of bytes
  p_msch->cbw.dir        = TUSB_DIR_IN_MASK;
  p_msch->cbw.cmd_len    = sizeof(scsi_read10_t);

  //------------- SCSI command -------------//
  scsi_read10_t cmd_read10 =msch_sem_hdl
  {
      .cmd_code    = SCSI_CMD_READ_10,
      .lba         = tu_htonl(lba),
      .block_count = tu_htons(block_count)
  };

  memcpy(p_msch->cbw.command, &cmd_read10, p_msch->cbw.cmd_len);

  TU_ASSERT_ERR ( send_cbw(dev_addr, p_msch, p_buffer));

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

  TU_ASSERT_ERR ( send_cbw(dev_addr, p_msch, (void*) p_buffer));

  return TUSB_ERROR_NONE;
}
#endif

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
void msch_init(void)
{
  tu_memclr(msch_data, sizeof(msch_interface_t)*CFG_TUSB_HOST_DEVICE_MAX);
}


void msch_close(uint8_t dev_addr)
{
  tu_memclr(&msch_data[dev_addr-1], sizeof(msch_interface_t));
  tuh_msc_unmounted_cb(dev_addr); // invoke Application Callback
}

static bool get_csw(uint8_t dev_addr, msch_interface_t * p_msc)
{
  p_msc->stage = MSC_STAGE_STATUS;
  TU_ASSERT(usbh_edpt_xfer(dev_addr, p_msc->ep_in, (uint8_t*) &p_msc->csw, sizeof(msc_csw_t)));
  return true;
}

bool msch_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  msch_interface_t* p_msc = &msch_data[dev_addr-1];
  msc_cbw_t const * cbw = &p_msc->cbw;
  msc_csw_t       * csw = &p_msc->csw;

  switch (p_msc->stage)
  {
    case MSC_STAGE_CMD:
      // Must be Command Block
      TU_ASSERT(ep_addr == p_msc->ep_out &&  event == XFER_RESULT_SUCCESS && xferred_bytes == sizeof(msc_cbw_t));

      if ( cbw->total_bytes && p_msc->buffer )
      {
        // Data stage if any
        p_msc->stage = MSC_STAGE_DATA;

        uint8_t const ep_data = (cbw->dir & TUSB_DIR_IN_MASK) ? p_msc->ep_in : p_msc->ep_out;
        TU_ASSERT(usbh_edpt_xfer(dev_addr, ep_data, p_msc->buffer, cbw->total_bytes));
      }else
      {
        // Status stage
        get_csw(dev_addr, p_msc);
      }
    break;

    case MSC_STAGE_DATA:
      get_csw(dev_addr, p_msc);
    break;

    case MSC_STAGE_STATUS:
      // SCSI op is complete
      p_msc->stage = MSC_STAGE_IDLE;

      if (p_msc->complete_cb) p_msc->complete_cb(dev_addr, cbw, csw);
    break;

    // unknown state
    default: break;
  }

  return true;
}

//--------------------------------------------------------------------+
// MSC Enumeration
//--------------------------------------------------------------------+

static bool open_get_maxlun_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool open_test_unit_ready_complete(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw);
static bool open_request_sense_complete(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw);
static bool open_read_capacity10_complete(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw);

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

    TU_ASSERT(usbh_edpt_open(rhport, dev_addr, ep_desc));

    if ( tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN )
    {
      p_msc->ep_in = ep_desc->bEndpointAddress;
    }else
    {
      p_msc->ep_out = ep_desc->bEndpointAddress;
    }

    ep_desc = (tusb_desc_endpoint_t const *) tu_desc_next(ep_desc);
  }

  p_msc->itf_num = itf_desc->bInterfaceNumber;
  (*p_length) += sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);

  //------------- Get Max Lun -------------//
  TU_LOG2("MSC Get Max Lun\r\n");
  tusb_control_request_t request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_IN
    },
    .bRequest = MSC_REQ_GET_MAX_LUN,
    .wValue   = 0,
    .wIndex   = p_msc->itf_num,
    .wLength  = 1
  };
  TU_ASSERT(tuh_control_xfer(dev_addr, &request, msch_buffer, open_get_maxlun_complete));

  return true;
}

static bool open_get_maxlun_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;

  msch_interface_t* p_msc = &msch_data[dev_addr-1];

  // STALL means zero
  p_msc->max_lun = (XFER_RESULT_SUCCESS == result) ? msch_buffer[0] : 0;

  // MSCU Reset
#if 0 // not really needed
  tusb_control_request_t const new_request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = MSC_REQ_RESET,
    .wValue   = 0,
    .wIndex   = p_msc->itf_num,
    .wLength  = 0
  };
  TU_ASSERT( usbh_control_xfer( dev_addr, &new_request, NULL ) );
#endif

#if 0
  //------------- SCSI Inquiry -------------//
  TU_LOG2("SCSI Inquiry\r\n");
  tusbh_msc_inquiry(dev_addr, 0, msch_buffer);
  TU_ASSERT( osal_semaphore_wait(msch_sem_hdl, SCSI_XFER_TIMEOUT) );

  memcpy(p_msc->vendor_id , ((scsi_inquiry_resp_t*) msch_buffer)->vendor_id , 8);
  memcpy(p_msc->product_id, ((scsi_inquiry_resp_t*) msch_buffer)->product_id, 16);
#endif

  // TODO multiple LUN support
  TU_LOG2("SCSI Test Unit Ready\r\n");
  tuh_msc_test_unit_ready(dev_addr, 0, open_test_unit_ready_complete);

  return true;
}

static bool open_test_unit_ready_complete(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw)
{
  if (csw->status == 0)
  {
    TU_LOG2("SCSI Read Capacity 10\r\n");

    msch_interface_t* p_msc = &msch_data[dev_addr-1];
    msc_cbw_t new_cbw = { 0 };

    msc_cbw_add_signature(&new_cbw, cbw->lun);
    new_cbw.total_bytes = sizeof(scsi_read_capacity10_resp_t);
    new_cbw.dir        = TUSB_DIR_IN_MASK;
    new_cbw.cmd_len    = sizeof(scsi_read_capacity10_t);
    new_cbw.command[0] = SCSI_CMD_READ_CAPACITY_10;

    TU_ASSERT(tuh_msc_scsi_command(dev_addr, &new_cbw, &p_msc->capacity, open_read_capacity10_complete));
  }else
  {
    // Note: During enumeration, some device fails Test Unit Ready and require a few retries
    // with Request Sense to start working !!
    // TODO limit number of retries
    TU_ASSERT(tuh_msc_request_sense(dev_addr, cbw->lun, msch_buffer, open_request_sense_complete));
  }

  return true;
}

static bool open_request_sense_complete(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw)
{
  TU_ASSERT(tuh_msc_test_unit_ready(dev_addr, cbw->lun, open_test_unit_ready_complete));
  return true;
}

static bool open_read_capacity10_complete(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw)
{
  TU_ASSERT(csw->status == 0);

  msch_interface_t* p_msc = &msch_data[dev_addr-1];

  // Note: Block size and last LBA are big-endian
  p_msc->capacity.last_lba = tu_ntohl(p_msc->capacity.last_lba);
  p_msc->capacity.block_size = tu_ntohl(p_msc->capacity.block_size);

  // Enumeration is complete
  p_msc->is_initialized = true; // open complete TODO remove
  tuh_msc_mounted_cb(dev_addr);

  return true;
}

#endif
