/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Andrew Leech
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
 */

/*
 * Shared types, state and helpers for the usbipd implementation.
 * Private to the example - not part of the public usbip_server.h API.
 *
 * The implementation is split across:
 *   usbip_server.c    connection state, TCP callbacks, pre-import
 *                     framing, listener init.
 *   usbip_devices.c   cached device table, mount/umount hooks,
 *                     DEVLIST and IMPORT op handlers.
 *   usbip_inflight.c  inflight URB pool, completion -> RET_SUBMIT.
 *   usbip_submit.c    CMD_SUBMIT dispatch, per-EP queue, CMD_UNLINK.
 */

#ifndef USBIP_INTERNAL_H_
#define USBIP_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "tusb.h"
#include "lwip/tcp.h"

#include "usbip_protocol.h"

//--------------------------------------------------------------------+
// Constants
//--------------------------------------------------------------------+

#define MAX_DEV          (CFG_TUH_DEVICE_MAX + 1)
#define MAX_CONFIG_DESC  512
// Inflight slot pool. cdc-acm queues 16 read-ahead URBs on its bulk-IN;
// 32 slots covers the read queue plus a handful of outstanding controls
// and the IF1 interrupt notification URB. The pool is GLOBAL across all
// devices on the bridge; under hub topology with multiple cdc-acm
// devices, the pool can be exhausted - intended for single-device use.
// Buffer: 1536 bytes per slot covers FS bulk wMaxPacketSize chains plus
// the largest control descriptor we forward (config + interfaces).
#define MAX_INFLIGHT     32
#define MAX_URB_BUF      1536

//--------------------------------------------------------------------+
// Endian helpers
//--------------------------------------------------------------------+

static inline uint16_t be16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
static inline uint32_t be32(uint32_t v) {
  return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
         ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

//--------------------------------------------------------------------+
// Cached device table
//--------------------------------------------------------------------+

typedef struct {
  bool                 mounted;
  uint8_t              daddr;
  tusb_desc_device_t   dev_desc;
  uint8_t              speed;
  uint8_t              num_interfaces;
  char                 busid[USBIP_BUSID_SIZE];
  // Captured config descriptor (sniffed from GET_DESCRIPTOR(CONFIG) replies).
  uint8_t              config_desc[MAX_CONFIG_DESC];
  uint16_t             config_desc_len;
  // Per-EP open state: bit (ep_addr & 0x8F) packed; 0..15 IN, 16..31 OUT
  uint32_t             ep_open;
} cached_dev_t;

extern cached_dev_t s_devs[MAX_DEV];

static inline uint32_t ep_open_bit(uint8_t ep_addr) {
  uint8_t n = ep_addr & 0x0F;
  return (ep_addr & 0x80) ? (1u << n) : (1u << (n + 16));
}

const tusb_desc_endpoint_t *find_ep_desc(const cached_dev_t *d, uint8_t ep_addr);

//--------------------------------------------------------------------+
// Inflight URB pool
//--------------------------------------------------------------------+

typedef struct {
  bool       in_use;
  // queued: slot is allocated and ready to submit but the target EP was
  // already claimed at submit time (TinyUSB enforces 1-URB-per-EP). The
  // pump in on_xfer_complete drains queued slots when the EP frees.
  bool       queued;
  uint64_t   queue_seq;     // FIFO order within the queue
  uint32_t   seqnum;
  uint32_t   devid;
  uint32_t   direction;     // USBIP_DIR_IN / OUT
  uint8_t    daddr;
  uint8_t    ep;            // raw EP number (0..15)
  uint8_t    ep_addr;       // includes direction bit
  uint8_t    is_control;
  // Buffer used for both setup-attached IN data and bulk/int data.
  uint8_t    buf[MAX_URB_BUF];
  uint32_t   transfer_buffer_length;
  // Setup packet kept here so tuh_xfer_t.setup remains valid until
  // completion (tuh_xfer holds the pointer).
  tusb_control_request_t setup;
  tuh_xfer_t xfer;
} inflight_t;

extern inflight_t s_inflight[MAX_INFLIGHT];

inflight_t *inflight_alloc(void);
inflight_t *inflight_find_seqnum(uint32_t seqnum);
void on_xfer_complete(tuh_xfer_t *xfer);

//--------------------------------------------------------------------+
// Per-connection state
//--------------------------------------------------------------------+

typedef enum {
  ST_WAIT_OP = 0,
  ST_WAIT_IMPORT_BUSID,
  ST_IMPORTED,
  ST_CLOSING,
} conn_state_t;

typedef enum {
  URB_RX_HEADER = 0,        // expecting next 48-byte usbip_header_t
  URB_RX_OUT_DATA,          // header parsed, expecting OUT payload bytes
} urb_rx_state_t;

typedef struct {
  struct tcp_pcb *pcb;
  conn_state_t    state;

  // Reassembly: pre-import op_common parsing uses rxbuf inline. Once
  // imported, we use a larger reassembly arena (urb_buf) for headers
  // + OUT payloads.
  uint8_t  rxbuf[64];
  size_t   rxlen;
  size_t   need;
  uint8_t  imported_daddr;

  urb_rx_state_t  urb_state;
  uint8_t         urb_hdr_buf[sizeof(usbip_header_t)];
  size_t          urb_hdr_have;
  // Pending OUT URB: header is parsed, we are reading OUT data into
  // an inflight slot's buf[].
  inflight_t     *urb_out_inflight;
  size_t          urb_out_have;
  size_t          urb_out_need;
} conn_t;

extern conn_t s_conn;

//--------------------------------------------------------------------+
// Wire helpers (defined in usbip_server.c)
//--------------------------------------------------------------------+

err_t conn_send(conn_t *c, const void *buf, size_t len);
err_t send_op_common(conn_t *c, uint16_t code, uint32_t status);
void  fill_dev_desc(usbip_device_desc_t *out, const cached_dev_t *d);

//--------------------------------------------------------------------+
// Op handlers
//--------------------------------------------------------------------+

err_t handle_devlist(conn_t *c);
err_t handle_import(conn_t *c, const char *busid);
err_t submit_urb(conn_t *c, const usbip_header_t *hdr_be);
err_t submit_or_queue(conn_t *c, inflight_t *u);
bool  drain_queue_for_ep(uint8_t daddr, uint8_t ep_addr);
err_t handle_unlink(conn_t *c, const usbip_header_t *hdr_be);

#endif // USBIP_INTERNAL_H_
