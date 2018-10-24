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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_MSC)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#define _TINY_USB_SOURCE_FILE_

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

//#if defined (__ICCARM__) && (CFG_TUSB_MCU == OPT_MCU_LPC11UXX || CFG_TUSB_MCU == OPT_MCU_LPC13UXX)
//  uint8_t padding1[64-sizeof(msc_cbw_t)]; // IAR cannot align struct's member
//#endif

  CFG_TUSB_MEM_ALIGN msc_csw_t csw;

  uint8_t  itf_num;
  uint8_t  ep_in;
  uint8_t  ep_out;

  // Bulk Only Transfer (BOT) Protocol
  uint8_t  stage;
  uint32_t total_len;
  uint32_t xferred_len; // numbered of bytes transferred so far in the Data Stage

  // Sense Response Data
  uint8_t sense_key;
  uint8_t add_sense_code;
  uint8_t add_sense_qualifier;
}mscd_interface_t;

CFG_TUSB_ATTR_USBRAM CFG_TUSB_MEM_ALIGN static mscd_interface_t _mscd_itf;
CFG_TUSB_ATTR_USBRAM CFG_TUSB_MEM_ALIGN static uint8_t _mscd_buf[CFG_TUD_MSC_BUFSIZE];

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static void proc_read10_cmd(uint8_t rhport, mscd_interface_t* p_msc);
static void proc_write10_cmd(uint8_t rhport, mscd_interface_t* p_msc);

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

  return __be2n_16(block_count);
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_msc_ready(void)
{
  return ( _mscd_itf.ep_in != 0 ) && ( _mscd_itf.ep_out != 0 ) ;
}

bool tud_msc_set_sense(uint8_t lun, uint8_t sense_key, uint8_t add_sense_code, uint8_t add_sense_qualifier)
{
  (void) lun;

  _mscd_itf.sense_key           = sense_key;
  _mscd_itf.add_sense_code      = add_sense_code;
  _mscd_itf.add_sense_qualifier = add_sense_qualifier;

  return true;
}


//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void mscd_init(void)
{
  tu_memclr(&_mscd_itf, sizeof(mscd_interface_t));
}

void mscd_reset(uint8_t rhport)
{
  tu_memclr(&_mscd_itf, sizeof(mscd_interface_t));
}

tusb_error_t mscd_open(uint8_t rhport, tusb_desc_interface_t const * p_desc_itf, uint16_t *p_len)
{
  // only support SCSI's BOT protocol
  TU_VERIFY( ( MSC_SUBCLASS_SCSI == p_desc_itf->bInterfaceSubClass &&
            MSC_PROTOCOL_BOT  == p_desc_itf->bInterfaceProtocol ), TUSB_ERROR_MSC_UNSUPPORTED_PROTOCOL );

  mscd_interface_t * p_msc = &_mscd_itf;

  // Open endpoint pair with usbd helper
  tusb_desc_endpoint_t const *p_desc_ep = (tusb_desc_endpoint_t const *) descriptor_next( (uint8_t const*) p_desc_itf );
  TU_ASSERT_ERR( usbd_open_edpt_pair(rhport, p_desc_ep, TUSB_XFER_BULK, &p_msc->ep_out, &p_msc->ep_in) );

  p_msc->itf_num = p_desc_itf->bInterfaceNumber;
  (*p_len) = sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);

  //------------- Queue Endpoint OUT for Command Block Wrapper -------------//
  TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)), TUSB_ERROR_DCD_EDPT_XFER );

  return TUSB_ERROR_NONE;
}

tusb_error_t mscd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request)
{
  OSAL_SUBTASK_BEGIN

  TU_ASSERT(p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS, TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);

  if(MSC_REQ_RESET == p_request->bRequest)
  {
    dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
  }
  else if (MSC_REQ_GET_MAX_LUN == p_request->bRequest)
  {
    // returned MAX LUN is minus 1 by specs
    _usbd_ctrl_buf[0] = CFG_TUD_MSC_MAXLUN-1;
    usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, _usbd_ctrl_buf, 1);
  }else
  {
    dcd_control_stall(rhport); // stall unsupported request
  }

  OSAL_SUBTASK_END
}

// return length of response (copied to buffer), -1 if it is not an built-in commands
int32_t proc_builtin_scsi(msc_cbw_t const * p_cbw, uint8_t* buffer, uint32_t bufsize)
{
  int32_t ret;

  switch ( p_cbw->command[0] )
  {
    case SCSI_CMD_READ_CAPACITY_10:
    {
      scsi_read_capacity10_resp_t read_capa10 =
      {
          .last_lba   = ENDIAN_BE(CFG_TUD_MSC_BLOCK_NUM-1), // read capacity
          .block_size = ENDIAN_BE(CFG_TUD_MSC_BLOCK_SZ)
      };

      ret = sizeof(read_capa10);
      memcpy(buffer, &read_capa10, ret);
    }
    break;

    case SCSI_CMD_READ_FORMAT_CAPACITY:
    {
      scsi_read_format_capacity_data_t read_fmt_capa =
      {
          .list_length     = 8,
          .block_num       = ENDIAN_BE(CFG_TUD_MSC_BLOCK_NUM),  // write capacity
          .descriptor_type = 2,                                 // formatted media
          .block_size_u16  = ENDIAN_BE16(CFG_TUD_MSC_BLOCK_SZ)
      };

      ret = sizeof(read_fmt_capa);
      memcpy(buffer, &read_fmt_capa, ret);
    }
    break;

    case SCSI_CMD_INQUIRY:
    {
      scsi_inquiry_resp_t inquiry_rsp =
      {
          .is_removable         = 1,
          .version              = 2,
          .response_data_format = 2,
          .vendor_id            = "Adafruit",
          .product_id           = "Feather52840",
          .product_rev          = "1.0"
      };

      strncpy((char*) inquiry_rsp.vendor_id  , CFG_TUD_MSC_VENDOR     , sizeof(inquiry_rsp.vendor_id));
      strncpy((char*) inquiry_rsp.product_id , CFG_TUD_MSC_PRODUCT    , sizeof(inquiry_rsp.product_id));
      strncpy((char*) inquiry_rsp.product_rev, CFG_TUD_MSC_PRODUCT_REV, sizeof(inquiry_rsp.product_rev));

      ret = sizeof(inquiry_rsp);
      memcpy(buffer, &inquiry_rsp, ret);
    }
    break;

    case SCSI_CMD_MODE_SENSE_6:
    {
      scsi_mode_sense6_resp_t const mode_resp = {
        .data_len = 3,
        .medium_type = 0,
        .device_specific_para = 0,
        .block_descriptor_len = 0    // no block descriptor are included
      };

      ret = sizeof(mode_resp);
      memcpy(buffer, &mode_resp, ret);
    }
    break;

    case SCSI_CMD_REQUEST_SENSE:
    {
      scsi_sense_fixed_resp_t sense_rsp =
      {
          .response_code = 0x70,
          .valid         = 1
      };

      sense_rsp.add_sense_len = sizeof(scsi_sense_fixed_resp_t) - 8;

      sense_rsp.sense_key           = _mscd_itf.sense_key;
      sense_rsp.add_sense_code      = _mscd_itf.add_sense_code;
      sense_rsp.add_sense_qualifier = _mscd_itf.add_sense_qualifier;

      ret = sizeof(sense_rsp);
      memcpy(buffer, &sense_rsp, ret);

      // Clear sense data after copy
      tud_msc_set_sense(p_cbw->lun, 0, 0, 0);
    }
    break;

    default: ret = -1; break;
  }

  return ret;
}

tusb_error_t mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  mscd_interface_t* p_msc = &_mscd_itf;
  msc_cbw_t const * p_cbw = &p_msc->cbw;
  msc_csw_t       * p_csw = &p_msc->csw;

  switch (p_msc->stage)
  {
    case MSC_STAGE_CMD:
      //------------- new CBW received -------------//
      // Complete IN while waiting for CMD is usually Status of previous SCSI op, ignore it
      if(ep_addr != p_msc->ep_out) return TUSB_ERROR_NONE;

      TU_ASSERT( event == DCD_XFER_SUCCESS  &&
                 xferred_bytes == sizeof(msc_cbw_t) && p_cbw->signature == MSC_CBW_SIGNATURE, TUSB_ERROR_INVALID_PARA );

      p_csw->signature    = MSC_CSW_SIGNATURE;
      p_csw->tag          = p_cbw->tag;
      p_csw->data_residue = 0;

      /*------------- Parse command and prepare DATA -------------*/
      p_msc->stage = MSC_STAGE_DATA;
      p_msc->total_len = p_cbw->total_bytes;
      p_msc->xferred_len = 0;

      if (SCSI_CMD_READ_10 == p_cbw->command[0])
      {
        proc_read10_cmd(rhport, p_msc);
      }
      else if (SCSI_CMD_WRITE_10 == p_cbw->command[0])
      {
        proc_write10_cmd(rhport, p_msc);
      }
      else
      {
        // For other SCSI commands
        // 1. Zero : Invoke app callback, skip DATA and move to STATUS stage
        // 2. OUT  : queue transfer (invoke app callback after done)
        // 3. IN   : invoke app callback to get response
        if ( p_cbw->total_bytes == 0)
        {
          int32_t const cb_result = tud_msc_scsi_cb(p_cbw->lun, p_cbw->command, NULL, 0);

          p_msc->total_len = 0;
          p_msc->stage = MSC_STAGE_STATUS;

          if ( cb_result < 0 )
          {
            p_csw->status = MSC_CSW_STATUS_FAILED;
            tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Sense = Invalid Command Operation
          }
          else
          {
            p_csw->status = MSC_CSW_STATUS_PASSED;
          }
        }
        else if ( !BIT_TEST_(p_cbw->dir, 7) )
        {
          // OUT transfer
          TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, _mscd_buf, p_msc->total_len), TUSB_ERROR_DCD_EDPT_XFER );
        }
        else
        {
          // IN Transfer
          int32_t cb_result;

          // first process if it is a built-in commands
          cb_result = proc_builtin_scsi(p_cbw, _mscd_buf, sizeof(_mscd_buf));

          // Not an built-in command, invoke user callback
          if ( cb_result < 0 )
          {
            cb_result = tud_msc_scsi_cb(p_cbw->lun, p_cbw->command, _mscd_buf, p_msc->total_len);
          }

          if ( cb_result > 0 )
          {
            p_msc->total_len = (uint32_t) cb_result;
            p_csw->status = MSC_CSW_STATUS_PASSED;

            TU_ASSERT( p_cbw->total_bytes >= p_msc->total_len, TUSB_ERROR_INVALID_PARA ); // cannot return more than host expect
            TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_in, _mscd_buf, p_msc->total_len), TUSB_ERROR_DCD_EDPT_XFER );
          }else
          {
            p_msc->total_len = 0;
            p_csw->status = MSC_CSW_STATUS_FAILED;
            p_msc->stage = MSC_STAGE_STATUS;

            tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Sense = Invalid Command Operation
            dcd_edpt_stall(rhport, p_msc->ep_in);
          }
        }
      }
    break;

    case MSC_STAGE_DATA:
      // OUT transfer, invoke callback if needed
      if ( !BIT_TEST_(p_cbw->dir, 7) )
      {
        if ( SCSI_CMD_WRITE_10 != p_cbw->command[0] )
        {
          int32_t cb_result = tud_msc_scsi_cb(p_cbw->lun, p_cbw->command, _mscd_buf, p_msc->total_len);

          if ( cb_result < 0 )
          {
            p_csw->status = MSC_CSW_STATUS_FAILED;
            tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Sense = Invalid Command Operation
          }else
          {
            p_csw->status = MSC_CSW_STATUS_PASSED;
          }
        }
        else
        {
          uint16_t const block_sz = p_cbw->total_bytes / rdwr10_get_blockcount(p_cbw->command);

          // Adjust lba with transferred bytes
          uint32_t const lba = rdwr10_get_lba(p_cbw->command) + (p_msc->xferred_len / block_sz);

          // Application can consume smaller bytes
          int32_t nbytes = tud_msc_write10_cb(p_cbw->lun, lba, p_msc->xferred_len % block_sz, _mscd_buf, xferred_bytes);

          if ( nbytes < 0 )
          {
            // negative means error -> skip to status phase, status in CSW set to failed
            p_csw->data_residue = p_cbw->total_bytes - p_msc->xferred_len;
            p_csw->status       = MSC_CSW_STATUS_FAILED;
            p_msc->stage        = MSC_STAGE_STATUS;

            tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Sense = Invalid Command Operation
            break;
          }else
          {
            // Application consume less than what we got (including zero)
            if ( nbytes < xferred_bytes )
            {
              if ( nbytes > 0 )
              {
                p_msc->xferred_len += nbytes;
                memmove(_mscd_buf, _mscd_buf+nbytes, xferred_bytes-nbytes);
              }

              // simulate an transfer complete with adjusted parameters --> this driver callback will fired again
              dcd_event_xfer_complete(rhport, p_msc->ep_out, xferred_bytes-nbytes, DCD_XFER_SUCCESS, false);

              return TUSB_ERROR_NONE; // skip the rest
            }
            else
            {
              // Application consume all bytes in our buffer. Nothing to do, process with normal flow
            }
          }
        }
      }

      // Accumulate data so far
      p_msc->xferred_len += xferred_bytes;

      if ( p_msc->xferred_len >= p_msc->total_len )
      {
        // Data Stage is complete
        p_msc->stage = MSC_STAGE_STATUS;
      }
      else
      {
        // READ10 & WRITE10 Can be executed with large bulk of data e.g write 8K bytes (several flash write)
        // We break it into multiple smaller command whose data size is up to CFG_TUD_MSC_BUFSIZE
        if (SCSI_CMD_READ_10 == p_cbw->command[0])
        {
          proc_read10_cmd(rhport, p_msc);
        }
        else if (SCSI_CMD_WRITE_10 == p_cbw->command[0])
        {
          proc_write10_cmd(rhport, p_msc);
        }else
        {
          // No other command take more than one transfer yet -> unlikely error
          TU_BREAKPOINT();
        }
      }
    break;

    case MSC_STAGE_STATUS: break; // processed immediately after this switch
    default : break;
  }

  if ( p_msc->stage == MSC_STAGE_STATUS )
  {
    // Either endpoints is stalled, need to wait until it is cleared by host
    if ( dcd_edpt_stalled(rhport,  p_msc->ep_in) || dcd_edpt_stalled(rhport,  p_msc->ep_out) )
    {
      // simulate an transfer complete with adjusted parameters --> this driver callback will fired again
      dcd_event_xfer_complete(rhport, p_msc->ep_out, 0, DCD_XFER_SUCCESS, false);
    }
    else
    {
      // Invoke complete callback if defined
      if ( SCSI_CMD_READ_10 == p_cbw->command[0])
      {
        if ( tud_msc_read10_complete_cb ) tud_msc_read10_complete_cb(p_cbw->lun);
      }
      else if ( SCSI_CMD_WRITE_10 == p_cbw->command[0] )
      {
        if ( tud_msc_write10_complete_cb ) tud_msc_write10_complete_cb(p_cbw->lun);
      }
      else
      {
        if ( tud_msc_scsi_complete_cb ) tud_msc_scsi_complete_cb(p_cbw->lun, p_cbw->command);
      }

      // Move to default CMD stage after sending status
      p_msc->stage         = MSC_STAGE_CMD;

      TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_in , (uint8_t*) &p_msc->csw, sizeof(msc_csw_t)) );

      //------------- Queue the next CBW -------------//
      TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)) );
    }
  }

  return TUSB_ERROR_NONE;
}

/*------------------------------------------------------------------*/
/* SCSI Command Process
 *------------------------------------------------------------------*/
static void proc_read10_cmd(uint8_t rhport, mscd_interface_t* p_msc)
{
  msc_cbw_t const * p_cbw = &p_msc->cbw;
  msc_csw_t       * p_csw = &p_msc->csw;

  uint16_t const block_sz = p_cbw->total_bytes / rdwr10_get_blockcount(p_cbw->command);

  // Adjust lba with transferred bytes
  uint32_t const lba = rdwr10_get_lba(p_cbw->command) + (p_msc->xferred_len / block_sz);

  // remaining bytes capped at class buffer
  int32_t nbytes = (int32_t) tu_min32(sizeof(_mscd_buf), p_cbw->total_bytes-p_msc->xferred_len);

  // Application can consume smaller bytes
  nbytes = tud_msc_read10_cb(p_cbw->lun, lba, p_msc->xferred_len % block_sz, _mscd_buf, (uint32_t) nbytes);

  if ( nbytes < 0 )
  {
    // negative means error -> pipe is stalled & status in CSW set to failed
    p_csw->data_residue = p_cbw->total_bytes - p_msc->xferred_len;
    p_csw->status       = MSC_CSW_STATUS_FAILED;

    tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Sense = Invalid Command Operation
    dcd_edpt_stall(rhport, p_msc->ep_in);
  }
  else if ( nbytes == 0 )
  {
    // zero means not ready -> simulate an transfer complete so that this driver callback will fired again
    dcd_event_xfer_complete(rhport, p_msc->ep_in, 0, DCD_XFER_SUCCESS, false);
  }
  else
  {
    TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_in, _mscd_buf, nbytes), );
  }
}

static void proc_write10_cmd(uint8_t rhport, mscd_interface_t* p_msc)
{
  msc_cbw_t const * p_cbw = &p_msc->cbw;

  // remaining bytes capped at class buffer
  int32_t nbytes = (int32_t) tu_min32(sizeof(_mscd_buf), p_cbw->total_bytes-p_msc->xferred_len);

  // Write10 callback will be called later when usb transfer complete
  TU_ASSERT( dcd_edpt_xfer(rhport, p_msc->ep_out, _mscd_buf, nbytes), );
}

#endif
