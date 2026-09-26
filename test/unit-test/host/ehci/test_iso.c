// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "tusb_option.h"
#include "common/tusb_common.h"

// The QH software tail and ISO state contain native pointers. On a 64-bit test
// host their sizes differ from the target ABI. Recheck hardware layouts below.
#undef TU_VERIFY_STATIC
#define TU_VERIFY_STATIC(condition, ...) \
  _Static_assert((condition) || sizeof(void*) == 8, "EHCI ABI")
#include "portable/ehci/ehci.h"
#include "portable/ehci/ehci.c"
#undef TU_VERIFY_STATIC
#define TU_VERIFY_STATIC(condition, ...) _Static_assert(condition, __VA_ARGS__)

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
  events = 0;
}

static void test_endpoint_lookup(void) {
  reset(TUSB_SPEED_HIGH);
  // Same endpoint number in both directions, and the same address on another device.
  uint8_t const dev[] = {1, 1, 2};
  uint8_t const addr[] = {1, 0x81, 0x81};
  ehci_qhd_t* qhd[3];
  for (unsigned i = 0; i < TU_ARRAY_SIZE(dev); i++) {
    buses[dev[i]].speed = TUSB_SPEED_HIGH;
    tusb_desc_endpoint_t const desc = {
      .bLength = sizeof(desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
      .bEndpointAddress = addr[i], .bmAttributes = {.xfer = TUSB_XFER_INTERRUPT},
      .wMaxPacketSize = 64, .bInterval = 1
    };
    assert(hcd_edpt_open(0, dev[i], &desc));
    qhd[i] = &ehci_data.qhd_pool[i].qhd;
    ehci_edpt_t const found = edpt_find(dev[i], addr[i]);
    assert(found.qhd == qhd[i]);
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
    assert(found.iso == NULL);
#endif
  }
  for (unsigned i = 0; i < TU_ARRAY_SIZE(ehci_data.control); i++) {
    assert(edpt_find(i, 0).qhd == qhd_control(i));
    assert(edpt_find(i, 0x80).qhd == qhd_control(i));
  }
  assert(edpt_find(1, 0x82).qhd == NULL);
  qhd[1]->removing = 1;
  assert(edpt_find(1, 0x81).qhd == NULL);
  qhd[1]->removing = 0;
  qhd[1]->used = 0;
  assert(edpt_find(1, 0x81).qhd == NULL); // A stale key cannot match a free slot.
  assert(edpt_find(1, 1).qhd == qhd[0]);
  assert(edpt_find(2, 0x81).qhd == qhd[2]);
}

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
#define TEST_ISO_STREAM_EP_COUNT 4

static iso_ep_t* iso_ep_find(uint8_t daddr, uint8_t ep_addr) {
  return edpt_find(daddr, ep_addr).iso;
}

static ehci_qhd_t* qhd_get_from_addr(uint8_t daddr, uint8_t ep_addr) {
  return edpt_find(daddr, ep_addr).qhd;
}

static bool open_ep(uint8_t addr, uint8_t speed, uint16_t size, uint8_t interval) {
  buses[1].speed = speed;
  tusb_desc_endpoint_t desc = {
    .bLength = sizeof(desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = addr, .bmAttributes = {.xfer = TUSB_XFER_ISOCHRONOUS},
    .wMaxPacketSize = size, .bInterval = interval
  };
  return iso_ep_open(0, 1, &desc);
}

static void test_shared_pools(void) {
  reset(TUSB_SPEED_HIGH);
  buses[1].speed = TUSB_SPEED_HIGH;
  tusb_desc_endpoint_t const intr_desc = {
    .bLength = sizeof(intr_desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x83, .bmAttributes = {.xfer = TUSB_XFER_INTERRUPT},
    .wMaxPacketSize = 64, .bInterval = 1
  };
  assert(hcd_edpt_open(0, 1, &intr_desc));
  assert(hcd_edpt_xfer(0, 1, 0x83, buffer, 64));
  ehci_qhd_t* qhd = qhd_get_from_addr(1, 0x83);
  ehci_qtd_t* qtd = qhd->attached_qtd;
  assert(qtd == &ehci_data.qtd_pool[0].qtd);

  // ISO uses one free QH for its TD and one free qTD for its state.
  assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, 1));
  iso_ep_t* ep = iso_ep_find(1, 0x81);
  assert(qhd_get_from_addr(1, 0x81) == NULL);
  assert(qtd_find_free() == &ehci_data.qtd_pool[2].qtd);
  assert(ep->td == &ehci_data.qhd_pool[1].iso);
  assert(((uintptr_t) ep->td & 63) == 0);
  // Async reclamation must not interpret an ISO descriptor as a QH.
  iso_ep_t const saved = *ep;
  async_advance_isr(0);
  assert(memcmp(ep, &saved, sizeof(saved)) == 0);
  assert(qhd_get_from_addr(1, 0x83) == qhd);

  // Release unpublished ISO descriptors and reuse their space as qTDs.
  iso_ep_free(ep);
  assert(iso_ep_find(1, 0x81) == NULL);
  ehci_qtd_t* second = qtd_find_free();
  qtd_init(second, buffer + 64, 32);
  ehci_qtd_t* recycled = qtd_find_free();
  assert(recycled == &ehci_data.qtd_pool[2].qtd);
  qtd_init(recycled, buffer + 128, 16);
  assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, 1));
  assert(iso_ep_find(1, 0x81) == &ehci_data.qtd_pool[3].iso);
  assert(iso_ep_find(1, 0x81)->td == &ehci_data.qhd_pool[1].iso);
  assert(second->active && recycled->active && qtd->active);
  qtd->active = 0;
  qtd->total_bytes = 0;
  qhd_xfer_complete_isr(qhd);
  assert(events == 1 && event.xfer_complete.len == 64 && !qtd->used);
  assert(second->active && recycled->active);

  // Exhaust the TD pool and check that failed open releases its QH slot.
  reset(TUSB_SPEED_HIGH);
  for (size_t i = 0; i < QTD_MAX; i++) {
    qtd = qtd_find_free();
    assert(qtd != NULL);
    qtd_init(qtd, buffer, 64);
  }
  assert(qtd_find_free() == NULL);
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 64, 1));
  assert(iso_ep_find(1, 0x81) == NULL);
  assert(qhd_find_free() == &ehci_data.qhd_pool[0].qhd);
  for (size_t i = 0; i < TU_ARRAY_SIZE(ehci_data.qtd_is_iso); i++) {
    assert(!ehci_data.qtd_is_iso[i]);
  }
  for (size_t i = 0; i < QTD_MAX; i++) {
    qtd = &ehci_data.qtd_pool[i].qtd;
    assert(qtd->used && qtd->active && qtd->expected_bytes == 64);
  }
}

static void test_native_fs(void) {
  reset(TUSB_SPEED_FULL);
  assert(open_ep(0x81, TUSB_SPEED_FULL, 1023, 1));
  iso_ep_t* ep = iso_ep_find(1, 0x81);
  assert(iso_xfer(0, ep, buffer + 4090, 1023));
  ehci_sitd_t* td = &ep->td->sitd;
  assert(ep->uframe == 800);
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
  td = &ep->td->sitd;
  td->active = 0;
  regs.frame_index += 8;
  iso_process(true);
  assert(events == 2 && event.xfer_complete.len == 0);
  assert(event.xfer_complete.result == XFER_RESULT_SUCCESS);
}

static void test_sparse_iso_pool(void) {
  reset(TUSB_SPEED_HIGH);
  // Keep the first 31 QHs/qTDs occupied by other endpoint types. ISO lookup and
  // completion must handle both sides of the ownership bitmap boundary.
  _Static_assert(QHD_MAX > 32, "fixture must exercise multiple bitmap words");
  for (size_t i = 0; i < 31; i++) {
    ehci_data.qhd_pool[i].qhd.used = 1;
    ehci_data.qtd_pool[i].qtd.used = 1;
  }
  assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, 1));
  assert(open_ep(0x82, TUSB_SPEED_HIGH, 64, 1));
  iso_ep_t* first = iso_ep_find(1, 0x81);
  iso_ep_t* second = iso_ep_find(1, 0x82);
  assert(first == &ehci_data.qtd_pool[31].iso);
  assert(second == &ehci_data.qtd_pool[32].iso);
  assert(iso_xfer(0, first, buffer, 64));
  assert(iso_xfer(0, second, buffer + 64, 64));
  first->td->itd.xact[first->uframe & 7].active = 0;
  second->td->itd.xact[second->uframe & 7].active = 0;
  regs.frame_index = first->uframe;
  iso_process(true);
  assert(events == 2 && !first->busy && !second->busy);
  assert(terminal_events[0].xfer_complete.ep_addr == 0x81);
  assert(terminal_events[1].xfer_complete.ep_addr == 0x82);
  iso_ep_free(first);
  assert(iso_ep_find(1, 0x81) == NULL);
  assert(iso_ep_find(1, 0x82) == second);
  assert(qhd_find_free() == &ehci_data.qhd_pool[31].qhd);
  iso_ep_free(second);
  assert(iso_ep_next(0) == QHD_MAX);
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
  ehci_sitd_t* td = &ep->td->sitd;
  assert(td->hub_addr == 3 && td->port_number == 4);
  assert(td->int_smask == 4 && td->fl_int_cmask == 0xf0);
  assert(open_ep(1, TUSB_SPEED_FULL, 1023, 1));
  ep = iso_ep_find(1, 1);
  assert(iso_xfer(0, ep, buffer, 1023));
  td = &ep->td->sitd;
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
  ehci_sitd_t* in_td = &in->td->sitd;
  ehci_sitd_t* out_td = &out->td->sitd;
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
  assert(!iso_xfer(0, ep, buffer, 64));
  assert(ep->buffer == buffer + 4095);
  ehci_itd_t* td = &ep->td->itd;
  uint8_t slot = ep->uframe & 7;
  assert(ep->uframe == 801);
  assert(((uintptr_t)td & 63) == 0);
  assert(td->xact[slot].offset == 4095 && td->xact[slot].length == 3072);
  assert((td->BufferPointer[0] & 0xfff) == 0x201);
  assert((td->BufferPointer[1] & 0xfff) == 0xc00);
  assert((td->BufferPointer[2] & 0xfff) == 3);
  assert((td->BufferPointer[0] & ~0xfffu) == (uint32_t)(uintptr_t)buffer);
  assert((td->BufferPointer[1] & ~0xfffu) == (uint32_t)(uintptr_t)(buffer + 4096));
  // Even the largest transfer at the last byte of a page only needs two pages.
  assert(td->BufferPointer[2] == 3);
  for (unsigned i = 0; i < 8; i++) {
    assert(td->xact[i].active == (i == slot));
  }
  td->xact[slot].active = 0;
  td->xact[slot].length = 2048;
  regs.frame_index = 803;
  iso_process(true);
  assert(events == 1 && event.xfer_complete.len == 2048);
  assert(iso_xfer(0, ep, buffer, 10));
  td = &ep->td->itd;
  slot = ep->uframe & 7;
  td->xact[slot].active = 0;
  td->xact[slot].babble_err = 1;
  regs.frame_index = ep->uframe + 1;
  iso_process(true);
  assert(events == 2 && event.xfer_complete.result == XFER_RESULT_FAILED);
  assert(event.xfer_complete.len == 0);
}

static void test_long_interval_and_wrap(void) {
  reset(TUSB_SPEED_HIGH);
  assert(open_ep(1, TUSB_SPEED_HIGH, 64, 10));
  iso_ep_t* ep = iso_ep_find(1, 1);
  assert(iso_xfer(0, ep, buffer, 64));
  assert(!ep->armed && ep->uframe == 1312);
  regs.frame_index = 1264;
  iso_process(true);
  assert(ep->armed && events == 0);
  ep->td->itd.xact[0].active = 0;
  regs.frame_index = 1313;
  iso_process(true);
  assert(events == 1 && event.xfer_complete.len == 64);
  assert(iso_xfer(0, ep, buffer, 64));
  assert(!ep->armed && iso_stop(0, ep, false));
  assert(!ep->busy && events == 1);
  // Start a separate long interval request and let its arm window expire.
  ep->uframe = regs.frame_index + ep->interval;
  assert(iso_xfer(0, ep, buffer, 64));
  regs.frame_index = ep->uframe + 1;
  iso_process(true);
  assert(!ep->busy && events == 2 && event.xfer_complete.result == XFER_RESULT_FAILED);
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
      iso_td_t* td = ep->td;
      // Active work must remain queued, even if status bits are already set.
      if (hs) {
        unsigned const slot = ep->uframe & 7;
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
      regs.frame_index = (ep->uframe + ehci_data.iso_frame_offset) & 0x3fff;
      iso_process(true);
      assert(events == 1 && event.xfer_complete.result == XFER_RESULT_FAILED);
      assert(event.xfer_complete.len == 0);
    }
  }
}

static void test_iso_clock_config(void) {
  static uint16_t const due[] = {801, 802, 803, 804, 805, 806, 807, 808,
                                808, 808, 808, 808, 808, 808, 808, 808};
  for (unsigned threshold = 0; threshold < TU_ARRAY_SIZE(due); threshold++) {
    reset(TUSB_SPEED_HIGH);
    caps.hccparams_bm.iso_schedule_threshold = threshold;
    assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, 1));
    assert(iso_earliest(iso_now()) == due[threshold]);
    // Model completed teardown; this fixture cannot emulate the DMA stop/start handshake.
    iso_ep_free(iso_ep_find(1, 0x81));
    regs.inten &= ~EHCI_INT_MASK_NXP_SOF;
    init_periodic_list(0);
    // A new root connection must refresh both cached scheduling attributes.
    regs.portsc = (uint32_t) TUSB_SPEED_FULL << 26;
    caps.hccparams_bm.iso_schedule_threshold = 0;
    assert(open_ep(0x81, TUSB_SPEED_FULL, 64, 1));
    assert(iso_now() == 792 && iso_earliest(iso_now()) == 793);
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

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
static void test_limits_and_late_completion(void) {
  reset(TUSB_SPEED_HIGH);
  assert(!open_ep(0x80, TUSB_SPEED_HIGH, 64, 1));
  assert(!open_ep(0x81, TUSB_SPEED_LOW, 64, 1));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 64, 0));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 64, 17));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 0, 1));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 1025, 1));
  assert(!open_ep(0x81, TUSB_SPEED_HIGH, 64 | (3 << 11), 1));
  // Every slot, including the odd final slot, can serve an ISO endpoint.
  for (unsigned i = 0; i <= QTD_MAX; i++) {
    uint8_t const daddr = (uint8_t)(i / 30 + 1);
    buses[daddr].speed = TUSB_SPEED_HIGH;
    tusb_desc_endpoint_t const desc = {
      .bLength = sizeof(desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
      .bEndpointAddress = tu_edpt_addr(i % 15 + 1, (i % 30) / 15),
      .bmAttributes = {.xfer = TUSB_XFER_ISOCHRONOUS},
      .wMaxPacketSize = 64, .bInterval = 1
    };
    assert(iso_ep_open(0, daddr, &desc) == (i < QTD_MAX));
    if (i < QTD_MAX) {
      iso_td_t* td = iso_ep_find(daddr, desc.bEndpointAddress)->td;
      assert(((uintptr_t)td & 63) == 0); // Every iTD stays within a 4 KiB page.
    }
  }
  assert(qhd_find_free() == NULL && qtd_find_free() == NULL);
  iso_ep_t* ep = iso_ep_find(1, 1);
  assert(iso_xfer(0, ep, buffer, 64));
  ep->td->itd.xact[ep->uframe & 7].active = 0;
  regs.frame_index = (ep->uframe & ~7u) + FRAMELIST_SIZE * 8u;
  iso_process(true);
  assert(events == 1 && event.xfer_complete.result == XFER_RESULT_FAILED);
  assert(!ep->busy);
  for (size_t i = 0; i < QTD_MAX; i++) {
    iso_ep_free(&ehci_data.qtd_pool[i].iso);
  }
  assert(iso_ep_next(0) == QTD_MAX);
  assert(qhd_find_free() == &ehci_data.qhd_pool[0].qhd);
  assert(qtd_find_free() == &ehci_data.qtd_pool[0].qtd);
}


static void test_schedule_sweep(void) {
  // Exercise every descriptor slot, all intervals and frame-counter wrap.
  for (uint8_t interval = 1; interval <= 16; interval++) {
    for (uint32_t start = 16368; start < 16400; start++) {
      reset(TUSB_SPEED_HIGH);
      regs.frame_index = start & 0x3fff;
      assert(open_ep(0x81, TUSB_SPEED_HIGH, 64, interval));
      iso_ep_t* ep = iso_ep_find(1, 0x81);
      assert(iso_xfer(0, ep, buffer, 64));
      uint32_t const due = ep->uframe;
      uint32_t const now = iso_now();
      assert((int32_t)(due - now) >= 1);
      assert((due - now) % ep->interval == 0);
      assert(ep->armed == (due - now < (FRAMELIST_SIZE - 1) * 8));
      if (!ep->armed) {
        assert(iso_stop(0, ep, false));
      } else {
        ep->td->itd.xact[due & 7].active = 0;
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
      for (unsigned round = 0; round < 16; round++) {
        ep->uframe = round * FRAMELIST_SIZE * 8 + (hs ? round & 7 : 0);
        ep->buffer = buffer + ((round & 1) ? 0 : 4095);
        ep->buflen = round % 3 == 0 ? mps * (hs ? 3 : 1) : (round % 3 == 1 ? 192 : 0);
        ep->armed = false;
        iso_arm(ep, ep->uframe - 2);
        assert(ep->armed);
        iso_td_t* td = ep->td;
        uint32_t const ptr = (uint32_t)(uintptr_t)ep->buffer;
        uint32_t const page = ptr & ~0xfffu;
        if (hs) {
          unsigned const slot = ep->uframe & 7;
          for (unsigned i = 0; i < 8; i++) {
            assert(td->itd.xact[i].active == (i == slot));
            if (i != slot) { assert(td->words[1 + i] == 0); }
          }
          assert(td->itd.xact[slot].length == ep->buflen);
          assert(td->itd.xact[slot].offset == (ptr & 0xfff));
          assert(td->itd.xact[slot].page_select == 0 && td->itd.xact[slot].int_on_complete);
          assert(td->itd.BufferPointer[0] == (page | 0x101));
          assert(td->itd.BufferPointer[1] == ((page + 4096) | mps | (dir << 11)));
          assert(td->itd.BufferPointer[2] == 3);
          // Retired hardware status, including changed page/offset and errors.
          td->words[1 + slot] = 0x7fffffff;
        } else {
          ehci_sitd_t* s = &td->sitd;
          assert(s->dev_addr == 1 && s->ep_number == 1 && s->direction == dir);
          assert(s->hub_addr == (mode == 2 ? 2 : 0));
          assert(s->port_number == (mode == 2 ? 3 : 0));
          assert(s->back.terminate && s->active && s->int_on_complete);
          assert(s->total_bytes == ep->buflen && !s->cmask_progress && !s->page_select);
          assert(!s->split_state && !s->missed_uframe && !s->xact_err && !s->error);
          assert(!s->buffer_err && !s->babble_err && s->buffer[0] == ptr);
          unsigned const count = ep->buflen ? (ep->buflen + 187) / 188 : 1;
          assert(s->int_smask == (mode == 1 ? 0 : (dir ? 4 : (1u << count) - 1)));
          assert(s->fl_int_cmask == (mode == 2 && dir ? 0xf0 : 0));
          assert(s->buffer[1] == ((page + 4096) |
                 (mode == 2 && !dir ? count | (count > 1 ? 8 : 0) : 0)));
          td->words[3] = 0xffffff7f; // retired, all other status/progress bits set
          s->buffer[0] += 100;
          s->buffer[1] ^= 0x1f; // hardware advances OUT split count/position
        }
        iso_td_unlink(ep);
        ep->armed = false;
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
        ep->uframe = now - delays[d];
        // Reference: advance one endpoint interval at a time, including wrap.
        uint32_t expected = ep->uframe;
        while ((int32_t)(expected - (now + 1)) < 0) {
          expected += ep->interval;
        }
        assert(iso_xfer(0, ep, buffer, 64));
        assert(ep->uframe == expected);
      }
    }
  }
}

static void test_pool_hs_interval(uint8_t interval) {
  // Walk the actual DMA chains while four endpoints reuse TDs across both
  // frame-list and extended-clock wrap. This also detects orphaned active TDs.
  reset(TUSB_SPEED_HIGH);
  for (unsigned i = 0; i < TEST_ISO_STREAM_EP_COUNT; i++) {
    assert(open_ep((uint8_t)(0x81 + i), TUSB_SPEED_HIGH, 64, interval));
  }
  uint32_t const start = 0xfffffff0;
  ehci_data.iso_uframe = start;
  ehci_data.iso_last_frindex = start & 0x3fff;
  regs.frame_index = start & 0x3fff;
  for (unsigned i = 0; i < TEST_ISO_STREAM_EP_COUNT; i++) {
    ehci_data.qtd_pool[i].iso.uframe = start;
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
      assert(++visited <= TEST_ISO_STREAM_EP_COUNT);
      iso_td_t* td = (iso_td_t*)(uintptr_t)tu_align32(link.address);
      if (td->itd.xact[now & 7].active) {
        unsigned owners = 0;
        for (unsigned i = 0; i < TEST_ISO_STREAM_EP_COUNT; i++) {
          iso_ep_t* ep = &ehci_data.qtd_pool[i].iso;
          if (ep->busy && ep->armed && ep->td == td) {
            assert(ep->uframe == now);
            owners++;
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
    for (unsigned i = 0; i < TEST_ISO_STREAM_EP_COUNT; i++) {
      iso_ep_t* ep = &ehci_data.qtd_pool[i].iso;
      if (!ep->busy) {
        assert(iso_xfer(0, ep, buffer + i * 128, 24));
      }
    }
  }
  // Immediate completion/refill sustains every microframe even with one TD.
  if (interval == 1) {
    assert(transferred == (ticks - 1) * TEST_ISO_STREAM_EP_COUNT);
  }
  assert(transferred != 0);
}

static void test_pool_fs_interval(uint8_t interval) {
  for (unsigned hub = 0; hub < 2; hub++) {
    reset(hub ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL);
    for (unsigned i = 0; i < TEST_ISO_STREAM_EP_COUNT; i++) {
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
          assert(++visited <= TEST_ISO_STREAM_EP_COUNT);
          iso_td_t* td = (iso_td_t*)(uintptr_t)tu_align32(link.address);
          if (td->sitd.active) {
            unsigned owners = 0;
            for (unsigned i = 0; i < TEST_ISO_STREAM_EP_COUNT; i++) {
              iso_ep_t* ep = &ehci_data.qtd_pool[i].iso;
              if (ep->busy && ep->armed && ep->td == td) {
                assert(ep->uframe == (now & ~7u));
                owners++;
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
      for (unsigned i = 0; i < TEST_ISO_STREAM_EP_COUNT; i++) {
        iso_ep_t* ep = &ehci_data.qtd_pool[i].iso;
        if (!ep->busy) {
          assert(iso_xfer(0, ep, buffer + i * 128, 64));
        }
      }
    }
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

static void test_completion_unlink(bool hs) {
  reset(TUSB_SPEED_HIGH);
  if (!hs) {
    buses[1].hub_addr = 2;
    buses[1].hub_port = 1;
    buses[2].speed = TUSB_SPEED_HIGH;
  }
  uint32_t const due = hs ? 801 : 808;
  size_t const frame = (due >> 3) % FRAMELIST_SIZE;
  unsigned const status_word = hs ? 1 + (due & 7) : 3;
  uint32_t const active = TU_BIT(hs ? 31 : 7);
  unsigned const type = hs ? EHCI_QTYPE_ITD : EHCI_QTYPE_SITD;
  ehci_link_t const original = ehci_data.period_framelist[frame];
  assert(open_ep(0x81, hs ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL, 64, 1));
  assert(open_ep(0x82, hs ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL, 64, 1));
  iso_ep_t* first = iso_ep_find(1, 0x81);
  iso_ep_t* second = iso_ep_find(1, 0x82);
  assert(iso_xfer(0, first, buffer, 24));
  assert(iso_xfer(0, second, buffer + 64, 24));
  iso_td_t* first_td = first->td;
  iso_td_t* second_td = second->td;
  first_td->words[status_word] &= ~active;
  regs.frame_index = due;
  iso_process(true);
  assert(events == 1 && !first->busy && !first->armed);
  // Completion removes a middle TD immediately, preserving the active sibling.
  assert(second_td->words[status_word] & active);
  assert(second_td->itd.next.address == original.address);
  assert(ehci_data.period_framelist[frame].address == ((uint32_t)(uintptr_t)second_td | (type << 1)));

  uint32_t const next_due = hs ? 802 : 816;
  size_t const next_frame = (next_due >> 3) % FRAMELIST_SIZE;
  ehci_link_t const next_original = hs ? original : ehci_data.period_framelist[next_frame];
  assert(iso_xfer(0, first, buffer, 24));
  iso_td_t* replacement = first->td;
  assert(first->armed && first->uframe == next_due);
  second_td->words[status_word] &= ~active;
  iso_process(true);
  assert(events == 2 && !second->busy);
  // Preserve the replacement, including when it precedes the completed HS TD.
  unsigned const next_status_word = hs ? 1 + (next_due & 7) : 3;
  assert(replacement->words[next_status_word] & active);
  assert(replacement->itd.next.address == next_original.address);
  replacement->words[next_status_word] &= ~active;
  regs.frame_index = next_due;
  iso_process(true);
  assert(events == 3 && !first->busy);
  assert(ehci_data.period_framelist[frame].address == original.address);
  assert(ehci_data.period_framelist[next_frame].address == next_original.address);
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
      ep->uframe = starts[i];
      ehci_data.iso_uframe = starts[i] + ehci_data.iso_frame_offset;
      regs.frame_index = ehci_data.iso_last_frindex = ehci_data.iso_uframe & 0x3fff;
      assert(iso_xfer(0, ep, buffer, 64));
      assert(ep->armed);
      uint32_t const due = ep->uframe;
      regs.frame_index = (due - 1 + ehci_data.iso_frame_offset) & 0x3fff;
      iso_process(true);
      assert(events == 0 && ep->busy);

      // Once the interval starts, inspect status and allow immediate completion.
      regs.frame_index = (due + ehci_data.iso_frame_offset) & 0x3fff;
      iso_process(true);
      assert(events == 0);
      iso_td_t* td = ep->td;
      if (mode == 0) {
        td->itd.xact[due & 7].active = 0;
      } else {
        td->sitd.active = 0;
        td->sitd.total_bytes = 0;
      }
      iso_process(true);
      assert(events == 1 && !ep->busy);
      assert(event.xfer_complete.result == XFER_RESULT_SUCCESS && event.xfer_complete.len == 64);
    }
  }
}

#endif

int main(void) {
  // Hardware links are 32-bit. The runner places static fixtures below 4 GiB.
  assert((uintptr_t)&ehci_data <= UINT32_MAX && (uintptr_t)buffer <= UINT32_MAX);
  test_endpoint_lookup();
  test_qtd_retirement();
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  test_shared_pools();
  test_sparse_iso_pool();
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
  test_completion_unlink(true);
  test_completion_unlink(false);
  test_future_completion();
#else
  reset(TUSB_SPEED_HIGH);
  tusb_desc_endpoint_t const iso_desc = {
    .bLength = sizeof(iso_desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x81, .bmAttributes = {.xfer = TUSB_XFER_ISOCHRONOUS},
    .wMaxPacketSize = 64, .bInterval = 1
  };
  assert(!hcd_edpt_open(0, 1, &iso_desc));
  assert(regs.command_bm.int_threshold == 8);
#endif
  puts("EHCI ISO regression tests passed");
  return 0;
}
