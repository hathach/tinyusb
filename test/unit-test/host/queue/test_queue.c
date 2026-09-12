// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "host/usbh.c"
#include "class/audio/audio_host.c"

static bool accept = true;
static uint32_t time_ms;
static uint32_t reset_started_ms;
static bool check_reset_timing;
static bool reset_ended;
static unsigned reset_count;
void hcd_int_enable(uint8_t rhport) { (void)rhport; }
void hcd_int_disable(uint8_t rhport) { (void)rhport; }
bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t* buffer, uint16_t len) {
  (void)rhport; (void)daddr; (void)ep_addr; (void)buffer; (void)len;
  return accept;
}

uint32_t tusb_time_millis_api(void) { return time_ms; }
bool hcd_init(uint8_t p, const tusb_rhport_init_t* init) { (void)p; (void)init; return true; }
void hcd_int_handler(uint8_t p, bool isr) { (void)p; (void)isr; }
void hcd_device_close(uint8_t p, uint8_t d) { (void)p; (void)d; }
void hcd_port_reset(uint8_t p) { (void)p; reset_started_ms = time_ms; reset_count++; }
void hcd_port_reset_end(uint8_t p) {
  (void)p;
  if (check_reset_timing) {
#if defined(TUP_USBIP_EHCI) && !CFG_TUH_MAX3421
    // Model ChipIdea's automatically timed reset completing after 50 ms.
    assert(time_ms - reset_started_ms >= 55);
#else
    assert(time_ms - reset_started_ms >= 50);
#endif
    reset_ended = true;
  }
}
bool hcd_port_connect_status(uint8_t p) { (void)p; return true; }
tusb_speed_t hcd_port_speed_get(uint8_t p) {
  (void)p;
  assert(!check_reset_timing || reset_ended);
  return TUSB_SPEED_HIGH;
}
bool hcd_edpt_open(uint8_t p, uint8_t d, const tusb_desc_endpoint_t* desc) {
  (void)p; (void)d; (void)desc; return true;
}
bool hcd_edpt_close(uint8_t p, uint8_t d, uint8_t ep) { (void)p; (void)d; (void)ep; return true; }
bool hcd_edpt_abort_xfer(uint8_t p, uint8_t d, uint8_t ep) { (void)p; (void)d; (void)ep; return accept; }
bool hcd_setup_send(uint8_t p, uint8_t d, const uint8_t setup[8]) {
  (void)p; (void)d; (void)setup; return true;
}

static unsigned app_completions;
static void test_root_reset_timing(void) {
  check_reset_timing = true;
  reset_ended = false;
  reset_count = 0;
  time_ms = UINT32_MAX - 100; // Also exercise a deadline across clock wrap.
  hcd_event_t attach = {.rhport = 0, .event_id = HCD_EVENT_DEVICE_ATTACH};
  enum_new_device(&attach);
  assert(reset_count == 0);
  assert(_usbh_data.call_after.arg == ENUM_AFTER_DEBOUNCING_DELAY);
  assert(_usbh_data.call_after.at_ms - time_ms >= ENUM_DEBOUNCING_DELAY_MS);
  // Advance through debounce, reset and post-reset deferred callbacks.
  for (unsigned i = 0; i < 3; i++) {
    assert(_usbh_data.call_after.func == enum_delay_async);
    time_ms = _usbh_data.call_after.at_ms;
    uintptr_t const arg = _usbh_data.call_after.arg;
    _usbh_data.call_after.func = NULL;
    enum_delay_async(arg);
    assert(reset_count == 1);
  }
  assert(reset_count == 1 && reset_ended && _usbh_data.dev0_bus.speed == TUSB_SPEED_HIGH);
  _usbh_data.call_after.func = NULL;
  check_reset_timing = false;
  time_ms = 0;
}

static void app_complete(tuh_xfer_t* xfer) {
  assert(xfer->result != XFER_RESULT_QUEUED);
  app_completions++;
}

#if CFG_TUH_XFER_QUEUE_DEPTH > 1
static void test_public_dispatch_and_close(void) {
  usbh_device_t* dev = get_device(1);
  memset(dev, 0, sizeof(*dev));
  dev->connected = true;
  dev->bus_info.speed = TUSB_SPEED_HIGH;
  _usbh_controller_id = 0;
  _usbh_q = osal_queue_create(&_usbh_qdef);
  accept = true;
  tusb_desc_endpoint_t desc = {.bLength = sizeof(desc), .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x81, .bmAttributes = {.xfer = TUSB_XFER_ISOCHRONOUS},
    .wMaxPacketSize = 24, .bInterval = 1};
  assert(tuh_edpt_open(1, &desc));
  tuh_xfer_t x = {.daddr = 1, .ep_addr = 0x81, .complete_cb = app_complete};
  assert(tuh_edpt_xfer(&x));
  hcd_event_xfer_complete(1, 0x81, 0, XFER_RESULT_QUEUED, true);
  tuh_task_ext(0, false);
  assert(app_completions == 0 && usbh_edpt_busy(1, 0x81));
  // The public claim must reject queue credit atomically, even if a caller
  // observed the endpoint idle before another task submitted its request.
  assert(!usbh_edpt_claim_internal(1, 0x81, false));
  assert(usbh_edpt_claim(1, 0x81));
  assert(usbh_edpt_release(1, 0x81));
  assert(!tuh_edpt_xfer(&x));
  // Closing clears the pending requests before the endpoint is reopened.
  assert(tuh_edpt_close(1, 0x81));
  assert(tuh_edpt_open(1, &desc));
  assert(tuh_edpt_xfer(&x));
  tuh_task_ext(0, false);
  assert(app_completions == 0 && dev->ep_pending[1][1] == 1);
  accept = false;
  assert(!tuh_edpt_abort_xfer(1, 0x81));
  assert(usbh_edpt_busy(1, 0x81));
  hcd_event_xfer_complete(1, 0x81, 0, XFER_RESULT_SUCCESS, true);
  tuh_task_ext(0, false);
  assert(app_completions == 1 && !usbh_edpt_busy(1, 0x81));
  osal_queue_delete(_usbh_q);
  _usbh_q = NULL;
  _usbh_controller_id = TUSB_INDEX_INVALID_8;
}

static void audio_event_ep(usbh_device_t* dev, uint8_t ep_addr, xfer_result_t result, uint32_t len) {
  hcd_event_t e = {.xfer_complete = {.ep_addr = ep_addr,
                                    .result = result, .len = len}};
  assert(usbh_edpt_retire_event(dev, &e));
  assert(audioh_xfer_cb(1, ep_addr, result, len));
}

static void audio_event(usbh_device_t* dev, xfer_result_t result, uint32_t len) {
  audio_event_ep(dev, 0x81, result, len);
}

static void test_audio_ring(void) {
  usbh_device_t* dev = get_device(1);
  memset(dev, 0, sizeof(*dev));
  dev->connected = true;
  accept = true;
  assert(audioh_init());
  tuh_audio_stream_t* s = &_audioh_itf[0].in_stream;
  s->daddr = 1;
  s->active_config = s->active_as = 0;
  s->as[0].ep_addr = 0x81;
  s->as[0].ep_size = 24;
  s->frame_bytes = 4;
  s->running = true;
  assert(audioh_stream_capture_xfer(s));
  uint8_t* first = audioh_packet_buffer(s, 0);
  uint8_t* second = audioh_packet_buffer(s, 1);
  memset(first, 0x11, 24);
  audio_event(dev, XFER_RESULT_QUEUED, 0);
  assert(s->packet_count == 2 && first != second);
  memset(second, 0x22, 24);
  for (unsigned slot = 2; slot < CFG_TUH_XFER_QUEUE_DEPTH; slot++) {
    audio_event(dev, XFER_RESULT_QUEUED, 0);
    assert(s->packet_count == slot + 1);
    uint8_t* packet = audioh_packet_buffer(s, slot);
    assert(packet != first && packet != second);
    memset(packet, 0x11 * (slot + 1), 24);
  }
  audio_event(dev, XFER_RESULT_QUEUED, 0);
  assert(s->packet_count == CFG_TUH_XFER_QUEUE_DEPTH && first[0] == 0x11 && second[0] == 0x22);
  audio_event(dev, XFER_RESULT_SUCCESS, 24);
  assert(s->packet_count == CFG_TUH_XFER_QUEUE_DEPTH && s->packet_head == 1);
  uint8_t received[24];
  assert(tu_fifo_read_n(&s->edpt.ff, received, 24) == 24 && received[0] == 0x11);
  assert(second[0] == 0x22);
  audio_event(dev, XFER_RESULT_SUCCESS, 24);
  assert(tu_fifo_read_n(&s->edpt.ff, received, 24) == 24 && received[0] == 0x22);
  for (unsigned slot = 2; slot < CFG_TUH_XFER_QUEUE_DEPTH; slot++) {
    audio_event(dev, XFER_RESULT_SUCCESS, 24);
    assert(tu_fifo_read_n(&s->edpt.ff, received, 24) == 24 && received[0] == 0x11 * (slot + 1));
  }
  // Stop keeps the buffers owned until terminal events drain every slot.
  audioh_stream_stop_xfers(s);
  audio_event(dev, XFER_RESULT_QUEUED, 0);
  assert(s->packet_count == CFG_TUH_XFER_QUEUE_DEPTH);
  for (unsigned remaining = CFG_TUH_XFER_QUEUE_DEPTH - 1; remaining; remaining--) {
    audio_event(dev, XFER_RESULT_SUCCESS, 24);
    assert(s->packet_count == remaining && usbh_edpt_busy(1, 0x81));
  }
  audio_event(dev, XFER_RESULT_FAILED, 0);
  assert(s->packet_count == 0 && !usbh_edpt_busy(1, 0x81));
  s->running = true;
  assert(audioh_stream_capture_xfer(s));
  audio_event(dev, XFER_RESULT_QUEUED, 0);
  assert(s->packet_count == 2);
  // A failed refill retires exactly one buffer and stops; its sibling drains.
  accept = false;
  audio_event(dev, XFER_RESULT_SUCCESS, 24);
  assert(!s->running && s->packet_count == 1 && dev->ep_pending[1][1] == 1);
  audio_event(dev, XFER_RESULT_SUCCESS, 24);
  assert(s->packet_count == 0 && !usbh_edpt_busy(1, 0x81));
  assert(audioh_deinit());
}

static void test_playback_ring(void) {
  usbh_device_t* dev = get_device(1);
  memset(dev, 0, sizeof(*dev));
  dev->connected = true;
  accept = true;
  assert(audioh_init());
  tuh_audio_stream_t* s = &_audioh_itf[0].out_stream;
  s->daddr = 1;
  s->active_config = s->active_as = 0;
  s->as[0].ep_addr = 7;
  s->as[0].ep_size = 28;
  s->frame_bytes = 4;
  s->running = true;
  audioh_playback_t* playback = audioh_get_playback(s);
  playback->target_frames_q16 = 6u << 16;
  uint8_t samples[48];
  memset(samples, 0x11, 24);
  memset(samples + 24, 0x22, 24);
  assert(tu_fifo_write_n(&s->edpt.ff, samples, sizeof(samples)) == sizeof(samples));
  assert(audioh_stream_playback_xfer(s));
  audio_event_ep(dev, 7, XFER_RESULT_QUEUED, 0);
  uint8_t* first = audioh_packet_buffer(s, 0);
  uint8_t* second = audioh_packet_buffer(s, 1);
  assert(first[0] == 0x11 && second[0] == 0x22 && s->packet_count == 2);
  for (unsigned slot = 2; slot < CFG_TUH_XFER_QUEUE_DEPTH; slot++) {
    audio_event_ep(dev, 7, XFER_RESULT_QUEUED, 0);
    assert(s->packet_count == slot + 1 && audioh_packet_buffer(s, slot)[0] == 0);
  }
  audio_event_ep(dev, 7, XFER_RESULT_SUCCESS, 24);
  assert(first[0] == 0 && second[0] == 0x22); // Only retired buffer becomes silence.
  audio_event_ep(dev, 7, XFER_RESULT_QUEUED, 0);
  assert(second[0] == 0x22 && s->packet_count == CFG_TUH_XFER_QUEUE_DEPTH);
  // Explicit feedback keeps its own single buffer; QUEUED cannot prime it twice.
  playback->feedback[0].ep_addr = 0x82;
  assert(usbh_edpt_claim(1, 0x82));
  assert(usbh_edpt_xfer(1, 0x82, _audioh_epbuf[0].feedback, 4));
  audio_event_ep(dev, 0x82, XFER_RESULT_QUEUED, 0);
  assert(dev->ep_pending[2][1] == 1 && s->packet_count == CFG_TUH_XFER_QUEUE_DEPTH);
  audioh_stream_stop_xfers(s);
  audio_event_ep(dev, 0x82, XFER_RESULT_SUCCESS, 4);
  assert(s->packet_count == CFG_TUH_XFER_QUEUE_DEPTH);
  for (unsigned slot = 0; slot < CFG_TUH_XFER_QUEUE_DEPTH; slot++) {
    audio_event_ep(dev, 7, XFER_RESULT_SUCCESS, 24);
  }
  assert(s->packet_count == 0 && !usbh_edpt_busy(1, 7));
  assert(audioh_deinit());
}

int main(void) {
  test_root_reset_timing();
  usbh_device_t* dev = get_device(1);
  dev->connected = true;
  hcd_event_t e = {.dev_addr = 1, .event_id = HCD_EVENT_XFER_COMPLETE,
                  .xfer_complete = {.ep_addr = 0x81, .result = XFER_RESULT_QUEUED}};
  assert(usbh_edpt_claim(1, 0x81));
  assert(usbh_edpt_xfer(1, 0x81, NULL, 0));
  assert(dev->ep_pending[1][1] == 1 && usbh_edpt_busy(1, 0x81));
  assert(!usbh_edpt_claim(1, 0x81));
  assert(usbh_edpt_retire_event(dev, &e));
  assert(dev->ep_pending[1][1] == 1 && usbh_edpt_busy(1, 0x81));
  assert(usbh_edpt_claim(1, 0x81));
  assert(usbh_edpt_retire_event(dev, &e));
  assert(!usbh_edpt_claim(1, 0x81));
  assert(usbh_edpt_release(1, 0x81));
  assert(usbh_edpt_claim(1, 0x81));
  assert(usbh_edpt_xfer(1, 0x81, NULL, 0));
  assert(dev->ep_pending[1][1] == 2);
  for (unsigned slot = 2; slot < CFG_TUH_XFER_QUEUE_DEPTH; slot++) {
    assert(usbh_edpt_retire_event(dev, &e));
    assert(usbh_edpt_claim(1, 0x81));
    assert(usbh_edpt_xfer(1, 0x81, NULL, 0));
    assert(dev->ep_pending[1][1] == slot + 1);
  }
  // A delayed capacity event does not create a slot beyond the queue depth.
  assert(usbh_edpt_retire_event(dev, &e));
  assert(!usbh_edpt_claim(1, 0x81));
  e.xfer_complete.result = XFER_RESULT_SUCCESS;
  assert(usbh_edpt_retire_event(dev, &e));
  assert(dev->ep_pending[1][1] == CFG_TUH_XFER_QUEUE_DEPTH - 1 && usbh_edpt_busy(1, 0x81));
  assert(usbh_edpt_claim(1, 0x81));
  accept = false;
  assert(!usbh_edpt_xfer(1, 0x81, NULL, 0));
  assert(dev->ep_pending[1][1] == CFG_TUH_XFER_QUEUE_DEPTH - 1 && usbh_edpt_busy(1, 0x81));
  for (unsigned slot = 1; slot < CFG_TUH_XFER_QUEUE_DEPTH; slot++) {
    assert(usbh_edpt_retire_event(dev, &e));
  }
  assert(!usbh_edpt_busy(1, 0x81));
  assert(!usbh_edpt_retire_event(dev, &e));
  // A terminal event must preserve the reservation for a second request.
  accept = true;
  assert(usbh_edpt_claim(1, 0x81));
  assert(usbh_edpt_xfer(1, 0x81, NULL, 0));
  e.xfer_complete.result = XFER_RESULT_QUEUED;
  assert(usbh_edpt_retire_event(dev, &e));
  assert(usbh_edpt_claim(1, 0x81));
  e.xfer_complete.result = XFER_RESULT_SUCCESS;
  assert(usbh_edpt_retire_event(dev, &e));
  assert(!usbh_edpt_claim(1, 0x81));
  assert(usbh_edpt_release(1, 0x81));
  test_public_dispatch_and_close();
  test_audio_ring();
  test_playback_ring();
  puts("USBH/audio queue ownership tests passed");
  return 0;
}
#else
int main(void) {
  test_root_reset_timing();
  usbh_device_t* dev = get_device(1);
  dev->connected = true;
  dev->bus_info.speed = TUSB_SPEED_HIGH;
  _usbh_controller_id = 0;
  _usbh_q = osal_queue_create(&_usbh_qdef);
  tuh_xfer_t x = {.daddr = 1, .ep_addr = 0x81, .complete_cb = app_complete};
  assert(tuh_edpt_xfer(&x));
  assert(!tuh_edpt_xfer(&x));
  hcd_event_xfer_complete(1, 0x81, 0, XFER_RESULT_SUCCESS, true);
  tuh_task_ext(0, false);
  assert(app_completions == 1 && !usbh_edpt_busy(1, 0x81));
  accept = false;
  assert(!tuh_edpt_xfer(&x));
  assert(!usbh_edpt_busy(1, 0x81));
  accept = true;
  assert(tuh_edpt_xfer(&x));
  assert(tuh_edpt_abort_xfer(1, 0x81));
  assert(!usbh_edpt_busy(1, 0x81));

  assert(audioh_init());
  tuh_audio_stream_t* s = &_audioh_itf[0].in_stream;
  s->daddr = 1;
  s->active_config = s->active_as = 0;
  s->as[0].ep_addr = 0x81;
  s->as[0].ep_size = 24;
  s->frame_bytes = 4;
  s->running = true;
  assert(audioh_stream_capture_xfer(s));
  assert(!audioh_stream_capture_xfer(s));
  memset(s->edpt.ep_buf, 0x11, 24);
  hcd_event_xfer_complete(1, 0x81, 24, XFER_RESULT_SUCCESS, true);
  tuh_task_ext(0, false);
  uint8_t received[24];
  assert(tu_fifo_read_n(&s->edpt.ff, received, sizeof(received)) == sizeof(received));
  assert(received[0] == 0x11 && usbh_edpt_busy(1, 0x81));
  audioh_stream_stop_xfers(s);
  hcd_event_xfer_complete(1, 0x81, 24, XFER_RESULT_SUCCESS, true);
  tuh_task_ext(0, false);
  assert(!usbh_edpt_busy(1, 0x81) && tu_fifo_empty(&s->edpt.ff));

  s = &_audioh_itf[0].out_stream;
  s->daddr = 1;
  s->active_config = s->active_as = 0;
  s->as[0].ep_addr = 7;
  s->as[0].ep_size = 28;
  s->frame_bytes = 4;
  s->running = true;
  audioh_get_playback(s)->target_frames_q16 = 6u << 16;
  assert(tu_fifo_write_n(&s->edpt.ff, received, sizeof(received)) == sizeof(received));
  assert(audioh_stream_playback_xfer(s));
  assert(!audioh_stream_playback_xfer(s));
  assert(s->edpt.ep_buf[0] == 0x11);
  hcd_event_xfer_complete(1, 7, 24, XFER_RESULT_SUCCESS, true);
  tuh_task_ext(0, false);
  assert(s->edpt.ep_buf[0] == 0 && usbh_edpt_busy(1, 7)); // silence on underrun
  audioh_stream_stop_xfers(s);
  hcd_event_xfer_complete(1, 7, 24, XFER_RESULT_SUCCESS, true);
  tuh_task_ext(0, false);
  assert(!usbh_edpt_busy(1, 7));
  assert(audioh_deinit());
  osal_queue_delete(_usbh_q);
  puts("USBH/audio default single-buffer tests passed");
  return 0;
}
#endif
