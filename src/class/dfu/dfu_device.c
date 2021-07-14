/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 XMOS LIMITED
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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_DFU_MODE)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "dfu_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
typedef struct
{
    dfu_device_status_t status;
    dfu_state_t state;
    uint8_t attrs;
    bool blk_transfer_in_proc;
    uint8_t alt_num;
    uint16_t block;
    uint16_t length;
    CFG_TUSB_MEM_ALIGN uint8_t transfer_buf[CFG_TUD_DFU_TRANSFER_BUFFER_SIZE];
} dfu_state_ctx_t;

// Only a single dfu state is allowed
CFG_TUSB_MEM_SECTION static dfu_state_ctx_t _dfu_ctx;


static void     dfu_req_dnload_setup(uint8_t rhport, tusb_control_request_t const * request);
static void     dfu_req_getstatus_reply(uint8_t rhport, tusb_control_request_t const * request);
static void     dfu_req_dnload_reply(uint8_t rhport, tusb_control_request_t const * request);
static bool     dfu_state_machine(uint8_t rhport, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= 2

static tu_lookup_entry_t const _dfu_request_lookup[] =
{
  { .key = DFU_REQUEST_DETACH         , .data = "DETACH" },
  { .key = DFU_REQUEST_DNLOAD         , .data = "DNLOAD" },
  { .key = DFU_REQUEST_UPLOAD         , .data = "UPLOAD" },
  { .key = DFU_REQUEST_GETSTATUS      , .data = "GETSTATUS" },
  { .key = DFU_REQUEST_CLRSTATUS      , .data = "CLRSTATUS" },
  { .key = DFU_REQUEST_GETSTATE       , .data = "GETSTATE" },
  { .key = DFU_REQUEST_ABORT          , .data = "ABORT" },
};

static tu_lookup_table_t const _dfu_request_table =
{
  .count = TU_ARRAY_SIZE(_dfu_request_lookup),
  .items = _dfu_request_lookup
};

static tu_lookup_entry_t const _dfu_state_lookup[] =
{
  { .key = APP_IDLE                   , .data = "APP_IDLE" },
  { .key = APP_DETACH                 , .data = "APP_DETACH" },
  { .key = DFU_IDLE                   , .data = "DFU_IDLE" },
  { .key = DFU_DNLOAD_SYNC            , .data = "DFU_DNLOAD_SYNC" },
  { .key = DFU_DNBUSY                 , .data = "DFU_DNBUSY" },
  { .key = DFU_DNLOAD_IDLE            , .data = "DFU_DNLOAD_IDLE" },
  { .key = DFU_MANIFEST_SYNC          , .data = "DFU_MANIFEST_SYNC" },
  { .key = DFU_MANIFEST               , .data = "DFU_MANIFEST" },
  { .key = DFU_MANIFEST_WAIT_RESET    , .data = "DFU_MANIFEST_WAIT_RESET" },
  { .key = DFU_UPLOAD_IDLE            , .data = "DFU_UPLOAD_IDLE" },
  { .key = DFU_ERROR                  , .data = "DFU_ERROR" },
};

static tu_lookup_table_t const _dfu_state_table =
{
  .count = TU_ARRAY_SIZE(_dfu_state_lookup),
  .items = _dfu_state_lookup
};

static tu_lookup_entry_t const _dfu_status_lookup[] =
{
  { .key = DFU_STATUS_OK              , .data = "OK" },
  { .key = DFU_STATUS_ERRTARGET       , .data = "errTARGET" },
  { .key = DFU_STATUS_ERRFILE         , .data = "errFILE" },
  { .key = DFU_STATUS_ERRWRITE        , .data = "errWRITE" },
  { .key = DFU_STATUS_ERRERASE        , .data = "errERASE" },
  { .key = DFU_STATUS_ERRCHECK_ERASED , .data = "errCHECK_ERASED" },
  { .key = DFU_STATUS_ERRPROG         , .data = "errPROG" },
  { .key = DFU_STATUS_ERRVERIFY       , .data = "errVERIFY" },
  { .key = DFU_STATUS_ERRADDRESS      , .data = "errADDRESS" },
  { .key = DFU_STATUS_ERRNOTDONE      , .data = "errNOTDONE" },
  { .key = DFU_STATUS_ERRFIRMWARE     , .data = "errFIRMWARE" },
  { .key = DFU_STATUS_ERRVENDOR       , .data = "errVENDOR" },
  { .key = DFU_STATUS_ERRUSBR         , .data = "errUSBR" },
  { .key = DFU_STATUS_ERRPOR          , .data = "errPOR" },
  { .key = DFU_STATUS_ERRUNKNOWN      , .data = "errUNKNOWN" },
  { .key = DFU_STATUS_ERRSTALLEDPKT   , .data = "errSTALLEDPKT" },
};

static tu_lookup_table_t const _dfu_status_table =
{
  .count = TU_ARRAY_SIZE(_dfu_status_lookup),
  .items = _dfu_status_lookup
};

#endif

#define dfu_debug_print_context()                                              \
{                                                                              \
  TU_LOG2("  DFU at State: %s\r\n         Status: %s\r\n",                     \
          tu_lookup_find(&_dfu_state_table, _dfu_ctx.state),        \
          tu_lookup_find(&_dfu_status_table, _dfu_ctx.status) );    \
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void dfu_moded_init(void)
{
  _dfu_ctx.state = DFU_IDLE;
  _dfu_ctx.status = DFU_STATUS_OK;
  _dfu_ctx.attrs = 0;
  _dfu_ctx.blk_transfer_in_proc = false;
  _dfu_ctx.alt_num = 0;

  dfu_debug_print_context();
}

void dfu_moded_reset(uint8_t rhport)
{
  (void) rhport;

  _dfu_ctx.state = DFU_IDLE;
  _dfu_ctx.status = DFU_STATUS_OK;
  _dfu_ctx.attrs = 0;
  _dfu_ctx.blk_transfer_in_proc = false;
  _dfu_ctx.alt_num = 0;

  dfu_debug_print_context();
}

uint16_t dfu_moded_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  (void) rhport;

  //------------- Interface (with Alt) descriptor -------------//
  uint8_t const itf_num = itf_desc->bInterfaceNumber;
  uint8_t alt_count = 0;

  uint16_t drv_len = 0;
  while(itf_desc->bInterfaceSubClass == TUD_DFU_APP_SUBCLASS && itf_desc->bInterfaceProtocol == DFU_PROTOCOL_DFU)
  {
    TU_ASSERT(max_len > drv_len, 0);

    // Alternate must have the same interface number
    TU_ASSERT(itf_desc->bInterfaceNumber == itf_num, 0);

    // Alt should increase by one every time
    TU_ASSERT(itf_desc->bAlternateSetting == alt_count, 0);
    alt_count++;

    drv_len += tu_desc_len(itf_desc);
    itf_desc = (tusb_desc_interface_t const *) tu_desc_next(itf_desc);
  }

  //------------- DFU Functional descriptor -------------//
  tusb_desc_dfu_functional_t const *func_desc = (tusb_desc_dfu_functional_t const *) itf_desc;
  TU_ASSERT(tu_desc_type(func_desc) == TUSB_DESC_FUNCTIONAL, 0);
  drv_len += sizeof(tusb_desc_dfu_functional_t);

  _dfu_ctx.attrs = func_desc->bAttributes;

  // CFG_TUD_DFU_TRANSFER_BUFFER_SIZE has to be set to the buffer size used in TUD_DFU_DESCRIPTOR
  uint16_t const transfer_size = tu_le16toh( tu_unaligned_read16(&func_desc->wTransferSize) );
  TU_ASSERT(transfer_size <= CFG_TUD_DFU_TRANSFER_BUFFER_SIZE, drv_len);

  return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool dfu_moded_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to do with DATA stage
  if ( stage == CONTROL_STAGE_DATA ) return true;

  TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

  if ( request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD )
  {
    // Standard request include GET/SET_INTERFACE
    switch ( request->bRequest )
    {
      case TUSB_REQ_SET_INTERFACE:
        if ( stage == CONTROL_STAGE_SETUP )
        {
          // Switch Alt interface and  Re-initalize state machine
          _dfu_ctx.alt_num = (uint8_t) request->wValue;
          _dfu_ctx.state = DFU_IDLE;
          _dfu_ctx.status = DFU_STATUS_OK;
          _dfu_ctx.blk_transfer_in_proc = false;

          return tud_control_status(rhport, request);
        }
      break;

      case TUSB_REQ_GET_INTERFACE:
        if(stage == CONTROL_STAGE_SETUP)
        {
          return tud_control_xfer(rhport, request, &_dfu_ctx.alt_num, 1);
        }
      break;

      // unsupported request
      default: return false;
    }
  }
  else if ( request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS )
  {
    // Class request
    switch ( request->bRequest )
    {
      case DFU_REQUEST_DETACH:
        if ( stage == CONTROL_STAGE_SETUP )
        {
          tud_control_status(rhport, request);
        }
        else if ( stage == CONTROL_STAGE_ACK )
        {
          if (tud_dfu_detach_cb) tud_dfu_detach_cb();
        }
      break;

      case DFU_REQUEST_UPLOAD:
        if ( stage == CONTROL_STAGE_SETUP )
        {
          TU_VERIFY(_dfu_ctx.attrs & DFU_FUNC_ATTR_CAN_UPLOAD_BITMASK);
          TU_VERIFY(tud_dfu_upload_cb);
          TU_VERIFY(request->wLength <= CFG_TUD_DFU_TRANSFER_BUFFER_SIZE);

          uint16_t const xfer_len = tud_dfu_upload_cb(_dfu_ctx.alt_num, request->wValue, _dfu_ctx.transfer_buf, request->wLength);

          tud_control_xfer(rhport, request, _dfu_ctx.transfer_buf, xfer_len);
        }
      break;

      case DFU_REQUEST_DNLOAD:
      {
        if ( (stage == CONTROL_STAGE_ACK)
             && ((_dfu_ctx.attrs & DFU_FUNC_ATTR_CAN_DOWNLOAD_BITMASK) != 0)
             && (_dfu_ctx.state == DFU_DNLOAD_SYNC))
        {
          _dfu_ctx.block = request->wValue;
          _dfu_ctx.length = request->wLength;
          return true;
        }
      }
      // fallthrough
      case DFU_REQUEST_GETSTATUS:
      case DFU_REQUEST_CLRSTATUS:
      case DFU_REQUEST_GETSTATE:
      case DFU_REQUEST_ABORT:
      {
        if(stage == CONTROL_STAGE_SETUP)
        {
          return dfu_state_machine(rhport, request);
        }
      }
      break;

      default:
      {
        TU_LOG2("  DFU Nonstandard Request: %u\r\n", request->bRequest);
        return false; // stall unsupported request
      }
      break;
    }
  }else
  {
    return false; // unsupported request
  }

  return true;
}

static void dfu_req_getstatus_reply(uint8_t rhport, tusb_control_request_t const * request)
{
  uint32_t timeout = 0;
  if ( tud_dfu_get_status_cb )
  {
    timeout = tud_dfu_get_status_cb(_dfu_ctx.alt_num, _dfu_ctx.state);
  }

  dfu_status_req_payload_t resp;
  resp.bStatus = _dfu_ctx.status;
  resp.bwPollTimeout[0] = TU_U32_BYTE0(timeout);
  resp.bwPollTimeout[1] = TU_U32_BYTE1(timeout);
  resp.bwPollTimeout[2] = TU_U32_BYTE2(timeout);
  resp.bState = _dfu_ctx.state;
  resp.iString = 0;

  tud_control_xfer(rhport, request, &resp, sizeof(dfu_status_req_payload_t));
}

static void dfu_req_getstate_reply(uint8_t rhport, tusb_control_request_t const * request)
{
  tud_control_xfer(rhport, request, &_dfu_ctx.state, 1);
}

static void dfu_req_dnload_setup(uint8_t rhport, tusb_control_request_t const * request)
{
  // TODO: add "zero" copy mode so the buffer we read into can be provided by the user
  // if they wish, there still will be the internal control buffer copy to this buffer
  // but this mode would provide zero copy from the class driver to the application

  TU_VERIFY( request->wLength <= CFG_TUD_DFU_TRANSFER_BUFFER_SIZE, );
  // setup for data phase
  tud_control_xfer(rhport, request, _dfu_ctx.transfer_buf, request->wLength);
}

static void dfu_req_dnload_reply(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;
  TU_VERIFY( request->wLength <= CFG_TUD_DFU_TRANSFER_BUFFER_SIZE, );
  tud_dfu_download_cb(_dfu_ctx.alt_num,_dfu_ctx.block, (uint8_t *)_dfu_ctx.transfer_buf, _dfu_ctx.length);
  _dfu_ctx.blk_transfer_in_proc = false;
}

void tud_dfu_download_complete(void)
{
  if (_dfu_ctx.state == DFU_DNBUSY)
  {
    _dfu_ctx.state = DFU_DNLOAD_SYNC;
  } else if (_dfu_ctx.state == DFU_MANIFEST)
  {
    _dfu_ctx.state = ((_dfu_ctx.attrs & DFU_FUNC_ATTR_MANIFESTATION_TOLERANT_BITMASK) == 0)
                           ? DFU_MANIFEST_WAIT_RESET : DFU_MANIFEST_SYNC;
  }
}

static bool dfu_state_machine(uint8_t rhport, tusb_control_request_t const * request)
{
  TU_LOG2("  DFU Request: %s\r\n", tu_lookup_find(&_dfu_request_table, request->bRequest));
  TU_LOG2("  DFU State Machine: %s\r\n", tu_lookup_find(&_dfu_state_table, _dfu_ctx.state));

  switch (_dfu_ctx.state)
  {
    case DFU_IDLE:
    {
      switch (request->bRequest)
      {
        case DFU_REQUEST_DNLOAD:
        {
          if( ((_dfu_ctx.attrs & DFU_FUNC_ATTR_CAN_DOWNLOAD_BITMASK) != 0)
              && (request->wLength > 0) )
          {
            _dfu_ctx.state = DFU_DNLOAD_SYNC;
            _dfu_ctx.blk_transfer_in_proc = true;
            dfu_req_dnload_setup(rhport, request);
          } else {
            _dfu_ctx.state = DFU_ERROR;
          }
        }
        break;

        case DFU_REQUEST_GETSTATUS:
          dfu_req_getstatus_reply(rhport, request);
        break;

        case DFU_REQUEST_GETSTATE:
          dfu_req_getstate_reply(rhport, request);
        break;

        case DFU_REQUEST_ABORT:
          ; // do nothing, but don't stall so continue on
        break;

        default:
          _dfu_ctx.state = DFU_ERROR;
          return false;  // stall on all other requests
        break;
      }
    }
    break;

    case DFU_DNLOAD_SYNC:
    {
      switch (request->bRequest)
      {
        case DFU_REQUEST_GETSTATUS:
        {
          if ( _dfu_ctx.blk_transfer_in_proc )
          {
            _dfu_ctx.state = DFU_DNBUSY;
            dfu_req_getstatus_reply(rhport, request);
            dfu_req_dnload_reply(rhport, request);
          } else {
            _dfu_ctx.state = DFU_DNLOAD_IDLE;
            dfu_req_getstatus_reply(rhport, request);
          }
        }
        break;

        case DFU_REQUEST_GETSTATE:
          dfu_req_getstate_reply(rhport, request);
        break;

        default:
          _dfu_ctx.state = DFU_ERROR;
          return false;  // stall on all other requests
        break;
      }
    }
    break;

    case DFU_DNBUSY:
    {
      switch (request->bRequest)
      {
        default:
          _dfu_ctx.state = DFU_ERROR;
          return false;  // stall on all other requests
        break;
      }
    }
    break;

    case DFU_DNLOAD_IDLE:
    {
        switch (request->bRequest)
        {
          case DFU_REQUEST_DNLOAD:
          {
            if( ((_dfu_ctx.attrs & DFU_FUNC_ATTR_CAN_DOWNLOAD_BITMASK) != 0)
                && (request->wLength > 0) )
            {
              _dfu_ctx.state = DFU_DNLOAD_SYNC;
              _dfu_ctx.blk_transfer_in_proc = true;
              dfu_req_dnload_setup(rhport, request);
            } else {
              if ( tud_dfu_device_data_done_check_cb(_dfu_ctx.alt_num) )
              {
                _dfu_ctx.state = DFU_MANIFEST_SYNC;
                tud_control_status(rhport, request);
              } else {
                _dfu_ctx.state = DFU_ERROR;
                return false;  // stall
              }
            }
          }
          break;

          case DFU_REQUEST_GETSTATUS:
            dfu_req_getstatus_reply(rhport, request);
          break;

          case DFU_REQUEST_GETSTATE:
            dfu_req_getstate_reply(rhport, request);
          break;

          case DFU_REQUEST_ABORT:
            if ( tud_dfu_abort_cb )
            {
              tud_dfu_abort_cb(_dfu_ctx.alt_num);
            }
            _dfu_ctx.state = DFU_IDLE;
          break;

          default:
            _dfu_ctx.state = DFU_ERROR;
            return false;  // stall on all other requests
          break;
        }
    }
    break;

    case DFU_MANIFEST_SYNC:
    {
      switch (request->bRequest)
      {
        case DFU_REQUEST_GETSTATUS:
        {
          if ((_dfu_ctx.attrs & DFU_FUNC_ATTR_MANIFESTATION_TOLERANT_BITMASK) == 0)
          {
            _dfu_ctx.state = DFU_MANIFEST;
            dfu_req_getstatus_reply(rhport, request);
          } else 
          {
            if ( tud_dfu_firmware_valid_check_cb(_dfu_ctx.alt_num) )
            {
              _dfu_ctx.state = DFU_IDLE;
            }
            dfu_req_getstatus_reply(rhport, request);
          }
        }
        break;

        case DFU_REQUEST_GETSTATE:
          dfu_req_getstate_reply(rhport, request);
        break;

        default:
          _dfu_ctx.state = DFU_ERROR;
          return false;  // stall on all other requests
        break;
      }
    }
    break;

    case DFU_MANIFEST:
    {
      switch (request->bRequest)
      {
        // stall on all other requests
        default:
          return false;
        break;
      }
    }
    break;

    case DFU_MANIFEST_WAIT_RESET:
    {
      // technically we should never even get here, but we will handle it just in case
      TU_LOG2("  DFU was in DFU_MANIFEST_WAIT_RESET and got unexpected request: %u\r\n", request->bRequest);
      switch (request->bRequest)
      {
        default:
          return false;  // stall on all other requests
        break;
      }
    }
    break;

    case DFU_UPLOAD_IDLE:
    {
      switch (request->bRequest)
      {
        case DFU_REQUEST_GETSTATUS:
          dfu_req_getstatus_reply(rhport, request);
        break;

        case DFU_REQUEST_GETSTATE:
          dfu_req_getstate_reply(rhport, request);
        break;

        case DFU_REQUEST_ABORT:
        {
          if (tud_dfu_abort_cb)
          {
            tud_dfu_abort_cb(_dfu_ctx.alt_num);
          }
          _dfu_ctx.state = DFU_IDLE;
        }
        break;

        default:
          return false;  // stall on all other requests
        break;
      }
    }
    break;

    case DFU_ERROR:
    {
      switch (request->bRequest)
      {
        case DFU_REQUEST_GETSTATUS:
          dfu_req_getstatus_reply(rhport, request);
        break;

        case DFU_REQUEST_CLRSTATUS:
          _dfu_ctx.state = DFU_IDLE;
        break;

        case DFU_REQUEST_GETSTATE:
          dfu_req_getstate_reply(rhport, request);
        break;

        default:
          return false;  // stall on all other requests
        break;
      }
    }
    break;

    default:
      _dfu_ctx.state = DFU_ERROR;
      TU_LOG2("  DFU ERROR: Unexpected state\r\nStalling control pipe\r\n");
      return false;  // Unexpected state, stall and change to error
  }

  return true;
}


#endif
