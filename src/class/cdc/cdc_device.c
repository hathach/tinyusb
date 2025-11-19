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
 */

#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_CDC)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "cdc_device.h"

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUD_CDC_LOG_LEVEL
  #define CFG_TUD_CDC_LOG_LEVEL   CFG_TUD_LOG_LEVEL
#endif

#define TU_LOG_DRV(...)   TU_LOG(CFG_TUD_CDC_LOG_LEVEL, __VA_ARGS__)

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define BULK_PACKET_SIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)

typedef struct {
  uint8_t rhport;
  uint8_t itf_num;
  uint8_t ep_notify;
  uint8_t line_state; // Bit 0: DTR, Bit 1: RTS

  /*------------- From this point, data is not cleared by bus reset -------------*/
  TU_ATTR_ALIGNED(4) cdc_line_coding_t line_coding;
  char wanted_char;

  struct {
    tu_edpt_stream_t tx;
    tu_edpt_stream_t rx;

    uint8_t tx_ff_buf[CFG_TUD_CDC_TX_BUFSIZE];
    uint8_t rx_ff_buf[CFG_TUD_CDC_RX_BUFSIZE];
  } stream;
} cdcd_interface_t;

#define ITF_MEM_RESET_SIZE offsetof(cdcd_interface_t, line_coding)

typedef struct {
  TUD_EPBUF_DEF(epout, CFG_TUD_CDC_EP_BUFSIZE);
  TUD_EPBUF_DEF(epin, CFG_TUD_CDC_EP_BUFSIZE);

  #if CFG_TUD_CDC_NOTIFY
  TUD_EPBUF_TYPE_DEF(cdc_notify_msg_t, epnotify);
  #endif
} cdcd_epbuf_t;

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tud_cdc_rx_cb(uint8_t itf) {
  (void)itf;
}

TU_ATTR_WEAK void tud_cdc_rx_wanted_cb(uint8_t itf, char wanted_char) {
  (void)itf;
  (void)wanted_char;
}

TU_ATTR_WEAK void tud_cdc_tx_complete_cb(uint8_t itf) {
  (void)itf;
}

TU_ATTR_WEAK void tud_cdc_notify_complete_cb(uint8_t itf) {
  (void)itf;
}

TU_ATTR_WEAK void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf;
  (void)dtr;
  (void)rts;
}

TU_ATTR_WEAK void tud_cdc_line_coding_cb(uint8_t itf, const cdc_line_coding_t *p_line_coding) {
  (void)itf;
  (void)p_line_coding;
}

TU_ATTR_WEAK void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms) {
  (void)itf;
  (void)duration_ms;
}

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static cdcd_interface_t _cdcd_itf[CFG_TUD_CDC];
CFG_TUD_MEM_SECTION static cdcd_epbuf_t _cdcd_epbuf[CFG_TUD_CDC];
static tud_cdc_configure_t _cdcd_cfg = TUD_CDC_CONFIGURE_DEFAULT();

TU_ATTR_ALWAYS_INLINE static inline uint8_t find_cdc_itf(uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUD_CDC; idx++) {
    const cdcd_interface_t *p_cdc = &_cdcd_itf[idx];
    if (ep_addr == p_cdc->stream.rx.ep_addr || ep_addr == p_cdc->stream.tx.ep_addr ||
        (ep_addr == p_cdc->ep_notify && ep_addr != 0)) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_cdc_configure(const tud_cdc_configure_t* driver_cfg) {
  TU_VERIFY(driver_cfg != NULL);
  _cdcd_cfg = *driver_cfg;
  return true;
}

bool tud_cdc_n_ready(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_CDC);
  TU_VERIFY(tud_ready());
  const cdcd_interface_t *p_cdc = &_cdcd_itf[itf];

  const bool in_opened  = tu_edpt_stream_is_opened(&p_cdc->stream.tx);
  const bool out_opened = tu_edpt_stream_is_opened(&p_cdc->stream.rx);
  return in_opened && out_opened;
}

bool tud_cdc_n_connected(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_CDC);
  TU_VERIFY(tud_ready());
  // DTR (bit 0) active  is considered as connected
  return tu_bit_test(_cdcd_itf[itf].line_state, 0);
}

uint8_t tud_cdc_n_get_line_state(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_CDC, 0);
  return _cdcd_itf[itf].line_state;
}

void tud_cdc_n_get_line_coding(uint8_t itf, cdc_line_coding_t *coding) {
  TU_VERIFY(itf < CFG_TUD_CDC, );
  (*coding) = _cdcd_itf[itf].line_coding;
}

#if CFG_TUD_CDC_NOTIFY
bool tud_cdc_n_notify_uart_state (uint8_t itf, const cdc_notify_uart_state_t *state) {
  TU_VERIFY(itf < CFG_TUD_CDC);
  cdcd_interface_t *p_cdc   = &_cdcd_itf[itf];
  cdcd_epbuf_t     *p_epbuf = &_cdcd_epbuf[itf];
  TU_VERIFY(tud_ready() && p_cdc->ep_notify != 0);
  TU_VERIFY(usbd_edpt_claim(p_cdc->rhport, p_cdc->ep_notify));

  cdc_notify_msg_t* notify_msg = &p_epbuf->epnotify;
  notify_msg->request.bmRequestType = CDC_REQ_TYPE_NOTIF;
  notify_msg->request.bRequest = CDC_NOTIF_SERIAL_STATE;
  notify_msg->request.wValue = 0;
  notify_msg->request.wIndex = p_cdc->itf_num;
  notify_msg->request.wLength = sizeof(cdc_notify_uart_state_t);
  notify_msg->serial_state = *state;

  return usbd_edpt_xfer(p_cdc->rhport, p_cdc->ep_notify, (uint8_t *)notify_msg, 8 + sizeof(cdc_notify_uart_state_t), false);
}

bool tud_cdc_n_notify_conn_speed_change(uint8_t itf, const cdc_notify_conn_speed_change_t* conn_speed_change) {
  TU_VERIFY(itf < CFG_TUD_CDC);
  cdcd_interface_t *p_cdc   = &_cdcd_itf[itf];
  cdcd_epbuf_t     *p_epbuf = &_cdcd_epbuf[itf];
  TU_VERIFY(tud_ready() && p_cdc->ep_notify != 0);
  TU_VERIFY(usbd_edpt_claim(p_cdc->rhport, p_cdc->ep_notify));

  cdc_notify_msg_t* notify_msg = &p_epbuf->epnotify;
  notify_msg->request.bmRequestType = CDC_REQ_TYPE_NOTIF;
  notify_msg->request.bRequest = CDC_NOTIF_CONNECTION_SPEED_CHANGE;
  notify_msg->request.wValue = 0;
  notify_msg->request.wIndex = p_cdc->itf_num;
  notify_msg->request.wLength = sizeof(cdc_notify_conn_speed_change_t);
  notify_msg->conn_speed_change = *conn_speed_change;

  return usbd_edpt_xfer(p_cdc->rhport, p_cdc->ep_notify, (uint8_t *)notify_msg, 8 + sizeof(cdc_notify_conn_speed_change_t), false);
}
#endif

void tud_cdc_n_set_wanted_char(uint8_t itf, char wanted) {
  TU_VERIFY(itf < CFG_TUD_CDC, );
  _cdcd_itf[itf].wanted_char = wanted;
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
uint32_t tud_cdc_n_available(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_CDC, 0);
  return tu_edpt_stream_read_available(&_cdcd_itf[itf].stream.rx);
}

uint32_t tud_cdc_n_read(uint8_t itf, void* buffer, uint32_t bufsize) {
  TU_VERIFY(itf < CFG_TUD_CDC, 0);
  cdcd_interface_t *p_cdc = &_cdcd_itf[itf];
  return tu_edpt_stream_read(p_cdc->rhport, &p_cdc->stream.rx, buffer, bufsize);
}

bool tud_cdc_n_peek(uint8_t itf, uint8_t *chr) {
  TU_VERIFY(itf < CFG_TUD_CDC);
  return tu_edpt_stream_peek(&_cdcd_itf[itf].stream.rx, chr);
}

void tud_cdc_n_read_flush(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_CDC, );
  cdcd_interface_t *p_cdc = &_cdcd_itf[itf];
  tu_edpt_stream_clear(&p_cdc->stream.rx);
  tu_edpt_stream_read_xfer(p_cdc->rhport, &p_cdc->stream.rx);
}

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+
uint32_t tud_cdc_n_write(uint8_t itf, const void* buffer, uint32_t bufsize) {
  TU_VERIFY(itf < CFG_TUD_CDC, 0);
  cdcd_interface_t *p_cdc = &_cdcd_itf[itf];
  return tu_edpt_stream_write(p_cdc->rhport, &p_cdc->stream.tx, buffer, bufsize);
}

uint32_t tud_cdc_n_write_flush(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_CDC, 0);
  cdcd_interface_t *p_cdc = &_cdcd_itf[itf];
  return tu_edpt_stream_write_xfer(p_cdc->rhport, &p_cdc->stream.tx);
}

uint32_t tud_cdc_n_write_available(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_CDC, 0);
  cdcd_interface_t *p_cdc = &_cdcd_itf[itf];
  return tu_edpt_stream_write_available(p_cdc->rhport, &p_cdc->stream.tx);
}

bool tud_cdc_n_write_clear(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_CDC);
  cdcd_interface_t *p_cdc = &_cdcd_itf[itf];
  return tu_edpt_stream_clear(&p_cdc->stream.tx);
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void cdcd_init(void) {
  tu_memclr(_cdcd_itf, sizeof(_cdcd_itf));
  for (uint8_t i = 0; i < CFG_TUD_CDC; i++) {
    cdcd_interface_t *p_cdc   = &_cdcd_itf[i];
    cdcd_epbuf_t     *p_epbuf = &_cdcd_epbuf[i];

    p_cdc->wanted_char = (char) -1;

    // default line coding is : stop bit = 1, parity = none, data bits = 8
    p_cdc->line_coding.bit_rate = 115200;
    p_cdc->line_coding.stop_bits = 0;
    p_cdc->line_coding.parity = 0;
    p_cdc->line_coding.data_bits = 8;

    tu_edpt_stream_init(&p_cdc->stream.rx, false, false, false, p_cdc->stream.rx_ff_buf, CFG_TUD_CDC_RX_BUFSIZE,
                        p_epbuf->epout, CFG_TUD_CDC_EP_BUFSIZE);

    // TX fifo can be configured to change to overwritable if not connected (DTR bit not set). Without DTR we do not
    // know if data is actually polled by terminal. This way the most current data is prioritized.
    // Default: is overwritable
    tu_edpt_stream_init(&p_cdc->stream.tx, false, true, _cdcd_cfg.tx_overwritabe_if_not_connected,
                        p_cdc->stream.tx_ff_buf, CFG_TUD_CDC_TX_BUFSIZE, p_epbuf->epin, CFG_TUD_CDC_EP_BUFSIZE);
  }
}

bool cdcd_deinit(void) {
  for (uint8_t i = 0; i < CFG_TUD_CDC; i++) {
    cdcd_interface_t* p_cdc = &_cdcd_itf[i];
    tu_edpt_stream_deinit(&p_cdc->stream.rx);
    tu_edpt_stream_deinit(&p_cdc->stream.tx);
  }
  return true;
}

void cdcd_reset(uint8_t rhport) {
  (void) rhport;

  for (uint8_t i = 0; i < CFG_TUD_CDC; i++) {
    cdcd_interface_t* p_cdc = &_cdcd_itf[i];
    tu_memclr(p_cdc, ITF_MEM_RESET_SIZE);

    tu_fifo_set_overwritable(&p_cdc->stream.tx.ff, _cdcd_cfg.tx_overwritabe_if_not_connected); // back to default
    tu_edpt_stream_close(&p_cdc->stream.rx);
    tu_edpt_stream_close(&p_cdc->stream.tx);
  }
}

uint16_t cdcd_open(uint8_t rhport, const tusb_desc_interface_t* itf_desc, uint16_t max_len) {
  // Only support ACM subclass
  TU_VERIFY(TUSB_CLASS_CDC == itf_desc->bInterfaceClass &&
              CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == itf_desc->bInterfaceSubClass,
            0);

  const uint8_t cdc_id = find_cdc_itf(0); // Find available interface
  TU_ASSERT(cdc_id < CFG_TUD_CDC, 0);
  cdcd_interface_t *p_cdc = &_cdcd_itf[cdc_id];

  //------------- Control Interface -------------//
  p_cdc->rhport = rhport;
  p_cdc->itf_num = itf_desc->bInterfaceNumber;

  const uint8_t *p_desc   = (const uint8_t *)itf_desc;
  const uint8_t *desc_end = p_desc + max_len;

  // Skip all class-specific descriptor
  p_desc = tu_desc_next(itf_desc);
  while (tu_desc_in_bounds(p_desc, desc_end) && TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc)) {
    p_desc = tu_desc_next(p_desc);
  }

  // notification endpoint (optional)
  if (TUSB_DESC_ENDPOINT == tu_desc_type(p_desc)) {
    const tusb_desc_endpoint_t* desc_ep = (const tusb_desc_endpoint_t*) p_desc;
    TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
    p_cdc->ep_notify = desc_ep->bEndpointAddress;

    p_desc = tu_desc_next(p_desc);
  }

  //------------- Data Interface (optional) -------------//
  if (TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) {
    const tusb_desc_interface_t *data_itf_desc = (const tusb_desc_interface_t *)p_desc;
    if (TUSB_CLASS_CDC_DATA == data_itf_desc->bInterfaceClass) {
      for (uint8_t e = 0; e < data_itf_desc->bNumEndpoints; e++) {
        if (!tu_desc_in_bounds(p_desc, desc_end)) {
          break;
        }
        p_desc = tu_desc_next(p_desc);

        const tusb_desc_endpoint_t *desc_ep = (const tusb_desc_endpoint_t *)p_desc;
        TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType && TUSB_XFER_BULK == desc_ep->bmAttributes.xfer, 0);

        TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
        if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
          tu_edpt_stream_t *stream_tx = &p_cdc->stream.tx;

          tu_edpt_stream_open(stream_tx, desc_ep);
          if (_cdcd_cfg.tx_persistent) {
            tu_edpt_stream_write_xfer(rhport, stream_tx); // flush pending data
          } else {
            tu_edpt_stream_clear(stream_tx);
          }
        } else {
          tu_edpt_stream_t *stream_rx = &p_cdc->stream.rx;

          tu_edpt_stream_open(stream_rx, desc_ep);
          if (!_cdcd_cfg.rx_persistent) {
            tu_edpt_stream_clear(stream_rx);
          }
          TU_ASSERT(tu_edpt_stream_read_xfer(rhport, stream_rx) > 0, 0); // prepare for incoming data
        }
      }

      p_desc = tu_desc_next(p_desc);
    }
  }

  return (uint16_t)(p_desc - (const uint8_t *)itf_desc);
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool cdcd_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t* request) {
  // Handle class request only
  TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

  uint8_t itf;
  cdcd_interface_t* p_cdc;

  // Identify which interface to use
  for (itf = 0; itf < CFG_TUD_CDC; itf++) {
    p_cdc = &_cdcd_itf[itf];
    if (p_cdc->itf_num == request->wIndex) {
      break;
    }
  }
  TU_VERIFY(itf < CFG_TUD_CDC);

  switch (request->bRequest) {
    case CDC_REQUEST_SET_LINE_CODING:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG_DRV("  Set Line Coding\r\n");
        tud_control_xfer(rhport, request, &p_cdc->line_coding, sizeof(cdc_line_coding_t));
      } else if (stage == CONTROL_STAGE_ACK) {
        tud_cdc_line_coding_cb(itf, &p_cdc->line_coding);
      } else {
        // nothing to do
      }
      break;

    case CDC_REQUEST_GET_LINE_CODING:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG_DRV("  Get Line Coding\r\n");
        tud_control_xfer(rhport, request, &p_cdc->line_coding, sizeof(cdc_line_coding_t));
      }
      break;

    case CDC_REQUEST_SET_CONTROL_LINE_STATE:
      if (stage == CONTROL_STAGE_SETUP) {
        tud_control_status(rhport, request);
      } else if (stage == CONTROL_STAGE_ACK) {
        // CDC PSTN v1.2 section 6.3.12
        // Bit 0: Indicates if DTE is present or not.
        //        This signal corresponds to V.24 signal 108/2 and RS-232 signal DTR (Data Terminal Ready)
        // Bit 1: Carrier control for half-duplex modems.
        //        This signal corresponds to V.24 signal 105 and RS-232 signal RTS (Request to Send)
        bool const dtr = tu_bit_test(request->wValue, 0);
        bool const rts = tu_bit_test(request->wValue, 1);

        p_cdc->line_state = (uint8_t) request->wValue;

        // If enabled: fifo overwriting is disabled if DTR bit is set and vice versa
        if (_cdcd_cfg.tx_overwritabe_if_not_connected) {
          tu_fifo_set_overwritable(&p_cdc->stream.tx.ff, !dtr);
        } else {
          tu_fifo_set_overwritable(&p_cdc->stream.tx.ff, false);
        }

        TU_LOG_DRV("  Set Control Line State: DTR = %d, RTS = %d\r\n", dtr, rts);
        tud_cdc_line_state_cb(itf, dtr, rts); // invoke callback
      } else {
        // nothing to do
      }
      break;

    case CDC_REQUEST_SEND_BREAK:
      if (stage == CONTROL_STAGE_SETUP) {
        tud_control_status(rhport, request);
      } else if (stage == CONTROL_STAGE_ACK) {
        TU_LOG_DRV("  Send Break\r\n");
        tud_cdc_send_break_cb(itf, request->wValue);
      } else {
        // nothing to do
      }
      break;

    default:
      return false; // stall unsupported request
  }

  return true;
}

bool cdcd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void)result;

  uint8_t itf = find_cdc_itf(ep_addr);
  TU_ASSERT(itf < CFG_TUD_CDC);
  cdcd_interface_t *p_cdc     = &_cdcd_itf[itf];
  tu_edpt_stream_t *stream_rx = &p_cdc->stream.rx;
  tu_edpt_stream_t *stream_tx = &p_cdc->stream.tx;

  // Received new data, move to fifo
  if (ep_addr == stream_rx->ep_addr) {
    tu_edpt_stream_read_xfer_complete(stream_rx, xferred_bytes);

    // Check for wanted char and invoke wanted callback (multiple times if multiple wanted received)
    if (((signed char)p_cdc->wanted_char) != -1) {
      for (uint32_t i = 0; i < xferred_bytes; i++) {
        if ((p_cdc->wanted_char == (char)stream_rx->ep_buf[i]) && !tu_edpt_stream_empty(stream_rx)) {
          tud_cdc_rx_wanted_cb(itf, p_cdc->wanted_char);
        }
      }
    }

    // invoke receive callback if there is still data
    if (!tu_edpt_stream_empty(stream_rx)) {
      tud_cdc_rx_cb(itf);
    }

    tu_edpt_stream_read_xfer(rhport, stream_rx); // prepare for more data
  }

  // Data sent to host, we continue to fetch from tx fifo to send.
  // Note: This will cause incorrect baudrate set in line coding.
  //       Though maybe the baudrate is not really important !!!
  if (ep_addr == stream_tx->ep_addr) {
    // invoke transmit callback to possibly refill tx fifo
    tud_cdc_tx_complete_cb(itf);

    if (0 == tu_edpt_stream_write_xfer(rhport, stream_tx)) {
      // If there is no data left, a ZLP should be sent if needed
      tu_edpt_stream_write_zlp_if_needed(rhport, stream_tx, xferred_bytes);
    }
  }

  // Sent notification to host
  if (ep_addr == p_cdc->ep_notify) {
    tud_cdc_notify_complete_cb(itf);
  }

  return true;
}

#endif
