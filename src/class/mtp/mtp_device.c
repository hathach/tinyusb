/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ennebi Elettronica (https://ennebielettronica.com)
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
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
// Data callbacks default to 0: their return is honored (negative stalls), so an
// application that does not implement them keeps the previous no-op behaviour.
TU_ATTR_WEAK int32_t tud_mtp_data_xfer_cb(tud_mtp_cb_data_t* cb_data) {
  (void) cb_data;
  return 0;
}
TU_ATTR_WEAK int32_t tud_mtp_data_complete_cb(tud_mtp_cb_data_t* cb_data) {
  (void) cb_data;
  return 0;
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
  uint8_t ep_sz_fs;
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
  "Data Complete",
  "Response",
  "Error"
};

#endif


//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
static bool prepare_new_command(mtpd_interface_t* p_mtp) {
  p_mtp->phase = MTP_PHASE_COMMAND;
  if (usbd_edpt_busy(p_mtp->rhport, p_mtp->ep_out)) {
    return true; // a read is already outstanding and will receive the command block
  }
  return usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_out, _mtpd_epbuf.buf, CFG_TUD_MTP_EP_BUFSIZE, false);
}

// Data IN: transmit the zero-length packet that terminates the data phase.
static bool queue_zlp_in(mtpd_interface_t* p_mtp, uint8_t ep_addr) {
  TU_LOG_DRV("  queue ZLP IN\r\n");
  TU_VERIFY(usbd_edpt_claim(p_mtp->rhport, ep_addr));
  TU_ASSERT(usbd_edpt_xfer(p_mtp->rhport, ep_addr, NULL, 0, false));
  return true;
}

// Data OUT: arm the read for the host's terminating zero-length packet. Sized full rather
// than zero: a read completes on any short packet and a ZLP is the shortest, so this still
// completes with xferred_bytes == 0, but a read left over from a transaction the host
// abandoned (cancel/reset) can then still receive the next command block instead of
// swallowing it. That keeps recovery to a phase reset, as MSC's Bulk-Only Reset does.
static bool queue_zlp_out(mtpd_interface_t* p_mtp, uint8_t ep_addr) {
  TU_LOG_DRV("  queue ZLP OUT\r\n");
  TU_VERIFY(usbd_edpt_claim(p_mtp->rhport, ep_addr));
  TU_ASSERT(usbd_edpt_xfer(p_mtp->rhport, ep_addr, _mtpd_epbuf.buf, CFG_TUD_MTP_EP_BUFSIZE, false));
  return true;
}

bool tud_mtp_data_send(mtp_container_info_t *p_container) {
  mtpd_interface_t *p_mtp = &_mtpd_itf;
  const uint8_t prev_phase = p_mtp->phase;
  if (p_mtp->phase == MTP_PHASE_COMMAND) {
    // 1st data block: header + payload
    p_mtp->phase = MTP_PHASE_DATA;
    p_mtp->xferred_len = 0;
    p_mtp->total_len   = p_container->header->len;

    p_container->header->type           = MTP_CONTAINER_TYPE_DATA_BLOCK;
    p_container->header->transaction_id = p_mtp->command.header.transaction_id;
    p_mtp->io_header                    = *p_container->header; // save header for subsequent data
  }

  const uint16_t xact_len = (uint16_t)tu_min32(p_mtp->total_len - p_mtp->xferred_len, CFG_TUD_MTP_EP_BUFSIZE);

  TU_LOG_DRV("  MTP Data IN: xferred_len/total_len=%lu/%lu, xact_len=%u\r\n", p_mtp->xferred_len, p_mtp->total_len,
             xact_len);
  if (xact_len) {
    if (!usbd_edpt_claim(p_mtp->rhport, p_mtp->ep_in)) {
      p_mtp->phase = prev_phase; // no data phase started: the app can retry later
      return false;
    }
    TU_ASSERT(usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_in, _mtpd_epbuf.buf, xact_len, false));
  }
  return true;
}

bool tud_mtp_data_receive(mtp_container_info_t *p_container) {
  mtpd_interface_t *p_mtp = &_mtpd_itf;
  const uint8_t prev_phase = p_mtp->phase;
  if (p_mtp->phase == MTP_PHASE_COMMAND) {
    // 1st data block: header + payload
    p_mtp->phase       = MTP_PHASE_DATA;
    p_mtp->xferred_len = 0;
    p_mtp->total_len   = p_container->header->len;
  }

  // up to buffer size since 1st packet (with header) may also contain payload
  const uint16_t xact_len = CFG_TUD_MTP_EP_BUFSIZE;

  TU_LOG_DRV("  MTP Data OUT: xferred_len/total_len=%lu/%lu, xact_len=%u\r\n", p_mtp->xferred_len, p_mtp->total_len,
             xact_len);
  if (!usbd_edpt_claim(p_mtp->rhport, p_mtp->ep_out)) {
    p_mtp->phase = prev_phase; // no data phase started: the app can retry later
    return false;
  }
  TU_ASSERT(usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_out, _mtpd_epbuf.buf, xact_len, false));
  return true;
}

bool tud_mtp_response_send(mtp_container_info_t* p_container) {
  mtpd_interface_t* p_mtp = &_mtpd_itf;
  mtp_generic_container_t* epbuf = (mtp_generic_container_t*) _mtpd_epbuf.buf;
  const uint8_t prev_phase = p_mtp->phase;

  p_container->header->type = MTP_CONTAINER_TYPE_RESPONSE_BLOCK;
  p_container->header->transaction_id = p_mtp->command.header.transaction_id;
  // The response is always transmitted from the endpoint buffer. On 2nd+ data packets the
  // wire carries payload only, so the application is handed the headerless view whose
  // header is p_mtp->io_header, outside that buffer; responding from there would otherwise
  // put payload bytes on the wire under an unrelated length. Copy the header in.
  if (p_container->header != &epbuf->header) {
    epbuf->header = *p_container->header;
  }
  TU_VERIFY(epbuf->header.len >= sizeof(mtp_container_header_t) && epbuf->header.len <= CFG_TUD_MTP_EP_BUFSIZE);

  // May be called from a data callback to end the transaction early (e.g. the application
  // rejects the data): the terminating ZLP the host still owes is absorbed below.
  p_mtp->phase = MTP_PHASE_RESPONSE;
  if (!usbd_edpt_claim(p_mtp->rhport, p_mtp->ep_in)) {
    p_mtp->phase = prev_phase; // nothing queued: do not strand the transaction
    return false;
  }
  return usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_in, (uint8_t*) epbuf, (uint16_t) epbuf->header.len, false);
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
  return usbd_edpt_xfer(p_mtp->rhport, p_mtp->ep_event, (uint8_t*) &_mtpd_epbuf.buf_event, sizeof(mtp_event_t), false);
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
  const tusb_desc_endpoint_t* ep_desc_int = (const tusb_desc_endpoint_t*) tu_desc_next(itf_desc);
  TU_ASSERT(ep_desc_int->bDescriptorType == TUSB_DESC_ENDPOINT && ep_desc_int->bmAttributes.xfer == TUSB_XFER_INTERRUPT, 0);
  TU_ASSERT(usbd_edpt_open(rhport, ep_desc_int), 0);
  p_mtp->ep_event = ep_desc_int->bEndpointAddress;

  // Open endpoint pair
  const tusb_desc_endpoint_t* ep_desc_bulk = (const tusb_desc_endpoint_t*) tu_desc_next(ep_desc_int);
  TU_ASSERT(usbd_open_edpt_pair(rhport, (const uint8_t*)ep_desc_bulk, 2, TUSB_XFER_BULK, &p_mtp->ep_out, &p_mtp->ep_in), 0);
  TU_ASSERT(prepare_new_command(p_mtp), 0);

  if (tud_speed_get() == TUSB_SPEED_FULL) {
    p_mtp->ep_sz_fs = (uint8_t)tu_edpt_packet_size(ep_desc_bulk);
  }

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
    .bufsize = request->wLength,
  };

  switch (request->bRequest) {
    case MTP_REQ_CANCEL:
      TU_LOG_DRV("  MTP request: Cancel\n");
      if (stage == CONTROL_STAGE_SETUP) {
        return tud_control_xfer(rhport, request, p_mtp->control_buf, CFG_TUD_MTP_EP_CONTROL_BUFSIZE);
      } else if (stage == CONTROL_STAGE_ACK) {
        if (p_mtp->phase == MTP_PHASE_DATA) {
          // Only a data phase is abandoned: leave it so the application can respond, and
          // re-arm ep_out (deferred if its read is still outstanding). Resetting from any
          // other phase would discard the next command or an in-flight response.
          prepare_new_command(p_mtp);
        }
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
        // no data stage: the status stage must be armed explicitly, otherwise the request
        // never completes and CONTROL_STAGE_ACK below is never reached
        tud_control_status(rhport, request);
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
  const uint16_t code = (p_mtp->phase == MTP_PHASE_COMMAND) ? p_container->header.code : p_mtp->command.header.code;
  TU_LOG_DRV("  MTP %s: %s phase\r\n", (const char *) tu_lookup_find(&_mtp_op_table, code),
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
      if (ep_addr == p_mtp->ep_in) {
        break; // leftover IN completion of an abandoned transaction: nothing to do
      }
      if (xferred_bytes == 0) {
        // Terminating ZLP of a transaction the host abandoned: absorb it and listen again.
        prepare_new_command(p_mtp);
        break;
      }
      // received new command. A runt container cannot be parsed and must not be matched
      // against stale buffer contents -> error phase, stall for the host to reset us.
      if (xferred_bytes < sizeof(mtp_container_header_t) || ep_addr != p_mtp->ep_out ||
          p_container->header.type != MTP_CONTAINER_TYPE_COMMAND_BLOCK) {
        p_mtp->phase = MTP_PHASE_ERROR;
        break;
      }
      memcpy(&p_mtp->command, p_container, sizeof(mtp_container_command_t)); // save new command
      p_container->header.len = sizeof(mtp_container_header_t); // default container to header only
      preprocess_cmd(p_mtp, &cb_data);
      if (tud_mtp_command_received_cb(&cb_data) < 0) {
        p_mtp->phase = MTP_PHASE_ERROR;
      }
      break;
    }

    case MTP_PHASE_DATA: {
      p_mtp->xferred_len += xferred_bytes;
      cb_data.total_xferred_bytes = p_mtp->xferred_len;

      const bool is_data_in = (ep_addr == p_mtp->ep_in);
      // For IN endpoint, threshold is bulk max packet size
      // For OUT endpoint, threshold is endpoint buffer size, since we always queue fixed size
      uint16_t threshold;
      if (is_data_in) {
        threshold = (p_mtp->ep_sz_fs > 0) ? p_mtp->ep_sz_fs : 512; // full speed bulk if set
      } else {
        threshold = CFG_TUD_MTP_EP_BUFSIZE;
      }

      // Check completion for IN and OUT separately
      bool is_complete;
      if (is_data_in) {
        // IN completion: short packet, ZLP, or reaching total_len
        is_complete = (xferred_bytes == 0 || xferred_bytes < threshold || p_mtp->xferred_len >= p_mtp->total_len);
      } else {
        // OUT completion: reaching total_len or ZLP only. A short packet does NOT end the phase
        // (an early short packet before total_len is the cancel case, not normal completion).
        is_complete = (p_mtp->xferred_len >= p_mtp->total_len) || ((xferred_bytes == 0 && p_mtp->xferred_len > 0));
      }

      TU_LOG_DRV("  MTP Data %s CB: xferred_bytes=%lu, xferred_len/total_len=%lu/%lu, is_complete=%d\r\n",
                 is_data_in ? "IN" : "OUT", xferred_bytes, p_mtp->xferred_len, p_mtp->total_len, is_complete ? 1 : 0);

      // Send/queue ZLP if packet is full-sized but transfer is complete. Both directions
      // claim the endpoint before yielding to the application; OUT then still delivers this
      // final payload from the same buffer the ZLP read is armed on, which a host that
      // honours its own declared container length will only ever complete with 0 bytes.
      const bool need_zlp = is_complete && xferred_bytes > 0 && !(xferred_bytes & (threshold - 1));
      if (is_data_in && need_zlp) {
        if (queue_zlp_in(p_mtp, ep_addr)) {
          return true;
        }
        p_mtp->phase = MTP_PHASE_ERROR; // endpoint unavailable: stall instead of wedging
        break;
      }

      if (is_data_in) {
        // Data In
        if (is_complete) {
          p_mtp->phase = MTP_PHASE_DATA_COMPLETE;
          cb_data.phase = MTP_PHASE_DATA_COMPLETE;
          cb_data.io_container.header->len = sizeof(mtp_container_header_t);
          if (tud_mtp_data_complete_cb(&cb_data) < 0) {
            p_mtp->phase = MTP_PHASE_ERROR;
          }
        } else {
          // 2nd+ packet: payload only
          cb_data.io_container = headerless_packet;
          if (tud_mtp_data_xfer_cb(&cb_data) < 0) {
            p_mtp->phase = MTP_PHASE_ERROR;
          }
        }
      } else {
        // Data Out
        if (p_mtp->xferred_len == xferred_bytes) {
          // 1st OUT packet: header + payload. A transaction shorter than a container
          // header (including a bare ZLP, which would otherwise leave no read armed and
          // no app callback) violates the protocol -> error phase, stall both endpoints.
          if (xferred_bytes < sizeof(mtp_container_header_t)) {
            p_mtp->phase = MTP_PHASE_ERROR;
            break;
          }
          p_mtp->io_header = p_container->header; // save header for subsequent transaction
          cb_data.io_container.payload_bytes = xferred_bytes - sizeof(mtp_container_header_t);
        } else {
          // 2nd+ packet: payload only
          cb_data.io_container = headerless_packet;
          cb_data.io_container.payload_bytes = xferred_bytes;
        }
        if (need_zlp && !queue_zlp_out(p_mtp, ep_addr)) {
          p_mtp->phase = MTP_PHASE_ERROR; // endpoint unavailable: stall instead of wedging
          break;
        }
        if (xferred_bytes > 0 && tud_mtp_data_xfer_cb(&cb_data) < 0) {
          // application aborts the transfer: stall rather than arm another read. This is
          // its only way out of a data phase, since tud_mtp_response_send() is refused
          // until the phase completes.
          p_mtp->phase = MTP_PHASE_ERROR;
        } else if (need_zlp) {
          // The data phase is not over until the host's terminating ZLP, whose read was
          // armed above; tud_mtp_data_complete_cb() is delivered when it completes.
          return true;
        } else if (is_complete && p_mtp->phase == MTP_PHASE_DATA) {
          // back to header + payload for response. Skipped when the callback above already
          // answered, otherwise the application would be asked to respond a second time.
          p_mtp->phase = MTP_PHASE_DATA_COMPLETE;
          cb_data.phase = MTP_PHASE_DATA_COMPLETE;
          cb_data.io_container = headered_packet;
          cb_data.io_container.header->len = sizeof(mtp_container_header_t);
          if (tud_mtp_data_complete_cb(&cb_data) < 0) {
            p_mtp->phase = MTP_PHASE_ERROR;
          }
        }
      }
      break;
    }

    case MTP_PHASE_DATA_COMPLETE:
      // nothing is armed while waiting for the application's response: any completion
      // here means it queued a transfer instead -> stall so the host can reset us
      p_mtp->phase = MTP_PHASE_ERROR;
      break;

    case MTP_PHASE_RESPONSE:
      if (ep_addr == p_mtp->ep_out) {
        // terminating ZLP of a data phase the application answered early: absorb it
        TU_VERIFY(xferred_bytes == 0);
        break;
      }
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
