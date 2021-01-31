/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber
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

#ifndef _TUSB_AUDIO_DEVICE_H_
#define _TUSB_AUDIO_DEVICE_H_

#include "assert.h"
#include "common/tusb_common.h"
#include "device/usbd.h"

#include "audio.h"

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

// All sizes are in bytes!

// Number of Standard AS Interface Descriptors (4.9.1) defined per audio function - this is required to be able to remember the current alternate settings of these interfaces - We restrict us here to have a constant number for all audio functions (which means this has to be the maximum number of AS interfaces an audio function has and a second audio function with less AS interfaces just waste a few bytes)
#ifndef CFG_TUD_AUDIO_N_AS_INT
#error You must tell the driver the number of Standard AS Interface Descriptors you have defined in the descriptors!
#endif

// Size of control buffer used to receive and send control messages via EP0 - has to be big enough to hold your biggest request structure e.g. range requests with multiple intervals defined or cluster descriptors
#ifndef CFG_TUD_AUDIO_CTRL_BUF_SIZE
#error You must define an audio class control request buffer size!
#endif

// End point sizes - Limits: Full Speed <= 1023, High Speed <= 1024
#ifndef CFG_TUD_AUDIO_EPSIZE_IN
#define CFG_TUD_AUDIO_EPSIZE_IN 0   // TX
#endif

#ifndef CFG_TUD_AUDIO_EPSIZE_OUT
#define CFG_TUD_AUDIO_EPSIZE_OUT 0  // RX
#endif

// Software EP FIFO buffer sizes - must be >= EP SIZEs!
#if CFG_TUD_AUDIO_EPSIZE_IN
#ifndef CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE
#define CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE CFG_TUD_AUDIO_EPSIZE_IN    // TX
#endif
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT
#ifndef CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE
#define CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE CFG_TUD_AUDIO_EPSIZE_OUT  // RX
#endif
#endif

// General information of number of TX and/or RX channels - is used in case support FIFOs (see below) are used and can be used for descriptor definitions
#ifndef CFG_TUD_AUDIO_N_CHANNELS_TX
#define CFG_TUD_AUDIO_N_CHANNELS_TX                     0
#endif

#ifndef CFG_TUD_AUDIO_N_CHANNELS_RX
#define CFG_TUD_AUDIO_N_CHANNELS_RX                     0
#endif

// Use of TX/RX support FIFOs

// Support FIFOs are not mandatory for the audio driver, rather they are intended to be of use in
// - TX case: CFG_TUD_AUDIO_N_CHANNELS_TX channels need to be encoded into one USB output stream (currently PCM type I is implemented)
// - RX case: CFG_TUD_AUDIO_N_CHANNELS_RX channels need to be decoded from a single USB input stream (currently PCM type I is implemented)
//
// This encoding/decoding is done in software and thus time consuming. If you can encode/decode your stream more efficiently do not use the
// support FIFOs but write/read directly into/from the EP_X_SW_BUFFER_FIFOs using
// - tud_audio_n_write() or
// - tud_audio_n_read().
// To write/read to/from the support FIFOs use
// - tud_audio_n_write_support_ff() or
// - tud_audio_n_read_support_ff().
//
// The encoding/decoding format type done is defined below.
//
// The encoding/decoding starts when the private callback functions
// - audio_tx_done_cb()
// - audio_rx_done_cb()
// are invoked. If support FIFOs are used the corresponding encoding/decoding functions are called from there.
// Once encoding/decoding is done the result is put directly into the EP_X_SW_BUFFER_FIFOs. You can use the public callback functions
// - tud_audio_tx_done_pre_load_cb() or tud_audio_tx_done_post_load_cb()
// - tud_audio_rx_done_pre_read_cb() or tud_audio_rx_done_post_read_cb()
// if you want to get informed what happened.
//
// If you don't use the support FIFOs you may use the public callback functions
// - tud_audio_tx_done_pre_load_cb() or tud_audio_tx_done_post_load_cb()
// - tud_audio_rx_done_pre_read_cb() or tud_audio_rx_done_post_read_cb()
// to write/read from/into the EP_X_SW_BUFFER_FIFOs at the right time.
//
// If you need a different encoding which is not support so far implement it in the
// - audio_tx_done_cb()
// - audio_rx_done_cb()
// functions.

// Size of support FIFOs - if size > 0 there are as many FIFOs set up as TX/RX channels defined
#ifndef CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
#define CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE  0                // Buffer size per channel - minimum size: ceil(f_s/1000)*CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX
#endif

#ifndef CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
#define CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE  0                // Buffer size per channel - minimum size: ceil(f_s/1000)*CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX
#endif

// Enable/disable feedback EP (required for asynchronous RX applications)
#ifndef CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP 0                      // Feedback - 0 or 1
#endif

// Audio interrupt control EP size - disabled if 0
#ifndef CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
#define CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN 0                       // Audio interrupt control - if required - 6 Bytes according to UAC 2 specification (p. 74)
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
#ifndef CFG_TUD_AUDIO_INT_CTR_BUFSIZE
#define CFG_TUD_AUDIO_INT_CTR_BUFSIZE   6                       // Buffer size of audio control interrupt EP - 6 Bytes according to UAC 2 specification (p. 74)
#endif
#endif

// Audio data format types - look in audio.h for existing types
// Used in case support FIFOs are used
#ifndef CFG_TUD_AUDIO_FORMAT_TYPE_TX
#define CFG_TUD_AUDIO_FORMAT_TYPE_TX                AUDIO_FORMAT_TYPE_UNDEFINED     // If this option is used, an encoding function has to be implemented in audio_device.c
#endif

#ifndef CFG_TUD_AUDIO_FORMAT_TYPE_RX
#define CFG_TUD_AUDIO_FORMAT_TYPE_RX                AUDIO_FORMAT_TYPE_UNDEFINED     // If this option is used, a decoding function has to be implemented in audio_device.c
#endif

// Audio data format type I specifications
#if CFG_TUD_AUDIO_FORMAT_TYPE_TX == AUDIO_FORMAT_TYPE_I

// Type definitions - for possible formats see: audio_data_format_type_I_t and further in UAC2 specifications.
#ifndef CFG_TUD_AUDIO_FORMAT_TYPE_I_TX
#define CFG_TUD_AUDIO_FORMAT_TYPE_I_TX              AUDIO_DATA_FORMAT_TYPE_I_PCM
#endif

#ifndef CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX         // bSubslotSize
#define CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX         1
#endif

#ifndef CFG_TUD_AUDIO_TX_ITEMSIZE
#if CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX == 1
#define CFG_TUD_AUDIO_TX_ITEMSIZE 1
#elif CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX == 2
#define CFG_TUD_AUDIO_TX_ITEMSIZE 2
#else
#define CFG_TUD_AUDIO_TX_ITEMSIZE 4
#endif
#endif

#if CFG_TUD_AUDIO_TX_ITEMSIZE < CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX
#error FIFO element size (ITEMSIZE) must not be smaller then sample size
#endif

#endif

#if CFG_TUD_AUDIO_FORMAT_TYPE_RX == AUDIO_FORMAT_TYPE_I

#ifndef CFG_TUD_AUDIO_FORMAT_TYPE_I_RX
#define CFG_TUD_AUDIO_FORMAT_TYPE_I_RX              AUDIO_DATA_FORMAT_TYPE_I_PCM
#endif

#ifndef CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX         // bSubslotSize
#define CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX         1
#endif

#if CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX == 1
#define CFG_TUD_AUDIO_RX_ITEMSIZE 1
#elif CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX == 2
#define CFG_TUD_AUDIO_RX_ITEMSIZE 2
#else
#define CFG_TUD_AUDIO_RX_ITEMSIZE 4
#endif

#endif

//static_assert(sizeof(tud_audio_desc_lengths) != CFG_TUD_AUDIO, "Supply audio function descriptor pack length!");

// Supported types of this driver:
// AUDIO_DATA_FORMAT_TYPE_I_PCM     -   Required definitions: CFG_TUD_AUDIO_N_CHANNELS and CFG_TUD_AUDIO_BYTES_PER_CHANNEL

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup AUDIO_Serial Serial
 *  @{
 *  \defgroup   AUDIO_Serial_Device Device
 *  @{ */

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces)
// CFG_TUD_AUDIO > 1
//--------------------------------------------------------------------+
bool     tud_audio_n_mounted    (uint8_t itf);

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE

uint16_t tud_audio_n_available                    (uint8_t itf);
uint16_t tud_audio_n_read                         (uint8_t itf, void* buffer, uint16_t bufsize);
void     tud_audio_n_clear_ep_out_ff              (uint8_t itf);                          // Delete all content in the EP OUT FIFO

#if CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
void     tud_audio_n_clear_rx_support_ff          (uint8_t itf, uint8_t channelId);       // Delete all content in the support RX FIFOs
uint16_t tud_audio_n_available_support_ff         (uint8_t itf, uint8_t channelId);
uint16_t tud_audio_n_read_support_ff              (uint8_t itf, uint8_t channelId, void* buffer, uint16_t bufsize);
#endif

#endif

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE

uint16_t tud_audio_n_write                        (uint8_t itf, const void * data, uint16_t len);
void     tud_audio_n_clear_ep_in_ff               (uint8_t itf);                          // Delete all content in the EP IN FIFO

#if CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
uint16_t tud_audio_n_flush_tx_support_ff          (uint8_t itf);      // Force all content in the support TX FIFOs to be written into EP SW FIFO
uint16_t tud_audio_n_clear_tx_support_ff          (uint8_t itf, uint8_t channelId);
uint16_t tud_audio_n_write_support_ff             (uint8_t itf, uint8_t channelId, const void * data, uint16_t len);
#endif

#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
uint16_t    tud_audio_int_ctr_n_available         (uint8_t itf);
uint16_t    tud_audio_int_ctr_n_read              (uint8_t itf, void* buffer, uint16_t bufsize);
void        tud_audio_int_ctr_n_clear             (uint8_t itf);      // Delete all content in the AUDIO_INT_CTR FIFO
uint16_t    tud_audio_int_ctr_n_write             (uint8_t itf, uint8_t const* buffer, uint16_t len);
#endif

//--------------------------------------------------------------------+
// Application API (Interface0)
//--------------------------------------------------------------------+

static inline bool         tud_audio_mounted                (void);

// RX API

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE

static inline uint16_t     tud_audio_available              (void);
static inline void         tud_audio_clear_ep_out_ff        (void);                       // Delete all content in the EP OUT FIFO
static inline uint16_t     tud_audio_read                   (void* buffer, uint16_t bufsize);

#if CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
static inline void     tud_audio_clear_rx_support_ff        (uint8_t channelId);
static inline uint16_t tud_audio_available_support_ff       (uint8_t channelId);
static inline uint16_t tud_audio_read_support_ff            (uint8_t channelId, void* buffer, uint16_t bufsize);
#endif

#endif

// TX API

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE

static inline uint16_t tud_audio_write                      (const void * data, uint16_t len);
static inline uint16_t tud_audio_clear_ep_in_ff             (void);

#if CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
static inline uint16_t tud_audio_flush_tx_support_ff        (void);
static inline uint16_t tud_audio_clear_tx_support_ff        (uint8_t channelId);
static inline uint16_t tud_audio_write_support_ff           (uint8_t channelId, const void * data, uint16_t len);
#endif

#endif

// INT CTR API

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
static inline uint16_t     tud_audio_int_ctr_available      (void);
static inline uint16_t     tud_audio_int_ctr_read           (void* buffer, uint16_t bufsize);
static inline void         tud_audio_int_ctr_clear          (void);
static inline uint16_t     tud_audio_int_ctr_write          (uint8_t const* buffer, uint16_t len);
#endif

// Buffer control EP data and schedule a transmit
// This function is intended to be used if you do not have a persistent buffer or memory location available (e.g. non-local variables) and need to answer onto a
// get request. This function buffers your answer request frame into the control buffer of the corresponding audio driver and schedules a transmit for sending it.
// Since transmission is triggered via interrupts, a persistent memory location is required onto which the buffer pointer in pointing. If you already have such
// available you may directly use 'tud_control_xfer(...)'. In this case data does not need to be copied into an additional buffer and you save some time.
// If the request's wLength is zero, a status packet is sent instead.
bool tud_audio_buffer_and_schedule_control_xfer(uint8_t rhport, tusb_control_request_t const * p_request, void* data, uint16_t len);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE
TU_ATTR_WEAK bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting);
TU_ATTR_WEAK bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting);
#endif

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE
TU_ATTR_WEAK bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint8_t itf, uint8_t ep_out, uint8_t cur_alt_setting);
TU_ATTR_WEAK bool tud_audio_rx_done_post_read_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t itf, uint8_t ep_out, uint8_t cur_alt_setting);
#endif

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
TU_ATTR_WEAK bool tud_audio_fb_done_cb(uint8_t rhport);
// User code should call this function with feedback value in 16.16 format for FS and HS.
// Value will be corrected for FS to 10.14 format automatically.
// (see Universal Serial Bus Specification Revision 2.0 5.12.4.2).
// Feedback value will be sent at FB endpoint interval till it's changed.
bool tud_audio_fb_set(uint8_t rhport, uint32_t feedback);
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
TU_ATTR_WEAK bool tud_audio_int_ctr_done_cb(uint8_t rhport, uint16_t * n_bytes_copied);
#endif

// Invoked when audio set interface request received
TU_ATTR_WEAK bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio set interface request received which closes an EP
TU_ATTR_WEAK bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio class specific set request received for an EP
TU_ATTR_WEAK bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific set request received for an interface
TU_ATTR_WEAK bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific set request received for an entity
TU_ATTR_WEAK bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific get request received for an EP
TU_ATTR_WEAK bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio class specific get request received for an interface
TU_ATTR_WEAK bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio class specific get request received for an entity
TU_ATTR_WEAK bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

static inline bool tud_audio_mounted(void)
{
  return tud_audio_n_mounted(0);
}

// RX API

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE

static inline uint16_t     tud_audio_available              (void)
{
  return tud_audio_n_available(0);
}

static inline uint16_t     tud_audio_read                   (void* buffer, uint16_t bufsize)
{
  return tud_audio_n_read(0, buffer, bufsize);
}

static inline uint16_t     tud_audio_clear_ep_out_ff        (void)
{
  return tud_audio_n_clear_ep_out_ff(0);
}

#if CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE

static inline void     tud_audio_clear_rx_support_ff        (uint8_t channelId)
{
  tud_audio_n_clear_rx_support_ff(0, channelId);
}

static inline uint16_t tud_audio_available_support_ff       (uint8_t channelId)
{
  return tud_audio_n_available_support_ff(0, channelId);
}

static inline uint16_t tud_audio_read_support_ff            (uint8_t channelId, void* buffer, uint16_t bufsize)
{
  return tud_audio_n_read_support_ff(0, channelId, buffer, bufsize);
}

#endif

#endif

// TX API

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE

static inline uint16_t tud_audio_write                      (const void * data, uint16_t len)
{
  return tud_audio_n_write(0, data, len);
}

static inline uint16_t tud_audio_clear_ep_in_ff             (void)
{
  return tud_audio_n_clear_ep_in_ff(0);
}

#if CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE

static inline uint16_t tud_audio_flush_tx_support_ff        (void)
{
  return tud_audio_n_flush_tx_support_ff(0);
}

static inline uint16_t tud_audio_clear_tx_support_ff        (uint8_t channelId)
{
  return tud_audio_n_clear_tx_support_ff(0, channelId);
}

static inline uint16_t tud_audio_write_support_ff           (uint8_t channelId, const void * data, uint16_t len)
{
  return tud_audio_n_write_support_ff(0, channelId, data, len);
}

#endif

#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
static inline uint16_t tud_audio_int_ctr_available(void)
{
  return tud_audio_int_ctr_n_available(0);
}

static inline uint16_t tud_audio_int_ctr_read(void* buffer, uint16_t bufsize)
{
  return tud_audio_int_ctr_n_read(0, buffer, bufsize);
}

static inline void tud_audio_int_ctr_clear(void)
{
  return tud_audio_int_ctr_n_clear(0);
}

static inline uint16_t tud_audio_int_ctr_write(uint8_t const* buffer, uint16_t len)
{
  return tud_audio_int_ctr_n_write(0, buffer, len);
}
#endif

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void audiod_init              (void);
void audiod_reset             (uint8_t rhport);
uint16_t audiod_open          (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool audiod_control_xfer_cb   (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool audiod_xfer_cb           (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_AUDIO_DEVICE_H_ */

/** @} */
/** @} */
