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

#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_FIFO_SIZE
#ifndef CFG_TUD_AUDIO_TX_FIFO_COUNT
#define CFG_TUD_AUDIO_TX_FIFO_COUNT CFG_TUD_AUDIO_N_CHANNELS_TX
#endif
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_FIFO_SIZE
#ifndef CFG_TUD_AUDIO_RX_FIFO_COUNT
#define CFG_TUD_AUDIO_RX_FIFO_COUNT CFG_TUD_AUDIO_N_CHANNELS_RX
#endif
#endif

typedef struct
{
  uint8_t rhport;
  uint8_t const * p_desc;       // Pointer pointing to Standard AC Interface Descriptor(4.7.1) - Audio Control descriptor defining audio function

#if CFG_TUD_AUDIO_EPSIZE_IN
  uint8_t ep_in;                // Outgoing (out of uC) audio data EP.
  uint16_t epin_buf_cnt;        // Count filling status of EP in buffer - this is a shared state currently and is intended to be removed once EP buffers can be implemented as FIFOs!
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

  // FIFO
#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_FIFO_SIZE
  tu_fifo_t tx_ff[CFG_TUD_AUDIO_TX_FIFO_COUNT];
  CFG_TUSB_MEM_ALIGN uint8_t tx_ff_buf[CFG_TUD_AUDIO_TX_FIFO_COUNT][CFG_TUD_AUDIO_TX_FIFO_SIZE];
#if CFG_FIFO_MUTEX
  osal_mutex_def_t tx_ff_mutex[CFG_TUD_AUDIO_TX_FIFO_COUNT];
#endif
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_FIFO_SIZE
  tu_fifo_t rx_ff[CFG_TUD_AUDIO_RX_FIFO_COUNT];
  CFG_TUSB_MEM_ALIGN uint8_t rx_ff_buf[CFG_TUD_AUDIO_RX_FIFO_COUNT][CFG_TUD_AUDIO_RX_FIFO_SIZE];
#if CFG_FIFO_MUTEX
  osal_mutex_def_t rx_ff_mutex[CFG_TUD_AUDIO_RX_FIFO_COUNT];
#endif
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
  tu_fifo_t int_ctr_ff;
  CFG_TUSB_MEM_ALIGN uint8_t int_ctr_ff_buf[CFG_TUD_AUDIO_INT_CTR_BUFSIZE];
#if CFG_FIFO_MUTEX
  osal_mutex_def_t int_ctr_ff_mutex;
#endif
#endif

  // Endpoint Transfer buffers
#if CFG_TUD_AUDIO_EPSIZE_OUT
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_AUDIO_EPSIZE_OUT];        // Bigger makes no sense for isochronous EP's (but technically possible here)

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  uint32_t fb_val;                                                       // Feedback value for asynchronous mode (in 16.16 format).
#endif

#endif

#if CFG_TUD_AUDIO_EPSIZE_IN
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_AUDIO_EPSIZE_IN];         // Bigger makes no sense for isochronous EP's (but technically possible here)
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
  CFG_TUSB_MEM_ALIGN uint8_t ep_int_ctr_buf[CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN];
#endif

} audiod_interface_t;

#define ITF_MEM_RESET_SIZE   offsetof(audiod_interface_t, ctrl_buf)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION audiod_interface_t _audiod_itf[CFG_TUD_AUDIO];

extern const uint16_t tud_audio_desc_lengths[];

#if CFG_TUD_AUDIO_EPSIZE_OUT
static bool audio_rx_done_type_I_pcm_ff_cb(uint8_t rhport, audiod_interface_t* audio, uint8_t * buffer, uint16_t bufsize);
#endif

#if CFG_TUD_AUDIO_EPSIZE_IN
static bool audiod_tx_done_type_I_pcm_ff_cb(uint8_t rhport, audiod_interface_t* audio);
#endif

static bool audiod_get_interface(uint8_t rhport, tusb_control_request_t const * p_request);
static bool audiod_set_interface(uint8_t rhport, tusb_control_request_t const * p_request);

static bool audiod_get_AS_interface_index(uint8_t itf, uint8_t *idxDriver, uint8_t *idxItf, uint8_t const **pp_desc_int);
static bool audiod_verify_entity_exists(uint8_t itf, uint8_t entityID, uint8_t *idxDriver);
static bool audiod_verify_itf_exists(uint8_t itf, uint8_t *idxDriver);
static bool audiod_verify_ep_exists(uint8_t ep, uint8_t *idxDriver);

bool tud_audio_n_mounted(uint8_t itf)
{
  audiod_interface_t* audio = &_audiod_itf[itf];

#if CFG_TUD_AUDIO_EPSIZE_OUT
  if (audio->ep_out == 0)
  {
    return false;
  }
#endif

#if CFG_TUD_AUDIO_EPSIZE_IN
  if (audio->ep_in == 0)
  {
    return false;
  }
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
  if (audio->ep_int_ctr == 0)
  {
    return false;
  }
#endif

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
  if (audio->ep_fb == 0)
  {
    return false;
  }
#endif

  return true;
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_FIFO_SIZE
#if CFG_TUD_AUDIO_RX_FIFO_COUNT > 1
uint16_t tud_audio_n_available(uint8_t itf, uint8_t channelId)
{
  TU_VERIFY(channelId < CFG_TUD_AUDIO_N_CHANNELS_RX);
  return tu_fifo_count(&_audiod_itf[itf].rx_ff[channelId]);
}

uint16_t tud_audio_n_read(uint8_t itf, uint8_t channelId, void* buffer, uint16_t bufsize)
{
  TU_VERIFY(channelId < CFG_TUD_AUDIO_N_CHANNELS_RX);
  return tu_fifo_read_n(&_audiod_itf[itf].rx_ff[channelId], buffer, bufsize);
}

void tud_audio_n_read_flush (uint8_t itf, uint8_t channelId)
{
  TU_VERIFY(channelId < CFG_TUD_AUDIO_N_CHANNELS_RX, );
  tu_fifo_clear(&_audiod_itf[itf].rx_ff[channelId]);
}
#else
uint16_t tud_audio_n_available(uint8_t itf)
{
  return tu_fifo_count(&_audiod_itf[itf].rx_ff[0]);
}

uint16_t tud_audio_n_read(uint8_t itf, void* buffer, uint16_t bufsize)
{
  return tu_fifo_read_n(&_audiod_itf[itf].rx_ff[0], buffer, bufsize);
}

void tud_audio_n_read_flush (uint8_t itf)
{
  tu_fifo_clear(&_audiod_itf[itf].rx_ff[0]);
}
#endif
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN

uint16_t tud_audio_int_ctr_n_available(uint8_t itf)
{
  return tu_fifo_count(&_audiod_itf[itf].int_ctr_ff);
}

uint16_t tud_audio_int_ctr_n_read(uint8_t itf, void* buffer, uint16_t bufsize)
{
  return tu_fifo_read_n(&_audiod_itf[itf].int_ctr_ff, buffer, bufsize);
}

void tud_audio_int_ctr_n_read_flush (uint8_t itf)
{
  tu_fifo_clear(&_audiod_itf[itf].int_ctr_ff);
}

#endif

// This function is called once something is received by USB and is responsible for decoding received stream into audio channels.
// If you prefer your own (more efficient) implementation suiting your purpose set CFG_TUD_AUDIO_RX_FIFO_SIZE = 0.

#if CFG_TUD_AUDIO_EPSIZE_OUT

static bool audio_rx_done_cb(uint8_t rhport, audiod_interface_t* audio, uint8_t* buffer, uint16_t bufsize)
{
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

#if CFG_TUD_AUDIO_RX_FIFO_SIZE
          TU_VERIFY(audio_rx_done_type_I_pcm_ff_cb(rhport, audio, buffer, bufsize));
#else
#error YOUR DECODING AND BUFFERING IS REQUIRED HERE!
#endif
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

  // Call a weak callback here - a possibility for user to get informed RX was completed
  if (tud_audio_rx_done_cb) TU_VERIFY(tud_audio_rx_done_cb(rhport, buffer, bufsize));

  return true;
}

#endif //CFG_TUD_AUDIO_EPSIZE_OUT

// The following functions are used in case CFG_TUD_AUDIO_RX_FIFO_SIZE != 0
#if CFG_TUD_AUDIO_RX_FIFO_SIZE
#if CFG_TUD_AUDIO_RX_FIFO_COUNT > 1
static bool audio_rx_done_type_I_pcm_ff_cb(uint8_t rhport, audiod_interface_t* audio, uint8_t * buffer, uint16_t bufsize)
{
  (void) rhport;

  // We expect to get a multiple of CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX * CFG_TUD_AUDIO_N_CHANNELS_RX per channel
  if (bufsize % (CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX * CFG_TUD_AUDIO_N_CHANNELS_RX) != 0)
  {
    return false;
  }

  uint8_t chId = 0;
  uint16_t cnt;
#if CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX == 1
  uint8_t sample = 0;
#elif CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX == 2
  uint16_t sample = 0;
#else
  uint32_t sample = 0;
#endif

  for(cnt = 0; cnt < bufsize; cnt += CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX)
  {
    // Let alignment problems be handled by memcpy
    memcpy(&sample, &buffer[cnt], CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX);
    if(tu_fifo_write_n(&audio->rx_ff[chId++], &sample, CFG_TUD_AUDIO_RX_ITEMSIZE) != CFG_TUD_AUDIO_RX_ITEMSIZE)
    {
      // Buffer overflow
      return false;
    }

    if (chId == CFG_TUD_AUDIO_N_CHANNELS_RX)
    {
      chId = 0;
    }
  }
  return true;
}
#else
static bool audio_rx_done_type_I_pcm_ff_cb(uint8_t rhport, audiod_interface_t *audio, uint8_t *buffer, uint16_t bufsize)
{
  (void) rhport;

  // We expect to get a multiple of CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX * CFG_TUD_AUDIO_N_CHANNELS_RX per channel
  if (bufsize % (CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX * CFG_TUD_AUDIO_N_CHANNELS_RX) != 0)
  {
    return false;
  }

  tu_fifo_write_n(&audio->rx_ff[0], buffer, bufsize);
  return true;
}
#endif // CFG_TUD_AUDIO_RX_FIFO_COUNT > 1
#endif //CFG_TUD_AUDIO_RX_FIFO_SIZE

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+

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
#if CFG_TUD_AUDIO_EPSIZE_IN
#if !CFG_TUD_AUDIO_TX_FIFO_SIZE
/*  This function is intended for later use once EP buffers (at least for ISO EPs) are implemented as ring buffers
uint16_t tud_audio_n_write_ep_in_buffer(uint8_t itf, const void * data, uint16_t len)
{
  audiod_interface_t* audio = &_audiod_itf[itf];
  if (audio->p_desc == NULL) {
    return 0;
  }

  // THIS IS A CRITICAL SECTION - audio->epin_buf_cnt MUST NOT BE MODIFIED FROM HERE - happens if audiod_tx_done_cb() is executed in between!

  // FOR SINGLE THREADED OPERATION:
  // AS LONG AS THIS FUNCTION IS NOT EXECUTED WITHIN AN INTERRUPT ALL IS FINE!

  // Determine free space
  uint16_t free = CFG_TUD_AUDIO_EPSIZE_IN - audio->epin_buf_cnt;

  // Clip length if needed
  if (len > free) len = free;

  // Write data
  memcpy((void *) &audio->epin_buf[audio->epin_buf_cnt], data, len);

  audio->epin_buf_cnt += len;

  // Return number of bytes written
  return len;
}
*/

#else

#if CFG_TUD_AUDIO_TX_FIFO_COUNT == 1
uint16_t tud_audio_n_write(uint8_t itf, void const* data, uint16_t len)
{
  {
    audiod_interface_t* audio = &_audiod_itf[itf];
    if (audio->p_desc == NULL)
    {
      return 0;
    }
    return tu_fifo_write_n(&audio->tx_ff[0], data, len);
  }
}
#else
uint16_t tud_audio_n_write(uint8_t itf, uint8_t channelId, const void * data, uint16_t len)
{
  audiod_interface_t* audio = &_audiod_itf[itf];
  if (audio->p_desc == NULL) {
    return 0;
  }

  return tu_fifo_write_n(&audio->tx_ff[channelId], data, len);
}
#endif

static bool audiod_tx_done_cb(uint8_t rhport, audiod_interface_t* audio, uint16_t * n_bytes_copied);

uint16_t tud_audio_n_write_flush(uint8_t itf)
{
  audiod_interface_t *audio = &_audiod_itf[itf];
  if (audio->p_desc == NULL) {
    return 0;
  }

  uint16_t n_bytes_copied;
  TU_VERIFY(audiod_tx_done_cb(audio->rhport, audio, &n_bytes_copied));
  return n_bytes_copied;
}

#endif
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN > 0
uint32_t tud_audio_int_ctr_n_write(uint8_t itf, uint8_t const* buffer, uint32_t bufsize)
{
  audiod_interface_t* audio = &_audiod_itf[itf];
  if (audio->p_desc == NULL) {
    return 0;
  }

  return tu_fifo_write_n(&audio->int_ctr_ff, buffer, bufsize);
}
#endif


// This function is called once a transmit of an audio packet was successfully completed. Here, we encode samples and place it in IN EP's buffer for next transmission.
// If you prefer your own (more efficient) implementation suiting your purpose set CFG_TUD_AUDIO_TX_FIFO_SIZE = 0 and use tud_audio_n_write_ep_in_buffer() (NOT IMPLEMENTED SO FAR).

// n_bytes_copied - Informs caller how many bytes were loaded. In case n_bytes_copied = 0, a ZLP is scheduled to inform host no data is available for current frame.
#if CFG_TUD_AUDIO_EPSIZE_IN
static bool audiod_tx_done_cb(uint8_t rhport, audiod_interface_t* audio, uint16_t * n_bytes_copied)
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
  // if no FIFOs are used the user may use this call back to load its data into the EP in buffer by use of tud_audio_n_write_ep_in_buffer().
  if (tud_audio_tx_done_pre_load_cb) TU_VERIFY(tud_audio_tx_done_pre_load_cb(rhport, idxDriver, audio->ep_in, audio->altSetting[idxItf]));

#if CFG_TUD_AUDIO_TX_FIFO_SIZE
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

          TU_VERIFY(audiod_tx_done_type_I_pcm_ff_cb(rhport, audio));

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
#endif

  // THIS IS A CRITICAL SECTION - audio->epin_buf_cnt MUST NOT BE MODIFIED FROM HERE - happens if tud_audio_n_write_ep_in_buffer() is executed in between!

  // THIS IS NOT SOLVED SO FAR!

  // FOR SINGLE THREADED OPERATION:
  // THIS FUNCTION IS NOT EXECUTED WITHIN AN INTERRUPT SO IT DOES NOT INTERRUPT tud_audio_n_write_ep_in_buffer()! AS LONG AS tud_audio_n_write_ep_in_buffer() IS NOT EXECUTED WITHIN AN INTERRUPT ALL IS FINE!

  // Schedule transmit
  TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_in, audio->epin_buf, audio->epin_buf_cnt));

  // Inform how many bytes were copied
  *n_bytes_copied = audio->epin_buf_cnt;

  // Declare EP in buffer empty
  audio->epin_buf_cnt = 0;

  // TO HERE

  // Call a weak callback here - a possibility for user to get informed former TX was completed and how many bytes were loaded for the next frame
  if (tud_audio_tx_done_post_load_cb) TU_VERIFY(tud_audio_tx_done_post_load_cb(rhport, *n_bytes_copied, idxDriver, audio->ep_in, audio->altSetting[idxItf]));

  return true;
}

#endif //CFG_TUD_AUDIO_EPSIZE_IN

#if CFG_TUD_AUDIO_TX_FIFO_SIZE
#if CFG_TUD_AUDIO_TX_FIFO_COUNT > 1 || (CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX != CFG_TUD_AUDIO_TX_ITEMSIZE)
static bool audiod_tx_done_type_I_pcm_ff_cb(uint8_t rhport, audiod_interface_t* audio)
{
  // We encode directly into IN EP's buffer - abort if previous transfer not complete
  TU_VERIFY(!usbd_edpt_busy(rhport, audio->ep_in));

  // Determine amount of samples
  uint16_t const nEndpointSampleCapacity = CFG_TUD_AUDIO_EPSIZE_IN / CFG_TUD_AUDIO_N_CHANNELS_TX / CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX;
  uint16_t nSamplesPerChannelToSend = tu_fifo_count(&audio->tx_ff[0]) / CFG_TUD_AUDIO_TX_ITEMSIZE;
  uint16_t nBytesToSend;
  uint8_t cntChannel;

  for (cntChannel = 1; cntChannel < CFG_TUD_AUDIO_N_CHANNELS_TX; cntChannel++)
  {
    uint16_t const count = tu_fifo_count(&audio->tx_ff[cntChannel]);
    if (count / CFG_TUD_AUDIO_TX_ITEMSIZE < nSamplesPerChannelToSend)
    {
      nSamplesPerChannelToSend = count * CFG_TUD_AUDIO_TX_ITEMSIZE;
    }
  }

  // Check if there is enough
  if (nSamplesPerChannelToSend == 0)
  {
    audio->epin_buf_cnt = 0;
    return true;
  }

  // Limit to maximum sample number - THIS IS A POSSIBLE ERROR SOURCE IF TOO MANY SAMPLE WOULD NEED TO BE SENT BUT CAN NOT!
  nSamplesPerChannelToSend = tu_min16(nSamplesPerChannelToSend, nEndpointSampleCapacity);
  nBytesToSend = nSamplesPerChannelToSend * CFG_TUD_AUDIO_N_CHANNELS_TX * CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX;

  // Encode
  uint16_t cntSample;
  uint8_t * pBuff = audio->epin_buf;
#if CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX == 1
  uint8_t sample;
#elif CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX == 2
  uint16_t sample;
#else
  uint32_t sample;
#endif

  // TODO: Big endianess handling
  for (cntSample = 0; cntSample < nSamplesPerChannelToSend; cntSample++)
  {
    for (cntChannel = 0; cntChannel < CFG_TUD_AUDIO_N_CHANNELS_TX; cntChannel++)
    {
      // Get sample from buffer
      tu_fifo_read_n(&audio->tx_ff[cntChannel], &sample, CFG_TUD_AUDIO_TX_ITEMSIZE);

      // Put it into EP's buffer - Let alignment problems be handled by memcpy
      memcpy(pBuff, &sample, CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX);

      // Advance pointer
      pBuff += CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX;
    }
  }

  audio->epin_buf_cnt = nBytesToSend;

  return true;
}

#else
static bool audiod_tx_done_type_I_pcm_ff_cb(uint8_t rhport, audiod_interface_t* audio)
{
  // We encode directly into IN EP's buffer - abort if previous transfer not complete
  TU_VERIFY(!usbd_edpt_busy(rhport, audio->ep_in));

  // Determine amount of samples
  uint16_t nByteCount = tu_fifo_count(&audio->tx_ff[0]);

  nByteCount = tu_min16(nByteCount, CFG_TUD_AUDIO_EPSIZE_IN);

  // Check if there is enough
  if (nByteCount == 0)
  {
    return true;
  }

  nByteCount = tu_fifo_read_n(&audio->tx_ff[0], audio->epin_buf, nByteCount);
  audio->epin_buf_cnt = nByteCount;

  return true;
}
#endif // CFG_TUD_AUDIO_TX_FIFO_COUNT > 1 || (CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX != CFG_TUD_AUDIO_TX_ITEMSIZE)

#endif //CFG_TUD_AUDIO_TX_FIFO_SIZE

// This function is called once a transmit of an feedback packet was successfully completed. Here, we get the next feedback value to be sent

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
static bool audio_fb_send(uint8_t rhport, audiod_interface_t *audio)
{
  uint8_t fb[4];
  uint16_t len;

  if (audio->fb_val == 0)
  {
    len = 0;
    return true;
  }
  else
  {
    len = 4;
    // Here we need to return the feedback value
    if (rhport == 0)
    {
      // For FS format is 10.14
      fb[0] = (audio->fb_val >> 2) & 0xFF;
      fb[1] = (audio->fb_val >> 10) & 0xFF;
      fb[2] = (audio->fb_val >> 18) & 0xFF;
      // 4th byte is needed to work correctly with MS Windows
      fb[3] = 0;
    }
    else
    {
      // For HS format is 16.16
      fb[0] = (audio->fb_val >> 0) & 0xFF;
      fb[1] = (audio->fb_val >> 8) & 0xFF;
      fb[2] = (audio->fb_val >> 16) & 0xFF;
      fb[3] = (audio->fb_val >> 24) & 0xFF;
    }
    return usbd_edpt_xfer(rhport, audio->ep_fb, fb, len);
  }

}

//static uint16_t audio_fb_done_cb(uint8_t rhport, audiod_interface_t* audio)
//{
//  (void) rhport;
//  (void) audio;
//
//  if (tud_audio_fb_done_cb) TU_VERIFY(tud_audio_fb_done_cb(rhport));
//  return 0;
//}

#endif

// This function is called once a transmit of an interrupt control packet was successfully completed. Here, we get the remaining bytes to send

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
static bool audio_int_ctr_done_cb(uint8_t rhport, audiod_interface_t* audio, uint16_t * n_bytes_copied)
{
  // We write directly into the EP's buffer - abort if previous transfer not complete
  TU_VERIFY(!usbd_edpt_busy(rhport, audio->ep_int_ctr));

  // TODO: Big endianess handling
  uint16_t cnt = tu_fifo_read_n(audio->int_ctr_ff, audio->ep_int_ctr_buf, CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN);

  if (cnt > 0)
  {
    // Schedule transmit
    TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_int_ctr, audio->ep_int_ctr_buf, cnt));
  }

  *n_bytes_copied = cnt;

  if (tud_audio_int_ctr_done_cb) TU_VERIFY(tud_audio_int_ctr_done_cb(rhport, n_bytes_copied));

  return true;
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

    // Initialize TX FIFOs if required
#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_FIFO_SIZE
    for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO_TX_FIFO_COUNT; cnt++)
    {
      tu_fifo_config(&audio->tx_ff[cnt], &audio->tx_ff_buf[cnt], CFG_TUD_AUDIO_TX_FIFO_SIZE, 1, true);
#if CFG_FIFO_MUTEX
      tu_fifo_config_mutex(&audio->tx_ff[cnt], osal_mutex_create(&audio->tx_ff_mutex[cnt]));
#endif
    }
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_FIFO_SIZE
    for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO_RX_FIFO_COUNT; cnt++)
    {
      tu_fifo_config(&audio->rx_ff[cnt], &audio->rx_ff_buf[cnt], CFG_TUD_AUDIO_RX_FIFO_SIZE, 1, true);
#if CFG_FIFO_MUTEX
      tu_fifo_config_mutex(&audio->rx_ff[cnt], osal_mutex_create(&audio->rx_ff_mutex[cnt]));
#endif
    }
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN > 0
    tu_fifo_config(&audio->int_ctr_ff, &audio->int_ctr_ff_buf, CFG_TUD_AUDIO_INT_CTR_BUFSIZE, 1, true);
#if CFG_FIFO_MUTEX
    tu_fifo_config_mutex(&audio->int_ctr_ff, osal_mutex_create(&audio->int_ctr_ff_mutex));
#endif
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

#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_FIFO_SIZE
    for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO_TX_FIFO_COUNT; cnt++)
    {
      tu_fifo_clear(&audio->tx_ff[cnt]);
    }
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_FIFO_SIZE
    for (uint8_t cnt = 0; cnt < CFG_TUD_AUDIO_RX_FIFO_COUNT; cnt++)
    {
      tu_fifo_clear(&audio->rx_ff[cnt]);
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
  // TODO: Find a way to find end of current audio function and avoid necessity of tud_audio_desc_lengths - since now max_length is available we could do this surely somehow
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
#if CFG_TUD_AUDIO_EPSIZE_IN > 0
  if (_audiod_itf[idxDriver].ep_in_as_intf_num == itf)
  {
    _audiod_itf[idxDriver].ep_in_as_intf_num = 0;
    usbd_edpt_close(rhport, _audiod_itf[idxDriver].ep_in);

    // Invoke callback - can be used to stop data sampling
    if (tud_audio_set_itf_close_EP_cb) TU_VERIFY(tud_audio_set_itf_close_EP_cb(rhport, p_request));

    _audiod_itf[idxDriver].ep_in = 0;                           // Necessary?
  }
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT
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

          // We need to set EP non busy since this is not taken care of right now in ep_close() - THIS IS A WORKAROUND!
          usbd_edpt_clear_stall(rhport, ep_addr);

#if CFG_TUD_AUDIO_EPSIZE_IN > 0
          if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN && ((tusb_desc_endpoint_t const *) p_desc)->bmAttributes.usage == 0x00)   // Check if usage is data EP
          {
            // Save address
            _audiod_itf[idxDriver].ep_in = ep_addr;
            _audiod_itf[idxDriver].ep_in_as_intf_num = itf;

            // Invoke callback - can be used to trigger data sampling if not already running
            if (tud_audio_set_itf_cb) TU_VERIFY(tud_audio_set_itf_cb(rhport, p_request));

            // Schedule first transmit - in case no sample data is available a ZLP is loaded
            uint16_t n_bytes_copied;
            TU_VERIFY(audiod_tx_done_cb(rhport, &_audiod_itf[idxDriver], &n_bytes_copied));
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
            TU_ASSERT(usbd_edpt_xfer(rhport, ep_addr, _audiod_itf[idxDriver].epout_buf, CFG_TUD_AUDIO_EPSIZE_OUT), false);
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

      // Load new data
      uint16 *n_bytes_copied;
      TU_VERIFY(audio_int_ctr_done_cb(rhport, &_audiod_itf[idxDriver], n_bytes_copied));

      if (*n_bytes_copied == 0 && xferred_bytes && (0 == (xferred_bytes % CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN)))
      {
        // There is no data left to send, a ZLP should be sent if
        // xferred_bytes is multiple of EP size and not zero
        return usbd_edpt_xfer(rhport, ep_addr, NULL, 0);
      }
    }

#endif

#if CFG_TUD_AUDIO_EPSIZE_IN

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
      uint16_t n_bytes_copied;
      TU_VERIFY(audiod_tx_done_cb(rhport, &_audiod_itf[idxDriver], &n_bytes_copied));

      // Transmission of ZLP is done by audiod_tx_done_cb()
      return true;
    }
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT

    // New audio packet received
    if (_audiod_itf[idxDriver].ep_out == ep_addr)
    {
      // Save into buffer - do whatever has to be done
      TU_VERIFY(audio_rx_done_cb(rhport, &_audiod_itf[idxDriver], _audiod_itf[idxDriver].epout_buf, xferred_bytes));

      // prepare for next transmission
      TU_ASSERT(usbd_edpt_xfer(rhport, ep_addr, _audiod_itf[idxDriver].epout_buf, CFG_TUD_AUDIO_EPSIZE_OUT), false);

      return true;
    }


#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
    // Transmission of feedback EP finished
    if (_audiod_itf[idxDriver].ep_fb == ep_addr)
    {
      if (tud_audio_fb_done_cb) TU_VERIFY(tud_audio_fb_done_cb(rhport));

      return audio_fb_send(rhport, &_audiod_itf[idxDriver]);
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
bool tud_audio_fb_set(uint8_t rhport, uint32_t feedback)
{
  audiod_interface_t *audio = &_audiod_itf[0];

  audio->fb_val = feedback;
  TU_VERIFY(!usbd_edpt_busy(rhport, audio->ep_fb), true);

  return audio_fb_send(rhport, audio);
}
#endif

#endif //TUSB_OPT_DEVICE_ENABLED && CFG_TUD_AUDIO
