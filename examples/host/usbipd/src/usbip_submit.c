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
 * CMD_SUBMIT dispatch and CMD_UNLINK handling. submit_urb takes a
 * parsed wire header (with OUT payload already in the inflight
 * slot's buf[] for OUT direction) and either hands it to TinyUSB
 * via tuh_control_xfer / tuh_edpt_xfer, or queues it behind any
 * URB already in flight on the same endpoint.
 *
 * Per-EP queue (drain_queue_for_ep): cdc-acm queues 16 read-ahead
 * URBs on its bulk-IN, but TinyUSB allows only one URB per EP in
 * flight, so the rejected ones get queued and drained from the
 * completion callback when the EP frees.
 */

#include <stdio.h>
#include <string.h>

#include "usbip_internal.h"

// Attempt to open a non-control endpoint from the cached config
// descriptor. Returns true if open or already opened.
static bool ensure_ep_open(uint8_t daddr, uint8_t ep_addr) {
  cached_dev_t *d = &s_devs[daddr];
  uint32_t bit = ep_open_bit(ep_addr);
  if (d->ep_open & bit) return true;
  const tusb_desc_endpoint_t *ep_desc = find_ep_desc(d, ep_addr);
  if (ep_desc == NULL) {
    printf("usbip: ep_open: no descriptor for daddr=%u ep=0x%02x\n",
           daddr, ep_addr);
    return false;
  }
  if (!tuh_edpt_open(daddr, ep_desc)) {
    printf("usbip: ep_open: tuh_edpt_open failed daddr=%u ep=0x%02x\n",
           daddr, ep_addr);
    return false;
  }
  d->ep_open |= bit;
  return true;
}

err_t submit_urb(conn_t *c, const usbip_header_t *hdr_be) {
  uint32_t seqnum = be32(hdr_be->base.seqnum);
  uint32_t devid  = be32(hdr_be->base.devid);
  uint32_t dir    = be32(hdr_be->base.direction);
  uint32_t ep     = be32(hdr_be->base.ep);
  int32_t  blen   = (int32_t)be32((uint32_t)hdr_be->u.cmd_submit.transfer_buffer_length);

  inflight_t *u;
  if (dir == USBIP_DIR_OUT && blen > 0) {
    // submit_urb is called after OUT payload is already in inflight->buf
    u = c->urb_out_inflight;
  } else {
    u = inflight_alloc();
    if (u == NULL) {
      printf("usbip: SUBMIT seq=%u: no inflight slot\n", (unsigned)seqnum);
      return ERR_VAL;
    }
    u->seqnum = seqnum;
    u->devid  = devid;
    u->direction = dir;
    u->ep        = ep;
    u->daddr     = c->imported_daddr;
    u->is_control = (ep == 0);
    // For control transfers tuh_control_xfer asserts ep_addr == 0
    // (the setup packet's bmRequestType.7 picks the data direction).
    // For bulk/int we OR in the IN bit per the standard ep_addr layout.
    u->ep_addr = u->is_control
        ? 0u
        : ((uint8_t)ep | (dir == USBIP_DIR_IN ? 0x80u : 0x00u));
    u->transfer_buffer_length = (blen > 0) ? (uint32_t)blen : 0u;
  }

  if ((size_t)u->transfer_buffer_length > MAX_URB_BUF) {
    printf("usbip: SUBMIT seq=%u: oversize (%u)\n",
           (unsigned)seqnum, (unsigned)u->transfer_buffer_length);
    u->in_use = false;
    return ERR_VAL;
  }

  // Setup packet copied for control transfers; buffer lifetime bound to slot.
  if (u->is_control) {
    memcpy(&u->setup, hdr_be->u.cmd_submit.setup, 8);
    // Refuse SET_ADDRESS over the bridge: the kernel can't issue this
    // legitimately (vhci_hcd handles its own address assignment), and
    // a remote SET_ADDRESS would silently desync TinyUSB's view of
    // the device's bus address. Reply RET_SUBMIT(EPIPE) and free.
    if (u->setup.bRequest == TUSB_REQ_SET_ADDRESS &&
        (u->setup.bmRequestType & 0x60u) == 0u) { // standard request
      printf("usbip: refusing SET_ADDRESS from wire (seq=%u)\n",
             (unsigned)u->seqnum);
      usbip_header_t out;
      memset(&out, 0, sizeof(out));
      out.base.command        = be32(USBIP_RET_SUBMIT);
      out.base.seqnum         = be32(u->seqnum);
      out.u.ret_submit.status = be32((uint32_t)-32);  // -EPIPE
      err_t e = conn_send(c, &out, sizeof(out));
      u->in_use = false;
      return e;
    }
  }

  // Open the endpoint up front so a queued slot doesn't need to retry it.
  if (!u->is_control && !ensure_ep_open(u->daddr, u->ep_addr)) {
    u->in_use = false;
    return ERR_VAL;
  }

  return submit_or_queue(c, u);
}

// Try to hand the inflight slot to TinyUSB. If the EP is busy, mark the
// slot queued and return ERR_OK; the completion callback will drain it.
err_t submit_or_queue(conn_t *c, inflight_t *u) {
  static uint64_t s_queue_counter = 1;

  // Connection gone (e.g. tcp_err_cb fired between rx and drain).
  // Free the slot and drop the URB - there's no client to reply to.
  if (c == NULL || c->pcb == NULL) {
    u->in_use = false;
    u->queued = false;
    return ERR_OK;
  }

  // Track whether this is a re-entry from drain_queue_for_ep so we
  // preserve the original queue_seq on requeue. Without this,
  // bouncing between busy and free shuffles slots to the back of
  // the queue, breaking FIFO ordering vs the kernel's submit order.
  const bool was_queued = u->queued;

  memset(&u->xfer, 0, sizeof(u->xfer));
  u->xfer.daddr = u->daddr;
  u->xfer.ep_addr = u->ep_addr;
  u->xfer.buffer = u->buf;
  u->xfer.complete_cb = on_xfer_complete;
  u->xfer.user_data = (uintptr_t)(u - s_inflight);

  bool ok = false;
  if (u->is_control) {
    u->xfer.setup = &u->setup;
    ok = tuh_control_xfer(&u->xfer);
  } else {
    u->xfer.buflen = u->transfer_buffer_length;
    ok = tuh_edpt_xfer(&u->xfer);
  }

  if (ok) {
    u->queued = false;
    return ERR_OK;
  }

  // Could not submit. For control transfers there's no queue concept
  // (kernel serialises ep0 itself); for bulk/int the kernel queues
  // ahead and we serialise on our side.
  if (u->is_control) {
    printf("usbip: ctl SUBMIT seq=%u rejected\n", (unsigned)u->seqnum);
    usbip_header_t out;
    memset(&out, 0, sizeof(out));
    out.base.command        = be32(USBIP_RET_SUBMIT);
    out.base.seqnum         = be32(u->seqnum);
    out.u.ret_submit.status = be32((uint32_t)-32);  // -EPIPE
    err_t e = conn_send(c, &out, sizeof(out));
    u->in_use = false;
    return e;
  }

  u->queued = true;
  if (!was_queued) {
    u->queue_seq = s_queue_counter++;
  }
  return ERR_OK;
}

// Find the oldest queued slot for a given (daddr, ep_addr) and submit
// it. Returns true if something was submitted (or attempted).
bool drain_queue_for_ep(uint8_t daddr, uint8_t ep_addr) {
  inflight_t *next = NULL;
  for (int i = 0; i < MAX_INFLIGHT; i++) {
    inflight_t *u = &s_inflight[i];
    if (!u->in_use || !u->queued) continue;
    if (u->daddr != daddr || u->ep_addr != ep_addr) continue;
    if (next == NULL || u->queue_seq < next->queue_seq) next = u;
  }
  if (next == NULL) return false;
  (void)submit_or_queue(&s_conn, next);
  return true;
}

err_t handle_unlink(conn_t *c, const usbip_header_t *hdr_be) {
  uint32_t seqnum         = be32(hdr_be->base.seqnum);
  uint32_t unlink_seqnum  = be32(hdr_be->u.cmd_unlink.unlink_seqnum);

  inflight_t *target = inflight_find_seqnum(unlink_seqnum);
  int32_t status = 0;
  if (target != NULL) {
    if (target->queued) {
      // Slot is waiting in our internal queue, never reached the bus.
      // Free it immediately and drain so the next queued URB gets a
      // chance. No RET_SUBMIT - we never told the kernel it started.
      printf("usbip: UNLINK seq=%u (cancels queued seq=%u ep=0x%02x)\n",
             (unsigned)seqnum, (unsigned)unlink_seqnum, target->ep_addr);
      uint8_t daddr = target->daddr, ep_addr = target->ep_addr;
      target->in_use = false;
      target->queued = false;
      drain_queue_for_ep(daddr, ep_addr);
      status = 0;
    } else if (target->is_control) {
      // Upstream has no API to cancel an in-flight control transfer.
      // Flag the unlink and let the kernel see status != 0 in
      // RET_UNLINK; the natural callback (PR4 timeout, if hit) will
      // also retire the URB.
      printf("usbip: UNLINK seq=%u (cancels in-flight control seq=%u)\n",
             (unsigned)seqnum, (unsigned)unlink_seqnum);
      status = -104;  // -ECONNRESET
    } else {
      printf("usbip: UNLINK seq=%u (cancels in-flight seq=%u ep=0x%02x)\n",
             (unsigned)seqnum, (unsigned)unlink_seqnum, target->ep_addr);
      // PR3: tuh_edpt_abort_xfer fires the natural completion callback,
      // which sends RET_SUBMIT(FAILED) for the in-flight URB. The
      // kernel sees that as the giveback; RET_UNLINK with status=0
      // tells it the unlink itself succeeded (URB had not yet
      // completed when unlink was processed).
      tuh_edpt_abort_xfer(target->daddr, target->ep_addr);
      status = 0;
    }
  } else {
    printf("usbip: UNLINK seq=%u (target seq=%u already complete)\n",
           (unsigned)seqnum, (unsigned)unlink_seqnum);
    status = 0;
  }

  usbip_header_t out;
  memset(&out, 0, sizeof(out));
  out.base.command = be32(USBIP_RET_UNLINK);
  out.base.seqnum  = be32(seqnum);
  out.u.ret_unlink.status = be32((uint32_t)status);
  return conn_send(c, &out, sizeof(out));
}
