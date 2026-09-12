// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "tusb_option.h"
#include "common/tusb_common.h"

// Only the QH software tail contains a native pointer. On a 64-bit test host
// its size differs from the 32-bit target ABI. Recheck hardware layouts below.
#undef TU_VERIFY_STATIC
#define TU_VERIFY_STATIC(condition, ...) \
  _Static_assert((condition) || sizeof(void*) == 8, "EHCI ABI")
#include "portable/ehci/ehci.h"
#undef TU_VERIFY_STATIC
#define TU_VERIFY_STATIC(condition, ...) _Static_assert(condition, __VA_ARGS__)
#include "portable/ehci/ehci.c"

_Static_assert(sizeof(ehci_link_t) == 4, "link ABI");
_Static_assert(sizeof(ehci_qtd_t) == 32, "qTD ABI");
_Static_assert(offsetof(ehci_qhd_t, qtd_overlay) == 16, "QH hardware prefix");
_Static_assert(sizeof(ehci_itd_t) == 64, "iTD ABI");
_Static_assert(sizeof(ehci_sitd_t) == 32, "siTD ABI");
_Static_assert(sizeof(ehci_cap_registers_t) == 16, "capability register ABI");

static ehci_registers_t regs;
static ehci_cap_registers_t caps;
static tuh_bus_info_t buses[8];
static hcd_event_t event;
static unsigned events;
static unsigned queued_events;
static hcd_event_t terminal_events[16];
static uint8_t buffer[8192] TU_ATTR_ALIGNED(4096);

void hcd_int_enable(uint8_t rhport) { (void) rhport; }
void hcd_int_disable(uint8_t rhport) { (void) rhport; }
void usbh_spin_lock(bool in_isr) { (void) in_isr; }
void usbh_spin_unlock(bool in_isr) { (void) in_isr; }
bool tuh_bus_info_get(uint8_t daddr, tuh_bus_info_t* bus) {
  memset(bus, 0, sizeof(*bus));
  if (daddr >= TU_ARRAY_SIZE(buses)) {
    return false;
  }
  *bus = buses[daddr];
  return true;
}
void hcd_event_handler(hcd_event_t const* e, bool in_isr) {
  (void) in_isr;
  if (e->event_id == HCD_EVENT_XFER_COMPLETE && e->xfer_complete.result == XFER_RESULT_QUEUED) {
    queued_events++;
    return;
  }
  event = *e;
  assert(events < TU_ARRAY_SIZE(terminal_events));
  terminal_events[events++] = *e;
}

static void reset(uint8_t root_speed) {
  memset(&ehci_data, 0, sizeof(ehci_data));
  memset((void*)&regs, 0, sizeof(regs));
  memset((void*)&caps, 0, sizeof(caps));
  memset(buses, 0, sizeof(buses));
  ehci_data.regs = &regs;
  ehci_data.cap_regs = &caps;
  regs.portsc = (uint32_t)root_speed << 26;
  regs.frame_index = 800;
  regs.command_bm.int_threshold = 8;
  init_periodic_list(0);
  events = queued_events = 0;
}

static void test_attach_debounce(void) {
  reset(TUSB_SPEED_FULL);
  regs.portsc |= EHCI_PORTSC_MASK_CURRENT_CONNECT_STATUS;
  uint32_t const before = regs.portsc;
  port_connect_status_change_isr(0);
  assert(events == 1 && event.event_id == HCD_EVENT_DEVICE_ATTACH);
  assert(regs.portsc == before); // Attach must not reset the port before USBH debounces it.
}

#if CFG_TUH_EHCI_ISO_EP_MAX
static bool open_ep(uint8_t addr, uint8_t speed, uint16_t size, uint8_t interval) {
  buses[1].speed = speed;
  tusb_desc_endpoint_t desc = {
    .bLength = sizeof(desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = addr, .bmAttributes = {.xfer = TUSB_XFER_ISOCHRONOUS},
    .wMaxPacketSize = size, .bInterval = interval
  };
  return iso_ep_open(0, 1, &desc);
}

static void test_native_fs(void) {
  reset(TUSB_SPEED_FULL);
  assert(open_ep(0x81, TUSB_SPEED_FULL, 1023, 1));
  iso_ep_t* ep = iso_ep_find(1, 0x81);
  assert(iso_xfer(0, ep, buffer + 4090, 1023));
  ehci_sitd_t* td = &iso_td(ep, &ep->req[ep->head])->sitd;
  assert(ep->req[ep->head].scheduled_uframe == 800);
  assert(td->active && td->int_on_complete && td->total_bytes == 1023);
  assert(td->int_smask == 0 && td->fl_int_cmask == 0);
  assert(td->buffer[0] == (uint32_t)(uintptr_t)(buffer + 4090));
  assert(td->buffer[1] == (uint32_t)(uintptr_t)(buffer + 4096));

  td->active = 0;
  td->total_bytes = 23;
  regs.frame_index = 808;
  iso_process(true);
  assert(events == 1 && event.xfer_complete.len == 1000);
  assert(event.xfer_complete.result == XFER_RESULT_SUCCESS);
  iso_process(true);
  assert(events == 1);
  assert(iso_xfer(0, ep, buffer, 0));
  td = &iso_td(ep, &ep->req[ep->head])->sitd;
  td->active = 0;
  regs.frame_index += 8;
  iso_process(true);
  assert(events == 2 && event.xfer_complete.len == 0);
  assert(event.xfer_complete.result == XFER_RESULT_SUCCESS);
}

static void test_split(void) {
  reset(TUSB_SPEED_HIGH);
  buses[1].hub_addr = 2;
  buses[1].hub_port = 3;
  buses[2].speed = TUSB_SPEED_FULL;
  buses[2].hub_addr = 3;
  buses[2].hub_port = 4;
  buses[3].speed = TUSB_SPEED_HIGH;
  assert(!open_ep(0x81, TUSB_SPEED_FULL, 565, 1));
  assert(open_ep(0x81, TUSB_SPEED_FULL, 564, 1));
  iso_ep_t* ep = iso_ep_find(1, 0x81);
  assert(iso_xfer(0, ep, buffer, 564));
  ehci_sitd_t* td = &iso_td(ep, &ep->req[ep->head])->sitd;
  assert(td->hub_addr == 3 && td->port_number == 4);
  assert(td->int_smask == 4 && td->fl_int_cmask == 0xf0);
  assert(open_ep(1, TUSB_SPEED_FULL, 1023, 1));
  ep = iso_ep_find(1, 1);
  assert(iso_xfer(0, ep, buffer, 1023));
  td = &iso_td(ep, &ep->req[ep->head])->sitd;
  assert(td->int_smask == 0x3f && td->fl_int_cmask == 0);
  assert((td->buffer[1] & 0xfff) == (6 | 8));
}

static void test_split_audio(void) {
  reset(TUSB_SPEED_HIGH);
  buses[1].hub_addr = 2;
  buses[1].hub_port = 1;
  buses[2].speed = TUSB_SPEED_HIGH;
  assert(open_ep(0x81, TUSB_SPEED_FULL, 98, 1));
  assert(open_ep(0x01, TUSB_SPEED_FULL, 196, 1));
  iso_ep_t* in = iso_ep_find(1, 0x81);
  iso_ep_t* out = iso_ep_find(1, 0x01);
  assert(iso_xfer(0, in, buffer, 98));
  assert(iso_xfer(0, out, buffer + 128, 192));
  ehci_sitd_t* in_td = &iso_td(in, &in->req[in->head])->sitd;
  ehci_sitd_t* out_td = &iso_td(out, &out->req[out->head])->sitd;
  // 48 kHz stereo needs two start-splits. Do not interleave an IN start
  // with the OUT Begin/End sequence on the same transaction translator.
  assert(out_td->int_smask == 3);
  assert((out_td->buffer[1] & 0x1f) == (2 | 8));
  assert((in_td->int_smask & out_td->int_smask) == 0);
  assert(in_td->int_smask > out_td->int_smask);
}

static void test_hs(void) {
  reset(TUSB_SPEED_HIGH);
  assert(open_ep(0x82, TUSB_SPEED_HIGH, 1024 | (2 << 11), 1));
  iso_ep_t* ep = iso_ep_find(1, 0x82);
  assert(!iso_xfer(0, ep, buffer, 3073));
  assert(iso_xfer(0, ep, buffer + 4095, 3072));
  ehci_itd_t* td = &iso_td(ep, &ep->req[ep->head])->itd;
  uint8_t slot = ep->req[ep->head].scheduled_uframe & 7;
  assert(ep->req[ep->head].scheduled_uframe == 802);
  assert(((uintptr_t)td & 63) == 0);
  assert(td->xact[slot].offset == 4095 && td->xact[slot].length == 3072);
  assert((td->BufferPointer[0] & 0xfff) == 0x201);
  assert((td->BufferPointer[1] & 0xfff) == 0xc00);
  assert((td->BufferPointer[2] & 0xfff) == 3);
  assert((td->BufferPointer[0] & ~0xfffu) == (uint32_t)(uintptr_t)buffer);
  assert((td->BufferPointer[1] & ~0xfffu) == (uint32_t)(uintptr_t)(buffer + 4096));
  assert((td->BufferPointer[2] & ~0xfffu) == (uint32_t)(uintptr_t)(buffer + 8192));
  for (unsigned i = 0; i < 8; i++) {
    assert(td->xact[i].active == (i == slot));
  }
  td->xact[slot].active = 0;
  td->xact[slot].length = 2048;
  regs.frame_index = 803;
  iso_process(true);
  assert(events == 1 && event.xfer_complete.len == 2048);
  assert(iso_xfer(0, ep, buffer, 10));
  td = &iso_td(ep, &ep->req[ep->head])->itd;
  slot = ep->req[ep->head].scheduled_uframe & 7;
  td->xact[slot].active = 0;
  td->xact[slot].babble_err = 1;
  regs.frame_index = ep->req[ep->head].scheduled_uframe + 1;
  iso_process(true);
  assert(events == 2 && event.xfer_complete.result == XFER_RESULT_FAILED);
  assert(event.xfer_complete.len == 0);
}

static void test_long_interval_and_wrap(void) {
  reset(TUSB_SPEED_HIGH);
  assert(open_ep(1, TUSB_SPEED_HIGH, 64, 10));
  iso_ep_t* ep = iso_ep_find(1, 1);
  assert(iso_xfer(0, ep, buffer, 64));
  assert(!ep->req[ep->head].armed && ep->req[ep->head].scheduled_uframe == 1312);
  regs.frame_index = 1264;
  iso_process(true);
  assert(ep->req[ep->head].armed && events == 0);
  iso_td(ep, &ep->req[ep->head])->itd.xact[0].active = 0;
  regs.frame_index = 1313;
  iso_process(true);
  assert(events == 1 && event.xfer_complete.len == 64);
  assert(iso_xfer(0, ep, buffer, 64));
  assert(!ep->req[ep->head].armed && iso_abort(0, ep));
  assert(ep->count == 0 && events == 1);
  // Start a separate long interval request and let its arm window expire.
  ep->next_uframe = regs.frame_index + ep->interval;
  assert(iso_xfer(0, ep, buffer, 64));
  regs.frame_index = ep->req[ep->head].scheduled_uframe + 1;
  iso_process(true);
  assert(ep->count == 0 && events == 2 && event.xfer_complete.result == XFER_RESULT_FAILED);
  ehci_data.iso_last_frindex = 16380;
  ehci_data.iso_uframe = 0xfffffffcu;
  regs.frame_index = 4;
  assert(iso_now() == 4);
}

static void test_iso_status_errors(void) {
  for (unsigned hs = 0; hs < 2; hs++) {
    for (unsigned err = 0; err < (hs ? 3u : 5u); err++) {
      reset(hs ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL);
      assert(open_ep(0x81, hs ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL, 64, 1));
      iso_ep_t* ep = iso_ep_find(1, 0x81);
      assert(iso_xfer(0, ep, buffer, 64));
      iso_req_t* req = &ep->req[ep->head];
      iso_td_t* td = iso_td(ep, req);
      // Active work must remain queued, even if status bits are already set.
      if (hs) {
        unsigned const slot = req->scheduled_uframe & 7;
        switch (err) {
          case 0: td->itd.xact[slot].error = 1; break;
          case 1: td->itd.xact[slot].babble_err = 1; break;
          case 2: td->itd.xact[slot].buffer_err = 1; break;
        }
        iso_process(true);
        assert(events == 0);
        td->itd.xact[slot].active = 0;
      } else {
        switch (err) {
          case 0: td->sitd.error = 1; break;
          case 1: td->sitd.buffer_err = 1; break;
          case 2: td->sitd.babble_err = 1; break;
          case 3: td->sitd.xact_err = 1; break;
          case 4: td->sitd.missed_uframe = 1; break;
        }
        iso_process(true);
        assert(events == 0);
        td->sitd.active = 0;
      }
      regs.frame_index = (req->scheduled_uframe + ehci_data.iso_frame_offset) & 0x3fff;
      iso_process(true);
      assert(events == 1 && event.xfer_complete.result == XFER_RESULT_FAILED);
      assert(event.xfer_complete.len == 0);
    }
  }
}

static void test_iso_clock_config(void) {
  static uint16_t const due[] = {802, 802, 803, 804, 805, 806, 807, 808,
                                808, 808, 808, 808, 808, 808, 808, 808};
  for (unsigned threshold = 0; threshold < TU_ARRAY_SIZE(due); threshold++) {
    reset(TUSB_SPEED_HIGH);
    caps.hccparams_bm.iso_schedule_threshold = threshold;
    assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, 1));
    assert(iso_earliest(iso_now()) == due[threshold]);
    // Model completed teardown; this fixture cannot emulate the DMA stop/start handshake.
    memset(ehci_data.iso_ep, 0, sizeof(ehci_data.iso_ep));
    regs.inten &= ~EHCI_INT_MASK_NXP_SOF;
    init_periodic_list(0);
    // A new root connection must refresh both cached scheduling attributes.
    regs.portsc = (uint32_t) TUSB_SPEED_FULL << 26;
    caps.hccparams_bm.iso_schedule_threshold = 0;
    assert(open_ep(0x81, TUSB_SPEED_FULL, 64, 1));
    assert(iso_now() == 792 && iso_earliest(iso_now()) == 794);
  }
}

#endif

static void test_qtd_retirement(void) {
  reset(TUSB_SPEED_FULL);
  ehci_qhd_t* qh = &ehci_data.control[1].qhd;
  ehci_qtd_t* td = &ehci_data.control[1].qtd;
  qh->dev_addr = 1;
  qh->attached_qtd = td;
  td->active = 1;
  td->expected_bytes = 3;
  qhd_xfer_complete_isr(qh);
  assert(events == 0 && qh->attached_qtd == td);
  td->active = 0;
  qhd_xfer_complete_isr(qh);
  assert(events == 1 && event.xfer_complete.len == 3);
  qhd_xfer_complete_isr(qh);
  assert(events == 1);
}

#if CFG_TUH_EHCI_ISO_EP_MAX
static void test_limits_and_late_completion(void) {
  reset(TUSB_SPEED_HIGH);
  assert(!open_ep(0x80, TUSB_SPEED_HIGH, 64, 1));
  assert(!open_ep(0x81, TUSB_SPEED_LOW, 64, 1));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 64, 0));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 64, 17));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 0, 1));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 1025, 1));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 64 | (3 << 11), 1));
  for (unsigned i = 1; i <= CFG_TUH_EHCI_ISO_EP_MAX; i++) {
    assert(open_ep((uint8_t)i, TUSB_SPEED_HIGH, 64, 1));
  }
  assert(!open_ep(CFG_TUH_EHCI_ISO_EP_MAX + 1, TUSB_SPEED_HIGH, 64, 1));
  iso_ep_t* ep = iso_ep_find(1, 1);
  assert(iso_xfer(0, ep, buffer, 64));
  iso_td(ep, &ep->req[ep->head])->itd.xact[ep->req[ep->head].scheduled_uframe & 7].active = 0;
  regs.frame_index = (ep->req[ep->head].scheduled_uframe & ~7u) + FRAMELIST_SIZE * 8u;
  iso_process(true);
  assert(events == 1 && event.xfer_complete.result == XFER_RESULT_FAILED);
  assert(ep->count == 0);
}

#if CFG_TUH_XFER_QUEUE_DEPTH > 1
static void test_queue(void) {
  reset(TUSB_SPEED_HIGH);
  assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, 1));
  iso_ep_t* ep = iso_ep_find(1, 0x81);
  for (unsigned round = 0; round < 3; round++) {
    iso_req_t* first = &ep->req[ep->head];
    for (unsigned slot = 0; slot < CFG_TUH_XFER_QUEUE_DEPTH; slot++) {
      assert(iso_xfer(0, ep, buffer + slot * 64, 24 + slot * 4));
      iso_req_t* req = &ep->req[(ep->head + slot) % CFG_TUH_XFER_QUEUE_DEPTH];
      assert(req->scheduled_uframe == first->scheduled_uframe + slot);
      assert(ep->count == slot + 1);
      assert(queued_events == round * (CFG_TUH_XFER_QUEUE_DEPTH - 1) +
             tu_min32(slot + 1, CFG_TUH_XFER_QUEUE_DEPTH - 1));
      if (slot != 0) {
        assert(iso_td(ep, req) != iso_td(ep, first) && req->buffer != first->buffer);
        // Tail completions must never bypass the FIFO head.
        iso_td(ep, req)->itd.xact[req->scheduled_uframe & 7].active = 0;
      }
    }
    assert(!iso_xfer(0, ep, buffer + CFG_TUH_XFER_QUEUE_DEPTH * 64, 32));
    iso_process(true);
    assert(events == round * CFG_TUH_XFER_QUEUE_DEPTH && ep->count == CFG_TUH_XFER_QUEUE_DEPTH);
    iso_td(ep, first)->itd.xact[first->scheduled_uframe & 7].active = 0;
    regs.frame_index = first->scheduled_uframe + CFG_TUH_XFER_QUEUE_DEPTH - 1;
    iso_process(true);
    assert(events == (round + 1) * CFG_TUH_XFER_QUEUE_DEPTH && ep->count == 0);
    for (unsigned slot = 0; slot < CFG_TUH_XFER_QUEUE_DEPTH; slot++) {
      assert(terminal_events[round * CFG_TUH_XFER_QUEUE_DEPTH + slot].xfer_complete.len == 24 + slot * 4);
    }
  }
}

#endif

static void test_schedule_sweep(void) {
  // Exercise every descriptor slot, all intervals and frame-counter wrap.
  for (uint8_t interval = 1; interval <= 16; interval++) {
    for (uint32_t start = 16368; start < 16400; start++) {
      reset(TUSB_SPEED_HIGH);
      regs.frame_index = start & 0x3fff;
      assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, interval));
      iso_ep_t* ep = iso_ep_find(1, 0x81);
      assert(iso_xfer(0, ep, buffer, 64));
      uint32_t const due = ep->req[ep->head].scheduled_uframe;
      uint32_t const now = iso_now();
      assert((int32_t)(due - now) >= 2);
      assert((due - now) % ep->interval == 0);
      assert(ep->req[ep->head].armed == (due - now < (FRAMELIST_SIZE - 1) * 8));
      if (!ep->req[ep->head].armed) {
        assert(iso_abort(0, ep));
      } else {
        iso_td(ep, &ep->req[ep->head])->itd.xact[due & 7].active = 0;
        regs.frame_index = (due + 1) & 0x3fff;
        iso_process(true);
        assert(events == 1 && event.xfer_complete.result == XFER_RESULT_SUCCESS);
      }
    }
  }
}

static void test_descriptor_reuse(void) {
  // Revisit the same descriptor with new lengths/pages and (for HS) microframes.
  for (unsigned mode = 0; mode < 3; mode++) {
    for (unsigned dir = 0; dir < 2; dir++) {
      bool const hs = mode == 0;
      reset(mode == 1 ? TUSB_SPEED_FULL : TUSB_SPEED_HIGH);
      if (mode == 2) {
        buses[1].hub_addr = 2;
        buses[1].hub_port = 3;
        buses[2].speed = TUSB_SPEED_HIGH;
      }
      uint8_t const addr = tu_edpt_addr(1, dir);
      uint16_t const mps = hs ? 1024 : (dir ? 564 : 1023);
      assert(open_ep(addr, hs ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL,
                     hs ? mps | (2 << 11) : mps, 1));
      iso_ep_t* ep = iso_ep_find(1, addr);
      for (unsigned queue = 0; queue < CFG_TUH_XFER_QUEUE_DEPTH; queue++) {
        iso_req_t* req = &ep->req[queue];
        for (unsigned round = 0; round < 16; round++) {
          req->scheduled_uframe = round * FRAMELIST_SIZE * 8 + (hs ? round & 7 : 0);
          req->buffer = buffer + ((round & 1) ? 0 : 4095);
          req->buflen = round % 3 == 0 ? mps * ep->mult : (round % 3 == 1 ? 192 : 0);
          req->armed = false;
          iso_arm(ep, req, req->scheduled_uframe - 2);
          assert(req->armed);
          iso_td_t* td = iso_td(ep, req);
          uint32_t const ptr = (uint32_t)(uintptr_t)req->buffer;
          uint32_t const page = ptr & ~0xfffu;
          if (hs) {
            unsigned const slot = req->scheduled_uframe & 7;
            for (unsigned i = 0; i < 8; i++) {
              assert(td->itd.xact[i].active == (i == slot));
              if (i != slot) { assert(td->words[1 + i] == 0); }
            }
            assert(td->itd.xact[slot].length == req->buflen);
            assert(td->itd.xact[slot].offset == (ptr & 0xfff));
            assert(td->itd.xact[slot].page_select == 0 && td->itd.xact[slot].int_on_complete);
            assert(td->itd.BufferPointer[0] == (page | 0x101));
            assert(td->itd.BufferPointer[1] == ((page + 4096) | mps | (dir << 11)));
            assert(td->itd.BufferPointer[2] == ((page + 8192) | 3));
            // Retired hardware status, including changed page/offset and errors.
            td->words[1 + slot] = 0x7fffffff;
          } else {
            ehci_sitd_t* s = &td->sitd;
            assert(s->dev_addr == 1 && s->ep_number == 1 && s->direction == dir);
            assert(s->hub_addr == (mode == 2 ? 2 : 0));
            assert(s->port_number == (mode == 2 ? 3 : 0));
            assert(s->back.terminate && s->active && s->int_on_complete);
            assert(s->total_bytes == req->buflen && !s->cmask_progress && !s->page_select);
            assert(!s->split_state && !s->missed_uframe && !s->xact_err && !s->error);
            assert(!s->buffer_err && !s->babble_err && s->buffer[0] == ptr);
            unsigned const count = req->buflen ? (req->buflen + 187) / 188 : 1;
            assert(s->int_smask == (mode == 1 ? 0 : (dir ? 4 : (1u << count) - 1)));
            assert(s->fl_int_cmask == (mode == 2 && dir ? 0xf0 : 0));
            assert(s->buffer[1] == ((page + 4096) |
                   (mode == 2 && !dir ? count | (count > 1 ? 8 : 0) : 0)));
            td->words[3] = 0xffffff7f; // retired, all other status/progress bits set
            s->buffer[0] += 100;
            s->buffer[1] ^= 0x1f; // hardware advances OUT split count/position
          }
          req->armed = false;
          iso_bank_reclaim(ep, (req->scheduled_uframe & ~7u) + 10);
        }
      }
    }
  }
}

static void test_late_schedule_phase(void) {
  uint32_t const starts[] = {800, 16383, 0xfffffffe};
  uint32_t const delays[] = {0, 1, 19, 1031};
  for (unsigned interval = 1; interval <= 16; interval++) {
    for (unsigned s = 0; s < TU_ARRAY_SIZE(starts); s++) {
      for (unsigned d = 0; d < TU_ARRAY_SIZE(delays); d++) {
        reset(TUSB_SPEED_HIGH);
        assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, interval));
        iso_ep_t* ep = iso_ep_find(1, 0x81);
        uint32_t const now = starts[s];
        ehci_data.iso_uframe = now;
        ehci_data.iso_last_frindex = now & 0x3fff;
        regs.frame_index = now & 0x3fff;
        ep->next_uframe = now - delays[d];
        // Reference: advance one endpoint interval at a time, including wrap.
        uint32_t expected = ep->next_uframe;
        while ((int32_t)(expected - (now + 2)) < 0) {
          expected += ep->interval;
        }
        assert(iso_xfer(0, ep, buffer, 64));
        assert(ep->req[ep->head].scheduled_uframe == expected);
      }
    }
  }
}

static void test_pool_hs_interval(uint8_t interval) {
  // Walk the actual DMA chains while four endpoints reuse banks across both
  // frame-list and extended-clock wrap. This also detects orphaned active TDs.
  reset(TUSB_SPEED_HIGH);
  for (unsigned i = 0; i < CFG_TUH_EHCI_ISO_EP_MAX; i++) {
    assert(open_ep((uint8_t)(0x81 + i), TUSB_SPEED_HIGH, 64, interval));
  }
  uint32_t const start = 0xfffffff0;
  ehci_data.iso_uframe = start;
  ehci_data.iso_last_frindex = start & 0x3fff;
  regs.frame_index = start & 0x3fff;
  for (unsigned i = 0; i < CFG_TUH_EHCI_ISO_EP_MAX; i++) {
    ehci_data.iso_ep[i].next_uframe = start;
  }
  unsigned transferred = 0;
  unsigned const ticks = tu_max32(1024, (1u << (interval - 1)) * 8);
  for (unsigned tick = 0; tick < ticks; tick++) {
    uint32_t const now = start + tick;
    regs.frame_index = now & 0x3fff;
    ehci_link_t link = ehci_data.period_framelist[(now >> 3) % FRAMELIST_SIZE];
    unsigned visited = 0, completed = 0;
    while (!link.terminate && link.type != EHCI_QTYPE_QHD) {
      assert(link.type == EHCI_QTYPE_ITD);
      assert(++visited <= CFG_TUH_EHCI_ISO_EP_MAX * ISO_TD_BANK_COUNT * CFG_TUH_XFER_QUEUE_DEPTH);
      iso_td_t* td = (iso_td_t*)(uintptr_t)tu_align32(link.address);
      if (td->itd.xact[now & 7].active) {
        unsigned owners = 0;
        for (unsigned i = 0; i < CFG_TUH_EHCI_ISO_EP_MAX; i++) {
          iso_ep_t* ep = &ehci_data.iso_ep[i];
          for (unsigned j = 0; j < ep->count; j++) {
            iso_req_t* req = &ep->req[(ep->head + j) % CFG_TUH_XFER_QUEUE_DEPTH];
            if (req->armed && iso_td(ep, req) == td) {
              assert(req->scheduled_uframe == now);
              owners++;
            }
          }
        }
        assert(owners == 1);
        td->itd.xact[now & 7].active = 0;
        td->itd.xact[now & 7].length = 24;
        completed++;
      }
      link = td->itd.next;
    }
    assert(link.terminate || link.type == EHCI_QTYPE_QHD);
    events = 0;
    iso_process(true);
    assert(events == completed);
    for (unsigned i = 0; i < events; i++) {
      assert(terminal_events[i].xfer_complete.result == XFER_RESULT_SUCCESS);
      assert(terminal_events[i].xfer_complete.len == 24);
    }
    transferred += completed;
    for (unsigned i = 0; i < CFG_TUH_EHCI_ISO_EP_MAX; i++) {
      iso_ep_t* ep = &ehci_data.iso_ep[i];
      while (ep->count < CFG_TUH_XFER_QUEUE_DEPTH) {
        assert(iso_xfer(0, ep, buffer + i * 128 + ep->count * 64, 24));
      }
    }
  }
#if CFG_TUH_XFER_QUEUE_DEPTH > 1
  if (interval == 1) {
    assert(transferred == (ticks - 2) * CFG_TUH_EHCI_ISO_EP_MAX);
  }
#endif
  assert(transferred != 0);
}

static void test_pool_fs_interval(uint8_t interval) {
  for (unsigned hub = 0; hub < 2; hub++) {
    reset(hub ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL);
    for (unsigned i = 0; i < CFG_TUH_EHCI_ISO_EP_MAX; i++) {
      if (hub) {
        buses[1].hub_addr = 2;
        buses[1].hub_port = 3;
        buses[2].speed = TUSB_SPEED_HIGH;
      }
      assert(open_ep((uint8_t)(0x81 + i), TUSB_SPEED_FULL, 64, interval));
    }
    uint32_t const start = iso_now();
    unsigned transferred = 0;
    unsigned const ticks = tu_max32(512, (1u << (interval - 1)) * 64);
    for (unsigned tick = 0; tick < ticks; tick += hub ? 1 : 8) {
      uint32_t const now = start + tick;
      regs.frame_index = (now + (hub ? 0 : 8)) & 0x3fff;
      unsigned completed = 0;
      if (!hub || (now & 7) == 7) {
        ehci_link_t link = ehci_data.period_framelist[(now >> 3) % FRAMELIST_SIZE];
        unsigned visited = 0;
        while (!link.terminate && link.type != EHCI_QTYPE_QHD) {
          assert(link.type == EHCI_QTYPE_SITD);
          assert(++visited <= CFG_TUH_EHCI_ISO_EP_MAX * ISO_TD_BANK_COUNT * CFG_TUH_XFER_QUEUE_DEPTH);
          iso_td_t* td = (iso_td_t*)(uintptr_t)tu_align32(link.address);
          if (td->sitd.active) {
            unsigned owners = 0;
            for (unsigned i = 0; i < CFG_TUH_EHCI_ISO_EP_MAX; i++) {
              iso_ep_t* ep = &ehci_data.iso_ep[i];
              for (unsigned j = 0; j < ep->count; j++) {
                iso_req_t* req = &ep->req[(ep->head + j) % CFG_TUH_XFER_QUEUE_DEPTH];
                if (req->armed && iso_td(ep, req) == td) {
                  assert(req->scheduled_uframe == (now & ~7u));
                  owners++;
                }
              }
            }
            assert(owners == 1);
            td->sitd.active = 0;
            td->sitd.total_bytes = 0;
            completed++;
          }
          link = td->itd.next;
        }
      }
      events = 0;
      iso_process(true);
      if (events != completed) {
        fprintf(stderr, "FS pool hub=%u tick=%u now=%u completed=%u events=%u result=%u\n",
                hub, tick, now, completed, events, event.xfer_complete.result);
      }
      assert(events == completed);
      for (unsigned i = 0; i < events; i++) {
        assert(terminal_events[i].xfer_complete.result == XFER_RESULT_SUCCESS);
      }
      transferred += completed;
      for (unsigned i = 0; i < CFG_TUH_EHCI_ISO_EP_MAX; i++) {
        iso_ep_t* ep = &ehci_data.iso_ep[i];
        while (ep->count < CFG_TUH_XFER_QUEUE_DEPTH) {
          assert(iso_xfer(0, ep, buffer + i * 128 + ep->count * 64, 64));
        }
      }
    }
#if CFG_TUH_XFER_QUEUE_DEPTH > 1
    if (interval == 1) {
      assert(transferred == 63 * CFG_TUH_EHCI_ISO_EP_MAX);
    }
#endif
    assert(transferred != 0);
  }
}

static void test_pool_stream(void) {
  for (uint8_t interval = 1; interval <= 16; interval++) {
    test_pool_hs_interval(interval);
  }
}

static void test_pool_fs_stream(void) {
  for (uint8_t interval = 1; interval <= 16; interval++) {
    test_pool_fs_interval(interval);
  }
}

static void test_bank_reclamation(void) {
  reset(TUSB_SPEED_HIGH);
  ehci_link_t const original = ehci_data.period_framelist[4]; // frame start 800
  assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, 1));
  assert(open_ep(0x82, TUSB_SPEED_HIGH, 64, 1));
  iso_ep_t* first = iso_ep_find(1, 0x81);
  iso_ep_t* second = iso_ep_find(1, 0x82);
  assert(iso_xfer(0, first, buffer, 24));
  assert(iso_xfer(0, second, buffer + 64, 24));
  unsigned const first_bank = first->req[0].bank;
  unsigned const second_bank = second->req[0].bank;
  iso_td(first, &first->req[0])->itd.xact[2].active = 0;
  first->req[0].armed = false;
  first->count = 0;
  iso_bank_reclaim(first, 809);
  assert(first->td_frame[first_bank] == 800); // grace has not elapsed
  iso_bank_reclaim(first, 810);
  assert(first->td_frame[first_bank] == UINT32_MAX);
  // Removing a bank in the middle preserves its predecessor and the QH tail.
  assert(iso_td(second, &second->req[0])->itd.xact[2].active);
  assert(ehci_data.iso_td[1][second_bank][0].itd.next.address == original.address);
  iso_bank_reclaim(second, 810);
  assert(second->td_frame[second_bank] == 800); // outstanding request pins the bank
  iso_td(second, &second->req[0])->itd.xact[2].active = 0;
  second->req[0].armed = false;
  second->count = 0;
  iso_bank_reclaim(second, 867);
  assert(second->td_frame[second_bank] == 800); // same entry is being revisited
  iso_bank_reclaim(second, 874);
  assert(second->td_frame[second_bank] == UINT32_MAX);
  assert(ehci_data.period_framelist[4].address == original.address);
}

static void test_future_completion(void) {
  uint32_t const starts[] = {800, 0xfffffff8};
  for (unsigned mode = 0; mode < 3; mode++) {
    for (unsigned i = 0; i < TU_ARRAY_SIZE(starts); i++) {
      reset(mode == 1 ? TUSB_SPEED_FULL : TUSB_SPEED_HIGH);
      if (mode == 2) {
        buses[1].hub_addr = 2;
        buses[1].hub_port = 1;
        buses[2].speed = TUSB_SPEED_HIGH;
      }
      assert(open_ep(0x81, mode == 0 ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL, 64, 1));
      iso_ep_t* ep = iso_ep_find(1, 0x81);
      ep->next_uframe = starts[i];
      ehci_data.iso_uframe = starts[i] + ehci_data.iso_frame_offset;
      regs.frame_index = ehci_data.iso_last_frindex = ehci_data.iso_uframe & 0x3fff;
      assert(iso_xfer(0, ep, buffer, 64));
      iso_req_t* req = &ep->req[ep->head];
      assert(req->armed);
      uint32_t const due = req->scheduled_uframe;
      regs.frame_index = (due - 1 + ehci_data.iso_frame_offset) & 0x3fff;
      iso_process(true);
      assert(events == 0 && ep->count == 1);

      // Once the interval starts, inspect status and allow immediate completion.
      regs.frame_index = (due + ehci_data.iso_frame_offset) & 0x3fff;
      iso_process(true);
      assert(events == 0);
      iso_td_t* td = iso_td(ep, req);
      if (mode == 0) {
        td->itd.xact[due & 7].active = 0;
      } else {
        td->sitd.active = 0;
        td->sitd.total_bytes = 0;
      }
      iso_process(true);
      assert(events == 1 && ep->count == 0);
      assert(event.xfer_complete.result == XFER_RESULT_SUCCESS && event.xfer_complete.len == 64);
    }
  }
}

#endif

int main(void) {
  // Hardware links are 32-bit. The runner places static fixtures below 4 GiB.
  assert((uintptr_t)&ehci_data <= UINT32_MAX && (uintptr_t)buffer <= UINT32_MAX);
  test_attach_debounce();
  test_qtd_retirement();
#if CFG_TUH_EHCI_ISO_EP_MAX
  test_native_fs();
  test_split();
  test_split_audio();
  test_hs();
  test_iso_status_errors();
  test_iso_clock_config();
  test_long_interval_and_wrap();
  test_limits_and_late_completion();
  test_schedule_sweep();
  test_descriptor_reuse();
  test_late_schedule_phase();
  test_pool_stream();
  test_pool_fs_stream();
  test_bank_reclamation();
  test_future_completion();
#if CFG_TUH_XFER_QUEUE_DEPTH > 1
  test_queue();
#endif
#endif
  puts("EHCI ISO regression tests passed");
  return 0;
}
