/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Reinhard Panhuber, Jerzy Kasenberg
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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_AUDIO)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "audio_device.h"
#include "class/audio/audio.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

  // Linear buffer in case target MCU is not capable of handling a ring buffer FIFO e.g. no hardware buffer is available or driver is would need to be changed dramatically
#if ( CFG_TUSB_MCU == OPT_MCU_MKL25ZXX            || /* Intermediate software buffer required */                        \
    CFG_TUSB_MCU == OPT_MCU_DA1469X               || /* Intermediate software buffer required */                        \
    CFG_TUSB_MCU == OPT_MCU_LPC18XX               || /* No clue how driver works */                                     \
    CFG_TUSB_MCU == OPT_MCU_LPC43XX               ||                                                                    \
    CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX            ||                                                                    \
    CFG_TUSB_MCU == OPT_MCU_RP2040                || /* Don't want to change driver */                                  \
    CFG_TUSB_MCU == OPT_MCU_VALENTYUSB_EPTRI      || /* Intermediate software buffer required */                        \
    CFG_TUSB_MCU == OPT_MCU_CXD56                 || /* No clue how driver works */                                     \
    CFG_TUSB_MCU == OPT_MCU_NRF5X                 || /* Uses DMA - Ok for FIFO, had no time for implementation */       \
    CFG_TUSB_MCU == OPT_MCU_LPC11UXX              || /* Uses DMA - Ok for FIFO, had no time for implementation */       \
    CFG_TUSB_MCU == OPT_MCU_LPC13XX               || \
    CFG_TUSB_MCU == OPT_MCU_LPC15XX               || \
    CFG_TUSB_MCU == OPT_MCU_LPC51UXX              || \
    CFG_TUSB_MCU == OPT_MCU_LPC54XXX              || \
    CFG_TUSB_MCU == OPT_MCU_LPC55XX               || \
    CFG_TUSB_MCU == OPT_MCU_LPC175X_6X            || \
    CFG_TUSB_MCU == OPT_MCU_LPC40XX               || \
    CFG_TUSB_MCU == OPT_MCU_SAMD11                || /* Uses DMA - Ok for FIFO, had no time for implementation */       \
    CFG_TUSB_MCU == OPT_MCU_SAMD21                || \
    CFG_TUSB_MCU == OPT_MCU_SAMD51                || \
    CFG_TUSB_MCU == OPT_MCU_SAME5X )

#define USE_LINEAR_BUFFER      1

#else
#define  USE_LINEAR_BUFFER     0
#endif

typedef struct
{
  uint8_t rhport;
  uint8_t const * p_desc;       // Pointer pointing to Standard AC Interface Descriptor(4.7.1) - Audio Control descriptor defining audio function

#if CFG_TUD_AUDIO_EPSIZE_IN
  uint8_t ep_in;                // TX audio data EP.
  uint8_t ep_in_as_intf_num;    // Corresponding Standard AS Interface Descriptor (4.9.1) belonging to output terminal to which this EP belongs - 0 is invalid (this fits to UAC2 specification since AS interfaces can not have interface number equal to zero)
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT
  uint8_t ep_out;               // Incoming (into uC) audio data EP.
  uint8_t ep_out_as_intf_num;   // Corresponding Standard AS Interface Descriptor (4.9.1) belonging to input terminal to which this EP belongs - 0 is invalid (this fits to UAC2 specification since AS interfaces can not have interface number equal to zero)

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  uint8_t ep_fb;                // Feedback EP.
#endif

#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
  uint8_t ep_int_ctr;           // Audio control interrupt EP.
#endif

#if CFG_TUD_AUDIO_N_AS_INT
  uint8_t altSetting[CFG_TUD_AUDIO_N_AS_INT];   // We need to save the current alternate setting this way, because it is possible that there are AS interfaces which do not have an EP!
#endif

  /*------------- From this point, data is not cleared by bus reset -------------*/

  // Buffer for control requests
  CFG_TUSB_MEM_ALIGN uint8_t ctrl_buf[CFG_TUD_AUDIO_CTRL_BUF_SIZE];

  // EP Transfer buffers and FIFOs
#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE

#if !CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
  CFG_TUSB_MEM_ALIGN uint8_t ep_out_buf[CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE];
  tu_fifo_t ep_out_ff;

#if CFG_FIFO_MUTEX
  osal_mutex_def_t ep_out_ff_mutex_rd;  // No need for write mutex as only USB driver writes into FIFO
#endif

#endif

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  uint32_t fb_val;                                                              // Feedback value for asynchronous mode (in 16.16 format).
#endif

#endif

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE && !CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
  CFG_TUSB_MEM_ALIGN uint8_t ep_in_buf[CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE];
  tu_fifo_t ep_in_ff;

#if CFG_FIFO_MUTEX
  osal_mutex_def_t ep_in_ff_mutex_wr;   // No need for read mutex as only USB driver reads from FIFO
#endif

#endif

  // Audio control interrupt buffer - no FIFO
#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
  CFG_TUSB_MEM_ALIGN uint8_t ep_int_ctr_buf[CFG_TUD_AUDIO_INT_CTR_EP_IN_SW_BUFFER_SIZE];
#endif

  // Support FIFOs
#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
  tu_fifo_t tx_supp_ff[CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO];
  CFG_TUSB_MEM_ALIGN uint8_t tx_supp_ff_buf[CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO][CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX];
#if CFG_FIFO_MUTEX
  osal_mutex_def_t tx_supp_ff_mutex_wr[CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO];  // No need for read mutex as only USB driver reads from FIFO
#endif
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
  tu_fifo_t rx_supp_ff[CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO];
  CFG_TUSB_MEM_ALIGN uint8_t rx_supp_ff_buf[CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO][CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX];
#if CFG_FIFO_MUTEX
  osal_mutex_def_t rx_supp_ff_mutex_rd[CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO];  // No need for write mutex as only USB driver writes into FIFO
#endif
#endif

  // Linear buffer in case target MCU is not capable of handling a ring buffer FIFO e.g. no hardware buffer is available or driver is would need to be changed dramatically OR the support FIFOs are used
#if CFG_TUD_AUDIO_EPSIZE_OUT && (USE_LINEAR_BUFFER || CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE)
  CFG_TUSB_MEM_ALIGN uint8_t lin_buf_out[CFG_TUD_AUDIO_EPSIZE_OUT];
#define USE_LINEAR_BUFFER_RX   1
#endif

#if CFG_TUD_AUDIO_EPSIZE_IN && (USE_LINEAR_BUFFER || CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE)
  CFG_TUSB_MEM_ALIGN uint8_t lin_buf_in[CFG_TUD_AUDIO_EPSIZE_IN];
#define USE_LINEAR_BUFFER_TX   1
#endif

} audiod_interface_t;

#ifndef USE_LINEAR_BUFFER_TX
#define USE_LINEAR_BUFFER_TX   0
#endif

#ifndef USE_LINEAR_BUFFER_RX
#define USE_LINEAR_BUFFER_RX   0
#endif

#define ITF_MEM_RESET_SIZE   offsetof(audiod_interface_t, ctrl_buf)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION audiod_interface_t _audiod_itf[CFG_TUD_AUDIO];

extern const uint16_t tud_audio_desc_lengths[];

// bSubslotSize for PCM encoding/decoding using support FIFOs
#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
extern const tud_audio_n_bytes_per_sample_tx[CFG_TUD_AUDIO][CFG_TUD_AUDIO_N_AS_INT];
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
extern const tud_audio_n_bytes_per_sample_rx[CFG_TUD_AUDIO][CFG_TUD_AUDIO_N_AS_INT];
#endif

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE
static bool audiod_rx_done_cb(uint8_t rhport, audiod_interface_t* audio, uint16_t n_bytes_received);
#endif

#if CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE && CFG_TUD_AUDIO_EPSIZE_OUT
static bool audiod_decode_type_I_pcm(uint8_t rhport, audiod_interface_t* audio, uint16_t n_bytes_received);
#endif

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE
static bool audiod_tx_done_cb(uint8_t rhport, audiod_interface_t* audio);
#endif

#if CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE && CFG_TUD_AUDIO_EPSIZE_IN
static uint16_t audiod_encode_type_I_pcm(uint8_t rhport, audiod_interface_t* audio);
#endif

static bool audiod_get_interface(uint8_t rhport, tusb_control_request_t const * p_request);
static bool audiod_set_interface(uint8_t rhport, tusb_control_request_t const * p_request);

static bool audiod_get_AS_interface_index(uint8_t itf, uint8_t *idxDriver, uint8_t *idxItf, uint8_t const **pp_desc_int);
static bool audiod_verify_entity_exists(uint8_t itf, uint8_t entityID, uint8_t *idxDriver);
static bool audiod_verify_itf_exists(uint8_t itf, uint8_t *idxDriver);
static bool audiod_verify_ep_exists(uint8_t ep, uint8_t *idxDriver);

bool tud_audio_n_mounted(uint8_t itf)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO);
  audiod_interface_t* audio = &_audiod_itf[itf];

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE
  if (audio->ep_out == 0) return false;
#endif

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE
  if (audio->ep_in == 0) return false;
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
  if (audio->ep_int_ctr == 0) return false;
#endif

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  if (audio->ep_fb == 0) return false;
#endif

  return true;
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE && !CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE

uint16_t tud_audio_n_available(uint8_t itf)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL);
  return tu_fifo_count(&_audiod_itf[itf].ep_out_ff);
}

uint16_t tud_audio_n_read(uint8_t itf, void* buffer, uint16_t bufsize)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL);
  return tu_fifo_read_n(&_audiod_itf[itf].ep_out_ff, buffer, bufsize);
}

bool tud_audio_n_clear_ep_out_ff(uint8_t itf)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL);
  return tu_fifo_clear(&_audiod_itf[itf].ep_out_ff);
}

#endif

#if CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE && CFG_TUD_AUDIO_EPSIZE_OUT
// Delete all content in the support RX FIFOs
bool tud_audio_n_clear_rx_support_ff(uint8_t itf, uint8_t channelId)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL, channelId < CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO);
  return tu_fifo_clear(&_audiod_itf[itf].rx_supp_ff[channelId]);
}

uint16_t tud_audio_n_available_support_ff(uint8_t itf, uint8_t channelId)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL, channelId < CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO);
  return tu_fifo_count(&_audiod_itf[itf].rx_supp_ff[channelId]);
}

uint16_t tud_audio_n_read_support_ff(uint8_t itf, uint8_t channelId, void* buffer, uint16_t bufsize)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL, channelId < CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO);
  return tu_fifo_read_n(&_audiod_itf[itf].rx_supp_ff[channelId], buffer, bufsize);
}
#endif

// This function is called once an audio packet is received by the USB and is responsible for putting data from USB memory into EP_OUT_FIFO (or support FIFOs + decoding of received stream into audio channels).
// If you prefer your own (more efficient) implementation suiting your purpose set CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE = 0.

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE

static bool audiod_rx_done_cb(uint8_t rhport, audiod_interface_t* audio, uint16_t n_bytes_received)
{
  uint8_t idxDriver, idxItf;
  uint8_t const *dummy2;

  // If a callback is used determine current alternate setting of
  if (tud_audio_rx_done_pre_read_cb || tud_audio_rx_done_post_read_cb)
  {
    // Find index of audio streaming interface and index of interface
    TU_VERIFY(audiod_get_AS_interface_index(audio->ep_out_as_intf_num, &idxDriver, &idxItf, &dummy2));
  }

  // Call a weak callback here - a possibility for user to get informed an audio packet was received and data gets now loaded into EP FIFO (or decoded into support RX software FIFO)
  if (tud_audio_rx_done_pre_read_cb) TU_VERIFY(tud_audio_rx_done_pre_read_cb(rhport, n_bytes_received, idxDriver, audio->ep_out, audio->altSetting[idxItf]));

#if CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE && CFG_TUD_AUDIO_EPSIZE_OUT

  switch (CFG_TUD_AUDIO_FORMAT_TYPE_RX)
  {
    case AUDIO_FORMAT_TYPE_UNDEFINED:
      // INDIVIDUAL DECODING PROCEDURE REQUIRED HERE!
      TU_LOG2("  Desired CFG_TUD_AUDIO_FORMAT encoding not implemented!\r\n");
      TU_BREAKPOINT();
      break;

    case AUDIO_FORMAT_TYPE_I:

      switch (CFG_TUD_AUDIO_FORMAT_TYPE_I_RX)
      {
        case AUDIO_DATA_FORMAT_TYPE_I_PCM:
          TU_VERIFY(audiod_decode_type_I_pcm(rhport, audio, n_bytes_received));
          break;

        default:
          // DESIRED CFG_TUD_AUDIO_FORMAT_TYPE_I_RX NOT IMPLEMENTED!
          TU_LOG2("  Desired CFG_TUD_AUDIO_FORMAT_TYPE_I_RX encoding not implemented!\r\n");
          TU_BREAKPOINT();
          break;
      }
      break;

        default:
          // Desired CFG_TUD_AUDIO_FORMAT_TYPE_RX not implemented!
          TU_LOG2("  Desired CFG_TUD_AUDIO_FORMAT_TYPE_RX not implemented!\r\n");
          TU_BREAKPOINT();
          break;
  }

  // Prepare for next transmission
  TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_out, audio->lin_buf_out, CFG_TUD_AUDIO_EPSIZE_OUT), false);

#else

#if USE_LINEAR_BUFFER_RX
  // Data currently is in linear buffer, copy into EP OUT FIFO
  TU_VERIFY(tu_fifo_write_n(&audio->ep_out_ff, audio->lin_buf_out, n_bytes_received));

  // Schedule for next receive
  TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_out, audio->lin_buf_out, CFG_TUD_AUDIO_EPSIZE_OUT), false);
#else
  // Data is already placed in EP FIFO, schedule for next receive
  TU_VERIFY(usbd_edpt_iso_xfer(rhport, audio->ep_out, &audio->ep_out_ff, CFG_TUD_AUDIO_EPSIZE_OUT), false);
#endif

#endif

  // Call a weak callback here - a possibility for user to get informed decoding was completed
  if (tud_audio_rx_done_post_read_cb) TU_VERIFY(tud_audio_rx_done_post_read_cb(rhport, n_bytes_received, idxDriver, audio->ep_out, audio->altSetting[idxItf]));

  return true;
}

#endif //CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE

// The following functions are used in case CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE != 0
#if CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE && CFG_TUD_AUDIO_EPSIZE_OUT

// Decoding according to 2.3.1.5 Audio Streams
static bool audiod_decode_type_I_pcm(uint8_t rhport, audiod_interface_t* audio, uint16_t n_bytes_received)
{
  (void) rhport;

  // Determine amount of samples
  uint16_t const nChannelsPerFF         = CFG_TUD_AUDIO_N_CHANNELS_RX / CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO;
  uint16_t const nBytesToCopy           = nChannelsPerFF*CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX;
  uint16_t const nSamplesPerFFToRead    = n_bytes_received / CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX / CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO;
  uint8_t cnt_ff;

  // Decode
  void * dst;
  uint8_t * src;
  uint8_t * dst_end;
  uint16_t len;

  for (cnt_ff = 0; cnt_ff < CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO; cnt_ff++)
  {
    src = &audio->lin_buf_out[cnt_ff*nChannelsPerFF*CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX];

    len = tu_fifo_get_linear_write_info(&audio->rx_supp_ff[cnt_ff], 0, &dst, nSamplesPerFFToRead);
    tu_fifo_advance_write_pointer(&audio->rx_supp_ff[cnt_ff], len);

    dst_end = dst + len * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX;

    while((uint8_t *)dst < dst_end)
    {
      memcpy(dst, src, nBytesToCopy);
      dst = (uint8_t *)dst + nBytesToCopy;
      src += nBytesToCopy * CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO;
    }

    // Handle wrapped part of FIFO
    if (len < nSamplesPerFFToRead)
    {
      len = tu_fifo_get_linear_write_info(&audio->rx_supp_ff[cnt_ff], 0, &dst, nSamplesPerFFToRead - len);
      tu_fifo_advance_write_pointer(&audio->rx_supp_ff[cnt_ff], len);

      dst_end = dst + len * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX;

      while((uint8_t *)dst < dst_end)
      {
        memcpy(dst, src, nBytesToCopy);
        dst = (uint8_t *)dst + nBytesToCopy;
        src += nBytesToCopy * CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO;
      }
    }
  }

  // Number of bytes should be a multiple of CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX * CFG_TUD_AUDIO_N_CHANNELS_RX but checking makes no sense - no way to correct it
  // TU_VERIFY(cnt != n_bytes);

  return true;
}
#endif //CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE && !CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE

/**
 * \brief           Write data to EP in buffer
 *
 *  Write data to buffer. If it is full, new data can be inserted once a transmit was scheduled. See audiod_tx_done_cb().
 *  If TX FIFOs are used, this function is not available in order to not let the user mess up the encoding process.
 *
 * \param[in]       itf: Index of audio function interface
 * \param[in]       data: Pointer to data array to be copied from
 * \param[in]       len: # of array elements to copy
 * \return          Number of bytes actually written
 */
uint16_t tud_audio_n_write(uint8_t itf, const void * data, uint16_t len)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL);
  return tu_fifo_write_n(&_audiod_itf[itf].ep_in_ff, data, len);
}

bool tud_audio_n_clear_ep_in_ff(uint8_t itf)                          // Delete all content in the EP IN FIFO
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL);
  return tu_fifo_clear(&_audiod_itf[itf].ep_in_ff);
}

#endif

#if CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE && CFG_TUD_AUDIO_EPSIZE_IN
uint16_t tud_audio_n_flush_tx_support_ff(uint8_t itf)                 // Force all content in the support TX FIFOs to be written into linear buffer and schedule a transmit
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL);
  audiod_interface_t* audio = &_audiod_itf[itf];

  uint16_t n_bytes_copied = tu_fifo_count(&audio->tx_supp_ff[0]);

  TU_VERIFY(audiod_tx_done_cb(audio->rhport, audio));

  n_bytes_copied -= tu_fifo_count(&audio->tx_supp_ff[0]);
  n_bytes_copied = n_bytes_copied*audio->tx_supp_ff[0].item_size;

  return n_bytes_copied;
}

bool tud_audio_n_clear_tx_support_ff(uint8_t itf, uint8_t channelId)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL, channelId < CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO);
  return tu_fifo_clear(&_audiod_itf[itf].tx_supp_ff[channelId]);
}

uint16_t tud_audio_n_write_support_ff(uint8_t itf, uint8_t channelId, const void * data, uint16_t len)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL, channelId < CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO);
  return tu_fifo_write_n(&_audiod_itf[itf].tx_supp_ff[channelId], data, len);
}
#endif


#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN

// If no interrupt transmit is pending bytes get written into buffer and a transmit is scheduled - once transmit completed tud_audio_int_ctr_done_cb() is called in inform user
uint16_t tud_audio_int_ctr_n_write(uint8_t itf, uint8_t const* buffer, uint16_t len)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL);

  // We write directly into the EP's buffer - abort if previous transfer not complete
  TU_VERIFY(!usbd_edpt_busy(_audiod_itf[itf].rhport, _audiod_itf[itf].ep_int_ctr));

  // Check length
  TU_VERIFY(len <= CFG_TUD_AUDIO_INT_CTR_EP_IN_SW_BUFFER_SIZE);

  memcpy(_audiod_itf[itf].ep_int_ctr_buf, buffer, len);

  // Schedule transmit
  TU_VERIFY(usbd_edpt_xfer(_audiod_itf[itf].rhport, _audiod_itf[itf].ep_int_ctr, _audiod_itf[itf].ep_int_ctr_buf, len));

  return true;
}
#endif


// This function is called once a transmit of an audio packet was successfully completed. Here, we encode samples and place it in IN EP's buffer for next transmission.
// If you prefer your own (more efficient) implementation suiting your purpose set CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE = 0 and use tud_audio_n_write.

// n_bytes_copied - Informs caller how many bytes were loaded. In case n_bytes_copied = 0, a ZLP is scheduled to inform host no data is available for current frame.
#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE
static bool audiod_tx_done_cb(uint8_t rhport, audiod_interface_t * audio)
{
  uint8_t idxDriver, idxItf;
  uint8_t const *dummy2;

  // If a callback is used determine current alternate setting of
  if (tud_audio_tx_done_pre_load_cb || tud_audio_tx_done_post_load_cb)
  {
    // Find index of audio streaming interface and index of interface
    TU_VERIFY(audiod_get_AS_interface_index(audio->ep_in_as_intf_num, &idxDriver, &idxItf, &dummy2));
  }

  // Call a weak callback here - a possibility for user to get informed former TX was completed and data gets now loaded into EP in buffer (in case FIFOs are used) or
  // if no FIFOs are used the user may use this call back to load its data into the EP IN buffer by use of tud_audio_n_write_ep_in_buffer().
  if (tud_audio_tx_done_pre_load_cb) TU_VERIFY(tud_audio_tx_done_pre_load_cb(rhport, idxDriver, audio->ep_in, audio->altSetting[idxItf]));

  // Send everything in ISO EP FIFO
  uint16_t n_bytes_tx;

  // If support FIFOs are used, encode and schedule transmit
#if CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE && CFG_TUD_AUDIO_EPSIZE_IN
  switch (CFG_TUD_AUDIO_FORMAT_TYPE_TX)
  {
    case AUDIO_FORMAT_TYPE_UNDEFINED:
      // INDIVIDUAL ENCODING PROCEDURE REQUIRED HERE!
      TU_LOG2("  Desired CFG_TUD_AUDIO_FORMAT encoding not implemented!\r\n");
      TU_BREAKPOINT();
      break;

    case AUDIO_FORMAT_TYPE_I:

      switch (CFG_TUD_AUDIO_FORMAT_TYPE_I_TX)
      {
        case AUDIO_DATA_FORMAT_TYPE_I_PCM:

          n_bytes_tx = audiod_encode_type_I_pcm(rhport, audio);
          break;

        default:
          // YOUR ENCODING IS REQUIRED HERE!
          TU_LOG2("  Desired CFG_TUD_AUDIO_FORMAT_TYPE_I_TX encoding not implemented!\r\n");
          TU_BREAKPOINT();
          break;
      }
      break;

        default:
          // Desired CFG_TUD_AUDIO_FORMAT_TYPE_TX not implemented!
          TU_LOG2("  Desired CFG_TUD_AUDIO_FORMAT_TYPE_TX not implemented!\r\n");
          TU_BREAKPOINT();
          break;
  }

  TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_in, audio->lin_buf_in, n_bytes_tx));

#else
  // No support FIFOs, if no linear buffer required schedule transmit, else put data into linear buffer and schedule

  n_bytes_tx = tu_fifo_count(&audio->ep_in_ff);

  #if USE_LINEAR_BUFFER_TX
    tu_fifo_write_n(&audio->ep_in_ff, audio->lin_buf_in, n_bytes_tx);
    TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_in, audio->lin_buf_in, n_bytes_tx));
  #else
    // Send everything in ISO EP FIFO
    TU_VERIFY(usbd_edpt_iso_xfer(rhport, audio->ep_in, &audio->ep_in_ff, n_bytes_tx));
  #endif

#endif

  // Call a weak callback here - a possibility for user to get informed former TX was completed and how many bytes were loaded for the next frame
  if (tud_audio_tx_done_post_load_cb) TU_VERIFY(tud_audio_tx_done_post_load_cb(rhport, n_bytes_tx, idxDriver, audio->ep_in, audio->altSetting[idxItf]));

  return true;
}

#endif //CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE

#if CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE && CFG_TUD_AUDIO_EPSIZE_IN
// Take samples from the support buffer and encode them into the IN EP software FIFO
// Returns number of bytes written into linear buffer

/* 2.3.1.7.1 PCM Format
The PCM (Pulse Coded Modulation) format is the most commonly used audio format to represent audio
data streams. The audio data is not compressed and uses a signed two’s-complement fixed point format. It
is left-justified (the sign bit is the Msb) and data is padded with trailing zeros to fill the remaining unused
bits of the subslot. The binary point is located to the right of the sign bit so that all values lie within the
range [-1, +1)
*/

/*
 * This function encodes channels saved within the support FIFOs into one stream by interleaving the PCM samples
 * in the support FIFOs according to 2.3.1.5 Audio Streams. It does not control justification (left or right) and
 * does not change the number of bytes per sample.
 * */

static uint16_t audiod_encode_type_I_pcm(uint8_t rhport, audiod_interface_t* audio)
{
  // We encode directly into IN EP's linear buffer - abort if previous transfer not complete
  TU_VERIFY(!usbd_edpt_busy(rhport, audio->ep_in));

  // Determine amount of samples
  uint16_t const nChannelsPerFF         = CFG_TUD_AUDIO_N_CHANNELS_TX / CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO;
  uint16_t const nBytesToCopy           = nChannelsPerFF*CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX;
  uint16_t const capSamplesPerFF        = CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE / CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX / CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO;
  uint16_t nSamplesPerFFToSend          = tu_fifo_count(&audio->tx_supp_ff[0]);
  uint8_t cnt_ff;

  for (cnt_ff = 1; cnt_ff < CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO; cnt_ff++)
  {
    uint16_t const count = tu_fifo_count(&audio->tx_supp_ff[cnt_ff]);
    if (count < nSamplesPerFFToSend)
    {
      nSamplesPerFFToSend = count;
    }
  }

  // Check if there is enough
  if (nSamplesPerFFToSend == 0)    return 0;

  // Limit to maximum sample number - THIS IS A POSSIBLE ERROR SOURCE IF TOO MANY SAMPLE WOULD NEED TO BE SENT BUT CAN NOT!
  nSamplesPerFFToSend = tu_min16(nSamplesPerFFToSend, capSamplesPerFF);

  // Round to full number of samples (flooring)
  nSamplesPerFFToSend = (nSamplesPerFFToSend / nChannelsPerFF) * nChannelsPerFF;

  // Encode
  void * src;
  uint8_t * dst;
  uint8_t * src_end;
  uint16_t len;

  for (cnt_ff = 0; cnt_ff < CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO; cnt_ff++)
  {
    dst = &audio->lin_buf_in[cnt_ff*nChannelsPerFF*CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX];

    len = tu_fifo_get_linear_read_info(&audio->tx_supp_ff[cnt_ff], 0, &src, nSamplesPerFFToSend);
    tu_fifo_advance_read_pointer(&audio->tx_supp_ff[cnt_ff], len);

    src_end = src + len * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX;

    while((uint8_t *)src < src_end)
    {
      memcpy(dst, src, nBytesToCopy);
      src = (uint8_t *)src + nBytesToCopy;
      dst += nBytesToCopy * CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO;
    }

    // Handle wrapped part of FIFO
    if (len < nSamplesPerFFToSend)
    {
      len = tu_fifo_get_linear_read_info(&audio->tx_supp_ff[cnt_ff], 0, &src, nSamplesPerFFToSend - len);
      tu_fifo_advance_read_pointer(&audio->tx_supp_ff[cnt_ff], len);

      src_end = src + len * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX;

      while((uint8_t *)src < src_end)
      {
        memcpy(dst, src, nBytesToCopy);
        src = (uint8_t *)src + nBytesToCopy;
        dst += nBytesToCopy * CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO;
      }
    }
  }

  return nSamplesPerFFToSend * CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO;
}
#endif //CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE

// This function is called once a transmit of a feedback packet was successfully completed. Here, we get the next feedback value to be sent

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
static inline bool audiod_fb_send(uint8_t rhport, audiod_interface_t *audio)
{
  return usbd_edpt_xfer(rhport, audio->ep_fb, (uint8_t *) &audio->fb_val, 4);
}
#endif

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void audiod_init(void)
{
  tu_memclr(_audiod_itf, sizeof(_audiod_itf));

  for(uint8_t i=0; i<CFG_TUD_AUDIO; i++)
  {
    audiod_interface_t* audio = &_audiod_itf[i];

    // Initialize IN EP FIFO if required
#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE && !CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
    tu_fifo_config(&audio->ep_in_ff, &audio->ep_in_buf, CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE, 1, true);
#if CFG_FIFO_MUTEX
    tu_fifo_config_mutex(&audio->ep_in_ff, osal_mutex_create(&audio->ep_in_ff_mutex_wr), NULL);
#endif
#endif

    // Initialize OUT EP FIFO if required
#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE && !CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
    tu_fifo_config(&audio->ep_out_ff, &audio->ep_out_buf, CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE, 1, true);
#if CFG_FIFO_MUTEX
    tu_fifo_config_mutex(&audio->ep_out_ff, NULL, osal_mutex_create(&audio->ep_out_ff_mutex_rd));
#endif
#endif

    // Initialize TX support FIFOs if required
#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
    for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO; cnt++)
    {
      tu_fifo_config(&audio->tx_supp_ff[cnt], &audio->tx_supp_ff_buf[cnt], CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE, CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX, true);
#if CFG_FIFO_MUTEX
      tu_fifo_config_mutex(&audio->tx_supp_ff[cnt], osal_mutex_create(&audio->tx_supp_ff_mutex_wr[cnt]), NULL);
#endif
    }
#endif

    // Initialize RX support FIFOs if required
#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
    for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO; cnt++)
    {
      tu_fifo_config(&audio->rx_supp_ff[cnt], &audio->rx_supp_ff_buf[cnt], CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE, CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX, true);
#if CFG_FIFO_MUTEX
      tu_fifo_config_mutex(&audio->rx_supp_ff[cnt], NULL, osal_mutex_create(&audio->rx_supp_ff_mutex_rd[cnt]));
#endif
    }
#endif
  }
}

void audiod_reset(uint8_t rhport)
{
  (void) rhport;

  for(uint8_t i=0; i<CFG_TUD_AUDIO; i++)
  {
    audiod_interface_t* audio = &_audiod_itf[i];
    tu_memclr(audio, ITF_MEM_RESET_SIZE);

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE && !CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
    tu_fifo_clear(&audio->ep_in_ff);
#endif

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE && !CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
    tu_fifo_clear(&audio->ep_out_ff);
#endif

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE && CFG_TUD_AUDIO_TX_SUPPORT_SW_FIFO_SIZE
    for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO_N_TX_SUPPORT_SW_FIFO; cnt++)
    {
      tu_fifo_clear(&audio->tx_supp_ff[cnt]);
    }
#endif

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE && CFG_TUD_AUDIO_RX_SUPPORT_SW_FIFO_SIZE
    for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO_N_RX_SUPPORT_SW_FIFO; cnt++)
    {
      tu_fifo_clear(&audio->rx_supp_ff[cnt]);
    }
#endif
  }
}

uint16_t audiod_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  (void) max_len;

  TU_VERIFY ( TUSB_CLASS_AUDIO  == itf_desc->bInterfaceClass &&
              AUDIO_SUBCLASS_CONTROL    == itf_desc->bInterfaceSubClass);

  // Verify version is correct - this check can be omitted
  TU_VERIFY(itf_desc->bInterfaceProtocol == AUDIO_INT_PROTOCOL_CODE_V2);

  // Verify interrupt control EP is enabled if demanded by descriptor - this should be best some static check however - this check can be omitted
  if (itf_desc->bNumEndpoints == 1) // 0 or 1 EPs are allowed
  {
    TU_VERIFY(CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN > 0);
  }

  // Alternate setting MUST be zero - this check can be omitted
  TU_VERIFY(itf_desc->bAlternateSetting == 0);

  // Find available audio driver interface
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++)
  {
    if (!_audiod_itf[i].p_desc)
    {
      _audiod_itf[i].p_desc = (uint8_t const *)itf_desc;    // Save pointer to AC descriptor which is by specification always the first one
      _audiod_itf[i].rhport = rhport;
      break;
    }
  }

  // Verify we found a free one
  TU_ASSERT( i < CFG_TUD_AUDIO );

  // This is all we need so far - the EPs are setup by a later set_interface request (as per UAC2 specification)
  uint16_t drv_len = tud_audio_desc_lengths[i] - TUD_AUDIO_DESC_IAD_LEN;    // - TUD_AUDIO_DESC_IAD_LEN since tinyUSB already handles the IAD descriptor

  return drv_len;
}

static bool audiod_get_interface(uint8_t rhport, tusb_control_request_t const * p_request)
{
#if CFG_TUD_AUDIO_N_AS_INT > 0
  uint8_t const itf = tu_u16_low(p_request->wIndex);

  // Find index of audio streaming interface
  uint8_t idxDriver, idxItf;
  uint8_t const *dummy;

  TU_VERIFY(audiod_get_AS_interface_index(itf, &idxDriver, &idxItf, &dummy));
  TU_VERIFY(tud_control_xfer(rhport, p_request, &_audiod_itf[idxDriver].altSetting[idxItf], 1));

  TU_LOG2("  Get itf: %u - current alt: %u\r\n", itf, _audiod_itf[idxDriver].altSetting[idxItf]);

  return true;

#else
  (void) rhport;
  (void) p_request;
  return false;
#endif
}

static bool audiod_set_interface(uint8_t rhport, tusb_control_request_t const * p_request)
{
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
  uint8_t idxDriver, idxItf;
  uint8_t const *p_desc;
  TU_VERIFY(audiod_get_AS_interface_index(itf, &idxDriver, &idxItf, &p_desc));

  // Look if there is an EP to be closed - for this driver, there are only 3 possible EPs which may be closed (only AS related EPs can be closed, AC EP (if present) is always open)
#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE
  if (_audiod_itf[idxDriver].ep_in_as_intf_num == itf)
  {
    _audiod_itf[idxDriver].ep_in_as_intf_num = 0;
    usbd_edpt_close(rhport, _audiod_itf[idxDriver].ep_in);

    // Invoke callback - can be used to stop data sampling
    if (tud_audio_set_itf_close_EP_cb) TU_VERIFY(tud_audio_set_itf_close_EP_cb(rhport, p_request));

    _audiod_itf[idxDriver].ep_in = 0;                           // Necessary?
  }
#endif

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE
  if (_audiod_itf[idxDriver].ep_out_as_intf_num == itf)
  {
    _audiod_itf[idxDriver].ep_out_as_intf_num = 0;
    usbd_edpt_close(rhport, _audiod_itf[idxDriver].ep_out);
    _audiod_itf[idxDriver].ep_out = 0;                          // Necessary?

    // Close corresponding feedback EP
#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
    usbd_edpt_close(rhport, _audiod_itf[idxDriver].ep_fb);
    _audiod_itf[idxDriver].ep_fb = 0;                           // Necessary?
#endif
  }
#endif

  // Save current alternative interface setting
  _audiod_itf[idxDriver].altSetting[idxItf] = alt;

  // Open new EP if necessary - EPs are only to be closed or opened for AS interfaces - Look for AS interface with correct alternate interface
  // Get pointer at end
  uint8_t const *p_desc_end = _audiod_itf[idxDriver].p_desc + tud_audio_desc_lengths[idxDriver] - TUD_AUDIO_DESC_IAD_LEN;

  // p_desc starts at required interface with alternate setting zero
  while (p_desc < p_desc_end)
  {
    // Find correct interface
    if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE && ((tusb_desc_interface_t const * )p_desc)->bInterfaceNumber == itf && ((tusb_desc_interface_t const * )p_desc)->bAlternateSetting == alt)
    {
      // From this point forward follow the EP descriptors associated to the current alternate setting interface - Open EPs if necessary
      uint8_t foundEPs = 0, nEps = ((tusb_desc_interface_t const * )p_desc)->bNumEndpoints;
      while (foundEPs < nEps && p_desc < p_desc_end)
      {
        if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT)
        {
          TU_ASSERT(usbd_edpt_open(rhport, (tusb_desc_endpoint_t const *)p_desc));

          uint8_t ep_addr = ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress;

          //TODO: We need to set EP non busy since this is not taken care of right now in ep_close() - THIS IS A WORKAROUND!
          usbd_edpt_clear_stall(rhport, ep_addr);

#if CFG_TUD_AUDIO_EPSIZE_IN
          if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN && ((tusb_desc_endpoint_t const *) p_desc)->bmAttributes.usage == 0x00)   // Check if usage is data EP
          {
            // Save address
            _audiod_itf[idxDriver].ep_in = ep_addr;
            _audiod_itf[idxDriver].ep_in_as_intf_num = itf;

            // Invoke callback - can be used to trigger data sampling if not already running
            if (tud_audio_set_itf_cb) TU_VERIFY(tud_audio_set_itf_cb(rhport, p_request));

            // Schedule first transmit - in case no sample data is available a ZLP is loaded
            // It is necessary to trigger this here since the refill is done with an TX FIFO empty interrupt which can only trigger if something was in there
            TU_VERIFY(audiod_tx_done_cb(rhport, &_audiod_itf[idxDriver]));
          }
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT

          if (tu_edpt_dir(ep_addr) == TUSB_DIR_OUT)     // Checking usage not necessary
          {
            // Save address
            _audiod_itf[idxDriver].ep_out = ep_addr;
            _audiod_itf[idxDriver].ep_out_as_intf_num = itf;

            // Invoke callback
            if (tud_audio_set_itf_cb) TU_VERIFY(tud_audio_set_itf_cb(rhport, p_request));

            // Prepare for incoming data
#if USE_LINEAR_BUFFER_RX
            TU_VERIFY(usbd_edpt_xfer(rhport, _audiod_itf[idxDriver].ep_out, _audiod_itf[idxDriver].lin_buf_out, CFG_TUD_AUDIO_EPSIZE_OUT), false);
#else
            TU_VERIFY(usbd_edpt_iso_xfer(rhport, _audiod_itf[idxDriver].ep_out, &_audiod_itf[idxDriver].ep_out_ff, CFG_TUD_AUDIO_EPSIZE_OUT), false);
#endif
          }

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
          if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN && ((tusb_desc_endpoint_t const *) p_desc)->bmAttributes.usage == 1)   // Check if usage is explicit data feedback
          {
            _audiod_itf[idxDriver].ep_fb = ep_addr;

            // Invoke callback
            if (tud_audio_set_itf_cb) TU_VERIFY(tud_audio_set_itf_cb(rhport, p_request));
          }
#endif

#endif
          foundEPs += 1;
        }
        p_desc = tu_desc_next(p_desc);
      }

      TU_VERIFY(foundEPs == nEps);

      // We are done - abort loop
      break;
    }

    // Moving forward
    p_desc = tu_desc_next(p_desc);
  }

  tud_control_status(rhport, p_request);

  return true;
}

// Invoked when class request DATA stage is finished.
// return false to stall control EP (e.g Host send non-sense DATA)
static bool audiod_control_complete(uint8_t rhport, tusb_control_request_t const * p_request)
{
  // Handle audio class specific set requests
  if(p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS && p_request->bmRequestType_bit.direction == TUSB_DIR_OUT)
  {
    uint8_t idxDriver;

    switch (p_request->bmRequestType_bit.recipient)
    {
      case TUSB_REQ_RCPT_INTERFACE: ;       // The semicolon is there to enable a declaration right after the label

      uint8_t itf = TU_U16_LOW(p_request->wIndex);
      uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

      if (entityID != 0)
      {
        if (tud_audio_set_req_entity_cb)
        {
          // Check if entity is present and get corresponding driver index
          TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &idxDriver));

          // Invoke callback
          return tud_audio_set_req_entity_cb(rhport, p_request, _audiod_itf[idxDriver].ctrl_buf);
        }
        else
        {
          TU_LOG2("  No entity set request callback available!\r\n");
          return false;     // In case no callback function is present or request can not be conducted we stall it
        }
      }
      else
      {
        if (tud_audio_set_req_itf_cb)
        {
          // Find index of audio driver structure and verify interface really exists
          TU_VERIFY(audiod_verify_itf_exists(itf, &idxDriver));

          // Invoke callback
          return tud_audio_set_req_itf_cb(rhport, p_request, _audiod_itf[idxDriver].ctrl_buf);
        }
        else
        {
          TU_LOG2("  No interface set request callback available!\r\n");
          return false;     // In case no callback function is present or request can not be conducted we stall it
        }
      }

      break;

      case TUSB_REQ_RCPT_ENDPOINT: ;        // The semicolon is there to enable a declaration right after the label

      uint8_t ep = TU_U16_LOW(p_request->wIndex);

      if (tud_audio_set_req_ep_cb)
      {
        // Check if entity is present and get corresponding driver index
        TU_VERIFY(audiod_verify_ep_exists(ep, &idxDriver));

        // Invoke callback
        return tud_audio_set_req_ep_cb(rhport, p_request, _audiod_itf[idxDriver].ctrl_buf);
      }
      else
      {
        TU_LOG2("  No EP set request callback available!\r\n");
        return false;   // In case no callback function is present or request can not be conducted we stall it
      }

      // Unknown/Unsupported recipient
      default: TU_BREAKPOINT(); return false;
    }
  }
  return true;
}

// Handle class control request
// return false to stall control endpoint (e.g unsupported request)
static bool audiod_control_request(uint8_t rhport, tusb_control_request_t const * p_request)
{
  (void) rhport;

  // Handle standard requests - standard set requests usually have no data stage so we also handle set requests here
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD)
  {
    switch (p_request->bRequest)
    {
      case TUSB_REQ_GET_INTERFACE:
        return audiod_get_interface(rhport, p_request);

      case TUSB_REQ_SET_INTERFACE:
        return audiod_set_interface(rhport, p_request);

        // Unknown/Unsupported request
      default: TU_BREAKPOINT(); return false;
    }
  }

  // Handle class requests
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS)
  {
    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t idxDriver;

    // Conduct checks which depend on the recipient
    switch (p_request->bmRequestType_bit.recipient)
    {
      case TUSB_REQ_RCPT_INTERFACE: ;       // The semicolon is there to enable a declaration right after the label

      uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

      // Verify if entity is present
      if (entityID != 0)
      {
        // Find index of audio driver structure and verify entity really exists
        TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &idxDriver));

        // In case we got a get request invoke callback - callback needs to answer as defined in UAC2 specification page 89 - 5. Requests
        if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN)
        {
          if (tud_audio_get_req_entity_cb)
          {
            return tud_audio_get_req_entity_cb(rhport, p_request);
          }
          else
          {
            TU_LOG2("  No entity get request callback available!\r\n");
            return false;   // Stall
          }
        }
      }
      else
      {
        // Find index of audio driver structure and verify interface really exists
        TU_VERIFY(audiod_verify_itf_exists(itf, &idxDriver));

        // In case we got a get request invoke callback - callback needs to answer as defined in UAC2 specification page 89 - 5. Requests
        if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN)
        {
          if (tud_audio_get_req_itf_cb)
          {
            return tud_audio_get_req_itf_cb(rhport, p_request);
          }
          else
          {
            TU_LOG2("  No interface get request callback available!\r\n");
            return false;   // Stall
          }
        }
      }
      break;

      case TUSB_REQ_RCPT_ENDPOINT: ;        // The semicolon is there to enable a declaration right after the label

      uint8_t ep = TU_U16_LOW(p_request->wIndex);

      // Find index of audio driver structure and verify EP really exists
      TU_VERIFY(audiod_verify_ep_exists(ep, &idxDriver));

      // In case we got a get request invoke callback - callback needs to answer as defined in UAC2 specification page 89 - 5. Requests
      if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN)
      {
        if (tud_audio_get_req_ep_cb)
        {
          return tud_audio_get_req_ep_cb(rhport, p_request);
        }
        else
        {
          TU_LOG2("  No EP get request callback available!\r\n");
          return false;     // Stall
        }
      }
      break;

      // Unknown/Unsupported recipient
      default: TU_LOG2("  Unsupported recipient: %d\r\n", p_request->bmRequestType_bit.recipient); TU_BREAKPOINT(); return false;
    }

    // If we end here, the received request is a set request - we schedule a receive for the data stage and return true here. We handle the rest later in audiod_control_complete() once the data stage was finished
    TU_VERIFY(tud_control_xfer(rhport, p_request, _audiod_itf[idxDriver].ctrl_buf, CFG_TUD_AUDIO_CTRL_BUF_SIZE));
    return true;
  }

  // There went something wrong - unsupported control request type
  TU_BREAKPOINT();
  return false;
}

bool audiod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  if ( stage == CONTROL_STAGE_SETUP )
  {
    return audiod_control_request(rhport, request);
  }
  else if ( stage == CONTROL_STAGE_DATA )
  {
    return audiod_control_complete(rhport, request);
  }

  return true;
}

bool audiod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;
  (void) xferred_bytes;

  // Search for interface belonging to given end point address and proceed as required
  uint8_t idxDriver;
  for (idxDriver = 0; idxDriver < CFG_TUD_AUDIO; idxDriver++)
  {

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN

    // Data transmission of control interrupt finished
    if (_audiod_itf[idxDriver].ep_int_ctr == ep_addr)
    {
      // According to USB2 specification, maximum payload of interrupt EP is 8 bytes on low speed, 64 bytes on full speed, and 1024 bytes on high speed (but only if an alternate interface other than 0 is used - see specification p. 49)
      // In case there is nothing to send we have to return a NAK - this is taken care of by PHY ???
      // In case of an erroneous transmission a retransmission is conducted - this is taken care of by PHY ???

      // I assume here, that things above are handled by PHY
      // All transmission is done - what remains to do is to inform job was completed

      if (tud_audio_int_ctr_done_cb) TU_VERIFY(tud_audio_int_ctr_done_cb(rhport, (uint16_t) xferred_bytes));
    }

#endif

#if CFG_TUD_AUDIO_EP_IN_SW_BUFFER_SIZE

    // Data transmission of audio packet finished
    if (_audiod_itf[idxDriver].ep_in == ep_addr)
    {
      // USB 2.0, section 5.6.4, third paragraph, states "An isochronous endpoint must specify its required bus access period. However, an isochronous endpoint must be prepared to handle poll rates faster than the one specified."
      // That paragraph goes on to say "An isochronous IN endpoint must return a zero-length packet whenever data is requested at a faster interval than the specified interval and data is not available."
      // This can only be solved reliably if we load a ZLP after every IN transmission since we can not say if the host requests samples earlier than we declared! Once all samples are collected we overwrite the loaded ZLP.

      // Check if there is data to load into EPs buffer - if not load it with ZLP
      // Be aware - we as a device are not able to know if the host polls for data with a faster rate as we stated this in the descriptors. Therefore we always have to put something into the EPs buffer. However, once we did that, there is no way of aborting this or replacing what we put into the buffer before!
      // This is the only place where we can fill something into the EPs buffer!

      // Load new data
      TU_VERIFY(audiod_tx_done_cb(rhport, &_audiod_itf[idxDriver]));

      // Transmission of ZLP is done by audiod_tx_done_cb()
      return true;
    }
#endif

#if CFG_TUD_AUDIO_EP_OUT_SW_BUFFER_SIZE

    // New audio packet received
    if (_audiod_itf[idxDriver].ep_out == ep_addr)
    {
      TU_VERIFY(audiod_rx_done_cb(rhport, &_audiod_itf[idxDriver], (uint16_t) xferred_bytes));
      return true;
    }


#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
    // Transmission of feedback EP finished
    if (_audiod_itf[idxDriver].ep_fb == ep_addr)
    {
      if (tud_audio_fb_done_cb) TU_VERIFY(tud_audio_fb_done_cb(rhport));

      // Schedule next transmission - value is changed bytud_audio_n_fb_set() in the meantime or the old value gets sent
      return audiod_fb_send(rhport, &_audiod_itf[idxDriver]);
    }
#endif
#endif
  }

  return false;
}

bool tud_audio_buffer_and_schedule_control_xfer(uint8_t rhport, tusb_control_request_t const * p_request, void* data, uint16_t len)
{
  // Handles only sending of data not receiving
  if (p_request->bmRequestType_bit.direction == TUSB_DIR_OUT) return false;

  // Get corresponding driver index
  uint8_t idxDriver;
  uint8_t itf = TU_U16_LOW(p_request->wIndex);

  // Conduct checks which depend on the recipient
  switch (p_request->bmRequestType_bit.recipient)
  {
    case TUSB_REQ_RCPT_INTERFACE: ;     // The semicolon is there to enable a declaration right after the label

    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    // Verify if entity is present
    if (entityID != 0)
    {
      // Find index of audio driver structure and verify entity really exists
      TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &idxDriver));
    }
    else
    {
      // Find index of audio driver structure and verify interface really exists
      TU_VERIFY(audiod_verify_itf_exists(itf, &idxDriver));
    }
    break;

    case TUSB_REQ_RCPT_ENDPOINT: ;      // The semicolon is there to enable a declaration right after the label

    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    // Find index of audio driver structure and verify EP really exists
    TU_VERIFY(audiod_verify_ep_exists(ep, &idxDriver));
    break;

    // Unknown/Unsupported recipient
    default: TU_LOG2("  Unsupported recipient: %d\r\n", p_request->bmRequestType_bit.recipient); TU_BREAKPOINT(); return false;
  }

  // Crop length
  if (len > CFG_TUD_AUDIO_CTRL_BUF_SIZE) len = CFG_TUD_AUDIO_CTRL_BUF_SIZE;

  // Copy into buffer
  memcpy((void *)_audiod_itf[idxDriver].ctrl_buf, data, (size_t)len);

  // Schedule transmit
  return tud_control_xfer(rhport, p_request, (void*)_audiod_itf[idxDriver].ctrl_buf, len);
}

// This helper function finds for a given AS interface number the index of the attached driver structure, the index of the interface in the audio function
// (e.g. the std. AS interface with interface number 15 is the first AS interface for the given audio function and thus gets index zero), and
// finally a pointer to the std. AS interface, where the pointer always points to the first alternate setting i.e. alternate interface zero.
static bool audiod_get_AS_interface_index(uint8_t itf, uint8_t *idxDriver, uint8_t *idxItf, uint8_t const **pp_desc_int)
{
  // Loop over audio driver interfaces
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++)
  {
    if (_audiod_itf[i].p_desc)
    {
      // Get pointer at end
      uint8_t const *p_desc_end = _audiod_itf[i].p_desc + tud_audio_desc_lengths[i] - TUD_AUDIO_DESC_IAD_LEN;

      // Advance past AC descriptors
      uint8_t const *p_desc = tu_desc_next(_audiod_itf[i].p_desc);
      p_desc += ((audio_desc_cs_ac_interface_t const *)p_desc)->wTotalLength;

      uint8_t tmp = 0;
      while (p_desc < p_desc_end)
      {
        // We assume the number of alternate settings is increasing thus we return the index of alternate setting zero!
        if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE && ((tusb_desc_interface_t const * )p_desc)->bInterfaceNumber == itf)
        {
          *idxItf = tmp;
          *idxDriver = i;
          *pp_desc_int = p_desc;
          return true;
        }

        // Increase index, bytes read, and pointer
        tmp++;
        p_desc = tu_desc_next(p_desc);
      }
    }
  }

  return false;
}

// Verify an entity with the given ID exists and returns also the corresponding driver index
static bool audiod_verify_entity_exists(uint8_t itf, uint8_t entityID, uint8_t *idxDriver)
{
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++)
  {
    // Look for the correct driver by checking if the unique standard AC interface number fits
    if (_audiod_itf[i].p_desc && ((tusb_desc_interface_t const *)_audiod_itf[i].p_desc)->bInterfaceNumber == itf)
    {
      // Get pointers after class specific AC descriptors and end of AC descriptors - entities are defined in between
      uint8_t const *p_desc = tu_desc_next(_audiod_itf[i].p_desc);                                          // Points to CS AC descriptor
      uint8_t const *p_desc_end = ((audio_desc_cs_ac_interface_t const *)p_desc)->wTotalLength + p_desc;
      p_desc = tu_desc_next(p_desc);                                                                            // Get past CS AC descriptor

      while (p_desc < p_desc_end)
      {
        if (p_desc[3] == entityID)  // Entity IDs are always at offset 3
        {
          *idxDriver = i;
          return true;
        }
        p_desc = tu_desc_next(p_desc);
      }
    }
  }
  return false;
}

static bool audiod_verify_itf_exists(uint8_t itf, uint8_t *idxDriver)
{
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++)
  {
    if (_audiod_itf[i].p_desc)
    {
      // Get pointer at beginning and end
      uint8_t const *p_desc = _audiod_itf[i].p_desc;
      uint8_t const *p_desc_end = _audiod_itf[i].p_desc + tud_audio_desc_lengths[i] - TUD_AUDIO_DESC_IAD_LEN;

      while (p_desc < p_desc_end)
      {
        if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE && ((tusb_desc_interface_t const *)_audiod_itf[i].p_desc)->bInterfaceNumber == itf)
        {
          *idxDriver = i;
          return true;
        }
        p_desc = tu_desc_next(p_desc);
      }
    }
  }
  return false;
}

static bool audiod_verify_ep_exists(uint8_t ep, uint8_t *idxDriver)
{
  uint8_t i;
  for (i = 0; i < CFG_TUD_AUDIO; i++)
  {
    if (_audiod_itf[i].p_desc)
    {
      // Get pointer at end
      uint8_t const *p_desc_end = _audiod_itf[i].p_desc + tud_audio_desc_lengths[i];

      // Advance past AC descriptors - EP we look for are streaming EPs
      uint8_t const *p_desc = tu_desc_next(_audiod_itf[i].p_desc);
      p_desc += ((audio_desc_cs_ac_interface_t const *)p_desc)->wTotalLength;

      while (p_desc < p_desc_end)
      {
        if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT && ((tusb_desc_endpoint_t const * )p_desc)->bEndpointAddress == ep)
        {
          *idxDriver = i;
          return true;
        }
        p_desc = tu_desc_next(p_desc);
      }
    }
  }
  return false;
}

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP

// Input value feedback has to be in 16.16 format - the format will be converted according to speed settings automatically
bool tud_audio_n_fb_set(uint8_t itf, uint32_t feedback)
{
  TU_VERIFY(itf < CFG_TUD_AUDIO && _audiod_itf[itf].p_desc != NULL);

  // Format the feedback value
  if (_audiod_itf[itf].rhport == 0)
  {
    uint8_t * fb = (uint8_t *) &_audiod_itf[itf].fb_val;

    // For FS format is 10.14
    *(fb++) = (feedback >> 2) & 0xFF;
    *(fb++) = (feedback >> 10) & 0xFF;
    *(fb++) = (feedback >> 18) & 0xFF;
    // 4th byte is needed to work correctly with MS Windows
    *fb = 0;
  }
  else
  {
    // For HS format is 16.16 as originally demanded
    _audiod_itf[itf].fb_val = feedback;
  }

  // Schedule a transmit with the new value if EP is not busy - this triggers repetitive scheduling of the feedback value
  if (!usbd_edpt_busy(_audiod_itf[itf].rhport, _audiod_itf[itf].ep_fb))
  {
    return audiod_fb_send(_audiod_itf[itf].rhport, &_audiod_itf[itf]);
  }

  return true;
}
#endif

#endif //TUSB_OPT_DEVICE_ENABLED && CFG_TUD_AUDIO
