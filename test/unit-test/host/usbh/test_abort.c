// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "host/usbh.c"

static bool abort_result;
static bool close_result;
static unsigned abort_calls;
static unsigned submissions;
static unsigned completions;
static uint8_t buffer[64];

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* init) {
  (void) rhport;
  (void) init;
  return true;
}
void hcd_int_enable(uint8_t rhport) { (void) rhport; }
void hcd_int_disable(uint8_t rhport) { (void) rhport; }
void hcd_int_handler(uint8_t rhport, bool in_isr) { (void) rhport; (void) in_isr; }
uint32_t hcd_frame_number(uint8_t rhport) { (void) rhport; return 0; }
bool hcd_port_connect_status(uint8_t rhport) { (void) rhport; return true; }
void hcd_port_reset(uint8_t rhport) { (void) rhport; }
void hcd_port_reset_end(uint8_t rhport) { (void) rhport; }
tusb_speed_t hcd_port_speed_get(uint8_t rhport) { (void) rhport; return TUSB_SPEED_HIGH; }
void hcd_device_close(uint8_t rhport, uint8_t daddr) { (void) rhport; (void) daddr; }
bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, const tusb_desc_endpoint_t* ep) {
  (void) rhport; (void) daddr; (void) ep;
  return true;
}
bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport; (void) daddr; (void) ep_addr;
  return close_result;
}
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup[8]) {
  (void) rhport; (void) daddr; (void) setup;
  return true;
}
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport; (void) daddr; (void) ep_addr;
  return true;
}
bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t* data, uint16_t len) {
  assert(rhport == 0 && daddr == 1 && (ep_addr == 1 || ep_addr == 0x81));
  assert(data == buffer && len == sizeof(buffer));
  submissions++;
  return true;
}
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  assert(rhport == 0 && daddr == 1 && (ep_addr == 1 || ep_addr == 0x81));
  abort_calls++;
  return abort_result;
}
uint32_t tusb_time_millis_api(void) { return 0; }
void tusb_time_delay_ms_api(uint32_t ms) { (void) ms; }

static void complete(tuh_xfer_t* xfer) {
  assert(xfer->result == XFER_RESULT_SUCCESS && xfer->actual_len == sizeof(buffer));
  assert(xfer->user_data == 0x1234);
  completions++;
}

static void test_abort(uint8_t ep_addr) {
  abort_calls = submissions = completions = 0;
  abort_result = false;
  tusb_rhport_init_t const init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_HIGH};
  assert(tuh_rhport_init(0, &init));
  // Install an enumerated device; this fixture tests transfer ownership only.
  usbh_device_t* dev = get_device(1);
  dev->connected = dev->addressed = dev->configured = 1;
  dev->bus_info.rhport = 0;
  dev->bus_info.speed = TUSB_SPEED_HIGH;
  tuh_xfer_t xfer = {
    .daddr = 1, .ep_addr = ep_addr, .buffer = buffer, .buflen = sizeof(buffer),
    .complete_cb = complete, .user_data = 0x1234
  };
  assert(!tuh_edpt_abort_xfer(1, ep_addr) && abort_calls == 0);
  assert(tuh_edpt_xfer(&xfer) && submissions == 1);

  // Hardware has finished, but USBH has not consumed its completion event.
  hcd_event_xfer_complete(1, ep_addr, sizeof(buffer), XFER_RESULT_SUCCESS, true);
  assert(!tuh_edpt_abort_xfer(1, ep_addr) && abort_calls == 1);
  assert(usbh_edpt_busy(1, ep_addr));
  assert(!tuh_edpt_xfer(&xfer) && submissions == 1);
  tuh_task_ext(0, false);
  assert(completions == 1 && !usbh_edpt_busy(1, ep_addr));

  // A failed abort of an in-flight request must preserve ownership too.
  assert(tuh_edpt_xfer(&xfer) && submissions == 2);
  assert(!tuh_edpt_abort_xfer(1, ep_addr) && abort_calls == 2);
  assert(usbh_edpt_busy(1, ep_addr));
  assert(!tuh_edpt_xfer(&xfer) && submissions == 2);

  // Successfully cancelling queued work releases it without a completion.
  abort_result = true;
  assert(tuh_edpt_abort_xfer(1, ep_addr) && abort_calls == 3);
  assert(!usbh_edpt_busy(1, ep_addr));
  assert(tuh_edpt_xfer(&xfer) && submissions == 3);
  hcd_event_xfer_complete(1, ep_addr, sizeof(buffer), XFER_RESULT_SUCCESS, true);
  tuh_task_ext(0, false);
  assert(completions == 2 && !usbh_edpt_busy(1, ep_addr));

  // Close must release in-flight work even when the queued-only abort fails.
  abort_result = close_result = false;
  assert(tuh_edpt_xfer(&xfer) && submissions == 4);
  assert(!tuh_edpt_close(1, ep_addr) && usbh_edpt_busy(1, ep_addr));
  close_result = true;
  assert(tuh_edpt_close(1, ep_addr) && !usbh_edpt_busy(1, ep_addr));
  tusb_desc_endpoint_t const desc = {
    .bLength = sizeof(desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = ep_addr, .bmAttributes = {.xfer = TUSB_XFER_ISOCHRONOUS},
    .wMaxPacketSize = sizeof(buffer), .bInterval = 1
  };
  assert(tuh_edpt_open(1, &desc));
  assert(tuh_edpt_xfer(&xfer) && submissions == 5);
  hcd_event_xfer_complete(1, ep_addr, sizeof(buffer), XFER_RESULT_SUCCESS, true);
  tuh_task_ext(0, false);
  assert(completions == 3);
}

int main(void) {
  test_abort(1);
  test_abort(0x81);
  puts("USBH abort ownership passed");
  return 0;
}
