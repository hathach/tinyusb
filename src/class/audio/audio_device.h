/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber
 * Copyright (c) 2023 HiFiPhile
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

#ifndef TUSB_AUDIO_DEVICE_H_
#define TUSB_AUDIO_DEVICE_H_

#include "audio.h"

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

// All sizes are in bytes!

#ifndef CFG_TUD_AUDIO_FUNC_1_DESC_LEN
#error You must tell the driver the length of the audio function descriptor including IAD descriptor
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_DESC_LEN
#error You must tell the driver the length of the audio function descriptor including IAD descriptor
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_DESC_LEN
#error You must tell the driver the length of the audio function descriptor including IAD descriptor
#endif
#endif

// Size of control buffer used to receive and send control messages via EP0 - has to be big enough to hold your biggest request structure e.g. range requests with multiple intervals defined or cluster descriptors
#ifndef CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ
#error You must define an audio class control request buffer size!
#endif

#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_CTRL_BUF_SZ
#error You must define an audio class control request buffer size!
#endif
#endif

#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_CTRL_BUF_SZ
#error You must define an audio class control request buffer size!
#endif
#endif

// End point sizes IN BYTES - Limits: Full Speed <= 1023, High Speed <= 1024
#ifndef CFG_TUD_AUDIO_ENABLE_EP_IN
#define CFG_TUD_AUDIO_ENABLE_EP_IN 0   // TX
#endif

#ifndef CFG_TUD_AUDIO_ENABLE_EP_OUT
#define CFG_TUD_AUDIO_ENABLE_EP_OUT 0  // RX
#endif

// Maximum EP sizes for all alternate AS interface settings - used for checks and buffer allocation
#if CFG_TUD_AUDIO_ENABLE_EP_IN
#ifndef CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX
#error You must tell the driver the biggest EP IN size!
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ_MAX
#error You must tell the driver the biggest EP IN size!
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_EP_IN_SZ_MAX
#error You must tell the driver the biggest EP IN size!
#endif
#endif
#endif // CFG_TUD_AUDIO_ENABLE_EP_IN

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
#ifndef CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX
#error You must tell the driver the biggest EP OUT size!
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX
#error You must tell the driver the biggest EP OUT size!
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX
#error You must tell the driver the biggest EP OUT size!
#endif
#endif
#endif // CFG_TUD_AUDIO_ENABLE_EP_OUT

// Software EP FIFO buffer sizes - must be >= max EP SIZEs!
#ifndef CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ                0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ                0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ                0
#endif

#ifndef CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ               0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ               0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ               0
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN
#if CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif

#if CFG_TUD_AUDIO > 1
#if CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif
#endif

#if CFG_TUD_AUDIO > 2
#if CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_3_EP_IN_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif
#endif
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
#if CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif

#if CFG_TUD_AUDIO > 1
#if CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif
#endif

#if CFG_TUD_AUDIO > 2
#if CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif
#endif
#endif

// (For TYPE-I format only) Flow control is necessary to allow IN ep send correct amount of data, unless it's a virtual device where data is perfectly synchronized to USB clock.
#ifndef CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL  1
#endif

// Enable/disable feedback EP (required for asynchronous RX applications)
#ifndef CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                    0                             // Feedback - 0 or 1
#endif

// Enable/disable conversion from 16.16 to 10.14 format on full-speed devices. See tud_audio_n_fb_set().
// Can be override by tud_audio_feedback_format_correction_cb()
#ifndef CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION     0                             // 0 or 1
#endif

// Enable/disable interrupt EP (required for notifying host of control changes)
#ifndef CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
#define CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP                   0                             // Feedback - 0 or 1
#endif

// Audio control interrupt EP - 6 Bytes according to UAC 2 specification (p. 74)
#define CFG_TUD_AUDIO_INTERRUPT_EP_SZ                       6

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
bool tud_audio_n_mounted(uint8_t func_id);

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
uint16_t   tud_audio_n_available       (uint8_t func_id);
uint16_t   tud_audio_n_read            (uint8_t func_id, void* buffer, uint16_t bufsize);
bool       tud_audio_n_clear_ep_out_ff (uint8_t func_id);
tu_fifo_t* tud_audio_n_get_ep_out_ff   (uint8_t func_id);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN
uint16_t   tud_audio_n_write          (uint8_t func_id, const void * data, uint16_t len);
bool       tud_audio_n_clear_ep_in_ff (uint8_t func_id);
tu_fifo_t* tud_audio_n_get_ep_in_ff   (uint8_t func_id);
#endif

#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
bool    tud_audio_int_n_write                     (uint8_t func_id, const audio_interrupt_data_t * data);
#endif

//--------------------------------------------------------------------+
// Application API (Interface0)
//--------------------------------------------------------------------+
static inline bool         tud_audio_mounted                (void);

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
static inline uint16_t   tud_audio_available       (void);
static inline bool       tud_audio_clear_ep_out_ff (void);
static inline uint16_t   tud_audio_read            (void* buffer, uint16_t bufsize);
static inline tu_fifo_t* tud_audio_get_ep_out_ff   (void);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN
static inline uint16_t   tud_audio_write          (const void * data, uint16_t len);
static inline bool       tud_audio_clear_ep_in_ff (void);
static inline tu_fifo_t* tud_audio_get_ep_in_ff   (void);
#endif

// INT CTR API

#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
static inline bool tud_audio_int_write                      (const audio_interrupt_data_t * data);
#endif

// Buffer control EP data and schedule a transmit
// This function is intended to be used if you do not have a persistent buffer or memory location available (e.g. non-local variables) and need to answer onto a
// get request. This function buffers your answer request frame into the control buffer of the corresponding audio driver and schedules a transmit for sending it.
// Since transmission is triggered via interrupts, a persistent memory location is required onto which the buffer pointer in pointing. If you already have such
// available you may directly use 'tud_control_xfer(...)'. In this case data does not need to be copied into an additional buffer and you save some time.
// If the request's wLength is zero, a status packet is sent instead.
bool tud_audio_buffer_and_schedule_control_xfer(uint8_t rhport, tusb_control_request_t const * p_request, void* data, uint16_t len);

//--------------------------------------------------------------------+
// Application Callback API
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_ENABLE_EP_IN
// Invoked in ISR context once an audio packet was sent successfully.
// Normally this function is not needed, since the data transfer should be driven by audio clock (i.e. I2S clock), call tud_audio_write() in I2S receive callback.
bool tud_audio_tx_done_isr(uint8_t rhport, uint16_t n_bytes_sent, uint8_t func_id, uint8_t ep_in, uint8_t cur_alt_setting);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
// Invoked in ISR context once an audio packet was received successfully.
// Normally this function is not needed, since the data transfer should be driven by audio clock (i.e. I2S clock), call tud_audio_read() in I2S transmit callback.
bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP

// Note about feedback calculation
//
// Option 1 - AUDIO_FEEDBACK_METHOD_FIFO_COUNT
// Feedback value is calculated within the audio driver by regulating the FIFO level to half fill.
// Advantage: No ISR interrupt is enabled, hence the CPU need not to handle an ISR every 1ms or 125us and thus less CPU load, well tested
// (Windows, Linux, OSX) with a reliable result so far.
// Disadvantage: A FIFO of minimal 4 frames is needed to compensate for jitter, an average delay of 2 frames is introduced.
//
// Option 2 - AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED / AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT
// Feedback value is calculated within the audio driver by use of SOF interrupt. The driver needs information about the master clock f_m from
// which the audio sample frequency f_s is derived, f_s itself, and the cycle count of f_m at time of the SOF interrupt (e.g. by use of a hardware counter).
// See tud_audio_set_fb_params() and tud_audio_feedback_update()
// Advantage: Reduced jitter in the feedback value computation, hence, the receive FIFO can be smaller and thus a smaller delay is possible.
// Disadvantage: higher CPU load due to SOF ISR handling every frame i.e. 1ms or 125us. (The most critical point is the reading of the cycle counter value of f_m.
// It is read from within the SOF ISR - see: audiod_sof() -, hence, the ISR must has a high priority such that no software dependent "random" delay i.e. jitter is introduced).
// Long-term drift could occur since error is accumulated.
//
// Option 3 - manual
// Determined by the user itself and set by use of tud_audio_n_fb_set(). The feedback value may be determined e.g. from some fill status of some FIFO buffer.
// Advantage: No ISR interrupt is enabled, hence the CPU need not to handle an ISR every 1ms or 125us and thus less CPU load.
// Disadvantage: typically a larger FIFO is needed to compensate for jitter (e.g. 6 frames), i.e. a larger delay is introduced.


// This function is used to provide data rate feedback from an asynchronous sink. Feedback value will be sent at FB endpoint interval till it's changed.
//
// The feedback format is specified to be 16.16 for HS and 10.14 for FS devices (see Universal Serial Bus Specification Revision 2.0 5.12.4.2). By default,
// the choice of format is left to the caller and feedback argument is sent as-is. If CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION is set or tud_audio_feedback_format_correction_cb()
// return true, then tinyusb expects 16.16 format and handles the conversion to 10.14 on FS.
//
// Note that due to a bug in its USB Audio 2.0 driver, Windows currently requires 16.16 format for _all_ USB 2.0 devices. On Linux and it seems the
// driver can work with either format.
//
// Feedback value can be determined from within the SOF ISR of the audio driver. This should reduce jitter. If the feature is used, the user can not set the feedback value.
//
// Determine feedback value - The feedback method is described in 5.12.4.2 of the USB 2.0 spec
// Boiled down, the feedback value Ff = n_samples / (micro)frame.
// Since an accuracy of less than 1 Sample / second is desired, at least n_frames = ceil(2^K * f_s / f_m) frames need to be measured, where K = 10 for full speed and K = 13
// for high speed, f_s is the sampling frequency e.g. 48 kHz and f_m is the cpu clock frequency e.g. 100 MHz (or any other master clock whose clock count is available and locked to f_s)
// The update interval in the (4.10.2.1) Feedback Endpoint Descriptor must be less or equal to 2^(K - P), where P = min( ceil(log2(f_m / f_s)), K)
// feedback = n_cycles / n_frames * f_s / f_m in 16.16 format, where n_cycles are the number of main clock cycles within fb_n_frames
bool tud_audio_n_fb_set(uint8_t func_id, uint32_t feedback);

// Update feedback value with passed MCLK cycles since last time this update function is called.
// Typically called within tud_audio_sof_isr(). Required tud_audio_feedback_params_cb() is implemented
// This function will also call tud_audio_feedback_set()
// return feedback value in 16.16 for reference (0 for error)
// Example :
//   binterval=3 (4ms); FS = 48kHz; MCLK = 12.288MHz
//   In 4 SOF MCLK counted 49152 cycles
uint32_t tud_audio_feedback_update(uint8_t func_id, uint32_t cycles);

enum {
  AUDIO_FEEDBACK_METHOD_DISABLED,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2, // For driver internal use only
  AUDIO_FEEDBACK_METHOD_FIFO_COUNT
};

typedef struct {
  uint8_t method;
  uint32_t sample_freq;   //  sample frequency in Hz

  union {
    struct {
      uint32_t mclk_freq; // Main clock frequency in Hz i.e. master clock to which sample clock is based on
    }frequency;

  };
}audio_feedback_params_t;

// Invoked when needed to set feedback parameters
void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t* feedback_param);

// Callback in ISR context, invoked periodically according to feedback endpoint bInterval.
// Could be used to compute and update feedback value, should be placed in RAM if possible
// frame_number  : current SOF count
// interval_shift: number of bit shift i.e log2(interval) from Feedback endpoint descriptor
TU_ATTR_FAST_FUNC void tud_audio_feedback_interval_isr(uint8_t func_id, uint32_t frame_number, uint8_t interval_shift);

// (Full-Speed only) Callback to set feedback format correction is applied or not,
// default to CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION if not implemented.
bool tud_audio_feedback_format_correction_cb(uint8_t func_id);
#endif // CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP

#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
void tud_audio_int_done_cb(uint8_t rhport);
#endif

// Invoked when audio set interface request received
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio set interface request received which closes an EP
bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// backward compatible for typo
#define tud_audio_set_itf_close_EP_cb   tud_audio_set_itf_close_ep_cb

// Invoked when audio class specific set request received for an EP
bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific set request received for an interface
bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific get request received for an EP
bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio class specific get request received for an interface
bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline bool tud_audio_mounted(void) {
  return tud_audio_n_mounted(0);
}

#if CFG_TUD_AUDIO_ENABLE_EP_OUT

TU_ATTR_ALWAYS_INLINE static inline uint16_t tud_audio_available(void) {
  return tud_audio_n_available(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint16_t tud_audio_read(void* buffer, uint16_t bufsize) {
  return tud_audio_n_read(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_audio_clear_ep_out_ff(void) {
  return tud_audio_n_clear_ep_out_ff(0);
}

TU_ATTR_ALWAYS_INLINE static inline tu_fifo_t* tud_audio_get_ep_out_ff(void) {
  return tud_audio_n_get_ep_out_ff(0);
}

#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN

TU_ATTR_ALWAYS_INLINE static inline uint16_t tud_audio_write(const void * data, uint16_t len) {
  return tud_audio_n_write(0, data, len);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_audio_clear_ep_in_ff(void) {
  return tud_audio_n_clear_ep_in_ff(0);
}

TU_ATTR_ALWAYS_INLINE static inline tu_fifo_t* tud_audio_get_ep_in_ff(void) {
  return tud_audio_n_get_ep_in_ff(0);
}

#endif

#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
TU_ATTR_ALWAYS_INLINE static inline bool tud_audio_int_write(const audio_interrupt_data_t * data) {
  return tud_audio_int_n_write(0, data);
}
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
TU_ATTR_ALWAYS_INLINE static inline bool tud_audio_fb_set(uint32_t feedback) {
  return tud_audio_n_fb_set(0, feedback);
}
#endif

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     audiod_init           (void);
bool     audiod_deinit         (void);
void     audiod_reset          (uint8_t rhport);
uint16_t audiod_open           (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     audiod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool     audiod_xfer_cb        (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);
bool     audiod_xfer_isr       (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);
void     audiod_sof_isr        (uint8_t rhport, uint32_t frame_count);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_AUDIO_DEVICE_H_ */

/** @} */
/** @} */
