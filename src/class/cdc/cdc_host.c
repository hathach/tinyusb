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
 * - Heiko Kuester: add support of CH34x & PL2303, improve support of FTDI & CP210x
 */

#include "tusb_option.h"

#if (CFG_TUH_ENABLED && CFG_TUH_CDC)

#include "host/usbh.h"
#include "host/usbh_pvt.h"

#include "cdc_host.h"
#include "serial/ftdi_sio.h"
#include "serial/cp210x.h"
#include "serial/ch34x.h"
#include "serial/pl2303.h"

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUH_CDC_LOG_LEVEL
  #define CFG_TUH_CDC_LOG_LEVEL   CFG_TUH_LOG_LEVEL
#endif

#define TU_LOG_DRV(...)                        TU_LOG(CFG_TUH_CDC_LOG_LEVEL, __VA_ARGS__)
#define TU_LOG_CDC(TXT,DADDR,ITF_NUM,NAME,...) TU_LOG_DRV("[:%u:%u] CDCh %s " TXT "\r\n", \
                                                          DADDR, ITF_NUM, NAME, ##__VA_ARGS__)
#define TU_LOG_P_CDC(TXT,...)                  TU_LOG_CDC(TXT, p_cdc->daddr, p_cdc->bInterfaceNumber, \
                                                          serial_drivers[p_cdc->serial_drid].name, ##__VA_ARGS__)
#define TU_LOG_P_CDC_BOOL(TXT,VAL)             TU_LOG_P_CDC(TXT " " #VAL " = %d", VAL)

// assert and set config complete
#define TU_ASSERT_COMPLETE_DEFINE(_cond, _itf_offset)                                                \
  do {                                                                                               \
    if (!(_cond)) { _MESS_FAILED(); TU_BREAKPOINT(); set_config_complete(idx, _itf_offset, false); } \
  } while(0)

#define TU_ASSERT_COMPLETE_1ARGS(_cond)               TU_ASSERT_COMPLETE_DEFINE(_cond, 0)
#define TU_ASSERT_COMPLETE_2ARGS(_cond, _itf_offset)  TU_ASSERT_COMPLETE_DEFINE(_cond, _itf_offset)

#define TU_ASSERT_COMPLETE(...) _GET_3RD_ARG(__VA_ARGS__, TU_ASSERT_COMPLETE_2ARGS, TU_ASSERT_COMPLETE_1ARGS, _dummy)(__VA_ARGS__)

// handle line control defines
#if defined(CFG_TUH_CDC_LINE_CONTROL_ON_ENUM) && \
    (defined(CFG_TUH_CDC_DTR_CONTROL_ON_ENUM) || defined(CFG_TUH_CDC_RTS_CONTROL_ON_ENUM))
  TU_VERIFY_STATIC(false, "Contradictory line control defines");
#endif

#ifdef CFG_TUH_CDC_LINE_CONTROL_ON_ENUM
  #define LINE_CONTROL_ON_ENUM CFG_TUH_CDC_LINE_CONTROL_ON_ENUM
#elif defined(CFG_TUH_CDC_DTR_CONTROL_ON_ENUM) || defined(CFG_TUH_CDC_RTS_CONTROL_ON_ENUM)
  #ifndef CFG_TUH_CDC_DTR_CONTROL_ON_ENUM
    #define CFG_TUH_CDC_DTR_CONTROL_ON_ENUM 0
  #endif
  #ifndef CFG_TUH_CDC_RTS_CONTROL_ON_ENUM
    #define CFG_TUH_CDC_RTS_CONTROL_ON_ENUM 0
  #endif
  #define LINE_CONTROL_ON_ENUM ( ( CFG_TUH_CDC_DTR_CONTROL_ON_ENUM ? CDC_CONTROL_LINE_STATE_DTR : 0 ) | \
                                 ( CFG_TUH_CDC_RTS_CONTROL_ON_ENUM ? CDC_CONTROL_LINE_STATE_RTS : 0 ) )
#endif

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
  TU_ATTR_ALIGNED(4) cdc_line_coding_t requested_line_coding;
  // 1 byte padding

  cdc_line_control_state_t line_state;
  cdc_line_control_state_t requested_line_state;

  tuh_xfer_cb_t user_control_cb;
  #if CFG_TUH_CDC_FTDI || CFG_TUH_CDC_CP210X || CFG_TUH_CDC_CH34X
    tuh_xfer_cb_t requested_complete_cb;
  #endif

  #if CFG_TUH_CDC_FTDI
    ftdi_private_t ftdi;
  #endif

  #if CFG_TUH_CDC_PL2303
    pl2303_private_t pl2303;
  #endif

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

#if CFG_TUH_CDC_FTDI || CFG_TUH_CDC_PL2303
  static tusb_desc_device_t desc_dev[CFG_TUH_ENUMERATION_BUFSIZE];
#endif

//--------------------------------------------------------------------+
// Serial Driver
//--------------------------------------------------------------------+

//------------- ACM prototypes -------------//
static bool acm_open(uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
static void acm_process_config(tuh_xfer_t * xfer);

static bool acm_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool acm_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool acm_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool acm_set_control_line_state(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

//------------- FTDI prototypes -------------//
#if CFG_TUH_CDC_FTDI
static uint16_t const ftdi_vid_pid_list[][2] = {CFG_TUH_CDC_FTDI_VID_PID_LIST};
#if CFG_TUSB_DEBUG && CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL
  static uint8_t const * ftdi_chip_name[] = { FTDI_CHIP_NAMES };
#endif

static bool ftdi_open(uint8_t daddr, const tusb_desc_interface_t * itf_desc, uint16_t max_len);
static void ftdi_process_config(tuh_xfer_t * xfer);

static bool ftdi_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ftdi_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ftdi_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ftdi_set_modem_ctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
#endif

//------------- CP210X prototypes -------------//
#if CFG_TUH_CDC_CP210X
static uint16_t const cp210x_vid_pid_list[][2] = {CFG_TUH_CDC_CP210X_VID_PID_LIST};

static bool cp210x_open(uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
static void cp210x_process_config(tuh_xfer_t * xfer);

static bool cp210x_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool cp210x_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool cp210x_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool cp210x_set_modem_ctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
#endif

//------------- CH34x prototypes -------------//
#if CFG_TUH_CDC_CH34X
static uint16_t const ch34x_vid_pid_list[][2] = {CFG_TUH_CDC_CH34X_VID_PID_LIST};

static bool ch34x_open(uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
static void ch34x_process_config(tuh_xfer_t * xfer);

static bool ch34x_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ch34x_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ch34x_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool ch34x_set_modem_ctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
#endif

//------------- PL2303 prototypes -------------//
#if CFG_TUH_CDC_PL2303
static uint16_t const pl2303_vid_pid_list[][2] = {CFG_TUH_CDC_PL2303_VID_PID_LIST};
static const struct pl2303_type_data pl2303_type_data[TYPE_COUNT] = {PL2303_TYPE_DATA};

CFG_TUH_MEM_SECTION CFG_TUH_MEM_ALIGN

static bool pl2303_open(uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
static void pl2303_process_config(tuh_xfer_t * xfer);

static bool pl2303_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool pl2303_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool pl2303_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool pl2303_set_modem_ctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
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
  bool (*const open)(uint8_t daddr, const tusb_desc_interface_t * itf_desc, uint16_t max_len);
  void (*const process_set_config)(tuh_xfer_t * xfer);
  bool (*const set_control_line_state)(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
  bool (*const set_baudrate)(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
  bool (*const set_data_format)(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
  bool (*const set_line_coding)(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
  #if CFG_TUSB_DEBUG && CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL
    uint8_t const * name;
  #endif
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
      .set_line_coding        = acm_set_line_coding,
    #if CFG_TUSB_DEBUG && CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL
      .name                   = (uint8_t const *) "ACM"
    #endif
  },

  #if CFG_TUH_CDC_FTDI
  {
      .vid_pid_list           = ftdi_vid_pid_list,
      .vid_pid_count          = TU_ARRAY_SIZE(ftdi_vid_pid_list),
      .open                   = ftdi_open,
      .process_set_config     = ftdi_process_config,
      .set_control_line_state = ftdi_set_modem_ctrl,
      .set_baudrate           = ftdi_set_baudrate,
      .set_data_format        = ftdi_set_data_format,
      .set_line_coding        = ftdi_set_line_coding,
    #if CFG_TUSB_DEBUG && CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL
      .name                   = (uint8_t const *) "FTDI"
    #endif
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
      .set_line_coding        = cp210x_set_line_coding,
    #if CFG_TUSB_DEBUG && CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL
      .name                   = (uint8_t const *) "CP210x"
    #endif
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
      .set_line_coding        = ch34x_set_line_coding,
    #if CFG_TUSB_DEBUG && CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL
      .name                   = (uint8_t const *) "CH34x"
    #endif
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
      .set_line_coding        = pl2303_set_line_coding,
    #if CFG_TUSB_DEBUG && CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL
      .name                   = (uint8_t const *) "PL2303"
    #endif
  }
  #endif
};

TU_VERIFY_STATIC(TU_ARRAY_SIZE(serial_drivers) == SERIAL_DRIVER_COUNT, "Serial driver count mismatch");

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

static inline cdch_interface_t * get_itf(uint8_t idx) {
  TU_ASSERT(idx < CFG_TUH_CDC, NULL);
  cdch_interface_t * p_cdc = &cdch_data[idx];

  return (p_cdc->daddr != 0) ? p_cdc : NULL;
}

static inline uint8_t get_idx_by_ep_addr(uint8_t daddr, uint8_t ep_addr) {
  for(uint8_t i=0; i<CFG_TUH_CDC; i++) {
    cdch_interface_t * p_cdc = &cdch_data[i];
    if ((p_cdc->daddr == daddr) &&
         (ep_addr == p_cdc->ep_notif || ep_addr == p_cdc->stream.rx.ep_addr || ep_addr == p_cdc->stream.tx.ep_addr)) {
      return i;
    }
  }

  return TUSB_INDEX_INVALID_8;
}

static cdch_interface_t * make_new_itf(uint8_t daddr, tusb_desc_interface_t const * itf_desc) {
  for(uint8_t i=0; i<CFG_TUH_CDC; i++) {
    if (cdch_data[i].daddr == 0) {
      cdch_interface_t * p_cdc = &cdch_data[i];
      p_cdc->daddr              = daddr;
      p_cdc->bInterfaceNumber   = itf_desc->bInterfaceNumber;
      p_cdc->bInterfaceSubClass = itf_desc->bInterfaceSubClass;
      p_cdc->bInterfaceProtocol = itf_desc->bInterfaceProtocol;
      p_cdc->line_coding        = (cdc_line_coding_t) { 0, 0, 0, 0 };
      p_cdc->line_state.all     = 0;
      return p_cdc;
    }
  }

  return NULL;
}

static bool open_ep_stream_pair(cdch_interface_t * p_cdc , tusb_desc_endpoint_t const *desc_ep);

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

uint8_t tuh_cdc_itf_get_index(uint8_t daddr, uint8_t itf_num) {
  for (uint8_t i = 0; i < CFG_TUH_CDC; i++) {
    const cdch_interface_t * p_cdc = &cdch_data[i];
    if (p_cdc->daddr == daddr && p_cdc->bInterfaceNumber == itf_num) return i;
  }

  return TUSB_INDEX_INVALID_8;
}

bool tuh_cdc_itf_get_info(uint8_t idx, tuh_itf_info_t * info) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && info);

  info->daddr = p_cdc->daddr;

  // re-construct descriptor
  tusb_desc_interface_t * desc = &info->desc;
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
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);
  return p_cdc->mounted;
}

bool tuh_cdc_get_dtr(uint8_t idx) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  bool ret = p_cdc->line_state.dtr;
//  TU_LOG_P_CDC_BOOL("get DTR", ret);

  return ret;
}

bool tuh_cdc_get_rts(uint8_t idx) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  bool ret = p_cdc->line_state.rts;
//  TU_LOG_P_CDC_BOOL("get RTS", ret);

  return ret;
}

bool tuh_cdc_get_local_line_coding(uint8_t idx, cdc_line_coding_t * line_coding) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  *line_coding = p_cdc->line_coding;
  TU_LOG_P_CDC("get line coding %lu %u%c%s",
               p_cdc->line_coding.bit_rate, p_cdc->line_coding.data_bits,
               CDC_LINE_CODING_PARITY_CHAR(p_cdc->line_coding.parity),
               CDC_LINE_CODING_STOP_BITS_TEXT(line_coding->stop_bits));

  return true;
}

//--------------------------------------------------------------------+
// Write
//--------------------------------------------------------------------+

uint32_t tuh_cdc_write(uint8_t idx, void const * buffer, uint32_t bufsize) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write(&p_cdc->stream.tx, buffer, bufsize);
}

uint32_t tuh_cdc_write_flush(uint8_t idx) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write_xfer(&p_cdc->stream.tx);
}

bool tuh_cdc_write_clear(uint8_t idx) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_clear(&p_cdc->stream.tx);
}

uint32_t tuh_cdc_write_available(uint8_t idx) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write_available(&p_cdc->stream.tx);
}

//--------------------------------------------------------------------+
// Read
//--------------------------------------------------------------------+

uint32_t tuh_cdc_read (uint8_t idx, void * buffer, uint32_t bufsize) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_read(&p_cdc->stream.rx, buffer, bufsize);
}

uint32_t tuh_cdc_read_available(uint8_t idx) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_read_available(&p_cdc->stream.rx);
}

bool tuh_cdc_peek(uint8_t idx, uint8_t * ch) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_peek(&p_cdc->stream.rx, ch);
}

bool tuh_cdc_read_clear (uint8_t idx) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  bool ret = tu_edpt_stream_clear(&p_cdc->stream.rx);
  tu_edpt_stream_read_xfer(&p_cdc->stream.rx);

  return ret;
}

//--------------------------------------------------------------------+
// Control Endpoint API
//--------------------------------------------------------------------+

// set line coding using sequence with 2 stages: set baudrate (stage1) + set data format (stage2)
static bool set_line_coding_sequence(
    cdch_interface_t * p_cdc,
    // control request function to set baudrate
    bool (*set_baudrate_request)(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data),
    // control request function to set data format
    bool (*set_data_format_request)(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data),
    // function to be called after stage 1 completed
    void (*set_line_coding_stage1_complete)(tuh_xfer_t * xfer),
    // control complete function to be called after request
    void (*internal_control_complete)(tuh_xfer_t * xfer),
    tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  if (complete_cb) {
    // non-blocking
    // stage 1 set baudrate
    p_cdc->requested_complete_cb = complete_cb; // store complete_cb to be used in set_line_coding_stage1_complete()
    p_cdc->user_control_cb = set_line_coding_stage1_complete;
    return set_baudrate_request(p_cdc, internal_control_complete, user_data);
  } else {
    // blocking sequence
    // stage 1 set baudrate
    xfer_result_t result = XFER_RESULT_INVALID; // use local result, because user_data ptr may be NULL
    bool ret = set_baudrate_request(p_cdc, NULL, (uintptr_t) &result);

    if (user_data) {
      *((xfer_result_t *) user_data) = result;
    }

    TU_ASSERT(ret);
    TU_VERIFY(result == XFER_RESULT_SUCCESS);

    // overtake baudrate after successful request
    p_cdc->line_coding.bit_rate = p_cdc->requested_line_coding.bit_rate;

    // stage 2 set data format
    result = XFER_RESULT_INVALID;
    ret = set_data_format_request(p_cdc, NULL, (uintptr_t) &result);

    if (user_data) {
      *((xfer_result_t *) user_data) = result;
    }

    TU_ASSERT(ret);
    return (result == XFER_RESULT_SUCCESS);
    // the overtaking of remaining requested_line_coding will be done in tuh_cdc_set_line_coding()
  }
}

static void set_line_coding_stage1_complete(
    tuh_xfer_t * xfer, uint8_t const itf_num,
    // control request function to set data format
    bool (*set_data_format_request)(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data),
    // control complete function to be called after request
    void (*internal_control_complete)(tuh_xfer_t * xfer)) {
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);

  if (xfer->result == XFER_RESULT_SUCCESS) {
    // stage 1 success, continue with stage 2
    p_cdc->user_control_cb = p_cdc->requested_complete_cb;
    set_data_format_request(p_cdc, internal_control_complete, xfer->user_data);
  } else {
    // stage 1 failed, notify user
    xfer->complete_cb = p_cdc->requested_complete_cb;
    if (xfer->complete_cb) {
      xfer->complete_cb(xfer);
    }
  }
}

// call of (non-)blocking set-functions (to set line state, baudrate, ...)
static bool set_function_call (
    cdch_interface_t * p_cdc,
    bool (*set_function)(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data),
    tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  if (complete_cb) {
    // non-blocking with call back
    return set_function(p_cdc, complete_cb, user_data);
  } else {
    // blocking
    xfer_result_t result = XFER_RESULT_INVALID; // use local result, because user_data ptr may be NULL
    bool ret = set_function(p_cdc, NULL, (uintptr_t) &result);

    if (user_data) {
      *((xfer_result_t *) user_data) = result;
    }

    return (ret && result == XFER_RESULT_SUCCESS);
  }
}

bool tuh_cdc_set_control_line_state_u(uint8_t idx, cdc_line_control_state_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  // uses cdc_line_control_state_t union for line_state
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  TU_LOG_P_CDC("set control line state dtr = %u rts = %u", line_state.dtr, line_state.rts );
  cdch_serial_driver_t const * driver = &serial_drivers[p_cdc->serial_drid];

  p_cdc->requested_line_state = line_state;

  bool ret = set_function_call(p_cdc, driver->set_control_line_state, complete_cb, user_data);

  if (ret && !complete_cb) {
    p_cdc->line_state = line_state;
  }
//  TU_LOG_P_CDC_BOOL("set control line state", ret);

  return ret;
}

bool tuh_cdc_set_control_line_state(uint8_t idx, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  // uses uint16_t for line_state => DTR (bit 0), RTS (bit 1)

  return tuh_cdc_set_control_line_state_u(idx, (cdc_line_control_state_t) { .all = (uint8_t) line_state },
                                          complete_cb, user_data);
}

bool tuh_cdc_set_dtr(uint8_t idx, bool dtr_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  cdc_line_control_state_t const line_state = { .dtr = dtr_state, .rts = p_cdc->line_state.rts };

  return tuh_cdc_set_control_line_state_u(idx, line_state, complete_cb, user_data);
}

bool tuh_cdc_set_rts(uint8_t idx, bool rts_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  cdc_line_control_state_t const line_state = { .rts = rts_state, .dtr = p_cdc->line_state.dtr };

  return tuh_cdc_set_control_line_state_u(idx, line_state, complete_cb, user_data);
}

bool tuh_cdc_set_baudrate(uint8_t idx, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  TU_LOG_P_CDC("set baudrate %lu", baudrate);
  cdch_serial_driver_t const * driver = &serial_drivers[p_cdc->serial_drid];

  p_cdc->requested_line_coding.bit_rate = baudrate;

  bool ret = set_function_call(p_cdc, driver->set_baudrate, complete_cb, user_data);

  if (ret && !complete_cb) {
    p_cdc->line_coding.bit_rate = baudrate;
  }
//  TU_LOG_P_CDC_BOOL("set baudrate", ret);

  return ret;
}

bool tuh_cdc_set_data_format(uint8_t idx, uint8_t stop_bits, uint8_t parity, uint8_t data_bits,
                             tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  TU_LOG_P_CDC("set data format %u%c%s",
               data_bits, CDC_LINE_CODING_PARITY_CHAR(parity),
               CDC_LINE_CODING_STOP_BITS_TEXT(stop_bits));
  cdch_serial_driver_t const * driver = &serial_drivers[p_cdc->serial_drid];

  p_cdc->requested_line_coding.stop_bits = stop_bits;
  p_cdc->requested_line_coding.parity    = parity;
  p_cdc->requested_line_coding.data_bits = data_bits;

  bool ret = set_function_call(p_cdc, driver->set_data_format, complete_cb, user_data);

  if (ret && !complete_cb) {
    p_cdc->line_coding.stop_bits = stop_bits;
    p_cdc->line_coding.parity    = parity;
    p_cdc->line_coding.data_bits = data_bits;
  }
//  TU_LOG_P_CDC_BOOL("set data format", ret);

  return ret;
}

bool tuh_cdc_set_line_coding(uint8_t idx, cdc_line_coding_t const * line_coding,
                             tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  TU_LOG_P_CDC("set line coding %lu %u%c%s",
               line_coding->bit_rate, line_coding->data_bits,
               CDC_LINE_CODING_PARITY_CHAR(line_coding->parity),
               CDC_LINE_CODING_STOP_BITS_TEXT(line_coding->stop_bits));
  cdch_serial_driver_t const * driver = &serial_drivers[p_cdc->serial_drid];

  p_cdc->requested_line_coding = *line_coding;

  bool ret = set_function_call(p_cdc, driver->set_line_coding, complete_cb, user_data);

  if (ret && !complete_cb) {
    p_cdc->line_coding = *line_coding;
  }
//  TU_LOG_P_CDC_BOOL("set line coding", ret);

  return ret;
}

//--------------------------------------------------------------------+
// CLASS-USBH API
//--------------------------------------------------------------------+

bool cdch_init(void) {
  TU_LOG_DRV("sizeof(cdch_interface_t) = %u\r\n", sizeof(cdch_interface_t));
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

  return true;
}

bool cdch_deinit(void) {
  for (size_t i = 0; i < CFG_TUH_CDC; i++) {
    cdch_interface_t* p_cdc = &cdch_data[i];
    tu_edpt_stream_deinit(&p_cdc->stream.tx);
    tu_edpt_stream_deinit(&p_cdc->stream.rx);
  }
  return true;
}

void cdch_close(uint8_t daddr) {
  for (uint8_t idx = 0; idx < CFG_TUH_CDC; idx++) {
    cdch_interface_t * p_cdc = &cdch_data[idx];
    if (p_cdc->daddr == daddr) {
      TU_LOG_P_CDC("close");

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

  if (ep_addr == p_cdc->stream.tx.ep_addr) {
    // invoke tx complete callback to possibly refill tx fifo
    if (tuh_cdc_tx_complete_cb) tuh_cdc_tx_complete_cb(idx);

    if (0 == tu_edpt_stream_write_xfer(&p_cdc->stream.tx)) {
      // If there is no data left, a ZLP should be sent if:
      // - xferred_bytes is multiple of EP Packet size and not zero
      tu_edpt_stream_write_zlp_if_needed(&p_cdc->stream.tx, xferred_bytes);
    }
  } else if (ep_addr == p_cdc->stream.rx.ep_addr) {
    #if CFG_TUH_CDC_FTDI
    if (p_cdc->serial_drid == SERIAL_DRIVER_FTDI) {
      // FTDI reserve 2 bytes for status
      // uint8_t status[2] = {p_cdc->stream.rx.ep_buf[0], p_cdc->stream.rx.ep_buf[1]};
      tu_edpt_stream_read_xfer_complete_offset(&p_cdc->stream.rx, xferred_bytes, 2);
    } else
    #endif
    {
      tu_edpt_stream_read_xfer_complete(&p_cdc->stream.rx, xferred_bytes);
    }

    // invoke receive callback
    if (tuh_cdc_rx_cb) {
      tuh_cdc_rx_cb(idx);
    }

    // prepare for next transfer if needed
    tu_edpt_stream_read_xfer(&p_cdc->stream.rx);
  } else if (ep_addr == p_cdc->ep_notif) {
    // TODO handle notification endpoint
  } else {
    TU_ASSERT(false);
  }

  return true;
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+

static bool open_ep_stream_pair(cdch_interface_t * p_cdc, tusb_desc_endpoint_t const * desc_ep) {
  for (size_t i = 0; i < 2; i++) {
    TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType &&
              TUSB_XFER_BULK == desc_ep->bmAttributes.xfer);
    TU_ASSERT(tuh_edpt_open(p_cdc->daddr, desc_ep));

    if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
      tu_edpt_stream_open(&p_cdc->stream.rx, p_cdc->daddr, desc_ep);
    } else {
      tu_edpt_stream_open(&p_cdc->stream.tx, p_cdc->daddr, desc_ep);
    }

    desc_ep = (tusb_desc_endpoint_t const *) tu_desc_next(desc_ep);
  }

  return true;
}

bool cdch_open(uint8_t rhport, uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len) {
  (void) rhport;
  cdch_serial_driver_t const * driver_detected = NULL;

  // For CDC: only support ACM subclass
  // Note: Protocol 0xFF can be RNDIS device
  if (TUSB_CLASS_CDC                           == itf_desc->bInterfaceClass &&
      CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == itf_desc->bInterfaceSubClass) {
    driver_detected = &serial_drivers[0];
  } else if (SERIAL_DRIVER_COUNT > 1 && TUSB_CLASS_VENDOR_SPECIFIC == itf_desc->bInterfaceClass) {
    uint16_t vid, pid;
    TU_VERIFY(tuh_vid_pid_get(daddr, &vid, &pid));

    for (size_t dr = 1; dr < SERIAL_DRIVER_COUNT; dr++) {
      cdch_serial_driver_t const * driver = &serial_drivers[dr];
      for (size_t i = 0; i < driver->vid_pid_count; i++) {
        if (driver->vid_pid_list[i][0] == vid && driver->vid_pid_list[i][1] == pid) {
          driver_detected = driver;
          break;
        }
      }
      if (driver_detected) {
        break;
      }
    }
  }

  if (driver_detected) {
    TU_LOG_CDC("open", daddr, itf_desc->bInterfaceNumber, driver_detected->name);
    bool ret = driver_detected->open(daddr, itf_desc, max_len);
//    TU_LOG_CDC("opened ret = %s", daddr, itf_desc->bInterfaceNumber, driver_detected->name, ret ? "true" : "FALSE" );
    return ret;
  }

  return false;
}

static void set_config_complete(uint8_t idx, uint8_t itf_offset, bool success) {
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);
  TU_LOG_P_CDC_BOOL("set config complete", success);

  if (success) {
    p_cdc->mounted = true;
    if (tuh_cdc_mount_cb) {
      tuh_cdc_mount_cb(idx);
    }
    // Prepare for incoming data
    tu_edpt_stream_read_xfer(&p_cdc->stream.rx);
  } else {
    // clear the interface entry
    p_cdc->daddr = 0;
    p_cdc->bInterfaceNumber = 0;
  }

  // notify usbh that driver enumeration is complete
  usbh_driver_set_config_complete(p_cdc->daddr, p_cdc->bInterfaceNumber + itf_offset);
}

bool cdch_set_config(uint8_t daddr, uint8_t itf_num) {
  tusb_control_request_t request;
  request.wIndex = tu_htole16((uint16_t) itf_num);
  uint8_t const idx = tuh_cdc_itf_get_index(daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc && p_cdc->serial_drid < SERIAL_DRIVER_COUNT);
  TU_LOG_P_CDC("set config");

  // fake transfer to kick-off process_set_config()
  tuh_xfer_t xfer;
  xfer.daddr  = daddr;
  xfer.result = XFER_RESULT_SUCCESS;
  xfer.setup  = &request;
  xfer.user_data = 0; // initial state 0

  serial_drivers[p_cdc->serial_drid].process_set_config(&xfer);

  return true;
}

//--------------------------------------------------------------------+
// ACM
//--------------------------------------------------------------------+

//------------- Driver API -------------//

// internal control complete to update state such as line state, encoding
static void acm_internal_control_complete(tuh_xfer_t * xfer) {
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);
  bool const success = (xfer->result == XFER_RESULT_SUCCESS);
  TU_LOG_P_CDC_BOOL("control complete", success);

  if (success) {
    switch (xfer->setup->bRequest) {
      case CDC_REQUEST_SET_CONTROL_LINE_STATE:
        p_cdc->line_state = p_cdc->requested_line_state;
        break;

      case CDC_REQUEST_SET_LINE_CODING:
        p_cdc->line_coding = p_cdc->requested_line_coding;
        break;

      default: break;
    }
  }

  xfer->complete_cb = p_cdc->user_control_cb;
  if (xfer->complete_cb) {
    xfer->complete_cb(xfer);
  }
}

static bool acm_set_control_line_state(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(p_cdc->acm_capability.support_line_request);

  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE,
    .wValue   = tu_htole16((uint16_t) p_cdc->requested_line_state.all),
    .wIndex   = tu_htole16((uint16_t) p_cdc->bInterfaceNumber),
    .wLength  = 0
  };

  p_cdc->user_control_cb = complete_cb;

  tuh_xfer_t xfer = {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = NULL,
    .complete_cb = complete_cb ? acm_internal_control_complete : NULL,
    .user_data   = user_data
  };

  TU_ASSERT(tuh_control_xfer(&xfer));

  return true;
}

static bool acm_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(p_cdc->acm_capability.support_line_request);
  TU_VERIFY((p_cdc->requested_line_coding.data_bits >= 5 && p_cdc->requested_line_coding.data_bits <= 8) ||
             p_cdc->requested_line_coding.data_bits == 16);

  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = CDC_REQUEST_SET_LINE_CODING,
    .wValue   = 0,
    .wIndex   = tu_htole16((uint16_t) p_cdc->bInterfaceNumber),
    .wLength  = tu_htole16((uint16_t) sizeof(cdc_line_coding_t))
  };

  // use usbh enum buf to hold line coding since user line_coding variable does not live long enough
  uint8_t * enum_buf = usbh_get_enum_buf();
  memcpy(enum_buf, &p_cdc->requested_line_coding, sizeof(cdc_line_coding_t));

  p_cdc->user_control_cb = complete_cb;

  tuh_xfer_t xfer = {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = enum_buf,
    .complete_cb = complete_cb ? acm_internal_control_complete : NULL,
    .user_data   = user_data
  };

  TU_ASSERT(tuh_control_xfer(&xfer));

  return true;
}

static bool acm_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->requested_line_coding.bit_rate = p_cdc->line_coding.bit_rate;

  return acm_set_line_coding(p_cdc, complete_cb, user_data);
}

static bool acm_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->requested_line_coding.stop_bits = p_cdc->line_coding.stop_bits;
  p_cdc->requested_line_coding.parity    = p_cdc->line_coding.parity;
  p_cdc->requested_line_coding.data_bits = p_cdc->line_coding.data_bits;

  return acm_set_line_coding(p_cdc, complete_cb, user_data);
}

//------------- Enumeration -------------//

enum {
  CONFIG_ACM_SET_CONTROL_LINE_STATE = 0,
  CONFIG_ACM_SET_LINE_CODING,
  CONFIG_ACM_COMPLETE,
};

static bool acm_open(uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len) {
  uint8_t const * p_desc_end = ((uint8_t const *) itf_desc) + max_len;

  cdch_interface_t * p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY(p_cdc);

  p_cdc->serial_drid = SERIAL_DRIVER_ACM;

  //------------- Control Interface -------------//
  uint8_t const * p_desc = tu_desc_next(itf_desc);

  // Communication Functional Descriptors
  while ((p_desc < p_desc_end) && (TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc))) {
    if (CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT == cdc_functional_desc_typeof(p_desc)) {
      // save ACM bmCapabilities
      p_cdc->acm_capability = ((cdc_desc_func_acm_t const *) p_desc)->bmCapabilities;
    }

    p_desc = tu_desc_next(p_desc);
  }

  // Open notification endpoint of control interface if any
  if (itf_desc->bNumEndpoints == 1) {
    TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc));
    tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;

    TU_ASSERT(tuh_edpt_open(daddr, desc_ep));
    p_cdc->ep_notif = desc_ep->bEndpointAddress;

    p_desc = tu_desc_next(p_desc);
  }

  //------------- Data Interface (if any) -------------//
  if ((TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) &&
      (TUSB_CLASS_CDC_DATA == ((tusb_desc_interface_t const *) p_desc)->bInterfaceClass)) {
    // next to endpoint descriptor
    p_desc = tu_desc_next(p_desc);

    // data endpoints expected to be in pairs
    TU_ASSERT(open_ep_stream_pair(p_cdc, (tusb_desc_endpoint_t const *) p_desc));
  }

  return true;
}

static void acm_process_config(tuh_xfer_t * xfer) {
  uintptr_t const state = xfer->user_data;
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT_COMPLETE(p_cdc && xfer->result == XFER_RESULT_SUCCESS, 1);

  switch (state) {
    case CONFIG_ACM_SET_CONTROL_LINE_STATE:
      #ifdef LINE_CONTROL_ON_ENUM
        if (p_cdc->acm_capability.support_line_request) {
          p_cdc->requested_line_state.all = LINE_CONTROL_ON_ENUM;
          TU_ASSERT_COMPLETE(acm_set_control_line_state(p_cdc, acm_process_config, CONFIG_ACM_SET_LINE_CODING), 1);
          break;
        }
      #endif
      TU_ATTR_FALLTHROUGH;

    case CONFIG_ACM_SET_LINE_CODING:
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
        if (p_cdc->acm_capability.support_line_request) {
          p_cdc->requested_line_coding = (cdc_line_coding_t) CFG_TUH_CDC_LINE_CODING_ON_ENUM;
          TU_ASSERT_COMPLETE(acm_set_line_coding(p_cdc, acm_process_config, CONFIG_ACM_COMPLETE), 1);
          break;
        }
      #endif
      TU_ATTR_FALLTHROUGH;

    case CONFIG_ACM_COMPLETE:
      // itf_num+1 to account for data interface as well
      set_config_complete(idx, 1, true);
      break;

    default:
      set_config_complete(idx, 1, false);
      break;
  }
}

//--------------------------------------------------------------------+
// FTDI
//--------------------------------------------------------------------+
#if CFG_TUH_CDC_FTDI

static bool ftdi_determine_type(cdch_interface_t * p_cdc);
static uint32_t ftdi_get_divisor(cdch_interface_t * p_cdc);
static uint8_t ftdi_get_idx(tuh_xfer_t * xfer);

//------------- Control Request -------------//

// set request without data
static bool ftdi_set_request(cdch_interface_t * p_cdc, uint8_t request, uint8_t requesttype,
                             uint16_t value, uint16_t index, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request_setup = {
    .bmRequestType = requesttype,
    .bRequest      = request,
    .wValue        = tu_htole16(value),
    .wIndex        = tu_htole16(index),
    .wLength       = 0
  };

  tuh_xfer_t xfer = {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request_setup,
    .buffer      = NULL,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

#ifdef CFG_TUH_CDC_FTDI_LATENCY
static int8_t ftdi_write_latency_timer(cdch_interface_t * p_cdc, uint16_t latency,
                                       tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  if (p_cdc->ftdi.chip_type == SIO /* || p_cdc->ftdi.chip_type == FT232A */ )
    return FTDI_NOT_POSSIBLE;
  return ftdi_set_request(p_cdc, FTDI_SIO_SET_LATENCY_TIMER_REQUEST, FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE,
                          latency, p_cdc->ftdi.channel, complete_cb, user_data) ? FTDI_REQUESTED : FTDI_FAIL;
}
#endif

static inline bool ftdi_sio_reset(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return ftdi_set_request(p_cdc, FTDI_SIO_RESET_REQUEST, FTDI_SIO_RESET_REQUEST_TYPE, FTDI_SIO_RESET_SIO,
                          p_cdc->ftdi.channel, complete_cb, user_data);
}

static bool ftdi_change_speed(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  uint32_t index_value = ftdi_get_divisor(p_cdc);
  TU_VERIFY(index_value);
  uint16_t value = (uint16_t) index_value;
  uint16_t index = (uint16_t) (index_value >> 16);
  if (p_cdc->ftdi.channel) {
    index = (uint16_t)((index << 8) | p_cdc->ftdi.channel);
  }

  return ftdi_set_request(p_cdc, FTDI_SIO_SET_BAUDRATE_REQUEST, FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE,
                          value, index, complete_cb, user_data);
}

static bool ftdi_set_data_request(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(p_cdc->requested_line_coding.data_bits >= 7 && p_cdc->requested_line_coding.data_bits <= 8, 0);
  uint16_t value = (uint16_t) (
    (p_cdc->requested_line_coding.data_bits & 0xfUL)       |  // data bit quantity is stored in bits 0-3
    (p_cdc->requested_line_coding.parity    & 0x7UL) <<  8 |  // parity is stored in bits 8-10, same coding
    (p_cdc->requested_line_coding.stop_bits & 0x3UL) << 11 ); // stop bits quantity is stored in bits 11-12, same coding
                                                                        // not each FTDI supports 1.5 stop bits

  return ftdi_set_request(p_cdc, FTDI_SIO_SET_DATA_REQUEST, FTDI_SIO_SET_DATA_REQUEST_TYPE,
                          value, p_cdc->ftdi.channel, complete_cb, user_data);
}

static inline bool ftdi_update_mctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  uint16_t value = (uint16_t) ((p_cdc->requested_line_state.dtr ? FTDI_SIO_SET_DTR_HIGH : FTDI_SIO_SET_DTR_LOW) |
                               (p_cdc->requested_line_state.rts ? FTDI_SIO_SET_RTS_HIGH : FTDI_SIO_SET_RTS_LOW));

  return ftdi_set_request(p_cdc, FTDI_SIO_SET_MODEM_CTRL_REQUEST, FTDI_SIO_SET_MODEM_CTRL_REQUEST_TYPE,
                          value, p_cdc->ftdi.channel, complete_cb, user_data);
}

//------------- Driver API -------------//

// internal control complete to update state such as line state, line_coding
static void ftdi_internal_control_complete(tuh_xfer_t * xfer) {
  uint8_t const idx = ftdi_get_idx(xfer);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);
  bool const success = (xfer->result == XFER_RESULT_SUCCESS);
  TU_LOG_P_CDC_BOOL("control complete", success);

  if (success) {
    if (xfer->setup->bRequest      == FTDI_SIO_SET_MODEM_CTRL_REQUEST &&
        xfer->setup->bmRequestType == FTDI_SIO_SET_MODEM_CTRL_REQUEST_TYPE ) {
      p_cdc->line_state = p_cdc->requested_line_state;
    }
    if (xfer->setup->bRequest      == FTDI_SIO_SET_DATA_REQUEST &&
        xfer->setup->bmRequestType == FTDI_SIO_SET_DATA_REQUEST_TYPE ) {
      p_cdc->line_coding.stop_bits = p_cdc->requested_line_coding.stop_bits;
      p_cdc->line_coding.parity    = p_cdc->requested_line_coding.parity;
      p_cdc->line_coding.data_bits = p_cdc->requested_line_coding.data_bits;
    }
    if (xfer->setup->bRequest      == FTDI_SIO_SET_BAUDRATE_REQUEST &&
        xfer->setup->bmRequestType == FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE ) {
      p_cdc->line_coding.bit_rate = p_cdc->requested_line_coding.bit_rate;
    }
  }

  xfer->complete_cb = p_cdc->user_control_cb;
  if (xfer->complete_cb) {
    xfer->complete_cb(xfer);
  }
}

static bool ftdi_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(ftdi_set_data_request(p_cdc, complete_cb ? ftdi_internal_control_complete : NULL, user_data));

  return true;
}

static bool ftdi_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(ftdi_change_speed(p_cdc, complete_cb ? ftdi_internal_control_complete : NULL, user_data));

  return true;
}

static void ftdi_set_line_coding_stage1_complete(tuh_xfer_t * xfer) {
  uint8_t const idx = ftdi_get_idx(xfer);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);
  uint8_t const itf_num = p_cdc->bInterfaceNumber;
  set_line_coding_stage1_complete(xfer, itf_num,
                                  ftdi_set_data_request,           // control request function to set data format
                                  ftdi_internal_control_complete); // control complete function to be called after request
}

// 2 stages: set baudrate (stage1) + set data format (stage2)
static bool ftdi_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return set_line_coding_sequence(p_cdc,
                                  ftdi_change_speed,                    // control request function to set baudrate
                                  ftdi_set_data_request,                // control request function to set data format
                                  ftdi_set_line_coding_stage1_complete, // function to be called after stage 1 completed
                                  ftdi_internal_control_complete,       // control complete function to be called after request
                                  complete_cb, user_data);
}

static bool ftdi_set_modem_ctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(ftdi_update_mctrl(p_cdc, complete_cb ? ftdi_internal_control_complete : NULL, user_data));

  return true;
}

//------------- Enumeration -------------//

enum {
  CONFIG_FTDI_GET_DESC = 0,
  CONFIG_FTDI_DETERMINE_TYPE,
  CONFIG_FTDI_WRITE_LATENCY,
  CONFIG_FTDI_SIO_RESET,
  CONFIG_FTDI_SET_DATA,
  CONFIG_FTDI_SET_BAUDRATE,
  CONFIG_FTDI_FLOW_CONTROL,
  CONFIG_FTDI_MODEM_CTRL,
  CONFIG_FTDI_COMPLETE
};

static bool ftdi_open(uint8_t daddr, const tusb_desc_interface_t * itf_desc, uint16_t max_len) {
  // FTDI Interface includes 1 vendor interface + 2 bulk endpoints
  TU_VERIFY(itf_desc->bInterfaceSubClass == 0xff && itf_desc->bInterfaceProtocol == 0xff &&
            itf_desc->bNumEndpoints == 2);
  TU_VERIFY(sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t) <= max_len);

  cdch_interface_t * p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY(p_cdc);

  p_cdc->serial_drid = SERIAL_DRIVER_FTDI;

  // endpoint pair
  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) tu_desc_next(itf_desc);

  /*
   * NOTE: Some customers have programmed FT232R/FT245R devices
   * with an endpoint size of 0 - not good.
   */
  TU_ASSERT(desc_ep->wMaxPacketSize != 0);

  // data endpoints expected to be in pairs
  return open_ep_stream_pair(p_cdc, desc_ep);
}

static void ftdi_process_config(tuh_xfer_t * xfer) {
  uintptr_t const state = xfer->user_data;
  uint8_t const idx = ftdi_get_idx(xfer);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT_COMPLETE(p_cdc && xfer->result == XFER_RESULT_SUCCESS);
  uint8_t const itf_num = p_cdc->bInterfaceNumber;

  switch(state) {

    // from here sequence overtaken from Linux Kernel function ftdi_port_probe()
    case CONFIG_FTDI_GET_DESC:
      // get device descriptor
      p_cdc->user_control_cb = ftdi_process_config; // set once for whole process config
      if (itf_num == 0) { // only necessary for 1st interface. other interface overtake type from interface 0
        TU_ASSERT_COMPLETE(tuh_descriptor_get_device(xfer->daddr, &desc_dev, sizeof(tusb_desc_device_t),
                                                     ftdi_process_config, CONFIG_FTDI_DETERMINE_TYPE));
        break;
      }
      TU_ATTR_FALLTHROUGH;

    case CONFIG_FTDI_DETERMINE_TYPE:
      // determine type
      if (itf_num == 0) {
        TU_ASSERT_COMPLETE(ftdi_determine_type(p_cdc));
      } else {
        // other interfaces have same type as interface 0
        uint8_t const idx_itf0 = tuh_cdc_itf_get_index(xfer->daddr, 0);
        cdch_interface_t const * p_cdc_itf0 = get_itf(idx_itf0);
        TU_ASSERT_COMPLETE(p_cdc_itf0);
        if (p_cdc_itf0) {
         p_cdc->ftdi.chip_type = p_cdc_itf0->ftdi.chip_type;
        }
      }
      TU_ATTR_FALLTHROUGH;

    case CONFIG_FTDI_WRITE_LATENCY:
      #ifdef CFG_TUH_CDC_FTDI_LATENCY
        int8_t result = ftdi_write_latency_timer(p_cdc, CFG_TUH_CDC_FTDI_LATENCY, ftdi_process_config,
                                                 CONFIG_FTDI_SIO_RESET);
        TU_ASSERT_COMPLETE(result != FTDI_FAIL);
        if(result == FTDI_REQUESTED) {
          break;
        } // else FTDI_NOT_POSSIBLE => continue directly with next state
      #endif
      TU_ATTR_FALLTHROUGH;

    // from here sequence overtaken from Linux Kernel function ftdi_open()
    case CONFIG_FTDI_SIO_RESET:
      TU_ASSERT_COMPLETE(ftdi_sio_reset(p_cdc, ftdi_process_config, CONFIG_FTDI_SET_DATA));
      break;

    // from here sequence overtaken from Linux Kernel function ftdi_set_termios()
    case CONFIG_FTDI_SET_DATA:
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
        p_cdc->requested_line_coding = (cdc_line_coding_t) CFG_TUH_CDC_LINE_CODING_ON_ENUM;
        TU_ASSERT_COMPLETE(ftdi_set_data_request(p_cdc, ftdi_internal_control_complete, CONFIG_FTDI_SET_BAUDRATE));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_FTDI_SET_BAUDRATE:
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
        TU_ASSERT_COMPLETE(ftdi_change_speed(p_cdc, ftdi_internal_control_complete, CONFIG_FTDI_FLOW_CONTROL));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_FTDI_FLOW_CONTROL:
      // disable flow control
      TU_ASSERT_COMPLETE(ftdi_set_request(p_cdc, FTDI_SIO_SET_FLOW_CTRL_REQUEST, FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
                                          0, FTDI_SIO_DISABLE_FLOW_CTRL, ftdi_process_config, CONFIG_FTDI_MODEM_CTRL));
      break;

    case CONFIG_FTDI_MODEM_CTRL:
      #ifdef LINE_CONTROL_ON_ENUM
        p_cdc->requested_line_state.all = LINE_CONTROL_ON_ENUM;
        TU_ASSERT_COMPLETE(ftdi_update_mctrl(p_cdc, ftdi_internal_control_complete, CONFIG_FTDI_COMPLETE));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_FTDI_COMPLETE:
      set_config_complete(idx, 0, true);
      break;

    default:
      set_config_complete(idx, 0, false);
      break;
  }
}

//------------- Helper -------------//

static bool ftdi_determine_type(cdch_interface_t * p_cdc)
{
  uint16_t const version = desc_dev->bcdDevice;
  uint8_t const itf_num = p_cdc->bInterfaceNumber;

  p_cdc->ftdi.chip_type = UNKNOWN;

  /* Assume Hi-Speed type */
  p_cdc->ftdi.channel = CHANNEL_A + itf_num;

  switch (version) {
    case 0x200:
      // FT232A not supported to keep it simple (no extra _read_latency_timer())
      // not testable
      // p_cdc->ftdi.chip_type = FT232A;
      // p_cdc->ftdi.baud_base = 48000000 / 2;
      // p_cdc->ftdi.channel = 0;
      // /*
      //  * FT232B devices have a bug where bcdDevice gets set to 0x200
      //  * when iSerialNumber is 0. Assume it is an FT232B in case the
      //  * latency timer is readable.
      //  */
      // if (desc->iSerialNumber == 0 &&
      //     _read_latency_timer(port) >= 0) {
      //   p_cdc->ftdi.chip_type = FT232B;
      // }
      break;
    case 0x400:
      p_cdc->ftdi.chip_type = FT232B;
      p_cdc->ftdi.channel = 0;
      break;
    case 0x500:
      p_cdc->ftdi.chip_type = FT2232C;
      break;
    case 0x600:
      p_cdc->ftdi.chip_type = FT232R;
      p_cdc->ftdi.channel = 0;
      break;
    case 0x700:
      p_cdc->ftdi.chip_type = FT2232H;
      break;
    case 0x800:
      p_cdc->ftdi.chip_type = FT4232H;
      break;
    case 0x900:
      p_cdc->ftdi.chip_type = FT232H;
      break;
    case 0x1000:
      p_cdc->ftdi.chip_type = FTX;
      break;
    case 0x2800:
      p_cdc->ftdi.chip_type = FT2233HP;
      break;
    case 0x2900:
      p_cdc->ftdi.chip_type = FT4233HP;
      break;
    case 0x3000:
      p_cdc->ftdi.chip_type = FT2232HP;
      break;
    case 0x3100:
      p_cdc->ftdi.chip_type = FT4232HP;
      break;
    case 0x3200:
      p_cdc->ftdi.chip_type = FT233HP;
      break;
    case 0x3300:
      p_cdc->ftdi.chip_type = FT232HP;
      break;
    case 0x3600:
      p_cdc->ftdi.chip_type = FT4232HA;
      break;
    default:
      if (version < 0x200) {
        p_cdc->ftdi.chip_type = SIO;
        p_cdc->ftdi.channel = 0;
      }
      break;
  }

  TU_LOG_P_CDC("%s detected (bcdDevice = 0x%04x)",
               ftdi_chip_name[p_cdc->ftdi.chip_type], desc_dev->bcdDevice);

  return (p_cdc->ftdi.chip_type != UNKNOWN);
}

// FT232A not supported
//static uint32_t ftdi_232am_baud_base_to_divisor(uint32_t baud, uint32_t base)
//{
//  uint32_t divisor;
//  /* divisor shifted 3 bits to the left */
//  uint32_t divisor3 = DIV_ROUND_CLOSEST(base, 2 * baud);
//  if ((divisor3 & 0x7) == 7)
//    divisor3++; /* round x.7/8 up to x+1 */
//  divisor = divisor3 >> 3;
//  divisor3 &= 0x7;
//  if (divisor3 == 1)
//    divisor |= 0xc000;  /* +0.125 */
//  else if (divisor3 >= 4)
//    divisor |= 0x4000;  /* +0.5 */
//  else if (divisor3 != 0)
//    divisor |= 0x8000;  /* +0.25 */
//  else if (divisor == 1)
//    divisor = 0;    /* special case for maximum baud rate */
//  return divisor;
//}

// FT232A not supported
//static inline uint32_t ftdi_232am_baud_to_divisor(uint32_t baud)
//{
//   return ftdi_232am_baud_base_to_divisor(baud, (uint32_t) 48000000);
//}

static uint32_t ftdi_232bm_baud_base_to_divisor(uint32_t baud, uint32_t base) {
  uint8_t divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
  uint32_t divisor;
  /* divisor shifted 3 bits to the left */
  uint32_t divisor3 = DIV_ROUND_CLOSEST(base, 2 * baud);
  divisor = divisor3 >> 3;
  divisor |= (uint32_t)divfrac[divisor3 & 0x7] << 14;
  /* Deal with special cases for highest baud rates. */
  if (divisor == 1)   /* 1.0 */
    divisor = 0;
  else if (divisor == 0x4001) /* 1.5 */
    divisor = 1;
  return divisor;
}

static inline uint32_t ftdi_232bm_baud_to_divisor(uint32_t baud)
{
   return ftdi_232bm_baud_base_to_divisor(baud, 48000000);
}

static uint32_t ftdi_2232h_baud_base_to_divisor(uint32_t baud, uint32_t base)
{
  static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
  uint32_t divisor;
  uint32_t divisor3;

  /* hi-speed baud rate is 10-bit sampling instead of 16-bit */
  divisor3 = DIV_ROUND_CLOSEST(8 * base, 10 * baud);

  divisor = divisor3 >> 3;
  divisor |= (uint32_t)divfrac[divisor3 & 0x7] << 14;
  /* Deal with special cases for highest baud rates. */
  if (divisor == 1)   /* 1.0 */
    divisor = 0;
  else if (divisor == 0x4001) /* 1.5 */
    divisor = 1;
  /*
   * Set this bit to turn off a divide by 2.5 on baud rate generator
   * This enables baud rates up to 12Mbaud but cannot reach below 1200
   * baud with this bit set
   */
  divisor |= 0x00020000;
  return divisor;
}

static inline uint32_t ftdi_2232h_baud_to_divisor(uint32_t baud)
{
   return ftdi_2232h_baud_base_to_divisor(baud, (uint32_t) 120000000);
}

static inline uint32_t ftdi_get_divisor(cdch_interface_t * p_cdc)
{
  uint32_t baud = p_cdc->requested_line_coding.bit_rate;
  uint32_t div_value = 0;
  TU_VERIFY(baud);

  switch (p_cdc->ftdi.chip_type) {
    case UNKNOWN:
      return 0;
    case SIO:
      switch (baud) {
        case 300: div_value = ftdi_sio_b300; break;
        case 600: div_value = ftdi_sio_b600; break;
        case 1200: div_value = ftdi_sio_b1200; break;
        case 2400: div_value = ftdi_sio_b2400; break;
        case 4800: div_value = ftdi_sio_b4800; break;
        case 9600: div_value = ftdi_sio_b9600; break;
        case 19200: div_value = ftdi_sio_b19200; break;
        case 38400: div_value = ftdi_sio_b38400; break;
        case 57600: div_value = ftdi_sio_b57600;  break;
        case 115200: div_value = ftdi_sio_b115200; break;
        default:
          // Baudrate not supported
          return 0;
          break;
      }
      break;
      // FT232A not supported
      // case FT232A:
      //   if (baud <= 3000000) {
      //     div_value = ftdi_232am_baud_to_divisor(baud);
      //   } else {
      //     // Baud rate too high!
      //     baud = 9600;
      //     div_value = ftdi_232am_baud_to_divisor(9600);
      //     div_okay = false;
      //   }
      //   break;
    case FT232B:
    case FT2232C:
    case FT232R:
    case FTX:
      TU_VERIFY(baud <= 3000000); // else Baud rate too high!
      div_value = ftdi_232bm_baud_to_divisor(baud);
      break;
    case FT232H:
    case FT2232H:
    case FT4232H:
    case FT4232HA:
    case FT232HP:
    case FT233HP:
    case FT2232HP:
    case FT2233HP:
    case FT4232HP:
    case FT4233HP:
    default:
      TU_VERIFY(baud <= 12000000); // else Baud rate too high!
      if (baud >= 1200) {
        div_value = ftdi_2232h_baud_to_divisor(baud);
      } else {
        div_value = ftdi_232bm_baud_to_divisor(baud);
      }
      break;
  }

  TU_LOG_P_CDC("Baudrate divisor = 0x%lu", div_value);

  return div_value;
}

static uint8_t ftdi_get_idx(tuh_xfer_t * xfer) {
  uint8_t const channel = (uint8_t) tu_le16toh(xfer->setup->wIndex); // channel index, or 0 for legacy types
  for (uint8_t i = 0; i < CFG_TUH_CDC; i++) {
    const cdch_interface_t * p_cdc = &cdch_data[i];
    if (p_cdc->daddr == xfer->daddr &&
        (!p_cdc->ftdi.channel ||           // 0 for legacy types (only interface 0)
        channel == p_cdc->ftdi.channel)) { // or  multi-channel types (interfaces 0..n)
      return i;
    }
  }

  return TUSB_INDEX_INVALID_8;
}

#endif

//--------------------------------------------------------------------+
// CP210x
//--------------------------------------------------------------------+
#if CFG_TUH_CDC_CP210X

//------------- Control Request -------------//

static bool cp210x_set_request(cdch_interface_t * p_cdc, uint8_t command, uint16_t value,
                               uint8_t * buffer, uint16_t length, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_VENDOR,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = command,
    .wValue   = tu_htole16(value),
    .wIndex   = tu_htole16((uint16_t) p_cdc->bInterfaceNumber),
    .wLength  = tu_htole16(length)
  };

  // use usbh enum buf since application variable does not live long enough
  uint8_t * enum_buf = NULL;

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

static inline bool cp210x_ifc_enable(cdch_interface_t * p_cdc, uint16_t enabled,
                                     tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return cp210x_set_request(p_cdc, CP210X_IFC_ENABLE, enabled, NULL, 0, complete_cb, user_data);
}

static bool cp210x_set_baudrate_request(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  // Not every baud rate is supported. See datasheets and AN205 "CP210x Baud Rate Support"
  uint32_t baud_le = tu_htole32(p_cdc->requested_line_coding.bit_rate);

  return cp210x_set_request(p_cdc, CP210X_SET_BAUDRATE, 0, (uint8_t *) &baud_le, 4, complete_cb, user_data);
}

static bool cp210x_set_line_ctl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(p_cdc->requested_line_coding.data_bits >= 5 && p_cdc->requested_line_coding.data_bits <= 8, 0);
  uint16_t lcr = (uint16_t) (
    (p_cdc->requested_line_coding.data_bits & 0xfUL) << 8 | // data bit quantity is stored in bits 8-11
    (p_cdc->requested_line_coding.parity    & 0xfUL) << 4 | // parity is stored in bits 4-7, same coding
    (p_cdc->requested_line_coding.stop_bits & 0xfUL));      // parity is stored in bits 0-3, same coding

  return cp210x_set_request(p_cdc, CP210X_SET_LINE_CTL, lcr, NULL, 0, complete_cb, user_data);
}

static inline bool cp210x_set_mhs(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  // CP210x has the same bit coding
  return cp210x_set_request(p_cdc, CP210X_SET_MHS,
                            (uint16_t) (CP210X_CONTROL_WRITE_DTR | CP210X_CONTROL_WRITE_RTS |
                                        p_cdc->requested_line_state.all),
                            NULL, 0, complete_cb, user_data);
}

//------------- Driver API -------------//

// internal control complete to update state such as line state, encoding
static void cp210x_internal_control_complete(tuh_xfer_t * xfer) {
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);
  bool const success = (xfer->result == XFER_RESULT_SUCCESS);
  TU_LOG_P_CDC_BOOL("control complete", success);

  if (success) {
    switch(xfer->setup->bRequest) {
      case CP210X_SET_MHS:
        p_cdc->line_state = p_cdc->requested_line_state;
        break;

      case CP210X_SET_LINE_CTL:
        p_cdc->line_coding.stop_bits = p_cdc->requested_line_coding.stop_bits;
        p_cdc->line_coding.parity    = p_cdc->requested_line_coding.parity;
        p_cdc->line_coding.data_bits = p_cdc->requested_line_coding.data_bits;
        break;

      case CP210X_SET_BAUDRATE:
        p_cdc->line_coding.bit_rate = p_cdc->requested_line_coding.bit_rate;
        break;

      default: break;
    }
  }

  xfer->complete_cb = p_cdc->user_control_cb;
  if (xfer->complete_cb) {
    xfer->complete_cb(xfer);
  }
}

static bool cp210x_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(cp210x_set_baudrate_request(p_cdc, complete_cb ? cp210x_internal_control_complete : NULL, user_data));

  return true;
}

static bool cp210x_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(cp210x_set_line_ctl(p_cdc, complete_cb ? cp210x_internal_control_complete : NULL, user_data));

  return true;
}

static void cp210x_set_line_coding_stage1_complete(tuh_xfer_t * xfer) {
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  set_line_coding_stage1_complete(xfer, itf_num,
                                  cp210x_set_line_ctl,               // control request function to set data format
                                  cp210x_internal_control_complete); // control complete function to be called after request
}

// 2 stages: set baudrate (stage1) + set data format (stage2)
static bool cp210x_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return set_line_coding_sequence(p_cdc,
                                  cp210x_set_baudrate_request,            // control request function to set baudrate
                                  cp210x_set_line_ctl,                    // control request function to set data format
                                  cp210x_set_line_coding_stage1_complete, // function to be called after stage 1 completed
                                  cp210x_internal_control_complete,       // control complete function to be called after request
                                  complete_cb, user_data);
}

static bool cp210x_set_modem_ctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(cp210x_set_mhs(p_cdc, complete_cb ? cp210x_internal_control_complete : NULL, user_data));

  return true;
}

//------------- Enumeration -------------//

enum {
  CONFIG_CP210X_IFC_ENABLE = 0,
  CONFIG_CP210X_SET_BAUDRATE_REQUEST,
  CONFIG_CP210X_SET_LINE_CTL,
  CONFIG_CP210X_SET_DTR_RTS,
  CONFIG_CP210X_COMPLETE
};

static bool cp210x_open(uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len) {
  // CP210x Interface includes 1 vendor interface + 2 bulk endpoints
  TU_VERIFY(itf_desc->bInterfaceSubClass == 0 && itf_desc->bInterfaceProtocol == 0 && itf_desc->bNumEndpoints == 2);
  TU_VERIFY(sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t) <= max_len);

  cdch_interface_t * p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY(p_cdc);

  p_cdc->serial_drid = SERIAL_DRIVER_CP210X;

  // endpoint pair
  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) tu_desc_next(itf_desc);

  // data endpoints expected to be in pairs
  return open_ep_stream_pair(p_cdc, desc_ep);
}

static void cp210x_process_config(tuh_xfer_t * xfer) {
  uintptr_t const state   = xfer->user_data;
  uint8_t const   itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t const   idx     = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t *p_cdc = get_itf(idx);
  TU_ASSERT_COMPLETE(p_cdc && xfer->result == XFER_RESULT_SUCCESS);

  switch (state) {
    case CONFIG_CP210X_IFC_ENABLE:
      p_cdc->user_control_cb = cp210x_process_config; // set once for whole process config
      TU_ASSERT_COMPLETE(cp210x_ifc_enable(p_cdc, CP210X_UART_ENABLE, cp210x_process_config,
                                           CONFIG_CP210X_SET_BAUDRATE_REQUEST));
      break;

    case CONFIG_CP210X_SET_BAUDRATE_REQUEST:
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
        p_cdc->requested_line_coding = (cdc_line_coding_t) CFG_TUH_CDC_LINE_CODING_ON_ENUM;
        TU_ASSERT_COMPLETE(cp210x_set_baudrate_request(p_cdc, cp210x_internal_control_complete,
                                                       CONFIG_CP210X_SET_LINE_CTL));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_CP210X_SET_LINE_CTL:
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
        TU_ASSERT_COMPLETE(cp210x_set_line_ctl(p_cdc, cp210x_internal_control_complete,
                                               CONFIG_CP210X_SET_DTR_RTS));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_CP210X_SET_DTR_RTS:
      #ifdef LINE_CONTROL_ON_ENUM
        p_cdc->requested_line_state.all = LINE_CONTROL_ON_ENUM;
        TU_ASSERT_COMPLETE(cp210x_set_mhs(p_cdc, cp210x_internal_control_complete,
                                          CONFIG_CP210X_COMPLETE));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_CP210X_COMPLETE:
      set_config_complete(idx, 0, true);
      break;

    default:
      set_config_complete(idx, 0, false);
      break;
  }
}

#endif

//--------------------------------------------------------------------+
// CH34x (CH340 & CH341)
//--------------------------------------------------------------------+

#if CFG_TUH_CDC_CH34X

static uint8_t ch34x_get_lcr(cdch_interface_t * p_cdc);
static uint16_t ch34x_get_divisor_prescaler(cdch_interface_t * p_cdc);

//------------- Control Request -------------//

static bool ch34x_set_request(cdch_interface_t * p_cdc, uint8_t direction, uint8_t request,
                              uint16_t value, uint16_t index, uint8_t * buffer, uint16_t length,
                              tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request_setup = {
      .bmRequestType_bit = {
          .recipient = TUSB_REQ_RCPT_DEVICE,
          .type      = TUSB_REQ_TYPE_VENDOR,
          .direction = direction & 0x01u
      },
      .bRequest = request,
      .wValue   = tu_htole16(value),
      .wIndex   = tu_htole16(index),
      .wLength  = tu_htole16(length)
  };

  // use usbh enum buf since application variable does not live long enough
  uint8_t * enum_buf = NULL;

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

static inline bool ch34x_control_out(cdch_interface_t * p_cdc, uint8_t request, uint16_t value, uint16_t index,
                                     tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return ch34x_set_request(p_cdc, TUSB_DIR_OUT, request, value, index, NULL, 0, complete_cb, user_data);
}

static inline bool ch34x_control_in(cdch_interface_t * p_cdc, uint8_t request, uint16_t value, uint16_t index,
                                    uint8_t * buffer, uint16_t buffersize, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return ch34x_set_request(p_cdc, TUSB_DIR_IN, request, value, index, buffer, buffersize,
                           complete_cb, user_data);
}

static inline bool ch34x_write_reg(cdch_interface_t * p_cdc, uint16_t reg, uint16_t reg_value,
                                   tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return ch34x_control_out(p_cdc, CH34X_REQ_WRITE_REG, reg, reg_value, complete_cb, user_data);
}

//static bool ch34x_read_reg_request ( cdch_interface_t * p_cdc, uint16_t reg,
//                                     uint8_t *buffer, uint16_t buffersize, tuh_xfer_cb_t complete_cb, uintptr_t user_data )
//{
//  return ch34x_control_in ( p_cdc, CH34X_REQ_READ_REG, reg, 0, buffer, buffersize, complete_cb, user_data );
//}

static bool ch34x_write_reg_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  uint8_t const lcr = ch34x_get_lcr(p_cdc);
  TU_VERIFY(lcr);

  return ch34x_write_reg(p_cdc, CH32X_REG16_LCR2_LCR, lcr, complete_cb, user_data);
}

static bool ch34x_write_reg_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  uint16_t const div_ps = ch34x_get_divisor_prescaler(p_cdc);
  TU_VERIFY(div_ps);

  return ch34x_write_reg(p_cdc, CH34X_REG16_DIVISOR_PRESCALER, div_ps, complete_cb, user_data);
}

static bool ch34x_modem_ctrl_request(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  uint8_t control = ~((p_cdc->requested_line_state.rts ? CH34X_BIT_RTS : 0) | // CH34x signals are inverted
                      (p_cdc->requested_line_state.dtr ? CH34X_BIT_DTR : 0));

  return ch34x_control_out(p_cdc, CH34X_REQ_MODEM_CTRL, control, 0, complete_cb, user_data);
}

//------------- Driver API -------------//

// internal control complete to update state such as line state, encoding
static void ch34x_internal_control_complete(tuh_xfer_t * xfer) {
  // CH34x has only interface 0, because wIndex is used as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  uint8_t idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);
  bool const success = (xfer->result == XFER_RESULT_SUCCESS);
  TU_LOG_P_CDC_BOOL("control complete", success);

  if (success) {
    switch (xfer->setup->bRequest) {
      case CH34X_REQ_WRITE_REG:
        // register write request
        switch (tu_le16toh(xfer->setup->wValue)) {
          case CH34X_REG16_DIVISOR_PRESCALER:
            // baudrate
            p_cdc->line_coding.bit_rate = p_cdc->requested_line_coding.bit_rate;
            break;

          case CH32X_REG16_LCR2_LCR:
            // data format
            p_cdc->line_coding.stop_bits = p_cdc->requested_line_coding.stop_bits;
            p_cdc->line_coding.parity    = p_cdc->requested_line_coding.parity;
            p_cdc->line_coding.data_bits = p_cdc->requested_line_coding.data_bits;
            break;

          default: break;
        }
        break;

      case CH34X_REQ_MODEM_CTRL:
        p_cdc->line_state = p_cdc->requested_line_state;
        break;

      default: break;
    }
  }

  xfer->complete_cb = p_cdc->user_control_cb;
  if (xfer->complete_cb) {
    xfer->complete_cb(xfer);
  }
}

static bool ch34x_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(ch34x_write_reg_data_format(p_cdc, complete_cb ? ch34x_internal_control_complete : NULL, user_data));

  return true;
}

static bool ch34x_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(ch34x_write_reg_baudrate(p_cdc, complete_cb ? ch34x_internal_control_complete : NULL, user_data));

  return true;
}

static void ch34x_set_line_coding_stage1_complete(tuh_xfer_t * xfer) {
  // CH34x has only interface 0, because wIndex is used as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  set_line_coding_stage1_complete(xfer, itf_num,
                                  ch34x_write_reg_data_format,      // control request function to set data format
                                  ch34x_internal_control_complete); // control complete function to be called after request
}

// 2 stages: set baudrate (stage1) + set data format (stage2)
static bool ch34x_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return set_line_coding_sequence(p_cdc,
                                  ch34x_write_reg_baudrate,              // control request function to set baudrate
                                  ch34x_write_reg_data_format,           // control request function to set data format
                                  ch34x_set_line_coding_stage1_complete, // function to be called after stage 1 completed
                                  ch34x_internal_control_complete,       // control complete function to be called after request
                                  complete_cb, user_data);
}

static bool ch34x_set_modem_ctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(ch34x_modem_ctrl_request(p_cdc, complete_cb ? ch34x_internal_control_complete : NULL, user_data));

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

static bool ch34x_open(uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len) {
  // CH34x Interface includes 1 vendor interface + 2 bulk + 1 interrupt endpoints
  TU_VERIFY(itf_desc->bNumEndpoints == 3);
  TU_VERIFY(sizeof(tusb_desc_interface_t) + 3 * sizeof(tusb_desc_endpoint_t) <= max_len);

  cdch_interface_t * p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY(p_cdc);

  p_cdc->serial_drid = SERIAL_DRIVER_CH34X;

  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) tu_desc_next(itf_desc);

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
  uintptr_t const state = xfer->user_data;
  // CH34x has only interface 0, because wIndex is used as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT_COMPLETE(p_cdc && xfer->result == XFER_RESULT_SUCCESS);
  uint8_t buffer[2]; // TODO remove

  switch (state) {
    case CONFIG_CH34X_READ_VERSION:
      p_cdc->user_control_cb = ch34x_process_config; // set once for whole process config
      TU_ASSERT_COMPLETE(ch34x_control_in(p_cdc, CH34X_REQ_READ_VERSION, 0, 0, buffer, 2,
                                          ch34x_process_config, CONFIG_CH34X_SERIAL_INIT));
      break;

    case CONFIG_CH34X_SERIAL_INIT: {
      // handle version read data, set CH34x line coding (incl. baudrate)
      uint8_t const version = xfer->buffer[0];
      TU_LOG_P_CDC("Chip Version = 0x%02x", version);
      // only versions >= 0x30 are tested, below 0x30 seems having other programming
      // see drivers from WCH vendor, Linux kernel and FreeBSD
      TU_ASSERT_COMPLETE(version >= 0x30);
      // init CH34x with line coding
      p_cdc->requested_line_coding = (cdc_line_coding_t) CFG_TUH_CDC_LINE_CODING_ON_ENUM_CH34X;
      uint16_t const div_ps = ch34x_get_divisor_prescaler(p_cdc);
      TU_ASSERT_COMPLETE(div_ps);
      uint8_t const lcr = ch34x_get_lcr(p_cdc);
      TU_ASSERT_COMPLETE(lcr);
      TU_ASSERT_COMPLETE(ch34x_control_out(p_cdc, CH34X_REQ_SERIAL_INIT, tu_u16(lcr, 0x9c), div_ps,
                                           ch34x_process_config, CONFIG_CH34X_SPECIAL_REG_WRITE));
      break;
    }

    case CONFIG_CH34X_SPECIAL_REG_WRITE:
      // overtake line coding and do special reg write, purpose unknown, overtaken from WCH driver
      p_cdc->line_coding = (cdc_line_coding_t) CFG_TUH_CDC_LINE_CODING_ON_ENUM_CH34X;
      TU_ASSERT_COMPLETE(ch34x_write_reg(p_cdc, TU_U16(CH341_REG_0x0F, CH341_REG_0x2C), 0x0007,
                                         ch34x_process_config, CONFIG_CH34X_FLOW_CONTROL));
      break;

    case CONFIG_CH34X_FLOW_CONTROL:
      // no hardware flow control
      TU_ASSERT_COMPLETE(ch34x_write_reg(p_cdc, TU_U16(CH341_REG_0x27, CH341_REG_0x27), 0x0000,
                                         ch34x_process_config, CONFIG_CH34X_MODEM_CONTROL));
      break;

    case CONFIG_CH34X_MODEM_CONTROL:
      #ifdef LINE_CONTROL_ON_ENUM
        p_cdc->requested_line_state.all = LINE_CONTROL_ON_ENUM;
        TU_ASSERT_COMPLETE(ch34x_modem_ctrl_request(p_cdc, ch34x_internal_control_complete,
                                                    CONFIG_CH34X_COMPLETE));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_CH34X_COMPLETE:
      set_config_complete(idx, 0, true);
      break;

    default:
      set_config_complete(idx, 0, false);
      break;
  }
}

//------------- Helper -------------//

// calculate divisor and prescaler for baudrate, return it as 16-bit combined value
static uint16_t ch34x_get_divisor_prescaler(cdch_interface_t * p_cdc) {
  uint32_t const baval = p_cdc->requested_line_coding.bit_rate;
  uint8_t a;
  uint8_t b;
  uint32_t c;

  TU_VERIFY(baval != 0 && baval <= 2000000, 0);
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
static uint8_t ch34x_get_lcr(cdch_interface_t * p_cdc) {
  uint8_t const stop_bits = p_cdc->requested_line_coding.stop_bits;
  uint8_t const parity    = p_cdc->requested_line_coding.parity;
  uint8_t const data_bits = p_cdc->requested_line_coding.data_bits;

  uint8_t lcr = CH34X_LCR_ENABLE_RX | CH34X_LCR_ENABLE_TX;
  TU_VERIFY(data_bits >= 5 && data_bits <= 8);
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
  TU_VERIFY(stop_bits != CDC_LINE_CODING_STOP_BITS_1_5);
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

static int8_t pl2303_detect_type(cdch_interface_t * p_cdc, uint8_t step,
                                 tuh_xfer_cb_t complete_cb, uintptr_t user_data);
static bool pl2303_encode_baud_rate(cdch_interface_t * p_cdc, uint8_t buf[PL2303_LINE_CODING_BAUDRATE_BUFSIZE]);

//------------- Control Request -------------//

static bool pl2303_set_request(cdch_interface_t * p_cdc, uint8_t request, uint8_t requesttype,
                               uint16_t value, uint16_t index, uint8_t * buffer, uint16_t length,
                               tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request_setup = {
    .bmRequestType = requesttype,
    .bRequest      = request,
    .wValue        = tu_htole16(value),
    .wIndex        = tu_htole16(index),
    .wLength       = tu_htole16(length)
  };

  // use usbh enum buf since application variable does not live long enough
  uint8_t * enum_buf = NULL;

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

static bool pl2303_vendor_read(cdch_interface_t * p_cdc, uint16_t value, uint8_t * buf,
                               tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  uint8_t request = p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN] ?
                    PL2303_VENDOR_READ_NREQUEST : PL2303_VENDOR_READ_REQUEST;

  return pl2303_set_request(p_cdc, request, PL2303_VENDOR_READ_REQUEST_TYPE, value, 0, buf, 1, complete_cb, user_data);
}

static bool pl2303_vendor_write(cdch_interface_t * p_cdc, uint16_t value, uint16_t index,
                                tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  uint8_t request = p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN] ?
                    PL2303_VENDOR_WRITE_NREQUEST : PL2303_VENDOR_WRITE_REQUEST;

  return pl2303_set_request(p_cdc, request, PL2303_VENDOR_WRITE_REQUEST_TYPE, value, index, NULL, 0, complete_cb, user_data);
}

static inline bool pl2303_supports_hx_status(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  uint8_t buf = 0;

  return pl2303_set_request(p_cdc, PL2303_VENDOR_READ_REQUEST, PL2303_VENDOR_READ_REQUEST_TYPE, PL2303_READ_TYPE_HX_STATUS, 0,
                            &buf, 1, complete_cb, user_data);
}

static inline bool pl2303_set_control_lines(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  // PL2303 has the same bit coding
  return pl2303_set_request(p_cdc, PL2303_SET_CONTROL_REQUEST, PL2303_SET_CONTROL_REQUEST_TYPE,
                            p_cdc->requested_line_state.all, 0, NULL, 0, complete_cb, user_data);
}

//static bool pl2303_get_line_request(cdch_interface_t * p_cdc, uint8_t buf[PL2303_LINE_CODING_BUFSIZE])
//{
//  return pl2303_set_request(p_cdc, PL2303_GET_LINE_REQUEST, PL2303_GET_LINE_REQUEST_TYPE, 0, 0, buf, PL2303_LINE_CODING_BUFSIZE);
//}

static bool pl2303_set_line_request(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  // the caller has to precheck, that the new line coding different than the current, else false returned
  uint8_t buf[PL2303_LINE_CODING_BUFSIZE];
  /*
   * Some PL2303 are known to lose bytes if you change serial settings
   * even to the same values as before. Thus we actually need to filter
   * in this specific case.
   */
  TU_VERIFY(p_cdc->requested_line_coding.data_bits != p_cdc->line_coding.data_bits ||
            p_cdc->requested_line_coding.stop_bits != p_cdc->line_coding.stop_bits ||
            p_cdc->requested_line_coding.parity    != p_cdc->line_coding.parity    ||
            p_cdc->requested_line_coding.bit_rate  != p_cdc->line_coding.bit_rate );

  /* For reference buf[6] data bits value */
  TU_VERIFY(p_cdc->requested_line_coding.data_bits >= 5 && p_cdc->requested_line_coding.data_bits <= 8, 0);
  buf[6] = p_cdc->requested_line_coding.data_bits;

  /* For reference buf[0]:buf[3] baud rate value */
  TU_VERIFY(pl2303_encode_baud_rate(p_cdc, &buf[0]));

  /* For reference buf[4]=0 is 1 stop bits */
  /* For reference buf[4]=1 is 1.5 stop bits */
  /* For reference buf[4]=2 is 2 stop bits */
  buf[4] = p_cdc->requested_line_coding.stop_bits; // PL2303 has the same coding

  /* For reference buf[5]=0 is none parity */
  /* For reference buf[5]=1 is odd parity */
  /* For reference buf[5]=2 is even parity */
  /* For reference buf[5]=3 is mark parity */
  /* For reference buf[5]=4 is space parity */
  buf[5] = p_cdc->requested_line_coding.parity; // PL2303 has the same coding

  return pl2303_set_request(p_cdc, PL2303_SET_LINE_REQUEST, PL2303_SET_LINE_REQUEST_TYPE, 0, 0,
                            buf, PL2303_LINE_CODING_BUFSIZE, complete_cb, user_data);
}

//static bool pl2303_set_break(cdch_interface_t * p_cdc, bool enable)
//{
//  uint16_t state = enable ? PL2303_BREAK_ON : PL2303_BREAK_OFF;
//  return pl2303_set_request(p_cdc, PL2303_BREAK_REQUEST, PL2303_BREAK_REQUEST_TYPE, state, 0, NULL, 0);
//}

static inline int pl2303_clear_halt(cdch_interface_t * p_cdc, uint8_t endp, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  /* we don't care if it wasn't halted first. in fact some devices
   * (like some ibmcam model 1 units) seem to expect hosts to make
   * this request for iso endpoints, which can't halt!
   */
  return pl2303_set_request(p_cdc, TUSB_REQ_CLEAR_FEATURE, PL2303_CLEAR_HALT_REQUEST_TYPE, TUSB_REQ_FEATURE_EDPT_HALT, endp,
                            NULL, 0, complete_cb, user_data);
}

//------------- Driver API -------------//

// internal control complete to update state such as line state, encoding
static void pl2303_internal_control_complete(tuh_xfer_t * xfer) {
  // PL2303 has only interface 0, because wIndex is used as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  uint8_t idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc,);
  bool const success = (xfer->result == XFER_RESULT_SUCCESS);
  TU_LOG_P_CDC_BOOL("control complete", success);

  if (success) {
    if (xfer->setup->bRequest == PL2303_SET_LINE_REQUEST &&
         xfer->setup->bmRequestType == PL2303_SET_LINE_REQUEST_TYPE) {
      p_cdc->line_coding = p_cdc->requested_line_coding;
    }
    if (xfer->setup->bRequest == PL2303_SET_CONTROL_REQUEST &&
         xfer->setup->bmRequestType == PL2303_SET_CONTROL_REQUEST_TYPE) {
      p_cdc->line_state = p_cdc->requested_line_state;
    }
  }

  xfer->complete_cb = p_cdc->user_control_cb;
  if (xfer->complete_cb) {
    xfer->complete_cb(xfer);
  }
}

static bool pl2303_set_line_coding(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(pl2303_set_line_request(p_cdc, complete_cb ? pl2303_internal_control_complete : NULL, user_data));

  return true;
}

static bool pl2303_set_data_format(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->requested_line_coding.bit_rate = p_cdc->line_coding.bit_rate;
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(pl2303_set_line_request(p_cdc, complete_cb ? pl2303_internal_control_complete : NULL, user_data));

  return true;
}

static bool pl2303_set_baudrate(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  p_cdc->requested_line_coding.stop_bits = p_cdc->line_coding.stop_bits;
  p_cdc->requested_line_coding.parity    = p_cdc->line_coding.parity;
  p_cdc->requested_line_coding.data_bits = p_cdc->line_coding.data_bits;
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(pl2303_set_line_request(p_cdc, complete_cb ? pl2303_internal_control_complete : NULL, user_data));

  return true;
}

static bool pl2303_set_modem_ctrl(cdch_interface_t * p_cdc, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  // PL2303 has the same bit coding
  p_cdc->user_control_cb = complete_cb;
  TU_ASSERT(pl2303_set_control_lines(p_cdc, complete_cb ? pl2303_internal_control_complete : NULL, user_data));

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

static bool pl2303_open(uint8_t daddr, tusb_desc_interface_t const * itf_desc, uint16_t max_len) {
  // PL2303 Interface includes 1 vendor interface + 1 interrupt endpoints + 2 bulk
  TU_VERIFY(itf_desc->bNumEndpoints == 3);
  TU_VERIFY(sizeof(tusb_desc_interface_t) + 3 * sizeof(tusb_desc_endpoint_t) <= max_len);

  cdch_interface_t * p_cdc = make_new_itf(daddr, itf_desc);
  TU_VERIFY(p_cdc);

  p_cdc->serial_drid = SERIAL_DRIVER_PL2303;
  p_cdc->pl2303.serial_private.quirks = 0;
  p_cdc->pl2303.supports_hx_status = false;

  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const * ) tu_desc_next(itf_desc);

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

static void pl2303_process_config(tuh_xfer_t * xfer) {
  uintptr_t const state = xfer->user_data;
  // PL2303 has only interface 0, because wIndex is used as payload and not for bInterfaceNumber
  uint8_t const itf_num = 0;
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t * p_cdc = get_itf(idx);
  // state CONFIG_PL2303_READ1 may have no success due to expected stall by pl2303_supports_hx_status()
  TU_ASSERT_COMPLETE(p_cdc && (xfer->result == XFER_RESULT_SUCCESS || xfer->user_data == CONFIG_PL2303_READ1));
  uint8_t buf = 0;
  int8_t type;

  switch (state) {

    // from here sequence overtaken from Linux Kernel function pl2303_startup()
    case CONFIG_PL2303_GET_DESC:
      p_cdc->user_control_cb = pl2303_process_config; // set once for whole process config
      // get device descriptor
      TU_ASSERT_COMPLETE(tuh_descriptor_get_device(xfer->daddr, &desc_dev, sizeof(tusb_desc_device_t),
                                                   pl2303_process_config, CONFIG_PL2303_DETECT_TYPE));
      break;

    case CONFIG_PL2303_DETECT_TYPE:
      // get type and quirks (step 1)
      type = pl2303_detect_type (p_cdc, 1, pl2303_process_config, CONFIG_PL2303_READ1); // step 1
      TU_ASSERT_COMPLETE(type!=PL2303_DETECT_TYPE_FAILED);
      if (type == PL2303_SUPPORTS_HX_STATUS_TRIGGERED) {
        break;
      } // else: no transfer triggered and continue with CONFIG_PL2303_READ1
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_READ1:
      // get supports_hx_status, type and quirks (step 2), do special read
      p_cdc->pl2303.supports_hx_status = ( // will not be true, if coming directly from previous case
                                           xfer->user_data == CONFIG_PL2303_READ1 && xfer->result == XFER_RESULT_SUCCESS );
      type = pl2303_detect_type (p_cdc, 2, NULL, 0); // step 2 now with supports_hx_status
      TU_ASSERT_COMPLETE(type!=PL2303_DETECT_TYPE_FAILED);
      p_cdc->pl2303.serial_private.type = &pl2303_type_data[type];
      p_cdc->pl2303.serial_private.quirks |= p_cdc->pl2303.serial_private.type->quirks;
      #if CFG_TUSB_DEBUG >= CFG_TUH_CDC_LOG_LEVEL && 0 // can be activated if necessary
        TU_LOG_P_CDC("bDeviceClass = 0x%02x bMaxPacketSize0 = %u bcdUSB = 0x%04x bcdDevice = 0x%04x",
                     desc_dev->bDeviceClass, desc_dev->bMaxPacketSize0,
                     desc_dev->bcdUSB, desc_dev->bcdDevice );
        uint16_t vid, pid;
        TU_ASSERT_COMPLETE(tuh_vid_pid_get(p_cdc->daddr, &vid, &pid));
        TU_LOG_P_CDC("vid = 0x%04x pid = 0x%04x supports_hx_status = %u type = %s quirks = %u",
                     vid, pid, p_cdc->pl2303.supports_hx_status,
                     p_cdc->pl2303.serial_private.type->name, p_cdc->pl2303.serial_private.quirks);
      #endif
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_read(p_cdc, 0x8484, &buf, pl2303_process_config, CONFIG_PL2303_WRITE1));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_WRITE1:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, 0x0404, 0, pl2303_process_config, CONFIG_PL2303_READ2));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_READ2:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_read(p_cdc, 0x8484, &buf, pl2303_process_config, CONFIG_PL2303_READ3));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_READ3:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_read(p_cdc, 0x8383, &buf, pl2303_process_config, CONFIG_PL2303_READ4));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_READ4:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_read(p_cdc, 0x8484, &buf, pl2303_process_config, CONFIG_PL2303_WRITE2));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_WRITE2:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, 0x0404, 1, pl2303_process_config, CONFIG_PL2303_READ5));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_READ5:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_read(p_cdc, 0x8484, &buf, pl2303_process_config, CONFIG_PL2303_READ6));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_READ6:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_read(p_cdc, 0x8383, &buf, pl2303_process_config, CONFIG_PL2303_WRITE3));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_WRITE3:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, 0, 1, pl2303_process_config, CONFIG_PL2303_WRITE4));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_WRITE4:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, 1, 0, pl2303_process_config, CONFIG_PL2303_WRITE5));
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    case CONFIG_PL2303_WRITE5:
      // purpose unknown, overtaken from Linux Kernel driver
      if (p_cdc->pl2303.serial_private.type != &pl2303_type_data[TYPE_HXN]) {
        if (p_cdc->pl2303.serial_private.quirks & PL2303_QUIRK_LEGACY) {
          TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, 2, 0x24, pl2303_process_config, CONFIG_PL2303_RESET_ENDP1));
        } else {
          TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, 2, 0x44, pl2303_process_config, CONFIG_PL2303_RESET_ENDP1));
        }
        break;
      } // else: continue with next step
      TU_ATTR_FALLTHROUGH;

    // from here sequence overtaken from Linux Kernel function pl2303_open()
    case CONFIG_PL2303_RESET_ENDP1:
      // step 1
      if (p_cdc->pl2303.serial_private.quirks & PL2303_QUIRK_LEGACY) {
        TU_ASSERT_COMPLETE(pl2303_clear_halt(p_cdc, PL2303_OUT_EP, pl2303_process_config, CONFIG_PL2303_RESET_ENDP2));
      } else {
        /* reset upstream data pipes */
        if (p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN]) {
          TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, PL2303_HXN_RESET_REG, // skip CONFIG_PL2303_RESET_ENDP2, no 2nd step
                                        PL2303_HXN_RESET_UPSTREAM_PIPE | PL2303_HXN_RESET_DOWNSTREAM_PIPE,
                                        pl2303_process_config, CONFIG_PL2303_LINE_CODING));
        } else {
          pl2303_vendor_write(p_cdc, 8, 0, pl2303_process_config, CONFIG_PL2303_RESET_ENDP2);
        }
      }
      break;

    case CONFIG_PL2303_RESET_ENDP2:
      // step 2
      if (p_cdc->pl2303.serial_private.quirks & PL2303_QUIRK_LEGACY) {
        TU_ASSERT_COMPLETE(pl2303_clear_halt(p_cdc, PL2303_IN_EP, pl2303_process_config, CONFIG_PL2303_LINE_CODING));
      } else {
        /* reset upstream data pipes */
        if (p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN]) {
          // here nothing to do, only structure of previous step overtaken for better reading and comparison
        } else {
          TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, 9, 0, pl2303_process_config, CONFIG_PL2303_LINE_CODING));
        }
      }
      break;

    // from here sequence overtaken from Linux Kernel function pl2303_set_termios()
    // unnecessary pl2303_get_line_request() is skipped due to a stall
    case CONFIG_PL2303_LINE_CODING:
      #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
        p_cdc->requested_line_coding = (cdc_line_coding_t) CFG_TUH_CDC_LINE_CODING_ON_ENUM;
        TU_ASSERT_COMPLETE( pl2303_set_line_request(p_cdc, pl2303_internal_control_complete, CONFIG_PL2303_MODEM_CONTROL));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_PL2303_MODEM_CONTROL:
      #ifdef LINE_CONTROL_ON_ENUM
        p_cdc->requested_line_state.all = LINE_CONTROL_ON_ENUM;
        TU_ASSERT_COMPLETE(pl2303_set_control_lines(p_cdc, pl2303_internal_control_complete, CONFIG_PL2303_FLOW_CTRL_READ));
        break;
      #else
        TU_ATTR_FALLTHROUGH;
      #endif

    case CONFIG_PL2303_FLOW_CTRL_READ:
      // read flow control register for modify & write back in next step
      if (p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN]) {
        TU_ASSERT_COMPLETE(pl2303_vendor_read(p_cdc, PL2303_HXN_FLOWCTRL_REG, &buf, pl2303_process_config,
                                              CONFIG_PL2303_FLOW_CTRL_WRITE));
      } else {
        TU_ASSERT_COMPLETE(pl2303_vendor_read(p_cdc, 0, &buf, pl2303_process_config, CONFIG_PL2303_FLOW_CTRL_WRITE));
      }
      break;

    case CONFIG_PL2303_FLOW_CTRL_WRITE:
      // no flow control
      buf = xfer->buffer[0];
      if (p_cdc->pl2303.serial_private.type == &pl2303_type_data[TYPE_HXN]) {
        buf &= (uint8_t) ~PL2303_HXN_FLOWCTRL_MASK;
        buf |= PL2303_HXN_FLOWCTRL_NONE;
        TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, PL2303_HXN_FLOWCTRL_REG, buf, pl2303_process_config,
                                               CONFIG_PL2303_COMPLETE));
      } else {
        buf &= (uint8_t) ~PL2303_FLOWCTRL_MASK;
        TU_ASSERT_COMPLETE(pl2303_vendor_write(p_cdc, 0, buf, pl2303_process_config, CONFIG_PL2303_COMPLETE));
      }
      break;

    case CONFIG_PL2303_COMPLETE:
      set_config_complete(idx, 0, true);
      break;

    default:
      set_config_complete(idx, 0, false);
      break;
  }
}

//------------- Helper -------------//

static int8_t pl2303_detect_type(cdch_interface_t * p_cdc, uint8_t step,
                                 tuh_xfer_cb_t complete_cb, uintptr_t user_data )
{
  /*
   * Legacy PL2303H, variants 0 and 1 (difference unknown).
   */
  if (desc_dev->bDeviceClass == 0x02) {
    return TYPE_H;    /* variant 0 */
  }

  if (desc_dev->bMaxPacketSize0 != 0x40) {
    if (desc_dev->bDeviceClass == 0x00 || desc_dev->bDeviceClass == 0xff) {
      return TYPE_H;  /* variant 1 */
    }
    return TYPE_H;    /* variant 0 */
  }

  switch (desc_dev->bcdUSB) {
    case 0x101:
      /* USB 1.0.1? Let's assume they meant 1.1... */
      TU_ATTR_FALLTHROUGH;
    case 0x110:
      switch (desc_dev->bcdDevice) {
        case 0x300:
          return TYPE_HX;
        case 0x400:
          return TYPE_HXD;
        default:
          return TYPE_HX;
      }
      break;
    case 0x200:
      switch (desc_dev->bcdDevice) {
        case 0x100: /* GC */
        case 0x105:
          return TYPE_HXN;
        case 0x300: /* GT / TA */
          if (step == 1) {
            // step 1 trigger pl2303_supports_hx_status() request
            TU_ASSERT(pl2303_supports_hx_status (p_cdc, complete_cb, user_data), PL2303_DETECT_TYPE_FAILED);
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
          if (step == 1) {
            // step 1 trigger pl2303_supports_hx_status() request
            TU_ASSERT(pl2303_supports_hx_status (p_cdc, complete_cb, user_data), PL2303_DETECT_TYPE_FAILED);
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
        default:
          break;
      }
      break;
    default: break;
  }

  TU_LOG_P_CDC("unknown device type bcdUSB = 0x%04x", desc_dev->bcdUSB);

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
  uint32_t baud_le = tu_htole32(baud);
  buf[0] = (uint8_t) ( baud_le        & 0xff);
  buf[1] = (uint8_t) ((baud_le >>  8) & 0xff);
  buf[2] = (uint8_t) ((baud_le >> 16) & 0xff);
  buf[3] = (uint8_t) ((baud_le >> 24) & 0xff);

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
    mantissa = 1; /* Avoid dividing by zero if baud > 32 * 12M. */
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
  buf[1] = (uint8_t) ((exponent << 1 |  mantissa >> 8) & 0xff);
  buf[0] = (uint8_t) (mantissa & 0xff);

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
    mantissa = 1;   /* Avoid dividing by zero if baud > 32 * 12M. */
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
  buf[2] = (uint8_t) (exponent & 0x01);
  buf[1] = (uint8_t) (((exponent &  (uint32_t) ~0x01) << 4 | mantissa >> 8 ) & 0xff);
  buf[0] = (uint8_t) (mantissa & 0xff);

  /* Calculate and return the exact baud rate. */
  baud = (baseline / mantissa) >> exponent;

  return baud;
}

static bool pl2303_encode_baud_rate(cdch_interface_t * p_cdc, uint8_t buf[PL2303_LINE_CODING_BAUDRATE_BUFSIZE])
{
  uint32_t baud = p_cdc->requested_line_coding.bit_rate;
  uint32_t baud_sup;

  TU_VERIFY(baud && baud <= p_cdc->pl2303.serial_private.type->max_baud_rate);
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
  TU_LOG_P_CDC("real baudrate %lu", baud);

  return true;
}

#endif // CFG_TUH_CDC_PL2303

#endif
