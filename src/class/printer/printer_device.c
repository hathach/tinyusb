/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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

#if (CFG_TUD_ENABLED && CFG_TUD_PRINTER)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "printer_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef struct {
  uint8_t itf_num;

  /*------------- From this point, data is not cleared by bus reset -------------*/

  tu_edpt_stream_t rx_stream;
  tu_edpt_stream_t tx_stream;

  uint8_t rx_ff_buf[CFG_TUD_PRINTER_RX_BUFSIZE];
  uint8_t tx_ff_buf[CFG_TUD_PRINTER_TX_BUFSIZE];
} printer_interface_t;

#define ITF_MEM_RESET_SIZE offsetof(printer_interface_t, rx_stream)

#if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0
typedef struct {
  TUD_EPBUF_DEF(epout, CFG_TUD_PRINTER_EP_BUFSIZE);
  TUD_EPBUF_DEF(epin, CFG_TUD_PRINTER_EP_BUFSIZE);
} printer_epbuf_t;

CFG_TUD_MEM_SECTION static printer_epbuf_t _printer_epbuf[CFG_TUD_PRINTER];
#endif

static printer_interface_t _printer_itf[CFG_TUD_PRINTER];

//--------------------------------------------------------------------+
// INTERNAL HELPERS
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint8_t _find_itf(uint8_t ep_addr) {
  for (uint8_t i = 0; i < CFG_TUD_PRINTER; i++) {
    const printer_interface_t *p = &_printer_itf[i];
    if (ep_addr == p->rx_stream.ep_addr || ep_addr == p->tx_stream.ep_addr) {
      return i;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tud_printer_rx_cb(uint8_t itf) {
  (void)itf;
}

TU_ATTR_WEAK void tud_printer_tx_complete_cb(uint8_t itf) {
  (void)itf;
}

TU_ATTR_WEAK void tud_printer_request_complete_cb(uint8_t itf, tusb_control_request_t const *request) {
  (void)itf;
  (void)request;
}

TU_ATTR_WEAK uint8_t const *tud_printer_get_device_id_cb(uint8_t itf) {
  (void)itf;
  return NULL;
}

TU_ATTR_WEAK uint8_t tud_printer_get_port_status_cb(uint8_t itf) {
  (void)itf;
  return 0x18; // not error, selected, paper not empty
}

TU_ATTR_WEAK void tud_printer_soft_reset_cb(uint8_t itf) {
  (void)itf;
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
uint32_t tud_printer_n_read_available(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_PRINTER, 0);
  return tu_edpt_stream_read_available(&_printer_itf[itf].rx_stream);
}

uint32_t tud_printer_n_read(uint8_t itf, void *buffer, uint32_t bufsize) {
  TU_VERIFY(itf < CFG_TUD_PRINTER, 0);
  return tu_edpt_stream_read(&_printer_itf[itf].rx_stream, buffer, bufsize);
}

bool tud_printer_n_peek(uint8_t itf, uint8_t *chr) {
  TU_VERIFY(itf < CFG_TUD_PRINTER);
  return tu_edpt_stream_peek(&_printer_itf[itf].rx_stream, chr);
}

void tud_printer_n_read_flush(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_PRINTER, );
  printer_interface_t *p = &_printer_itf[itf];
  tu_edpt_stream_clear(&p->rx_stream);
  tu_edpt_stream_read_xfer(&p->rx_stream);
}

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+
uint32_t tud_printer_n_write(uint8_t itf, const void *buffer, uint32_t bufsize) {
  TU_VERIFY(itf < CFG_TUD_PRINTER, 0);
  return tu_edpt_stream_write(&_printer_itf[itf].tx_stream, buffer, bufsize);
}

uint32_t tud_printer_n_write_flush(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_PRINTER, 0);
  return tu_edpt_stream_write_xfer(&_printer_itf[itf].tx_stream);
}

uint32_t tud_printer_n_write_available(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_PRINTER, 0);
  return tu_edpt_stream_write_available(&_printer_itf[itf].tx_stream);
}

bool tud_printer_n_write_clear(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_PRINTER);
  tu_edpt_stream_clear(&_printer_itf[itf].tx_stream);
  return true;
}

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void printerd_init(void) {
  tu_memclr(_printer_itf, sizeof(_printer_itf));

  for (uint8_t i = 0; i < CFG_TUD_PRINTER; i++) {
    printer_interface_t *p = &_printer_itf[i];

  #if CFG_TUD_EDPT_DEDICATED_HWFIFO
    uint8_t *epout_buf = NULL;
    uint8_t *epin_buf  = NULL;
  #else
    uint8_t *epout_buf = _printer_epbuf[i].epout;
    uint8_t *epin_buf  = _printer_epbuf[i].epin;
  #endif

    tu_edpt_stream_init(&p->rx_stream, false, false, false,
                        p->rx_ff_buf, CFG_TUD_PRINTER_RX_BUFSIZE,
                        epout_buf, CFG_TUD_PRINTER_EP_BUFSIZE);

    tu_edpt_stream_init(&p->tx_stream, false, true, true,
                        p->tx_ff_buf, CFG_TUD_PRINTER_TX_BUFSIZE,
                        epin_buf, CFG_TUD_PRINTER_EP_BUFSIZE);
  }
}

bool printerd_deinit(void) {
  for (uint8_t i = 0; i < CFG_TUD_PRINTER; i++) {
    printer_interface_t *p = &_printer_itf[i];
    tu_edpt_stream_deinit(&p->rx_stream);
    tu_edpt_stream_deinit(&p->tx_stream);
  }
  return true;
}

void printerd_reset(uint8_t rhport) {
  (void)rhport;

  for (uint8_t i = 0; i < CFG_TUD_PRINTER; i++) {
    printer_interface_t *p = &_printer_itf[i];
    tu_memclr(p, ITF_MEM_RESET_SIZE);
    tu_edpt_stream_close(&p->rx_stream);
    tu_edpt_stream_close(&p->tx_stream);
  }
}

uint16_t printerd_open(uint8_t rhport, const tusb_desc_interface_t *itf_desc, uint16_t max_len) {
  TU_VERIFY(TUSB_CLASS_PRINTER == itf_desc->bInterfaceClass, 0);

  // Find available interface slot
  uint8_t const printer_id = _find_itf(0);
  TU_ASSERT(printer_id < CFG_TUD_PRINTER, 0);
  printer_interface_t *p = &_printer_itf[printer_id];

  p->itf_num = itf_desc->bInterfaceNumber;

  //------------- Endpoints -------------//
  const uint8_t *p_desc   = (const uint8_t *)itf_desc;
  const uint8_t *desc_end = p_desc + max_len;
  uint16_t drv_len = sizeof(tusb_desc_interface_t);

  p_desc = tu_desc_next(itf_desc);
  for (uint8_t e = 0; e < itf_desc->bNumEndpoints; e++) {
    TU_VERIFY(tu_desc_in_bounds(p_desc, desc_end), 0);
    const tusb_desc_endpoint_t *desc_ep = (const tusb_desc_endpoint_t *)p_desc;
    TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType && TUSB_XFER_BULK == desc_ep->bmAttributes.xfer, 0);

    TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);

    if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
      tu_edpt_stream_open(&p->tx_stream, rhport, desc_ep);
      tu_edpt_stream_clear(&p->tx_stream);
    } else {
      tu_edpt_stream_open(&p->rx_stream, rhport, desc_ep);
      tu_edpt_stream_clear(&p->rx_stream);
      TU_ASSERT(tu_edpt_stream_read_xfer(&p->rx_stream) > 0, 0);
    }

    drv_len += sizeof(tusb_desc_endpoint_t);
    p_desc = tu_desc_next(p_desc);
  }

  return drv_len;
}

bool printerd_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request) {
  TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE &&
    request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

  // GET_DEVICE_ID: wIndex = (interface_number << 8) | alt_setting
  // GET_PORT_STATUS / SOFT_RESET: wIndex = interface_number
  uint8_t itf_num;
  if (TUSB_PRINTER_REQUEST_GET_DEVICE_ID == request->bRequest) {
    itf_num = tu_u16_high(request->wIndex);
  } else {
    itf_num = tu_u16_low(request->wIndex);
  }

  // Find the printer instance index from the USB interface number
  uint8_t itf = TUSB_INDEX_INVALID_8;
  for (uint8_t i = 0; i < CFG_TUD_PRINTER; i++) {
    if (_printer_itf[i].itf_num == itf_num) {
      itf = i;
      break;
    }
  }
  TU_VERIFY(itf < CFG_TUD_PRINTER);

  // https://www.usb.org/sites/default/files/usbprint11a021811.pdf
  if (stage == CONTROL_STAGE_SETUP) {
    switch (request->bRequest) {
      case TUSB_PRINTER_REQUEST_GET_DEVICE_ID: {
        const uint8_t *device_id = tud_printer_get_device_id_cb(itf);
        TU_VERIFY(device_id);
        const uint16_t total_len = (uint16_t)((device_id[0] << 8) | device_id[1]);
        return tud_control_xfer(rhport, request, (void *)(uintptr_t)device_id, total_len);
      }

      case TUSB_PRINTER_REQUEST_GET_PORT_STATUS: {
        static uint8_t port_status;
        port_status = tud_printer_get_port_status_cb(itf);
        return tud_control_xfer(rhport, request, &port_status, sizeof(port_status));
      }

      case TUSB_PRINTER_REQUEST_SOFT_RESET:
        tud_printer_soft_reset_cb(itf);
        tud_control_status(rhport, request);
        return true;

      default:
        return false;
    }
  } else if (stage == CONTROL_STAGE_ACK) {
    tud_printer_request_complete_cb(itf, request);
  }

  return true;
}

bool printerd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void)rhport;
  (void)result;

  uint8_t const itf = _find_itf(ep_addr);
  TU_ASSERT(itf < CFG_TUD_PRINTER);
  printer_interface_t *p = &_printer_itf[itf];

  // Received new data
  if (ep_addr == p->rx_stream.ep_addr) {
    tu_edpt_stream_read_xfer_complete(&p->rx_stream, xferred_bytes);

    if (!tu_edpt_stream_empty(&p->rx_stream)) {
      tud_printer_rx_cb(itf);
    }

    tu_edpt_stream_read_xfer(&p->rx_stream);
  }

  // Data sent to host
  if (ep_addr == p->tx_stream.ep_addr) {
    tud_printer_tx_complete_cb(itf);

    if (0 == tu_edpt_stream_write_xfer(&p->tx_stream)) {
      tu_edpt_stream_write_zlp_if_needed(&p->tx_stream, xferred_bytes);
    }
  }

  return true;
}

#endif
