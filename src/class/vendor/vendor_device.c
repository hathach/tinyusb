/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
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

  #if CFG_TUD_VENDOR_EP_INT_OUT
  uint8_t  ep_int_out;
  uint16_t int_rx_xfer_len;
  #endif
  #if CFG_TUD_VENDOR_EP_INT_IN
  uint8_t  ep_int_in;
  #endif
  #if CFG_TUD_VENDOR_EP_ISO_OUT
  uint8_t  ep_iso_out;
  uint16_t iso_rx_xfer_len;
  const tusb_desc_endpoint_t* iso_out_desc; // for deactivation on altsetting de-selection
  #endif
  #if CFG_TUD_VENDOR_EP_ISO_IN
  uint8_t  ep_iso_in;
  const tusb_desc_endpoint_t* iso_in_desc; // for deactivation on altsetting de-selection
  #endif
  #if CFG_TUD_VENDOR_ALT_SETTINGS // implies non-buffered: fields cleared by bus reset
  uint8_t        cur_alt;
  const uint8_t* p_itf_desc; // whole interface block incl. all altsettings (static app descriptor)
  uint16_t       itf_desc_len;
  #endif

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  /*------------- From this point, data is not cleared by bus reset -------------*/
  tu_edpt_stream_t tx_stream;
  tu_edpt_stream_t rx_stream;
  uint8_t          tx_ff_buf[CFG_TUD_VENDOR_TX_BUFSIZE];
  uint8_t          rx_ff_buf[CFG_TUD_VENDOR_RX_BUFSIZE];
  #else
  uint8_t  ep_in;
  uint8_t  ep_out;
  uint16_t rx_xfer_len;
  #endif
} vendord_interface_t;

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
    // The reset region is everything before the streams; tx_stream is the first preserved field
    // (see the struct comment), so its offset is exactly that boundary regardless of which endpoint
    // gates are enabled.
    #define ITF_MEM_RESET_SIZE offsetof(vendord_interface_t, tx_stream)
  #else
    #define ITF_MEM_RESET_SIZE sizeof(vendord_interface_t)
  #endif

static vendord_interface_t _vendord_itf[CFG_TUD_VENDOR];

// Skip local EP buffer if dedicated hw FIFO is supported or no fifo mode
#if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0 || !CFG_TUD_VENDOR_TXRX_BUFFERED
typedef struct {
  TUD_EPBUF_DEF(epout, CFG_TUD_VENDOR_RX_EPSIZE);
  TUD_EPBUF_DEF(epin, CFG_TUD_VENDOR_TX_EPSIZE);
} vendord_epbuf_t;

CFG_TUD_MEM_SECTION static vendord_epbuf_t _vendord_epbuf[CFG_TUD_VENDOR];
#endif

#if CFG_TUD_VENDOR_EP_INT_OUT || CFG_TUD_VENDOR_EP_INT_IN
typedef struct {
  #if CFG_TUD_VENDOR_EP_INT_OUT
  TUD_EPBUF_DEF(int_out, CFG_TUD_VENDOR_EP_INT_OUT_BUFSIZE);
  #endif
  #if CFG_TUD_VENDOR_EP_INT_IN
  TUD_EPBUF_DEF(int_in, CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE);
  #endif
} vendord_int_epbuf_t;

CFG_TUD_MEM_SECTION static vendord_int_epbuf_t _vendord_int_epbuf[CFG_TUD_VENDOR];
#endif

#if CFG_TUD_VENDOR_EP_ISO_OUT || CFG_TUD_VENDOR_EP_ISO_IN
typedef struct {
  #if CFG_TUD_VENDOR_EP_ISO_OUT
  TUD_EPBUF_DEF(iso_out, CFG_TUD_VENDOR_EP_ISO_OUT_BUFSIZE);
  #endif
  #if CFG_TUD_VENDOR_EP_ISO_IN
  TUD_EPBUF_DEF(iso_in, CFG_TUD_VENDOR_EP_ISO_IN_BUFSIZE);
  #endif
} vendord_iso_epbuf_t;

CFG_TUD_MEM_SECTION static vendord_iso_epbuf_t _vendord_iso_epbuf[CFG_TUD_VENDOR];
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

#if CFG_TUD_VENDOR_EP_INT_OUT
TU_ATTR_WEAK void tud_vendor_int_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize) {
  (void)idx;
  (void)buffer;
  (void)bufsize;
}
#endif

#if CFG_TUD_VENDOR_EP_INT_IN
TU_ATTR_WEAK void tud_vendor_int_tx_cb(uint8_t idx, uint32_t sent_bytes) {
  (void)idx;
  (void)sent_bytes;
}
#endif

#if CFG_TUD_VENDOR_EP_ISO_OUT
TU_ATTR_WEAK void tud_vendor_iso_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize) {
  (void)idx;
  (void)buffer;
  (void)bufsize;
}
#endif

#if CFG_TUD_VENDOR_EP_ISO_IN
TU_ATTR_WEAK void tud_vendor_iso_tx_cb(uint8_t idx, uint32_t sent_bytes) {
  (void)idx;
  (void)sent_bytes;
}
#endif

bool tud_vendor_n_mounted(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];

  // bulk may be absent (interrupt-only vendor interface): count the interrupt endpoints too
  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  bool mounted = (p_itf->rx_stream.ep_addr != 0) || (p_itf->tx_stream.ep_addr != 0);
  #else
  bool mounted = (p_itf->ep_out != 0) || (p_itf->ep_in != 0);
  #endif
  #if CFG_TUD_VENDOR_EP_INT_OUT
  mounted = mounted || (p_itf->ep_int_out != 0);
  #endif
  #if CFG_TUD_VENDOR_EP_INT_IN
  mounted = mounted || (p_itf->ep_int_in != 0);
  #endif
  // an altsetting may expose only isochronous endpoints; count them so apps that gate an iso
  // pump on tud_vendor_mounted() still arm it
  #if CFG_TUD_VENDOR_EP_ISO_OUT
  mounted = mounted || (p_itf->ep_iso_out != 0);
  #endif
  #if CFG_TUD_VENDOR_EP_ISO_IN
  mounted = mounted || (p_itf->ep_iso_in != 0);
  #endif
  return mounted;
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

// Shared non-buffered transfer helpers for the bulk / interrupt / isochronous endpoints, which are
// identical apart from the endpoint, its epbuf and its buffer size. TU_ATTR_UNUSED: in buffered
// mode with the int/iso gates off none is referenced, and clang/IAR error on an unused static.
TU_ATTR_UNUSED static inline uint32_t vendord_ep_write(vendord_interface_t *p_itf, uint8_t ep, uint8_t *epbuf,
                                                       uint32_t bufsize, const void *buffer, uint32_t len) {
  TU_VERIFY(ep > 0, 0); // must be opened
  TU_VERIFY(usbd_edpt_claim(p_itf->rhport, ep), 0);
  const uint32_t xact_len = tu_min32(len, bufsize);
  memcpy(epbuf, buffer, xact_len);
  TU_ASSERT(usbd_edpt_xfer(p_itf->rhport, ep, epbuf, (uint16_t) xact_len, false), 0);
  return xact_len;
}

TU_ATTR_UNUSED static inline uint32_t vendord_ep_write_available(vendord_interface_t *p_itf, uint8_t ep, uint32_t bufsize) {
  TU_VERIFY(ep > 0, 0); // must be opened
  return usbd_edpt_busy(p_itf->rhport, ep) ? 0 : bufsize;
}

TU_ATTR_UNUSED static inline bool vendord_ep_read_xfer(vendord_interface_t *p_itf, uint8_t ep, uint8_t *epbuf, uint16_t xfer_len) {
  TU_VERIFY(ep > 0); // must be opened
  TU_VERIFY(usbd_edpt_claim(p_itf->rhport, ep));
  return usbd_edpt_xfer(p_itf->rhport, ep, epbuf, xfer_len, false);
}

  #if CFG_TUD_VENDOR_RX_MANUAL_XFER
bool tud_vendor_n_read_xfer(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];

    #if CFG_TUD_VENDOR_TXRX_BUFFERED
  return tu_edpt_stream_read_xfer(&p_itf->rx_stream);

    #else
  // Non-FIFO mode (0 while an altsetting without a bulk OUT ep is active)
  return vendord_ep_read_xfer(p_itf, p_itf->ep_out, _vendord_epbuf[idx].epout, p_itf->rx_xfer_len);
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
  // non-fifo mode: direct transfer (ep_in is 0 while an altsetting without a bulk IN ep is active)
  return vendord_ep_write(p_itf, p_itf->ep_in, _vendord_epbuf[idx].epin, CFG_TUD_VENDOR_TX_EPSIZE, buffer, bufsize);
  #endif
}

uint32_t tud_vendor_n_write_available(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
  return tu_edpt_stream_write_available(&p_itf->tx_stream);

  #else
  return vendord_ep_write_available(p_itf, p_itf->ep_in, CFG_TUD_VENDOR_TX_EPSIZE);
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
// Interrupt endpoint API
//--------------------------------------------------------------------+
#if CFG_TUD_VENDOR_EP_INT_OUT
bool tud_vendor_n_int_read_xfer(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return vendord_ep_read_xfer(p_itf, p_itf->ep_int_out, _vendord_int_epbuf[idx].int_out, p_itf->int_rx_xfer_len);
}
#endif

#if CFG_TUD_VENDOR_EP_INT_IN
uint32_t tud_vendor_n_int_write(uint8_t idx, const void *buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return vendord_ep_write(p_itf, p_itf->ep_int_in, _vendord_int_epbuf[idx].int_in, CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE, buffer, bufsize);
}

uint32_t tud_vendor_n_int_write_available(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return vendord_ep_write_available(p_itf, p_itf->ep_int_in, CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE);
}
#endif

//--------------------------------------------------------------------+
// Isochronous endpoint API
//--------------------------------------------------------------------+
#if CFG_TUD_VENDOR_EP_ISO_OUT
bool tud_vendor_n_iso_read_xfer(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return vendord_ep_read_xfer(p_itf, p_itf->ep_iso_out, _vendord_iso_epbuf[idx].iso_out, p_itf->iso_rx_xfer_len);
}
#endif

#if CFG_TUD_VENDOR_EP_ISO_IN
uint32_t tud_vendor_n_iso_write(uint8_t idx, const void *buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return vendord_ep_write(p_itf, p_itf->ep_iso_in, _vendord_iso_epbuf[idx].iso_in, CFG_TUD_VENDOR_EP_ISO_IN_BUFSIZE, buffer, bufsize);
}

uint32_t tud_vendor_n_iso_write_available(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_itf = &_vendord_itf[idx];
  return vendord_ep_write_available(p_itf, p_itf->ep_iso_in, CFG_TUD_VENDOR_EP_ISO_IN_BUFSIZE);
}
#endif

#if CFG_TUD_VENDOR_ALT_SETTINGS
uint8_t tud_vendor_n_alt(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUD_VENDOR, 0);
  return _vendord_itf[idx].cur_alt;
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
    tu_edpt_stream_init(&p_itf->rx_stream, false, false, false, rx_ff_buf, CFG_TUD_VENDOR_RX_BUFSIZE, epout_buf);

    uint8_t *tx_ff_buf = p_itf->tx_ff_buf;
    tu_edpt_stream_init(&p_itf->tx_stream, false, true, false, tx_ff_buf, CFG_TUD_VENDOR_TX_BUFSIZE, epin_buf);
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
      // find unused interface slot
  #if CFG_TUD_VENDOR_ALT_SETTINGS
      // an opened interface parked in an altsetting without endpoints (the mandatory empty alt 0)
      // has all ep fields 0, so the endpoint fields cannot distinguish free from open: use p_itf_desc
      if (p_vendor->p_itf_desc == NULL) {
        return idx;
      }
  #elif CFG_TUD_VENDOR_TXRX_BUFFERED
      // A slot is free only if none of its endpoints are assigned; bulk may be absent
      // (an interrupt-only vendor interface), so check the interrupt endpoints too.
      if (p_vendor->rx_stream.ep_addr == 0 && p_vendor->tx_stream.ep_addr == 0
      #if CFG_TUD_VENDOR_EP_INT_OUT
          && p_vendor->ep_int_out == 0
      #endif
      #if CFG_TUD_VENDOR_EP_INT_IN
          && p_vendor->ep_int_in == 0
      #endif
      ) {
        return idx;
      }
  #else
      // A slot is free only if none of its endpoints are assigned. Bulk may be absent (an
      // interrupt-only vendor interface), so the interrupt endpoints must be checked too.
      if (p_vendor->ep_out == 0 && p_vendor->ep_in == 0
      #if CFG_TUD_VENDOR_EP_INT_OUT
          && p_vendor->ep_int_out == 0
      #endif
      #if CFG_TUD_VENDOR_EP_INT_IN
          && p_vendor->ep_int_in == 0
      #endif
      ) {
        return idx;
      }
  #endif
    } else {
  #if CFG_TUD_VENDOR_EP_INT_OUT
      if (ep_addr == p_vendor->ep_int_out) {
        return idx;
      }
  #endif
  #if CFG_TUD_VENDOR_EP_INT_IN
      if (ep_addr == p_vendor->ep_int_in) {
        return idx;
      }
  #endif
  #if CFG_TUD_VENDOR_EP_ISO_OUT
      if (ep_addr == p_vendor->ep_iso_out) {
        return idx;
      }
  #endif
  #if CFG_TUD_VENDOR_EP_ISO_IN
      if (ep_addr == p_vendor->ep_iso_in) {
        return idx;
      }
  #endif
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

#if CFG_TUD_VENDOR_ALT_SETTINGS

// Reserve an isochronous endpoint at open time. Ports with a dedicated iso allocator
// (TUP_DCD_EDPT_ISO_ALLOC) reserve the FIFO here and (re)activate on altsetting selection;
// ports with dcd_edpt_close instead open it once here (open == allocate + activate).
static inline bool vendord_iso_ep_alloc(uint8_t rhport, const tusb_desc_endpoint_t* desc_ep) {
  #ifdef TUP_DCD_EDPT_ISO_ALLOC
  return usbd_edpt_iso_alloc(rhport, desc_ep->bEndpointAddress, tu_edpt_packet_size(desc_ep));
  #else
  return usbd_edpt_open(rhport, desc_ep);
  #endif
}

// Deactivate a de-selected altsetting's isochronous endpoint: abort any in-flight
// transfer and release its usbd claim so a stale completion cannot fire into a
// no-longer-tracked endpoint (iso cannot be stalled like bulk/interrupt below). With the
// iso-alloc API, (re)activation with the endpoint's descriptor is the abort/scrub
// primitive; without it, close does (the next selection re-opens).
static inline void vendord_iso_ep_deactivate(uint8_t rhport, const tusb_desc_endpoint_t* desc_ep) {
  if (desc_ep != NULL) {
  #ifdef TUP_DCD_EDPT_ISO_ALLOC
    usbd_edpt_iso_activate(rhport, desc_ep);
  #else
    usbd_edpt_close(rhport, desc_ep->bEndpointAddress);
  #endif
  }
}

// (Re)activate an isochronous endpoint on altsetting selection.
static inline bool vendord_iso_ep_activate(uint8_t rhport, const tusb_desc_endpoint_t* desc_ep) {
  #ifdef TUP_DCD_EDPT_ISO_ALLOC
  return usbd_edpt_iso_activate(rhport, desc_ep); // resets ep_status, aborting any stale transfer
  #else
  // No iso alloc/activate API: close (which zeros ep_status and frees a stale claim left by a
  // prior selection) then re-open, so a re-selected altsetting starts from a clean state.
  usbd_edpt_close(rhport, desc_ep->bEndpointAddress);
  return usbd_edpt_open(rhport, desc_ep);
  #endif
}

// Abort any in-flight transfer on a tracked endpoint and release its usbd claim; no-op for an
// unset (0) address. stall disables the endpoint in the dcd, clear-stall resets it to DATA0.
static inline void vendord_abort_ep(uint8_t rhport, uint8_t ep_addr) {
  if (ep_addr) {
    usbd_edpt_stall(rhport, ep_addr);
    usbd_edpt_clear_stall(rhport, ep_addr);
  }
}

// Select an altsetting. Endpoints were hardware-opened once at vendord_open (dcds like
// dwc2 allocate FIFO linearly and cannot close/re-open endpoints dynamically): switching
// only re-targets the API to the selected altsetting's endpoints. Bulk/interrupt
// endpoints get a stall/clear-stall cycle, which portably aborts any in-flight transfer
// (stall disables the endpoint in the dcd) and resets the data toggle to DATA0 as
// SET_INTERFACE requires. Isochronous endpoints are (re)activated, which does the same.
// Single pass: the current altsetting's endpoints are dropped only once the target altsetting
// is confirmed present, so a SET_INTERFACE to an unknown alt leaves the interface intact.
static bool vendord_set_alt(uint8_t rhport, uint8_t idx, uint8_t alt) {
  vendord_interface_t *p_vendor = &_vendord_itf[idx];
  const uint8_t* p_desc = p_vendor->p_itf_desc;
  const uint8_t* desc_end = p_desc + p_vendor->itf_desc_len;
  bool in_target_alt = false;
  bool alt_found = false;

  while (tu_desc_in_bounds(p_desc, desc_end)) {
    const uint8_t desc_type = tu_desc_type(p_desc);
    if (desc_type == TUSB_DESC_INTERFACE) {
      in_target_alt = (((const tusb_desc_interface_t*)p_desc)->bAlternateSetting == alt);
      if (in_target_alt && !alt_found) {
        alt_found = true;
        // target altsetting confirmed present: abort then drop the previous altsetting's endpoints,
        // so an endpoint absent from the target altsetting can't stay armed and keep its usbd
        // claim in the dcd. (Endpoints the target altsetting reuses are reset again below;
        // a double reset is harmless.)
        vendord_abort_ep(rhport, p_vendor->ep_in);
        vendord_abort_ep(rhport, p_vendor->ep_out);
  #if CFG_TUD_VENDOR_EP_INT_OUT
        vendord_abort_ep(rhport, p_vendor->ep_int_out);
  #endif
  #if CFG_TUD_VENDOR_EP_INT_IN
        vendord_abort_ep(rhport, p_vendor->ep_int_in);
  #endif
        p_vendor->ep_in = 0;
        p_vendor->ep_out = 0;
  #if CFG_TUD_VENDOR_EP_INT_OUT
        p_vendor->ep_int_out = 0;
  #endif
  #if CFG_TUD_VENDOR_EP_INT_IN
        p_vendor->ep_int_in = 0;
  #endif
  #if CFG_TUD_VENDOR_EP_ISO_OUT
        vendord_iso_ep_deactivate(rhport, p_vendor->iso_out_desc);
        p_vendor->iso_out_desc = NULL;
        p_vendor->ep_iso_out = 0;
  #endif
  #if CFG_TUD_VENDOR_EP_ISO_IN
        vendord_iso_ep_deactivate(rhport, p_vendor->iso_in_desc);
        p_vendor->iso_in_desc = NULL;
        p_vendor->ep_iso_in = 0;
  #endif
      }
    } else if (in_target_alt && desc_type == TUSB_DESC_ENDPOINT) {
      const tusb_desc_endpoint_t* desc_ep = (const tusb_desc_endpoint_t*) p_desc;
      const uint8_t ep_addr = desc_ep->bEndpointAddress;
      const bool is_in = tu_edpt_dir(ep_addr) == TUSB_DIR_IN;
      (void) is_in;

      switch (desc_ep->bmAttributes.xfer) {
        case TUSB_XFER_BULK:
          // abort in-flight transfer + reset data toggle
          usbd_edpt_stall(rhport, ep_addr);
          usbd_edpt_clear_stall(rhport, ep_addr);
          if (is_in) {
            p_vendor->ep_in = ep_addr;
          } else {
            p_vendor->ep_out = ep_addr;
            p_vendor->rx_xfer_len =
              CFG_TUD_VENDOR_RX_NEED_ZLP ? CFG_TUD_VENDOR_RX_EPSIZE : tu_edpt_packet_size(desc_ep);
  #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
            TU_ASSERT(usbd_edpt_xfer(rhport, p_vendor->ep_out, _vendord_epbuf[idx].epout,
                                     p_vendor->rx_xfer_len, false));
  #endif
          }
          break;

  #if CFG_TUD_VENDOR_EP_INT_IN || CFG_TUD_VENDOR_EP_INT_OUT
        case TUSB_XFER_INTERRUPT:
          // stall/clear only for the enabled direction (an endpoint of a disabled
          // direction was never opened, so must not be poked in the dcd)
    #if CFG_TUD_VENDOR_EP_INT_IN
          if (is_in) {
            usbd_edpt_stall(rhport, ep_addr);
            usbd_edpt_clear_stall(rhport, ep_addr);
            p_vendor->ep_int_in = ep_addr;
          }
    #endif
    #if CFG_TUD_VENDOR_EP_INT_OUT
          if (!is_in) {
            usbd_edpt_stall(rhport, ep_addr);
            usbd_edpt_clear_stall(rhport, ep_addr);
            p_vendor->ep_int_out = ep_addr;
            p_vendor->int_rx_xfer_len = tu_edpt_packet_size(desc_ep);
          }
    #endif
          break;
  #endif

  #if CFG_TUD_VENDOR_EP_ISO_IN || CFG_TUD_VENDOR_EP_ISO_OUT
        case TUSB_XFER_ISOCHRONOUS:
    #if CFG_TUD_VENDOR_EP_ISO_IN
          if (is_in) {
            TU_ASSERT(vendord_iso_ep_activate(rhport, desc_ep));
            p_vendor->ep_iso_in = ep_addr;
            p_vendor->iso_in_desc = desc_ep; // points into p_itf_desc (static app descriptor)
          }
    #endif
    #if CFG_TUD_VENDOR_EP_ISO_OUT
          if (!is_in) {
            TU_ASSERT(vendord_iso_ep_activate(rhport, desc_ep));
            p_vendor->ep_iso_out = ep_addr;
            p_vendor->iso_out_desc = desc_ep; // points into p_itf_desc (static app descriptor)
            p_vendor->iso_rx_xfer_len = tu_edpt_packet_size(desc_ep);
          }
    #endif
          break;
  #endif

        default:
          break; // unsupported endpoint type / direction gate disabled: ignore
      }
    }
    p_desc = tu_desc_next(p_desc);
  }

  TU_VERIFY(alt_found); // unknown alt: endpoints were never cleared, current altsetting intact
  p_vendor->cur_alt = alt;
  return true;
}

uint16_t vendord_open(uint8_t rhport, const tusb_desc_interface_t *desc_itf, uint16_t max_len) {
  TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == desc_itf->bInterfaceClass, 0);
  const uint8_t* desc_end = (const uint8_t*)desc_itf + max_len;

  const uint8_t idx = find_vendor_itf(0);
  TU_ASSERT(idx < CFG_TUD_VENDOR, 0);
  vendord_interface_t *p_vendor = &_vendord_itf[idx];
  p_vendor->rhport     = rhport;
  p_vendor->itf_num    = desc_itf->bInterfaceNumber;
  // p_itf_desc is assigned only after the parse succeeds: find_vendor_itf() treats a non-NULL
  // p_itf_desc as an occupied slot, so setting it before a mid-parse TU_ASSERT could fail would
  // leak the slot (a retry would find no free interface until the next bus reset).

  // Consume every altsetting of this interface and hardware-open each endpoint exactly once
  // (bulk/interrupt via usbd_edpt_open, isochronous FIFO-allocated; iso activation and the
  // toggle reset happen on altsetting selection). An endpoint address that recurs in another
  // altsetting must carry an identical configuration, since it is opened only on first sight;
  // reconfiguring the same address per-alt is not supported and is rejected here.
  uint8_t seen_type[CFG_TUD_ENDPPOINT_MAX][2];
  uint16_t seen_mps[CFG_TUD_ENDPPOINT_MAX][2];
  tu_memclr(seen_type, sizeof(seen_type)); // 0 == TUSB_XFER_CONTROL, never used as a data ep here
  const uint8_t* p_desc = tu_desc_next(desc_itf);
  while (tu_desc_in_bounds(p_desc, desc_end)) {
    const uint8_t desc_type = tu_desc_type(p_desc);
    if (desc_type == TUSB_DESC_INTERFACE_ASSOCIATION) {
      break;
    }
    if (desc_type == TUSB_DESC_INTERFACE) {
      if (((const tusb_desc_interface_t*)p_desc)->bInterfaceNumber != p_vendor->itf_num) {
        break; // next interface
      }
    } else if (desc_type == TUSB_DESC_ENDPOINT) {
      const tusb_desc_endpoint_t* desc_ep = (const tusb_desc_endpoint_t*) p_desc;
      const uint8_t epnum = tu_edpt_number(desc_ep->bEndpointAddress);
      const uint8_t dir = tu_edpt_dir(desc_ep->bEndpointAddress);
      const uint8_t xfer = desc_ep->bmAttributes.xfer;
      const uint16_t mps = tu_edpt_packet_size(desc_ep);
      TU_ASSERT(epnum < CFG_TUD_ENDPPOINT_MAX, 0);

      if (seen_type[epnum][dir] != 0) {
        // reused address in a later altsetting: must be an exact match (opened only once)
        TU_ASSERT(seen_type[epnum][dir] == xfer && seen_mps[epnum][dir] == mps, 0);
      } else {
        switch (xfer) {
          case TUSB_XFER_BULK:
            TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
            break;

  #if CFG_TUD_VENDOR_EP_INT_IN || CFG_TUD_VENDOR_EP_INT_OUT
          case TUSB_XFER_INTERRUPT:
    #if CFG_TUD_VENDOR_EP_INT_IN
            if (dir == TUSB_DIR_IN) {
              TU_ASSERT(mps <= CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE, 0);
              TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
            }
    #endif
    #if CFG_TUD_VENDOR_EP_INT_OUT
            if (dir == TUSB_DIR_OUT) {
              TU_ASSERT(mps <= CFG_TUD_VENDOR_EP_INT_OUT_BUFSIZE, 0);
              TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
            }
    #endif
            break;
  #endif

  #if CFG_TUD_VENDOR_EP_ISO_IN || CFG_TUD_VENDOR_EP_ISO_OUT
          case TUSB_XFER_ISOCHRONOUS:
    #if CFG_TUD_VENDOR_EP_ISO_IN
            if (dir == TUSB_DIR_IN) {
              TU_ASSERT(mps <= CFG_TUD_VENDOR_EP_ISO_IN_BUFSIZE, 0);
              TU_ASSERT(vendord_iso_ep_alloc(rhport, desc_ep), 0);
            }
    #endif
    #if CFG_TUD_VENDOR_EP_ISO_OUT
            if (dir == TUSB_DIR_OUT) {
              TU_ASSERT(mps <= CFG_TUD_VENDOR_EP_ISO_OUT_BUFSIZE, 0);
              TU_ASSERT(vendord_iso_ep_alloc(rhport, desc_ep), 0);
            }
    #endif
            break;
  #endif

          default:
            break; // unsupported endpoint type / direction gate disabled: ignore
        }
        seen_type[epnum][dir] = xfer;
        seen_mps[epnum][dir] = mps;
      }
    }
    p_desc = tu_desc_next(p_desc);
  }
  // parse succeeded: commit the descriptor pointer (marks the slot occupied) before selecting alt 0
  p_vendor->p_itf_desc   = (const uint8_t*) desc_itf;
  p_vendor->itf_desc_len = (uint16_t)((uintptr_t)p_desc - (uintptr_t)desc_itf);

  // default altsetting active until the host selects another
  TU_ASSERT(vendord_set_alt(rhport, idx, 0), 0);
  return p_vendor->itf_desc_len;
}

#else // !CFG_TUD_VENDOR_ALT_SETTINGS

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

  #if CFG_TUD_VENDOR_EP_INT_OUT || CFG_TUD_VENDOR_EP_INT_IN
      if (desc_ep->bmAttributes.xfer == TUSB_XFER_INTERRUPT) {
        const bool is_int_in = tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN;
        (void) is_int_in;
    #if CFG_TUD_VENDOR_EP_INT_IN
        if (is_int_in) {
          TU_ASSERT(tu_edpt_packet_size(desc_ep) <= CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE, 0);
          TU_ASSERT(usbd_edpt_open(rhport, desc_ep));
          p_vendor->ep_int_in = desc_ep->bEndpointAddress;
        }
    #endif
    #if CFG_TUD_VENDOR_EP_INT_OUT
        if (!is_int_in) {
          TU_ASSERT(tu_edpt_packet_size(desc_ep) <= CFG_TUD_VENDOR_EP_INT_OUT_BUFSIZE, 0);
          TU_ASSERT(usbd_edpt_open(rhport, desc_ep));
          p_vendor->ep_int_out = desc_ep->bEndpointAddress;
          p_vendor->int_rx_xfer_len = tu_edpt_packet_size(desc_ep);
        }
    #endif
        p_desc = tu_desc_next(p_desc);
        continue;
      }
  #endif

      TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

      uint16_t rx_xfer_len = CFG_TUD_VENDOR_RX_NEED_ZLP ? CFG_TUD_VENDOR_RX_EPSIZE : tu_edpt_packet_size(desc_ep);

  #if CFG_TUD_VENDOR_TXRX_BUFFERED
      // open endpoint stream
      if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
        tu_edpt_stream_t *tx_stream = &p_vendor->tx_stream;
        tu_edpt_stream_open(tx_stream, rhport, desc_ep, CFG_TUD_VENDOR_TX_EPSIZE);
        tu_edpt_stream_write_xfer(tx_stream); // flush pending data
      } else {
        tu_edpt_stream_t *rx_stream = &p_vendor->rx_stream;
        tu_edpt_stream_open(rx_stream, rhport, desc_ep, rx_xfer_len);
    #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
        TU_ASSERT(tu_edpt_stream_read_xfer(rx_stream) > 0, 0); // prepare for incoming data
    #endif
      }
  #else
      p_vendor->rx_xfer_len = rx_xfer_len;
      // Non-FIFO mode: store endpoint info
      if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
        p_vendor->ep_in     = desc_ep->bEndpointAddress;
      } else {
        p_vendor->ep_out     = desc_ep->bEndpointAddress;
    #if CFG_TUD_VENDOR_RX_MANUAL_XFER == 0
        // Prepare for incoming data
        TU_ASSERT(usbd_edpt_xfer(rhport, p_vendor->ep_out, _vendord_epbuf[idx].epout, rx_xfer_len, false), 0);
    #endif
      }
  #endif
    }

    p_desc = tu_desc_next(p_desc);
  }

  return (uint16_t)((uintptr_t)p_desc - (uintptr_t)desc_itf);
}

#endif // CFG_TUD_VENDOR_ALT_SETTINGS

// Handle interface standard requests (GET/SET_INTERFACE when altsettings are enabled),
// delegate everything else to the application callback as before.
bool vendord_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
#if CFG_TUD_VENDOR_ALT_SETTINGS
  if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE) {
    const uint8_t itf_num = tu_u16_low(request->wIndex);
    uint8_t idx;
    for (idx = 0; idx < CFG_TUD_VENDOR; idx++) {
      if (_vendord_itf[idx].itf_num == itf_num && _vendord_itf[idx].p_itf_desc != NULL) {
        break;
      }
    }
    if (idx < CFG_TUD_VENDOR) {
      if (request->bRequest == TUSB_REQ_SET_INTERFACE) {
        if (stage == CONTROL_STAGE_SETUP) {
          TU_VERIFY(vendord_set_alt(rhport, idx, tu_u16_low(request->wValue)));
          return tud_control_status(rhport, request);
        }
        return true;
      } else if (request->bRequest == TUSB_REQ_GET_INTERFACE) {
        if (stage == CONTROL_STAGE_SETUP) {
          return tud_control_xfer(rhport, request, &_vendord_itf[idx].cur_alt, 1);
        }
        return true;
      }
    }
  }
#endif
  return tud_vendor_control_xfer_cb(rhport, stage, request);
}

bool vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void)rhport;
  (void)result;
  const uint8_t idx = find_vendor_itf(ep_addr);
  TU_VERIFY(idx < CFG_TUD_VENDOR);
  vendord_interface_t *p_vendor = &_vendord_itf[idx];

#if CFG_TUD_VENDOR_EP_INT_OUT
  if (ep_addr == p_vendor->ep_int_out) {
    // not re-armed automatically: application calls tud_vendor_n_int_read_xfer()
    tud_vendor_int_rx_cb(idx, _vendord_int_epbuf[idx].int_out, xferred_bytes);
    return true;
  }
#endif
#if CFG_TUD_VENDOR_EP_INT_IN
  if (ep_addr == p_vendor->ep_int_in) {
    tud_vendor_int_tx_cb(idx, xferred_bytes);
    return true;
  }
#endif
#if CFG_TUD_VENDOR_EP_ISO_OUT
  if (ep_addr == p_vendor->ep_iso_out) {
    // not re-armed automatically: application calls tud_vendor_n_iso_read_xfer()
    tud_vendor_iso_rx_cb(idx, _vendord_iso_epbuf[idx].iso_out, xferred_bytes);
    return true;
  }
#endif
#if CFG_TUD_VENDOR_EP_ISO_IN
  if (ep_addr == p_vendor->ep_iso_in) {
    tud_vendor_iso_tx_cb(idx, xferred_bytes);
    return true;
  }
#endif

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
    usbd_edpt_xfer(rhport, p_vendor->ep_out, _vendord_epbuf[idx].epout, p_vendor->rx_xfer_len, false);
    #endif
  } else if (ep_addr == p_vendor->ep_in) {
    // Send complete
    tud_vendor_tx_cb(idx, (uint16_t)xferred_bytes);
  }
  #endif

  return true;
}

#endif
