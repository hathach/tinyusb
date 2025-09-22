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

typedef struct {
  const mtp_container_header_t* cmd_header;
  tusb_xfer_result_t xfer_result;
  uint32_t xferred_bytes;
} tud_mtp_cb_complete_data_t;

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

#define MTP_DEVICE_INFO_NO_EXTENSION_STRUCT(_op_count, _event_count, _devprop_count, _capture_count, _playback_count) \
  struct TU_ATTR_PACKED { \
    uint16_t standard_version; \
    uint32_t mtp_vendor_extension_id; \
    uint16_t mtp_version; \
    uint8_t mtp_extensions; \
    uint16_t functional_mode; \
    mtp_auint16_t(_op_count) supported_operations; \
    mtp_auint16_t(_event_count) supported_events; \
    mtp_auint16_t(_devprop_count) supported_device_properties; \
    mtp_auint16_t(_capture_count) capture_formats; \
    mtp_auint16_t(_playback_count) playback_formats; \
    /* string fields will be added using append function */ \
  }

#ifdef CFG_TUD_MTP_DEVICEINFO_EXTENSIONS
   typedef MTP_DEVICE_INFO_STRUCT(
     sizeof(CFG_TUD_MTP_DEVICEINFO_EXTENSIONS), TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS),
     TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS), TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES),
     TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS), TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS)
   ) tud_mtp_device_info_t;
#else
  typedef MTP_DEVICE_INFO_NO_EXTENSION_STRUCT(
    TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS),
    TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS), TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES),
    TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS), TU_ARGS_NUM(CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS)
  ) tud_mtp_device_info_t;
#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+
bool tud_mtp_data_send(mtp_generic_container_t* data_block);
// bool tud_mtp_block_data_receive();
bool tud_mtp_response_send(mtp_generic_container_t* resp_block);

//--------------------------------------------------------------------+
// Application Callbacks
//--------------------------------------------------------------------+

// Invoked when new command is received. Application fill the out_block with either DATA or RESPONSE container
// return MTP response code
int32_t tud_mtp_command_received_cb(uint8_t idx, mtp_generic_container_t* cmd_block, mtp_generic_container_t* out_block);

// Invoked when data phase is complete
int32_t tud_mtp_data_complete_cb(uint8_t idx, mtp_container_header_t* cmd_header, mtp_generic_container_t* resp_block, tusb_xfer_result_t xfer_result, uint32_t xferred_bytes);

// Invoked when response phase is complete
int32_t tud_mtp_response_complete_cb(uint8_t idx, mtp_container_header_t* cmd_header, mtp_generic_container_t* resp_block, tusb_xfer_result_t xfer_result, uint32_t xferred_bytes);

//--------------------------------------------------------------------+
// Helper functions
//--------------------------------------------------------------------+
// Generic container function
void mtpd_wc16cpy(uint8_t *dest, const char *src);
bool mtpd_gct_append_uint8(const uint8_t value);
bool mtpd_gct_append_object_handle(const uint32_t object_handle);
bool mtpd_gct_append_wstring(const char *s);
bool mtpd_gct_get_string(uint16_t *offset_data, char *string, const uint16_t max_size);

// Append the given array to the global context buffer
// The function returns true if the data fits in the available buffer space.
bool mtpd_gct_append_array(uint32_t array_size, const void *data, size_t type_size);

// Append an UTC date string to the global context buffer
// Required format is 'YYYYMMDDThhmmss.s' optionally added 'Z' for UTC or +/-hhmm for time zone
// This function is provided for reference and only supports UTC format without partial seconds
// The function returns true if the data fits in the available buffer space.
bool mtpd_gct_append_date(struct tm *timeinfo);

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
