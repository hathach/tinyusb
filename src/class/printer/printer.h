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

#include <stdint.h>
#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_PRINTER)

  #include "device/usbd.h"
  #include "device/usbd_pvt.h"

// #include "bsp/board_api.h"

  #include "printer_device.h"
  #include "printer.h"


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef struct {
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;
  /*------------- From this point, data is not cleared by bus reset -------------*/

  // FIFO
  tu_fifo_t rx_ff;
  tu_fifo_t tx_ff;

  uint8_t rx_ff_buf[CFG_TUD_PRINTER_RX_BUFSIZE];
  uint8_t tx_ff_buf[CFG_TUD_PRINTER_TX_BUFSIZE];

  OSAL_MUTEX_DEF(rx_ff_mutex);
  OSAL_MUTEX_DEF(tx_ff_mutex);
} printer_interface_t;

  #define ITF_MEM_RESET_SIZE offsetof(printer_interface_t, wanted_char)

typedef struct {
  TUD_EPBUF_DEF(epout, CFG_TUD_PRINTER_EP_BUFSIZE);
  TUD_EPBUF_DEF(epin, CFG_TUD_PRINTER_EP_BUFSIZE);
} printer_epbuf_t;

static printer_interface_t                 _printer_itf[CFG_TUD_PRINTER];
CFG_TUD_MEM_SECTION static printer_epbuf_t _printer_epbuf[CFG_TUD_PRINTER];


//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

static tud_printer_configure_fifo_t _printer_fifo_cfg;

static bool _prep_out_transaction(uint8_t itf) {
  const uint8_t        rhport    = 0;
  printer_interface_t *p_printer = &_printer_itf[itf];
  printer_epbuf_t     *p_epbuf   = &_printer_epbuf[itf];

  // Skip if usb is not ready yet
  TU_VERIFY(tud_ready() && p_printer->ep_out);

  uint16_t available = tu_fifo_remaining(&p_printer->rx_ff);

  // Prepare for incoming data but only allow what we can store in the ring buffer.
  // TODO Actually we can still carry out the transfer, keeping count of received bytes
  // and slowly move it to the FIFO when read().
  // This pre-check reduces endpoint claiming
  TU_VERIFY(available >= CFG_TUD_PRINTER_EP_BUFSIZE);

  // claim endpoint
  TU_VERIFY(usbd_edpt_claim(rhport, p_printer->ep_out));

  // fifo can be changed before endpoint is claimed
  available = tu_fifo_remaining(&p_printer->rx_ff);

  if (available >= CFG_TUD_PRINTER_EP_BUFSIZE) {
    return usbd_edpt_xfer(rhport, p_printer->ep_out, p_epbuf->epout, CFG_TUD_PRINTER_EP_BUFSIZE);
  } else {
    // Release endpoint since we don't make any transfer
    usbd_edpt_release(rhport, p_printer->ep_out);
    return false;
  }
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

uint32_t tud_printer_n_available(uint8_t itf) {
  return tu_fifo_count(&_printer_itf[itf].rx_ff);
}

uint32_t tud_printer_n_read(uint8_t itf, void *buffer, uint32_t bufsize) {
  printer_interface_t *p_printer = &_printer_itf[itf];
  uint32_t             num_read  = tu_fifo_read_n(&p_printer->rx_ff, buffer, (uint16_t)TU_MIN(bufsize, UINT16_MAX));
  _prep_out_transaction(itf);
  return num_read;
}

bool tud_printer_n_peek(uint8_t itf, uint8_t *chr) {
  return tu_fifo_peek(&_printer_itf[itf].rx_ff, chr);
}

void tud_printer_n_read_flush(uint8_t itf) {
  printer_interface_t *p_printer = &_printer_itf[itf];
  tu_fifo_clear(&p_printer->rx_ff);
  _prep_out_transaction(itf);
}


//--------------------------------------------------------------------+
// USBD PRINTER DRIVER API
//--------------------------------------------------------------------+

void printer_init(void) {
  tu_memclr(_printer_itf, sizeof(_printer_itf));
  tu_memclr(&_printer_fifo_cfg, sizeof(_printer_fifo_cfg));

  for (uint8_t i = 0; i < CFG_TUD_PRINTER; i++) {
    printer_interface_t *p_printer = &_printer_itf[i];

    tu_fifo_config(&p_printer->rx_ff, p_printer->rx_ff_buf, TU_ARRAY_SIZE(p_printer->rx_ff_buf), 1, false);
    tu_fifo_config(&p_printer->tx_ff, p_printer->tx_ff_buf, TU_ARRAY_SIZE(p_printer->tx_ff_buf), 1, true);

  #if OSAL_MUTEX_REQUIRED
    osal_mutex_t mutex_rd = osal_mutex_create(&p_printer->rx_ff_mutex);
    osal_mutex_t mutex_wr = osal_mutex_create(&p_printer->tx_ff_mutex);
    TU_ASSERT(mutex_rd != NULL && mutex_wr != NULL, );

    tu_fifo_config_mutex(&p_printer->rx_ff, NULL, mutex_rd);
    tu_fifo_config_mutex(&p_printer->tx_ff, mutex_wr, NULL);
  #endif
  }
}

bool printer_deinit(void) {
  #if OSAL_MUTEX_REQUIRED
  for (uint8_t i = 0; i < CFG_TUD_PRINTER; i++) {
    printer_interface_t *p_printer = &_printer_itf[i];
    osal_mutex_t         mutex_rd  = p_printer->rx_ff.mutex_rd;
    osal_mutex_t         mutex_wr  = p_printer->tx_ff.mutex_rd;

    if (mutex_rd) {
      osal_mutex_delete(mutex_rd);
      tu_fifo_config_mutex(&p_printer->rx_ff, NULL, NULL);
    }

    if (mutex_wr) {
      osal_mutex_delete(mutex_wr);
      tu_fifo_config_mutex(&p_printer->tx_ff, NULL, NULL);
    }
  }
  #endif

  return true;
}

void printer_reset(uint8_t rhport) {
  (void)rhport;

  for (uint8_t i = 0; i < CFG_TUD_PRINTER; i++) {
    printer_interface_t *p_printer = &_printer_itf[i];

    tu_memclr(p_printer, sizeof(p_printer));
    if (!_printer_fifo_cfg.rx_persistent) {
      tu_fifo_clear(&p_printer->rx_ff);
    }
    if (!_printer_fifo_cfg.tx_persistent) {
      tu_fifo_clear(&p_printer->tx_ff);
    }
    // tu_fifo_set_overwritable(&p_printer->rx_ff, true);
    tu_fifo_set_overwritable(&p_printer->tx_ff, true);
  }
}

uint16_t printer_open(uint8_t rhport, const tusb_desc_interface_t *itf_desc, uint16_t max_len) {
  TU_VERIFY(TUSB_CLASS_PRINTER == itf_desc->bInterfaceClass, 0);

  // Identify available interface to open
  printer_interface_t *p_printer;
  uint8_t              printer_id;
  for (printer_id = 0; printer_id < CFG_TUD_PRINTER; printer_id++) {
    p_printer = &_printer_itf[printer_id];
    if (p_printer->ep_out == 0) {
      break;
    }
  }
  TU_ASSERT(printer_id < CFG_TUD_PRINTER);

  //------------- Interface -------------//
  uint16_t drv_len = sizeof(tusb_desc_interface_t);

  //------------- Endpoints -------------//
  TU_ASSERT(itf_desc->bNumEndpoints == 2);
  drv_len += 2 * sizeof(tusb_desc_endpoint_t);
  p_printer->itf_num    = 2;
  const uint8_t *p_desc = tu_desc_next(itf_desc);
  TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, 2, TUSB_XFER_BULK, &p_printer->ep_out, &p_printer->ep_in), 0);

  _prep_out_transaction(printer_id);

  return drv_len;
}

bool printer_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request) {
  TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

  if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
    //------------- STD Request -------------//
    if (stage != CONTROL_STAGE_SETUP) {
      return true;
    }
  } else if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
    switch (request->bRequest) {
      // https://www.usb.org/sites/default/files/usbprint11a021811.pdf
      case PRINTER_REQ_CONTROL_GET_DEVICE_ID:
        if (stage == CONTROL_STAGE_SETUP) {
          const char deviceId[] = "MANUFACTURER:ACME Manufacturing;"
                                  "MODEL:LaserBeam 9;"
                                  "COMMAND SET:PS;"
                                  "COMMENT:Anything you like;"
                                  "ACTIVE COMMAND SET:PS;";
          char       buffer[256];
          strcpy(buffer + 2, deviceId);
          buffer[0] = 0x00;
          buffer[1] = strlen(deviceId);
          return tud_control_xfer(rhport, request, buffer, strlen(deviceId) + 2);
        }
        break;
      case PRINTER_REQ_CONTROL_GET_PORT_STATUS:
        if (stage == CONTROL_STAGE_SETUP) {
          static uint8_t port_status = (0 << 3) | (1 << 1) | (1 << 2); // ~Paper empty + Selected + NoError
          return tud_control_xfer(rhport, request, &port_status, sizeof(port_status));
        }
        break;
      case PRINTER_REQ_CONTROL_SOFT_RESET:
        if (stage == CONTROL_STAGE_SETUP) {
          return false; // what to do ?
        }
        break;
      default:
        return false;
    }
  } else {
    return false;
  }
  return true;
}

bool printer_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  uint8_t              itf;
  printer_interface_t *p_printer;

  // Identify which interface to use
  for (itf = 0; itf < CFG_TUD_PRINTER; itf++) {
    p_printer = &_printer_itf[itf];
    if (ep_addr == p_printer->ep_out) {
      break;
    }
  }
  TU_ASSERT(itf < CFG_TUD_PRINTER);
  printer_epbuf_t *p_epbuf = &_printer_epbuf[itf];

  // Received new data
  if (ep_addr == p_printer->ep_out) {
    tu_fifo_write_n(&p_printer->rx_ff, p_epbuf->epout, (uint16_t)xferred_bytes);
    // invoke receive callback (if there is still data)
    if (tud_printer_rx_cb && !tu_fifo_empty(&p_printer->rx_ff)) {
      tud_printer_rx_cb(itf, xferred_bytes);
    }
    // prepare for OUT transaction
    _prep_out_transaction(itf);
  }

  return true;
}

#endif
