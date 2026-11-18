// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "tusb_option.h"
#include "common/tusb_common.h"

// The QH software tail contains a native pointer; see test_iso.c.
#pragma push_macro("TU_VERIFY_STATIC")
#undef TU_VERIFY_STATIC
#define TU_VERIFY_STATIC(condition, ...) \
  enum { TU_XSTRCAT(_verify_static_, TU_COUNTER) = 1 / (!!((condition) || sizeof(void*) == 8)) }
#include "portable/ehci/ehci.h"
#include "portable/ehci/ehci.c"
#pragma pop_macro("TU_VERIFY_STATIC")

TU_VERIFY_STATIC(offsetof(ehci_qhd_t, qtd_overlay) == 16, "QH hardware prefix");

#define QTD_CAPACITY (5 * 4096u)

extern unsigned test_cache_calls[3];

static ehci_registers_t regs;
static ehci_cap_registers_t caps;
static tuh_bus_info_t buses[CFG_TUH_DEVICE_MAX + CFG_TUH_HUB + 1];
static hcd_event_t event;
static unsigned events;
static uint8_t buffer[0x10000 + 4096] TU_ATTR_ALIGNED(4096);

// Payload invalidates since the last reset, in order.
static struct {
  uintptr_t addr;
  uint32_t size;
} invalidates[8];
static unsigned invalidate_count;
static unsigned cache_seen[3];

void test_cache_buffer(void const* addr, uint32_t size) {
  uintptr_t const start = (uintptr_t) addr;
  uintptr_t const descriptors = (uintptr_t) &ehci_data;
  assert(start + size <= descriptors || start >= descriptors + sizeof(ehci_data));
  if (test_cache_calls[1] != cache_seen[1]) {
    assert(invalidate_count < TU_ARRAY_SIZE(invalidates));
    invalidates[invalidate_count].addr = start;
    invalidates[invalidate_count].size = size;
    invalidate_count++;
  }
  memcpy(cache_seen, test_cache_calls, sizeof(cache_seen));
}

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
  events++;
}

static void reset(void) {
  memset(&ehci_data, 0, sizeof(ehci_data));
  memset((void*) &regs, 0, sizeof(regs));
  memset((void*) &caps, 0, sizeof(caps));
  memset(buses, 0, sizeof(buses));
  ehci_data.regs = &regs;
  ehci_data.cap_regs = &caps;
  regs.portsc = (uint32_t) TUSB_SPEED_HIGH << 26;
  init_periodic_list(0);
  ehci_qhd_t* async_head = list_get_async_head(0);
  async_head->next.address = (uint32_t) (uintptr_t) async_head | (EHCI_QTYPE_QHD << 1);
  async_head->head_list_flag = 1;
  async_head->qtd_overlay.halted = 1;
  events = 0;
  invalidate_count = 0;
  memcpy(cache_seen, test_cache_calls, sizeof(cache_seen));
}

static ehci_qhd_t* open_ep(uint8_t daddr, uint8_t ep_addr, uint8_t xfer, uint16_t mps, uint8_t speed) {
  buses[daddr].speed = speed;
  tusb_desc_endpoint_t const desc = {
    .bLength = sizeof(desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = (uint8_t) (xfer == TUSB_XFER_CONTROL ? 0 : ep_addr), .bmAttributes = {.xfer = xfer},
    .wMaxPacketSize = mps, .bInterval = (uint8_t) (xfer == TUSB_XFER_INTERRUPT ? 1 : 0)
  };
  assert(hcd_edpt_open(0, daddr, &desc));
  ehci_qhd_t* qhd = edpt_find(daddr, ep_addr).qhd;
  assert(qhd != NULL);
  return qhd;
}

// Chunk the driver is expected to schedule at addr with remaining bytes left.
static uint32_t expected_chunk(uintptr_t addr, uint32_t remaining, uint16_t mps) {
  uint32_t const capacity = QTD_CAPACITY - (addr & 0xfffu);
  return remaining <= capacity ? remaining : capacity - capacity % mps;
}

// Load the attached qTD into the overlay as the controller does (EHCI 4.10.2).
static void hc_fetch(ehci_qhd_t* qhd) {
  ehci_qtd_t* qtd = qhd->attached_qtd;
  assert(qhd->qtd_overlay.next.address == (uint32_t) (uintptr_t) qtd);
  uint8_t const toggle = qhd->qtd_overlay.data_toggle;
  uint8_t const ping = qhd->qtd_overlay.ping_err;
  qhd->qtd_overlay = *qtd;
  if (!qhd->data_toggle_control) {
    qhd->qtd_overlay.data_toggle = toggle;
  }
  if (qhd->ep_speed == TUSB_SPEED_HIGH) {
    qhd->qtd_overlay.ping_err = ping;
  }
  qhd->qtd_addr = (uint32_t) (uintptr_t) qtd;
}

// Retire the attached qTD after moving `moved` bytes and write it back.
static void hc_retire(ehci_qhd_t* qhd, uint16_t moved, bool halt, bool xact_err) {
  ehci_qtd_t* qtd = qhd->attached_qtd;
  assert(qtd != NULL && qtd->active);
  if (!qhd->qtd_overlay.active) {
    hc_fetch(qhd);
  }
  uint16_t const mps = qhd->max_packet_size;
  uint32_t const packets = moved ? (moved + mps - 1u) / mps : (qtd->expected_bytes == 0);
  volatile ehci_qtd_t* overlay = &qhd->qtd_overlay;
  overlay->total_bytes = qtd->expected_bytes - moved;
  overlay->data_toggle ^= packets & 1u;
  overlay->halted = halt;
  overlay->xact_err = xact_err;
  overlay->active = 0;
  qtd->total_bytes = overlay->total_bytes;
  qtd->data_toggle = overlay->data_toggle;
  qtd->halted = halt;
  qtd->xact_err = xact_err;
  qtd->active = 0;
}

// The completion scan of hcd_int_handler(). Generic EHCI's 256-entry frame list
// has no interval heads past 8 ms, so the scan stops there.
static void run_isr(void) {
  proccess_async_xfer_isr(list_get_async_head(0));
  for (uint32_t i = 1; i <= 8; i *= 2) {
    process_period_xfer_isr(0, i);
  }
}

// Submit len bytes at offset and retire every chunk in full; check each chunk's
// placement, PID, toggle and pages, then the single completion.
static void check_chunks(uint8_t xfer, uint8_t ep_addr, uint16_t mps, uint32_t offset, uint16_t len) {
  reset();
  uint8_t const daddr = 1;
  ehci_qhd_t* qhd = open_ep(daddr, ep_addr, xfer, mps, TUSB_SPEED_HIGH);
  uint8_t* const start = buffer + offset;
  assert(hcd_edpt_xfer(0, daddr, ep_addr, start, len));
  ehci_qtd_t* const qtd = qhd->attached_qtd;
  uint8_t const pid = tu_edpt_dir(ep_addr) ? EHCI_PID_IN : EHCI_PID_OUT;

  uint32_t done = 0;
  unsigned chunks = 0;
  uint8_t toggle = 1; // control data stage starts with DATA1
  for (;;) {
    uintptr_t const addr = (uintptr_t) start + done;
    uint32_t const chunk = expected_chunk(addr, len - done, mps);
    assert(qhd->attached_qtd == qtd && qtd->used && qtd->active);
    assert(qtd->buffer[0] == addr && qhd->attached_buffer == addr);
    for (unsigned i = 1; i < 5; i++) {
      assert(qtd->buffer[i] == tu_align4k((uint32_t) addr) + 4096u * i);
    }
    assert(qtd->expected_bytes == chunk && qtd->total_bytes == chunk);
    assert(chunk <= QTD_CAPACITY && (done + chunk == len || chunk % mps == 0));
    assert(qtd->pid == pid && qtd->int_on_complete && qtd->next.terminate);
    if (xfer == TUSB_XFER_CONTROL) {
      assert(qtd->data_toggle == toggle);
    }
    assert(qhd->xferred == done && qhd->remaining == len - done - chunk);
    chunks++;

    hc_retire(qhd, (uint16_t) chunk, false, false);
    toggle = qhd->qtd_overlay.data_toggle;
    run_isr();
    done += chunk;
    if (done == len) {
      break;
    }
    assert(events == 0);
  }
  assert(chunks >= (len + QTD_CAPACITY - 1) / QTD_CAPACITY);
  assert(events == 1 && event.event_id == HCD_EVENT_XFER_COMPLETE);
  assert(event.dev_addr == daddr && event.xfer_complete.ep_addr == ep_addr);
  assert(event.xfer_complete.result == XFER_RESULT_SUCCESS && event.xfer_complete.len == len);
  assert(qhd->attached_qtd == NULL && !qtd->used && qhd->remaining == 0 && qhd->xferred == 0);
  if (tu_edpt_dir(ep_addr) && len) {
    // one invalidate at submission and one over the whole received range at completion
    assert(invalidate_count == 2);
    assert(invalidates[1].addr == (uintptr_t) start && invalidates[1].size == len);
  } else {
    assert(invalidate_count == (tu_edpt_dir(ep_addr) ? 1u : 0u));
  }
  // nothing left to complete
  run_isr();
  assert(events == 1);
}

static void test_chunk_lengths(void) {
  static uint32_t const offsets[] = {0, 1, 4095};
  static struct {
    uint8_t xfer;
    uint16_t mps;
  } const eps[] = {
    {TUSB_XFER_BULK, 512}, {TUSB_XFER_BULK, 64}, {TUSB_XFER_INTERRUPT, 1000}, {TUSB_XFER_CONTROL, 64},
  };
  for (size_t o = 0; o < TU_ARRAY_SIZE(offsets); o++) {
    uint32_t const capacity = QTD_CAPACITY - offsets[o];
    uint32_t const lengths[] = {0, 16384, capacity - 1, capacity, capacity + 1, 24576, 32768, 0xfc00, 0xffff};
    for (size_t e = 0; e < TU_ARRAY_SIZE(eps); e++) {
      for (size_t l = 0; l < TU_ARRAY_SIZE(lengths); l++) {
        for (uint8_t dir = 0; dir < 2; dir++) {
          uint8_t const ep_addr = (uint8_t) ((eps[e].xfer == TUSB_XFER_CONTROL ? 0 : 1) | (dir ? 0x80 : 0));
          check_chunks(eps[e].xfer, ep_addr, eps[e].mps, offsets[o], (uint16_t) lengths[l]);
        }
      }
    }
  }
}

// Start a three-chunk bulk IN transfer and retire the first chunk in full.
static ehci_qhd_t* start_three_chunks(void) {
  reset();
  ehci_qhd_t* qhd = open_ep(1, 0x81, TUSB_XFER_BULK, 512, TUSB_SPEED_HIGH);
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 0xc000));
  assert(qhd->attached_qtd->expected_bytes == QTD_CAPACITY);
  hc_retire(qhd, QTD_CAPACITY, false, false);
  run_isr();
  assert(events == 0 && qhd->attached_qtd->active);
  return qhd;
}

static void test_short_and_error(void) {
  // Short packet in the second chunk ends the transfer.
  ehci_qhd_t* qhd = start_three_chunks();
  ehci_qtd_t* qtd = qhd->attached_qtd;
  hc_retire(qhd, 700, false, false);
  run_isr();
  assert(events == 1 && event.xfer_complete.result == XFER_RESULT_SUCCESS);
  assert(event.xfer_complete.len == QTD_CAPACITY + 700);
  assert(invalidate_count == 2 && invalidates[1].addr == (uintptr_t) buffer &&
         invalidates[1].size == QTD_CAPACITY + 700);
  assert(!qtd->used && qhd->remaining == 0 && qhd->xferred == 0);

  // Transaction error in the second chunk: bytes so far, halt cleared.
  qhd = start_three_chunks();
  hc_retire(qhd, 1024, true, true);
  run_isr();
  assert(events == 1 && event.xfer_complete.result == XFER_RESULT_FAILED);
  assert(event.xfer_complete.len == QTD_CAPACITY + 1024 && !qhd->qtd_overlay.halted);
  assert(invalidates[1].size == QTD_CAPACITY + 1024);

  // STALL in the third chunk keeps the endpoint halted.
  qhd = start_three_chunks();
  hc_retire(qhd, QTD_CAPACITY, false, false);
  run_isr();
  assert(events == 0 && qhd->attached_qtd->expected_bytes == 0xc000 - 2 * QTD_CAPACITY);
  assert(qhd->attached_qtd->buffer[0] == (uintptr_t) buffer + 2 * QTD_CAPACITY);
  hc_retire(qhd, 0, true, false);
  run_isr();
  assert(events == 1 && event.xfer_complete.result == XFER_RESULT_STALLED);
  assert(event.xfer_complete.len == 2 * QTD_CAPACITY && qhd->qtd_overlay.halted);
  assert(invalidates[1].addr == (uintptr_t) buffer && invalidates[1].size == 2 * QTD_CAPACITY);
}

// Control setup, then a data stage whose first chunk ends on an odd packet count.
static void test_control_toggle(void) {
  reset();
  ehci_qhd_t* qhd = open_ep(1, 0, TUSB_XFER_CONTROL, 64, TUSB_SPEED_HIGH);
  static uint8_t const setup[8] = {0};
  assert(hcd_setup_send(0, 1, setup));
  ehci_qtd_t* qtd = qhd->attached_qtd;
  assert(qtd->pid == EHCI_PID_SETUP && qhd->remaining == 0 && qhd->xferred == 0);
  hc_retire(qhd, 8, false, false);
  run_isr();
  assert(events == 1 && event.xfer_complete.len == 8);

  assert(hcd_edpt_xfer(0, 1, 0x80, buffer + 1, 32768));
  assert(qtd->data_toggle == 1 && qtd->expected_bytes == 20416); // 319 packets
  hc_retire(qhd, 20416, false, false);
  run_isr();
  assert(qtd->active && qtd->pid == EHCI_PID_IN && qtd->data_toggle == 0);
  hc_retire(qhd, 32768 - 20416, false, false);
  run_isr();
  assert(events == 2 && event.xfer_complete.len == 32768 && event.xfer_complete.ep_addr == 0x80);
}

// A closed endpoint is not continued, reports nothing, and is reclaimed with
// its counters cleared.
static void test_close_mid_chain(bool periodic, bool retired) {
  reset();
  uint8_t const ep_addr = periodic ? 0x82 : 0x81;
  uint8_t const xfer = periodic ? TUSB_XFER_INTERRUPT : TUSB_XFER_BULK;
  ehci_qhd_t* qhd = open_ep(1, ep_addr, xfer, 512, TUSB_SPEED_HIGH);
  assert(hcd_edpt_xfer(0, 1, ep_addr, buffer, 0xc000));
  ehci_qtd_t* qtd = qhd->attached_qtd;
  if (retired) {
    hc_retire(qhd, QTD_CAPACITY, false, false);
  }
  assert(hcd_edpt_close(0, 1, ep_addr) && qhd->removing);
  run_isr();
  assert(events == 0 && qtd->used && qtd->active == !retired && qtd->expected_bytes == QTD_CAPACITY);

  // A direct visit does not continue a removing QH either.
  if (retired) {
    qhd_xfer_complete_isr(qhd);
    assert(qtd->active == 0 && qhd->attached_qtd == NULL);
    assert(events == 1 && event.xfer_complete.len == QTD_CAPACITY);
    events = 0;
  }
  if (periodic) {
    regs.frame_index += 9;
    (void) qtd_find_free();
  } else {
    async_advance_isr(0);
  }
  assert(!qhd->used && !qtd->used && qhd->remaining == 0 && qhd->xferred == 0);
  assert(events == 0);
}

int main(void) {
  assert((uintptr_t) &ehci_data <= UINT32_MAX && (uintptr_t) buffer + sizeof(buffer) <= UINT32_MAX);
  test_chunk_lengths();
  test_short_and_error();
  test_control_toggle();
  for (int i = 0; i < 4; i++) {
    test_close_mid_chain(i & 1, i & 2);
  }
  puts("EHCI transfer tests passed");
  return 0;
}
