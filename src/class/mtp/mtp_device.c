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
#include "device/dcd.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "mtp_device.h"

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUD_MTP_LOG_LEVEL
  #define CFG_TUD_MTP_LOG_LEVEL   CFG_TUD_LOG_LEVEL
#endif

#define TU_LOG_DRV(...)   TU_LOG(CFG_TUD_MTP_LOG_LEVEL, __VA_ARGS__)

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+
TU_ATTR_WEAK bool tud_mtp_request_cancel_cb(tud_mtp_request_cb_data_t* cb_data) {
  (void) cb_data;
  return false;
}
TU_ATTR_WEAK bool tud_mtp_request_device_reset_cb(tud_mtp_request_cb_data_t* cb_data) {
  (void) cb_data;
  return false;
}
TU_ATTR_WEAK int32_t tud_mtp_request_get_extended_event_cb(tud_mtp_request_cb_data_t* cb_data) {
  (void) cb_data;
  return -1;
}
TU_ATTR_WEAK int32_t tud_mtp_request_get_device_status_cb(tud_mtp_request_cb_data_t* cb_data) {
  (void) cb_data;
  return -1;
}
TU_ATTR_WEAK bool tud_mtp_request_vendor_cb(tud_mtp_request_cb_data_t* cb_data) {
  (void) cb_data;
  return false;
}
TU_ATTR_WEAK int32_t tud_mtp_command_received_cb(tud_mtp_cb_data_t * cb_data) {
  (void) cb_data;
  return -1;
}
TU_ATTR_WEAK int32_t tud_mtp_data_xfer_cb(tud_mtp_cb_data_t* cb_data) {
  (void) cb_data;
  return -1;
}
TU_ATTR_WEAK int32_t tud_mtp_data_complete_cb(tud_mtp_cb_data_t* cb_data) {
  (void) cb_data;
  return -1;
}
TU_ATTR_WEAK int32_t tud_mtp_response_complete_cb(tud_mtp_cb_data_t* cb_data) {
  (void) cb_data;
  return -1;
}

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
  mtp_container_header_t io_header;

  TU_ATTR_ALIGNED(4) uint8_t control_buf[CFG_TUD_MTP_EP_CONTROL_BUFSIZE];
} mtpd_interface_t;

typedef struct {
  TUD_EPBUF_DEF(buf, CFG_TUD_MTP_EP_BUFSIZE);
  TUD_EPBUF_TYPE_DEF(mtp_event_t, buf_event);
} mtpd_epbuf_t;

//--------------------------------------------------------------------+
// INTERNAL FUNCTION DECLARATION
//--------------------------------------------------------------------+
static mtpd_interface_t _mtpd_itf;
CFG_TUD_MEM_SECTION static mtpd_epbuf_t _mtpd_epbuf;

static void preprocess_cmd(mtpd_interface_t* p_mtp, tud_mtp_cb_data_t* cb_data);

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

TU_ATTR_UNUSED static const char* _mtp_phase_str[] = {
  "Command",
  "Data",
  "Response",
  "Error"
};

#endif


//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
static bool prepare_new_command(mtpd_interface_t* p_mtp) {
  p_mtp->phase = MTP_PHASE_COMMAND;
  return usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_out, _mtpd_epbuf.buf, CFG_TUD_MTP_EP_BUFSIZE);
}

static bool mtpd_data_xfer(mtp_container_info_t* p_container, uint8_t ep_addr) {
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  if (p_mtp->phase == MTP_PHASE_COMMAND) {
    // 1st data block: header + payload
    p_mtp->phase = MTP_PHASE_DATA;
    p_mtp->xferred_len = 0;

    if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
      p_mtp->total_len = p_container->header->len;
      p_container->header->type = MTP_CONTAINER_TYPE_DATA_BLOCK;
      p_container->header->transaction_id = p_mtp->command.header.transaction_id;
      p_mtp->io_header = *p_container->header; // save header for subsequent data
    } else {
      // OUT transfer: total length is at least max packet size
      p_mtp->total_len = tu_max32(p_container->header->len, CFG_TUD_MTP_EP_BUFSIZE);
    }
  } else {
    // subsequent data block: payload only
    TU_ASSERT(p_mtp->phase == MTP_PHASE_DATA);
  }

  const uint16_t xact_len = tu_min16((uint16_t) (p_mtp->total_len - p_mtp->xferred_len), CFG_TUD_MTP_EP_BUFSIZE);
  if (xact_len) {
    // already transferred all bytes in header's length. Application make an unnecessary extra call
    TU_VERIFY(usbd_edpt_claim(p_mtp->rhport, ep_addr));
    TU_ASSERT(usbd_edpt_xfer(p_mtp->rhport, ep_addr, _mtpd_epbuf.buf, xact_len));
  }
  return true;
}

bool tud_mtp_data_send(mtp_container_info_t* p_container) {
  return mtpd_data_xfer(p_container, _mtpd_itf.ep_in);
}

bool tud_mtp_data_receive(mtp_container_info_t* p_container) {
  return mtpd_data_xfer(p_container, _mtpd_itf.ep_out);
}

bool tud_mtp_response_send(mtp_container_info_t* p_container) {
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  p_mtp->phase = MTP_PHASE_RESPONSE;
  p_container->header->type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  p_container->header->transaction_id = p_mtp->command.header.transaction_id;
  TU_VERIFY(usbd_edpt_claim(p_mtp->rhport, p_mtp->ep_in));
  return usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_in, _mtpd_epbuf.buf, (uint16_t)p_container->header->len);
}

bool tud_mtp_mounted(void) {
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  return p_mtp->ep_out != 0 && p_mtp->ep_in != 0;
}

bool tud_mtp_event_send(mtp_event_t* event) {
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  TU_VERIFY(p_mtp->ep_event != 0);
  _mtpd_epbuf.buf_event = *event;
  TU_VERIFY(usbd_edpt_claim(p_mtp->rhport, p_mtp->ep_event)); // Claim the endpoint
  return usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_event, (uint8_t*) &_mtpd_epbuf.buf_event, sizeof(mtp_event_t));
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void mtpd_init(void) {
  tu_memclr(&_mtpd_itf, sizeof(mtpd_interface_t));
}

bool mtpd_deinit(void) {
  return true; // nothing to do
}

void mtpd_reset(uint8_t rhport) {
  (void) rhport;
  tu_memclr(&_mtpd_itf, sizeof(mtpd_interface_t));
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
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  tud_mtp_request_cb_data_t cb_data = {
    .idx = 0,
    .stage = stage,
    .session_id = p_mtp->session_id,
    .request = request,
    .buf = p_mtp->control_buf,
    .bufsize = tu_le16toh(request->wLength),
  };

  switch (request->bRequest) {
    case MTP_REQ_CANCEL:
      TU_LOG_DRV("  MTP request: Cancel\n");
      if (stage == CONTROL_STAGE_SETUP) {
        return tud_control_xfer(rhport, request, p_mtp->control_buf, CFG_TUD_MTP_EP_CONTROL_BUFSIZE);
      } else if (stage == CONTROL_STAGE_ACK) {
        return tud_mtp_request_cancel_cb(&cb_data);
      }
      break;

    case MTP_REQ_GET_EXT_EVENT_DATA:
      TU_LOG_DRV("  MTP request: Get Extended Event Data\n");
      if (stage == CONTROL_STAGE_SETUP) {
        const int32_t len = tud_mtp_request_get_extended_event_cb(&cb_data);
        TU_VERIFY(len > 0);
        return tud_control_xfer(rhport,request, p_mtp->control_buf, (uint16_t) len);
      }
      break;

    case MTP_REQ_RESET:
      TU_LOG_DRV("  MTP request: Device Reset\n");
      // used by the host to return the Still Image Capture Device to the Idle state after the Bulk-pipe has stalled
      if (stage == CONTROL_STAGE_SETUP) {
        // clear stalled
        if (usbd_edpt_stalled(rhport, p_mtp->ep_out)) {
          usbd_edpt_clear_stall(rhport, p_mtp->ep_out);
        }
        if (usbd_edpt_stalled(rhport, p_mtp->ep_in)) {
          usbd_edpt_clear_stall(rhport, p_mtp->ep_in);
        }
      } else if (stage == CONTROL_STAGE_ACK) {
        prepare_new_command(p_mtp);
        return tud_mtp_request_device_reset_cb(&cb_data);
      }
      break;

    case MTP_REQ_GET_DEVICE_STATUS: {
      TU_LOG_DRV("  MTP request: Get Device Status\n");
      if (stage == CONTROL_STAGE_SETUP) {
        const int32_t len = tud_mtp_request_get_device_status_cb(&cb_data);
        TU_VERIFY(len > 0);
        return tud_control_xfer(rhport, request, p_mtp->control_buf, (uint16_t) len);
      }
      break;
    }

    default:
      return tud_mtp_request_vendor_cb(&cb_data);
  }

  return true;
}

// Transfer on bulk endpoints
bool mtpd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes) {
  if (ep_addr == _mtpd_itf.ep_event) {
    // nothing to do
    return true;
  }

  mtpd_interface_t* p_mtp = &_mtpd_itf;
  mtp_generic_container_t* p_container = (mtp_generic_container_t*) _mtpd_epbuf.buf;

#if CFG_TUSB_DEBUG >= CFG_TUD_MTP_LOG_LEVEL
  tu_lookup_find(&_mtp_op_table, p_mtp->command.header.code);
  TU_LOG_DRV("  MTP %s: %s phase\r\n", (const char *) tu_lookup_find(&_mtp_op_table, p_mtp->command.header.code),
    _mtp_phase_str[p_mtp->phase]);
#endif

  const mtp_container_info_t headered_packet = {
    .header = &p_container->header,
    .payload = p_container->payload,
    .payload_bytes = CFG_TUD_MTP_EP_BUFSIZE - sizeof(mtp_container_header_t)
  };

  const mtp_container_info_t headerless_packet = {
    .header = &p_mtp->io_header,
    .payload = _mtpd_epbuf.buf,
    .payload_bytes = CFG_TUD_MTP_EP_BUFSIZE
  };

  tud_mtp_cb_data_t cb_data;
  cb_data.idx = 0;
  cb_data.phase = p_mtp->phase;
  cb_data.session_id = p_mtp->session_id;
  cb_data.command_container = &p_mtp->command;
  cb_data.io_container = headered_packet;
  cb_data.total_xferred_bytes = 0;
  cb_data.xfer_result = event;

  switch (p_mtp->phase) {
    case MTP_PHASE_COMMAND: {
      // received new command
      TU_VERIFY(ep_addr == p_mtp->ep_out && p_container->header.type == MTP_CONTAINER_TYPE_COMMAND_BLOCK);
      memcpy(&p_mtp->command, p_container, sizeof(mtp_container_command_t)); // save new command
      p_container->header.len = sizeof(mtp_container_header_t); // default container to header only
      preprocess_cmd(p_mtp, &cb_data);
      if (tud_mtp_command_received_cb(&cb_data) < 0) {
        p_mtp->phase = MTP_PHASE_ERROR;
      }
      break;
    }

    case MTP_PHASE_DATA: {
      const uint16_t bulk_mps = (tud_speed_get() == TUSB_SPEED_HIGH) ? 512 : 64;
      p_mtp->xferred_len += xferred_bytes;
      cb_data.total_xferred_bytes = p_mtp->xferred_len;

      bool is_complete = false;
      // complete if ZLP or short packet or overflow
      if (xferred_bytes == 0 || // ZLP
          (xferred_bytes & (bulk_mps - 1)) || // short packet
          p_mtp->xferred_len > p_mtp->total_len) {
        is_complete = true;
      }

      if (ep_addr == p_mtp->ep_in) {
        // Data In
        if (is_complete) {
          cb_data.io_container.header->len = sizeof(mtp_container_header_t);
          tud_mtp_data_complete_cb(&cb_data);
        } else {
          // 2nd+ packet: payload only
          cb_data.io_container = headerless_packet;
          tud_mtp_data_xfer_cb(&cb_data);
        }
      } else {
        // Data Out
        if (p_mtp->xferred_len == xferred_bytes) {
          // 1st OUT packet: header + payload
          p_mtp->io_header = p_container->header; // save header for subsequent transaction
          cb_data.io_container.payload_bytes = xferred_bytes - sizeof(mtp_container_header_t);
        } else {
          // 2nd+ packet: payload only
          cb_data.io_container = headerless_packet;
          cb_data.io_container.payload_bytes = xferred_bytes;
        }
        tud_mtp_data_xfer_cb(&cb_data);

        if (is_complete) {
          // back to header + payload for response
          cb_data.io_container = headered_packet;
          cb_data.io_container.header->len = sizeof(mtp_container_header_t);
          tud_mtp_data_complete_cb(&cb_data);
        }
      }
      break;
    }

    case MTP_PHASE_RESPONSE:
      // response phase is complete -> prepare for new command
      TU_ASSERT(ep_addr == p_mtp->ep_in);
      tud_mtp_response_complete_cb(&cb_data);
      prepare_new_command(p_mtp);
      break;

    case MTP_PHASE_ERROR:
      // handled after switch, supposedly to be empty
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

// pre-processed commands
void preprocess_cmd(mtpd_interface_t* p_mtp, tud_mtp_cb_data_t* cb_data) {
  switch (p_mtp->command.header.code) {
    case MTP_OP_GET_DEVICE_INFO: {
      tud_mtp_device_info_t dev_info = {
        .standard_version = 100,
        .mtp_vendor_extension_id = 6, // MTP specs say 0xFFFFFFFF but libMTP check for value 6
        .mtp_version = 100,
        .mtp_extensions = {
          .count = sizeof(CFG_TUD_MTP_DEVICEINFO_EXTENSIONS),
          .utf16 = { 0 }
        },
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

      for (uint8_t i=0; i < dev_info.mtp_extensions.count; i++) {
        dev_info.mtp_extensions.utf16[i] = (uint16_t)CFG_TUD_MTP_DEVICEINFO_EXTENSIONS[i];
      }

      mtp_container_add_raw(&cb_data->io_container, &dev_info, sizeof(tud_mtp_device_info_t));
      break;
    }

    default:
      break;
  }
}

#endif
