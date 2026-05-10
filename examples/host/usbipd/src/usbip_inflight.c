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
 * Inflight URB pool and the completion handler that turns a TinyUSB
 * giveback into a RET_SUBMIT on the wire. The completion path also
 * sniffs GET_DESCRIPTOR(CONFIG) replies into the cached_dev_t so the
 * submit path can open bulk/interrupt endpoints lazily.
 */

#include <stdio.h>
#include <string.h>

#include "usbip_internal.h"

inflight_t s_inflight[MAX_INFLIGHT];

inflight_t *inflight_alloc(void) {
  for (int i = 0; i < MAX_INFLIGHT; i++) {
    if (!s_inflight[i].in_use) {
      memset(&s_inflight[i], 0, sizeof(inflight_t));
      s_inflight[i].in_use = true;
      return &s_inflight[i];
    }
  }
  return NULL;
}

inflight_t *inflight_find_seqnum(uint32_t seqnum) {
  for (int i = 0; i < MAX_INFLIGHT; i++) {
    if (s_inflight[i].in_use && s_inflight[i].seqnum == seqnum) {
      return &s_inflight[i];
    }
  }
  return NULL;
}

// Sniff GET_DESCRIPTOR(CONFIGURATION) replies to populate the per-
// device config_desc cache, used later for tuh_edpt_open.
static void maybe_capture_config_desc(inflight_t *u) {
  if (!u->is_control || u->direction != USBIP_DIR_IN) return;
  uint8_t bm = u->setup.bmRequestType;
  uint8_t br = u->setup.bRequest;
  uint8_t hi = (uint8_t)(u->setup.wValue >> 8);
  if (bm != 0x80 || br != 6 || hi != TUSB_DESC_CONFIGURATION) return;

  cached_dev_t *d = &s_devs[u->daddr];
  uint32_t actual = u->xfer.actual_len;
  if (actual > MAX_CONFIG_DESC) actual = MAX_CONFIG_DESC;
  if (actual > d->config_desc_len) {
    memcpy(d->config_desc, u->buf, actual);
    d->config_desc_len = (uint16_t)actual;
    // Pull bNumInterfaces out of the configuration descriptor header
    // so DEVLIST replies can advertise the per-interface records the
    // Linux usbip-utils expects. The first 9 bytes are
    // tusb_desc_configuration_t; bNumInterfaces is at offset 4.
    if (actual >= sizeof(tusb_desc_configuration_t)) {
      const tusb_desc_configuration_t *cfg =
          (const tusb_desc_configuration_t *)d->config_desc;
      d->num_interfaces = cfg->bNumInterfaces;
    }
  }
}

void on_xfer_complete(tuh_xfer_t *xfer) {
  uint32_t idx = (uint32_t)xfer->user_data;
  if (idx >= MAX_INFLIGHT) return;
  inflight_t *u = &s_inflight[idx];
  if (!u->in_use) return;

  // Sync result + actual_len back into our slot. tuh_control_xfer
  // copies the xfer struct internally, so the callback receives a
  // pointer to the stack's own copy - reading u->xfer fields after
  // submit returns the values we filled in pre-submit, not the
  // post-completion values.
  u->xfer.result     = xfer->result;
  u->xfer.actual_len = xfer->actual_len;

  maybe_capture_config_desc(u);

  // Build RET_SUBMIT.
  usbip_header_t out;
  memset(&out, 0, sizeof(out));
  out.base.command = be32(USBIP_RET_SUBMIT);
  out.base.seqnum  = be32(u->seqnum);

  int32_t status_code = 0;
  switch (xfer->result) {
    case XFER_RESULT_SUCCESS: status_code = 0; break;
    case XFER_RESULT_STALLED: status_code = -32;  break; // -EPIPE
    case XFER_RESULT_FAILED:  status_code = -71;  break; // -EPROTO
    case XFER_RESULT_TIMEOUT: status_code = -110; break; // -ETIMEDOUT
    default:                  status_code = -71;  break;
  }
  out.u.ret_submit.status        = be32((uint32_t)status_code);
  out.u.ret_submit.actual_length = be32(xfer->actual_len);
  out.u.ret_submit.start_frame   = 0;
  out.u.ret_submit.number_of_packets = be32(USBIP_NON_ISO_PACKETS);

  err_t e = conn_send(&s_conn, &out, sizeof(out));
  if (e == ERR_OK && u->direction == USBIP_DIR_IN && xfer->actual_len > 0) {
    e = conn_send(&s_conn, u->buf, xfer->actual_len);
  }
  if (e != ERR_OK) {
    printf("usbip: RET_SUBMIT seq=%u: tcp_write %d\n",
           (unsigned)u->seqnum, (int)e);
  }
  uint8_t freed_daddr   = u->daddr;
  uint8_t freed_ep_addr = u->ep_addr;
  bool    freed_control = u->is_control != 0;
  u->in_use = false;

  // Drain any queued URBs targeting the same EP. Control transfers
  // never queue (we serialise on tuh_control_xfer's failure), so skip.
  if (!freed_control) {
    drain_queue_for_ep(freed_daddr, freed_ep_addr);
  }
}
