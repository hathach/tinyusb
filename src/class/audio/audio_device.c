/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Reinhard Panhuber, Jerzy Kasenberg
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

/*
 * This driver supports at most one out EP, one in EP, one control EP, and one feedback EP and one alternative interface other than zero. Hence, only one input terminal and one output terminal are support, if you need more adjust the driver!
 * It supports multiple TX and RX channels.
 *
 * In case you need more alternate interfaces, you need to define additional defines for this specific alternate interface. Just define them and set them in the set_interface function.
 *
 * There are three data flow structures currently implemented, where at least one SW-FIFO is used to decouple the asynchronous processes MCU vs. host
 *
 * 1. Input data -> SW-FIFO -> MCU USB
 *
 * The most easiest version, available in case the target MCU can handle the software FIFO (SW-FIFO) and if it is implemented in the device driver (if yes then dcd_edpt_xfer_fifo() is available)
 *
 * 2. Input data -> SW-FIFO -> Linear buffer -> MCU USB
 *
 * In case the target MCU can not handle a SW-FIFO, a linear buffer is used. This uses the default function dcd_edpt_xfer(). In this case more memory is required.
 *
 * 3. (Input data 1 | Input data 2 | ... | Input data N) ->  (SW-FIFO 1 | SW-FIFO 2 | ... | SW-FIFO N) -> Linear buffer -> MCU USB
 *
 * This case is used if you have more channels which need to be combined into one stream. Every channel has its own SW-FIFO. All data is encoded into an Linear buffer.
 *
 * The same holds in the RX case.
 *
 * */

#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_AUDIO)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "audio_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Use ring buffer if it's available, some MCUs need extra RAM requirements
// For DWC2 enable ring buffer will disable DMA (if available)
#ifndef TUD_AUDIO_PREFER_RING_BUFFER
  #if CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_MIMXRT1XXX || \
      defined(TUP_USBIP_DWC2)
    #define TUD_AUDIO_PREFER_RING_BUFFER 0
  #else
    #define TUD_AUDIO_PREFER_RING_BUFFER 1
  #endif
#endif

// Linear buffer in case target MCU is not capable of handling a ring buffer FIFO e.g. no hardware buffer
// is available or driver is would need to be changed dramatically

// Only STM32 and dcd_transdimension use non-linear buffer for now
// dwc2 except esp32sx (since it may use dcd_esp32sx)
// Ring buffer is incompatible with dcache, since neither address nor size is aligned to cache line
#if (defined(TUP_USBIP_DWC2) && !TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)) || \
    defined(TUP_USBIP_FSDEV) ||                                                     \
    CFG_TUSB_MCU == OPT_MCU_RX63X ||                                                \
    CFG_TUSB_MCU == OPT_MCU_RX65X ||                                                \
    CFG_TUSB_MCU == OPT_MCU_RX72N ||                                                \
    CFG_TUSB_MCU == OPT_MCU_LPC18XX ||                                              \
    CFG_TUSB_MCU == OPT_MCU_LPC43XX ||                                              \
    CFG_TUSB_MCU == OPT_MCU_MIMXRT1XXX ||                                           \
    CFG_TUSB_MCU == OPT_MCU_MSP432E4
  #if TUD_AUDIO_PREFER_RING_BUFFER && !CFG_TUD_MEM_DCACHE_ENABLE
    #define USE_LINEAR_BUFFER 0
  #else
    #define USE_LINEAR_BUFFER 1
  #endif
#else
  #define USE_LINEAR_BUFFER 1
#endif

// Declaration of buffers

// Check for maximum supported numbers
#if CFG_TUD_AUDIO > 3
  #error Maximum number of audio functions restricted to three!
#endif

// Put swap buffer in USB section only if necessary
#if USE_LINEAR_BUFFER
  #define IN_SW_BUF_MEM_ATTR TU_ATTR_ALIGNED(4)
#else
  #define IN_SW_BUF_MEM_ATTR CFG_TUD_MEM_SECTION CFG_TUD_MEM_ALIGN
#endif
#if USE_LINEAR_BUFFER
  #define OUT_SW_BUF_MEM_ATTR TU_ATTR_ALIGNED(4)
#else
  #define OUT_SW_BUF_MEM_ATTR CFG_TUD_MEM_SECTION CFG_TUD_MEM_ALIGN
#endif

// EP IN software buffers
#if CFG_TUD_AUDIO_ENABLE_EP_IN
tu_static IN_SW_BUF_MEM_ATTR struct {
  #if CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ > 0
  TUD_EPBUF_DEF(buf_1, CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ);
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ > 0
  TUD_EPBUF_DEF(buf_2, CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ);
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ > 0
  TUD_EPBUF_DEF(buf_3, CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ);
  #endif
} ep_in_sw_buf;
#endif// CFG_TUD_AUDIO_ENABLE_EP_IN

// Linear buffer TX in case:
// - target MCU is not capable of handling a ring buffer FIFO e.g. no hardware buffer is available or driver is would need to be changed dramatically OR
#if CFG_TUD_AUDIO_ENABLE_EP_IN && USE_LINEAR_BUFFER
tu_static CFG_TUD_MEM_SECTION struct {
  #if CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX > 0
  TUD_EPBUF_DEF(buf_1, CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX);
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ_MAX > 0
  TUD_EPBUF_DEF(buf_2, CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ_MAX);
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_IN_SZ_MAX > 0
  TUD_EPBUF_DEF(buf_3, CFG_TUD_AUDIO_FUNC_3_EP_IN_SZ_MAX);
  #endif
} lin_buf_in;
#endif// CFG_TUD_AUDIO_ENABLE_EP_IN && USE_LINEAR_BUFFER

// EP OUT software buffers
#if CFG_TUD_AUDIO_ENABLE_EP_OUT
tu_static OUT_SW_BUF_MEM_ATTR struct {
  #if CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ > 0
  TUD_EPBUF_DEF(buf_1, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ);
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ > 0
  TUD_EPBUF_DEF(buf_2, CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ);
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ > 0
  TUD_EPBUF_DEF(buf_3, CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ);
  #endif
} ep_out_sw_buf;
#endif// CFG_TUD_AUDIO_ENABLE_EP_OUT

// Linear buffer RX in case:
// - target MCU is not capable of handling a ring buffer FIFO e.g. no hardware buffer is available or driver is would need to be changed dramatically OR
#if CFG_TUD_AUDIO_ENABLE_EP_OUT && USE_LINEAR_BUFFER
tu_static CFG_TUD_MEM_SECTION struct {
  #if CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX > 0
  TUD_EPBUF_DEF(buf_1, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX);
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX > 0
  TUD_EPBUF_DEF(buf_2, CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX);
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX > 0
  TUD_EPBUF_DEF(buf_3, CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX);
  #endif
} lin_buf_out;
#endif// CFG_TUD_AUDIO_ENABLE_EP_OUT && USE_LINEAR_BUFFER

// Control buffers
tu_static CFG_TUD_MEM_SECTION struct {
  TUD_EPBUF_DEF(buf1, CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ);
  #if CFG_TUD_AUDIO > 1
  TUD_EPBUF_DEF(buf2, CFG_TUD_AUDIO_FUNC_2_CTRL_BUF_SZ);
  #endif
  #if CFG_TUD_AUDIO > 2
  TUD_EPBUF_DEF(buf3, CFG_TUD_AUDIO_FUNC_3_CTRL_BUF_SZ);
  #endif
} ctrl_buf;

// Aligned buffer for feedback EP
#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
tu_static CFG_TUD_MEM_SECTION struct {
  #if CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX > 0
  TUD_EPBUF_TYPE_DEF(uint32_t, buf_1);
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX > 0
  TUD_EPBUF_TYPE_DEF(uint32_t, buf_2);
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX > 0
  TUD_EPBUF_TYPE_DEF(uint32_t, buf_3);
  #endif
} fb_ep_buf;
#endif

// Aligned buffer for interrupt EP
#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
tu_static CFG_TUD_MEM_SECTION struct {
  TUD_EPBUF_DEF(buf, CFG_TUD_AUDIO_INTERRUPT_EP_SZ);
} int_ep_buf[CFG_TUD_AUDIO];
#endif

typedef struct
{
  uint8_t rhport;
  uint8_t const *p_desc;// Pointer pointing to Standard AC Interface Descriptor(4.7.1) - Audio Control descriptor defining audio function

#if CFG_TUD_AUDIO_ENABLE_EP_IN
  uint8_t ep_in;            // TX audio data EP.
  uint16_t ep_in_sz;        // Current size of TX EP
  uint8_t ep_in_as_intf_num;// Corresponding Standard AS Interface Descriptor (4.9.1) belonging to output terminal to which this EP belongs - 0 is invalid (this fits to UAC2 specification since AS interfaces can not have interface number equal to zero)
  uint8_t ep_in_alt;        // Current alternate setting of TX EP
  #endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
  uint8_t ep_out;            // Incoming (into uC) audio data EP.
  uint16_t ep_out_sz;        // Current size of RX EP
  uint8_t ep_out_as_intf_num;// Corresponding Standard AS Interface Descriptor (4.9.1) belonging to input terminal to which this EP belongs - 0 is invalid (this fits to UAC2 specification since AS interfaces can not have interface number equal to zero)
  uint8_t ep_out_alt;        // Current alternate setting of RX EP
  #if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  uint8_t ep_fb;// Feedback EP.
  #endif

#endif

#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
  uint8_t ep_int;// Audio control interrupt EP.
#endif

  bool mounted;// Device opened

  uint16_t desc_length;// Length of audio function descriptor

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  struct {
    uint32_t value;    // Feedback value for asynchronous mode (in 16.16 format).
    uint32_t min_value;// min value according to UAC2 FMT-2.0 section 2.3.1.1.
    uint32_t max_value;// max value according to UAC2 FMT-2.0 section 2.3.1.1.

    uint8_t frame_shift;// bInterval-1 in unit of frame (FS), micro-frame (HS)
    uint8_t compute_method;
    bool format_correction;
    union {
      uint8_t power_of_2;// pre-computed power of 2 shift
      float float_const; // pre-computed float constant

      struct {
        uint32_t sample_freq;
        uint32_t mclk_freq;
      } fixed;

      struct {
        uint32_t nom_value;    // In 16.16 format
        uint32_t fifo_lvl_avg; // In 16.16 format
        uint16_t fifo_lvl_thr; // fifo level threshold
        uint16_t rate_const[2];// pre-computed feedback/fifo_depth rate
      } fifo_count;
    } compute;

  } feedback;
#endif// CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
  uint32_t sample_rate_tx;
  uint16_t packet_sz_tx[3];
  uint8_t bclock_id_tx;
  uint8_t interval_tx;
#endif

// Encoding parameters - parameters are set when alternate AS interface is set by host
#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
  audio_format_type_t format_type_tx;
  uint8_t n_channels_tx;
  uint8_t n_bytes_per_sample_tx;
#endif

  /*------------- From this point, data is not cleared by bus reset -------------*/

  // Buffer for control requests
  uint8_t *ctrl_buf;
  uint8_t ctrl_buf_sz;

// EP Transfer buffers and FIFOs
#if CFG_TUD_AUDIO_ENABLE_EP_OUT
  tu_fifo_t ep_out_ff;
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN
  tu_fifo_t ep_in_ff;
#endif

// Linear buffer in case target MCU is not capable of handling a ring buffer FIFO e.g. no hardware buffer is available or driver is would need to be changed dramatically
#if CFG_TUD_AUDIO_ENABLE_EP_OUT && USE_LINEAR_BUFFER
  uint8_t *lin_buf_out;
  #define USE_LINEAR_BUFFER_RX 1
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN && USE_LINEAR_BUFFER
  uint8_t *lin_buf_in;
  #define USE_LINEAR_BUFFER_TX 1
#endif

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  uint32_t *fb_buf;
#endif
} audiod_function_t;

#ifndef USE_LINEAR_BUFFER_TX
  #define USE_LINEAR_BUFFER_TX 0
#endif

#ifndef USE_LINEAR_BUFFER_RX
  #define USE_LINEAR_BUFFER_RX 0
#endif

#define ITF_MEM_RESET_SIZE offsetof(audiod_function_t, ctrl_buf)

//--------------------------------------------------------------------+
// WEAK FUNCTION STUBS
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_ENABLE_EP_IN
TU_ATTR_WEAK bool tud_audio_tx_done_isr(uint8_t rhport, uint16_t n_bytes_sent, uint8_t func_id, uint8_t ep_in, uint8_t cur_alt_setting) {
  (void) rhport;
  (void) n_bytes_sent;
  (void) func_id;
  (void) ep_in;
  (void) cur_alt_setting;
  return true;
}

#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
TU_ATTR_WEAK bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting) {
  (void) rhport;
  (void) n_bytes_received;
  (void) func_id;
  (void) ep_out;
  (void) cur_alt_setting;
  return true;
}
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
TU_ATTR_WEAK void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t *feedback_param) {
  (void) func_id;
  (void) alt_itf;
  feedback_param->method = AUDIO_FEEDBACK_METHOD_DISABLED;
}

TU_ATTR_WEAK bool tud_audio_feedback_format_correction_cb(uint8_t func_id) {
  (void) func_id;
  return CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION;
}

TU_ATTR_WEAK TU_ATTR_FAST_FUNC void tud_audio_feedback_interval_isr(uint8_t func_id, uint32_t frame_number, uint8_t interval_shift) {
  (void) func_id;
  (void) frame_number;
  (void) interval_shift;
}
#endif

#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
TU_ATTR_WEAK void tud_audio_int_xfer_cb(uint8_t rhport) {
  (void) rhport;
}
#endif

// Invoked when audio set interface request received
TU_ATTR_WEAK bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  (void) p_request;
  return true;
}

// Invoked when audio set interface request received which closes an EP
TU_ATTR_WEAK bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  (void) p_request;
  return true;
}

// Invoked when audio class specific set request received for an EP
TU_ATTR_WEAK bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
  (void) rhport;
  (void) p_request;
  (void) pBuff;
  TU_LOG2("  No EP set request callback available!\r\n");
  return false;// In case no callback function is present or request can not be conducted we stall it
}

// Invoked when audio class specific set request received for an interface
TU_ATTR_WEAK bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
  (void) rhport;
  (void) p_request;
  (void) pBuff;
  TU_LOG2("  No interface set request callback available!\r\n");
  return false;// In case no callback function is present or request can not be conducted we stall it
}

// Invoked when audio class specific set request received for an entity
TU_ATTR_WEAK bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
  (void) rhport;
  (void) p_request;
  (void) pBuff;
  TU_LOG2("  No entity set request callback available!\r\n");
  return false;// In case no callback function is present or request can not be conducted we stall it
}

// Invoked when audio class specific get request received for an EP
TU_ATTR_WEAK bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  (void) p_request;
  TU_LOG2("  No EP get request callback available!\r\n");
  return false;// Stall
}

// Invoked when audio class specific get request received for an interface
TU_ATTR_WEAK bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  (void) p_request;
  TU_LOG2("  No interface get request callback available!\r\n");
  return false;// Stall
}

// Invoked when audio class specific get request received for an entity
TU_ATTR_WEAK bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  (void) p_request;
  TU_LOG2("  No entity get request callback available!\r\n");
  return false;// Stall
}

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
tu_static CFG_TUD_MEM_SECTION audiod_function_t _audiod_fct[CFG_TUD_AUDIO];

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
static bool audiod_rx_xfer_isr(uint8_t rhport, audiod_function_t* audio, uint16_t n_bytes_received);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN
static bool audiod_tx_xfer_isr(uint8_t rhport, audiod_function_t* audio, uint16_t n_bytes_sent);
#endif

static bool audiod_get_interface(uint8_t rhport, tusb_control_request_t const *p_request);
static bool audiod_set_interface(uint8_t rhport, tusb_control_request_t const *p_request);

static bool audiod_verify_entity_exists(uint8_t itf, uint8_t entityID, uint8_t *func_id);
static bool audiod_verify_itf_exists(uint8_t itf, uint8_t *func_id);
static bool audiod_verify_ep_exists(uint8_t ep, uint8_t *func_id);
static uint8_t audiod_get_audio_fct_idx(audiod_function_t *audio);

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
static void audiod_parse_flow_control_params(audiod_function_t *audio, uint8_t const *p_desc);
static bool audiod_calc_tx_packet_sz(audiod_function_t *audio);
static uint16_t audiod_tx_packet_size(const uint16_t *norminal_size, uint16_t data_count, uint16_t fifo_depth, uint16_t max_size);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
static bool audiod_set_fb_params_freq(audiod_function_t *audio, uint32_t sample_freq, uint32_t mclk_freq);
static void audiod_fb_fifo_count_update(audiod_function_t *audio, uint16_t lvl_new);
#endif

bool tud_audio_n_mounted(uint8_t func_id) {
  TU_VERIFY(func_id < CFG_TUD_AUDIO);
  audiod_function_t *audio = &_audiod_fct[func_id];

  return audio->mounted;
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_ENABLE_EP_OUT

uint16_t tud_audio_n_available(uint8_t func_id) {
  TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
  return tu_fifo_count(&_audiod_fct[func_id].ep_out_ff);
}

uint16_t tud_audio_n_read(uint8_t func_id, void *buffer, uint16_t bufsize) {
  TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
  return tu_fifo_read_n(&_audiod_fct[func_id].ep_out_ff, buffer, bufsize);
}

bool tud_audio_n_clear_ep_out_ff(uint8_t func_id) {
  TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
  return tu_fifo_clear(&_audiod_fct[func_id].ep_out_ff);
}

tu_fifo_t *tud_audio_n_get_ep_out_ff(uint8_t func_id) {
  if (func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL) {
    return &_audiod_fct[func_id].ep_out_ff;
  }
  return NULL;
}

static bool audiod_rx_xfer_isr(uint8_t rhport, audiod_function_t* audio, uint16_t n_bytes_received) {
  uint8_t idx_audio_fct = audiod_get_audio_fct_idx(audio);

  #if USE_LINEAR_BUFFER_RX
  // Data currently is in linear buffer, copy into EP OUT FIFO
  TU_VERIFY(tu_fifo_write_n(&audio->ep_out_ff, audio->lin_buf_out, n_bytes_received));

  // Schedule for next receive
  TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_out, audio->lin_buf_out, audio->ep_out_sz), false);
  #else
  // Data is already placed in EP FIFO, schedule for next receive
  TU_VERIFY(usbd_edpt_xfer_fifo(rhport, audio->ep_out, &audio->ep_out_ff, audio->ep_out_sz), false);
  #endif

  #if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  if (audio->feedback.compute_method == AUDIO_FEEDBACK_METHOD_FIFO_COUNT) {
    audiod_fb_fifo_count_update(audio, tu_fifo_count(&audio->ep_out_ff));
  }
  #endif

  // Call a weak callback here - a possibility for user to get informed an audio packet was received and data gets now loaded into EP FIFO
  TU_VERIFY(tud_audio_rx_done_isr(rhport, n_bytes_received, idx_audio_fct, audio->ep_out, audio->ep_out_alt));

  return true;
}

#endif//CFG_TUD_AUDIO_ENABLE_EP_OUT

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_ENABLE_EP_IN

uint16_t tud_audio_n_write(uint8_t func_id, const void *data, uint16_t len) {
  TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
  return tu_fifo_write_n(&_audiod_fct[func_id].ep_in_ff, data, len);
}

bool tud_audio_n_clear_ep_in_ff(uint8_t func_id) {
  TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
  return tu_fifo_clear(&_audiod_fct[func_id].ep_in_ff);
}

tu_fifo_t *tud_audio_n_get_ep_in_ff(uint8_t func_id) {
  if (func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL) {
    return &_audiod_fct[func_id].ep_in_ff;
  }
  return NULL;
}

static bool audiod_tx_xfer_isr(uint8_t rhport, audiod_function_t * audio, uint16_t n_bytes_sent) {
  uint8_t idx_audio_fct = audiod_get_audio_fct_idx(audio);

  // Only send something if current alternate interface is not 0 as in this case nothing is to be sent due to UAC2 specifications
  if (audio->ep_in_alt == 0) { return false; }

  // Send everything in ISO EP FIFO
  uint16_t n_bytes_tx;

  #if CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
  // packet_sz_tx is based on total packet size, here we want size for each support buffer.
  n_bytes_tx = audiod_tx_packet_size(audio->packet_sz_tx, tu_fifo_count(&audio->ep_in_ff), audio->ep_in_ff.depth, audio->ep_in_sz);
  #else
  n_bytes_tx = tu_min16(tu_fifo_count(&audio->ep_in_ff), audio->ep_in_sz);// Limit up to max packet size, more can not be done for ISO
  #endif
  #if USE_LINEAR_BUFFER_TX
  tu_fifo_read_n(&audio->ep_in_ff, audio->lin_buf_in, n_bytes_tx);
  TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_in, audio->lin_buf_in, n_bytes_tx));
  #else
  // Send everything in ISO EP FIFO
  TU_VERIFY(usbd_edpt_xfer_fifo(rhport, audio->ep_in, &audio->ep_in_ff, n_bytes_tx));
  #endif

  // Call a weak callback here - a possibility for user to get informed former TX was completed and data gets now loaded into EP in buffer
  TU_VERIFY(tud_audio_tx_done_isr(rhport, n_bytes_sent, idx_audio_fct, audio->ep_in, audio->ep_in_alt));

  return true;
}

#endif

#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
// If no interrupt transmit is pending bytes get written into buffer and a transmit is scheduled - once transmit completed tud_audio_int_done_cb() is called in inform user
bool tud_audio_int_n_write(uint8_t func_id, const audio_interrupt_data_t *data) {
  TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);

  TU_VERIFY(_audiod_fct[func_id].ep_int != 0);

  // We write directly into the EP's buffer - abort if previous transfer not complete
  TU_VERIFY(usbd_edpt_claim(_audiod_fct[func_id].rhport, _audiod_fct[func_id].ep_int));

  // Check length
  if (tu_memcpy_s(int_ep_buf[func_id].buf, sizeof(int_ep_buf[func_id].buf), data, sizeof(audio_interrupt_data_t)) == 0) {
    // Schedule transmit
    TU_ASSERT(usbd_edpt_xfer(_audiod_fct[func_id].rhport, _audiod_fct[func_id].ep_int, int_ep_buf[func_id].buf, sizeof(int_ep_buf[func_id].buf)), 0);
  } else {
    // Release endpoint since we don't make any transfer
    usbd_edpt_release(_audiod_fct[func_id].rhport, _audiod_fct[func_id].ep_int);
  }

  return true;
}
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
// This function is called once a transmit of a feedback packet was successfully completed. Here, we get the next feedback value to be sent
static inline bool audiod_fb_send(audiod_function_t *audio) {
  bool apply_correction = (TUSB_SPEED_FULL == tud_speed_get()) && audio->feedback.format_correction;
  // Format the feedback value
  if (apply_correction) {
    uint8_t *fb = (uint8_t *) audio->fb_buf;

    // For FS format is 10.14
    *(fb++) = (audio->feedback.value >> 2) & 0xFF;
    *(fb++) = (audio->feedback.value >> 10) & 0xFF;
    *(fb++) = (audio->feedback.value >> 18) & 0xFF;
    *fb = 0;
  } else {
    *audio->fb_buf = audio->feedback.value;
  }

  // About feedback format on FS
  //
  // 3 variables: Format | packetSize | sendSize | Working OS:
  //              16.16    4            4          Linux, Windows
  //              16.16    4            3          Linux
  //              16.16    3            4          Linux
  //              16.16    3            3          Linux
  //              10.14    4            4          Linux
  //              10.14    4            3          Linux
  //              10.14    3            4          Linux, OSX
  //              10.14    3            3          Linux, OSX
  //
  // We send 3 bytes since sending packet larger than wMaxPacketSize is pretty ugly
  return usbd_edpt_xfer(audio->rhport, audio->ep_fb, (uint8_t *) audio->fb_buf, apply_correction ? 3 : 4);
}
#endif

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void audiod_init(void) {
  tu_memclr(_audiod_fct, sizeof(_audiod_fct));

  for (uint8_t i = 0; i < CFG_TUD_AUDIO; i++) {
    audiod_function_t *audio = &_audiod_fct[i];

    // Initialize control buffers
    switch (i) {
      case 0:
        audio->ctrl_buf = ctrl_buf.buf1;
        audio->ctrl_buf_sz = CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ;
        break;
#if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_CTRL_BUF_SZ > 0
      case 1:
        audio->ctrl_buf = ctrl_buf.buf2;
        audio->ctrl_buf_sz = CFG_TUD_AUDIO_FUNC_2_CTRL_BUF_SZ;
        break;
#endif
#if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_CTRL_BUF_SZ > 0
      case 2:
        audio->ctrl_buf = ctrl_buf.buf3;
        audio->ctrl_buf_sz = CFG_TUD_AUDIO_FUNC_3_CTRL_BUF_SZ;
        break;
#endif
    }

      // Initialize IN EP FIFO if required
#if CFG_TUD_AUDIO_ENABLE_EP_IN

    switch (i) {
  #if CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ > 0
      case 0:
        tu_fifo_config(&audio->ep_in_ff, ep_in_sw_buf.buf_1, CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ, 1, true);
        break;
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ > 0
      case 1:
        tu_fifo_config(&audio->ep_in_ff, ep_in_sw_buf.buf_2, CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ, 1, true);
        break;
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ > 0
      case 2:
        tu_fifo_config(&audio->ep_in_ff, ep_in_sw_buf.buf_3, CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ, 1, true);
        break;
  #endif
    }
#endif// CFG_TUD_AUDIO_ENABLE_EP_IN

      // Initialize linear buffers
#if USE_LINEAR_BUFFER_TX
    switch (i) {
  #if CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX > 0
      case 0:
        audio->lin_buf_in = lin_buf_in.buf_1;
        break;
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ_MAX > 0
      case 1:
        audio->lin_buf_in = lin_buf_in.buf_2;
        break;
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_IN_SZ_MAX > 0
      case 2:
        audio->lin_buf_in = lin_buf_in.buf_3;
        break;
  #endif
    }
#endif// USE_LINEAR_BUFFER_TX

      // Initialize OUT EP FIFO if required
#if CFG_TUD_AUDIO_ENABLE_EP_OUT

    switch (i) {
  #if CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ > 0
      case 0:
        tu_fifo_config(&audio->ep_out_ff, ep_out_sw_buf.buf_1, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ, 1, true);
        break;
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ > 0
      case 1:
        tu_fifo_config(&audio->ep_out_ff, ep_out_sw_buf.buf_2, CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ, 1, true);
        break;
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ > 0
      case 2:
        tu_fifo_config(&audio->ep_out_ff, ep_out_sw_buf.buf_3, CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ, 1, true);
        break;
  #endif
    }
#endif// CFG_TUD_AUDIO_ENABLE_EP_OUT

      // Initialize linear buffers
#if USE_LINEAR_BUFFER_RX
    switch (i) {
  #if CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX > 0
      case 0:
        audio->lin_buf_out = lin_buf_out.buf_1;
        break;
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX > 0
      case 1:
        audio->lin_buf_out = lin_buf_out.buf_2;
        break;
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX > 0
      case 2:
        audio->lin_buf_out = lin_buf_out.buf_3;
        break;
  #endif
    }
#endif// USE_LINEAR_BUFFER_RX

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
    switch (i) {
  #if CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX > 0
      case 0:
        audio->fb_buf = &fb_ep_buf.buf_1;
        break;
  #endif
  #if CFG_TUD_AUDIO > 1 && CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX > 0
      case 1:
        audio->fb_buf = &fb_ep_buf.buf_2;
        break;
  #endif
  #if CFG_TUD_AUDIO > 2 && CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX > 0
      case 2:
        audio->fb_buf = &fb_ep_buf.buf_3;
        break;
  #endif
    }
#endif// CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  }
}

bool audiod_deinit(void) {
  return false;// TODO not implemented yet
}

void audiod_reset(uint8_t rhport) {
  (void) rhport;

  for (uint8_t i = 0; i < CFG_TUD_AUDIO; i++) {
    audiod_function_t *audio = &_audiod_fct[i];
    tu_memclr(audio, ITF_MEM_RESET_SIZE);

#if CFG_TUD_AUDIO_ENABLE_EP_IN
    tu_fifo_clear(&audio->ep_in_ff);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
    tu_fifo_clear(&audio->ep_out_ff);
#endif
  }
}

uint16_t audiod_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  (void) max_len;

  TU_VERIFY(TUSB_CLASS_AUDIO == itf_desc->bInterfaceClass &&
            AUDIO_SUBCLASS_CONTROL == itf_desc->bInterfaceSubClass);

  // Verify version is correct - this check can be omitted
  TU_VERIFY(itf_desc->bInterfaceProtocol == AUDIO_INT_PROTOCOL_CODE_V2);

  // Verify interrupt control EP is enabled if demanded by descriptor
  TU_ASSERT(itf_desc->bNumEndpoints <= 1);// 0 or 1 EPs are allowed
  if (itf_desc->bNumEndpoints == 1) {
    TU_ASSERT(CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP);
  }

  // Alternate setting MUST be zero - this check can be omitted
  TU_VERIFY(itf_desc->bAlternateSetting == 0);

  // Find available audio driver interface
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++) {
    if (!_audiod_fct[i].p_desc) {
      _audiod_fct[i].p_desc = (uint8_t const *) itf_desc;// Save pointer to AC descriptor which is by specification always the first one
      _audiod_fct[i].rhport = rhport;

      // Setup descriptor lengths
      switch (i) {
        case 0:
          _audiod_fct[i].desc_length = CFG_TUD_AUDIO_FUNC_1_DESC_LEN;
          break;
#if CFG_TUD_AUDIO > 1
        case 1:
          _audiod_fct[i].desc_length = CFG_TUD_AUDIO_FUNC_2_DESC_LEN;
          break;
#endif
#if CFG_TUD_AUDIO > 2
        case 2:
          _audiod_fct[i].desc_length = CFG_TUD_AUDIO_FUNC_3_DESC_LEN;
          break;
#endif
      }

#ifdef TUP_DCD_EDPT_ISO_ALLOC
      {
  #if CFG_TUD_AUDIO_ENABLE_EP_IN
        uint8_t ep_in = 0;
        uint16_t ep_in_size = 0;
  #endif

  #if CFG_TUD_AUDIO_ENABLE_EP_OUT
        uint8_t ep_out = 0;
        uint16_t ep_out_size = 0;
  #endif

  #if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
        uint8_t ep_fb = 0;
  #endif
        uint8_t const *p_desc = _audiod_fct[i].p_desc;
        uint8_t const *p_desc_end = p_desc + _audiod_fct[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12 strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) p_desc;
            if (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) {
  #if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
              // Explicit feedback EP
              if (desc_ep->bmAttributes.usage == 1) {
                ep_fb = desc_ep->bEndpointAddress;
              }
  #endif
  #if CFG_TUD_AUDIO_ENABLE_EP_IN
              // Data or data with implicit feedback IN EP
              if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN
                  && (desc_ep->bmAttributes.usage == 0 || desc_ep->bmAttributes.usage == 2)) {
                ep_in = desc_ep->bEndpointAddress;
                ep_in_size = TU_MAX(tu_edpt_packet_size(desc_ep), ep_in_size);
              }
  #endif
  #if CFG_TUD_AUDIO_ENABLE_EP_OUT
              // Data OUT EP
              if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_OUT
                  && desc_ep->bmAttributes.usage == 0) {
                ep_out = desc_ep->bEndpointAddress;
                ep_out_size = TU_MAX(tu_edpt_packet_size(desc_ep), ep_out_size);
              }
  #endif
            }
          }

          p_desc = tu_desc_next(p_desc);
        }

  #if CFG_TUD_AUDIO_ENABLE_EP_IN
        if (ep_in) {
          usbd_edpt_iso_alloc(rhport, ep_in, ep_in_size);
        }
  #endif

  #if CFG_TUD_AUDIO_ENABLE_EP_OUT
        if (ep_out) {
          usbd_edpt_iso_alloc(rhport, ep_out, ep_out_size);
        }
  #endif

  #if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
        if (ep_fb) {
          usbd_edpt_iso_alloc(rhport, ep_fb, 4);
        }
  #endif
      }
#endif// TUP_DCD_EDPT_ISO_ALLOC

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
      {
        uint8_t const *p_desc = _audiod_fct[i].p_desc;
        uint8_t const *p_desc_end = p_desc + _audiod_fct[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12 strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) p_desc;
            if (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) {
              // For data or data with implicit feedback IN EP
              if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN
                  && (desc_ep->bmAttributes.usage == 0 || desc_ep->bmAttributes.usage == 2)) {
                _audiod_fct[i].interval_tx = desc_ep->bInterval;
              }
            }
          } else if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE && tu_desc_subtype(p_desc) == AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL) {
            if (tu_unaligned_read16(p_desc + 4) == AUDIO_TERM_TYPE_USB_STREAMING) {
              _audiod_fct[i].bclock_id_tx = p_desc[8];
            }
          }
          p_desc = tu_desc_next(p_desc);
        }
      }
#endif// CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL

#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
      {
        uint8_t const *p_desc = _audiod_fct[i].p_desc;
        uint8_t const *p_desc_end = p_desc + _audiod_fct[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12 strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          // For each endpoint
          if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) p_desc;
            uint8_t const ep_addr = desc_ep->bEndpointAddress;
            // If endpoint is input-direction and interrupt-type
            if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN && desc_ep->bmAttributes.xfer == TUSB_XFER_INTERRUPT) {
              // Store endpoint number and open endpoint
              _audiod_fct[i].ep_int = ep_addr;
              TU_ASSERT(usbd_edpt_open(_audiod_fct[i].rhport, desc_ep));
            }
          }
          p_desc = tu_desc_next(p_desc);
        }
      }
#endif

      _audiod_fct[i].mounted = true;
      break;
    }
  }

  // Verify we found a free one
  TU_ASSERT(i < CFG_TUD_AUDIO);

  // This is all we need so far - the EPs are setup by a later set_interface request (as per UAC2 specification)
  uint16_t drv_len = _audiod_fct[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;// - TUD_AUDIO_DESC_IAD_LEN since tinyUSB already handles the IAD descriptor

  return drv_len;
}

static bool audiod_get_interface(uint8_t rhport, tusb_control_request_t const *p_request) {
  uint8_t const itf = tu_u16_low(p_request->wIndex);

  // Find index of audio streaming interface
  uint8_t func_id;
  TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

  // Default to 0 if interface not yet activated
  uint8_t alt = 0;
#if CFG_TUD_AUDIO_ENABLE_EP_IN
  if (_audiod_fct[func_id].ep_in_as_intf_num == itf) {
    alt = _audiod_fct[func_id].ep_in_alt;
  }
#endif
#if CFG_TUD_AUDIO_ENABLE_EP_OUT
  if (_audiod_fct[func_id].ep_out_as_intf_num == itf) {
    alt = _audiod_fct[func_id].ep_out_alt;
  }
#endif

  TU_VERIFY(tud_control_xfer(rhport, p_request, &alt, 1));

  TU_LOG2("  Get itf: %u - current alt: %u\r\n", itf, alt);

  return true;
}

static bool audiod_set_interface(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;

  // Here we need to do the following:

  // 1. Find the audio driver assigned to the given interface to be set
  // Since one audio driver interface has to be able to cover an unknown number of interfaces (AC, AS + its alternate settings), the best memory efficient way to solve this is to always search through the descriptors.
  // The audio driver is mapped to an audio function by a reference pointer to the corresponding AC interface of this audio function which serves as a starting point for searching

  // 2. Close EPs which are currently open
  // To do so it is not necessary to know the current active alternate interface since we already save the current EP addresses - we simply close them

  // 3. Open new EP

  uint8_t const itf = tu_u16_low(p_request->wIndex);
  uint8_t const alt = tu_u16_low(p_request->wValue);

  TU_LOG2("  Set itf: %u - alt: %u\r\n", itf, alt);

  // Find index of audio streaming interface and index of interface
  uint8_t func_id;
  TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

  audiod_function_t *audio = &_audiod_fct[func_id];

// Look if there is an EP to be closed - for this driver, there are only 3 possible EPs which may be closed (only AS related EPs can be closed, AC EP (if present) is always open)
#if CFG_TUD_AUDIO_ENABLE_EP_IN
  if (audio->ep_in_as_intf_num == itf) {
    audio->ep_in_as_intf_num = 0;
    audio->ep_in_alt = 0;
  #ifndef TUP_DCD_EDPT_ISO_ALLOC
    usbd_edpt_close(rhport, audio->ep_in);
  #endif

    // Clear FIFOs, since data is no longer valid
    tu_fifo_clear(&audio->ep_in_ff);

    // Invoke callback - can be used to stop data sampling
    TU_VERIFY(tud_audio_set_itf_close_ep_cb(rhport, p_request));

    audio->ep_in = 0;// Necessary?

  #if CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
    audio->packet_sz_tx[0] = 0;
    audio->packet_sz_tx[1] = 0;
    audio->packet_sz_tx[2] = 0;
  #endif
  }
#endif// CFG_TUD_AUDIO_ENABLE_EP_IN

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
  if (audio->ep_out_as_intf_num == itf) {
    audio->ep_out_as_intf_num = 0;
    audio->ep_out_alt = 0;
  #ifndef TUP_DCD_EDPT_ISO_ALLOC
    usbd_edpt_close(rhport, audio->ep_out);
  #endif

    // Clear FIFOs, since data is no longer valid
    tu_fifo_clear(&audio->ep_out_ff);

    // Invoke callback - can be used to stop data sampling
    TU_VERIFY(tud_audio_set_itf_close_ep_cb(rhport, p_request));

    audio->ep_out = 0;// Necessary?

    // Close corresponding feedback EP
  #if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
    #ifndef TUP_DCD_EDPT_ISO_ALLOC
    usbd_edpt_close(rhport, audio->ep_fb);
    #endif
    audio->ep_fb = 0;
    tu_memclr(&audio->feedback, sizeof(audio->feedback));
  #endif
  }
#endif// CFG_TUD_AUDIO_ENABLE_EP_OUT

  // Open new EP if necessary - EPs are only to be closed or opened for AS interfaces - Look for AS interface with correct alternate interface
  uint8_t const *p_desc = tu_desc_next(audio->p_desc);
  // Skip entire AC descriptor block
  p_desc += ((audio_desc_cs_ac_interface_t const *) p_desc)->wTotalLength;
  // Get pointer at end
  uint8_t const *p_desc_end = audio->p_desc + audio->desc_length - TUD_AUDIO_DESC_IAD_LEN;

  // p_desc starts at required interface with alternate setting zero
  // Condition modified from p_desc < p_desc_end to prevent gcc>=12 strict-overflow warning
  while (p_desc_end - p_desc > 0) {
    // Find correct interface
    if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE && ((tusb_desc_interface_t const *) p_desc)->bInterfaceNumber == itf && ((tusb_desc_interface_t const *) p_desc)->bAlternateSetting == alt) {
#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
      uint8_t const *p_desc_parse_for_params = p_desc;
#endif
      // From this point forward follow the EP descriptors associated to the current alternate setting interface - Open EPs if necessary
      uint8_t foundEPs = 0, nEps = ((tusb_desc_interface_t const *) p_desc)->bNumEndpoints;
      // Condition modified from p_desc < p_desc_end to prevent gcc>=12 strict-overflow warning
      while (foundEPs < nEps && (p_desc_end - p_desc > 0)) {
        if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
          tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) p_desc;
#ifdef TUP_DCD_EDPT_ISO_ALLOC
          TU_ASSERT(usbd_edpt_iso_activate(rhport, desc_ep));
#else
          TU_ASSERT(usbd_edpt_open(rhport, desc_ep));
#endif
          uint8_t const ep_addr = desc_ep->bEndpointAddress;

          //TODO: We need to set EP non busy since this is not taken care of right now in ep_close() - THIS IS A WORKAROUND!
          usbd_edpt_clear_stall(rhport, ep_addr);

#if CFG_TUD_AUDIO_ENABLE_EP_IN
          // For data or data with implicit feedback IN EP
          if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN && (desc_ep->bmAttributes.usage == 0 || desc_ep->bmAttributes.usage == 2))
          {
            // Save address
            audio->ep_in = ep_addr;
            audio->ep_in_as_intf_num = itf;
            audio->ep_in_alt = alt;
            audio->ep_in_sz = tu_edpt_packet_size(desc_ep);

            // If flow control is enabled, parse for the corresponding parameters - doing this here means only AS interfaces with EPs get scanned for parameters
  #if  CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
            audiod_parse_flow_control_params(audio, p_desc_parse_for_params);
  #endif
            // Schedule first transmit if alternate interface is not zero, as sample data is available a ZLP is loaded
  #if USE_LINEAR_BUFFER_TX
            TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_in, audio->lin_buf_in, 0));
  #else
            // Send everything in ISO EP FIFO
            TU_VERIFY(usbd_edpt_xfer_fifo(rhport, audio->ep_in, &audio->ep_in_ff, 0));
  #endif
          }
#endif// CFG_TUD_AUDIO_ENABLE_EP_IN

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
          // Checking usage not necessary
          if (tu_edpt_dir(ep_addr) == TUSB_DIR_OUT) {
            // Save address
            audio->ep_out = ep_addr;
            audio->ep_out_as_intf_num = itf;
            audio->ep_out_alt = alt;
            audio->ep_out_sz = tu_edpt_packet_size(desc_ep);

            // Prepare for incoming data
  #if USE_LINEAR_BUFFER_RX
            TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_out, audio->lin_buf_out, audio->ep_out_sz), false);
  #else
            TU_VERIFY(usbd_edpt_xfer_fifo(rhport, audio->ep_out, &audio->ep_out_ff, audio->ep_out_sz), false);
  #endif
          }

  #if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
          // Check if usage is explicit data feedback
          if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN && desc_ep->bmAttributes.usage == 1) {
            audio->ep_fb = ep_addr;
            audio->feedback.frame_shift = desc_ep->bInterval - 1;
            // Schedule first feedback transmit
            audiod_fb_send(audio);
          }
  #endif
#endif// CFG_TUD_AUDIO_ENABLE_EP_OUT

          foundEPs += 1;
        }
        p_desc = tu_desc_next(p_desc);
      }

      TU_VERIFY(foundEPs == nEps);

      // Invoke one callback for a final set interface
      TU_VERIFY(tud_audio_set_itf_cb(rhport, p_request));

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
      // Prepare feedback computation if endpoint is available
      if (audio->ep_fb != 0) {
        audio_feedback_params_t fb_param;

        tud_audio_feedback_params_cb(func_id, alt, &fb_param);
        audio->feedback.compute_method = fb_param.method;

        if (TUSB_SPEED_FULL == tud_speed_get())
          audio->feedback.format_correction = tud_audio_feedback_format_correction_cb(func_id);

        // Minimal/Maximum value in 16.16 format for full speed (1ms per frame) or high speed (125 us per frame)
        uint32_t const frame_div = (TUSB_SPEED_FULL == tud_speed_get()) ? 1000 : 8000;
        audio->feedback.min_value = ((fb_param.sample_freq - 1) / frame_div) << 16;
        audio->feedback.max_value = (fb_param.sample_freq / frame_div + 1) << 16;

        switch (fb_param.method) {
          case AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED:
          case AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT:
          case AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2:
            audiod_set_fb_params_freq(audio, fb_param.sample_freq, fb_param.frequency.mclk_freq);
            break;

          case AUDIO_FEEDBACK_METHOD_FIFO_COUNT: {
            // Initialize the threshold level to half filled
            uint16_t fifo_lvl_thr = tu_fifo_depth(&audio->ep_out_ff) / 2;
            audio->feedback.compute.fifo_count.fifo_lvl_thr = fifo_lvl_thr;
            audio->feedback.compute.fifo_count.fifo_lvl_avg = ((uint32_t) fifo_lvl_thr) << 16;
            // Avoid 64bit division
            uint32_t nominal = ((fb_param.sample_freq / 100) << 16) / (frame_div / 100);
            audio->feedback.compute.fifo_count.nom_value = nominal;
            audio->feedback.compute.fifo_count.rate_const[0] = (uint16_t) ((audio->feedback.max_value - nominal) / fifo_lvl_thr);
            audio->feedback.compute.fifo_count.rate_const[1] = (uint16_t) ((nominal - audio->feedback.min_value) / fifo_lvl_thr);
            // On HS feedback is more sensitive since packet size can vary every MSOF, could cause instability
            if (tud_speed_get() == TUSB_SPEED_HIGH) {
              audio->feedback.compute.fifo_count.rate_const[0] /= 8;
              audio->feedback.compute.fifo_count.rate_const[1] /= 8;
            }
          } break;

          // nothing to do
          default:
            break;
        }
      }
#endif// CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP

      // We are done - abort loop
      break;
    }

    // Moving forward
    p_desc = tu_desc_next(p_desc);
  }

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  // Disable SOF interrupt if no driver has any enabled feedback EP
  bool enable_sof = false;
  for (uint8_t i = 0; i < CFG_TUD_AUDIO; i++) {
    if (_audiod_fct[i].ep_fb != 0 &&
        (_audiod_fct[i].feedback.compute_method == AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED ||
         _audiod_fct[i].feedback.compute_method == AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT ||
         _audiod_fct[i].feedback.compute_method == AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2)) {
      enable_sof = true;
      break;
    }
  }
  usbd_sof_enable(rhport, SOF_CONSUMER_AUDIO, enable_sof);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
  audiod_calc_tx_packet_sz(audio);
#endif

  tud_control_status(rhport, p_request);

  return true;
}

// Invoked when class request DATA stage is finished.
// return false to stall control EP (e.g Host send non-sense DATA)
static bool audiod_control_complete(uint8_t rhport, tusb_control_request_t const *p_request) {
  // Handle audio class specific set requests
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS && p_request->bmRequestType_bit.direction == TUSB_DIR_OUT) {
    uint8_t func_id;

    switch (p_request->bmRequestType_bit.recipient) {
      case TUSB_REQ_RCPT_INTERFACE: {
        uint8_t itf = TU_U16_LOW(p_request->wIndex);
        uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

        if (entityID != 0) {
          // Check if entity is present and get corresponding driver index
          TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &func_id));

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
          uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
          if (_audiod_fct[func_id].bclock_id_tx == entityID && ctrlSel == AUDIO_CS_CTRL_SAM_FREQ && p_request->bRequest == AUDIO_CS_REQ_CUR) {
            _audiod_fct[func_id].sample_rate_tx = tu_unaligned_read32(_audiod_fct[func_id].ctrl_buf);
          }
#endif

          // Invoke callback
          return tud_audio_set_req_entity_cb(rhport, p_request, _audiod_fct[func_id].ctrl_buf);
        } else {
          // Find index of audio driver structure and verify interface really exists
          TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

          // Invoke callback
          return tud_audio_set_req_itf_cb(rhport, p_request, _audiod_fct[func_id].ctrl_buf);
        }
      } break;

      case TUSB_REQ_RCPT_ENDPOINT: {
        uint8_t ep = TU_U16_LOW(p_request->wIndex);

        // Check if entity is present and get corresponding driver index
        TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));

        // Invoke callback
        return tud_audio_set_req_ep_cb(rhport, p_request, _audiod_fct[func_id].ctrl_buf);
      } break;
      // Unknown/Unsupported recipient
      default:
        TU_BREAKPOINT();
        return false;
    }
  }
  return true;
}

// Handle class control request
// return false to stall control endpoint (e.g unsupported request)
static bool audiod_control_request(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;

  // Handle standard requests - standard set requests usually have no data stage so we also handle set requests here
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
    switch (p_request->bRequest) {
      case TUSB_REQ_GET_INTERFACE:
        return audiod_get_interface(rhport, p_request);

      case TUSB_REQ_SET_INTERFACE:
        return audiod_set_interface(rhport, p_request);

      case TUSB_REQ_CLEAR_FEATURE:
        return true;

      // Unknown/Unsupported request
      default:
        TU_BREAKPOINT();
        return false;
    }
  }

  // Handle class requests
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t func_id;

    // Conduct checks which depend on the recipient
    switch (p_request->bmRequestType_bit.recipient) {
      case TUSB_REQ_RCPT_INTERFACE: {
        uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

        // Verify if entity is present
        if (entityID != 0) {
          // Find index of audio driver structure and verify entity really exists
          TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &func_id));

          // In case we got a get request invoke callback - callback needs to answer as defined in UAC2 specification page 89 - 5. Requests
          if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
            return tud_audio_get_req_entity_cb(rhport, p_request);
          }
        } else {
          // Find index of audio driver structure and verify interface really exists
          TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

          // In case we got a get request invoke callback - callback needs to answer as defined in UAC2 specification page 89 - 5. Requests
          if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
            return tud_audio_get_req_itf_cb(rhport, p_request);
          }
        }
      } break;

      case TUSB_REQ_RCPT_ENDPOINT: {
        uint8_t ep = TU_U16_LOW(p_request->wIndex);

        // Find index of audio driver structure and verify EP really exists
        TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));

        // In case we got a get request invoke callback - callback needs to answer as defined in UAC2 specification page 89 - 5. Requests
        if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
          return tud_audio_get_req_ep_cb(rhport, p_request);
        }
      } break;

      // Unknown/Unsupported recipient
      default:
        TU_LOG2("  Unsupported recipient: %d\r\n", p_request->bmRequestType_bit.recipient);
        TU_BREAKPOINT();
        return false;
    }

    // If we end here, the received request is a set request - we schedule a receive for the data stage and return true here. We handle the rest later in audiod_control_complete() once the data stage was finished
    TU_VERIFY(tud_control_xfer(rhport, p_request, _audiod_fct[func_id].ctrl_buf, _audiod_fct[func_id].ctrl_buf_sz));
    return true;
  }

  // There went something wrong - unsupported control request type
  TU_BREAKPOINT();
  return false;
}

bool audiod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  if (stage == CONTROL_STAGE_SETUP) {
    return audiod_control_request(rhport, request);
  } else if (stage == CONTROL_STAGE_DATA) {
    return audiod_control_complete(rhport, request);
  }

  return true;
}

bool audiod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) result;
  (void) xferred_bytes;

  #if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
  // Search for interface belonging to given end point address and proceed as required
  for (uint8_t func_id = 0; func_id < CFG_TUD_AUDIO; func_id++) {
    audiod_function_t *audio = &_audiod_fct[func_id];

    // Data transmission of control interrupt finished
    if (audio->ep_int == ep_addr) {
      // According to USB2 specification, maximum payload of interrupt EP is 8 bytes on low speed, 64 bytes on full speed, and 1024 bytes on high speed (but only if an alternate interface other than 0 is used - see specification p. 49)
      // In case there is nothing to send we have to return a NAK - this is taken care of by PHY ???
      // In case of an erroneous transmission a retransmission is conducted - this is taken care of by PHY ???

      // I assume here, that things above are handled by PHY
      // All transmission is done - what remains to do is to inform job was completed

      tud_audio_int_xfer_cb(rhport);
      return true;
    }

  }
  #else
  (void) rhport;
  (void) ep_addr;
  #endif

  return false;
}

bool audiod_xfer_isr(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) result;
  (void) xferred_bytes;

  // Search for interface belonging to given end point address and proceed as required
  for (uint8_t func_id = 0; func_id < CFG_TUD_AUDIO; func_id++)
  {
    audiod_function_t* audio = &_audiod_fct[func_id];

#if CFG_TUD_AUDIO_ENABLE_EP_IN

    // Data transmission of audio packet finished
    if (audio->ep_in == ep_addr) {
      // USB 2.0, section 5.6.4, third paragraph, states "An isochronous endpoint must specify its required bus access period. However, an isochronous endpoint must be prepared to handle poll rates faster than the one specified."
      // That paragraph goes on to say "An isochronous IN endpoint must return a zero-length packet whenever data is requested at a faster interval than the specified interval and data is not available."
      // This can only be solved reliably if we load a ZLP after every IN transmission since we can not say if the host requests samples earlier than we declared! Once all samples are collected we overwrite the loaded ZLP.

      // Check if there is data to load into EPs buffer - if not load it with ZLP
      // Be aware - we as a device are not able to know if the host polls for data with a faster rate as we stated this in the descriptors. Therefore we always have to put something into the EPs buffer. However, once we did that, there is no way of aborting this or replacing what we put into the buffer before!
      // This is the only place where we can fill something into the EPs buffer!

      // Load new data
      audiod_tx_xfer_isr(rhport, audio, (uint16_t) xferred_bytes);
      return true;
    }
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
    // New audio packet received
    if (audio->ep_out == ep_addr) {
      audiod_rx_xfer_isr(rhport, audio, (uint16_t) xferred_bytes);
      return true;
    }
  #if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
    // Transmission of feedback EP finished
    if (audio->ep_fb == ep_addr) {
      // Schedule a transmit with the new value if EP is not busy
      // Schedule next transmission - value is changed bytud_audio_n_fb_set() in the meantime or the old value gets sent
      audiod_fb_send(audio);
      return true;
    }
  #endif
#endif
  }

  return false;
}

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP

static bool audiod_set_fb_params_freq(audiod_function_t *audio, uint32_t sample_freq, uint32_t mclk_freq) {
  // Check if frame interval is within sane limits
  // The interval value n_frames was taken from the descriptors within audiod_set_interface()

  // n_frames_min is ceil(2^10 * f_s / f_m) for full speed and ceil(2^13 * f_s / f_m) for high speed
  // this lower limit ensures the measures feedback value has sufficient precision
  uint32_t const k = (TUSB_SPEED_FULL == tud_speed_get()) ? 10 : 13;
  uint32_t const n_frame = (1UL << audio->feedback.frame_shift);

  if ((((1UL << k) * sample_freq / mclk_freq) + 1) > n_frame) {
    TU_LOG1("  UAC2 feedback interval too small\r\n");
    TU_BREAKPOINT();
    return false;
  }

  // Check if parameters really allow for a power of two division
  if ((mclk_freq % sample_freq) == 0 && tu_is_power_of_two(mclk_freq / sample_freq)) {
    audio->feedback.compute_method = AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2;
    audio->feedback.compute.power_of_2 = (uint8_t) (16 - (audio->feedback.frame_shift - 1) - tu_log2(mclk_freq / sample_freq));
  } else if (audio->feedback.compute_method == AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT) {
    audio->feedback.compute.float_const = (float) sample_freq / (float) mclk_freq * (1UL << (16 - (audio->feedback.frame_shift - 1)));
  } else {
    audio->feedback.compute.fixed.sample_freq = sample_freq;
    audio->feedback.compute.fixed.mclk_freq = mclk_freq;
  }

  return true;
}

static void audiod_fb_fifo_count_update(audiod_function_t *audio, uint16_t lvl_new) {
  /* Low-pass (averaging) filter */
  uint32_t lvl = audio->feedback.compute.fifo_count.fifo_lvl_avg;
  lvl = (uint32_t) (((uint64_t) lvl * 63 + ((uint32_t) lvl_new << 16)) >> 6);
  audio->feedback.compute.fifo_count.fifo_lvl_avg = lvl;

  uint32_t const ff_lvl = lvl >> 16;
  uint16_t const ff_thr = audio->feedback.compute.fifo_count.fifo_lvl_thr;
  uint16_t const *rate = audio->feedback.compute.fifo_count.rate_const;

  uint32_t feedback;

  if (ff_lvl < ff_thr) {
    feedback = audio->feedback.compute.fifo_count.nom_value + (ff_thr - ff_lvl) * rate[0];
  } else {
    feedback = audio->feedback.compute.fifo_count.nom_value - (ff_lvl - ff_thr) * rate[1];
  }

  if (feedback > audio->feedback.max_value) feedback = audio->feedback.max_value;
  if (feedback < audio->feedback.min_value) feedback = audio->feedback.min_value;
  audio->feedback.value = feedback;
}

uint32_t tud_audio_feedback_update(uint8_t func_id, uint32_t cycles) {
  audiod_function_t *audio = &_audiod_fct[func_id];
  uint32_t feedback;

  switch (audio->feedback.compute_method) {
    case AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2:
      feedback = (cycles << audio->feedback.compute.power_of_2);
      break;

    case AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT:
      feedback = (uint32_t) ((float) cycles * audio->feedback.compute.float_const);
      break;

    case AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED: {
      uint64_t fb64 = (((uint64_t) cycles) * audio->feedback.compute.fixed.sample_freq) << (16 - (audio->feedback.frame_shift - 1));
      feedback = (uint32_t) (fb64 / audio->feedback.compute.fixed.mclk_freq);
    } break;

    default:
      return 0;
  }

  // For Windows: https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/usb-2-0-audio-drivers
  // The size of isochronous packets created by the device must be within the limits specified in FMT-2.0 section 2.3.1.1.
  // This means that the deviation of actual packet size from nominal size must not exceed +/- one audio slot
  // (audio slot = channel count samples).
  if (feedback > audio->feedback.max_value) feedback = audio->feedback.max_value;
  if (feedback < audio->feedback.min_value) feedback = audio->feedback.min_value;

  tud_audio_n_fb_set(func_id, feedback);

  return feedback;
}

bool tud_audio_n_fb_set(uint8_t func_id, uint32_t feedback) {
  TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);

  _audiod_fct[func_id].feedback.value = feedback;

  return true;
}
#endif

TU_ATTR_FAST_FUNC void audiod_sof_isr(uint8_t rhport, uint32_t frame_count) {
  (void) rhport;
  (void) frame_count;

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  // Determine feedback value - The feedback method is described in 5.12.4.2 of the USB 2.0 spec
  // Boiled down, the feedback value Ff = n_samples / (micro)frame.
  // Since an accuracy of less than 1 Sample / second is desired, at least n_frames = ceil(2^K * f_s / f_m) frames need to be measured, where K = 10 for full speed and K = 13 for high speed, f_s is the sampling frequency e.g. 48 kHz and f_m is the cpu clock frequency e.g. 100 MHz (or any other master clock whose clock count is available and locked to f_s)
  // The update interval in the (4.10.2.1) Feedback Endpoint Descriptor must be less or equal to 2^(K - P), where P = min( ceil(log2(f_m / f_s)), K)
  // feedback = n_cycles / n_frames * f_s / f_m in 16.16 format, where n_cycles are the number of main clock cycles within fb_n_frames

  // Iterate over audio functions and set feedback value
  for (uint8_t i = 0; i < CFG_TUD_AUDIO; i++) {
    audiod_function_t *audio = &_audiod_fct[i];

    if (audio->ep_fb != 0) {
      // HS shift need to be adjusted since SOF event is generated for frame only
      uint8_t const hs_adjust = (TUSB_SPEED_HIGH == tud_speed_get()) ? 3 : 0;
      uint32_t const interval = 1UL << (audio->feedback.frame_shift - hs_adjust);
      if (0 == (frame_count & (interval - 1))) {
        tud_audio_feedback_interval_isr(i, frame_count, audio->feedback.frame_shift);
      }
    }
  }
#endif// CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
}

bool tud_audio_buffer_and_schedule_control_xfer(uint8_t rhport, tusb_control_request_t const *p_request, void *data, uint16_t len) {
  // Handles only sending of data not receiving
  if (p_request->bmRequestType_bit.direction == TUSB_DIR_OUT) return false;

  // Get corresponding driver index
  uint8_t func_id;
  uint8_t itf = TU_U16_LOW(p_request->wIndex);

  // Conduct checks which depend on the recipient
  switch (p_request->bmRequestType_bit.recipient) {
    case TUSB_REQ_RCPT_INTERFACE: {
      uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

      // Verify if entity is present
      if (entityID != 0) {
        // Find index of audio driver structure and verify entity really exists
        TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &func_id));
      } else {
        // Find index of audio driver structure and verify interface really exists
        TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));
      }
    } break;

    case TUSB_REQ_RCPT_ENDPOINT: {
      uint8_t ep = TU_U16_LOW(p_request->wIndex);

      // Find index of audio driver structure and verify EP really exists
      TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));
    } break;

    // Unknown/Unsupported recipient
    default:
      TU_LOG2("  Unsupported recipient: %d\r\n", p_request->bmRequestType_bit.recipient);
      TU_BREAKPOINT();
      return false;
  }

  // Crop length
  if (len > _audiod_fct[func_id].ctrl_buf_sz) len = _audiod_fct[func_id].ctrl_buf_sz;

  // Copy into buffer
  TU_VERIFY(0 == tu_memcpy_s(_audiod_fct[func_id].ctrl_buf, _audiod_fct[func_id].ctrl_buf_sz, data, (size_t) len));

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
  // Find data for sampling_frequency_control
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS && p_request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE) {
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    if (_audiod_fct[func_id].bclock_id_tx == entityID && ctrlSel == AUDIO_CS_CTRL_SAM_FREQ && p_request->bRequest == AUDIO_CS_REQ_CUR) {
      _audiod_fct[func_id].sample_rate_tx = tu_unaligned_read32(_audiod_fct[func_id].ctrl_buf);
    }
  }
#endif

  // Schedule transmit
  return tud_control_xfer(rhport, p_request, (void *) _audiod_fct[func_id].ctrl_buf, len);
}

// Verify an entity with the given ID exists and returns also the corresponding driver index
static bool audiod_verify_entity_exists(uint8_t itf, uint8_t entityID, uint8_t *func_id) {
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++) {
    // Look for the correct driver by checking if the unique standard AC interface number fits
    if (_audiod_fct[i].p_desc && ((tusb_desc_interface_t const *) _audiod_fct[i].p_desc)->bInterfaceNumber == itf) {
      // Get pointers after class specific AC descriptors and end of AC descriptors - entities are defined in between
      uint8_t const *p_desc = tu_desc_next(_audiod_fct[i].p_desc);// Points to CS AC descriptor
      uint8_t const *p_desc_end = ((audio_desc_cs_ac_interface_t const *) p_desc)->wTotalLength + p_desc;
      p_desc = tu_desc_next(p_desc);// Get past CS AC descriptor

      // Condition modified from p_desc < p_desc_end to prevent gcc>=12 strict-overflow warning
      while (p_desc_end - p_desc > 0) {
        // Entity IDs are always at offset 3
        if (p_desc[3] == entityID) {
          *func_id = i;
          return true;
        }
        p_desc = tu_desc_next(p_desc);
      }
    }
  }
  return false;
}

static bool audiod_verify_itf_exists(uint8_t itf, uint8_t *func_id) {
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++) {
    if (_audiod_fct[i].p_desc) {
      // Get pointer at beginning and end
      uint8_t const *p_desc = _audiod_fct[i].p_desc;
      uint8_t const *p_desc_end = _audiod_fct[i].p_desc + _audiod_fct[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
      // Condition modified from p_desc < p_desc_end to prevent gcc>=12 strict-overflow warning
      while (p_desc_end - p_desc > 0) {
        if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE && ((tusb_desc_interface_t const *)p_desc)->bInterfaceNumber == itf) {
          *func_id = i;
          return true;
        }
        p_desc = tu_desc_next(p_desc);
      }
    }
  }
  return false;
}

static bool audiod_verify_ep_exists(uint8_t ep, uint8_t *func_id) {
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++) {
    if (_audiod_fct[i].p_desc) {
      // Get pointer at end
      uint8_t const *p_desc_end = _audiod_fct[i].p_desc + _audiod_fct[i].desc_length;

      // Advance past AC descriptors - EP we look for are streaming EPs
      uint8_t const *p_desc = tu_desc_next(_audiod_fct[i].p_desc);
      p_desc += ((audio_desc_cs_ac_interface_t const *) p_desc)->wTotalLength;

      // Condition modified from p_desc < p_desc_end to prevent gcc>=12 strict-overflow warning
      while (p_desc_end - p_desc > 0) {
        if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT && ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress == ep) {
          *func_id = i;
          return true;
        }
        p_desc = tu_desc_next(p_desc);
      }
    }
  }
  return false;
}

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL
static void audiod_parse_flow_control_params(audiod_function_t *audio, uint8_t const *p_desc) {

  p_desc = tu_desc_next(p_desc);// Exclude standard AS interface descriptor of current alternate interface descriptor

  // Look for a Class-Specific AS Interface Descriptor(4.9.2) to verify format type and format and also to get number of physical channels
  if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE && tu_desc_subtype(p_desc) == AUDIO_CS_AS_INTERFACE_AS_GENERAL) {
    audio->n_channels_tx = ((audio_desc_cs_as_interface_t const *) p_desc)->bNrChannels;
    audio->format_type_tx = (audio_format_type_t) (((audio_desc_cs_as_interface_t const *) p_desc)->bFormatType);
    // Look for a Type I Format Type Descriptor(2.3.1.6 - Audio Formats)
    p_desc = tu_desc_next(p_desc);
    if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE && tu_desc_subtype(p_desc) == AUDIO_CS_AS_INTERFACE_FORMAT_TYPE && ((audio_desc_type_I_format_t const *) p_desc)->bFormatType == AUDIO_FORMAT_TYPE_I) {
      audio->n_bytes_per_sample_tx = ((audio_desc_type_I_format_t const *) p_desc)->bSubslotSize;
    }
  }
}

static bool audiod_calc_tx_packet_sz(audiod_function_t *audio) {
  TU_VERIFY(audio->format_type_tx == AUDIO_FORMAT_TYPE_I);
  TU_VERIFY(audio->n_channels_tx);
  TU_VERIFY(audio->n_bytes_per_sample_tx);
  TU_VERIFY(audio->interval_tx);
  TU_VERIFY(audio->sample_rate_tx);

  const uint8_t interval = (tud_speed_get() == TUSB_SPEED_FULL) ? audio->interval_tx : 1 << (audio->interval_tx - 1);

  const uint16_t sample_normimal = (uint16_t) (audio->sample_rate_tx * interval / ((tud_speed_get() == TUSB_SPEED_FULL) ? 1000 : 8000));
  const uint16_t sample_reminder = (uint16_t) (audio->sample_rate_tx * interval % ((tud_speed_get() == TUSB_SPEED_FULL) ? 1000 : 8000));

  const uint16_t packet_sz_tx_min = (uint16_t) ((sample_normimal - 1) * audio->n_channels_tx * audio->n_bytes_per_sample_tx);
  const uint16_t packet_sz_tx_norm = (uint16_t) (sample_normimal * audio->n_channels_tx * audio->n_bytes_per_sample_tx);
  const uint16_t packet_sz_tx_max = (uint16_t) ((sample_normimal + 1) * audio->n_channels_tx * audio->n_bytes_per_sample_tx);

  // Endpoint size must larger than packet size
  TU_ASSERT(packet_sz_tx_max <= audio->ep_in_sz);

  // Frmt20.pdf 2.3.1.1 USB Packets
  if (sample_reminder) {
    // All virtual frame packets must either contain INT(nav) audio slots (small VFP) or INT(nav)+1 (large VFP) audio slots
    audio->packet_sz_tx[0] = packet_sz_tx_norm;
    audio->packet_sz_tx[1] = packet_sz_tx_norm;
    audio->packet_sz_tx[2] = packet_sz_tx_max;
  } else {
    // In the case where nav = INT(nav), ni may vary between INT(nav)-1 (small VFP), INT(nav)
    // (medium VFP) and INT(nav)+1 (large VFP).
    audio->packet_sz_tx[0] = packet_sz_tx_min;
    audio->packet_sz_tx[1] = packet_sz_tx_norm;
    audio->packet_sz_tx[2] = packet_sz_tx_max;
  }

  return true;
}

static uint16_t audiod_tx_packet_size(const uint16_t *norminal_size, uint16_t data_count, uint16_t fifo_depth, uint16_t max_depth) {
  // Flow control need a FIFO size of at least 4*Navg
  if (norminal_size[1] && norminal_size[1] <= fifo_depth * 4) {
    // Use blackout to prioritize normal size packet
    static int ctrl_blackout = 0;
    uint16_t packet_size;
    uint16_t slot_size = norminal_size[2] - norminal_size[1];
    if (data_count < norminal_size[0]) {
      // If you get here frequently, then your I2S clock deviation is too big !
      packet_size = 0;
    } else if (data_count < fifo_depth / 2 - slot_size && !ctrl_blackout) {
      packet_size = norminal_size[0];
      ctrl_blackout = 10;
    } else if (data_count > fifo_depth / 2 + slot_size && !ctrl_blackout) {
      packet_size = norminal_size[2];
      if (norminal_size[0] == norminal_size[1]) {
        // nav > INT(nav), eg. 44.1k, 88.2k
        ctrl_blackout = 0;
      } else {
        // nav = INT(nav), eg. 48k, 96k
        ctrl_blackout = 10;
      }
    } else {
      packet_size = norminal_size[1];
      if (ctrl_blackout) {
        ctrl_blackout--;
      }
    }
    // Normally this cap is not necessary
    return tu_min16(packet_size, max_depth);
  } else {
    return tu_min16(data_count, max_depth);
  }
}

#endif

// No security checks here - internal function only which should always succeed
static uint8_t audiod_get_audio_fct_idx(audiod_function_t *audio) {
  for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO; cnt++) {
    if (&_audiod_fct[cnt] == audio) return cnt;
  }
  return 0;
}

#endif // (CFG_TUD_ENABLED && CFG_TUD_AUDIO)
