/**************************************************************************/
/*!
    @file     msc_device.c
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

#include "tusb_option.h"

#if (MODE_DEVICE_SUPPORTED && TUSB_CFG_DEVICE_MSC)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "msc_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t scsi_data[64]; // buffer for scsi's response other than read10 & write10. NOTE should be multiple of 64 to be compatible with lpc11/13u
  ATTR_USB_MIN_ALIGNMENT msc_cmd_block_wrapper_t  cbw;

#if defined (__ICCARM__) && (TUSB_CFG_MCU == MCU_LPC11UXX || TUSB_CFG_MCU == MCU_LPC13UXX)
  uint8_t padding1[64-sizeof(msc_cmd_block_wrapper_t)]; // IAR cannot align struct's member
#endif

  ATTR_USB_MIN_ALIGNMENT msc_cmd_status_wrapper_t csw;

  uint8_t max_lun;
  uint8_t interface_number;
  endpoint_handle_t edpt_in, edpt_out;
}mscd_interface_t;

TUSB_CFG_ATTR_USBRAM STATIC_VAR mscd_interface_t mscd_data;
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static bool read10_write10_data_xfer(mscd_interface_t* p_msc);

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void mscd_init(void)
{
  memclr_(&mscd_data, sizeof(mscd_interface_t));
}

void mscd_close(uint8_t coreid)
{
  memclr_(&mscd_data, sizeof(mscd_interface_t));
  tusbd_msc_unmounted_cb(coreid);
}

tusb_error_t mscd_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length)
{
  ASSERT( ( MSC_SUBCLASS_SCSI == p_interface_desc->bInterfaceSubClass &&
            MSC_PROTOCOL_BOT  == p_interface_desc->bInterfaceProtocol ), TUSB_ERROR_MSC_UNSUPPORTED_PROTOCOL );

  mscd_interface_t * p_msc = &mscd_data;

  //------------- Open Data Pipe -------------//
  tusb_descriptor_endpoint_t const *p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*) p_interface_desc );
  for(uint32_t i=0; i<2; i++)
  {
    ASSERT(TUSB_DESC_TYPE_ENDPOINT == p_endpoint->bDescriptorType &&
           TUSB_XFER_BULK == p_endpoint->bmAttributes.xfer, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

    endpoint_handle_t * p_edpt_hdl =  ( p_endpoint->bEndpointAddress &  TUSB_DIR_DEV_TO_HOST_MASK ) ?
        &p_msc->edpt_in : &p_msc->edpt_out;

    (*p_edpt_hdl) = dcd_pipe_open(coreid, p_endpoint, p_interface_desc->bInterfaceClass);
    ASSERT( endpointhandle_is_valid(*p_edpt_hdl), TUSB_ERROR_DCD_FAILED);

    p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*)  p_endpoint );
  }

  p_msc->interface_number = p_interface_desc->bInterfaceNumber;

  (*p_length) += sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);

  tusbd_msc_mounted_cb(coreid);

  //------------- Queue Endpoint OUT for Command Block Wrapper -------------//
  ASSERT_STATUS( dcd_pipe_xfer(p_msc->edpt_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cmd_block_wrapper_t), true) );

  return TUSB_ERROR_NONE;
}

tusb_error_t mscd_control_request_subtask(uint8_t coreid, tusb_control_request_t const * p_request)
{
  ASSERT(p_request->bmRequestType_bit.type == TUSB_REQUEST_TYPE_CLASS, TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);

  mscd_interface_t * p_msc = &mscd_data;

  switch(p_request->bRequest)
  {
    case MSC_REQUEST_RESET:
      dcd_pipe_control_xfer(coreid, TUSB_DIR_HOST_TO_DEV, NULL, 0, false);
    break;

    case MSC_REQUEST_GET_MAX_LUN:
      p_msc->scsi_data[0] = p_msc->max_lun; // Note: lpc11/13u need xfer data's address to be aligned 64 -> make use of scsi_data instead of using max_lun directly
      dcd_pipe_control_xfer(coreid, TUSB_DIR_DEV_TO_HOST, p_msc->scsi_data, 1, false);
    break;

    default:
      return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// MSCD APPLICATION CALLBACK
//--------------------------------------------------------------------+
tusb_error_t mscd_xfer_cb(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
  static bool is_waiting_read10_write10 = false; // indicate we are transferring data in READ10, WRITE10 command

  mscd_interface_t *         const p_msc = &mscd_data;
  msc_cmd_block_wrapper_t *  const p_cbw = &p_msc->cbw;
  msc_cmd_status_wrapper_t * const p_csw = &p_msc->csw;

  //------------- new CBW received -------------//
  if ( !is_waiting_read10_write10 )
  {
//    if ( endpointhandle_is_equal(p_msc->edpt_in, edpt_hdl) ) return TUSB_ERROR_NONE; // bulk in interrupt for dcd to clean up

    ASSERT( endpointhandle_is_equal(p_msc->edpt_out, edpt_hdl) &&
            xferred_bytes == sizeof(msc_cmd_block_wrapper_t)   &&
            event == TUSB_EVENT_XFER_COMPLETE                  &&
            p_cbw->signature == MSC_CBW_SIGNATURE, TUSB_ERROR_INVALID_PARA );

    p_csw->signature    = MSC_CSW_SIGNATURE;
    p_csw->tag          = p_cbw->tag;
    p_csw->data_residue = 0;

    if ( (SCSI_CMD_READ_10 != p_cbw->command[0]) && (SCSI_CMD_WRITE_10 != p_cbw->command[0]) )
    {
      void const *p_buffer = NULL;
      uint16_t actual_length = (uint16_t) p_cbw->xfer_bytes;

      // TODO SCSI data out transfer is not yet supported
      ASSERT_FALSE( p_cbw->xfer_bytes > 0 && !BIT_TEST_(p_cbw->dir, 7), TUSB_ERROR_NOT_SUPPORTED_YET);

      p_csw->status = tusbd_msc_scsi_cb(edpt_hdl.coreid, p_cbw->lun, p_cbw->command, &p_buffer, &actual_length);

      //------------- Data Phase (non READ10, WRITE10) -------------//
      if ( p_cbw->xfer_bytes )
      {
        ASSERT( p_cbw->xfer_bytes >= actual_length, TUSB_ERROR_INVALID_PARA );
        ASSERT( sizeof(p_msc->scsi_data) >= actual_length, TUSB_ERROR_NOT_ENOUGH_MEMORY); // needs to increase size for scsi_data

        endpoint_handle_t const edpt_data = BIT_TEST_(p_cbw->dir, 7) ? p_msc->edpt_in : p_msc->edpt_out;

        if ( p_buffer == NULL || actual_length == 0 )
        { // application does not provide data to response --> possibly unsupported SCSI command
          ASSERT_STATUS( dcd_pipe_stall(edpt_data) );
          p_csw->status = MSC_CSW_STATUS_FAILED;
        }else
        {
          memcpy(p_msc->scsi_data, p_buffer, actual_length);
          ASSERT_STATUS( dcd_pipe_queue_xfer( edpt_data, p_msc->scsi_data, actual_length ) );
        }
      }
    }
  }

  //------------- Data Phase For READ10 & WRITE10 (can be executed several times) -------------//
  if ( (SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0]) )
  {
    is_waiting_read10_write10 = !read10_write10_data_xfer(p_msc);
  }

  //------------- Status Phase -------------//
  // Either bulk in & out can be stalled in the data phase, dcd must make sure these queued transfer will be resumed after host clear stall
  if (!is_waiting_read10_write10)
  {
    ASSERT_STATUS( dcd_pipe_xfer( p_msc->edpt_in , (uint8_t*) p_csw, sizeof(msc_cmd_status_wrapper_t), false) );

    //------------- Queue the next CBW -------------//
    ASSERT_STATUS( dcd_pipe_xfer( p_msc->edpt_out, (uint8_t*) p_cbw, sizeof(msc_cmd_block_wrapper_t), true) );
  }

  return TUSB_ERROR_NONE;
}

// return true if data phase is complete, false if not yet complete
static bool read10_write10_data_xfer(mscd_interface_t* p_msc)
{
  msc_cmd_block_wrapper_t *  const p_cbw = &p_msc->cbw;
  msc_cmd_status_wrapper_t * const p_csw = &p_msc->csw;

  scsi_read10_t* p_readwrite = (scsi_read10_t*) &p_cbw->command; // read10 & write10 has the same format

  endpoint_handle_t const edpt_hdl = BIT_TEST_(p_cbw->dir, 7) ? p_msc->edpt_in : p_msc->edpt_out;

  uint32_t const lba         = __be2n(p_readwrite->lba);
  uint16_t const block_count = __be2n_16(p_readwrite->block_count);
  void *p_buffer = NULL;

  uint16_t xferred_block = (SCSI_CMD_READ_10 == p_cbw->command[0]) ? tusbd_msc_read10_cb (edpt_hdl.coreid, p_cbw->lun, &p_buffer, lba, block_count) :
                                                                     tusbd_msc_write10_cb(edpt_hdl.coreid, p_cbw->lun, &p_buffer, lba, block_count);
  xferred_block = min16_of(xferred_block, block_count);

  uint16_t const xferred_byte = xferred_block * (p_cbw->xfer_bytes / block_count);

  if ( 0 == xferred_block  )
  { // xferred_block is zero will cause pipe is stalled & status in CSW set to failed
    p_csw->data_residue = p_cbw->xfer_bytes;
    p_csw->status       = MSC_CSW_STATUS_FAILED;

    (void) dcd_pipe_stall(edpt_hdl);

    return true;
  } else if (xferred_block < block_count)
  {
    ASSERT_STATUS( dcd_pipe_xfer( edpt_hdl, p_buffer, xferred_byte, true) );

    // adjust lba, block_count, xfer_bytes for the next call
    p_readwrite->lba         = __n2be(lba+xferred_block);
    p_readwrite->block_count = __n2be_16(block_count - xferred_block);
    p_cbw->xfer_bytes       -= xferred_byte;

    return false;
  }else
  {
    p_csw->status = MSC_CSW_STATUS_PASSED;
    ASSERT_STATUS( dcd_pipe_queue_xfer( edpt_hdl, p_buffer, xferred_byte) );
    return true;
  }
}

#endif
