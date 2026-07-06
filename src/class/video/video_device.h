/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-FileCopyrightText: Copyright (c) 2021 Koji Kitayama
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_VIDEO_DEVICE_H_
#define TUSB_VIDEO_DEVICE_H_

#include "common/tusb_common.h"
#include "video.h"

#ifdef __cplusplus
extern "C" {
#endif


//--------------------------------------------------------------------+
// Payload request
//--------------------------------------------------------------------+
typedef struct TU_ATTR_PACKED {
    void* buf;      /* Payload buffer to be filled */
    size_t length;  /* Length of the requested data in bytes */
    size_t offset;  /* Offset within the frame (in bytes) */
} tud_video_payload_request_t;

//--------------------------------------------------------------------+
// Application API (Multiple Ports)
// CFG_TUD_VIDEO > 1
//--------------------------------------------------------------------+

bool tud_video_n_connected(uint_fast8_t ctl_idx);

/** Return true if streaming
 *
 * @param[in] ctl_idx    Destination control interface index
 * @param[in] stm_idx    Destination streaming interface index */
bool tud_video_n_streaming(uint_fast8_t ctl_idx, uint_fast8_t stm_idx);

/** Transfer a frame
 *
 * @param[in] ctl_idx    Destination control interface index
 * @param[in] stm_idx    Destination streaming interface index
 * @param[in] buffer     Frame buffer. The caller must not use this buffer until the operation is completed.
 * @param[in] bufsize    Byte size of the frame buffer */
bool tud_video_n_frame_xfer(uint_fast8_t ctl_idx, uint_fast8_t stm_idx, void *buffer, size_t bufsize);

/*------------- Optional callbacks -------------*/
/** Invoked when compeletion of a frame transfer
 *
 * @param[in] ctl_idx    Destination control interface index
 * @param[in] stm_idx    Destination streaming interface index */
void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

/** Invoked when SET_POWER_MODE request received
 *
 * @param[in] ctl_idx    Destination control interface index
 * @param[in] stm_idx    Destination streaming interface index
 * @return video_error_code_t */
int tud_video_power_mode_cb(uint_fast8_t ctl_idx, uint8_t power_mod);

/** Invoked when VS_COMMIT_CONTROL(SET_CUR) request received
 *
 * @param[in] ctl_idx     Destination control interface index
 * @param[in] stm_idx     Destination streaming interface index
 * @param[in] parameters  Video streaming parameters
 * @return video_error_code_t */
int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                                     video_probe_and_commit_control_t const *parameters);

/** Invoked if buffer is set to NULL (allows bufferless on the fly data generation)
 *
 * @param[in]   ctl_idx       Destination control interface index
 * @param[in]   stm_idx       Destination streaming interface index
 * @param[out]  payload_buf   Payload storage buffer (target buffer for requested data)
 * @param[in]   payload_size  Size of payload_buf (requested data size)
 * @param[in]   offset        Current byte offset relative to given bufsize from tud_video_n_frame_xfer (framesize)  */
void tud_video_prepare_payload_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx, tud_video_payload_request_t* request);

//--------------------------------------------------------------------+
// INTERNAL USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
void     videod_init           (void);
bool     videod_deinit         (void);
void     videod_reset          (uint8_t rhport);
uint16_t videod_open           (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool     videod_xfer_cb        (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif
