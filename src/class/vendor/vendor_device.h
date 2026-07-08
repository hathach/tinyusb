/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_VENDOR_DEVICE_H_
#define TUSB_VENDOR_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common/tusb_common.h"

//--------------------------------------------------------------------+
// Configuration
//--------------------------------------------------------------------+
#ifndef CFG_TUD_VENDOR_RX_EPSIZE
  #ifdef CFG_TUD_VENDOR_EPSIZE
    #define CFG_TUD_VENDOR_RX_EPSIZE CFG_TUD_VENDOR_EPSIZE
  #else
    #define CFG_TUD_VENDOR_RX_EPSIZE TUD_EPSIZE_BULK_MAX
  #endif
#endif

#ifndef CFG_TUD_VENDOR_TX_EPSIZE
  #ifdef CFG_TUD_VENDOR_EPSIZE
    #define CFG_TUD_VENDOR_TX_EPSIZE CFG_TUD_VENDOR_EPSIZE
  #else
    #define CFG_TUD_VENDOR_TX_EPSIZE TUD_EPSIZE_BULK_MAX
  #endif
#endif

// RX FIFO can be disabled by setting this value to 0
#ifndef CFG_TUD_VENDOR_RX_BUFSIZE
  #define CFG_TUD_VENDOR_RX_BUFSIZE TUD_EPSIZE_BULK_MAX
#endif

// TX FIFO can be disabled by setting this value to 0
#ifndef CFG_TUD_VENDOR_TX_BUFSIZE
  #define CFG_TUD_VENDOR_TX_BUFSIZE TUD_EPSIZE_BULK_MAX
#endif

// Vendor is buffered (FIFO mode) if both TX and RX buffers are configured
// If either is 0, vendor operates in non-buffered (direct transfer) mode
#ifndef CFG_TUD_VENDOR_TXRX_BUFFERED
  #define CFG_TUD_VENDOR_TXRX_BUFFERED ((CFG_TUD_VENDOR_RX_BUFSIZE > 0) && (CFG_TUD_VENDOR_TX_BUFSIZE > 0))
#endif

// Application will manually schedule RX transfer. This can be useful when using with non-fifo (buffered) mode
// i.e. CFG_TUD_VENDOR_TXRX_BUFFERED = 0
#ifndef CFG_TUD_VENDOR_RX_MANUAL_XFER
  #define CFG_TUD_VENDOR_RX_MANUAL_XFER 0
#endif

// Enable multi-packet RX transfer with ZLP termination for better throughput. Requires host support for ZLP.
#ifndef CFG_TUD_VENDOR_RX_NEED_ZLP
  #define CFG_TUD_VENDOR_RX_NEED_ZLP 0
#endif

// Enable support for an optional interrupt OUT / interrupt IN endpoint in the vendor
// interface, each direction gated separately. Interrupt endpoints are non-buffered:
// OUT is armed manually one packet at a time with tud_vendor_n_int_read_xfer() (data
// delivered via tud_vendor_int_rx_cb), IN is a direct transfer via tud_vendor_n_int_write().
#ifndef CFG_TUD_VENDOR_EP_INT_OUT
  #define CFG_TUD_VENDOR_EP_INT_OUT 0
#endif

#ifndef CFG_TUD_VENDOR_EP_INT_IN
  #define CFG_TUD_VENDOR_EP_INT_IN 0
#endif

// Buffer sizes for interrupt endpoint transfers, must be >= the endpoint max packet size
#ifndef CFG_TUD_VENDOR_EP_INT_OUT_BUFSIZE
  #define CFG_TUD_VENDOR_EP_INT_OUT_BUFSIZE 64
#endif

#ifndef CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE
  #define CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE 64
#endif

// Enable support for an optional isochronous OUT / IN endpoint, each direction gated
// separately, with the same non-buffered API shape as the interrupt pair. Isochronous
// endpoints must not claim bandwidth in the default altsetting (USB 2.0 5.6.3): place
// them in a non-zero altsetting and enable CFG_TUD_VENDOR_ALT_SETTINGS.
#ifndef CFG_TUD_VENDOR_EP_ISO_OUT
  #define CFG_TUD_VENDOR_EP_ISO_OUT 0
#endif

#ifndef CFG_TUD_VENDOR_EP_ISO_IN
  #define CFG_TUD_VENDOR_EP_ISO_IN 0
#endif

// Buffer sizes for isochronous endpoint transfers, must be >= the endpoint max packet size
#ifndef CFG_TUD_VENDOR_EP_ISO_OUT_BUFSIZE
  #define CFG_TUD_VENDOR_EP_ISO_OUT_BUFSIZE 64
#endif

#ifndef CFG_TUD_VENDOR_EP_ISO_IN_BUFSIZE
  #define CFG_TUD_VENDOR_EP_ISO_IN_BUFSIZE 64
#endif

// Enable alternate-setting support: the vendor interface may carry multiple altsettings,
// each with its own endpoint set. GET_INTERFACE is answered and SET_INTERFACE performed by
// closing the current altsetting's endpoints and opening the requested one's (isochronous
// endpoints are FIFO-allocated at open and activated on selection). The configuration
// descriptor must stay valid while mounted (static, the usual TinyUSB pattern).
// Non-buffered mode only.
#ifndef CFG_TUD_VENDOR_ALT_SETTINGS
  #define CFG_TUD_VENDOR_ALT_SETTINGS 0
#endif

#if CFG_TUD_VENDOR_ALT_SETTINGS && CFG_TUD_VENDOR_TXRX_BUFFERED
  #error CFG_TUD_VENDOR_ALT_SETTINGS requires non-buffered mode (CFG_TUD_VENDOR_RX/TX_BUFSIZE = 0)
#endif

// An isochronous endpoint must not claim bandwidth in the default altsetting (USB 2.0 5.6.3),
// so it can only live in a non-zero altsetting, which requires alternate-setting support.
#if (CFG_TUD_VENDOR_EP_ISO_OUT || CFG_TUD_VENDOR_EP_ISO_IN) && !CFG_TUD_VENDOR_ALT_SETTINGS
  #error CFG_TUD_VENDOR_EP_ISO_OUT/IN requires CFG_TUD_VENDOR_ALT_SETTINGS
#endif

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces) i.e CFG_TUD_VENDOR > 1
//--------------------------------------------------------------------+

// Return whether the vendor interface is mounted
bool tud_vendor_n_mounted(uint8_t idx);

//------------- RX -------------//
#if CFG_TUD_VENDOR_TXRX_BUFFERED
// Return number of available bytes for reading
uint32_t tud_vendor_n_available(uint8_t idx);

// Peek a byte from RX buffer
bool tud_vendor_n_peek(uint8_t idx, uint8_t *ui8);

// Read from RX FIFO
uint32_t tud_vendor_n_read(uint8_t idx, void *buffer, uint32_t bufsize);

// Flush (clear) RX FIFO
void tud_vendor_n_read_flush(uint8_t idx);
#endif

#if CFG_TUD_VENDOR_RX_MANUAL_XFER
// Start a new RX transfer to fill the RX FIFO, return false if previous transfer is still ongoing
bool tud_vendor_n_read_xfer(uint8_t idx);
#endif

//------------- TX -------------//
// Write to TX FIFO. This can be buffered and not sent immediately unless buffered bytes >= USB endpoint size
uint32_t tud_vendor_n_write(uint8_t idx, const void *buffer, uint32_t bufsize);

// Return number of bytes available for writing in TX FIFO (or endpoint if non-buffered)
uint32_t tud_vendor_n_write_available(uint8_t idx);

#if CFG_TUD_VENDOR_TXRX_BUFFERED
// Force sending buffered data, return number of bytes sent
uint32_t tud_vendor_n_write_flush(uint8_t idx);

// Clear the transmit FIFO
bool tud_vendor_n_write_clear(uint8_t idx);
#endif

// Write a null-terminated string to TX FIFO
TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_n_write_str(uint8_t idx, const char *str) {
  return tud_vendor_n_write(idx, str, strlen(str));
}

//------------- Interrupt endpoints -------------//
#if CFG_TUD_VENDOR_EP_INT_OUT
// Arm the interrupt OUT endpoint for one packet, return false if a transfer is still ongoing.
// Received data is delivered via tud_vendor_int_rx_cb(); re-arm from the callback or by polling.
bool tud_vendor_n_int_read_xfer(uint8_t idx);
#endif

#if CFG_TUD_VENDOR_EP_INT_IN
// Send on the interrupt IN endpoint (direct transfer, up to CFG_TUD_VENDOR_EP_INT_IN_BUFSIZE
// bytes). Returns number of bytes queued, 0 if the endpoint is busy or not opened.
uint32_t tud_vendor_n_int_write(uint8_t idx, const void *buffer, uint32_t bufsize);

// Return available bytes for interrupt IN write: 0 while busy, else the buffer size
uint32_t tud_vendor_n_int_write_available(uint8_t idx);
#endif

//------------- Isochronous endpoints -------------//
#if CFG_TUD_VENDOR_EP_ISO_OUT
// Arm the isochronous OUT endpoint for one packet, return false if a transfer is still ongoing.
// Received data is delivered via tud_vendor_iso_rx_cb(); re-arm from the callback or by polling.
bool tud_vendor_n_iso_read_xfer(uint8_t idx);
#endif

#if CFG_TUD_VENDOR_EP_ISO_IN
// Send on the isochronous IN endpoint (direct transfer, up to CFG_TUD_VENDOR_EP_ISO_IN_BUFSIZE
// bytes). Returns number of bytes queued, 0 if the endpoint is busy or not opened.
uint32_t tud_vendor_n_iso_write(uint8_t idx, const void *buffer, uint32_t bufsize);

// Return available bytes for isochronous IN write: 0 while busy, else the buffer size
uint32_t tud_vendor_n_iso_write_available(uint8_t idx);
#endif

#if CFG_TUD_VENDOR_ALT_SETTINGS
// Return the currently selected alternate setting
uint8_t tud_vendor_n_alt(uint8_t idx);
#endif

// backward compatible
#define tud_vendor_n_flush(idx) tud_vendor_n_write_flush(idx)

//--------------------------------------------------------------------+
// Application API (Single Port) i.e CFG_TUD_VENDOR = 1
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_mounted(void) {
  return tud_vendor_n_mounted(0);
}

#if CFG_TUD_VENDOR_TXRX_BUFFERED
TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_available(void) {
  return tud_vendor_n_available(0);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_peek(uint8_t *ui8) {
  return tud_vendor_n_peek(0, ui8);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_read(void *buffer, uint32_t bufsize) {
  return tud_vendor_n_read(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline void tud_vendor_read_flush(void) {
  tud_vendor_n_read_flush(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_write_flush(void) {
  return tud_vendor_n_write_flush(0);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_write_clear(void) {
  return tud_vendor_n_write_clear(0);
}
#endif

#if CFG_TUD_VENDOR_RX_MANUAL_XFER
TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_read_xfer(void) {
  return tud_vendor_n_read_xfer(0);
}
#endif

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_write(const void *buffer, uint32_t bufsize) {
  return tud_vendor_n_write(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_write_str(const char *str) {
  return tud_vendor_n_write_str(0, str);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_write_available(void) {
  return tud_vendor_n_write_available(0);
}

#if CFG_TUD_VENDOR_EP_INT_OUT
TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_int_read_xfer(void) {
  return tud_vendor_n_int_read_xfer(0);
}
#endif

#if CFG_TUD_VENDOR_EP_INT_IN
TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_int_write(const void *buffer, uint32_t bufsize) {
  return tud_vendor_n_int_write(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_int_write_available(void) {
  return tud_vendor_n_int_write_available(0);
}
#endif

#if CFG_TUD_VENDOR_EP_ISO_OUT
TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_iso_read_xfer(void) {
  return tud_vendor_n_iso_read_xfer(0);
}
#endif

#if CFG_TUD_VENDOR_EP_ISO_IN
TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_iso_write(const void *buffer, uint32_t bufsize) {
  return tud_vendor_n_iso_write(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_iso_write_available(void) {
  return tud_vendor_n_iso_write_available(0);
}
#endif

#if CFG_TUD_VENDOR_ALT_SETTINGS
TU_ATTR_ALWAYS_INLINE static inline uint8_t tud_vendor_alt(void) {
  return tud_vendor_n_alt(0);
}
#endif

// backward compatible
#define tud_vendor_flush() tud_vendor_write_flush()

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when received new data.
// - CFG_TUD_VENDOR_TXRX_BUFFERED = 1: buffer and bufsize must not be used (both NULL,0) since data is in RX FIFO
// - CFG_TUD_VENDOR_TXRX_BUFFERED = 0: Buffer and bufsize are valid
void tud_vendor_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize);

// Invoked when tx transfer is finished
void tud_vendor_tx_cb(uint8_t idx, uint32_t sent_bytes);

#if CFG_TUD_VENDOR_EP_INT_OUT
// Invoked when data is received on the interrupt OUT endpoint. The endpoint is not
// re-armed automatically: call tud_vendor_n_int_read_xfer() to receive more.
void tud_vendor_int_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize);
#endif

#if CFG_TUD_VENDOR_EP_INT_IN
// Invoked when an interrupt IN transfer is finished
void tud_vendor_int_tx_cb(uint8_t idx, uint32_t sent_bytes);
#endif

#if CFG_TUD_VENDOR_EP_ISO_OUT
// Invoked when data is received on the isochronous OUT endpoint. The endpoint is not
// re-armed automatically: call tud_vendor_n_iso_read_xfer() to receive more.
void tud_vendor_iso_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize);
#endif

#if CFG_TUD_VENDOR_EP_ISO_IN
// Invoked when an isochronous IN transfer is finished (result may be a missed frame:
// the data was not necessarily taken by the host, re-arm regardless)
void tud_vendor_iso_tx_cb(uint8_t idx, uint32_t sent_bytes);
#endif

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     vendord_init(void);
bool     vendord_deinit(void);
void     vendord_reset(uint8_t rhport);
uint16_t vendord_open(uint8_t rhport, const tusb_desc_interface_t *idx_desc, uint16_t max_len);
bool     vendord_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request);
bool     vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif /* TUSB_VENDOR_DEVICE_H_ */
