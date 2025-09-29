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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN0
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_MTP_DEVICE_H_
#define TUSB_MTP_DEVICE_H_

#include "common/tusb_common.h"
#include "mtp.h"

#if (CFG_TUD_ENABLED && CFG_TUD_MTP)

#ifdef __cplusplus
 extern "C" {
#endif

// callback data for Bulk Only Transfer (BOT) protocol
typedef struct {
  uint8_t idx; // mtp instance
  uint8_t phase; // current phase
  uint32_t session_id;

  const mtp_container_command_t* command_container;
  mtp_container_info_t io_container;

  tusb_xfer_result_t xfer_result;
  uint32_t total_xferred_bytes; // number of bytes transferred so far in this phase
} tud_mtp_cb_data_t;

// callback data for Control requests
typedef struct {
  uint8_t idx;
  uint8_t stage; // control stage
  uint32_t session_id;

  const tusb_control_request_t* request;
  // buffer for data stage
  uint8_t* buf;
  uint16_t bufsize;
} tud_mtp_request_cb_data_t;

// Number of supported operations, events, device properties, capture formats, playback formats
// and max number of characters for strings manufacturer, model, device_version, serial_number
#define MTP_DEVICE_INFO_STRUCT(_extension_nchars, _op_count, _event_count, _devprop_count, _capture_count, _playback_count) \
  struct TU_ATTR_PACKED { \
    uint16_t standard_version; \
    uint32_t mtp_vendor_extension_id; \
    uint16_t mtp_version; \
    mtp_string_t(_extension_nchars) mtp_extensions; \
    uint16_t functional_mode; \
    mtp_auint16_t(_op_count) supported_operations; \
    mtp_auint16_t(_event_count) supported_events; \
    mtp_auint16_t(_devprop_count) supported_device_properties; \
    mtp_auint16_t(_capture_count) capture_formats; \
    mtp_auint16_t(_playback_count) playback_formats; \
    /* string fields will be added using append function */ \
  }

typedef MTP_DEVICE_INFO_STRUCT(
  sizeof(CFG_TUD_MTP_DEVICEINFO_EXTENSIONS), TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS),
  TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS), TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES),
  TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS), TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS)
) tud_mtp_device_info_t;

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// check if mtp interface is mounted
bool tud_mtp_mounted(void);

// send data phase
bool tud_mtp_data_send(mtp_container_info_t* p_container);

// receive data phase
bool tud_mtp_data_receive(mtp_container_info_t* p_container);

// send response
bool tud_mtp_response_send(mtp_container_info_t* p_container);

// send event notification on event endpoint
bool tud_mtp_event_send(mtp_event_t* event);

//--------------------------------------------------------------------+
// Control request Callbacks
//--------------------------------------------------------------------+

// Invoked when received Cancel request. Data is available in callback data's buffer
// return false to stall the request
bool tud_mtp_request_cancel_cb(tud_mtp_request_cb_data_t* cb_data);

// Invoked when received Device Reset request
// return false to stall the request
bool tud_mtp_request_device_reset_cb(tud_mtp_request_cb_data_t* cb_data);

// Invoked when received Get Extended Event request. Application fill callback data's buffer for response
// return negative to stall the request
int32_t tud_mtp_request_get_extended_event_cb(tud_mtp_request_cb_data_t* cb_data);

// Invoked when received Get DeviceStatus request. Application fill callback data's buffer for response
// return negative to stall the request
int32_t tud_mtp_request_get_device_status_cb(tud_mtp_request_cb_data_t* cb_data);

// Invoked when received vendor-specific request not in the above standard MTP requests
// return false to stall the request
bool tud_mtp_request_vendor_cb(tud_mtp_request_cb_data_t* cb_data);

//--------------------------------------------------------------------+
// Bulk only protocol Callbacks
//--------------------------------------------------------------------+

// Invoked when new command is received. Application fill the cb_data->io_container and call tud_mtp_data_send() or
// tud_mtp_response_send() for Data or Response phase.
// Return negative to stall the endpoints
int32_t tud_mtp_command_received_cb(tud_mtp_cb_data_t * cb_data);

// Invoked when a data packet is transferred. If data spans over multiple packets, application can use
// total_xferred_bytes and io_container's payload_bytes to determine the offset and remaining bytes to be transferred.
// Return negative to stall the endpoints
int32_t tud_mtp_data_xfer_cb(tud_mtp_cb_data_t* cb_data);

// Invoked when all bytes in DATA phase is complete. A response packet is expected
// Return negative to stall the endpoints
int32_t tud_mtp_data_complete_cb(tud_mtp_cb_data_t* cb_data);

// Invoked when response phase is complete
// Return negative to stall the endpoints
int32_t tud_mtp_response_complete_cb(tud_mtp_cb_data_t* cb_data);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     mtpd_init            (void);
bool     mtpd_deinit          (void);
void     mtpd_reset           (uint8_t rhport);
uint16_t mtpd_open            (uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
bool     mtpd_control_xfer_cb (uint8_t rhport, uint8_t stage, tusb_control_request_t const *p_request);
bool     mtpd_xfer_cb         (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif
#endif
