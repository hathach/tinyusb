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
 * USB/IP server: connection orchestration.
 *
 * Holds the single-client conn_t state and the lwIP TCP callbacks
 * (accept / recv / err). Dispatches incoming bytes through two
 * framing state machines:
 *
 *   - pre-import: 8-byte op_common header then OP_REQ_DEVLIST or
 *     OP_REQ_IMPORT (busid). Implementations live in usbip_devices.c.
 *
 *   - imported: 48-byte usbip_header_t + optional OUT payload, then
 *     CMD_SUBMIT or CMD_UNLINK. Implementations live in
 *     usbip_submit.c, completion in usbip_inflight.c.
 *
 * Single-client. lwIP raw API, NO_SYS=1: the lwIP recv path and
 * tuh_task share the main thread so no locking is needed.
 */

#include <stdio.h>
#include <string.h>

#include "lwip/tcp.h"
#include "lwip/pbuf.h"

#include "usbip_server.h"
#include "usbip_internal.h"

conn_t s_conn;
static struct tcp_pcb *s_listen_pcb;

static void conn_close(conn_t *c);

//--------------------------------------------------------------------+
// Wire helpers
//--------------------------------------------------------------------+

err_t conn_send(conn_t *c, const void *buf, size_t len) {
  if (c->pcb == NULL) return ERR_CONN;
  err_t e = tcp_write(c->pcb, buf, len, TCP_WRITE_FLAG_COPY);
  if (e != ERR_OK) return e;
  return tcp_output(c->pcb);
}

err_t send_op_common(conn_t *c, uint16_t code, uint32_t status) {
  usbip_op_common_t op = {
      .version = be16(USBIP_VERSION),
      .code    = be16(code),
      .status  = be32(status),
  };
  return conn_send(c, &op, sizeof(op));
}

//--------------------------------------------------------------------+
// IMPORTED-state framing: 48-byte header, optional OUT payload
//--------------------------------------------------------------------+

static err_t process_imported_input(conn_t *c) {
  while (1) {
    if (c->urb_state == URB_RX_HEADER) {
      // Drain rxbuf into urb_hdr_buf
      while (c->urb_hdr_have < sizeof(usbip_header_t) && c->rxlen > 0) {
        size_t want = sizeof(usbip_header_t) - c->urb_hdr_have;
        size_t take = c->rxlen < want ? c->rxlen : want;
        memcpy(c->urb_hdr_buf + c->urb_hdr_have, c->rxbuf, take);
        c->urb_hdr_have += take;
        memmove(c->rxbuf, c->rxbuf + take, c->rxlen - take);
        c->rxlen -= take;
      }
      if (c->urb_hdr_have < sizeof(usbip_header_t)) return ERR_OK;

      const usbip_header_t *hdr_be = (const usbip_header_t *)c->urb_hdr_buf;
      uint32_t cmd = be32(hdr_be->base.command);

      if (cmd == USBIP_CMD_SUBMIT) {
        uint32_t dir   = be32(hdr_be->base.direction);
        uint32_t ep    = be32(hdr_be->base.ep);
        int32_t  blen  = (int32_t)be32((uint32_t)hdr_be->u.cmd_submit.transfer_buffer_length);
        // Wire-field validation: reject obviously bad headers up front
        // rather than letting them cast-truncate into bus calls. The
        // attacker model is "anyone on the LAN can connect".
        if (dir > 1u || ep > 15u || blen < 0 || blen > (int32_t)MAX_URB_BUF) {
          printf("usbip: bad SUBMIT (dir=%u ep=%u blen=%d)\n",
                 (unsigned)dir, (unsigned)ep, (int)blen);
          return ERR_VAL;
        }
        if (dir == USBIP_DIR_OUT && blen > 0) {
          // Need to read OUT payload before dispatching.
          inflight_t *u = inflight_alloc();
          if (u == NULL) {
            printf("usbip: SUBMIT: no inflight slot for OUT (blen=%d)\n",
                   (int)blen);
            return ERR_VAL;
          }
          // Stash header-derived metadata; we'll fill the rest in submit_urb.
          u->seqnum    = be32(hdr_be->base.seqnum);
          u->devid     = be32(hdr_be->base.devid);
          u->direction = dir;
          u->ep        = ep;
          u->daddr     = c->imported_daddr;
          u->ep_addr   = (uint8_t)ep | 0x00; // OUT
          u->is_control = (ep == 0);
          u->transfer_buffer_length = (uint32_t)blen;
          c->urb_out_inflight = u;
          c->urb_out_have = 0;
          c->urb_out_need = (size_t)blen;
          c->urb_state = URB_RX_OUT_DATA;
          continue;
        }
        // No OUT payload: dispatch right now using hdr_be.
        c->urb_hdr_have = 0;
        err_t e = submit_urb(c, hdr_be);
        if (e != ERR_OK) return e;
      } else if (cmd == USBIP_CMD_UNLINK) {
        c->urb_hdr_have = 0;
        err_t e = handle_unlink(c, hdr_be);
        if (e != ERR_OK) return e;
      } else {
        // Drop the connection on unknown command. Continuing would
        // happily consume any 48-byte aligned junk indefinitely.
        printf("usbip: unknown cmd 0x%08x, closing conn\n", (unsigned)cmd);
        return ERR_VAL;
      }
    } else { // URB_RX_OUT_DATA
      if (c->urb_out_inflight == NULL) return ERR_VAL;
      while (c->urb_out_have < c->urb_out_need && c->rxlen > 0) {
        size_t want = c->urb_out_need - c->urb_out_have;
        size_t take = c->rxlen < want ? c->rxlen : want;
        memcpy(c->urb_out_inflight->buf + c->urb_out_have, c->rxbuf, take);
        c->urb_out_have += take;
        memmove(c->rxbuf, c->rxbuf + take, c->rxlen - take);
        c->rxlen -= take;
      }
      if (c->urb_out_have < c->urb_out_need) return ERR_OK;
      // Got full OUT data; dispatch.
      const usbip_header_t *hdr_be = (const usbip_header_t *)c->urb_hdr_buf;
      err_t e = submit_urb(c, hdr_be);
      c->urb_hdr_have = 0;
      c->urb_out_inflight = NULL;
      c->urb_out_have = 0;
      c->urb_out_need = 0;
      c->urb_state = URB_RX_HEADER;
      if (e != ERR_OK) return e;
    }
  }
}

//--------------------------------------------------------------------+
// Pre-import state machine (DEVLIST / IMPORT framing)
//--------------------------------------------------------------------+

static err_t conn_step_preimport(conn_t *c) {
  while (1) {
    switch (c->state) {
      case ST_WAIT_OP: {
        if (c->rxlen < sizeof(usbip_op_common_t)) return ERR_OK;
        usbip_op_common_t op;
        memcpy(&op, c->rxbuf, sizeof(op));
        uint16_t code = be16(op.code);
        memmove(c->rxbuf, c->rxbuf + sizeof(op), c->rxlen - sizeof(op));
        c->rxlen -= sizeof(op);
        if (code == USBIP_OP_REQ_DEVLIST) {
          err_t e = handle_devlist(c);
          if (e != ERR_OK) return e;
          c->state = ST_WAIT_OP;
        } else if (code == USBIP_OP_REQ_IMPORT) {
          c->state = ST_WAIT_IMPORT_BUSID;
          c->need  = USBIP_BUSID_SIZE;
        } else {
          printf("usbip: unknown op 0x%04x\n", code);
          return ERR_VAL;
        }
        break;
      }
      case ST_WAIT_IMPORT_BUSID: {
        if (c->rxlen < c->need) return ERR_OK;
        char busid_z[USBIP_BUSID_SIZE + 1];
        memcpy(busid_z, c->rxbuf, USBIP_BUSID_SIZE);
        busid_z[USBIP_BUSID_SIZE] = 0;
        memmove(c->rxbuf, c->rxbuf + c->need, c->rxlen - c->need);
        c->rxlen -= c->need;
        c->need = 0;
        err_t e = handle_import(c, busid_z);
        if (e != ERR_OK) return e;
        if (c->state == ST_IMPORTED) return ERR_OK;
        c->state = ST_WAIT_OP;
        break;
      }
      default:
        return ERR_OK;
    }
  }
}

//--------------------------------------------------------------------+
// lwIP TCP callbacks
//--------------------------------------------------------------------+

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
  conn_t *c = (conn_t *)arg;
  if (err != ERR_OK) {
    if (p) pbuf_free(p);
    conn_close(c);
    return ERR_OK;
  }
  if (p == NULL) { conn_close(c); return ERR_OK; }

  // Drain pbuf into rxbuf in chunks. After a chunk, run state machine.
  // The pbuf can be larger than rxbuf, so loop until consumed.
  size_t consumed = 0;
  while (consumed < p->tot_len) {
    size_t avail = sizeof(c->rxbuf) - c->rxlen;
    size_t take  = (size_t)(p->tot_len - consumed);
    if (take > avail) take = avail;
    pbuf_copy_partial(p, c->rxbuf + c->rxlen, take, (uint16_t)consumed);
    c->rxlen += take;
    consumed += take;

    err_t e = (c->state == ST_IMPORTED) ? process_imported_input(c)
                                        : conn_step_preimport(c);
    if (e != ERR_OK) {
      tcp_recved(pcb, p->tot_len);
      pbuf_free(p);
      conn_close(c);
      return ERR_OK;
    }
    if (avail == 0 && take == 0) {
      // Cannot drain - state machine refused to consume. Drop the rest.
      printf("usbip: rx stuck, dropping conn\n");
      tcp_recved(pcb, p->tot_len);
      pbuf_free(p);
      conn_close(c);
      return ERR_OK;
    }
  }
  tcp_recved(pcb, p->tot_len);
  pbuf_free(p);
  return ERR_OK;
}

// Walk the inflight pool and abort any in-flight (non-queued, non-control)
// transfers via tuh_edpt_abort_xfer. With the abort_xfer fix the natural
// completion callback fires for each abort and runs through
// on_xfer_complete which would normally send RET_SUBMIT(FAILED) - but the
// connection is gone, so conn_send would fail. We zero in_use first so
// on_xfer_complete silently drops the result and just releases the EP
// claim. Control transfers don't have a per-EP claim concept here; the
// bus task's own retry/timeout handles them.
static void abort_all_inflight_and_clear(void) {
  for (int i = 0; i < MAX_INFLIGHT; i++) {
    inflight_t *u = &s_inflight[i];
    if (!u->in_use) continue;
    bool was_in_flight = !u->queued && !u->is_control;
    u->in_use = false;
    if (was_in_flight) {
      tuh_edpt_abort_xfer(u->daddr, u->ep_addr);
    }
  }
}

static void tcp_err_cb(void *arg, err_t err) {
  (void)err;
  conn_t *c = (conn_t *)arg;
  c->pcb   = NULL;
  c->state = ST_CLOSING;
  c->rxlen = 0;
  // Release EP claims via the abort path. Without this, in-flight
  // bulk/int URBs would stay claimed in TinyUSB until next umount,
  // blocking subsequent reattaches on the same EP.
  abort_all_inflight_and_clear();
}

static void conn_close(conn_t *c) {
  if (c->pcb != NULL) {
    tcp_arg(c->pcb, NULL);
    tcp_recv(c->pcb, NULL);
    tcp_err(c->pcb, NULL);
    if (tcp_close(c->pcb) != ERR_OK) tcp_abort(c->pcb);
    c->pcb = NULL;
  }
  c->state = ST_CLOSING;
  c->rxlen = 0;
  c->imported_daddr = 0;
  c->urb_state = URB_RX_HEADER;
  c->urb_hdr_have = 0;
  c->urb_out_inflight = NULL;
  abort_all_inflight_and_clear();
}

static err_t tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
  (void)arg;
  if (err != ERR_OK || newpcb == NULL) return ERR_VAL;
  if (s_conn.pcb != NULL) {
    printf("usbip: rejecting second client\n");
    tcp_abort(newpcb);
    return ERR_ABRT;
  }
  printf("usbip: accept\n");
  memset(&s_conn, 0, sizeof(s_conn));
  s_conn.pcb   = newpcb;
  s_conn.state = ST_WAIT_OP;
  tcp_arg(newpcb, &s_conn);
  tcp_recv(newpcb, tcp_recv_cb);
  tcp_err(newpcb, tcp_err_cb);
  return ERR_OK;
}

// Override at build time with -DUSBIPD_BIND_ADDR=&my_addr to bind on a
// specific interface; default IP_ADDR_ANY exposes the bridge to anyone
// reachable at L2.
#ifndef USBIPD_BIND_ADDR
#define USBIPD_BIND_ADDR  IP_ADDR_ANY
#endif

void usbip_server_init(void) {
  struct tcp_pcb *pcb = tcp_new();
  if (pcb == NULL) { printf("usbip: tcp_new failed\n"); return; }
  if (tcp_bind(pcb, USBIPD_BIND_ADDR, USBIP_TCP_PORT) != ERR_OK) {
    printf("usbip: tcp_bind(%u) failed\n", (unsigned)USBIP_TCP_PORT);
    tcp_close(pcb);
    return;
  }
  s_listen_pcb = tcp_listen(pcb);
  if (s_listen_pcb == NULL) {
    printf("usbip: tcp_listen failed\n");
    tcp_close(pcb);
    return;
  }
  tcp_accept(s_listen_pcb, tcp_accept_cb);
}
