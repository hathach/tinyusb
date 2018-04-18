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

#if (MODE_DEVICE_SUPPORTED && CFG_TUD_MSC)

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
  // buffer for scsi's response other than read10 & write10.
  // NOTE should be multiple of 64 to be compatible with lpc11/13u
  uint8_t scsi_data[64];
  CFG_TUSB_MEM_ALIGN msc_cbw_t  cbw;

#if defined (__ICCARM__) && (CFG_TUSB_MCU == OPT_MCU_LPC11UXX || CFG_TUSB_MCU == OPT_MCU_LPC13UXX)
  uint8_t padding1[64-sizeof(msc_cbw_t)]; // IAR cannot align struct's member
#endif

  CFG_TUSB_MEM_ALIGN msc_csw_t csw;

  uint8_t max_lun;
  uint8_t interface_num;
  uint8_t ep_in, ep_out;

  uint8_t stage;
  uint16_t data_len;
  uint16_t xferred_len; // numbered of bytes transferred so far in the Data Stage
}mscd_interface_t;

CFG_TUSB_ATTR_USBRAM CFG_TUSB_MEM_ALIGN STATIC_VAR mscd_interface_t mscd_data;
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static bool read10_write10_data_xfer(uint8_t rhport, mscd_interface_t* p_msc);

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void mscd_init(void)
{
  memclr_(&mscd_data, sizeof(mscd_interface_t));
}

void mscd_close(uint8_t rhport)
{
  memclr_(&mscd_data, sizeof(mscd_interface_t));
}

tusb_error_t mscd_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length)
{
  VERIFY( ( MSC_SUBCLASS_SCSI == p_interface_desc->bInterfaceSubClass &&
            MSC_PROTOCOL_BOT  == p_interface_desc->bInterfaceProtocol ), TUSB_ERROR_MSC_UNSUPPORTED_PROTOCOL );

  mscd_interface_t * p_msc = &mscd_data;

  //------------- Open Data Pipe -------------//
  tusb_desc_endpoint_t const *p_endpoint = (tusb_desc_endpoint_t const *) descriptor_next( (uint8_t const*) p_interface_desc );
  for(int i=0; i<2; i++)
  {
    TU_ASSERT(TUSB_DESC_ENDPOINT == p_endpoint->bDescriptorType &&
              TUSB_XFER_BULK == p_endpoint->bmAttributes.xfer, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

    TU_ASSERT( dcd_edpt_open(rhport, p_endpoint), TUSB_ERROR_DCD_FAILED );

    if ( p_endpoint->bEndpointAddress &  TUSB_DIR_IN_MASK )
    {
      p_msc->ep_in = p_endpoint->bEndpointAddress;
    }else
    {
      p_msc->ep_out = p_endpoint->bEndpointAddress;
    }

    p_endpoint = (tusb_desc_endpoint_t const *) descriptor_next( (uint8_t const*)  p_endpoint );
  }

  p_msc->interface_num = p_interface_desc->bInterfaceNumber;

  (*p_length) += sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);

  //------------- Queue Endpoint OUT for Command Block Wrapper -------------//
  TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)), TUSB_ERROR_DCD_EDPT_XFER );

  return TUSB_ERROR_NONE;
}

tusb_error_t mscd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request)
{
  OSAL_SUBTASK_BEGIN

  tusb_error_t err;

  TU_ASSERT(p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS, TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);

  mscd_interface_t * p_msc = &mscd_data;

  if(MSC_REQUEST_RESET == p_request->bRequest)
  {
    dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
  }
  else if (MSC_REQUEST_GET_MAX_LUN == p_request->bRequest)
  {
    // Note: lpc11/13u need xfer data's address to be aligned 64 -> make use of scsi_data instead of using max_lun directly
    p_msc->scsi_data[0] = p_msc->max_lun;
    usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, p_msc->scsi_data, 1);
  }else
  {
    dcd_control_stall(rhport); // stall unsupported request
  }

  OSAL_SUBTASK_END
}

//--------------------------------------------------------------------+
// MSCD APPLICATION CALLBACK
//--------------------------------------------------------------------+
tusb_error_t mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, tusb_event_t event, uint32_t xferred_bytes)
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

      /*------------- Parse command and prepare DATA -------------*/
      p_msc->stage       = MSC_STAGE_DATA;
      p_msc->data_len    = p_cbw->xfer_bytes;
      p_msc->xferred_len = 0;

      if ( (SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0]) )
      {
        // Read10 & Write10 data len is same as CBW's xfer bytes
        read10_write10_data_xfer(rhport, p_msc);
      }
      else
      {
        // For other SCSI commands
        // 1. Zero : Invoke app callback, skip DATA and move to STATUS stage
        // 2. OUT  : queue transfer (invoke app callback there)
        // 3. IN   : invoke app callback to get response

        if ( p_cbw->xfer_bytes == 0)
        {
          p_csw->status = tud_msc_scsi_cb(rhport, p_cbw->lun, p_cbw->command, p_msc->scsi_data, &p_msc->data_len);
          p_msc->stage  = MSC_STAGE_STATUS;
        }
        else if ( !BIT_TEST_(p_cbw->dir, 7) )
        {
          TU_ASSERT( sizeof(p_msc->scsi_data) >= p_msc->data_len, TUSB_ERROR_NOT_ENOUGH_MEMORY); // needs to increase size for scsi_data
          TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, p_msc->scsi_data, p_msc->data_len), TUSB_ERROR_DCD_EDPT_XFER );
        }
        else
        {
          p_csw->status = tud_msc_scsi_cb(rhport, p_cbw->lun, p_cbw->command, p_msc->scsi_data, &p_msc->data_len);

          TU_ASSERT( p_cbw->xfer_bytes >= p_msc->data_len, TUSB_ERROR_INVALID_PARA ); // cannot return more than host expect
          TU_ASSERT( sizeof(p_msc->scsi_data) >= p_msc->data_len, TUSB_ERROR_NOT_ENOUGH_MEMORY); // needs to increase size for scsi_data

          if ( p_msc->data_len )
          {
            TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_in, p_msc->scsi_data, p_msc->data_len), TUSB_ERROR_DCD_EDPT_XFER );
          }else
          {
            // application does not provide data to response --> possibly unsupported SCSI command
            dcd_edpt_stall(rhport, p_msc->ep_in);

            p_csw->status = MSC_CSW_STATUS_FAILED;
            p_msc->stage  = MSC_STAGE_STATUS;
          }
        }
      }
    break;

    case MSC_STAGE_DATA:
      p_msc->xferred_len += xferred_bytes;

      // Still transferring
      if ( p_msc->xferred_len < p_msc->data_len )
      {
        if ( (SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0]) )
        {
          // Can be executed several times e.g write 8K bytes (several flash write)
          read10_write10_data_xfer(rhport, p_msc);
        }else
        {
          verify_breakpoint(); // unexpected error
        }
      }
      // Data Stage is complete
      else
      {
        // Invoke callback if it is not READ10, WRITE10
        if ( ! ((SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0])) )
        {
          p_csw->status = tud_msc_scsi_cb(rhport, p_cbw->lun, p_cbw->command, p_msc->scsi_data, &p_msc->data_len);
        }

        p_msc->stage = MSC_STAGE_STATUS;
      }
    break;

    case MSC_STAGE_STATUS: break; // processed immediately after this switch
    default : break;
  }

  if ( p_msc->stage == MSC_STAGE_STATUS )
  {
    // Move to default CMD stage after sending status
    p_msc->stage         = MSC_STAGE_CMD;

    TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_in , (uint8_t*) &p_msc->csw, sizeof(msc_csw_t)) );

    //------------- Queue the next CBW -------------//
    TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)) );
  }

  return TUSB_ERROR_NONE;
}

// return true if data phase is complete, false if not yet complete
static bool read10_write10_data_xfer(uint8_t rhport, mscd_interface_t* p_msc)
{
  msc_cbw_t* const p_cbw = &p_msc->cbw;
  msc_csw_t* const p_csw = &p_msc->csw;

  // read10 & write10 has the same format
  scsi_read10_t* p_readwrite = (scsi_read10_t*) &p_cbw->command;

  uint8_t const ep_data = BIT_TEST_(p_cbw->dir, 7) ? p_msc->ep_in : p_msc->ep_out;

  // LBA and Block count are in Big Endian
  uint32_t lba              = __be2n(p_readwrite->lba);
  uint16_t block_count      = __be2n_16(p_readwrite->block_count);

  uint16_t const block_size = p_cbw->xfer_bytes / block_count;

  // Adjust lba and block count according to byte transferred so far
  lba         += (p_msc->xferred_len / block_size);
  block_count -= (p_msc->xferred_len / block_size);

  void *p_buffer = NULL;
  uint16_t xfer_block;

  if (SCSI_CMD_READ_10 == p_cbw->command[0])
  {
    xfer_block = tud_msc_read10_cb (rhport, p_cbw->lun, &p_buffer, lba, block_count);
  }else
  {
    xfer_block = tud_msc_write10_cb(rhport, p_cbw->lun, &p_buffer, lba, block_count);
  }

  xfer_block = min16_of(xfer_block, block_count);

  if ( 0 == xfer_block  )
  {
    // xferred_block is zero will cause pipe is stalled & status in CSW set to failed
    p_csw->data_residue = p_cbw->xfer_bytes;
    p_csw->status       = MSC_CSW_STATUS_FAILED;

    dcd_edpt_stall(rhport, ep_data);

    return true;
  }else
  {
    TU_ASSERT( dcd_edpt_xfer(rhport, ep_data, p_buffer, xfer_block * block_size) );
  }

  return true;
}

#endif
