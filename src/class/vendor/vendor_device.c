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

#if (CFG_TUD_ENABLED && CFG_TUD_VENDOR)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "vendor_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t rhport;
  uint8_t itf_num;

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  /*------------- From this point, data is not cleared by bus reset -------------*/
  tu_edpt_stream_t tx_stream;
  tu_edpt_stream_t rx_stream;
  uint8_t          tx_ff_buf[CFG_TUD_VENDOR_TX_BUFSIZE];
  uint8_t          rx_ff_buf[CFG_TUD_VENDOR_RX_BUFSIZE];
  #else
  uint8_t  ep_in;
  uint8_t  ep_out;
  uint16_t ep_in_mps;
  uint16_t ep_out_mps;
  #endif
} vendord_interface_t;

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
    #define ITF_MEM_RESET_SIZE (offsetof(vendord_interface_t, itf_num) + TU_FIELD_SIZE(vendord_interface_t, itf_num))
  #else
    #define ITF_MEM_RESET_SIZE sizeof(vendord_interface_t)
  #endif

static vendord_interface_t _vendord_itf[CFG_TUD_VENDOR];

  // Skip local EP buffer if dedicated hw FIFO is supported or no fifo mode
  #if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0 || !CFG_TUD_VENDOR_TXRX_BUFFERED
typedef struct {
  TUD_EPBUF_DEF(epout, CFG_TUD_VENDOR_EPSIZE);
  TUD_EPBUF_DEF(epin, CFG_TUD_VENDOR_EPSIZE);
} vendord_epbuf_t;

CFG_TUD_MEM_SECTION static vendord_epbuf_t _vendord_epbuf[CFG_TUD_VENDOR];
  #endif

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tud_vendor_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize) {
  (void)idx;
  (void)buffer;
  (void)bufsize;
}

TU_ATTR_WEAK void tud_vendor_tx_cb(uint8_t idx, uint32_t sent_bytes) {
  (void)idx;
  (void) sent_bytes;
}

//--------------------------------------------------------------------
// Application API
//--------------------------------------------------------------------
bool tud_vendor_n_mounted(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  return (p_itf->rx_stream.ep_addr != 0) || (p_itf->tx_stream.ep_addr != 0);
  #else
  return (p_itf->ep_out != 0) || (p_itf->ep_in != 0);
  #endif
}

//--------------------------------------------------------------------+
// Read API
//--------------------------------------------------------------------+
  #if CFG_TUD_VENDOR_TXRX_BUFFERED
uint32_t tud_vendor_n_available(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_read_available(&p_itf->rx_stream);
}

bool tud_vendor_n_peek(uint8_t idx, uint8_t *u8) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_peek(&p_itf->rx_stream, u8);
}

uint32_t tud_vendor_n_read(uint8_t idx, void *buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_read(&p_itf->rx_stream, buffer, bufsize);
}

void tud_vendor_n_read_flush(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, );
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  tu_edpt_stream_clear(&p_itf->rx_stream);
  tu_edpt_stream_read_xfer(&p_itf->rx_stream);
}
#endif

#if CFG_TUD_VENDOR_RX_MANUAL_XFER
bool tud_vendor_n_read_xfer(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];

    #if CFG_TUD_VENDOR_TXRX_BUFFERED
  return tu_edpt_stream_read_xfer(&p_itf->rx_stream);

    #else
  // Non-FIFO mode
  TU_VERIFY(usbd_edpt_claim(p_itf->rhport, p_itf->ep_out));
  return usbd_edpt_xfer(p_itf->rhport, p_itf->ep_out, _vendord_epbuf[idx].epout, CFG_TUD_VENDOR_EPSIZE, false);
    #endif
}
  #endif


//--------------------------------------------------------------------+
// Write API
//--------------------------------------------------------------------+
uint32_t tud_vendor_n_write(uint8_t idx, const void *buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  return tu_edpt_stream_write(&p_itf->tx_stream, buffer, (uint16_t)bufsize);

  #else
  // non-fifo mode: direct transfer
  TU_VERIFY(usbd_edpt_claim(p_itf->rhport, p_itf->ep_in), 0);
  const uint32_t xact_len = tu_min32(bufsize, CFG_TUD_VENDOR_EPSIZE);
  memcpy(_vendord_epbuf[idx].epin, buffer, xact_len);
  TU_ASSERT(usbd_edpt_xfer(p_itf->rhport, p_itf->ep_in, _vendord_epbuf[idx].epin, (uint16_t)xact_len, false), 0);
  return xact_len;
  #endif
}

uint32_t tud_vendor_n_write_available(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  return tu_edpt_stream_write_available(&p_itf->tx_stream);

  #else
  // Non-FIFO mode
  TU_VERIFY(p_itf->ep_in > 0, 0); // must be opened
  return usbd_edpt_busy(p_itf->rhport, p_itf->ep_in) ? 0 : CFG_TUD_VENDOR_EPSIZE;
  #endif
}

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
uint32_t tud_vendor_n_write_flush(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_write_xfer(&p_itf->tx_stream);
}

bool tud_vendor_n_write_clear(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  tu_edpt_stream_clear(&p_itf->tx_stream);
  return true;
}
#endif

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void vendord_init(void) {
  tu_memclr(_vendord_itf, sizeof(_vendord_itf));

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  for (uint8_t i = 0; i < CFG_TUD_VENDOR; i++) {
    vendord_interface_t *p_itf = &_vendord_itf[i];

    #if CFG_TUD_EDPT_DEDICATED_HWFIFO
    uint8_t *epout_buf = NULL;
    uint8_t *epin_buf  = NULL;
    #else
    uint8_t *epout_buf = _vendord_epbuf[i].epout;
    uint8_t *epin_buf  = _vendord_epbuf[i].epin;
    #endif

    uint8_t *rx_ff_buf = p_itf->rx_ff_buf;
    tu_edpt_stream_init(&p_itf->rx_stream, false, false, false, rx_ff_buf, CFG_TUD_VENDOR_RX_BUFSIZE, epout_buf,
                        CFG_TUD_VENDOR_EPSIZE);

    uint8_t *tx_ff_buf = p_itf->tx_ff_buf;
    tu_edpt_stream_init(&p_itf->tx_stream, false, true, false, tx_ff_buf, CFG_TUD_VENDOR_TX_BUFSIZE, epin_buf,
                        CFG_TUD_VENDOR_EPSIZE);
  }
  #endif
}

bool vendord_deinit(void) {
  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  for (uint8_t i = 0; i < CFG_TUD_VENDOR; i++) {
    vendord_interface_t *p_itf = &_vendord_itf[i];
    tu_edpt_stream_deinit(&p_itf->rx_stream);
    tu_edpt_stream_deinit(&p_itf->tx_stream);
  }
  #endif
  return true;
}

void vendord_reset(uint8_t rhport) {
  (void) rhport;

  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++) {
    vendord_interface_t* p_itf = &_vendord_itf[i];
    tu_memclr(p_itf, ITF_MEM_RESET_SIZE);

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
    tu_edpt_stream_clear(&p_itf->rx_stream);
    tu_edpt_stream_close(&p_itf->rx_stream);
    tu_edpt_stream_clear(&p_itf->tx_stream);
    tu_edpt_stream_close(&p_itf->tx_stream);
  #endif
  }
}

// Find vendor interface by endpoint address
static uint8_t find_vendor_itf(uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUD_VENDOR; idx++) {
    const vendord_interface_t *p_vendor = &_vendord_itf[idx];
    if (ep_addr == 0) {
      // find unused: require both ep == 0
  #if CFG_TUD_VENDOR_TXRX_BUFFERED
      if (p_vendor->rx_stream.ep_addr == 0 && p_vendor->tx_stream.ep_addr == 0) {
        return idx;
      }
  #else
      if (p_vendor->ep_out == 0 && p_vendor->ep_in == 0) {
        return idx;
      }
  #endif
    } else {
  #if CFG_TUD_VENDOR_TXRX_BUFFERED
      if (ep_addr == p_vendor->rx_stream.ep_addr || ep_addr == p_vendor->tx_stream.ep_addr) {
        return idx;
      }
  #else
      if (ep_addr == p_vendor->ep_out || ep_addr == p_vendor->ep_in) {
        return idx;
      }
  #endif
    }
  }
  return 0xff;
}

uint16_t vendord_open(uint8_t rhport, const tusb_desc_interface_t *desc_itf, uint16_t max_len) {
  TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == desc_itf->bInterfaceClass, 0);
  const uint8_t* desc_end = (const uint8_t*)desc_itf + max_len;
  const uint8_t* p_desc = tu_desc_next(desc_itf);

  // Find available interface
  const uint8_t idx = find_vendor_itf(0);
  TU_ASSERT(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_vendor = &_vendord_itf[idx];
  p_vendor->rhport  = rhport;
  p_vendor->itf_num = desc_itf->bInterfaceNumber;

  while (tu_desc_in_bounds(p_desc, desc_end)) {
    const uint8_t desc_type = tu_desc_type(p_desc);
    if (desc_type == TUSB_DESC_INTERFACE || desc_type == TUSB_DESC_INTERFACE_ASSOCIATION) {
      break; // end of this interface
    } else if (desc_type == TUSB_DESC_ENDPOINT) {
      const tusb_desc_endpoint_t* desc_ep = (const tusb_desc_endpoint_t*) p_desc;
      TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
      // open endpoint stream
      if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
        tu_edpt_stream_t *tx_stream = &p_vendor->tx_stream;
        tu_edpt_stream_open(tx_stream, rhport, desc_ep);
        tu_edpt_stream_write_xfer(tx_stream); // flush pending data
      } else {
        tu_edpt_stream_t *rx_stream = &p_vendor->rx_stream;
        tu_edpt_stream_open(rx_stream, rhport, desc_ep);
    #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
        TU_ASSERT(tu_edpt_stream_read_xfer(rx_stream) > 0, 0); // prepare for incoming data
    #endif
      }
  #else
      // Non-FIFO mode: store endpoint info
      if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
        p_vendor->ep_in     = desc_ep->bEndpointAddress;
        p_vendor->ep_in_mps = tu_edpt_packet_size(desc_ep);
      } else {
        p_vendor->ep_out     = desc_ep->bEndpointAddress;
        p_vendor->ep_out_mps = tu_edpt_packet_size(desc_ep);
    #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
        // Prepare for incoming data
        TU_ASSERT(usbd_edpt_xfer(rhport, p_vendor->ep_out, _vendord_epbuf[idx].epout, CFG_TUD_VENDOR_EPSIZE, false), 0);
    #endif
      }
  #endif
    }

    p_desc = tu_desc_next(p_desc);
  }

  return (uint16_t)((uintptr_t)p_desc - (uintptr_t)desc_itf);
}

bool vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void)rhport;
  (void)result;
  const uint8_t idx = find_vendor_itf(ep_addr);
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_vendor = &_vendord_itf[idx];

#if CFG_TUD_VENDOR_TXRX_BUFFERED
  if (ep_addr == p_vendor->rx_stream.ep_addr) {
    // Put received data to FIFO
    tu_edpt_stream_read_xfer_complete(&p_vendor->rx_stream, xferred_bytes);
    tud_vendor_rx_cb(idx, NULL, 0);
    #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
    tu_edpt_stream_read_xfer(&p_vendor->rx_stream); // prepare next data
    #endif
  } else if (ep_addr == p_vendor->tx_stream.ep_addr) {
    // Send complete
    tud_vendor_tx_cb(idx, (uint16_t)xferred_bytes);

    // try to send more if possible
    if (0 == tu_edpt_stream_write_xfer(&p_vendor->tx_stream)) {
      // If there is no data left, a ZLP should be sent if xferred_bytes is multiple of EP Packet size and not zero
      tu_edpt_stream_write_zlp_if_needed(&p_vendor->tx_stream, xferred_bytes);
    }
  }
  #else
  if (ep_addr == p_vendor->ep_out) {
    // Non-FIFO mode: invoke callback with buffer
    tud_vendor_rx_cb(idx, _vendord_epbuf[idx].epout, xferred_bytes);
    #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
    usbd_edpt_xfer(rhport, p_vendor->ep_out, _vendord_epbuf[idx].epout, CFG_TUD_VENDOR_EPSIZE, false);
    #endif
  } else if (ep_addr == p_vendor->ep_in) {
    // Send complete
    tud_vendor_tx_cb(idx, (uint16_t)xferred_bytes);
  }
  #endif

  return true;
}

#endif
