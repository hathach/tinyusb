/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 *
 * Contribution
 * - Heiko Kuester: CH34x support
 * - Heiko Kuester: PL2303 support
 */

#include "tusb_option.h"

#if (CFG_TUH_ENABLED && CFG_TUH_CDC)

#include "host/usbh.h"
#include "host/usbh_pvt.h"

#include "cdc_host.h"

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUH_CDC_LOG_LEVEL
  #define CFG_TUH_CDC_LOG_LEVEL   CFG_TUH_LOG_LEVEL
#endif

#define TU_LOG_DRV(...)   TU_LOG(CFG_TUH_CDC_LOG_LEVEL, __VA_ARGS__)

//--------------------------------------------------------------------+
// Host CDC Interface
//--------------------------------------------------------------------+

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;

  uint8_t ep_notif;
  uint8_t serial_drid; // Serial Driver ID
  bool mounted;        // Enumeration is complete
  cdc_acm_capability_t acm_capability;

  TU_ATTR_ALIGNED(4) cdc_line_coding_t line_coding; // Baudrate, stop bits, parity, data width
  uint8_t line_state;                               // DTR (bit0), RTS (bit1)

  #if CFG_TUH_CDC_FTDI || CFG_TUH_CDC_CP210X || CFG_TUH_CDC_CH34X || CFG_TUH_CDC_PL2303
    cdc_line_coding_t requested_line_coding;
    // 1 byte padding
  #endif

  #if CFG_TUH_CDC_PL2303
    pl2303_t pl2303;
  #endif

  tuh_xfer_cb_t user_control_cb;

  struct {
    tu_edpt_stream_t tx;
    tu_edpt_stream_t rx;

    uint8_t tx_ff_buf[CFG_TUH_CDC_TX_BUFSIZE];
    CFG_TUH_MEM_ALIGN uint8_t tx_ep_buf[CFG_TUH_CDC_TX_EPSIZE];

    uint8_t rx_ff_buf[CFG_TUH_CDC_TX_BUFSIZE];
    CFG_TUH_MEM_ALIGN uint8_t rx_ep_buf[CFG_TUH_CDC_TX_EPSIZE];
  } stream;
} cdch_interface_t;

CFG_TUH_MEM_SECTION
static cdch_interface_t cdch_data[CFG_TUH_CDC];

//--------------------------------------------------------------------+
// Serial Driver
//--------------------------------------------------------------------+

//------------- ACM prototypes -------------//
static bool acm_open(uint8_t daddr, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
static void acm_process_config(tuh_xfer_t* xfer);

static bool acm_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool acm_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool acm_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool acm_set_control_line_state(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

//------------- FTDI prototypes -------------//
#if CFG_TUH_CDC_FTDI
#include "serial/ftdi_sio.h"

static uint16_t const ftdi_vid_pid_list[][2] = {CFG_TUH_CDC_FTDI_VID_PID_LIST};

static bool ftdi_open(uint8_t daddr, const tusb_desc_interface_t *itf_desc, uint16_t max_len);
static void ftdi_process_config(tuh_xfer_t* xfer);

static bool ftdi_sio_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ftdi_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ftdi_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ftdi_sio_set_modem_ctrl(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
#endif

//------------- CP210X prototypes -------------//
#if CFG_TUH_CDC_CP210X
#include "serial/cp210x.h"

static uint16_t const cp210x_vid_pid_list[][2] = {CFG_TUH_CDC_CP210X_VID_PID_LIST};

static bool cp210x_open(uint8_t daddr, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
static void cp210x_process_config(tuh_xfer_t* xfer);

static bool cp210x_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool cp210x_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool cp210x_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool cp210x_set_modem_ctrl(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
#endif

//------------- CH34x prototypes -------------//
#if CFG_TUH_CDC_CH34X
#include "serial/ch34x.h"

static uint16_t const ch34x_vid_pid_list[][2] = {CFG_TUH_CDC_CH34X_VID_PID_LIST};

static bool ch34x_open(uint8_t daddr, tusb_desc_interface_t const* itf_desc, uint16_t max_len);
static void ch34x_process_config(tuh_xfer_t* xfer);

static bool ch34x_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ch34x_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ch34x_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ch34x_set_modem_ctrl(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
#endif

//------------- PL2303 prototypes -------------//
#if CFG_TUH_CDC_PL2303
#include "serial/pl2303.h"

static uint16_t const pl2303_vid_pid_list[][2] = {CFG_TUH_CDC_PL2303_VID_PID_LIST};
static const struct pl2303_type_data pl2303_type_data[TYPE_COUNT] = {PL2303_TYPE_DATA};

CFG_TUH_MEM_SECTION CFG_TUH_MEM_ALIGN
static tusb_desc_device_t pl2303_desc[CFG_TUH_CDC][CFG_TUH_ENUMERATION_BUFSIZE];

static bool pl2303_open(uint8_t daddr, tusb_desc_interface_t const* itf_desc, uint16_t max_len);
static void pl2303_process_config(tuh_xfer_t* xfer);

static bool pl2303_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool pl2303_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool pl2303_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool pl2303_set_modem_ctrl(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
#endif

//------------- Common -------------//
enum {
  SERIAL_DRIVER_ACM = 0,

#if CFG_TUH_CDC_FTDI
  SERIAL_DRIVER_FTDI,
#endif

#if CFG_TUH_CDC_CP210X
  SERIAL_DRIVER_CP210X,
#endif

#if CFG_TUH_CDC_CH34X
  SERIAL_DRIVER_CH34X,
#endif

#if CFG_TUH_CDC_PL2303
  SERIAL_DRIVER_PL2303,
#endif

  SERIAL_DRIVER_COUNT
};

typedef struct {
  uint16_t const (*vid_pid_list)[2];
  uint16_t const vid_pid_count;
  bool (*const open)(uint8_t daddr, const tusb_desc_interface_t *itf_desc, uint16_t max_len);
  void (*const process_set_config)(tuh_xfer_t* xfer);
  bool (*const set_control_line_state)(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
  bool (*const set_baudrate)(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
  bool (*const set_data_format)(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
  bool (*const set_line_coding)(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
} cdch_serial_driver_t;

// Note driver list must be in the same order as SERIAL_DRIVER enum
static const cdch_serial_driver_t serial_drivers[] = {
  {
      .vid_pid_list           = NULL,
      .vid_pid_count          = 0,
      .open                   = acm_open,
      .process_set_config     = acm_process_config,
      .set_control_line_state = acm_set_control_line_state,
      .set_baudrate           = acm_set_baudrate,
      .set_data_format        = acm_set_data_format,
      .set_line_coding        = acm_set_line_coding
  },

  #if CFG_TUH_CDC_FTDI
  {
      .vid_pid_list           = ftdi_vid_pid_list,
      .vid_pid_count          = TU_ARRAY_SIZE(ftdi_vid_pid_list),
      .open                   = ftdi_open,
      .process_set_config     = ftdi_process_config,
      .set_control_line_state = ftdi_sio_set_modem_ctrl,
      .set_baudrate           = ftdi_sio_set_baudrate,
      .set_data_format        = ftdi_set_data_format,
      .set_line_coding        = ftdi_set_line_coding
  },
  #endif

  #if CFG_TUH_CDC_CP210X
  {
      .vid_pid_list           = cp210x_vid_pid_list,
      .vid_pid_count          = TU_ARRAY_SIZE(cp210x_vid_pid_list),
      .open                   = cp210x_open,
      .process_set_config     = cp210x_process_config,
      .set_control_line_state = cp210x_set_modem_ctrl,
      .set_baudrate           = cp210x_set_baudrate,
      .set_data_format        = cp210x_set_data_format,
      .set_line_coding        = cp210x_set_line_coding
  },
  #endif

  #if CFG_TUH_CDC_CH34X
  {
      .vid_pid_list           = ch34x_vid_pid_list,
      .vid_pid_count          = TU_ARRAY_SIZE(ch34x_vid_pid_list),
      .open                   = ch34x_open,
      .process_set_config     = ch34x_process_config,
      .set_control_line_state = ch34x_set_modem_ctrl,
      .set_baudrate           = ch34x_set_baudrate,
      .set_data_format        = ch34x_set_data_format,
      .set_line_coding        = ch34x_set_line_coding
  },
  #endif

  #if CFG_TUH_CDC_PL2303
  {
      .vid_pid_list           = pl2303_vid_pid_list,
      .vid_pid_count          = TU_ARRAY_SIZE(pl2303_vid_pid_list),
      .open                   = pl2303_open,
      .process_set_config     = pl2303_process_config,
      .set_control_line_state = pl2303_set_modem_ctrl,
      .set_baudrate           = pl2303_set_baudrate,
      .set_data_format        = pl2303_set_data_format,
      .set_line_coding        = pl2303_set_line_coding
  },
  #endif
};

TU_VERIFY_STATIC(TU_ARRAY_SIZE(serial_drivers) == SERIAL_DRIVER_COUNT, "Serial driver count mismatch");

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

static inline cdch_interface_t* get_itf(uint8_t idx) {
  TU_ASSERT(idx < CFG_TUH_CDC, NULL);
  cdch_interface_t* p_cdc = &cdch_data[idx];

  return (p_cdc->daddr != 0) ? p_cdc : NULL;
}

static inline uint8_t get_idx_by_ep_addr(uint8_t daddr, uint8_t ep_addr) {
  for(uint8_t i=0; i<CFG_TUH_CDC; i++) {
    cdch_interface_t* p_cdc = &cdch_data[i];
    if ( (p_cdc->daddr == daddr) &&
         (ep_addr == p_cdc->ep_notif || ep_addr == p_cdc->stream.rx.ep_addr || ep_addr == p_cdc->stream.tx.ep_addr)) {
      return i;
    }
  }

  return TUSB_INDEX_INVALID_8;
}

static cdch_interface_t* make_new_itf(uint8_t daddr, tusb_desc_interface_t const *itf_desc) {
  for(uint8_t i=0; i<CFG_TUH_CDC; i++) {
    if (cdch_data[i].daddr == 0) {
      cdch_interface_t* p_cdc = &cdch_data[i];
      p_cdc->daddr              = daddr;
      p_cdc->bInterfaceNumber   = itf_desc->bInterfaceNumber;
      p_cdc->bInterfaceSubClass = itf_desc->bInterfaceSubClass;
      p_cdc->bInterfaceProtocol = itf_desc->bInterfaceProtocol;
      p_cdc->line_state         = 0;
      return p_cdc;
    }
  }

  return NULL;
}

static bool open_ep_stream_pair(cdch_interface_t* p_cdc , tusb_desc_endpoint_t const *desc_ep);
static void set_config_complete(cdch_interface_t * p_cdc, uint8_t idx, uint8_t itf_num);
static void cdch_internal_control_complete(tuh_xfer_t* xfer);

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

uint8_t tuh_cdc_itf_get_index(uint8_t daddr, uint8_t itf_num) {
  for (uint8_t i = 0; i < CFG_TUH_CDC; i++) {
    const cdch_interface_t* p_cdc = &cdch_data[i];
    if (p_cdc->daddr == daddr && p_cdc->bInterfaceNumber == itf_num) return i;
  }

  return TUSB_INDEX_INVALID_8;
}

bool tuh_cdc_itf_get_info(uint8_t idx, tuh_itf_info_t* info) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && info);

  info->daddr = p_cdc->daddr;

  // re-construct descriptor
  tusb_desc_interface_t* desc = &info->desc;
  desc->bLength            = sizeof(tusb_desc_interface_t);
  desc->bDescriptorType    = TUSB_DESC_INTERFACE;

  desc->bInterfaceNumber   = p_cdc->bInterfaceNumber;
  desc->bAlternateSetting  = 0;
  desc->bNumEndpoints      = 2u + (p_cdc->ep_notif ? 1u : 0u);
  desc->bInterfaceClass    = TUSB_CLASS_CDC;
  desc->bInterfaceSubClass = p_cdc->bInterfaceSubClass;
  desc->bInterfaceProtocol = p_cdc->bInterfaceProtocol;
  desc->iInterface         = 0; // not used yet

  return true;
}

bool tuh_cdc_mounted(uint8_t idx) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);
  return p_cdc->mounted;
}

bool tuh_cdc_get_dtr(uint8_t idx) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return (p_cdc->line_state & CDC_CONTROL_LINE_STATE_DTR) ? true : false;
}

bool tuh_cdc_get_rts(uint8_t idx) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return (p_cdc->line_state & CDC_CONTROL_LINE_STATE_RTS) ? true : false;
}

bool tuh_cdc_get_local_line_coding(uint8_t idx, cdc_line_coding_t* line_coding) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  *line_coding = p_cdc->line_coding;

  return true;
}

//--------------------------------------------------------------------+
// Write
//--------------------------------------------------------------------+

uint32_t tuh_cdc_write(uint8_t idx, void const* buffer, uint32_t bufsize) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write(&p_cdc->stream.tx, buffer, bufsize);
}

uint32_t tuh_cdc_write_flush(uint8_t idx) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write_xfer(&p_cdc->stream.tx);
}

bool tuh_cdc_write_clear(uint8_t idx) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_clear(&p_cdc->stream.tx);
}

uint32_t tuh_cdc_write_available(uint8_t idx) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write_available(&p_cdc->stream.tx);
}

//--------------------------------------------------------------------+
// Read
//--------------------------------------------------------------------+

uint32_t tuh_cdc_read (uint8_t idx, void* buffer, uint32_t bufsize) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_read(&p_cdc->stream.rx, buffer, bufsize);
}

uint32_t tuh_cdc_read_available(uint8_t idx) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_read_available(&p_cdc->stream.rx);
}

bool tuh_cdc_peek(uint8_t idx, uint8_t* ch) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_peek(&p_cdc->stream.rx, ch);
}

bool tuh_cdc_read_clear (uint8_t idx) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  bool ret = tu_edpt_stream_clear(&p_cdc->stream.rx);
  tu_edpt_stream_read_xfer(&p_cdc->stream.rx);
  return ret;
}

//--------------------------------------------------------------------+
// Control Endpoint API
//--------------------------------------------------------------------+

static void process_internal_control_complete(tuh_xfer_t* xfer, uint8_t itf_num) {
  uint8_t idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc, );
  uint16_t const value = tu_le16toh(xfer->setup->wValue);

  if (xfer->result == XFER_RESULT_SUCCESS) {
    switch (p_cdc->serial_drid) {
      case SERIAL_DRIVER_ACM:
        switch (xfer->setup->bRequest) {
          case CDC_REQUEST_SET_CONTROL_LINE_STATE:
            p_cdc->line_state = (uint8_t) value;
            break;

          case CDC_REQUEST_SET_LINE_CODING: {
            uint16_t const len = tu_min16(sizeof(cdc_line_coding_t), tu_le16toh(xfer->setup->wLength));
            memcpy(&p_cdc->line_coding, xfer->buffer, len);
            break;
          }

          default: break;
        }
        break;

      #if CFG_TUH_CDC_FTDI
      case SERIAL_DRIVER_FTDI:
        switch (xfer->setup->bRequest) {
          case FTDI_SIO_MODEM_CTRL:
            p_cdc->line_state = (uint8_t) value;
            break;

          case FTDI_SIO_SET_BAUD_RATE:
            p_cdc->line_coding.bit_rate = p_cdc->requested_line_coding.bit_rate;
            break;

          default: break;
        }
        break;
      #endif

      #if CFG_TUH_CDC_CP210X
      case SERIAL_DRIVER_CP210X:
        switch(xfer->setup->bRequest) {
          case CP210X_SET_MHS:
            p_cdc->line_state = (uint8_t) value;
            break;

          case CP210X_SET_BAUDRATE: {
            uint32_t baudrate;
            memcpy(&baudrate, xfer->buffer, sizeof(uint32_t));
            p_cdc->line_coding.bit_rate = tu_le32toh(baudrate);
            break;
          }

          default: break;
        }
        break;
      #endif

      #if CFG_TUH_CDC_CH34X
      case SERIAL_DRIVER_CH34X:
        switch (xfer->setup->bRequest) {
          case CH34X_REQ_WRITE_REG:
            // register write request
            switch (value) {
              case CH34X_REG16_DIVISOR_PRESCALER:
                // baudrate
                p_cdc->line_coding.bit_rate = p_cdc->requested_line_coding.bit_rate;
                break;

              case CH32X_REG16_LCR2_LCR:
                // data format
                p_cdc->line_coding.stop_bits = p_cdc->requested_line_coding.stop_bits;
                p_cdc->line_coding.parity = p_cdc->requested_line_coding.parity;
                p_cdc->line_coding.data_bits = p_cdc->requested_line_coding.data_bits;
                break;

              default: break;
            }
            break;

          case CH34X_REQ_MODEM_CTRL: {
            // set modem controls RTS/DTR request. Note: signals are inverted
            uint16_t const modem_signal = ~value;
            if (modem_signal & CH34X_BIT_RTS) {
              p_cdc->line_state |= CDC_CONTROL_LINE_STATE_RTS;
            } else {
              p_cdc->line_state &= (uint8_t) ~CDC_CONTROL_LINE_STATE_RTS;
            }

            if (modem_signal & CH34X_BIT_DTR) {
              p_cdc->line_state |= CDC_CONTROL_LINE_STATE_DTR;
            } else {
              p_cdc->line_state &= (uint8_t) ~CDC_CONTROL_LINE_STATE_DTR;
            }
            break;
          }

          default: break;
        }
        break;
      #endif

      #if CFG_TUH_CDC_PL2303
      case SERIAL_DRIVER_PL2303:
        if ( xfer->setup->bRequest == PL2303_SET_LINE_REQUEST && xfer->setup->bmRequestType == PL2303_SET_LINE_REQUEST_TYPE ) {
          p_cdc->line_coding = p_cdc->requested_line_coding;
            }
        if ( xfer->setup->bRequest == PL2303_SET_CONTROL_REQUEST && xfer->setup->bmRequestType == PL2303_SET_CONTROL_REQUEST_TYPE ) {
          p_cdc->line_state = value;
        }
        break;
      #endif

      default: break;
    }
  }

  xfer->complete_cb = p_cdc->user_control_cb;
  if (xfer->complete_cb) {
    xfer->complete_cb(xfer);
  }
}

// internal control complete to update state such as line state, encoding
static void cdch_internal_control_complete(tuh_xfer_t* xfer) {
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  process_internal_control_complete(xfer, itf_num);
}

bool tuh_cdc_set_control_line_state(uint8_t idx, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  cdch_serial_driver_t const* driver = &serial_drivers[p_cdc->serial_drid];

  if (complete_cb) {
    return driver->set_control_line_state(p_cdc, line_state, complete_cb, user_data);
  } else {
    // blocking
    xfer_result_t result = XFER_RESULT_INVALID;
    bool ret = driver->set_control_line_state(p_cdc, line_state, complete_cb, (uintptr_t) &result);

    if (user_data) {
      // user_data is not NULL, return result via user_data
      *((xfer_result_t*) user_data) = result;
    }

    TU_VERIFY(ret && result == XFER_RESULT_SUCCESS);
    p_cdc->line_state = (uint8_t) line_state;
    return true;
  }
}

bool tuh_cdc_set_baudrate(uint8_t idx, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  cdch_serial_driver_t const* driver = &serial_drivers[p_cdc->serial_drid];

  if (complete_cb) {
    return driver->set_baudrate(p_cdc, baudrate, complete_cb, user_data);
  } else {
    // blocking
    xfer_result_t result = XFER_RESULT_INVALID;
    bool ret = driver->set_baudrate(p_cdc, baudrate, complete_cb, (uintptr_t) &result);

    if (user_data) {
      // user_data is not NULL, return result via user_data
      *((xfer_result_t*) user_data) = result;
    }

    TU_VERIFY(ret && result == XFER_RESULT_SUCCESS);
    p_cdc->line_coding.bit_rate = baudrate;
    return true;
  }
}

bool tuh_cdc_set_data_format(uint8_t idx, uint8_t stop_bits, uint8_t parity, uint8_t data_bits,
                             tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  cdch_serial_driver_t const* driver = &serial_drivers[p_cdc->serial_drid];

  if (complete_cb) {
    return driver->set_data_format(p_cdc, stop_bits, parity, data_bits, complete_cb, user_data);
  } else {
    // blocking
    xfer_result_t result = XFER_RESULT_INVALID;
    bool ret = driver->set_data_format(p_cdc, stop_bits, parity, data_bits, complete_cb, (uintptr_t) &result);

    if (user_data) {
      // user_data is not NULL, return result via user_data
      *((xfer_result_t*) user_data) = result;
    }

    TU_VERIFY(ret && result == XFER_RESULT_SUCCESS);
    p_cdc->line_coding.stop_bits = stop_bits;
    p_cdc->line_coding.parity = parity;
    p_cdc->line_coding.data_bits = data_bits;
    return true;
  }
}

bool tuh_cdc_set_line_coding(uint8_t idx, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  cdch_serial_driver_t const* driver = &serial_drivers[p_cdc->serial_drid];

  if ( complete_cb ) {
    return driver->set_line_coding(p_cdc, line_coding, complete_cb, user_data);
  } else {
    // blocking
    xfer_result_t result = XFER_RESULT_INVALID;
    bool ret = driver->set_line_coding(p_cdc, line_coding, complete_cb, (uintptr_t) &result);

    if (user_data) {
      // user_data is not NULL, return result via user_data
      *((xfer_result_t*) user_data) = result;
    }

    TU_VERIFY(ret && result == XFER_RESULT_SUCCESS);
    p_cdc->line_coding = *line_coding;
    return true;
  }
}

//--------------------------------------------------------------------+
// CLASS-USBH API
//--------------------------------------------------------------------+

void cdch_init(void) {
  tu_memclr(cdch_data, sizeof(cdch_data));

  for (size_t i = 0; i < CFG_TUH_CDC; i++) {
    cdch_interface_t* p_cdc = &cdch_data[i];

    tu_edpt_stream_init(&p_cdc->stream.tx, true, true, false,
                        p_cdc->stream.tx_ff_buf, CFG_TUH_CDC_TX_BUFSIZE,
                        p_cdc->stream.tx_ep_buf, CFG_TUH_CDC_TX_EPSIZE);

    tu_edpt_stream_init(&p_cdc->stream.rx, true, false, false,
                        p_cdc->stream.rx_ff_buf, CFG_TUH_CDC_RX_BUFSIZE,
                        p_cdc->stream.rx_ep_buf, CFG_TUH_CDC_RX_EPSIZE);
  }
}

void cdch_close(uint8_t daddr) {
  for (uint8_t idx = 0; idx < CFG_TUH_CDC; idx++) {
    cdch_interface_t* p_cdc = &cdch_data[idx];
    if (p_cdc->daddr == daddr) {
      TU_LOG_DRV("  CDCh close addr = %u index = %u\r\n", daddr, idx);

      // Invoke application callback
      if (tuh_cdc_umount_cb) tuh_cdc_umount_cb(idx);

      p_cdc->daddr = 0;
      p_cdc->bInterfaceNumber = 0;
      p_cdc->mounted = false;
      tu_edpt_stream_close(&p_cdc->stream.tx);
      tu_edpt_stream_close(&p_cdc->stream.rx);
    }
  }
}

bool cdch_xfer_cb(uint8_t daddr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes) {
  // TODO handle stall response, retry failed transfer ...
  TU_ASSERT(event == XFER_RESULT_SUCCESS);

  uint8_t const idx = get_idx_by_ep_addr(daddr, ep_addr);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc);

  if ( ep_addr == p_cdc->stream.tx.ep_addr ) {
    // invoke tx complete callback to possibly refill tx fifo
    if (tuh_cdc_tx_complete_cb) tuh_cdc_tx_complete_cb(idx);

    if ( 0 == tu_edpt_stream_write_xfer(&p_cdc->stream.tx) ) {
      // If there is no data left, a ZLP should be sent if:
      // - xferred_bytes is multiple of EP Packet size and not zero
      tu_edpt_stream_write_zlp_if_needed(&p_cdc->stream.tx, xferred_bytes);
    }
  } else if ( ep_addr == p_cdc->stream.rx.ep_addr ) {
    #if CFG_TUH_CDC_FTDI
    if (p_cdc->serial_drid == SERIAL_DRIVER_FTDI) {
      // FTDI reserve 2 bytes for status
      // uint8_t status[2] = {p_cdc->stream.rx.ep_buf[0], p_cdc->stream.rx.ep_buf[1]};
      tu_edpt_stream_read_xfer_complete_offset(&p_cdc->stream.rx, xferred_bytes, 2);
    }else
    #endif
    {
      tu_edpt_stream_read_xfer_complete(&p_cdc->stream.rx, xferred_bytes);
    }

    // invoke receive callback
    if (tuh_cdc_rx_cb) tuh_cdc_rx_cb(idx);

    // prepare for next transfer if needed
    tu_edpt_stream_read_xfer(&p_cdc->stream.rx);
  }else if ( ep_addr == p_cdc->ep_notif ) {
    // TODO handle notification endpoint
  }else {
    TU_ASSERT(false);
  }

  return true;
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+

static bool open_ep_stream_pair(cdch_interface_t* p_cdc, tusb_desc_endpoint_t const* desc_ep) {
  for (size_t i = 0; i < 2; i++) {
    TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType &&
              TUSB_XFER_BULK == desc_ep->bmAttributes.xfer);
    TU_ASSERT(tuh_edpt_open(p_cdc->daddr, desc_ep));

    if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
      tu_edpt_stream_open(&p_cdc->stream.rx, p_cdc->daddr, desc_ep);
    } else {
      tu_edpt_stream_open(&p_cdc->stream.tx, p_cdc->daddr, desc_ep);
    }

    desc_ep = (tusb_desc_endpoint_t const*) tu_desc_next(desc_ep);
  }

  return true;
}

bool cdch_open(uint8_t rhport, uint8_t daddr, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  (void) rhport;
  cdch_serial_driver_t const* driver;

  // For CDC: only support ACM subclass
  // Note: Protocol 0xFF can be RNDIS device
  if (TUSB_CLASS_CDC                           == itf_desc->bInterfaceClass &&
      CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == itf_desc->bInterfaceSubClass) {
    driver = &serial_drivers[SERIAL_DRIVER_ACM];
    return driver->open(daddr, itf_desc, max_len);
  }
  else if (SERIAL_DRIVER_COUNT > 1 &&
           TUSB_CLASS_VENDOR_SPECIFIC == itf_desc->bInterfaceClass) {
    uint16_t vid, pid;
    TU_VERIFY(tuh_vid_pid_get(daddr, &vid, &pid));

    for (size_t dr = 1; dr < SERIAL_DRIVER_COUNT; dr++) {
      driver = &serial_drivers[dr];
      for (size_t i = 0; i < driver->vid_pid_count; i++) {
        if (driver->vid_pid_list != NULL && driver->open != NULL &&
            driver->vid_pid_list[i][0] == vid && driver->vid_pid_list[i][1] == pid) {
          return driver->open(daddr, itf_desc, max_len);
        }
      }
    }
  }

  return false;
}

static void set_config_complete(cdch_interface_t * p_cdc, uint8_t idx, uint8_t itf_num) {
  TU_LOG_DRV("CDCh Set Configure complete\r\n");
  p_cdc->mounted = true;
  if (tuh_cdc_mount_cb) tuh_cdc_mount_cb(idx);

  // Prepare for incoming data
  tu_edpt_stream_read_xfer(&p_cdc->stream.rx);

  // notify usbh that driver enumeration is complete
  usbh_driver_set_config_complete(p_cdc->daddr, itf_num);
}

bool cdch_set_config(uint8_t daddr, uint8_t itf_num) {
  tusb_control_request_t request;
  request.wIndex = tu_htole16((uint16_t) itf_num);

  // fake transfer to kick-off process
  tuh_xfer_t xfer;
  xfer.daddr  = daddr;
  xfer.result = XFER_RESULT_SUCCESS;
  xfer.setup  = &request;
  xfer.user_data = 0; // initial state

  uint8_t const idx = tuh_cdc_itf_get_index(daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);

  serial_drivers[p_cdc->serial_drid].process_set_config(&xfer);
  return true;
}

//--------------------------------------------------------------------+
// ACM
//--------------------------------------------------------------------+

enum {
  CONFIG_ACM_SET_CONTROL_LINE_STATE = 0,
  CONFIG_ACM_SET_LINE_CODING,
  CONFIG_ACM_COMPLETE,
};

static bool acm_open(uint8_t daddr, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
  uint8_t const* p_desc_end = ((uint8_t const*) itf_desc) + max_len;

  cdch_interface_t* p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY(p_cdc);
  p_cdc->serial_drid = SERIAL_DRIVER_ACM;

  //------------- Control Interface -------------//
  uint8_t const* p_desc = tu_desc_next(itf_desc);

  // Communication Functional Descriptors
  while ((p_desc < p_desc_end) && (TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc))) {
    if (CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT == cdc_functional_desc_typeof(p_desc)) {
      // save ACM bmCapabilities
      p_cdc->acm_capability = ((cdc_desc_func_acm_t const*) p_desc)->bmCapabilities;
    }

    p_desc = tu_desc_next(p_desc);
  }

  // Open notification endpoint of control interface if any
  if (itf_desc->bNumEndpoints == 1) {
    TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc));
    tusb_desc_endpoint_t const* desc_ep = (tusb_desc_endpoint_t const*) p_desc;

    TU_ASSERT(tuh_edpt_open(daddr, desc_ep));
    p_cdc->ep_notif = desc_ep->bEndpointAddress;

    p_desc = tu_desc_next(p_desc);
  }

  //------------- Data Interface (if any) -------------//
  if ((TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) &&
      (TUSB_CLASS_CDC_DATA == ((tusb_desc_interface_t const*) p_desc)->bInterfaceClass)) {
    // next to endpoint descriptor
    p_desc = tu_desc_next(p_desc);

    // data endpoints expected to be in pairs
    TU_ASSERT(open_ep_stream_pair(p_cdc, (tusb_desc_endpoint_t const*) p_desc));
  }

  return true;
}

static void acm_process_config(tuh_xfer_t* xfer) {
  uintptr_t const state = xfer->user_data;
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);

  switch (state) {
    case CONFIG_ACM_SET_CONTROL_LINE_STATE:
      #if CFG_TUH_CDC_LINE_CONTROL_ON_ENUM
      if (p_cdc->acm_capability.support_line_request) {
        TU_ASSERT(acm_set_control_line_state(p_cdc, CFG_TUH_CDC_LINE_CONTROL_ON_ENUM, acm_process_config, CONFIG_ACM_SET_LINE_CODING),);
        break;
      }
      #endif
      TU_ATTR_FALLTHROUGH;

    case CONFIG_ACM_SET_LINE_CODING:
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
      if (p_cdc->acm_capability.support_line_request) {
        cdc_line_coding_t line_coding = CFG_TUH_CDC_LINE_CODING_ON_ENUM;
        TU_ASSERT(acm_set_line_coding(p_cdc, &line_coding, acm_process_config, CONFIG_ACM_COMPLETE),);
        break;
      }
      #endif
      TU_ATTR_FALLTHROUGH;

    case CONFIG_ACM_COMPLETE:
      // itf_num+1 to account for data interface as well
      set_config_complete(p_cdc, idx, itf_num + 1);
      break;

    default:
      break;
  }
}

static bool acm_set_control_line_state(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(p_cdc->acm_capability.support_line_request);
  TU_LOG_DRV("CDC ACM Set Control Line State\r\n");

  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE,
    .wValue   = tu_htole16(line_state),
    .wIndex   = tu_htole16((uint16_t) p_cdc->bInterfaceNumber),
    .wLength  = 0
  };

  p_cdc->user_control_cb = complete_cb;

  tuh_xfer_t xfer = {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = NULL,
    .complete_cb = complete_cb ? cdch_internal_control_complete : NULL, // complete_cb is NULL for sync call
    .user_data   = user_data
  };

  TU_ASSERT(tuh_control_xfer(&xfer));
  return true;
}

static bool acm_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_DRV("CDC ACM Set Line Conding\r\n");

  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = CDC_REQUEST_SET_LINE_CODING,
    .wValue   = 0,
    .wIndex   = tu_htole16(p_cdc->bInterfaceNumber),
    .wLength  = tu_htole16(sizeof(cdc_line_coding_t))
  };

  // use usbh enum buf to hold line coding since user line_coding variable does not live long enough
  uint8_t* enum_buf = usbh_get_enum_buf();
  memcpy(enum_buf, line_coding, sizeof(cdc_line_coding_t));

  p_cdc->user_control_cb = complete_cb;
  tuh_xfer_t xfer = {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = enum_buf,
    .complete_cb = complete_cb ? cdch_internal_control_complete : NULL, // complete_cb is NULL for sync call
    .user_data   = user_data
  };

  TU_ASSERT(tuh_control_xfer(&xfer));
  return true;
}

static bool acm_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits,
                                tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_DRV("CDC ACM Set Data Format\r\n");

  cdc_line_coding_t line_coding;
  line_coding.bit_rate = p_cdc->line_coding.bit_rate;
  line_coding.stop_bits = stop_bits;
  line_coding.parity = parity;
  line_coding.data_bits = data_bits;

  return acm_set_line_coding(p_cdc, &line_coding, complete_cb, user_data);
}

static bool acm_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(p_cdc->acm_capability.support_line_request);
  cdc_line_coding_t line_coding = p_cdc->line_coding;
  line_coding.bit_rate = baudrate;
  return acm_set_line_coding(p_cdc, &line_coding, complete_cb, user_data);
}

//--------------------------------------------------------------------+
// FTDI
//--------------------------------------------------------------------+
#if CFG_TUH_CDC_FTDI

enum {
  CONFIG_FTDI_RESET = 0,
  CONFIG_FTDI_MODEM_CTRL,
  CONFIG_FTDI_SET_BAUDRATE,
  CONFIG_FTDI_SET_DATA,
  CONFIG_FTDI_COMPLETE
};

static bool ftdi_open(uint8_t daddr, const tusb_desc_interface_t *itf_desc, uint16_t max_len) {
  // FTDI Interface includes 1 vendor interface + 2 bulk endpoints
  TU_VERIFY(itf_desc->bInterfaceSubClass == 0xff && itf_desc->bInterfaceProtocol == 0xff && itf_desc->bNumEndpoints == 2);
  TU_VERIFY(sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t) <= max_len);

  cdch_interface_t * p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY(p_cdc);

  TU_LOG_DRV("FTDI opened\r\n");
  p_cdc->serial_drid = SERIAL_DRIVER_FTDI;

  // endpoint pair
  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) tu_desc_next(itf_desc);

  // data endpoints expected to be in pairs
  return open_ep_stream_pair(p_cdc, desc_ep);
}

// set request without data
static bool ftdi_sio_set_request(cdch_interface_t* p_cdc, uint8_t command, uint16_t value, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_VENDOR,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = command,
    .wValue   = tu_htole16(value),
    .wIndex   = 0,
    .wLength  = 0
  };

  tuh_xfer_t xfer = {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = NULL,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

static bool ftdi_sio_reset(cdch_interface_t* p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return ftdi_sio_set_request(p_cdc, FTDI_SIO_RESET, FTDI_SIO_RESET_SIO, complete_cb, user_data);
}

static bool ftdi_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits,
                                 tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) p_cdc;
  (void) stop_bits;
  (void) parity;
  (void) data_bits;
  (void) complete_cb;
  (void) user_data;
  // TODO not implemented yet
  return false;
}

static bool ftdi_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) p_cdc;
  (void) line_coding;
  (void) complete_cb;
  (void) user_data;
  // TODO not implemented yet
  return false;
}

static bool ftdi_sio_set_modem_ctrl(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_DRV("CDC FTDI Set Control Line State\r\n");
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(ftdi_sio_set_request(p_cdc, FTDI_SIO_MODEM_CTRL, 0x0300 | line_state,
                                 complete_cb ? cdch_internal_control_complete : NULL, user_data));
  return true;
}

static uint32_t ftdi_232bm_baud_base_to_divisor(uint32_t baud, uint32_t base) {
  const uint8_t divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
  uint32_t divisor;

  /* divisor shifted 3 bits to the left */
  uint32_t divisor3 = base / (2 * baud);
  divisor = (divisor3 >> 3);
  divisor |= (uint32_t) divfrac[divisor3 & 0x7] << 14;

  /* Deal with special cases for highest baud rates. */
  if (divisor == 1) { /* 1.0 */
    divisor = 0;
  }
  else if (divisor == 0x4001) { /* 1.5 */
    divisor = 1;
  }

  return divisor;
}

static uint32_t ftdi_232bm_baud_to_divisor(uint32_t baud) {
  return ftdi_232bm_baud_base_to_divisor(baud, 48000000u);
}

static bool ftdi_sio_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  uint16_t const divisor = (uint16_t) ftdi_232bm_baud_to_divisor(baudrate);
  TU_LOG_DRV("CDC FTDI Set BaudRate = %lu, divisor = 0x%04x\r\n", baudrate, divisor);

  p_cdc->user_control_cb = complete_cb;
  p_cdc->requested_line_coding.bit_rate = baudrate;
  TU_ASSERT(ftdi_sio_set_request(p_cdc, FTDI_SIO_SET_BAUD_RATE, divisor,
                                 complete_cb ? cdch_internal_control_complete : NULL, user_data));

  return true;
}

static void ftdi_process_config(tuh_xfer_t* xfer) {
  uintptr_t const state = xfer->user_data;
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc, );

  switch(state) {
    // Note may need to read FTDI eeprom
    case CONFIG_FTDI_RESET:
      TU_ASSERT(ftdi_sio_reset(p_cdc, ftdi_process_config, CONFIG_FTDI_MODEM_CTRL),);
      break;

    case CONFIG_FTDI_MODEM_CTRL:
      #if CFG_TUH_CDC_LINE_CONTROL_ON_ENUM
      TU_ASSERT(ftdi_sio_set_modem_ctrl(p_cdc, CFG_TUH_CDC_LINE_CONTROL_ON_ENUM, ftdi_process_config, CONFIG_FTDI_SET_BAUDRATE),);
      break;
      #else
      TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_FTDI_SET_BAUDRATE: {
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
      cdc_line_coding_t line_coding = CFG_TUH_CDC_LINE_CODING_ON_ENUM;
      TU_ASSERT(ftdi_sio_set_baudrate(p_cdc, line_coding.bit_rate, ftdi_process_config, CONFIG_FTDI_SET_DATA),);
      break;
      #else
      TU_ATTR_FALLTHROUGH;
      #endif
    }

    case CONFIG_FTDI_SET_DATA: {
      #if 0 // TODO set data format
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
      cdc_line_coding_t line_coding = CFG_TUH_CDC_LINE_CODING_ON_ENUM;
      TU_ASSERT(ftdi_sio_set_data(p_cdc, process_ftdi_config, CONFIG_FTDI_COMPLETE),);
      break;
      #endif
      #endif

      TU_ATTR_FALLTHROUGH;
    }

    case CONFIG_FTDI_COMPLETE:
      set_config_complete(p_cdc, idx, itf_num);
      break;

    default:
      break;
  }
}

#endif

//--------------------------------------------------------------------+
// CP210x
//--------------------------------------------------------------------+

#if CFG_TUH_CDC_CP210X

enum {
  CONFIG_CP210X_IFC_ENABLE = 0,
  CONFIG_CP210X_SET_BAUDRATE,
  CONFIG_CP210X_SET_LINE_CTL,
  CONFIG_CP210X_SET_DTR_RTS,
  CONFIG_CP210X_COMPLETE
};

static bool cp210x_open(uint8_t daddr, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  // CP210x Interface includes 1 vendor interface + 2 bulk endpoints
  TU_VERIFY(itf_desc->bInterfaceSubClass == 0 && itf_desc->bInterfaceProtocol == 0 && itf_desc->bNumEndpoints == 2);
  TU_VERIFY(sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t) <= max_len);

  cdch_interface_t * p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY(p_cdc);

  TU_LOG_DRV("CP210x opened\r\n");
  p_cdc->serial_drid = SERIAL_DRIVER_CP210X;

  // endpoint pair
  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) tu_desc_next(itf_desc);

  // data endpoints expected to be in pairs
  return open_ep_stream_pair(p_cdc, desc_ep);
}

static bool cp210x_set_request(cdch_interface_t* p_cdc, uint8_t command, uint16_t value, uint8_t* buffer, uint16_t length, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_VENDOR,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = command,
    .wValue   = tu_htole16(value),
    .wIndex   = p_cdc->bInterfaceNumber,
    .wLength  = tu_htole16(length)
  };

  // use usbh enum buf since application variable does not live long enough
  uint8_t* enum_buf = NULL;

  if (buffer && length > 0) {
    enum_buf = usbh_get_enum_buf();
    tu_memcpy_s(enum_buf, CFG_TUH_ENUMERATION_BUFSIZE, buffer, length);
  }

  tuh_xfer_t xfer = {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = enum_buf,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

static bool cp210x_ifc_enable(cdch_interface_t* p_cdc, uint16_t enabled, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return cp210x_set_request(p_cdc, CP210X_IFC_ENABLE, enabled, NULL, 0, complete_cb, user_data);
}

static bool cp210x_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  // TODO implement later
  (void) p_cdc;
  (void) line_coding;
  (void) complete_cb;
  (void) user_data;
  return false;
}

static bool cp210x_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_DRV("CDC CP210x Set BaudRate = %lu\r\n", baudrate);
  uint32_t baud_le = tu_htole32(baudrate);
  p_cdc->user_control_cb = complete_cb;
  return cp210x_set_request(p_cdc, CP210X_SET_BAUDRATE, 0, (uint8_t *) &baud_le, 4,
                            complete_cb ? cdch_internal_control_complete : NULL, user_data);
}

static bool cp210x_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits,
                                   tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) p_cdc;
  (void) stop_bits;
  (void) parity;
  (void) data_bits;
  (void) complete_cb;
  (void) user_data;
  // TODO not implemented yet
  return false;
}

static bool cp210x_set_modem_ctrl(cdch_interface_t* p_cdc, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_LOG_DRV("CDC CP210x Set Control Line State\r\n");
  p_cdc->user_control_cb = complete_cb;
  return cp210x_set_request(p_cdc, CP210X_SET_MHS, 0x0300 | line_state, NULL, 0,
                            complete_cb ? cdch_internal_control_complete : NULL, user_data);
}

static void cp210x_process_config(tuh_xfer_t* xfer) {
  uintptr_t const state   = xfer->user_data;
  uint8_t const   itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t const   idx     = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t *p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);

  switch (state) {
    case CONFIG_CP210X_IFC_ENABLE:
      TU_ASSERT(cp210x_ifc_enable(p_cdc, 1, cp210x_process_config, CONFIG_CP210X_SET_BAUDRATE),);
      break;

    case CONFIG_CP210X_SET_BAUDRATE: {
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
      cdc_line_coding_t line_coding = CFG_TUH_CDC_LINE_CODING_ON_ENUM;
      TU_ASSERT(cp210x_set_baudrate(p_cdc, line_coding.bit_rate, cp210x_process_config, CONFIG_CP210X_SET_LINE_CTL),);
      break;
      #else
      TU_ATTR_FALLTHROUGH;
      #endif
    }

    case CONFIG_CP210X_SET_LINE_CTL: {
      #if defined(CFG_TUH_CDC_LINE_CODING_ON_ENUM) && 0 // skip for now
      cdc_line_coding_t line_coding = CFG_TUH_CDC_LINE_CODING_ON_ENUM;
      break;
      #else
      TU_ATTR_FALLTHROUGH;
      #endif
    }

    case CONFIG_CP210X_SET_DTR_RTS:
      #if CFG_TUH_CDC_LINE_CONTROL_ON_ENUM
      TU_ASSERT(cp210x_set_modem_ctrl(p_cdc, CFG_TUH_CDC_LINE_CONTROL_ON_ENUM, cp210x_process_config, CONFIG_CP210X_COMPLETE),);
      break;
      #else
      TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_CP210X_COMPLETE:
      set_config_complete(p_cdc, idx, itf_num);
      break;

    default: break;
  }
}

#endif

//--------------------------------------------------------------------+
// CH34x (CH340 & CH341)
//--------------------------------------------------------------------+

#if CFG_TUH_CDC_CH34X

static uint8_t ch34x_get_lcr(uint8_t stop_bits, uint8_t parity, uint8_t data_bits);
static uint16_t ch34x_get_divisor_prescaler(uint32_t baval);

//------------- control request -------------//

static bool ch34x_set_request(cdch_interface_t* p_cdc, uint8_t direction, uint8_t request, uint16_t value,
                              uint16_t index, uint8_t* buffer, uint16_t length, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request_setup = {
      .bmRequestType_bit = {
          .recipient = TUSB_REQ_RCPT_DEVICE,
          .type      = TUSB_REQ_TYPE_VENDOR,
          .direction = direction & 0x01u
      },
      .bRequest = request,
      .wValue   = tu_htole16 (value),
      .wIndex   = tu_htole16 (index),
      .wLength  = tu_htole16 (length)
  };

  // use usbh enum buf since application variable does not live long enough
  uint8_t* enum_buf = NULL;

  if (buffer && length > 0) {
    enum_buf = usbh_get_enum_buf();
    if (direction == TUSB_DIR_OUT) {
      tu_memcpy_s(enum_buf, CFG_TUH_ENUMERATION_BUFSIZE, buffer, length);
    }
  }

  tuh_xfer_t xfer = {
      .daddr       = p_cdc->daddr,
      .ep_addr     = 0,
      .setup       = &request_setup,
      .buffer      = enum_buf,
      .complete_cb = complete_cb,
      .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

static inline bool ch34x_control_out(cdch_interface_t* p_cdc, uint8_t request, uint16_t value, uint16_t index,
                                     tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return ch34x_set_request(p_cdc, TUSB_DIR_OUT, request, value, index, NULL, 0, complete_cb, user_data);
}

static inline bool ch34x_control_in(cdch_interface_t* p_cdc, uint8_t request, uint16_t value, uint16_t index,
                                    uint8_t* buffer, uint16_t buffersize, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return ch34x_set_request(p_cdc, TUSB_DIR_IN, request, value, index, buffer, buffersize,
                           complete_cb, user_data);
}

static bool ch34x_write_reg(cdch_interface_t* p_cdc, uint16_t reg, uint16_t reg_value, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return ch34x_control_out(p_cdc, CH34X_REQ_WRITE_REG, reg, reg_value, complete_cb, user_data);
}

//static bool ch34x_read_reg_request ( cdch_interface_t* p_cdc, uint16_t reg,
//                                     uint8_t *buffer, uint16_t buffersize, tuh_xfer_cb_t complete_cb, uintptr_t user_data )
//{
//  return ch34x_control_in ( p_cdc, CH34X_REQ_READ_REG, reg, 0, buffer, buffersize, complete_cb, user_data );
//}

static bool ch34x_write_reg_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate,
                                     tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  uint16_t const div_ps = ch34x_get_divisor_prescaler(baudrate);
  TU_VERIFY(div_ps != 0);
  TU_ASSERT(ch34x_write_reg(p_cdc, CH34X_REG16_DIVISOR_PRESCALER, div_ps,
                            complete_cb, user_data));
  return true;
}

//------------- Driver API -------------//

// internal control complete to update state such as line state, encoding
static void ch34x_control_complete(tuh_xfer_t* xfer) {
  // CH34x only has 1 interface and use wIndex as payload and not for bInterfaceNumber
  process_internal_control_complete(xfer, 0);
}

static bool ch34x_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits,
                                tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->requested_line_coding.stop_bits = stop_bits;
  p_cdc->requested_line_coding.parity = parity;
  p_cdc->requested_line_coding.data_bits = data_bits;

  uint8_t const lcr = ch34x_get_lcr(stop_bits, parity, data_bits);
  TU_VERIFY(lcr != 0);
  TU_ASSERT (ch34x_control_out(p_cdc, CH34X_REQ_WRITE_REG, CH32X_REG16_LCR2_LCR, lcr,
                               complete_cb ? ch34x_control_complete : NULL, user_data));
  return true;
}

static bool ch34x_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate,
                               tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->requested_line_coding.bit_rate = baudrate;
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(ch34x_write_reg_baudrate(p_cdc, baudrate,
                                     complete_cb ? ch34x_control_complete : NULL, user_data));
  return true;
}

static void ch34x_set_line_coding_stage1_complete(tuh_xfer_t* xfer) {
  // CH34x only has 1 interface and use wIndex as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc, );

  if (xfer->result == XFER_RESULT_SUCCESS) {
    // stage 1 success, continue to stage 2
    p_cdc->line_coding.bit_rate = p_cdc->requested_line_coding.bit_rate;
    TU_ASSERT(ch34x_set_data_format(p_cdc, p_cdc->requested_line_coding.stop_bits, p_cdc->requested_line_coding.parity,
                                    p_cdc->requested_line_coding.data_bits, ch34x_control_complete, xfer->user_data), );
  } else {
    // stage 1 failed, notify user
    xfer->complete_cb = p_cdc->user_control_cb;
    if (xfer->complete_cb) {
      xfer->complete_cb(xfer);
    }
  }
}

// 2 stages: set baudrate (stage1) + set data format (stage2)
static bool ch34x_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->requested_line_coding = *line_coding;
  p_cdc->user_control_cb = complete_cb;

  if (complete_cb) {
    // stage 1 set baudrate
    TU_ASSERT(ch34x_write_reg_baudrate(p_cdc, line_coding->bit_rate,
                                       ch34x_set_line_coding_stage1_complete, user_data));
  } else {
    // sync call
    xfer_result_t result;

    // stage 1 set baudrate
    TU_ASSERT(ch34x_write_reg_baudrate(p_cdc, line_coding->bit_rate, NULL, (uintptr_t) &result));
    TU_VERIFY(result == XFER_RESULT_SUCCESS);
    p_cdc->line_coding.bit_rate = line_coding->bit_rate;

    // stage 2 set data format
    TU_ASSERT(ch34x_set_data_format(p_cdc, line_coding->stop_bits, line_coding->parity, line_coding->data_bits,
                                    NULL, (uintptr_t) &result));
    TU_VERIFY(result == XFER_RESULT_SUCCESS);
    p_cdc->line_coding.stop_bits = line_coding->stop_bits;
    p_cdc->line_coding.parity = line_coding->parity;
    p_cdc->line_coding.data_bits = line_coding->data_bits;

    // update transfer result, user_data is expected to point to xfer_result_t
    if (user_data) {
      user_data = result;
    }
  }

  return true;
}

static bool ch34x_set_modem_ctrl(cdch_interface_t* p_cdc, uint16_t line_state,
                                 tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  uint8_t control = 0;
  if (line_state & CDC_CONTROL_LINE_STATE_RTS) {
    control |= CH34X_BIT_RTS;
  }
  if (line_state & CDC_CONTROL_LINE_STATE_DTR) {
    control |= CH34X_BIT_DTR;
  }

  // CH34x signals are inverted
  control = ~control;

  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT (ch34x_control_out(p_cdc, CH34X_REQ_MODEM_CTRL, control, 0,
                               complete_cb ? ch34x_control_complete : NULL, user_data));
  return true;
}

//------------- Enumeration -------------//
enum {
  CONFIG_CH34X_READ_VERSION = 0,
  CONFIG_CH34X_SERIAL_INIT,
  CONFIG_CH34X_SPECIAL_REG_WRITE,
  CONFIG_CH34X_FLOW_CONTROL,
  CONFIG_CH34X_MODEM_CONTROL,
  CONFIG_CH34X_COMPLETE
};

static bool ch34x_open(uint8_t daddr, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
  // CH34x Interface includes 1 vendor interface + 2 bulk + 1 interrupt endpoints
  TU_VERIFY (itf_desc->bNumEndpoints == 3);
  TU_VERIFY (sizeof(tusb_desc_interface_t) + 3 * sizeof(tusb_desc_endpoint_t) <= max_len);

  cdch_interface_t* p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY (p_cdc);

  TU_LOG_DRV ("CH34x opened\r\n");
  p_cdc->serial_drid = SERIAL_DRIVER_CH34X;

  tusb_desc_endpoint_t const* desc_ep = (tusb_desc_endpoint_t const*) tu_desc_next(itf_desc);

  // data endpoints expected to be in pairs
  TU_ASSERT(open_ep_stream_pair(p_cdc, desc_ep));
  desc_ep += 2;

  // Interrupt endpoint: not used for now
  TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(desc_ep) &&
            TUSB_XFER_INTERRUPT == desc_ep->bmAttributes.xfer);
  TU_ASSERT(tuh_edpt_open(daddr, desc_ep));
  p_cdc->ep_notif = desc_ep->bEndpointAddress;

  return true;
}

static void ch34x_process_config(tuh_xfer_t* xfer) {
  // CH34x only has 1 interface and use wIndex as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t* p_cdc = get_itf(idx);
  uintptr_t const state = xfer->user_data;
  uint8_t buffer[2]; // TODO remove
  TU_ASSERT (p_cdc,);

  // TODO check xfer->result

  switch (state) {
    case CONFIG_CH34X_READ_VERSION:
      TU_LOG_DRV("[%u] CDCh CH34x attempt to read Chip Version\r\n", p_cdc->daddr);
      TU_ASSERT (ch34x_control_in(p_cdc, CH34X_REQ_READ_VERSION, 0, 0, buffer, 2, ch34x_process_config, CONFIG_CH34X_SERIAL_INIT),);
      break;

    case CONFIG_CH34X_SERIAL_INIT: {
      // handle version read data, set CH34x line coding (incl. baudrate)
      uint8_t const version = xfer->buffer[0];
      TU_LOG_DRV("[%u] CDCh CH34x Chip Version = %02x\r\n", p_cdc->daddr, version);
      // only versions >= 0x30 are tested, below 0x30 seems having other programming, see drivers from WCH vendor, Linux kernel and FreeBSD
      TU_ASSERT (version >= 0x30,);
      // init CH34x with line coding
      cdc_line_coding_t const line_coding = CFG_TUH_CDC_LINE_CODING_ON_ENUM_CH34X;
      uint16_t const div_ps = ch34x_get_divisor_prescaler(line_coding.bit_rate);
      TU_ASSERT(div_ps != 0, );
      uint8_t const lcr = ch34x_get_lcr(line_coding.stop_bits, line_coding.parity, line_coding.data_bits);
      TU_ASSERT(lcr != 0, );
      TU_ASSERT (ch34x_control_out(p_cdc, CH34X_REQ_SERIAL_INIT, tu_u16(lcr, 0x9c), div_ps,
                                   ch34x_process_config, CONFIG_CH34X_SPECIAL_REG_WRITE),);
      break;
    }

    case CONFIG_CH34X_SPECIAL_REG_WRITE:
      // overtake line coding and do special reg write, purpose unknown, overtaken from WCH driver
      p_cdc->line_coding = ((cdc_line_coding_t) CFG_TUH_CDC_LINE_CODING_ON_ENUM_CH34X);
      TU_ASSERT (ch34x_write_reg(p_cdc, TU_U16(CH341_REG_0x0F, CH341_REG_0x2C), 0x0007, ch34x_process_config, CONFIG_CH34X_FLOW_CONTROL),);
      break;

    case CONFIG_CH34X_FLOW_CONTROL:
      // no hardware flow control
      TU_ASSERT (ch34x_write_reg(p_cdc, TU_U16(CH341_REG_0x27, CH341_REG_0x27), 0x0000, ch34x_process_config, CONFIG_CH34X_MODEM_CONTROL),);
      break;

    case CONFIG_CH34X_MODEM_CONTROL:
      // !always! set modem controls RTS/DTR (CH34x has no reset state after CH34X_REQ_SERIAL_INIT)
      TU_ASSERT (ch34x_set_modem_ctrl(p_cdc, CFG_TUH_CDC_LINE_CONTROL_ON_ENUM, ch34x_process_config, CONFIG_CH34X_COMPLETE),);
      break;

    case CONFIG_CH34X_COMPLETE:
      set_config_complete(p_cdc, idx, itf_num);
      break;

    default:
      TU_ASSERT (false,);
      break;
  }
}

//------------- CH34x helper  -------------//

// calculate divisor and prescaler for baudrate, return it as 16-bit combined value
static uint16_t ch34x_get_divisor_prescaler(uint32_t baval) {
  uint8_t a;
  uint8_t b;
  uint32_t c;

  TU_VERIFY(baval != 0, 0);
  switch (baval) {
    case 921600:
      a = 0xf3;
      b = 7;
      break;

    case 307200:
      a = 0xd9;
      b = 7;
      break;

    default:
      if (baval > 6000000 / 255) {
        b = 3;
        c = 6000000;
      } else if (baval > 750000 / 255) {
        b = 2;
        c = 750000;
      } else if (baval > 93750 / 255) {
        b = 1;
        c = 93750;
      } else {
        b = 0;
        c = 11719;
      }
      a = (uint8_t) (c / baval);
      if (a == 0 || a == 0xFF) {
        return 0;
      }
      if ((c / a - baval) > (baval - c / (a + 1))) {
        a++;
      }
      a = (uint8_t) (256 - a);
      break;
  }

  // reg divisor = a, reg prescaler = b
  // According to linux code we need to set bit 7 of UCHCOM_REG_BPS_PRE,
  // otherwise the chip will buffer data.
  return (uint16_t) ((uint16_t)a << 8 | 0x80 | b);
}

// calculate lcr value from data coding
static uint8_t ch34x_get_lcr(uint8_t stop_bits, uint8_t parity, uint8_t data_bits) {
  uint8_t lcr = CH34X_LCR_ENABLE_RX | CH34X_LCR_ENABLE_TX;
  TU_VERIFY(data_bits >= 5 && data_bits <= 8, 0);
  lcr |= (uint8_t) (data_bits - 5);

  switch(parity) {
    case CDC_LINE_CODING_PARITY_NONE:
      break;

    case CDC_LINE_CODING_PARITY_ODD:
      lcr |= CH34X_LCR_ENABLE_PAR;
      break;

    case CDC_LINE_CODING_PARITY_EVEN:
      lcr |= CH34X_LCR_ENABLE_PAR | CH34X_LCR_PAR_EVEN;
      break;

    case CDC_LINE_CODING_PARITY_MARK:
      lcr |= CH34X_LCR_ENABLE_PAR | CH34X_LCR_MARK_SPACE;
      break;

    case CDC_LINE_CODING_PARITY_SPACE:
      lcr |= CH34X_LCR_ENABLE_PAR | CH34X_LCR_MARK_SPACE | CH34X_LCR_PAR_EVEN;
      break;

    default: break;
  }

  // 1.5 stop bits not supported
  TU_VERIFY(stop_bits != CDC_LINE_CODING_STOP_BITS_1_5, 0);
  if (stop_bits == CDC_LINE_CODING_STOP_BITS_2) {
    lcr |= CH34X_LCR_STOP_BITS_2;
  }

  return lcr;
}

#endif // CFG_TUH_CDC_CH34X

//--------------------------------------------------------------------+
// PL2303
//--------------------------------------------------------------------+

#if CFG_TUH_CDC_PL2303

static int8_t pl2303_detect_type ( cdch_interface_t* p_cdc, tusb_desc_device_t const* desc, uint8_t step, 
                                   tuh_xfer_cb_t complete_cb, uintptr_t user_data );
static bool pl2303_encode_baud_rate(cdch_interface_t* p_cdc, uint32_t baud, uint8_t buf[PL2303_LINE_CODING_BAUDRATE_BUFSIZE]);

//------------- control request -------------//

static bool pl2303_set_request(cdch_interface_t* p_cdc, uint8_t request, uint8_t requesttype, uint16_t value,
                               uint16_t index, uint8_t* buffer, uint16_t length, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request_setup = {
    .bmRequestType = requesttype,
    .bRequest = request,
    .wValue   = tu_htole16 (value),
    .wIndex   = tu_htole16 (index),
    .wLength  = tu_htole16 (length)
  };

  // use usbh enum buf since application variable does not live long enough
  uint8_t* enum_buf = NULL;

  if (buffer && length > 0) {
    enum_buf = usbh_get_enum_buf();
    if (request_setup.bmRequestType_bit.direction == TUSB_DIR_OUT) {
      tu_memcpy_s(enum_buf, CFG_TUH_ENUMERATION_BUFSIZE, buffer, length);
    }
  }

  tuh_xfer_t xfer = {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request_setup,
    .buffer      = enum_buf,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

static bool pl2303_vendor_read(cdch_interface_t* p_cdc, uint16_t value, uint8_t* buf, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  uint8_t request = p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN] ? PL2303_VENDOR_READ_NREQUEST : PL2303_VENDOR_READ_REQUEST;
  return pl2303_set_request(p_cdc, request, PL2303_VENDOR_READ_REQUEST_TYPE, value, 0, buf, 1, complete_cb, user_data);
}

static bool pl2303_vendor_write(cdch_interface_t* p_cdc, uint16_t value, uint16_t index, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  uint8_t request = p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN] ? PL2303_VENDOR_WRITE_NREQUEST : PL2303_VENDOR_WRITE_REQUEST;
  return pl2303_set_request(p_cdc, request, PL2303_VENDOR_WRITE_REQUEST_TYPE, value, index, NULL, 0, complete_cb, user_data);
}

static bool pl2303_supports_hx_status ( cdch_interface_t* p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data )
{
  uint8_t buf;
  return pl2303_set_request(p_cdc, PL2303_VENDOR_READ_REQUEST, PL2303_VENDOR_READ_REQUEST_TYPE, PL2303_READ_TYPE_HX_STATUS, 0, &buf, 1, complete_cb, user_data);
}

static bool pl2303_set_control_lines(cdch_interface_t* p_cdc, uint8_t value, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  return pl2303_set_request(p_cdc, PL2303_SET_CONTROL_REQUEST, PL2303_SET_CONTROL_REQUEST_TYPE, value, 0, NULL, 0, complete_cb, user_data);
}

//static bool pl2303_get_line_request(cdch_interface_t* p_cdc, uint8_t buf[PL2303_LINE_CODING_BUFSIZE], tuh_xfer_cb_t complete_cb, uintptr_t user_data)
//{
//  return pl2303_set_request(p_cdc, PL2303_GET_LINE_REQUEST, PL2303_GET_LINE_REQUEST_TYPE, 0, 0, buf, PL2303_LINE_CODING_BUFSIZE, complete_cb, user_data);
//}

static bool pl2303_set_line_request(cdch_interface_t* p_cdc, uint8_t buf[PL2303_LINE_CODING_BUFSIZE], tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  return pl2303_set_request(p_cdc, PL2303_SET_LINE_REQUEST, PL2303_SET_LINE_REQUEST_TYPE, 0, 0, buf, PL2303_LINE_CODING_BUFSIZE, complete_cb, user_data);
}

//static bool pl2303_set_break(cdch_interface_t* p_cdc, bool enable, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
//{
//  uint16_t state = enable ? PL2303_BREAK_ON : PL2303_BREAK_OFF;
//  return pl2303_set_request(p_cdc, PL2303_BREAK_REQUEST, PL2303_BREAK_REQUEST_TYPE, state, 0, NULL, 0, complete_cb, user_data);
//}

int pl2303_clear_halt(cdch_interface_t* p_cdc, uint8_t endp, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  /* we don't care if it wasn't halted first. in fact some devices
   * (like some ibmcam model 1 units) seem to expect hosts to make
   * this request for iso endpoints, which can't halt!
   */
  return pl2303_set_request(p_cdc, TUSB_REQ_CLEAR_FEATURE, PL2303_CLEAR_HALT_REQUEST_TYPE, TUSB_REQ_FEATURE_EDPT_HALT, endp, NULL, 0, complete_cb, user_data);
}

//------------- Driver API -------------//

// internal control complete to update state such as line state, encoding
static void pl2303_control_complete(tuh_xfer_t* xfer) {
  // PL2303 only has 1 interface and use wIndex as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  process_internal_control_complete(xfer, itf_num);
}

static bool pl2303_set_line_coding(cdch_interface_t* p_cdc, cdc_line_coding_t const* line_coding,
                                   tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  uint8_t buf[PL2303_LINE_CODING_BUFSIZE];
  /*
   * Some PL2303 are known to lose bytes if you change serial settings
   * even to the same values as before. Thus we actually need to filter
   * in this specific case.
   *
   * Note that the tty_termios_hw_change check above is not sufficient
   * as a previously requested baud rate may differ from the one
   * actually used (and stored in old_termios).
   *
   * NOTE: No additional locking needed for line_settings as it is
   *       only used in set_termios, which is serialised against itself.
   */
  // TODO really necessary to check? what to do in this case when no transfer will happen?
  TU_ASSERT ( memcmp ( line_coding, &p_cdc->line_coding, PL2303_LINE_CODING_BUFSIZE ) != 0);

  p_cdc->requested_line_coding = *line_coding;

  /* For reference buf[6] data bits value */
  TU_VERIFY(line_coding->data_bits >= 5 && line_coding->data_bits <= 8, 0);
  buf[6] = line_coding->data_bits;

  /* For reference buf[0]:buf[3] baud rate value */
  pl2303_encode_baud_rate(p_cdc, line_coding->bit_rate, &buf[0]);

  /* For reference buf[4]=0 is 1 stop bits */
  /* For reference buf[4]=1 is 1.5 stop bits */
  /* For reference buf[4]=2 is 2 stop bits */
  buf[4] = line_coding->stop_bits; // TinyUSB has the same coding

  /* For reference buf[5]=0 is none parity */
  /* For reference buf[5]=1 is odd parity */
  /* For reference buf[5]=2 is even parity */
  /* For reference buf[5]=3 is mark parity */
  /* For reference buf[5]=4 is space parity */
  buf[5] = line_coding->parity; // TinyUSB has the same coding

  return pl2303_set_line_request(p_cdc, buf, complete_cb ? pl2303_control_complete : NULL, user_data);
}

static bool pl2303_set_data_format(cdch_interface_t* p_cdc, uint8_t stop_bits, uint8_t parity, uint8_t data_bits,
    tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdc_line_coding_t line_coding = p_cdc->line_coding;
  line_coding.data_bits = data_bits;
  line_coding.parity = parity;
  line_coding.stop_bits = stop_bits;
  return pl2303_set_line_coding(p_cdc, &line_coding, complete_cb, user_data);
}

static bool pl2303_set_baudrate(cdch_interface_t* p_cdc, uint32_t baudrate,
    tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdc_line_coding_t line_coding = p_cdc->line_coding;
  line_coding.bit_rate = baudrate;
  return pl2303_set_line_coding(p_cdc, &line_coding, complete_cb, user_data);
}

static bool pl2303_set_modem_ctrl(cdch_interface_t* p_cdc, uint16_t line_state,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  // PL2303 has the same bit coding
  TU_ASSERT (pl2303_set_control_lines(p_cdc, line_state, complete_cb ? pl2303_control_complete : NULL, user_data));
  return true;
}

//------------- Enumeration -------------//

enum {
  CONFIG_PL2303_GET_DESC = 0,
  CONFIG_PL2303_DETECT_TYPE,
  CONFIG_PL2303_READ1,
  CONFIG_PL2303_WRITE1,
  CONFIG_PL2303_READ2,
  CONFIG_PL2303_READ3,
  CONFIG_PL2303_READ4,
  CONFIG_PL2303_WRITE2,
  CONFIG_PL2303_READ5,
  CONFIG_PL2303_READ6,
  CONFIG_PL2303_WRITE3,
  CONFIG_PL2303_WRITE4,
  CONFIG_PL2303_WRITE5,
  CONFIG_PL2303_RESET_ENDP1,
  CONFIG_PL2303_RESET_ENDP2,
  CONFIG_PL2303_LINE_CODING,
  CONFIG_PL2303_MODEM_CONTROL,
  CONFIG_PL2303_FLOW_CTRL_READ,
  CONFIG_PL2303_FLOW_CTRL_WRITE,
  CONFIG_PL2303_COMPLETE
};

static bool pl2303_open(uint8_t daddr, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
  // PL2303 Interface includes 1 vendor interface + 1 interrupt endpoints + 2 bulk
  TU_VERIFY (itf_desc->bNumEndpoints == 3);
  TU_VERIFY (sizeof(tusb_desc_interface_t) + 3 * sizeof(tusb_desc_endpoint_t) <= max_len);

  cdch_interface_t* p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY (p_cdc);

  TU_LOG_DRV ("PL2303 opened\r\n");
  p_cdc->serial_drid = SERIAL_DRIVER_PL2303;
  p_cdc->pl2303.serial_private.quirks = 0;
  p_cdc->pl2303.supports_hx_status = false;

  tusb_desc_endpoint_t const* desc_ep = (tusb_desc_endpoint_t const*) tu_desc_next(itf_desc);

  // Interrupt endpoint: not used for now
  TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(desc_ep) &&
            TUSB_XFER_INTERRUPT == desc_ep->bmAttributes.xfer);
  TU_ASSERT(tuh_edpt_open(daddr, desc_ep));
  p_cdc->ep_notif = desc_ep->bEndpointAddress;
  desc_ep += 1;

  // data endpoints expected to be in pairs
  TU_ASSERT(open_ep_stream_pair(p_cdc, desc_ep));

  return true;
}

static void pl2303_process_config(tuh_xfer_t* xfer) {
  // PL2303 only has 1 interface and use wIndex as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t* p_cdc = get_itf(idx);
  uintptr_t const state = xfer->user_data;
  uint8_t buf;
  int8_t type;
  TU_ASSERT (p_cdc,);

  TU_ASSERT (xfer->result == XFER_RESULT_SUCCESS ||
             state == CONFIG_PL2303_READ1, ); // state CONFIG_PL2303_READ1 may have no success due to pl2303_supports_hx_status()

  switch (state) {

    // from here sequence overtaken from Linux Kernel function pl2303_startup()
    case CONFIG_PL2303_GET_DESC:
      TU_ASSERT(tuh_descriptor_get_device(xfer->daddr, pl2303_desc[idx], sizeof(tusb_desc_device_t),
                                          pl2303_process_config, CONFIG_PL2303_DETECT_TYPE), );
      break;

    case CONFIG_PL2303_DETECT_TYPE:
      // get type and quirks (step 1)
      type = pl2303_detect_type ( p_cdc, pl2303_desc[idx], 1, pl2303_process_config, CONFIG_PL2303_READ1 ); // step 1
      TU_ASSERT(type!=PL2303_DETECT_TYPE_FAILED,);
      if ( type == PL2303_SUPPORTS_HX_STATUS_TRIGGERED ) {
        break;
      } // else: no transfer triggered and continue with CONFIG_PL2303_READ1

    case CONFIG_PL2303_READ1:
      // get supports_hx_status, type and quirks (step 2), do special read
      p_cdc->pl2303.supports_hx_status = ( // will not be true, if coming directly from previous case
        xfer->setup->bRequest == PL2303_VENDOR_READ_REQUEST &&
        xfer->setup->bmRequestType == PL2303_VENDOR_READ_REQUEST_TYPE &&
        xfer->setup->wValue == PL2303_READ_TYPE_HX_STATUS &&
        xfer->result == XFER_RESULT_SUCCESS );
      type = pl2303_detect_type ( p_cdc, pl2303_desc[idx], 2, NULL, 0 ); // step 2 now with supports_hx_status
      TU_ASSERT(type!=PL2303_DETECT_TYPE_FAILED,);
      p_cdc->pl2303.serial_private.type = &pl2303_type_data[type];
      p_cdc->pl2303.serial_private.quirks |= p_cdc->pl2303.serial_private.type->quirks;
      #if CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL && 0 // can be activated if necessary
        TU_LOG(CFG_TUH_CDC_LOG_LEVEL, "PL2303 bDeviceClass = 0x%02x bMaxPacketSize0 = %u bcdUSB = 0x%04x bcdDevice = 0x%04x\r\n",
               pl2303_desc[idx]->bDeviceClass, pl2303_desc[idx]->bMaxPacketSize0, pl2303_desc[idx]->bcdUSB, pl2303_desc[idx]->bcdDevice );
        uint16_t vid, pid;
        TU_ASSERT(tuh_vid_pid_get(p_cdc->daddr, &vid, &pid),);
        TU_LOG(CFG_TUH_CDC_LOG_LEVEL, "       vid = 0x%04x pid = 0x%04x supports_hx_status = %u type = %s quirks = %u\r\n",
               vid, pid, p_cdc->pl2303.supports_hx_status, p_cdc->pl2303.serial_private.type->name, p_cdc->pl2303.serial_private.quirks);
      #endif
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_read(p_cdc, 0x8484, &buf, pl2303_process_config, CONFIG_PL2303_WRITE1), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_WRITE1:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_write(p_cdc, 0x0404, 0, pl2303_process_config, CONFIG_PL2303_READ2), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_READ2:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_read(p_cdc, 0x8484, &buf, pl2303_process_config, CONFIG_PL2303_READ3), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_READ3:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_read(p_cdc, 0x8383, &buf, pl2303_process_config, CONFIG_PL2303_READ4), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_READ4:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_read(p_cdc, 0x8484, &buf, pl2303_process_config, CONFIG_PL2303_WRITE2), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_WRITE2:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_write(p_cdc, 0x0404, 1, pl2303_process_config, CONFIG_PL2303_READ5), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_READ5:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_read(p_cdc, 0x8484, &buf, pl2303_process_config, CONFIG_PL2303_READ6), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_READ6:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_read(p_cdc, 0x8383, &buf, pl2303_process_config, CONFIG_PL2303_WRITE3), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_WRITE3:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_write(p_cdc, 0, 1, pl2303_process_config, CONFIG_PL2303_WRITE4), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_WRITE4:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_write(p_cdc, 1, 0, pl2303_process_config, CONFIG_PL2303_WRITE5), );
        break;
      } // else: continue with next step

    case CONFIG_PL2303_WRITE5:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        if (p_cdc->pl2303.serial_private.quirks & PL2303_QUIRK_LEGACY) {
          TU_ASSERT(pl2303_vendor_write(p_cdc, 2, 0x24, pl2303_process_config, CONFIG_PL2303_RESET_ENDP1), );
        } else {
          TU_ASSERT(pl2303_vendor_write(p_cdc, 2, 0x44, pl2303_process_config, CONFIG_PL2303_RESET_ENDP1), );
        }
        break;
      } // else: continue with next step

    // from here sequence overtaken from Linux Kernel function pl2303_open()
    case CONFIG_PL2303_RESET_ENDP1:
      // step 1
      if (p_cdc->pl2303.serial_private.quirks & PL2303_QUIRK_LEGACY) {
        TU_ASSERT(pl2303_clear_halt(p_cdc, PL2303_OUT_EP, pl2303_process_config, CONFIG_PL2303_RESET_ENDP2),);
      } else {
        /* reset upstream data pipes */
        if (p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN]) {
          TU_ASSERT(pl2303_vendor_write(p_cdc, PL2303_HXN_RESET_REG,
                                        PL2303_HXN_RESET_UPSTREAM_PIPE | PL2303_HXN_RESET_DOWNSTREAM_PIPE,
                                        pl2303_process_config, CONFIG_PL2303_LINE_CODING),); // skip CONFIG_PL2303_RESET_ENDP2, no 2nd step
        } else {
          pl2303_vendor_write(p_cdc, 8, 0, pl2303_process_config, CONFIG_PL2303_RESET_ENDP2);
        }
      }
      break;

    case CONFIG_PL2303_RESET_ENDP2:
      // step 2
      if (p_cdc->pl2303.serial_private.quirks & PL2303_QUIRK_LEGACY) {
        TU_ASSERT(pl2303_clear_halt(p_cdc, PL2303_IN_EP, pl2303_process_config, CONFIG_PL2303_LINE_CODING),);
      } else {
        /* reset upstream data pipes */
        if (p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN]) {
          // here nothing to do, only structure of previous step overtaken for better reading and comparison
        } else {
          TU_ASSERT(pl2303_vendor_write(p_cdc, 9, 0, pl2303_process_config, CONFIG_PL2303_LINE_CODING),);
        }
      }
      break;

    // from here sequence overtaken from Linux Kernel function pl2303_set_termios()
    // unnecessary pl2303_get_line_request() is skipped due to a stall
    case CONFIG_PL2303_LINE_CODING:
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
        cdc_line_coding_t const line_coding = CFG_TUH_CDC_LINE_CODING_ON_ENUM;
        TU_ASSERT ( pl2303_set_line_coding(p_cdc, &line_coding, pl2303_process_config, CONFIG_PL2303_MODEM_CONTROL ),);
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_PL2303_MODEM_CONTROL:
      #if CFG_TUH_CDC_LINE_CONTROL_ON_ENUM
        TU_ASSERT (pl2303_set_modem_ctrl(p_cdc, CFG_TUH_CDC_LINE_CONTROL_ON_ENUM, pl2303_process_config, CONFIG_PL2303_FLOW_CTRL_READ),);
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_PL2303_FLOW_CTRL_READ:
      // read flow control register for modify & write back in next step
      if (p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT(pl2303_vendor_read(p_cdc, PL2303_HXN_FLOWCTRL_REG, &buf, pl2303_process_config, CONFIG_PL2303_FLOW_CTRL_WRITE), );
      } else {
        TU_ASSERT(pl2303_vendor_read(p_cdc, 0, &buf, pl2303_process_config, CONFIG_PL2303_FLOW_CTRL_WRITE), );
      }
      break;

    case CONFIG_PL2303_FLOW_CTRL_WRITE:
      // no flow control
      buf = xfer->buffer[0];
      if (p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN]) {
        buf &= ~PL2303_HXN_FLOWCTRL_MASK;
        buf |= PL2303_HXN_FLOWCTRL_NONE;
        TU_ASSERT(pl2303_vendor_write(p_cdc, PL2303_HXN_FLOWCTRL_REG, buf, pl2303_process_config, CONFIG_PL2303_COMPLETE), );
      } else {
        buf &= ~PL2303_FLOWCTRL_MASK;
        TU_ASSERT(pl2303_vendor_write(p_cdc, 0, buf, pl2303_process_config, CONFIG_PL2303_COMPLETE), );
      }
      break;

    case CONFIG_PL2303_COMPLETE:
      set_config_complete(p_cdc, idx, itf_num);
      break;

    default:
      TU_ASSERT (false,);
      break;
  }
}

//------------- PL2303 helper  -------------//

static int8_t pl2303_detect_type ( cdch_interface_t* p_cdc, tusb_desc_device_t const* desc, uint8_t step, 
                                   tuh_xfer_cb_t complete_cb, uintptr_t user_data )
{
  /*
   * Legacy PL2303H, variants 0 and 1 (difference unknown).
   */
  if (desc->bDeviceClass == 0x02) {
    return TYPE_H;    /* variant 0 */
  }

  if (desc->bMaxPacketSize0 != 0x40) {
    if (desc->bDeviceClass == 0x00 || desc->bDeviceClass == 0xff) {
      return TYPE_H;  /* variant 1 */
    }
    return TYPE_H;    /* variant 0 */
  }

  switch (desc->bcdUSB) {
    case 0x101:
      /* USB 1.0.1? Let's assume they meant 1.1... */
      TU_ATTR_FALLTHROUGH;
    case 0x110:
      switch (desc->bcdDevice) {
        case 0x300:
          return TYPE_HX;
        case 0x400:
          return TYPE_HXD;
        default:
          return TYPE_HX;
      }
      break;
    case 0x200:
      switch (desc->bcdDevice) {
        case 0x100: /* GC */
        case 0x105:
          return TYPE_HXN;
        case 0x300: /* GT / TA */
          if ( step == 1 ) {
            // step 1 trigger pl2303_supports_hx_status() request
            TU_ASSERT ( pl2303_supports_hx_status ( p_cdc, complete_cb, user_data ), PL2303_DETECT_TYPE_FAILED);
            return PL2303_SUPPORTS_HX_STATUS_TRIGGERED;
          } else {
            // step 2 use supports_hx_status
            if (p_cdc->pl2303.supports_hx_status) {
              return TYPE_TA;
            }
          }
          TU_ATTR_FALLTHROUGH;
        case 0x305:
        case 0x400: /* GL */
        case 0x405:
          return TYPE_HXN;
        case 0x500: /* GE / TB */
          if ( step == 1 ) {
            // step 1 trigger pl2303_supports_hx_status() request
            TU_ASSERT ( pl2303_supports_hx_status ( p_cdc, complete_cb, user_data ), PL2303_DETECT_TYPE_FAILED);
            return PL2303_SUPPORTS_HX_STATUS_TRIGGERED;
          } else {
            // step 2 use supports_hx_status
            if (p_cdc->pl2303.supports_hx_status) {
              return TYPE_TB;
            }
          }
          TU_ATTR_FALLTHROUGH;
        case 0x505:
        case 0x600: /* GS */
        case 0x605:
        case 0x700: /* GR */
        case 0x705:
          return TYPE_HXN;
      }
      break;
  }

  return PL2303_DETECT_TYPE_FAILED;
}

/*
 * Returns the nearest supported baud rate that can be set directly without
 * using divisors.
 */
static uint32_t pl2303_get_supported_baud_rate(uint32_t baud)
{
  static const uint32_t baud_sup[] = {
    75, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600,
    14400, 19200, 28800, 38400, 57600, 115200, 230400, 460800,
    614400, 921600, 1228800, 2457600, 3000000, 6000000
  };

  uint8_t i;
  for (i = 0; i < TU_ARRAY_SIZE(baud_sup); ++i) {
    if (baud_sup[i] > baud) {
      break;
    }
  }

  if (i == TU_ARRAY_SIZE(baud_sup)) {
    baud = baud_sup[i - 1];
  } else if (i > 0 && (baud_sup[i] - baud) > (baud - baud_sup[i - 1])) {
    baud = baud_sup[i - 1];
  } else {
    baud = baud_sup[i];
  }

  return baud;
}

/*
 * NOTE: If unsupported baud rates are set directly, the PL2303 seems to
 *       use 9600 baud.
 */
static uint32_t pl2303_encode_baud_rate_direct(uint8_t buf[PL2303_LINE_CODING_BAUDRATE_BUFSIZE], uint32_t baud)
{
  uint32_t* buf32 = (uint32_t*)buf;
  *buf32 = tu_htole32(baud);

  return baud;
}

static uint32_t pl2303_encode_baud_rate_divisor(uint8_t buf[PL2303_LINE_CODING_BAUDRATE_BUFSIZE], uint32_t baud)
{
  uint32_t baseline, mantissa, exponent;

  /*
   * Apparently the formula is:
   *   baudrate = 12M * 32 / (mantissa * 4^exponent)
   * where
   *   mantissa = buf[8:0]
   *   exponent = buf[11:9]
   */
  baseline = 12000000 * 32;
  mantissa = baseline / baud;
  if (mantissa == 0)
    mantissa = 1; /* Avoid dividing by zero if baud > 32*12M. */
  exponent = 0;
  while (mantissa >= 512) {
    if (exponent < 7) {
      mantissa >>= 2; /* divide by 4 */
      exponent++;
    } else {
      /* Exponent is maxed. Trim mantissa and leave. */
      mantissa = 511;
      break;
    }
  }

  buf[3] = 0x80;
  buf[2] = 0;
  buf[1] = exponent << 1 | mantissa >> 8;
  buf[0] = mantissa & 0xff;

  /* Calculate and return the exact baud rate. */
  baud = (baseline / mantissa) >> (exponent << 1);

  return baud;
}

static uint32_t pl2303_encode_baud_rate_divisor_alt(uint8_t buf[PL2303_LINE_CODING_BAUDRATE_BUFSIZE], uint32_t baud)
{
  uint32_t baseline, mantissa, exponent;

  /*
   * Apparently, for the TA version the formula is:
   *   baudrate = 12M * 32 / (mantissa * 2^exponent)
   * where
   *   mantissa = buf[10:0]
   *   exponent = buf[15:13 16]
   */
  baseline = 12000000 * 32;
  mantissa = baseline / baud;
  if (mantissa == 0) {
    mantissa = 1;   /* Avoid dividing by zero if baud > 32*12M. */
  }
  exponent = 0;
  while (mantissa >= 2048) {
    if (exponent < 15) {
      mantissa >>= 1; /* divide by 2 */
      exponent++;
    } else {
      /* Exponent is maxed. Trim mantissa and leave. */
      mantissa = 2047;
      break;
    }
  }

  buf[3] = 0x80;
  buf[2] = exponent & 0x01;
  buf[1] = (exponent & ~0x01) << 4 | mantissa >> 8;
  buf[0] = mantissa & 0xff;

  /* Calculate and return the exact baud rate. */
  baud = (baseline / mantissa) >> exponent;

  return baud;
}

static bool pl2303_encode_baud_rate(cdch_interface_t* p_cdc, uint32_t baud, uint8_t buf[PL2303_LINE_CODING_BAUDRATE_BUFSIZE])
{
  uint32_t baud_sup;

  TU_ASSERT(baud && baud < p_cdc->pl2303.serial_private.type->max_baud_rate);
  /*
   * Use direct method for supported baud rates, otherwise use divisors.
   * Newer chip types do not support divisor encoding.
   */
  if (p_cdc->pl2303.serial_private.type->no_divisors) {
    baud_sup = baud;
  } else {
    baud_sup = pl2303_get_supported_baud_rate(baud);
  }

  if (baud == baud_sup) {
    baud = pl2303_encode_baud_rate_direct(buf, baud);
  } else if (p_cdc->pl2303.serial_private.type->alt_divisors) {
    baud = pl2303_encode_baud_rate_divisor_alt(buf, baud);
  } else {
    baud = pl2303_encode_baud_rate_divisor(buf, baud);
  }
  return true;
}

#endif // CFG_TUH_CDC_PL2303

#endif
