/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// The audio device on a port without the ISO alloc/activate API (TUP_DCD_EDPT_CLOSE_API), with feedback endpoints
// enabled: SET_INTERFACE closes the endpoints of the previous alternate setting.

#define AUDIO_DEVICE_TEST_FIXTURE
#include "audio_device_test.h"

TEST_SOURCE_FILE("audio_device.c")
TEST_SOURCE_FILE("tusb_fifo.c")

TU_VERIFY_STATIC(CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP, "test_audio_device_close_api needs feedback EP support");
#ifdef TUP_DCD_EDPT_ISO_ALLOC
  #error "test_audio_device_close_api needs TUP_DCD_EDPT_CLOSE_API"
#endif

static uint8_t closed_ep[4];
static uint8_t closed_count;

bool usbd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t* desc_ep) {
  (void) rhport;
  (void) desc_ep;
  return true;
}

void usbd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(closed_ep), closed_count);
  closed_ep[closed_count++] = ep_addr;
}

// no feedback EP to service in any alternate setting
void usbd_sof_enable(uint8_t rhport, sof_consumer_t consumer, bool en) {
  (void) rhport;
  (void) consumer;
  TEST_ASSERT_FALSE(en);
}

tusb_speed_t tud_speed_get(void) {
  return TUSB_SPEED_FULL;
}

void setUp(void) {
  closed_count = 0;
  audio_test_setup();
}

void tearDown(void) {
  audio_test_teardown();
}

// The OUT alternate setting has no feedback endpoint: leaving it closes the data endpoint only, not EP0
void test_audio_out_without_feedback_ep_closes_data_ep_only(void) {
  set_interface(1, 0);
  TEST_ASSERT_EQUAL(1, closed_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, closed_ep[0]);
}
