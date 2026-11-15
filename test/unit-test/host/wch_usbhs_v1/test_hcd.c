// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "tusb_option.h"
#include "common/tusb_common.h"

// Software scheduling/DMA fixture; does not emulate wire timing or W1C writes.
static struct {
  uint8_t CONTROL, HOST_CTRL, INT_EN, DEV_AD;
  uint16_t FRAME_NO;
  uint8_t SPEED_TYPE, MIS_ST, INT_FG, INT_ST;
  uint16_t RX_LEN;
  uint32_t HOST_EP_CONFIG, HOST_EP_TYPE, HOST_RX_DMA, HOST_TX_DMA;
  uint16_t HOST_RX_MAX_LEN, HOST_TX_LEN, HOST_SPLIT_DATA;
  uint8_t HOST_EP_PID, HOST_RX_CTRL, HOST_TX_CTRL;
} regs;
static TU_ATTR_ALIGNED(4) uint8_t buffer[4096];
static uintptr_t dma_address(void const *ptr);
#define USBHS_DMA_ADDRESS(ptr) dma_address(ptr)
#define USBHSH (&regs)
#define USBHS_IRQn 0
#define NVIC_EnableIRQ(irq) ((void)(irq))
#define NVIC_DisableIRQ(irq) ((void)(irq))
#if CFG_TUH_HUB && TUH_OPT_HIGH_SPEED
static uint32_t irq_state;
static unsigned irq_depth, pair_polls;
static bool pair_ready;
static uint32_t irq_save(void);
static void irq_restore(uint32_t state);
static bool split_ready(void);
#define USBHS_IRQ_SAVE() irq_save()
#define USBHS_IRQ_RESTORE(state) irq_restore(state)
#define USBHS_SPLIT_DONE() split_ready()
#endif
#include "portable/wch/usbhs_v1/hcd_wch_usbhs_v1.c"

static tuh_bus_info_t buses[128];
static hcd_event_t event;
static unsigned events;

// Map fixture objects to 32-bit DMA register values and back.
static uintptr_t dma_address(void const *ptr) {
  uintptr_t const addr = (uintptr_t)ptr;
  if (addr >= (uintptr_t)buffer && addr < (uintptr_t)buffer + sizeof(buffer)) {
    return 0x20000000u + addr - (uintptr_t)buffer;
  }
  if (addr >= (uintptr_t)&_hcd && addr < (uintptr_t)&_hcd + sizeof(_hcd)) {
    return 0x20010000u + addr - (uintptr_t)&_hcd;
  }
  return addr;
}
static void *dma_pointer(uint32_t addr) {
  if (addr >= 0x20010000u) {
    assert(addr - 0x20010000u < sizeof(_hcd));
    return (uint8_t *)&_hcd + addr - 0x20010000u;
  }
  assert(addr >= 0x20000000u && addr - 0x20000000u < sizeof(buffer));
  return buffer + addr - 0x20000000u;
}
#if CFG_TUH_HUB && TUH_OPT_HIGH_SPEED
static uint32_t irq_save(void) {
  uint32_t const old = irq_state;
  irq_state &= ~0x88u;
  assert(irq_depth++ == 0);
  return old;
}
static void irq_restore(uint32_t state) {
  assert(irq_depth-- == 1);
  irq_state = state;
}
static bool split_ready(void) {
  assert(irq_depth == 1 && !(irq_state & 0x88));
  assert(regs.HOST_EP_PID == 0x80 && regs.HOST_TX_CTRL == USBHS_UH_T_DATA_NO);
  pair_polls++;
  return pair_ready;
}
#endif
// No hub control/defer mocks: every matrix variant must link without them.
bool tuh_bus_info_get(uint8_t addr, tuh_bus_info_t *bus) { *bus = buses[addr]; return true; }
void tusb_time_delay_ms_api(uint32_t ms) { (void)ms; }
void hcd_event_handler(hcd_event_t const *e, bool in_isr) { (void)in_isr; event = *e; events++; }
static void reset(tusb_speed_t speed) {
  memset(&_hcd, 0, sizeof(_hcd));
  memset(&regs, 0, sizeof(regs));
  memset(buses, 0, sizeof(buses));
  memset(buffer, 0xa5, sizeof(buffer));
  _hcd.active = _hcd.last = -1;
#if CFG_TUH_HUB && TUH_OPT_HIGH_SPEED
  _hcd.split_owner = _hcd.periodic_owner = -1;
  irq_state = 0x1088;
  irq_depth = pair_polls = 0;
  pair_ready = true;
#endif
  _hcd.attached = true;
  _hcd.root_speed = speed;
  regs.MIS_ST = USBHS_SIE_FREE | USBHS_ATTACH;
  events = 0;
  for (unsigned i = 0; i < TU_ARRAY_SIZE(buses); i++) { buses[i].speed = speed; }
}
static bool open_ep(uint8_t addr, uint8_t endpoint, uint8_t type, uint16_t mps, uint8_t interval) {
  tusb_desc_endpoint_t d = { .bLength = sizeof(d), .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = endpoint, .bmAttributes = { .xfer = type },
    .wMaxPacketSize = mps, .bInterval = interval };
  return hcd_edpt_open(0, addr, &d);
}
static void tick(uint32_t uf) {
  uint16_t const raw = (uint16_t)uf & 0x3fff;
  regs.FRAME_NO = (uint16_t)((raw >> 3) | ((raw & 7) << 13));
  regs.INT_FG = USBHS_HST_SOF_FLAG;
  hcd_int_handler(0, true);
}
static void done(uint8_t status, uint16_t received) {
  assert(_hcd.active >= 0);
  regs.INT_ST = status;
  regs.RX_LEN = received;
  transaction_done(true);
}
static void receive(uint8_t value, uint16_t len) {
  assert(len <= regs.HOST_RX_MAX_LEN);
  memset(dma_pointer(regs.HOST_RX_DMA), value, len);
}
static void test_dma_and_round_robin(void) {
  reset(TUSB_SPEED_HIGH);
  assert(open_ep(1, 0x81, TUSB_XFER_BULK, 512, 0));
  assert(open_ep(2, 1, TUSB_XFER_BULK, 512, 0));
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 516));
  assert(regs.HOST_RX_DMA == dma_address(buffer) && regs.HOST_RX_MAX_LEN == 512);
  assert(hcd_edpt_xfer(0, 2, 1, buffer + 1025, 8)); // TX has no4-byte requirement
  receive(0x21, 512);
  done(USBHS_TOGGLE_OK | 3, 512);
  assert(regs.DEV_AD == 2 && regs.HOST_TX_DMA == dma_address(buffer + 1025));
  done(USBHS_PID_ACK, 0);
  assert(regs.DEV_AD == 1 && regs.HOST_RX_DMA == dma_address(buffer + 512));
  assert(regs.HOST_RX_MAX_LEN == 4);
  receive(0x22, 4);
  done(USBHS_TOGGLE_OK | 0xb, 4);
  assert(events == 2 && event.xfer_complete.len == 516);
  assert(buffer[511] == 0x21 && buffer[512] == 0x22 && buffer[516] == 0xa5);

  reset(TUSB_SPEED_FULL);
  assert(open_ep(1, 0x81, TUSB_XFER_INTERRUPT, 3, 1));
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 3));
  receive(7, 3); done(USBHS_TOGGLE_OK | 3, 3);
  assert(event.xfer_complete.len == 3 && buffer[3] == 0xa5);
}
static void test_native_ping_and_rx_failure(void) {
  reset(TUSB_SPEED_HIGH);
  assert(open_ep(1, 1, TUSB_XFER_BULK, 512, 0));
  assert(hcd_edpt_xfer(0, 1, 1, buffer, 1024));
  done(USBHS_PID_NYET, 0);
  assert(ep_find(1, 1)->done == 512 && regs.HOST_EP_PID >> 4 == USBHS_PID_PING);
  assert(regs.HOST_TX_CTRL & USBHS_UH_T_DATA_NO);
  done(USBHS_PID_ACK, 0);
  assert(regs.HOST_EP_PID >> 4 == USBHS_PID_OUT && regs.HOST_TX_DMA == dma_address(buffer + 512));
  done(USBHS_PID_ACK, 0);
  assert(events == 1 && event.xfer_complete.len == 1024);
  assert(open_ep(2, 0x81, TUSB_XFER_BULK, 512, 0));
  assert(hcd_edpt_xfer(0, 2, 0x81, buffer, 4));
  assert(regs.HOST_RX_MAX_LEN == 4);
  done(USBHS_TOGGLE_OK | 3, 5); // reported overrun must not clamp-and-succeed
  assert(events == 2 && event.xfer_complete.result == XFER_RESULT_FAILED && event.xfer_complete.len == 0);
  // Hardware truncation/RX_LEN semantics are a capture question, not emulated here.
}

static void test_setup_zlp_cancel(void) {
  reset(TUSB_SPEED_FULL);
  assert(open_ep(0, 0, TUSB_XFER_CONTROL, 64, 0));
  uint8_t setup[8] = { 0x80, 6, 0, 1, 0, 0, 18, 0 };
  assert(hcd_setup_send(0, 0, setup));
  setup[1] = 0;
  assert(((uint8_t *)dma_pointer(regs.HOST_TX_DMA))[1] == 6);
  assert(regs.HOST_TX_LEN == 8);
  done(USBHS_PID_ACK, 0);
  assert(hcd_edpt_xfer(0, 0, 0x80, buffer, 18));
  assert(regs.HOST_RX_MAX_LEN == 18);
  receive(0x42, 18); done(USBHS_TOGGLE_OK | 0xb, 18);
  assert(buffer[18] == 0xa5 && event.xfer_complete.len == 18);
  assert(hcd_edpt_xfer(0, 0, 0, NULL, 0));
  assert(regs.HOST_TX_LEN == 0 && dma_pointer(regs.HOST_TX_DMA) == ep_find(0, 0)->setup);
  done(USBHS_PID_ACK, 0);
  assert(open_ep(1, 1, TUSB_XFER_BULK, 64, 0));
  assert(hcd_edpt_xfer(0, 1, 1, buffer, 64));
  regs.INT_FG = 0;
  unsigned const count = events;
  assert(hcd_edpt_abort_xfer(0, 1, 1) && _hcd.active == -1 && events == count);
  hcd_device_close(0, 0);
  assert(ep_find(1, 1) && !ep_find(0, 0));
}
static void test_native_iso(tusb_speed_t speed) {
  reset(speed);
  uint16_t const mps = speed == TUSB_SPEED_HIGH ? 1024 : 1023;
  uint32_t const interval = speed == TUSB_SPEED_HIGH ? 2 : 16;
  assert(open_ep(1, 0x81, TUSB_XFER_ISOCHRONOUS, mps, 2));
  assert(ep_find(1, 0x81)->interval == interval);
  assert(!open_ep(1, 0x82, TUSB_XFER_ISOCHRONOUS, 1024 | 0x800, 1));
  assert(open_ep(1, 0x82, TUSB_XFER_ISOCHRONOUS, mps, 17));
  assert(ep_find(1, 0x82)->interval == (speed == TUSB_SPEED_HIGH ? 1u : 8u) << 15);
  assert(hcd_edpt_close(0, 1, 0x82));
  assert(!hcd_edpt_xfer(0, 1, 0x81, buffer, mps + 1));
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, mps));
  assert(regs.HOST_EP_TYPE == USBHS_ENDP_RX_ISO && regs.HOST_RX_CTRL == USBHS_UH_R_RES_NO);
  assert(regs.HOST_RX_MAX_LEN == mps);
  receive(0x66, mps); done(USBHS_TOGGLE_OK | 3, mps);
  assert(event.xfer_complete.result == XFER_RESULT_SUCCESS && event.xfer_complete.len == mps);
  assert(!ep_find(1, 0x81)->toggle && buffer[mps] == 0xa5);
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 8));
  assert(_hcd.active == -1); // no second transaction in the service interval
  tick(interval);
  done(USBHS_PID_NAK, 0);
  assert(events == 2 && event.xfer_complete.result == XFER_RESULT_FAILED && _hcd.active == -1);
  assert(!ep_find(1, 0x81)->ping && !ep_find(1, 0x81)->toggle);
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 8));
  tick(2 * interval);
  done(0xb, 8); // unverified alternate-PID DMA retention is not accepted
  assert(events == 3 && event.xfer_complete.result == XFER_RESULT_FAILED);

  assert(open_ep(2, 1, TUSB_XFER_ISOCHRONOUS, mps, 1));
  assert(hcd_edpt_xfer(0, 2, 1, buffer + 1, mps));
  assert(regs.HOST_EP_TYPE == USBHS_ENDP_TX_ISO && regs.HOST_TX_CTRL == USBHS_UH_T_RES_NO);
  assert(regs.HOST_TX_LEN == mps && !(regs.HOST_TX_CTRL & USBHS_UH_T_DATA_NO));
  done(USBHS_TOGGLE_OK | 0x13, 0); // synchronous OUT retains the preceding RX status
  assert(event.xfer_complete.len == mps && event.xfer_complete.result == XFER_RESULT_SUCCESS);
  assert(hcd_edpt_xfer(0, 2, 1, NULL, 0));
  tick(2 * interval + (speed == TUSB_SPEED_HIGH ? 1 : 8));
  assert(regs.HOST_TX_LEN == 0 && !(regs.HOST_TX_CTRL & USBHS_UH_T_DATA_NO));
  done(USBHS_TOGGLE_OK | 0x1b, 0);
  assert(event.xfer_complete.result == XFER_RESULT_SUCCESS && !ep_find(2, 1)->toggle);
}
static void test_iso_cancel_interval(tusb_speed_t speed) {
  reset(speed);
  uint32_t const interval = speed == TUSB_SPEED_HIGH ? 1 : 8;
  assert(open_ep(1, 1, TUSB_XFER_ISOCHRONOUS, 64, 1));
  assert(hcd_edpt_xfer(0, 1, 1, buffer, 8));
  regs.INT_FG = 0;
  assert(hcd_edpt_abort_xfer(0, 1, 1));
  assert(hcd_edpt_xfer(0, 1, 1, buffer, 8));
  assert(_hcd.active == -1); // cancelling a launched packet does not refund its slot
  tick(interval);
  assert(_hcd.active >= 0);
  done(0, 0);

  reset(speed);
  assert(open_ep(1, 1, TUSB_XFER_ISOCHRONOUS, 64, 1));
  assert(open_ep(2, 1, TUSB_XFER_BULK, 64, 0));
  assert(hcd_edpt_xfer(0, 2, 1, buffer, 8));
  assert(hcd_edpt_xfer(0, 1, 1, buffer + 64, 8));
  assert(hcd_edpt_abort_xfer(0, 1, 1)); // queued ISO was never launched
  done(USBHS_PID_ACK, 0);
  assert(hcd_edpt_xfer(0, 1, 1, buffer + 64, 8));
  assert(_hcd.active >= 0 && regs.DEV_AD == 1);
  done(0, 0);
}

static void test_iso_completion_at_boundary(void) {
  reset(TUSB_SPEED_HIGH);
  assert(open_ep(1, 1, TUSB_XFER_ISOCHRONOUS, 64, 1));
  tick(5);
  assert(hcd_edpt_xfer(0, 1, 1, buffer, 8));
  assert(_hcd.active_started == 5);
  // Launch read the physical counter after software time was sampled. A fast
  // completion must not replace the reserved deadline with that stale time.
  _hcd.now = _hcd.frame_raw = 4;
  done(0, 0);
  assert(hcd_edpt_xfer(0, 1, 1, buffer, 8));
  assert(_hcd.now == 5 && _hcd.active == -1 && ep_find(1, 1)->due == 6);
  tick(6);
  assert(_hcd.active >= 0);
}

static void test_iso_missing_completion(void) {
  reset(TUSB_SPEED_FULL);
  assert(open_ep(1, 0x81, TUSB_XFER_ISOCHRONOUS, 1023, 1));
  assert(open_ep(2, 1, TUSB_XFER_BULK, 64, 0));
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 1023));
  assert(hcd_edpt_xfer(0, 2, 1, buffer + 2048, 64));
  tick(8);
  assert(events == 0 && regs.DEV_AD == 1);
  tick(16);
  assert(events == 1 && event.xfer_complete.result == XFER_RESULT_FAILED && event.xfer_complete.len == 0);
  assert(regs.DEV_AD == 2 && !ep_find(1, 0x81)->pending);
  done(USBHS_PID_ACK, 0);
  assert(events == 2);
}
#if CFG_TUH_HUB && TUH_OPT_HIGH_SPEED
static void child(uint8_t addr) { buses[addr].speed = TUSB_SPEED_FULL; buses[addr].hub_addr = 5; buses[addr].hub_port = 1; }
static void test_split_dma_and_cancel(void) {
  reset(TUSB_SPEED_HIGH);
  child(0); child(1);
  assert(open_ep(5, 0, TUSB_XFER_CONTROL, 64, 0));
  assert(open_ep(0, 0, TUSB_XFER_CONTROL, 64, 0));
  assert(open_ep(2, 0x83, TUSB_XFER_BULK, 512, 0));
  hcd_device_close(0, 0); assert(ep_find(5, 0) && ep_find(2, 0x83));
  assert(!open_ep(1, 0x81, TUSB_XFER_ISOCHRONOUS, 64, 1));
  assert(open_ep(1, 0x81, TUSB_XFER_BULK, 64, 0));
  assert(open_ep(2, 1, TUSB_XFER_BULK, 512, 0));
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 18));
  assert(irq_state == 0x1088 && irq_depth == 0 && pair_polls == 1);
  done(USBHS_PID_ACK, 0);
  assert(hcd_edpt_xfer(0, 2, 1, buffer + 512, 8)); done(USBHS_PID_ACK, 0);
  tick(2);
  assert(regs.HOST_EP_TYPE == USBHS_ENDP_RX_ISO && regs.HOST_RX_MAX_LEN == 18);
  done(USBHS_PID_NYET, 0); tick(3);
  receive(0x32, 18); done(USBHS_TOGGLE_OK | 3, 18);
  assert(events == 2 && event.xfer_complete.len == 18 && buffer[18] == 0xa5);
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 18)); done(USBHS_PID_ACK, 0); tick(5);
  regs.INT_FG = 0;
  assert(hcd_edpt_abort_xfer(0, 1, 0x81));
  assert(_hcd.active == -1 && _hcd.split_owner == -1 && !ep_find(1, 0x81)->pending);
  assert(!hcd_edpt_xfer(0, 1, 0x81, buffer, 18)); // downstream toggle now unknown
  assert(hcd_edpt_close(0, 1, 0x81));
  assert(open_ep(1, 0x81, TUSB_XFER_BULK, 64, 0)); // no global TT quarantine
  hcd_device_close(0, 5);
  assert(!ep_find(5, 0) && !ep_find(1, 0x81) && ep_find(2, 0x83));
}
static void test_split_periodic_and_watchdog(void) {
  reset(TUSB_SPEED_HIGH); child(1);
  assert(open_ep(1, 0x81, TUSB_XFER_INTERRUPT, 8, 1));
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 8)); done(0, 0);
  tick(2); done(USBHS_PID_NYET, 0);
  tick(3); done(USBHS_PID_MDATA, 4);
  assert(events == 1 && event.xfer_complete.result == XFER_RESULT_FAILED && event.xfer_complete.len == 0);
  assert(_hcd.periodic_owner == -1 && _hcd.split_blocked_until == 8);

  reset(TUSB_SPEED_HIGH); child(1);
  assert(open_ep(1, 0x81, TUSB_XFER_BULK, 64, 0));
  assert(hcd_edpt_xfer(0, 1, 0x81, buffer, 64)); done(USBHS_PID_ACK, 0);
  tick(2); _hcd.active_started = _hcd.now + 1;
  split_maintenance(true); assert(ep_find(1, 0x81)->errors == 0);
  tick(4); assert(ep_find(1, 0x81)->errors == 0);
  tick(5); tick(7); tick(9);
  assert(events == 1 && event.xfer_complete.result == XFER_RESULT_FAILED && _hcd.split_owner == -1);
  assert(_hcd.active == -1); // no control requests/deferred work linked into HCD
}
#endif
int main(void) {
  assert(host_speed_bits(TUSB_SPEED_AUTO) == (TUH_OPT_HIGH_SPEED ? USBHS_HIGH_SPEED : USBHS_FULL_SPEED));
  test_dma_and_round_robin();
  test_setup_zlp_cancel();
  test_native_ping_and_rx_failure();
  test_native_iso(TUSB_SPEED_FULL);
  test_native_iso(TUSB_SPEED_HIGH);
  test_iso_completion_at_boundary();
  test_iso_missing_completion();
  test_iso_cancel_interval(TUSB_SPEED_FULL);
  test_iso_cancel_interval(TUSB_SPEED_HIGH);
#if CFG_TUH_HUB && TUH_OPT_HIGH_SPEED
  test_split_dma_and_cancel();
  test_split_periodic_and_watchdog();
#endif
  puts("WCH USBHS v1 direct DMA / ISO scheduling tests passed");
  return 0;
}
