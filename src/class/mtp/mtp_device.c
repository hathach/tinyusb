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

#define BULK_PACKET_SIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)

//--------------------------------------------------------------------+
// STRUCT
//--------------------------------------------------------------------+
typedef struct {
  uint8_t rhport;
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;
  uint8_t ep_event;

  // Bulk Only Transfer (BOT) Protocol
  uint8_t  phase;

  uint32_t total_len;
  uint32_t xferred_len;

  uint32_t session_id;
  mtp_container_command_t command;
  mtp_container_header_t reply_header;
} mtpd_interface_t;

typedef struct {
  TUD_EPBUF_TYPE_DEF(mtp_generic_container_t, container);
} mtpd_epbuf_t;

//--------------------------------------------------------------------+
// INTERNAL FUNCTION DECLARATION
//--------------------------------------------------------------------+
// Checker
static mtp_phase_type_t mtpd_chk_generic(const char *func_name, const bool err_cd, const uint16_t ret_code, const char *message);

// MTP commands
static mtp_phase_type_t mtpd_handle_cmd(mtpd_interface_t* p_mtp, tud_mtp_cb_data_t* cb_data);
static mtp_phase_type_t mtpd_handle_data(void);
static mtp_phase_type_t mtpd_handle_cmd_delete_object(void);
static mtp_phase_type_t mtpd_handle_cmd_send_object_info(void);
static mtp_phase_type_t mtpd_handle_dto_send_object_info(void);
static mtp_phase_type_t mtpd_handle_cmd_send_object(void);
static mtp_phase_type_t mtpd_handle_dto_send_object(void);
static mtp_phase_type_t mtpd_handle_cmd_format_store(void);

//--------------------------------------------------------------------+
// MTP variable declaration
//--------------------------------------------------------------------+
static mtpd_interface_t _mtpd_itf;
CFG_TUD_MEM_SECTION static mtpd_epbuf_t _mtpd_epbuf;

CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN static mtp_device_status_res_t _mtpd_device_status_res;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint32_t _mtpd_get_object_handle;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN static mtp_basic_object_info_t _mtpd_soi;
CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN static char _mtp_datestr[20];


//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= CFG_TUD_MTP_LOG_LEVEL

TU_ATTR_UNUSED static tu_lookup_entry_t const _mpt_op_lookup[] = {
{.key = MTP_OP_UNDEFINED                    , .data = "Undefined"                 } ,
{.key = MTP_OP_GET_DEVICE_INFO              , .data = "GetDeviceInfo"             } ,
{.key = MTP_OP_OPEN_SESSION                 , .data = "OpenSession"               } ,
{.key = MTP_OP_CLOSE_SESSION                , .data = "CloseSession"              } ,
{.key = MTP_OP_GET_STORAGE_IDS              , .data = "GetStorageIDs"             } ,
{.key = MTP_OP_GET_STORAGE_INFO             , .data = "GetStorageInfo"            } ,
{.key = MTP_OP_GET_NUM_OBJECTS              , .data = "GetNumObjects"             } ,
{.key = MTP_OP_GET_OBJECT_HANDLES           , .data = "GetObjectHandles"          } ,
{.key = MTP_OP_GET_OBJECT_INFO              , .data = "GetObjectInfo"             } ,
{.key = MTP_OP_GET_OBJECT                   , .data = "GetObject"                 } ,
{.key = MTP_OP_GET_THUMB                    , .data = "GetThumb"                  } ,
{.key = MTP_OP_DELETE_OBJECT                , .data = "DeleteObject"              } ,
{.key = MTP_OP_SEND_OBJECT_INFO             , .data = "SendObjectInfo"            } ,
{.key = MTP_OP_SEND_OBJECT                  , .data = "SendObject"                } ,
{.key = MTP_OP_INITIATE_CAPTURE             , .data = "InitiateCapture"           } ,
{.key = MTP_OP_FORMAT_STORE                 , .data = "FormatStore"               } ,
{.key = MTP_OP_RESET_DEVICE                 , .data = "ResetDevice"               } ,
{.key = MTP_OP_SELF_TEST                    , .data = "SelfTest"                  } ,
{.key = MTP_OP_SET_OBJECT_PROTECTION        , .data = "SetObjectProtection"       } ,
{.key = MTP_OP_POWER_DOWN                   , .data = "PowerDown"                 } ,
{.key = MTP_OP_GET_DEVICE_PROP_DESC         , .data = "GetDevicePropDesc"         } ,
{.key = MTP_OP_GET_DEVICE_PROP_VALUE        , .data = "GetDevicePropValue"        } ,
{.key = MTP_OP_SET_DEVICE_PROP_VALUE        , .data = "SetDevicePropValue"        } ,
{.key = MTP_OP_RESET_DEVICE_PROP_VALUE      , .data = "ResetDevicePropValue"      } ,
{.key = MTP_OP_TERMINATE_OPEN_CAPTURE       , .data = "TerminateOpenCapture"      } ,
{.key = MTP_OP_MOVE_OBJECT                  , .data = "MoveObject"                } ,
{.key = MTP_OP_COPY_OBJECT                  , .data = "CopyObject"                } ,
{.key = MTP_OP_GET_PARTIAL_OBJECT           , .data = "GetPartialObject"          } ,
{.key = MTP_OP_INITIATE_OPEN_CAPTURE        , .data = "InitiateOpenCapture"       } ,
{.key = MTP_OP_GET_OBJECT_PROPS_SUPPORTED   , .data = "GetObjectPropsSupported"   } ,
{.key = MTP_OP_GET_OBJECT_PROP_DESC         , .data = "GetObjectPropDesc"         } ,
{.key = MTP_OP_GET_OBJECT_PROP_VALUE        , .data = "GetObjectPropValue"        } ,
{.key = MTP_OP_SET_OBJECT_PROP_VALUE        , .data = "SetObjectPropValue"        } ,
{.key = MTP_OP_GET_OBJECT_PROPLIST          , .data = "GetObjectPropList"         } ,
{.key = MTP_OP_GET_OBJECT_PROP_REFERENCES   , .data = "GetObjectPropReferences"   } ,
{.key = MTP_OP_GET_SERVICE_IDS              , .data = "GetServiceIDs"             } ,
{.key = MTP_OP_GET_SERVICE_INFO             , .data = "GetServiceInfo"            } ,
{.key = MTP_OP_GET_SERVICE_CAPABILITIES     , .data = "GetServiceCapabilities"    } ,
{.key = MTP_OP_GET_SERVICE_PROP_DESC        , .data = "GetServicePropDesc"        } ,
{.key = MTP_OP_GET_OBJECT_PROP_LIST         , .data = "GetObjectPropList"         } ,
{.key = MTP_OP_SET_OBJECT_PROP_LIST         , .data = "SetObjectPropList"         } ,
{.key = MTP_OP_GET_INTERDEPENDENT_PROP_DESC , .data = "GetInterdependentPropDesc" } ,
{.key = MTP_OP_SEND_OBJECT_PROP_LIST        , .data = "SendObjectPropList"        }
};

TU_ATTR_UNUSED static tu_lookup_table_t const _mtp_op_table = {
  .count = TU_ARRAY_SIZE(_mpt_op_lookup),
  .items = _mpt_op_lookup
};

#endif


//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+

static bool prepare_new_command(mtpd_interface_t* p_mtp) {
  p_mtp->phase = MTP_PHASE_IDLE;
  return usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_out, (uint8_t *)(&_mtpd_epbuf.container), sizeof(mtp_generic_container_t));
}


//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void mtpd_init(void) {
  tu_memclr(&_mtpd_itf, sizeof(mtpd_interface_t));
  tu_memclr(&_mtpd_soi, sizeof(mtp_basic_object_info_t));
  _mtpd_get_object_handle = 0;
}

bool mtpd_deinit(void) {
  return true; // nothing to do
}

void mtpd_reset(uint8_t rhport) {
  (void) rhport;
  tu_memclr(&_mtpd_itf, sizeof(mtpd_interface_t));
  tu_memclr(&_mtpd_epbuf, sizeof(mtpd_epbuf_t));
  tu_memclr(&_mtpd_soi, sizeof(mtp_basic_object_info_t));
  _mtpd_get_object_handle = 0;
}

uint16_t mtpd_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
  // only support PIMA 15470 protocol
  TU_VERIFY(TUSB_CLASS_IMAGE == itf_desc->bInterfaceClass &&
            MTP_SUBCLASS_STILL_IMAGE == itf_desc->bInterfaceSubClass &&
            MTP_PROTOCOL_PIMA_15470 == itf_desc->bInterfaceProtocol, 0);

  // mtp driver length is fixed
  const uint16_t mtpd_itf_size = sizeof(tusb_desc_interface_t) + 3 * sizeof(tusb_desc_endpoint_t);

  // Max length must be at least 1 interface + 3 endpoints
  TU_ASSERT(itf_desc->bNumEndpoints == 3 && max_len >= mtpd_itf_size);
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  tu_memclr(p_mtp, sizeof(mtpd_interface_t));
  p_mtp->rhport = rhport;
  p_mtp->itf_num = itf_desc->bInterfaceNumber;

  // Open interrupt IN endpoint
  const tusb_desc_endpoint_t* ep_desc = (const tusb_desc_endpoint_t*) tu_desc_next(itf_desc);
  TU_ASSERT(ep_desc->bDescriptorType == TUSB_DESC_ENDPOINT && ep_desc->bmAttributes.xfer == TUSB_XFER_INTERRUPT, 0);
  TU_ASSERT(usbd_edpt_open(rhport, ep_desc), 0);
  p_mtp->ep_event = ep_desc->bEndpointAddress;

  // Open endpoint pair
  TU_ASSERT(usbd_open_edpt_pair(rhport, tu_desc_next(ep_desc), 2, TUSB_XFER_BULK, &p_mtp->ep_out, &p_mtp->ep_in), 0);

  TU_ASSERT(prepare_new_command(p_mtp), 0);

  return mtpd_itf_size;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool mtpd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
  if (stage != CONTROL_STAGE_SETUP) {
    return true; // nothing to do with DATA & ACK stage
  }

  switch (request->bRequest) {
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
      TU_ASSERT(usbd_edpt_xfer(rhport, _mtpd_itf.ep_out, (uint8_t *)(&_mtpd_epbuf.container), sizeof(mtp_generic_container_t)));
      break;

    case MTP_REQ_GET_DEVICE_STATUS: {
      TU_LOG_DRV("  MTP request: MTP_REQ_GET_DEVICE_STATUS\n");
      uint16_t len = 4;
      _mtpd_device_status_res.wLength = len;
      // Cancel is synchronous, always answer OK
      _mtpd_device_status_res.code = MTP_RESP_OK;
      TU_ASSERT(tud_control_xfer(rhport, request, (uint8_t *)&_mtpd_device_status_res , len));
      break;
    }

    default:
      TU_LOG_DRV("  MTP request: invalid request\r\n");
      return false; // stall unsupported request
  }

  return true;
}

bool tud_mtp_data_send(mtp_container_info_t* p_container) {
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  if (p_mtp->phase == MTP_PHASE_COMMAND) {
    // 1st data block: header + payload
    p_mtp->phase = MTP_PHASE_DATA;
    p_mtp->total_len = p_container->header->len;
    p_mtp->xferred_len = 0;

    p_container->header->type = MTP_CONTAINER_TYPE_DATA_BLOCK;
    p_container->header->transaction_id = p_mtp->command.header.transaction_id;
    p_mtp->reply_header = *p_container->header; // save header for subsequent data
  } else {
    // subsequent data block: payload only
    TU_ASSERT(p_mtp->phase == MTP_PHASE_DATA);
  }

  const uint16_t xact_len = tu_min32(p_mtp->total_len - p_mtp->xferred_len, CFG_TUD_MTP_EP_BUFSIZE);
  TU_ASSERT(usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_in, (uint8_t *)(&_mtpd_epbuf.container), xact_len));

  return true;
}

bool tud_mtp_response_send(mtp_container_info_t* p_container) {
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  p_mtp->phase = MTP_PHASE_RESPONSE_QUEUED;
  p_container->header->type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  p_container->header->transaction_id = p_mtp->command.header.transaction_id;
  TU_ASSERT(usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_in, (uint8_t *)(&_mtpd_epbuf.container), (uint16_t)p_container->header->len));
  return true;
}

// Transfer on bulk endpoints
bool mtpd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes) {
  TU_ASSERT(event == XFER_RESULT_SUCCESS);

  if (ep_addr == _mtpd_itf.ep_event) {
    // nothing to do
    return true;
  }

  mtpd_interface_t* p_mtp = &_mtpd_itf;
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;

  tud_mtp_cb_data_t cb_data;
  cb_data.idx = 0;
  cb_data.command = &p_mtp->command;
  cb_data.reply.header = (mtp_container_header_t*) p_container;
  cb_data.reply.payload32 = p_container->data;
  cb_data.reply.payload_size = CFG_TUD_MTP_EP_BUFSIZE - sizeof(mtp_container_header_t);
  cb_data.xferred_bytes = 0;
  cb_data.xfer_result = event;

  switch (p_mtp->phase) {
    case MTP_PHASE_IDLE:
      // received new command
      TU_VERIFY(ep_addr == p_mtp->ep_out && p_container->type == MTP_CONTAINER_TYPE_COMMAND_BLOCK);
      p_mtp->phase = MTP_PHASE_COMMAND;
      TU_ATTR_FALLTHROUGH; // handle in the next case

    case MTP_PHASE_COMMAND: {
      memcpy(&p_mtp->command, p_container, sizeof(mtp_container_command_t)); // copy new command
      mtpd_handle_cmd(p_mtp, &cb_data);
      break;
    }

    case MTP_PHASE_DATA: {
      const uint16_t bulk_mps = (tud_speed_get() == TUSB_SPEED_HIGH) ? 512 : 64;
      p_mtp->xferred_len += xferred_bytes;
      cb_data.xferred_bytes = p_mtp->xferred_len;

      // transfer complete if ZLP or short packet or overflow
      if (xferred_bytes == 0 || // ZLP
          (xferred_bytes & (bulk_mps - 1)) || // short packet
          p_mtp->xferred_len > p_mtp->total_len) {
        tud_mtp_data_complete_cb(&cb_data);
      } else {
        // payload only packet
        cb_data.reply.header = &p_mtp->reply_header;
        cb_data.reply.payload = (uint8_t*) p_container;
        cb_data.reply.payload_size = CFG_TUD_MTP_EP_BUFSIZE;
        tud_mtp_data_more_cb(&cb_data);
      }
      break;
    }

#if 0
    case MTP_PHASE_DATA_IN:
      p_mtp->xferred_len += xferred_bytes;
      p_mtp->handled_len = p_mtp->xferred_len;

      // Check if transfer completed TODO check ZLP with FS/HS bulk size
      if (p_mtp->xferred_len >= p_mtp->total_len && (xferred_bytes == 0 || (xferred_bytes % CFG_MTP_EP_SIZE) != 0)) {
        p_mtp->phase = MTP_PHASE_RESPONSE;
        p_container->code = MTP_RESP_OK;
        p_container->len = MTP_CONTAINER_HEADER_LENGTH;
        if (p_mtp->session_id != 0) { // is this needed ?
          p_container->data[0] = p_mtp->session_id;
          p_container->len += sizeof(uint32_t);
        }
      } else {
        // Send next block of DATA
        if (p_mtp->xferred_len == p_mtp->total_len) {
          // send Zero-Length Packet
          TU_ASSERT(usbd_edpt_xfer(rhport, p_mtp->ep_in, NULL, 0 ));
        } else {
          p_mtp->phase = mtpd_handle_data();
          if (p_mtp->phase == MTP_PHASE_DATA_IN) {
            TU_ASSERT(usbd_edpt_xfer(rhport, p_mtp->ep_in, ((uint8_t *)(&p_container->data)), (uint16_t)p_mtp->queued_len));
          }
        }
      }
      break;

    case MTP_PHASE_DATA_OUT:
      // First block of data
      if (p_mtp->xferred_len == 0) {
        p_mtp->total_len = p_container->len;
        p_mtp->handled_len = 0;
        p_mtp->xfer_completed = false;
        TU_ASSERT(p_container->type == MTP_CONTAINER_TYPE_DATA_BLOCK);
      }
      p_mtp->xferred_len += xferred_bytes;

      // A zero-length or a short packet termination
      if (xferred_bytes < CFG_MTP_EP_SIZE) {
        p_mtp->xfer_completed = true;
        // Handle data block
        p_mtp->phase = mtpd_handle_data();
        if (p_mtp->phase == MTP_PHASE_DATA_OUT) {
          TU_ASSERT(usbd_edpt_xfer(rhport, p_mtp->ep_out, (uint8_t*) p_container, sizeof(mtp_generic_container_t)), 0);
          p_mtp->xferred_len = 0;
          p_mtp->xfer_completed = false;
        }
      } else {
        // Handle data block when container is full
        if (p_mtp->xferred_len - p_mtp->handled_len >= MTP_MAX_PACKET_SIZE - CFG_MTP_EP_SIZE) {
          p_mtp->phase = mtpd_handle_data();
          p_mtp->handled_len = p_mtp->xferred_len;
        }
        // Transfer completed: wait for zero-length packet
        // Some platforms may not respect EP size and xferred_bytes may be more than CFG_MTP_EP_SIZE if
        // the OUT EP is waiting for more data. Ensure we are not waiting for more than CFG_MTP_EP_SIZE.
        if (p_mtp->total_len == p_mtp->xferred_len) {
          TU_ASSERT(usbd_edpt_xfer(rhport, p_mtp->ep_out, ((uint8_t *)(&p_container->data)), CFG_MTP_EP_SIZE), 0);
        } else if (p_mtp->handled_len == 0) {
          // First data block includes container header + container data
          TU_ASSERT(usbd_edpt_xfer(rhport, p_mtp->ep_out,
                                   (uint8_t*) p_container + p_mtp->xferred_len,
                                   (uint16_t)TU_MIN(p_mtp->total_len - p_mtp->xferred_len, CFG_MTP_EP_SIZE)));
        } else {
          // Successive data block includes only container data
          TU_ASSERT(usbd_edpt_xfer(rhport, p_mtp->ep_out,
                                   ((uint8_t *)(&p_container->data)) + p_mtp->xferred_len - p_mtp->handled_len,
                                   (uint16_t)TU_MIN(p_mtp->total_len - p_mtp->xferred_len, CFG_MTP_EP_SIZE)));
        }
      }
      break;
#endif

    case MTP_PHASE_RESPONSE_QUEUED:
      // response phase is complete -> prepare for new command
      TU_ASSERT(ep_addr == p_mtp->ep_in);
      tud_mtp_response_complete_cb(&cb_data);
      prepare_new_command(p_mtp);
      break;

    case MTP_PHASE_RESPONSE:
    case MTP_PHASE_ERROR:
      // supposedly to be empty
      break;
    default: return false;
  }

   if (p_mtp->phase == MTP_PHASE_ERROR) {
    // stall both IN & OUT endpoints
    usbd_edpt_stall(rhport, p_mtp->ep_out);
    usbd_edpt_stall(rhport, p_mtp->ep_in);
  }

  return true;
}

//--------------------------------------------------------------------+
// MTPD Internal functionality
//--------------------------------------------------------------------+

// Decode command and prepare response
mtp_phase_type_t mtpd_handle_cmd(mtpd_interface_t* p_mtp, tud_mtp_cb_data_t* cb_data) {
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  p_container->len = sizeof(mtp_container_header_t); // default data/response length

  tu_lookup_find(&_mtp_op_table, p_mtp->command.header.code);
  TU_LOG_DRV("  MTP command: %s\r\n", (char const*) tu_lookup_find(&_mtp_op_table, p_mtp->command.header.code));

  // pre-processed commands
  switch (p_mtp->command.header.code) {
    case MTP_OP_GET_DEVICE_INFO: {
      tud_mtp_device_info_t dev_info = {
        .standard_version = 100,
        .mtp_vendor_extension_id = 6, // MTP specs say 0xFFFFFFFF but libMTP check for value 6
        .mtp_version = 100,
#ifdef CFG_TUD_MTP_DEVICEINFO_EXTENSIONS
        .mtp_extensions = {
          .count = sizeof(CFG_TUD_MTP_DEVICEINFO_EXTENSIONS),
          .utf16 = { 0 }
        },
#else
        .mtp_extensions = 0,
#endif
        .functional_mode = 0x0000,
        .supported_operations = {
          .count = TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS),
          .arr = { CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS }
        },
        .supported_events = {
          .count = TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS),
          .arr = { CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS }
        },
        .supported_device_properties = {
          .count = TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES),
          .arr = { CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES }
        },
        .capture_formats = {
          .count = TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS),
          .arr = { CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS }
        },
        .playback_formats = {
          .count = TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS),
          .arr = { CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS }
        }
      };
#ifdef CFG_TUD_MTP_DEVICEINFO_EXTENSIONS
      for (uint8_t i=0; i < dev_info.mtp_extensions.count; i++) {
        dev_info.mtp_extensions.utf16[i] = (uint16_t)CFG_TUD_MTP_DEVICEINFO_EXTENSIONS[i];
      }
#endif
      mtp_container_add_raw(&cb_data->reply, &dev_info, sizeof(tud_mtp_device_info_t));
      p_container->type = MTP_CONTAINER_TYPE_DATA_BLOCK;
      break;
    }

    default:
      break;
  }

  tud_mtp_command_received_cb(cb_data);
  return MTP_PHASE_RESPONSE;
}
#if 0

mtp_phase_type_t mtpd_handle_data(void)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  TU_ASSERT(p_container->type == MTP_CONTAINER_TYPE_DATA_BLOCK);

  switch(p_container->code)
  {
    case MTP_OP_SEND_OBJECT_INFO:
      TU_LOG_DRV("  MTP command: MTP_OP_SEND_OBJECT_INFO-DATA_OUT\n");
      return mtpd_handle_dto_send_object_info();
    case MTP_OP_SEND_OBJECT:
      TU_LOG_DRV("  MTP command: MTP_OP_SEND_OBJECT-DATA_OUT\n");
      return mtpd_handle_dto_send_object();
    default:
      TU_LOG_DRV("  MTP command: MTP_OP_UNKNOWN_COMMAND %x!!!!\n", p_container->code);
      return false;
  }
  return true;
}

mtp_phase_type_t mtpd_handle_cmd_delete_object(void)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  uint32_t object_handle = p_container->data[0];
  uint32_t object_code_format = p_container->data[1]; // not used
  (void) object_code_format;

  mtp_response_t res = tud_mtp_storage_object_delete(object_handle);
  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESP_OK), res, "")) != MTP_PHASE_NONE) return phase;

  p_container->type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  p_container->code = MTP_RESP_OK;
  p_container->len = MTP_CONTAINER_HEADER_LENGTH;
  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_send_object_info(void)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  _mtpd_soi.storage_id = p_container->data[0];
  _mtpd_soi.parent_object_handle = (p_container->data[1] == 0xFFFFFFFF ? 0 : p_container->data[1]);

  // Enter OUT phase and wait for DATA BLOCK
  return MTP_PHASE_DATA_OUT;
}

mtp_phase_type_t mtpd_handle_dto_send_object_info(void)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  uint32_t new_object_handle = 0;
  mtp_response_t res = tud_mtp_storage_object_write_info(_mtpd_soi.storage_id, _mtpd_soi.parent_object_handle, &new_object_handle, (mtp_object_info_header_t *)p_container->data);
  mtp_phase_type_t phase;
  if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESP_OK), res, "")) != MTP_PHASE_NONE) return phase;

  // Save send_object_info
  _mtpd_soi.object_handle = new_object_handle;

  // Response
  p_container->len = MTP_CONTAINER_HEADER_LENGTH +  3 * sizeof(uint32_t);
  p_container->type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  p_container->code = MTP_RESP_OK;
  p_container->data[0] = _mtpd_soi.storage_id;
  p_container->data[1] = _mtpd_soi.parent_object_handle;
  p_container->data[2] = _mtpd_soi.object_handle;
  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_send_object(void)
{
  // Enter OUT phase and wait for DATA BLOCK
  return MTP_PHASE_DATA_OUT;
}

mtp_phase_type_t mtpd_handle_dto_send_object(void)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  uint8_t *buffer = (uint8_t *)&p_container->data;
  uint32_t buffer_size = _mtpd_itf.xferred_len - _mtpd_itf.handled_len;
  // First block of DATA
  if (_mtpd_itf.handled_len == 0)
  {
    buffer_size -= MTP_CONTAINER_HEADER_LENGTH;
  }

  if (buffer_size > 0)
  {
    mtp_response_t res = tud_mtp_storage_object_write(_mtpd_soi.object_handle, buffer, buffer_size);
    mtp_phase_type_t phase;
    if ((phase = mtpd_chk_generic(__func__, (res != MTP_RESP_OK), res, "")) != MTP_PHASE_NONE) return phase;
  }

  if (!_mtpd_itf.xfer_completed)
  {
    // Continue with next DATA BLOCK
    return MTP_PHASE_DATA_OUT;
  }

  // Send completed
  tud_mtp_storage_object_done();

  p_container->len = MTP_CONTAINER_HEADER_LENGTH;
  p_container->type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  p_container->code = MTP_RESP_OK;
  return MTP_PHASE_RESPONSE;
}

mtp_phase_type_t mtpd_handle_cmd_format_store(void)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  uint32_t storage_id = p_container->data[0];
  uint32_t file_system_format = p_container->data[1]; // not used
  (void) file_system_format;

  mtp_response_t res = tud_mtp_storage_format(storage_id);

  p_container->type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  p_container->code = res;
  p_container->len = MTP_CONTAINER_HEADER_LENGTH;
  return MTP_PHASE_RESPONSE;
}
#endif

//--------------------------------------------------------------------+
// Checker
//--------------------------------------------------------------------+
mtp_phase_type_t mtpd_chk_generic(const char *func_name, const bool err_cd, const uint16_t ret_code, const char *message)
{
  (void)func_name;
  (void)message;
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  if (err_cd)
  {
    TU_LOG_DRV("  MTP error in %s: (%x) %s\n", func_name, ret_code, message);
    p_container->type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
    p_container->code = ret_code;
    p_container->len = sizeof(mtp_container_header_t);
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
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  uint8_t *p_value = ((uint8_t *)p_container) + p_container->len;
  p_container->len += sizeof(uint8_t);
  // Verify space requirement (8 bit string length, number of wide characters including terminator)
  TU_ASSERT(p_container->len < sizeof(mtp_generic_container_t));
  *p_value = value;
  return true;
}

bool mtpd_gct_append_object_handle(const uint32_t object_handle)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  p_container->len += sizeof(uint32_t);
  TU_ASSERT(p_container->len < sizeof(mtp_generic_container_t));
  p_container->data[0]++;
  p_container->data[p_container->data[0]] = object_handle;
  return true;
}

bool mtpd_gct_append_wstring(const char *s)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  size_t len = strlen(s) + 1;
  TU_ASSERT(len <= UINT8_MAX);
  uint8_t *p_len = ((uint8_t *)p_container)+p_container->len;
  p_container->len += sizeof(uint8_t) + sizeof(wchar16_t) * len;
  // Verify space requirement (8 bit string length, number of wide characters including terminator)
  TU_ASSERT(p_container->len < sizeof(mtp_generic_container_t));
  *p_len = (uint8_t)len;
  uint8_t *p_str = p_len + sizeof(uint8_t);
  mtpd_wc16cpy(p_str, s);
  return true;
}

bool mtpd_gct_get_string(uint16_t *offset_data, char *string, const uint16_t max_size)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  uint16_t size = *(((uint8_t *)&p_container->data) + *offset_data);
  if (size > max_size)
      size = max_size;
  TU_ASSERT(*offset_data + size < sizeof(p_container->data));

  uint8_t *s = ((uint8_t *)&p_container->data) + *offset_data + sizeof(uint8_t);
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
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  TU_ASSERT(p_container->len + sizeof(uint32_t) + array_size * type_size < sizeof(p_container->data));
  uint8_t *p = ((uint8_t *)p_container) + p_container->len;
  memcpy(p, &array_size, sizeof(uint32_t));
  p += sizeof(uint32_t);
  memcpy(p, data, array_size * type_size);
  p_container->len += sizeof(uint32_t) + array_size * type_size;
  return true;
}

bool mtpd_gct_append_date(struct tm *timeinfo)
{
  mtp_generic_container_t* p_container = &_mtpd_epbuf.container;
  // strftime is not supported by all platform, this implementation is just for reference
  int len = snprintf(_mtp_datestr, sizeof(p_container->data) - p_container->len, "%04d%02d%02dT%02d%02d%02dZ",
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
