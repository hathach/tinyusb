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

  /*------------- From this point, data is not cleared by bus reset -------------*/
  struct {
    tu_edpt_stream_t tx;
    tu_edpt_stream_t rx;

  #if CFG_TUD_VENDOR_TX_BUFSIZE > 0
    uint8_t tx_ff_buf[CFG_TUD_VENDOR_TX_BUFSIZE];
  #endif

  #if CFG_TUD_VENDOR_RX_BUFSIZE > 0
    uint8_t rx_ff_buf[CFG_TUD_VENDOR_RX_BUFSIZE];
  #endif
  } stream;
} vendord_interface_t;

#define ITF_MEM_RESET_SIZE (offsetof(vendord_interface_t, itf_num) + sizeof(((vendord_interface_t *)0)->itf_num))

static vendord_interface_t _vendord_itf[CFG_TUD_VENDOR];

#if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0 || CFG_TUD_VENDOR_RX_BUFSIZE == 0
typedef struct {
    // Skip local EP buffer if dedicated hw FIFO is supported
    #if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0 || CFG_TUD_VENDOR_RX_BUFSIZE == 0
  TUD_EPBUF_DEF(epout, CFG_TUD_VENDOR_EPSIZE);
    #endif

    // Skip local EP buffer if dedicated hw FIFO is supported
    #if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0
  TUD_EPBUF_DEF(epin, CFG_TUD_VENDOR_EPSIZE);
    #endif
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
  return p_itf->stream.rx.ep_addr || p_itf->stream.tx.ep_addr;
}

//--------------------------------------------------------------------+
// Read API
//--------------------------------------------------------------------+
#if CFG_TUD_VENDOR_RX_BUFSIZE > 0
uint32_t tud_vendor_n_available(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_read_available(&p_itf->stream.rx);
}

bool tud_vendor_n_peek(uint8_t idx, uint8_t *u8) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_peek(&p_itf->stream.rx, u8);
}

uint32_t tud_vendor_n_read(uint8_t idx, void *buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_read(&p_itf->stream.rx, buffer, bufsize);
}

void tud_vendor_n_read_flush(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, );
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  tu_edpt_stream_clear(&p_itf->stream.rx);
  tu_edpt_stream_read_xfer(&p_itf->stream.rx);
}
#endif

#if CFG_TUD_VENDOR_RX_MANUAL_XFER
bool tud_vendor_n_read_xfer(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_read_xfer(&p_itf->stream.rx);
}
#endif


//--------------------------------------------------------------------+
// Write API
//--------------------------------------------------------------------+
uint32_t tud_vendor_n_write(uint8_t idx, const void *buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_write(&p_itf->stream.tx, buffer, (uint16_t)bufsize);
}

#if CFG_TUD_VENDOR_TX_BUFSIZE > 0
uint32_t tud_vendor_n_write_flush(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_write_xfer(&p_itf->stream.tx);
}

uint32_t tud_vendor_n_write_available(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return tu_edpt_stream_write_available(&p_itf->stream.tx);
}

bool tud_vendor_n_write_clear(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  tu_edpt_stream_clear(&p_itf->stream.tx);
  return true;
}
#endif

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void vendord_init(void) {
  tu_memclr(_vendord_itf, sizeof(_vendord_itf));

  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++) {
    vendord_interface_t* p_itf = &_vendord_itf[i];

  #if CFG_TUD_EDPT_DEDICATED_HWFIFO
    #if CFG_TUD_VENDOR_RX_BUFSIZE == 0 // non-fifo rx still need ep buffer
    uint8_t *epout_buf = _vendord_epbuf[i].epout;
    #else
    uint8_t *epout_buf = NULL;
    #endif

    uint8_t *epin_buf = NULL;
  #else
    uint8_t *epout_buf = _vendord_epbuf[i].epout;
    uint8_t *epin_buf  = _vendord_epbuf[i].epin;
  #endif

  #if CFG_TUD_VENDOR_RX_BUFSIZE > 0
    uint8_t *rx_ff_buf = p_itf->stream.rx_ff_buf;
  #else
    uint8_t *rx_ff_buf = NULL;
  #endif

    tu_edpt_stream_init(&p_itf->stream.rx, false, false, false, rx_ff_buf, CFG_TUD_VENDOR_RX_BUFSIZE, epout_buf,
                        CFG_TUD_VENDOR_EPSIZE);

  #if CFG_TUD_VENDOR_TX_BUFSIZE > 0
    uint8_t *tx_ff_buf = p_itf->stream.tx_ff_buf;
  #else
    uint8_t *tx_ff_buf = NULL;
  #endif

    tu_edpt_stream_init(&p_itf->stream.tx, false, true, false, tx_ff_buf, CFG_TUD_VENDOR_TX_BUFSIZE, epin_buf,
                        CFG_TUD_VENDOR_EPSIZE);
  }
}

bool vendord_deinit(void) {
  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++) {
    vendord_interface_t* p_itf = &_vendord_itf[i];
    tu_edpt_stream_deinit(&p_itf->stream.rx);
    tu_edpt_stream_deinit(&p_itf->stream.tx);
  }
  return true;
}

void vendord_reset(uint8_t rhport) {
  (void) rhport;

  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++) {
    vendord_interface_t* p_itf = &_vendord_itf[i];
    tu_memclr(p_itf, ITF_MEM_RESET_SIZE);

    tu_edpt_stream_clear(&p_itf->stream.rx);
    tu_edpt_stream_close(&p_itf->stream.rx);

    tu_edpt_stream_clear(&p_itf->stream.tx);
    tu_edpt_stream_close(&p_itf->stream.tx);
  }
}

// Find vendor interface by endpoint address
static uint8_t find_vendor_itf(uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUD_VENDOR; idx++) {
    const vendord_interface_t *p_vendor = &_vendord_itf[idx];
    if (ep_addr == 0) {
      // find unused: require both ep == 0
      if (p_vendor->stream.rx.ep_addr == 0 && p_vendor->stream.tx.ep_addr == 0) {
        return idx;
      }
    } else if (ep_addr == p_vendor->stream.rx.ep_addr || ep_addr == p_vendor->stream.tx.ep_addr) {
      return idx;
    } else {
      // nothing to do
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

      // open endpoint stream, skip if already opened (multiple IN/OUT endpoints)
      if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
        tu_edpt_stream_t *stream_tx = &p_vendor->stream.tx;
        if (stream_tx->ep_addr == 0) {
          tu_edpt_stream_open(stream_tx, rhport, desc_ep);
          tu_edpt_stream_write_xfer(stream_tx); // flush pending data
        }
      } else {
        tu_edpt_stream_t *stream_rx = &p_vendor->stream.rx;
        if (stream_rx->ep_addr == 0) {
          tu_edpt_stream_open(stream_rx, rhport, desc_ep);
  #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
          TU_ASSERT(tu_edpt_stream_read_xfer(stream_rx) > 0, 0); // prepare for incoming data
  #endif
        }
      }
    }

    p_desc = tu_desc_next(p_desc);
  }

  return (uint16_t) ((uintptr_t) p_desc - (uintptr_t) desc_itf);
}

bool vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void)rhport;
  (void)result;
  const uint8_t idx = find_vendor_itf(ep_addr);
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_vendor = &_vendord_itf[idx];

  if (ep_addr == p_vendor->stream.rx.ep_addr) {
  #if CFG_TUD_VENDOR_RX_BUFSIZE
    // Received new data: put into stream's fifo
    tu_edpt_stream_read_xfer_complete(&p_vendor->stream.rx, xferred_bytes);
  #endif

    // invoke callback
  #if CFG_TUD_VENDOR_RX_BUFSIZE == 0
    tud_vendor_rx_cb(idx, p_vendor->stream.rx.ep_buf, xferred_bytes);
  #else
    tud_vendor_rx_cb(idx, NULL, 0);
  #endif

  #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
    tu_edpt_stream_read_xfer(&p_vendor->stream.rx); // prepare next data
  #endif
  } else if (ep_addr == p_vendor->stream.tx.ep_addr) {
    // Send complete
    tud_vendor_tx_cb(idx, (uint16_t)xferred_bytes);

  #if CFG_TUD_VENDOR_TX_BUFSIZE > 0
    // try to send more if possible
    if (0 == tu_edpt_stream_write_xfer(&p_vendor->stream.tx)) {
      // If there is no data left, a ZLP should be sent if xferred_bytes is multiple of EP Packet size and not zero
      tu_edpt_stream_write_zlp_if_needed(&p_vendor->stream.tx, xferred_bytes);
    }
  #endif
  }

  return true;
}

#endif
