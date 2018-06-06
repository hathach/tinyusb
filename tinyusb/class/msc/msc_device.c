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
  CFG_TUSB_MEM_ALIGN msc_cbw_t  cbw;

#if defined (__ICCARM__) && (CFG_TUSB_MCU == OPT_MCU_LPC11UXX || CFG_TUSB_MCU == OPT_MCU_LPC13UXX)
  uint8_t padding1[64-sizeof(msc_cbw_t)]; // IAR cannot align struct's member
#endif

  CFG_TUSB_MEM_ALIGN msc_csw_t csw;

  uint8_t  itf_num;
  uint8_t  ep_in;
  uint8_t  ep_out;

  uint8_t  stage;
  uint16_t data_len;
  uint16_t xferred_len; // numbered of bytes transferred so far in the Data Stage
}mscd_interface_t;

CFG_TUSB_ATTR_USBRAM CFG_TUSB_MEM_ALIGN static mscd_interface_t _mscd_itf;
CFG_TUSB_ATTR_USBRAM CFG_TUSB_MEM_ALIGN static uint8_t _mscd_buf[CFG_TUD_MSC_BUFSIZE];

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static void proc_read10_write10(uint8_t rhport, mscd_interface_t* p_msc);

static inline uint32_t rdwr10_get_lba(uint8_t const command[])
{
  // read10 & write10 has the same format
  scsi_write10_t* p_rdwr10 = (scsi_write10_t*) command;

  // copy first to prevent mis-aligned access
  uint32_t lba;
  memcpy(&lba, &p_rdwr10->lba, 4);

  return  __be2n(lba);
}

static inline uint16_t rdwr10_get_blockcount(uint8_t const command[])
{
  // read10 & write10 has the same format
  scsi_write10_t* p_rdwr10 = (scsi_write10_t*) command;

  // copy first to prevent mis-aligned access
  uint16_t block_count;
  memcpy(&block_count, &p_rdwr10->block_count, 2);

  return __be2n(block_count);
}


//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void mscd_init(void)
{
  memclr_(&_mscd_itf, sizeof(mscd_interface_t));
}

void mscd_close(uint8_t rhport)
{
  memclr_(&_mscd_itf, sizeof(mscd_interface_t));
}

tusb_error_t mscd_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length)
{
  VERIFY( ( MSC_SUBCLASS_SCSI == p_interface_desc->bInterfaceSubClass &&
            MSC_PROTOCOL_BOT  == p_interface_desc->bInterfaceProtocol ), TUSB_ERROR_MSC_UNSUPPORTED_PROTOCOL );

  mscd_interface_t * p_msc = &_mscd_itf;

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

  p_msc->itf_num = p_interface_desc->bInterfaceNumber;

  (*p_length) += sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);

  //------------- Queue Endpoint OUT for Command Block Wrapper -------------//
  TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)), TUSB_ERROR_DCD_EDPT_XFER );

  return TUSB_ERROR_NONE;
}

tusb_error_t mscd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request)
{
  OSAL_SUBTASK_BEGIN

  TU_ASSERT(p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS, TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);

  if(MSC_REQUEST_RESET == p_request->bRequest)
  {
    dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
  }
  else if (MSC_REQUEST_GET_MAX_LUN == p_request->bRequest)
  {
    // returned MAX LUN is minus 1 by specs
    _mscd_buf[0] = CFG_TUD_MSC_MAXLUN-1;
    usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _mscd_buf, 1);
  }else
  {
    dcd_control_stall(rhport); // stall unsupported request
  }

  OSAL_SUBTASK_END
}

tusb_error_t mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  mscd_interface_t* p_msc = &_mscd_itf;
  msc_cbw_t const * p_cbw = &p_msc->cbw;
  msc_csw_t       * p_csw = &p_msc->csw;

  VERIFY( (ep_addr == p_msc->ep_out) || (ep_addr == p_msc->ep_in), TUSB_ERROR_INVALID_PARA);

  switch (p_msc->stage)
  {
    case MSC_STAGE_CMD:
      //------------- new CBW received -------------//

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
        proc_read10_write10(rhport, p_msc);
      }
      else
      {
        // For other SCSI commands
        // 1. Zero : Invoke app callback, skip DATA and move to STATUS stage
        // 2. OUT  : queue transfer (invoke app callback after done)
        // 3. IN   : invoke app callback to get response

        if ( p_cbw->xfer_bytes == 0)
        {
          p_msc->data_len = tud_msc_scsi_cb(rhport, p_cbw->lun, p_cbw->command, NULL, 0);
          p_csw->status   = (p_msc->data_len == 0) ? MSC_CSW_STATUS_PASSED : MSC_CSW_STATUS_FAILED;
          p_msc->stage    = MSC_STAGE_STATUS;

          TU_ASSERT( p_msc->data_len == 0, TUSB_ERROR_INVALID_PARA);
        }
        else if ( !BIT_TEST_(p_cbw->dir, 7) )
        {
          // OUT transfer
          TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, _mscd_buf, p_msc->data_len), TUSB_ERROR_DCD_EDPT_XFER );
        }
        else
        {
          p_msc->data_len = tud_msc_scsi_cb(rhport, p_cbw->lun, p_cbw->command, _mscd_buf, p_msc->data_len);
          p_csw->status   = (p_msc->data_len >= 0) ? MSC_CSW_STATUS_PASSED : MSC_CSW_STATUS_FAILED;

          TU_ASSERT( p_cbw->xfer_bytes >= p_msc->data_len, TUSB_ERROR_INVALID_PARA ); // cannot return more than host expect

          if ( p_msc->data_len )
          {
            TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_in, _mscd_buf, p_msc->data_len), TUSB_ERROR_DCD_EDPT_XFER );
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
      // OUT transfer, invoke callback if needed
      if ( !BIT_TEST_(p_cbw->dir, 7) )
      {
        if ( SCSI_CMD_WRITE_10 == p_cbw->command[0] )
        {
          uint32_t lba = rdwr10_get_lba(p_cbw->command);

          tud_msc_write10_cb(rhport, p_cbw->lun, lba, p_msc->xferred_len, _mscd_buf, xferred_bytes);
        }
        else
        {
          p_csw->status = (tud_msc_scsi_cb(rhport, p_cbw->lun, p_cbw->command, _mscd_buf, p_msc->data_len) >= 0 ) ? MSC_CSW_STATUS_PASSED : MSC_CSW_STATUS_FAILED;
        }
      }

      /*------------- Prepare for DATA transfer if not complete yet -------------*/
      p_msc->xferred_len += xferred_bytes;

      if ( p_msc->xferred_len >= p_msc->data_len )
      {
        // Data Stage is complete
        p_msc->stage = MSC_STAGE_STATUS;
      }
      else
      {
        if ( (SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0]) )
        {
          // Can be executed several times e.g write 8K bytes (several flash write)
          proc_read10_write10(rhport, p_msc);
        }else
        {
          // No other command take more than one transfer yet -> unlikely error
          verify_breakpoint();
        }
      }
    break;

    case MSC_STAGE_STATUS: break; // processed immediately after this switch
    default : break;
  }

  if ( p_msc->stage == MSC_STAGE_STATUS )
  {
    // Invoke Complete Callback if defined
    if ( SCSI_CMD_READ_10 == p_cbw->command[0])
    {
      if ( tud_msc_read10_complete_cb ) tud_msc_read10_complete_cb(rhport, p_cbw->lun);
    }else if ( SCSI_CMD_WRITE_10 == p_cbw->command[0] )
    {
      if ( tud_msc_write10_complete_cb ) tud_msc_write10_complete_cb(rhport, p_cbw->lun);
    }else
    {
      if ( tud_msc_scsi_complete_cb ) tud_msc_scsi_complete_cb(rhport, p_cbw->lun, p_cbw->command);
    }

    // Move to default CMD stage after sending status
    p_msc->stage         = MSC_STAGE_CMD;

    TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_in , (uint8_t*) &p_msc->csw, sizeof(msc_csw_t)) );

    //------------- Queue the next CBW -------------//
    TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)) );
  }

  return TUSB_ERROR_NONE;
}

static void proc_read10_write10(uint8_t rhport, mscd_interface_t* p_msc)
{
  msc_cbw_t const * p_cbw = &p_msc->cbw;
  msc_csw_t       * p_csw = &p_msc->csw;

  uint8_t const ep_data = BIT_TEST_(p_cbw->dir, 7) ? p_msc->ep_in : p_msc->ep_out;

  // LBA and Block count are in Big Endian. Use memcpy first to prevent mis-aligned access
  uint32_t lba = rdwr10_get_lba(p_cbw->command);
  uint16_t block_count = rdwr10_get_blockcount(p_cbw->command);

  int32_t xfer_bytes = (int32_t) min32_of(sizeof(_mscd_buf), p_cbw->xfer_bytes-p_msc->xferred_len);

  // Write10 callback will be called later when usb transfer complete
  if (SCSI_CMD_READ_10 == p_cbw->command[0])
  {
    xfer_bytes = tud_msc_read10_cb (rhport, p_cbw->lun, lba, p_msc->xferred_len, _mscd_buf, (uint32_t) xfer_bytes);
  }

  if ( xfer_bytes < 0 )
  {
    // negative is error -> pipe is stalled & status in CSW set to failed
    p_csw->data_residue = p_cbw->xfer_bytes - p_msc->xferred_len;
    p_csw->status       = MSC_CSW_STATUS_FAILED;

    dcd_edpt_stall(rhport, ep_data);
  }
  else if ( xfer_bytes == 0 )
  {
    // zero is not ready -> try again later by simulate an transfer complete
    dcd_xfer_complete(rhport, ep_data, 0, true);
  }
  else
  {
    TU_ASSERT( dcd_edpt_xfer(rhport, ep_data, _mscd_buf, xfer_bytes), );
  }
}

#endif
