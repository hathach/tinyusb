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
#include "common/tusb_common.h"
#include "msc_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
enum
{
  MSC_STAGE_CMD  = 0,
  MSC_STAGE_DATA,
  MSC_STAGE_STATUS
};

typedef struct {
  uint8_t scsi_data[64]; // buffer for scsi's response other than read10 & write10. NOTE should be multiple of 64 to be compatible with lpc11/13u
  ATTR_USB_MIN_ALIGNMENT msc_cbw_t  cbw;

#if defined (__ICCARM__) && (TUSB_CFG_MCU == MCU_LPC11UXX || TUSB_CFG_MCU == MCU_LPC13UXX)
  uint8_t padding1[64-sizeof(msc_cbw_t)]; // IAR cannot align struct's member
#endif

  ATTR_USB_MIN_ALIGNMENT msc_csw_t csw;

  uint8_t max_lun;
  uint8_t interface_num;
  uint8_t ep_in, ep_out;

  uint8_t stage;
}mscd_interface_t;

TUSB_CFG_ATTR_USBRAM STATIC_VAR mscd_interface_t mscd_data;
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static bool read10_write10_data_xfer(uint8_t port, mscd_interface_t* p_msc);

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void mscd_init(void)
{
  memclr_(&mscd_data, sizeof(mscd_interface_t));
}

void mscd_close(uint8_t port)
{
  memclr_(&mscd_data, sizeof(mscd_interface_t));
}

tusb_error_t mscd_open(uint8_t port, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length)
{
  VERIFY( ( MSC_SUBCLASS_SCSI == p_interface_desc->bInterfaceSubClass &&
            MSC_PROTOCOL_BOT  == p_interface_desc->bInterfaceProtocol ), TUSB_ERROR_MSC_UNSUPPORTED_PROTOCOL );

  mscd_interface_t * p_msc = &mscd_data;

  //------------- Open Data Pipe -------------//
  tusb_descriptor_endpoint_t const *p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*) p_interface_desc );
  for(int i=0; i<2; i++)
  {
    TU_ASSERT(TUSB_DESC_ENDPOINT == p_endpoint->bDescriptorType &&
              TUSB_XFER_BULK == p_endpoint->bmAttributes.xfer, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

    TU_ASSERT( tusb_dcd_edpt_open(port, p_endpoint), TUSB_ERROR_DCD_FAILED );

    if ( p_endpoint->bEndpointAddress &  TUSB_DIR_IN_MASK )
    {
      p_msc->ep_in = p_endpoint->bEndpointAddress;
    }else
    {
      p_msc->ep_out = p_endpoint->bEndpointAddress;
    }

    p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*)  p_endpoint );
  }

  p_msc->interface_num = p_interface_desc->bInterfaceNumber;

  (*p_length) += sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);

  //------------- Queue Endpoint OUT for Command Block Wrapper -------------//
  TU_ASSERT( tusb_dcd_edpt_xfer(port, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)), TUSB_ERROR_DCD_EDPT_XFER );

  return TUSB_ERROR_NONE;
}

tusb_error_t mscd_control_request_st(uint8_t port, tusb_control_request_t const * p_request)
{
  OSAL_SUBTASK_BEGIN

  tusb_error_t err;

  TU_ASSERT(p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS, TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);

  mscd_interface_t * p_msc = &mscd_data;

  if(MSC_REQUEST_RESET == p_request->bRequest)
  {
    usbd_control_status(port, p_request->bmRequestType_bit.direction);
  }
  else if (MSC_REQUEST_GET_MAX_LUN == p_request->bRequest)
  {
    // Note: lpc11/13u need xfer data's address to be aligned 64 -> make use of scsi_data instead of using max_lun directly
    p_msc->scsi_data[0] = p_msc->max_lun;
    STASK_INVOKE( usbd_control_xfer_st(port, p_request->bmRequestType_bit.direction, p_msc->scsi_data, 1), err);
  }else
  {
    usbd_control_stall(port); // stall unsupported request
  }

  OSAL_SUBTASK_END
}

//--------------------------------------------------------------------+
// MSCD APPLICATION CALLBACK
//--------------------------------------------------------------------+
tusb_error_t mscd_xfer_cb(uint8_t port, uint8_t ep_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  mscd_interface_t* const p_msc = &mscd_data;
  msc_cbw_t*        const p_cbw = &p_msc->cbw;
  msc_csw_t*        const p_csw = &p_msc->csw;

  VERIFY( (ep_addr == p_msc->ep_out) || (ep_addr == p_msc->ep_in), TUSB_ERROR_INVALID_PARA);

  switch (p_msc->stage)
  {
    //------------- new CBW received -------------//
    case MSC_STAGE_CMD:
      // Complete IN while waiting for CMD is usually Status of previous SCSI op, ignore it
      if(ep_addr != p_msc->ep_out) return TUSB_ERROR_NONE;

      TU_ASSERT( event == TUSB_EVENT_XFER_COMPLETE  &&
                 xferred_bytes == sizeof(msc_cbw_t) && p_cbw->signature == MSC_CBW_SIGNATURE, TUSB_ERROR_INVALID_PARA );

      p_csw->signature    = MSC_CSW_SIGNATURE;
      p_csw->tag          = p_cbw->tag;
      p_csw->data_residue = 0;

      // Valid command -> move to Data Stage
      p_msc->stage = MSC_STAGE_DATA;

      // If not read10 & write10, invoke application callback
      if ( (SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0]) )
      {
        if ( read10_write10_data_xfer(port, p_msc) )
        {
          // read10 & write10 data is complete -> move to Status Stage
          p_msc->stage = MSC_STAGE_STATUS;
        }
      }
      else
      {
        void const *p_buffer = NULL;
        uint16_t actual_length = (uint16_t) p_cbw->xfer_bytes;

        // TODO SCSI data out transfer is not yet supported
        ASSERT_FALSE( p_cbw->xfer_bytes > 0 && !BIT_TEST_(p_cbw->dir, 7), TUSB_ERROR_NOT_SUPPORTED_YET);

        p_csw->status = tud_msc_scsi_cb(port, p_cbw->lun, p_cbw->command, &p_buffer, &actual_length);

        //------------- Data Phase (non READ10, WRITE10) -------------//
        if ( p_cbw->xfer_bytes )
        {
          ASSERT( p_cbw->xfer_bytes >= actual_length, TUSB_ERROR_INVALID_PARA );
          ASSERT( sizeof(p_msc->scsi_data) >= actual_length, TUSB_ERROR_NOT_ENOUGH_MEMORY); // needs to increase size for scsi_data

          uint8_t const edpt_data = BIT_TEST_(p_cbw->dir, 7) ? p_msc->ep_in : p_msc->ep_out;

          if ( p_buffer == NULL || actual_length == 0 )
          {
            // application does not provide data to response --> possibly unsupported SCSI command
            tusb_dcd_edpt_stall(port, edpt_data);
            p_csw->status = MSC_CSW_STATUS_FAILED;
          }else
          {
            memcpy(p_msc->scsi_data, p_buffer, actual_length);
            TU_ASSERT( tusb_dcd_edpt_queue_xfer(port, edpt_data, p_msc->scsi_data, actual_length), TUSB_ERROR_DCD_EDPT_XFER );
          }
        }

        // consider other SCSI is complete after one DATA transfer
        p_msc->stage = MSC_STAGE_STATUS;
      }
    break;

    case MSC_STAGE_DATA:
      // Can be executed several times e.g write 8K bytes (several flash write)
      if ( (SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0]) )
      {
        if ( read10_write10_data_xfer(port, p_msc) )
        {
          // read10 & write10 data is complete -> move to Status Stage
          p_msc->stage = MSC_STAGE_STATUS;
        }
      }
    break;

    case MSC_STAGE_STATUS: break;
    default : break;
  }

  if ( p_msc->stage == MSC_STAGE_STATUS )
  {
    // Move to default CMD stage after sending status
    p_msc->stage = MSC_STAGE_CMD;

    TU_ASSERT( tusb_dcd_edpt_xfer(port, p_msc->ep_in , (uint8_t*) &p_msc->csw, sizeof(msc_csw_t)) );

    //------------- Queue the next CBW -------------//
    TU_ASSERT( tusb_dcd_edpt_xfer(port, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)) );
  }

  return TUSB_ERROR_NONE;
}

// return true if data phase is complete, false if not yet complete
static bool read10_write10_data_xfer(uint8_t port, mscd_interface_t* p_msc)
{
  msc_cbw_t* const p_cbw = &p_msc->cbw;
  msc_csw_t* const p_csw = &p_msc->csw;

  // read10 & write10 has the same format
  scsi_read10_t* p_readwrite = (scsi_read10_t*) &p_cbw->command;

  uint8_t const ep_addr = BIT_TEST_(p_cbw->dir, 7) ? p_msc->ep_in : p_msc->ep_out;

  uint32_t const lba         = __be2n(p_readwrite->lba);
  uint16_t const block_count = __be2n_16(p_readwrite->block_count);
  void *p_buffer = NULL;

  uint16_t xfer_block;

  if (SCSI_CMD_READ_10 == p_cbw->command[0])
  {
    xfer_block = tud_msc_read10_cb (port, p_cbw->lun, &p_buffer, lba, block_count);
  }else
  {
    xfer_block = tud_msc_write10_cb(port, p_cbw->lun, &p_buffer, lba, block_count);
  }

  xfer_block = min16_of(xfer_block, block_count);

  uint16_t const xfer_byte = xfer_block * (p_cbw->xfer_bytes / block_count);

  if ( 0 == xfer_block  )
  {
    // xferred_block is zero will cause pipe is stalled & status in CSW set to failed
    p_csw->data_residue = p_cbw->xfer_bytes;
    p_csw->status       = MSC_CSW_STATUS_FAILED;

    tusb_dcd_edpt_stall(port, ep_addr);

    return true;
  } else if (xfer_block < block_count)
  {
    TU_ASSERT( tusb_dcd_edpt_xfer(port, ep_addr, p_buffer, xfer_byte), TUSB_ERROR_DCD_EDPT_XFER );

    // adjust lba, block_count, xfer_bytes for the next call
    p_readwrite->lba         = __n2be(lba+xfer_block);
    p_readwrite->block_count = __n2be_16(block_count - xfer_block);
    p_cbw->xfer_bytes       -= xfer_byte;

    return false;
  }else
  {
    p_csw->status = MSC_CSW_STATUS_PASSED;
    TU_ASSERT( tusb_dcd_edpt_queue_xfer(port, ep_addr, p_buffer, xfer_byte), TUSB_ERROR_DCD_EDPT_XFER );
    return true;
  }
}

#endif
