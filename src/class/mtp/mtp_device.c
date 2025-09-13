/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ennebi Elettronica (https://ennebielettronica.com)
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

#if (CFG_TUD_ENABLED && CFG_TUD_MTP)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "device/dcd.h"         // for faking dcd_event_xfer_complete
#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "mtp_device.h"
#include "mtp_device_storage.h"

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUD_MTP_LOG_LEVEL
  #define CFG_TUD_MTP_LOG_LEVEL   CFG_TUD_LOG_LEVEL
#endif

#define TU_LOG_DRV(...)   TU_LOG(CFG_TUD_MTP_LOG_LEVEL, __VA_ARGS__)

//--------------------------------------------------------------------+
// STRUCT
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;
  uint8_t ep_evt;

  // Bulk Only Transfer (BOT) Protocol
  uint8_t  phase;

  uint32_t queued_len;  // number of bytes queued from the DataIN Stage
  uint32_t total_len;   // byte to be transferred, can be smaller than total_bytes in cbw
  uint32_t xferred_len; // number of bytes transferred so far in the Data Stage
  uint32_t handled_len; // number of bytes already handled in the Data Stage
  bool     xfer_completed; // true when DATA-IN/DATA-OUT transfer is completed

} mtpd_interface_t;

typedef struct
{
  uint32_t session_id;
  uint32_t transaction_id;
} mtpd_context_t;

//--------------------------------------------------------------------+
// INTERNAL FUNCTION DECLARATION
//--------------------------------------------------------------------+
// Checker
tu_static mtp_phase_type_t mtpd_chk_generic(const char *func_name, const bool err_cd, const uint16_t ret_code, const char *message);
tu_static mtp_phase_type_t mtpd_chk_session_open(const char *func_name);

// MTP commands
tu_static mtp_phase_type_t mtpd_handle_cmd(void);
tu_static mtp_phase_type_t mtpd_handle_data(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_get_device_info(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_open_session(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_close_session(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_get_storage_info(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_get_storage_ids(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_get_object_handles(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_get_object_info(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_get_object(void);
tu_static mtp_phase_type_t mtpd_handle_dti_get_object(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_delete_object(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_get_device_prop_desc(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_get_device_prop_value(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_send_object_info(void);
tu_static mtp_phase_type_t mtpd_handle_dto_send_object_info(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_send_object(void);
tu_static mtp_phase_type_t mtpd_handle_dto_send_object(void);
tu_static mtp_phase_type_t mtpd_handle_cmd_format_store(void);

//--------------------------------------------------------------------+
// MTP variable declaration
//--------------------------------------------------------------------+
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static mtpd_interface_t _mtpd_itf;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static mtp_generic_container_t _mtpd_gct;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static mtpd_context_t _mtpd_ctx;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static mtp_device_status_res_t _mtpd_device_status_res;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static uint32_t _mtpd_get_object_handle;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static mtp_basic_object_info_t _mtpd_soi;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN tu_static char _mtp_datestr[20];

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void mtpd_init(void) {
  TU_LOG_DRV("  MTP mtpd_init\n");
  tu_memclr(&_mtpd_itf, sizeof(mtpd_interface_t));
  tu_memclr(&_mtpd_ctx, sizeof(mtpd_context_t));
  _mtpd_get_object_handle = 0;
  tu_memclr(&_mtpd_soi, sizeof(mtp_basic_object_info_t));
}

bool mtpd_deinit(void) {
  TU_LOG_DRV("  MTP mtpd_deinit\n");
  // nothing to do
  return true;
}

void mtpd_reset(uint8_t rhport)
{
  TU_LOG_DRV("  MTP mtpd_reset\n");
  (void) rhport;

  // Close all endpoints
  dcd_edpt_close_all(rhport);
  tu_memclr(&_mtpd_itf, sizeof(mtpd_interface_t));
  tu_memclr(&_mtpd_ctx, sizeof(mtpd_context_t));
  tu_memclr(&_mtpd_gct, sizeof(mtp_generic_container_t));
  _mtpd_get_object_handle = 0;
  tu_memclr(&_mtpd_soi, sizeof(mtp_basic_object_info_t));
}

uint16_t mtpd_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
{
  TU_LOG_DRV("  MTP mtpd_open\n");
  tusb_desc_endpoint_t const *ep_desc;
  // only support SCSI's BOT protocol
  TU_VERIFY(TUSB_CLASS_IMAGE          == itf_desc->bInterfaceClass &&
            MTP_SUBCLASS              == itf_desc->bInterfaceSubClass &&
            MTP_PROTOCOL_STILL_IMAGE  == itf_desc->bInterfaceProtocol, 0);

  // mtp driver length is fixed
  uint16_t const mtpd_itf_size = sizeof(tusb_desc_interface_t) + 3 * sizeof(tusb_desc_endpoint_t);

  // Max length must be at least 1 interface + 3 endpoints
  TU_ASSERT(itf_desc->bNumEndpoints == 3 && max_len >= mtpd_itf_size);

  _mtpd_itf.itf_num = itf_desc->bInterfaceNumber;

  // Open interrupt IN endpoint
  ep_desc = (tusb_desc_endpoint_t const *)tu_desc_next(itf_desc);
  TU_ASSERT(ep_desc->bDescriptorType == TUSB_DESC_ENDPOINT && ep_desc->bmAttributes.xfer == TUSB_XFER_INTERRUPT, 0);
  TU_ASSERT(usbd_edpt_open(rhport, ep_desc), 0);
  _mtpd_itf.ep_evt = ep_desc->bEndpointAddress;

  // Open endpoint pair
  TU_ASSERT( usbd_open_edpt_pair(rhport, tu_desc_next(ep_desc), 2, TUSB_XFER_BULK, &_mtpd_itf.ep_out, &_mtpd_itf.ep_in), 0 );

  // Prepare rx on bulk out EP
  TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct)), CFG_MTP_EP_SIZE), 0);

  return mtpd_itf_size;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool mtpd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
  TU_LOG_DRV("  MTP mtpd_control_xfer_cb: bmRequest=0x%2x, bRequest=0x%2x\n", request->bmRequestType, request->bRequest);
  // nothing to do with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  uint16_t len = 0;

  switch ( request->bRequest )
  {
    case MTP_REQ_CANCEL:
      TU_LOG_DRV("  MTP request: MTP_REQ_CANCEL\n");
      tud_mtp_storage_cancel();
    break;
    case MTP_REQ_GET_EXT_EVENT_DATA:
      TU_LOG_DRV("  MTP request: MTP_REQ_GET_EXT_EVENT_DATA\n");
    break;
    case MTP_REQ_RESET:
      TU_LOG_DRV("  MTP request: MTP_REQ_RESET\n");
      tud_mtp_storage_reset();
      // Prepare for a new command
      TU_ASSERT( usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct)), CFG_MTP_EP_SIZE) );
    break;
    case MTP_REQ_GET_DEVICE_STATUS:
      TU_LOG_DRV("  MTP request: MTP_REQ_GET_DEVICE_STATUS\n");
      len = 4;
      _mtpd_device_status_res.wLength = len;
      // Cancel is synchronous, always answer OK
      _mtpd_device_status_res.code = MTP_RESC_OK;
      TU_ASSERT( tud_control_xfer(rhport, request, (uint8_t *)&_mtpd_device_status_res , len) );
    break;

    default:
      TU_LOG_DRV("  MTP request: invalid request\r\n");
      return false; // stall unsupported request
    }
  return true;
}

// Transfer on bulk endpoints
bool mtpd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  const unsigned dir = tu_edpt_dir(ep_addr);

  if (event != XFER_RESULT_SUCCESS)
    return false;

  // IN transfer completed
  if (dir == TUSB_DIR_IN)
  {
    if (_mtpd_itf.phase == MTP_PHASE_RESPONSE)
    {
      // IN transfer completed, prepare for a new command
      TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct)), CFG_MTP_EP_SIZE), 0);
      _mtpd_itf.phase = MTP_PHASE_IDLE;
    }
    else if (_mtpd_itf.phase == MTP_PHASE_DATA_IN)
    {
      _mtpd_itf.xferred_len += xferred_bytes;
      _mtpd_itf.handled_len = _mtpd_itf.xferred_len;

      // Check if transfer completed
      if (_mtpd_itf.xferred_len >= _mtpd_itf.total_len && (xferred_bytes == 0 || (xferred_bytes % CFG_MTP_EP_SIZE) != 0))
      {
        _mtpd_itf.phase = MTP_PHASE_RESPONSE;
        _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
        _mtpd_gct.code = MTP_RESC_OK;
        _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
        _mtpd_gct.transaction_id = _mtpd_ctx.transaction_id;
        if (_mtpd_ctx.session_id != 0)
        {
          _mtpd_gct.data[0] = _mtpd_ctx.session_id;
          _mtpd_gct.container_length += sizeof(uint32_t);
        }
        TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_in, ((uint8_t *)(&_mtpd_gct)), (uint16_t)_mtpd_gct.container_length), 0);
      }
      else
      // Send next block of DATA
      {
        // Send Zero-Lenght Packet
        if (_mtpd_itf.xferred_len == _mtpd_itf.total_len)
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_in, ((uint8_t *)(&_mtpd_gct.data)), 0 ));
        }
        else
        {
          _mtpd_itf.phase = mtpd_handle_data();
          if (_mtpd_itf.phase == MTP_PHASE_RESPONSE)
            TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_in, ((uint8_t *)(&_mtpd_gct)), (uint16_t)_mtpd_gct.container_length));
          else
            TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_in, ((uint8_t *)(&_mtpd_gct.data)), (uint16_t)_mtpd_itf.queued_len));
        }
      }
    }
    else
    {
      return false;
    }
  }

  if (dir == TUSB_DIR_OUT)
  {
    if (_mtpd_itf.phase == MTP_PHASE_IDLE)
    {
      // A new command has been received. Ensure this is the last of the sequence.
      _mtpd_itf.total_len = _mtpd_gct.container_length;
      // Stall in case of unexpected block
      if (_mtpd_gct.container_type != MTP_CONTAINER_TYPE_COMMAND_BLOCK)
      {
        return false;
      }
      _mtpd_itf.phase = MTP_PHASE_COMMAND;
      _mtpd_itf.total_len = _mtpd_gct.container_length;
      _mtpd_itf.xferred_len = xferred_bytes;
      _mtpd_itf.handled_len = 0;
      _mtpd_itf.xfer_completed = false;
      TU_ASSERT(_mtpd_itf.total_len < sizeof(mtp_generic_container_t));
    }

    if (_mtpd_itf.phase == MTP_PHASE_COMMAND)
    {
      // A zero-length or a short packet termination is expected
      if (xferred_bytes == CFG_MTP_EP_SIZE || (_mtpd_itf.total_len - _mtpd_itf.xferred_len) > 0 )
      {
        TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct)) + _mtpd_itf.xferred_len, (uint16_t)(_mtpd_itf.total_len - _mtpd_itf.xferred_len)));
      }
      else
      {
        // Handle command block
        _mtpd_itf.phase = mtpd_handle_cmd();
        if (_mtpd_itf.phase == MTP_PHASE_RESPONSE)
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_in, ((uint8_t *)(&_mtpd_gct)), (uint16_t)_mtpd_gct.container_length));
        }
        else if (_mtpd_itf.phase == MTP_PHASE_DATA_IN)
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_in, ((uint8_t *)(&_mtpd_gct)), (uint16_t)_mtpd_itf.queued_len));
          _mtpd_itf.total_len = _mtpd_gct.container_length;
          _mtpd_itf.xferred_len = 0;
          _mtpd_itf.handled_len = 0;
          _mtpd_itf.xfer_completed = false;
        }
        else if (_mtpd_itf.phase == MTP_PHASE_DATA_OUT)
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct)), CFG_MTP_EP_SIZE), 0);
          _mtpd_itf.xferred_len = 0;
          _mtpd_itf.handled_len = 0;
          _mtpd_itf.xfer_completed = false;
        }
        else
        {
          usbd_edpt_stall(rhport, _mtpd_itf.ep_out);
          usbd_edpt_stall(rhport, _mtpd_itf.ep_in);
        }
      }
      return true;
    }

    if (_mtpd_itf.phase == MTP_PHASE_DATA_OUT)
    {
      // First block of data
      if (_mtpd_itf.xferred_len == 0)
      {
        _mtpd_itf.total_len = _mtpd_gct.container_length;
        _mtpd_itf.handled_len = 0;
        _mtpd_itf.xfer_completed = false;
      }
      _mtpd_itf.xferred_len += xferred_bytes;
      // Stall in case of unexpected block
      if (_mtpd_gct.container_type != MTP_CONTAINER_TYPE_DATA_BLOCK)
      {
        return false;
      }

      // A zero-length or a short packet termination
      if (xferred_bytes < CFG_MTP_EP_SIZE)
      {
        _mtpd_itf.xfer_completed = true;
        // Handle data block
        _mtpd_itf.phase = mtpd_handle_data();
        if (_mtpd_itf.phase == MTP_PHASE_DATA_IN || _mtpd_itf.phase == MTP_PHASE_RESPONSE)
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_in, ((uint8_t *)(&_mtpd_gct)), (uint16_t)_mtpd_gct.container_length));
        }
        else if (_mtpd_itf.phase == MTP_PHASE_DATA_OUT)
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct)), CFG_MTP_EP_SIZE), 0);
          _mtpd_itf.xferred_len = 0;
          _mtpd_itf.xfer_completed = false;
        }
        else
        {
          usbd_edpt_stall(rhport, _mtpd_itf.ep_out);
          usbd_edpt_stall(rhport, _mtpd_itf.ep_in);
        }
      }
      else
      {
        // Handle data block when container is full
        if (_mtpd_itf.xferred_len - _mtpd_itf.handled_len >= MTP_MAX_PACKET_SIZE - CFG_MTP_EP_SIZE)
        {
          _mtpd_itf.phase = mtpd_handle_data();
          _mtpd_itf.handled_len = _mtpd_itf.xferred_len;
        }
        // Transfer completed: wait for zero-lenght packet
        // Some platforms may not respect EP size and xferred_bytes may be more than CFG_MTP_EP_SIZE if
        // the OUT EP is waiting for more data. Ensure we are not waiting for more than CFG_MTP_EP_SIZE.
        if (_mtpd_itf.total_len == _mtpd_itf.xferred_len)
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct.data)), CFG_MTP_EP_SIZE), 0);
        }
        // First data block includes container header + container data
        else if (_mtpd_itf.handled_len == 0)
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct)) + _mtpd_itf.xferred_len, (uint16_t)TU_MIN(_mtpd_itf.total_len - _mtpd_itf.xferred_len, CFG_MTP_EP_SIZE)));
        }
        else
        // Successive data block includes only container data
        {
          TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, ((uint8_t *)(&_mtpd_gct.data)) + _mtpd_itf.xferred_len - _mtpd_itf.handled_len, (uint16_t)TU_MIN(_mtpd_itf.total_len - _mtpd_itf.xferred_len, CFG_MTP_EP_SIZE)));
        }
      }
    }
  }
  return true;
}

//--------------------------------------------------------------------+
// MTPD Internal functionality
//--------------------------------------------------------------------+

// Decode command and prepare response
mtp_phase_type_t mtpd_handle_cmd(void)
{
  TU_ASSERT(_mtpd_gct.container_type == MTP_CONTAINER_TYPE_COMMAND_BLOCK);
  _mtpd_ctx.transaction_id = _mtpd_gct.transaction_id;
  if (_mtpd_gct.code != MTP_OPEC_SEND_OBJECT)
    _mtpd_soi.object_handle = 0;

  switch(_mtpd_gct.code)
  {
    case MTP_OPEC_GET_DEVICE_INFO:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_DEVICE_INFO\n");
      return mtpd_handle_cmd_get_device_info();
    case MTP_OPEC_OPEN_SESSION:
      TU_LOG_DRV("  MTP command: MTP_OPEC_OPEN_SESSION\n");
      return mtpd_handle_cmd_open_session();
    case MTP_OPEC_CLOSE_SESSION:
      TU_LOG_DRV("  MTP command: MTP_OPEC_CLOSE_SESSION\n");
      return mtpd_handle_cmd_close_session();
    case MTP_OPEC_GET_STORAGE_IDS:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_STORAGE_IDS\n");
      return mtpd_handle_cmd_get_storage_ids();
    case MTP_OPEC_GET_STORAGE_INFO:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_STORAGE_INFO for ID=%lu\n", _mtpd_gct.data[0]);
      return mtpd_handle_cmd_get_storage_info();
    case MTP_OPEC_GET_OBJECT_HANDLES:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_OBJECT_HANDLES\n");
      return mtpd_handle_cmd_get_object_handles();
    case MTP_OPEC_GET_OBJECT_INFO:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_OBJECT_INFO\n");
      return mtpd_handle_cmd_get_object_info();
    case MTP_OPEC_GET_OBJECT:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_OBJECT\n");
      return mtpd_handle_cmd_get_object();
    case MTP_OPEC_DELETE_OBJECT:
      TU_LOG_DRV("  MTP command: MTP_OPEC_DELETE_OBJECT\n");
      return mtpd_handle_cmd_delete_object();
    case MTP_OPEC_GET_DEVICE_PROP_DESC:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_DEVICE_PROP_DESC\n");
      return mtpd_handle_cmd_get_device_prop_desc();
    case MTP_OPEC_GET_DEVICE_PROP_VALUE:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_DEVICE_PROP_VALUE\n");
      return mtpd_handle_cmd_get_device_prop_value();
    case MTP_OPEC_SEND_OBJECT_INFO:
      TU_LOG_DRV("  MTP command: MTP_OPEC_SEND_OBJECT_INFO\n");
      return mtpd_handle_cmd_send_object_info();
    case MTP_OPEC_SEND_OBJECT:
      TU_LOG_DRV("  MTP command: MTP_OPEC_SEND_OBJECT\n");
      return mtpd_handle_cmd_send_object();
    case MTP_OPEC_FORMAT_STORE:
      TU_LOG_DRV("  MTP command: MTP_OPEC_FORMAT_STORE\n");
      return mtpd_handle_cmd_format_store();
    default:
      TU_LOG_DRV("  MTP command: MTP_OPEC_UNKNOWN_COMMAND %x!!!!\n", _mtpd_gct.code);
      return false;
  }
  return true;
}

mtp_phase_type_t mtpd_handle_data(void)
{
  TU_ASSERT(_mtpd_gct.container_type == MTP_CONTAINER_TYPE_DATA_BLOCK);
  _mtpd_ctx.transaction_id = _mtpd_gct.transaction_id;

  switch(_mtpd_gct.code)
  {
    case MTP_OPEC_GET_OBJECT:
      TU_LOG_DRV("  MTP command: MTP_OPEC_GET_OBJECT-DATA_IN\n");
      return mtpd_handle_dti_get_object();
    case MTP_OPEC_SEND_OBJECT_INFO:
      TU_LOG_DRV("  MTP command: MTP_OPEC_SEND_OBJECT_INFO-DATA_OUT\n");
      return mtpd_handle_dto_send_object_info();
    case MTP_OPEC_SEND_OBJECT:
      TU_LOG_DRV("  MTP command: MTP_OPEC_SEND_OBJECT-DATA_OUT\n");
      return mtpd_handle_dto_send_object();
    default:
      TU_LOG_DRV("  MTP command: MTP_OPEC_UNKNOWN_COMMAND %x!!!!\n", _mtpd_gct.code);
      return false;
  }
  return true;
}

mtp_phase_type_t mtpd_handle_cmd_get_device_info(void)
{
  TU_VERIFY_STATIC(sizeof(mtp_device_info_t) < MTP_MAX_PACKET_SIZE, "mtp_device_info_t shall fit in MTP_MAX_PACKET_SIZE");

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH + sizeof(mtp_device_info_t);
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_DATA_BLOCK;
  _mtpd_gct.code = MTP_OPEC_GET_DEVICE_INFO;
  mtp_device_info_t *d = (mtp_device_info_t *)_mtpd_gct.data;
  d->standard_version = 100;
  d->mtp_vendor_extension_id = 0x06;
  d->mtp_version = 100;
  d->mtp_extensions_len = TU_ARRAY_LEN(MTP_EXTENSIONS);
  mtpd_wc16cpy((uint8_t *)d->mtp_extensions, MTP_EXTENSIONS);
  d->functional_mode = 0x0000;
  d->operations_supported_len = TU_ARRAY_LEN(mtp_operations_supported);
  memcpy(d->operations_supported, mtp_operations_supported, sizeof(mtp_operations_supported));
  d->events_supported_len = TU_ARRAY_LEN(mtp_events_supported);
  memcpy(d->events_supported, mtp_events_supported, sizeof(mtp_events_supported));
  d->device_properties_supported_len = TU_ARRAY_LEN(mtp_device_properties_supported);
  memcpy(d->device_properties_supported, mtp_device_properties_supported, sizeof(mtp_device_properties_supported));
  d->capture_formats_len = TU_ARRAY_LEN(mtp_capture_formats);
  memcpy(d->capture_formats, mtp_capture_formats, sizeof(mtp_capture_formats));
  d->playback_formats_len = TU_ARRAY_LEN(mtp_playback_formats);
  memcpy(d->playback_formats, mtp_playback_formats, sizeof(mtp_playback_formats));
  mtpd_gct_append_wstring(CFG_TUD_MANUFACTURER);
  mtpd_gct_append_wstring(CFG_TUD_MODEL);
  mtpd_gct_append_wstring(CFG_MTP_DEVICE_VERSION);
  mtpd_gct_append_wstring(CFG_MTP_SERIAL_NUMBER);

  _mtpd_itf.queued_len = _mtpd_gct.container_length;
  return MTP_PHASE_DATA_IN;
}

mtp_phase_type_t mtpd_handle_cmd_open_session(void)
{
  uint32_t session_id = _mtpd_gct.data[0];

  mtp_response_t res = tud_mtp_storage_open_session(&session_id);
  if (res == MTP_RESC_SESSION_ALREADY_OPEN)
  {
    _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
    _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
    _mtpd_gct.code = res;
    _mtpd_gct.container_length += sizeof(_mtpd_gct.data[0]);
    _mtpd_gct.data[0] = session_id;
    _mtpd_ctx.session_id = session_id;
    return MTP_PHASE_RESPONSE;
  }

  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;

  _mtpd_ctx.session_id = session_id;

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  _mtpd_gct.code = MTP_RESC_OK;

  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_close_session(void)
{
  uint32_t session_id = _mtpd_gct.data[0];

  mtp_response_t res = tud_mtp_storage_close_session(session_id);

  _mtpd_ctx.session_id = session_id;

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  _mtpd_gct.code = res;

  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_get_storage_ids(void)
{
  TU_VERIFY_STATIC(sizeof(mtp_storage_ids_t) < MTP_MAX_PACKET_SIZE, "mtp_storage_ids_t shall fit in MTP_MAX_PACKET_SIZE");

  uint32_t storage_id;
  mtp_response_t res = tud_mtp_get_storage_id(&storage_id);
  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH + sizeof(mtp_storage_ids_t);
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_DATA_BLOCK;
  _mtpd_gct.code = MTP_OPEC_GET_STORAGE_IDS;
  mtp_storage_ids_t *d = (mtp_storage_ids_t *)_mtpd_gct.data;
  if (storage_id == 0)
  {
    // Storage not accessible
    d->storage_ids_len = 0;
    d->storage_ids[0] = 0;
  }
  else
  {
    d->storage_ids_len = 1;
    d->storage_ids[0] = storage_id;
  }

  _mtpd_itf.queued_len = _mtpd_gct.container_length;
  return MTP_PHASE_DATA_IN;
}

mtp_phase_type_t mtpd_handle_cmd_get_storage_info(void)
{
  TU_VERIFY_STATIC(sizeof(mtp_storage_info_t) < MTP_MAX_PACKET_SIZE, "mtp_storage_info_t shall fit in MTP_MAX_PACKET_SIZE");

  uint32_t storage_id = _mtpd_gct.data[0];

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH + sizeof(mtp_storage_info_t);
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_DATA_BLOCK;
  _mtpd_gct.code = MTP_OPEC_GET_STORAGE_INFO;

  mtp_response_t res = tud_mtp_get_storage_info(storage_id, (mtp_storage_info_t *)_mtpd_gct.data);
  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;

  _mtpd_itf.queued_len = _mtpd_gct.container_length;
  return MTP_PHASE_DATA_IN;
}

mtp_phase_type_t mtpd_handle_cmd_get_object_handles(void)
{
  uint32_t storage_id = _mtpd_gct.data[0];
  uint32_t object_format_code = _mtpd_gct.data[1]; // optional, not managed
  uint32_t parent_object_handle = _mtpd_gct.data[2]; // folder specification, 0xffffffff=objects with no parent

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH + sizeof(uint32_t);
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_DATA_BLOCK;
  _mtpd_gct.code = MTP_OPEC_GET_OBJECT_HANDLES;
  _mtpd_gct.data[0] = 0;

  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (object_format_code != 0), MTP_RESC_SPECIFICATION_BY_FORMAT_UNSUPPORTED, "specification by format unsupported")) != MTP_PHASE_NONE) return phase;
  //list of all object handles on all storages, not managed
  if ((phase = mtpd_chk_generic(__func__, (storage_id == 0xFFFFFFFF), MTP_RESC_OPERATION_NOT_SUPPORTED, "list of all object handles on all storages unsupported")) != MTP_PHASE_NONE) return phase;

  tud_mtp_storage_object_done();
  uint32_t next_child_handle = 0;
  while(true)
  {
    mtp_response_t res = tud_mtp_storage_association_get_object_handle(storage_id, parent_object_handle, &next_child_handle);
    if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;
    if (next_child_handle == 0)
      break;
    mtpd_gct_append_object_handle(next_child_handle);
  }
  tud_mtp_storage_object_done();

  _mtpd_itf.queued_len = _mtpd_gct.container_length;
  return MTP_PHASE_DATA_IN;
}

mtp_phase_type_t mtpd_handle_cmd_get_object_info(void)
{
  TU_VERIFY_STATIC(sizeof(mtp_object_info_t) < MTP_MAX_PACKET_SIZE, "mtp_object_info_t shall fit in MTP_MAX_PACKET_SIZE");

  uint32_t object_handle = _mtpd_gct.data[0];

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH + sizeof(mtp_object_info_t);
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_DATA_BLOCK;
  _mtpd_gct.code = MTP_OPEC_GET_OBJECT_INFO;
  mtp_response_t res = tud_mtp_storage_object_read_info(object_handle, (mtp_object_info_t *)_mtpd_gct.data);
  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;

  _mtpd_itf.queued_len = _mtpd_gct.container_length;
  return MTP_PHASE_DATA_IN;
}

mtp_phase_type_t mtpd_handle_cmd_get_object(void)
{
  _mtpd_get_object_handle = _mtpd_gct.data[0];

  // Continue with DATA-IN
  return mtpd_handle_dti_get_object();
}

mtp_phase_type_t mtpd_handle_dti_get_object(void)
{
  mtp_response_t res;
  mtp_phase_type_t phase;
  uint32_t file_size = 0;
  res = tud_mtp_storage_object_size(_mtpd_get_object_handle, &file_size);
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;
  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH + file_size;
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_DATA_BLOCK;
  _mtpd_gct.code = MTP_OPEC_GET_OBJECT;

  uint32_t buffer_size;
  uint32_t read_count;
  // Data block must be multiple of EP size
  if (_mtpd_itf.handled_len == 0)
  {
    // First data block: include container header
    buffer_size = ((MTP_MAX_PACKET_SIZE + MTP_GENERIC_DATA_BLOCK_LENGTH) / CFG_MTP_EP_SIZE) * CFG_MTP_EP_SIZE - MTP_GENERIC_DATA_BLOCK_LENGTH;
    res = tud_mtp_storage_object_read(_mtpd_get_object_handle, (void *)&_mtpd_gct.data, buffer_size, &read_count);
    if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;
    _mtpd_itf.queued_len = MTP_GENERIC_DATA_BLOCK_LENGTH + read_count;
  }
  else
  {
    // Successive data block: consider only container data
    buffer_size = (MTP_MAX_PACKET_SIZE / CFG_MTP_EP_SIZE) * CFG_MTP_EP_SIZE;
    res = tud_mtp_storage_object_read(_mtpd_get_object_handle, (void *)&_mtpd_gct.data, buffer_size, &read_count);
    if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;
    _mtpd_itf.queued_len = read_count;
  }

  // File completed
  if (read_count < buffer_size)
  {
    tud_mtp_storage_object_done();
  }

  return MTP_PHASE_DATA_IN;
}

mtp_phase_type_t mtpd_handle_cmd_delete_object(void)
{
  uint32_t object_handle = _mtpd_gct.data[0];
  uint32_t object_code_format = _mtpd_gct.data[1]; // not used
  (void) object_code_format;

  mtp_response_t res = tud_mtp_storage_object_delete(object_handle);
  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;

  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  _mtpd_gct.code = MTP_RESC_OK;
  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_get_device_prop_desc(void)
{
  uint32_t device_prop_code = _mtpd_gct.data[0];

  mtp_phase_type_t rt;
  if ((rt = mtpd_chk_session_open(__func__)) != MTP_PHASE_NONE) return rt;

  switch(device_prop_code)
  {
    case MTP_DEVP_DEVICE_FRIENDLY_NAME:
    {
      TU_VERIFY_STATIC(sizeof(mtp_device_prop_desc_t) < MTP_MAX_PACKET_SIZE, "mtp_device_info_t shall fit in MTP_MAX_PACKET_SIZE");
      _mtpd_gct.container_type = MTP_CONTAINER_TYPE_DATA_BLOCK;
      _mtpd_gct.code = MTP_OPEC_GET_DEVICE_PROP_DESC;
      _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH + sizeof(mtp_device_prop_desc_t);
      mtp_device_prop_desc_t *d = (mtp_device_prop_desc_t *)_mtpd_gct.data;
      d->device_property_code = (uint16_t)(device_prop_code);
      d->datatype = MTP_TYPE_STR;
      d->get_set = MTP_MODE_GET;
      mtpd_gct_append_wstring(CFG_TUD_MODEL); // factory_def_value
      mtpd_gct_append_wstring(CFG_TUD_MODEL); // current_value_len
      mtpd_gct_append_uint8(0x00); // form_flag
      _mtpd_itf.queued_len = _mtpd_gct.container_length;
      return MTP_PHASE_DATA_IN;
    }
    default:
      break;
  }

  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  _mtpd_gct.code = MTP_RESC_PARAMETER_NOT_SUPPORTED;
  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_get_device_prop_value(void)
{
  uint32_t device_prop_code = _mtpd_gct.data[0];

  mtp_phase_type_t rt;
  if ((rt = mtpd_chk_session_open(__func__)) != MTP_PHASE_NONE) return rt;

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_DATA_BLOCK;
  _mtpd_gct.code = MTP_OPEC_GET_DEVICE_PROP_VALUE;

  switch(device_prop_code)
  {
    // TODO support more device properties
    case MTP_DEVP_DEVICE_FRIENDLY_NAME:
      mtpd_gct_append_wstring(CFG_TUD_MODEL);
      _mtpd_itf.queued_len = _mtpd_gct.container_length;
      return MTP_PHASE_DATA_IN;
    default:
      _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
      _mtpd_gct.code = MTP_RESC_PARAMETER_NOT_SUPPORTED;
      return MTP_PHASE_RESPONSE;
  }
}

mtp_phase_type_t mtpd_handle_cmd_send_object_info(void)
{
  _mtpd_soi.storage_id = _mtpd_gct.data[0];
  _mtpd_soi.parent_object_handle = (_mtpd_gct.data[1] == 0xFFFFFFFF ? 0 : _mtpd_gct.data[1]);

  // Enter OUT phase and wait for DATA BLOCK
  return MTP_PHASE_DATA_OUT;
}

mtp_phase_type_t mtpd_handle_dto_send_object_info(void)
{
  uint32_t new_object_handle = 0;
  mtp_response_t res = tud_mtp_storage_object_write_info(_mtpd_soi.storage_id, _mtpd_soi.parent_object_handle, &new_object_handle, (mtp_object_info_t *)_mtpd_gct.data);
  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;

  // Save send_object_info
  _mtpd_soi.object_handle = new_object_handle;

  // Response
  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH +  3 * sizeof(uint32_t);
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  _mtpd_gct.code = MTP_RESC_OK;
  _mtpd_gct.data[0] = _mtpd_soi.storage_id;
  _mtpd_gct.data[1] = _mtpd_soi.parent_object_handle;
  _mtpd_gct.data[2] = _mtpd_soi.object_handle;
  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_send_object(void)
{
  // Enter OUT phase and wait for DATA BLOCK
  return MTP_PHASE_DATA_OUT;
}

mtp_phase_type_t mtpd_handle_dto_send_object(void)
{
  uint8_t *buffer = (uint8_t *)&_mtpd_gct.data;
  uint32_t buffer_size = _mtpd_itf.xferred_len - _mtpd_itf.handled_len;
  // First block of DATA
  if (_mtpd_itf.handled_len == 0)
  {
    buffer_size -= MTP_GENERIC_DATA_BLOCK_LENGTH;
  }

  if (buffer_size > 0)
  {
    mtp_response_t res = tud_mtp_storage_object_write(_mtpd_soi.object_handle, buffer, buffer_size);
    mtp_phase_type_t phase;
    if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESC_OK), res, "")) != MTP_PHASE_NONE) return phase;
  }

  if (!_mtpd_itf.xfer_completed)
  {
    // Continue with next DATA BLOCK
    return MTP_PHASE_DATA_OUT;
  }

  // Send completed
  tud_mtp_storage_object_done();

  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  _mtpd_gct.code = MTP_RESC_OK;
  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_format_store(void)
{
  uint32_t storage_id = _mtpd_gct.data[0];
  uint32_t file_system_format = _mtpd_gct.data[1]; // not used
  (void) file_system_format;

  mtp_response_t res = tud_mtp_storage_format(storage_id);

  _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  _mtpd_gct.code = res;
  _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
  return MTP_PHASE_RESPONSE;
}

//--------------------------------------------------------------------+
// Checker
//--------------------------------------------------------------------+
mtp_phase_type_t mtpd_chk_session_open(const char *func_name)
{
  (void)func_name;
  if (_mtpd_ctx.session_id == 0)
  {
    TU_LOG_DRV("  MTP error: %s session not open\n", func_name);
    _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
    _mtpd_gct.code = MTP_RESC_SESSION_NOT_OPEN;
    _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
    return MTP_PHASE_RESPONSE;
  }
  return MTP_PHASE_NONE;
}

mtp_phase_type_t mtpd_chk_generic(const char *func_name, const bool err_cd, const uint16_t ret_code, const char *message)
{
  (void)func_name;
  (void)message;
  if (err_cd)
  {
    TU_LOG_DRV("  MTP error in %s: (%x) %s\n", func_name, ret_code, message);
    _mtpd_gct.container_type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
    _mtpd_gct.code = ret_code;
    _mtpd_gct.container_length = MTP_GENERIC_DATA_BLOCK_LENGTH;
     return MTP_PHASE_RESPONSE;
  }
  return MTP_PHASE_NONE;
}

//--------------------------------------------------------------------+
// Generic container data
//--------------------------------------------------------------------+
void mtpd_wc16cpy(uint8_t *dest, const char *src)
{
  wchar16_t s;
  while(true)
  {
    s = *src;
    memcpy(dest, &s, sizeof(wchar16_t));
    if (*src == 0) break;
    ++src;
    dest += sizeof(wchar16_t);
  }
}

//--------------------------------------------------------------------+
// Generic container function
//--------------------------------------------------------------------+
bool mtpd_gct_append_uint8(const uint8_t value)
{
  uint8_t *p_value = ((uint8_t *)&_mtpd_gct) + _mtpd_gct.container_length;
  _mtpd_gct.container_length += sizeof(uint8_t);
  // Verify space requirement (8 bit string length, number of wide characters including terminator)
  TU_ASSERT(_mtpd_gct.container_length < sizeof(mtp_generic_container_t));
  *p_value = value;
  return true;
}

bool mtpd_gct_append_object_handle(const uint32_t object_handle)
{
  _mtpd_gct.container_length += sizeof(uint32_t);
  TU_ASSERT(_mtpd_gct.container_length < sizeof(mtp_generic_container_t));
  _mtpd_gct.data[0]++;
  _mtpd_gct.data[_mtpd_gct.data[0]] = object_handle;
  return true;
}

bool mtpd_gct_append_wstring(const char *s)
{
  size_t len = strlen(s) + 1;
  TU_ASSERT(len <= UINT8_MAX);
  uint8_t *p_len = ((uint8_t *)&_mtpd_gct)+_mtpd_gct.container_length;
  _mtpd_gct.container_length += sizeof(uint8_t) + sizeof(wchar16_t) * len;
  // Verify space requirement (8 bit string length, number of wide characters including terminator)
  TU_ASSERT(_mtpd_gct.container_length < sizeof(mtp_generic_container_t));
  *p_len = (uint8_t)len;
  uint8_t *p_str = p_len + sizeof(uint8_t);
  mtpd_wc16cpy(p_str, s);
  return true;
}

bool mtpd_gct_get_string(uint16_t *offset_data, char *string, const uint16_t max_size)
{
  uint16_t size = *(((uint8_t *)&_mtpd_gct.data) + *offset_data);
  if (size > max_size)
      size = max_size;
  TU_ASSERT(*offset_data + size < sizeof(_mtpd_gct.data));

  uint8_t *s = ((uint8_t *)&_mtpd_gct.data) + *offset_data + sizeof(uint8_t);
  for(uint16_t i = 0; i < size; i++)
  {
    string[i] = *s;
    s += sizeof(wchar16_t);
  }
  *offset_data += (uint16_t)(sizeof(uint8_t) + size * sizeof(wchar16_t));
  return true;
}

bool mtpd_gct_append_array(uint32_t array_size, const void *data, size_t type_size)
{
  TU_ASSERT(_mtpd_gct.container_length + sizeof(uint32_t) + array_size * type_size < sizeof(_mtpd_gct.data));
  uint8_t *p = ((uint8_t *)&_mtpd_gct) + _mtpd_gct.container_length;
  memcpy(p, &array_size, sizeof(uint32_t));
  p += sizeof(uint32_t);
  memcpy(p, data, array_size * type_size);
  _mtpd_gct.container_length += sizeof(uint32_t) + array_size * type_size;
  return true;
}

bool mtpd_gct_append_date(struct tm *timeinfo)
{
  // strftime is not supported by all platform, this implementation is just for reference
  int len = snprintf(_mtp_datestr, sizeof(_mtpd_gct.data) - _mtpd_gct.container_length, "%04d%02d%02dT%02d%02d%02dZ",
    timeinfo->tm_year + 1900,
    timeinfo->tm_mon + 1,
    timeinfo->tm_mday,
    timeinfo->tm_hour,
    timeinfo->tm_min,
    timeinfo->tm_sec);
  if (len == 0)
    return false;
  return mtpd_gct_append_wstring(_mtp_datestr);
}

#endif // (CFG_TUD_ENABLED && CFG_TUD_MTP)
